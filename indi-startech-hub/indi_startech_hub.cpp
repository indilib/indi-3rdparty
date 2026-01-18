/*
  INDI 3rdparty driver: StarTech Industrial USB Hub (Linux only)
  Model: 5G7AINDRM-USB-A-HUB
  Direct serial control (no cusbi dependency)
*/

#include "indi_startech_hub.h"
#include "config.h"

#include <indicom.h>
#include <indidevapi.h>
#include <indilogger.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>

namespace
{
constexpr int kBaudRate = 9600;
constexpr int kDataBits = 8;
constexpr int kParity = 0;
constexpr int kStopBits = 1;

constexpr const char *kCommandTerminator = "\r";
constexpr char kLineTerminator = '\n';

constexpr const char *kCmdQueryId  = "?Q";
constexpr const char *kCmdGetMask  = "GP";
constexpr const char *kCmdSetMask  = "SP";

constexpr const char *kPasswordDefault = "pass";

constexpr int kNumPorts = 7;

// 7 physical ports => bits 24..30
static inline uint32_t bit_for_port(int portIndex1Based)
{
    return (1u << (portIndex1Based - 1)) << 24;
}

static inline const char *zc(const char *s) { return s ? s : ""; }

static inline std::string trim(const std::string &s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    return s.substr(b, e - b + 1);
}

static bool set_dtr_rts(int fd, bool on)
{
    int status = 0;
    if (ioctl(fd, TIOCMGET, &status) < 0)
        return false;

    if (on)
        status |= (TIOCM_DTR | TIOCM_RTS);
    else
        status &= ~(TIOCM_DTR | TIOCM_RTS);

    return ioctl(fd, TIOCMSET, &status) == 0;
}

static bool parse_hex_u32(const std::string &s, uint32_t &out)
{
    if (s.size() != 8)
        return false;
    errno = 0;
    char *end = nullptr;
    unsigned long v = std::strtoul(s.c_str(), &end, 16);
    if (errno != 0 || end == nullptr || *end != '\0')
        return false;
    out = static_cast<uint32_t>(v);
    return true;
}

static std::string hex8(uint32_t v)
{
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%08X", v);
    return std::string(buf);
}

static std::string pass8(std::string p)
{
    if (p.size() > 8) p.resize(8);
    while (p.size() < 8) p.push_back(' ');
    return p;
}

} // namespace

static std::unique_ptr<StarTechHub> g_dev(new StarTechHub());

StarTechHub::StarTechHub()
{
    setVersion(STARTECH_HUB_VERSION_MAJOR, STARTECH_HUB_VERSION_MINOR);
}

StarTechHub::~StarTechHub() = default;

const char *StarTechHub::getDefaultName()
{
    return "StarTech Industrial USB Hub";
}

uint64_t StarTechHub::nowMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (static_cast<uint64_t>(ts.tv_sec) * 1000ULL) + (ts.tv_nsec / 1000000ULL);
}

bool StarTechHub::validPort(const std::string &port) const
{
    if (port.empty())
        return false;
    if (port.find('<') != std::string::npos || port.find('>') != std::string::npos)
        return false;
    if (port.find(' ') != std::string::npos)
        return false;
    return std::regex_match(port, std::regex("^[A-Za-z0-9_./:-]+$"));
}

std::string StarTechHub::normalizePort(const std::string &port) const
{
    return port;
}



bool StarTechHub::initProperties()
{
    INDI::DefaultDevice::initProperties();

    IUFillText(&CtrlPortT[0], "CTRL_PORT", "Control Port", "");
IUFillTextVector(&CtrlPortTP, CtrlPortT, 1, getDeviceName(),
                     "STARTECH_CTRL", "Control Port", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    defineProperty(&CtrlPortTP);

    IUFillSwitch(&AllSW[0], "ALL_ON", "All On", ISS_OFF);
    IUFillSwitch(&AllSW[1], "ALL_OFF", "All Off", ISS_OFF);
    IUFillSwitchVector(&AllSVP, AllSW, 2,
                       getDeviceName(), "ALL_PORTS", "All Ports",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    defineProperty(&AllSVP);

    definePortsOnce();

    AliasT.resize(kNumPorts);
    for (int i = 0; i < kNumPorts; ++i)
    {
        std::string iname = "ALIAS_" + std::to_string(i + 1);
        std::string ilabel = "Alias PORT " + std::to_string(i + 1);
        IUFillText(&AliasT[i], iname.c_str(), ilabel.c_str(), "");
    }
    IUFillTextVector(&AliasVP, AliasT.data(), kNumPorts,
                     getDeviceName(),
                     "PORT_ALIASES",
                     "Aliases",
                     "Additional Settings",
                     IP_RW, 60, IPS_IDLE);
    defineProperty(&AliasVP);

    char defv[16];
    std::snprintf(defv, sizeof(defv), "%d", stateRefreshMs);
    IUFillText(&PollT[0], "STATE_REFRESH_MS", "State refresh ms", defv);
    IUFillTextVector(&PollVP, PollT, 1,
                     getDeviceName(), "STATE_CACHE", "State Cache",
                     "Additional Settings", IP_RW, 60, IPS_IDLE);
    defineProperty(&PollVP);

    setDriverInterface(AUX_INTERFACE);
    setDefaultPollingPeriod(minPollingMs);
    addAuxControls();

    if (!configLoaded)
    {
        loadConfig(true);
        configLoaded = true;
        syncSettingsFromTexts();
        applyAliases();
    }

    return true;
}

bool StarTechHub::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
        SetTimer(getPollingPeriod());

    return true;
}

void StarTechHub::definePortsOnce()
{
    Ports.clear();
    Ports.reserve(kNumPorts);

    for (int i = 0; i < kNumPorts; ++i)
    {
        auto pp = std::make_unique<PortProp>();
        pp->index = i + 1;

        IUFillSwitch(&pp->sw[0], "ON", "On", ISS_OFF);
        IUFillSwitch(&pp->sw[1], "OFF", "Off", ISS_ON);

        std::string pname = "PORT_" + std::to_string(pp->index);
        std::string plabel = "P" + std::to_string(pp->index);
        IUFillSwitchVector(&pp->svp, pp->sw, 2,
                           getDeviceName(), pname.c_str(), plabel.c_str(),
                           MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        defineProperty(&pp->svp);

        Ports.push_back(std::move(pp));
    }
}

void StarTechHub::syncSettingsFromTexts()
{
    std::string port = std::string(zc(CtrlPortT[0].text));
    if (!port.empty())
    {
        IUSaveText(&CtrlPortT[0], port.c_str());
    IDSetText(&CtrlPortTP, nullptr);
}

    int v = std::atoi(zc(PollT[0].text));
    if (v < 200) v = 200;
    stateRefreshMs = v;
}

bool StarTechHub::Connect()
{
    std::string port = std::string(zc(CtrlPortT[0].text));
    if (!validPort(port))
    {
        LOG_ERROR("CTRL_PORT not valid. Example: startech or /dev/startech");
        return false;
    }

    std::string id;
    if (!sendCommand(kCmdQueryId, &id))
    {
        LOGF_ERROR("Cannot communicate with hub on %s", zc(CtrlPortT[0].text));
        return false;
    }

    cachedBits.clear();
    lastReadMs = 0;

    LOGF_INFO("Connected. Hub ID: %s", id.c_str());
    return true;
}

bool StarTechHub::Disconnect()
{
    return true;
}

void StarTechHub::TimerHit()
{
    if (!isConnected())
        return;

    refreshStates();
    SetTimer(getPollingPeriod());
}

bool StarTechHub::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()))
        return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);

    if (!strcmp(name, "POLLING"))
    {
        for (int i = 0; i < n; ++i)
        {
            if (names[i] && !strcmp(names[i], "PERIOD"))
            {
                if (values[i] < static_cast<double>(minPollingMs))
                    values[i] = static_cast<double>(minPollingMs);
            }
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool StarTechHub::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()))
        return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);

    if (!strcmp(name, CtrlPortTP.name))
    {
        IUUpdateText(&CtrlPortTP, texts, names, n);
        syncSettingsFromTexts();
        IDSetText(&CtrlPortTP, nullptr);
        return true;
    }

    if (AliasVP.name[0] && !strcmp(name, AliasVP.name))
    {
        IUUpdateText(&AliasVP, texts, names, n);
        IDSetText(&AliasVP, nullptr);
        applyAliases();
        return true;
    }

    if (PollVP.name[0] && !strcmp(name, PollVP.name))
    {
        IUUpdateText(&PollVP, texts, names, n);
        syncSettingsFromTexts();
        IDSetText(&PollVP, nullptr);
        return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool StarTechHub::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()))
        return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);

    if (AllSVP.name[0] && !strcmp(name, AllSVP.name))
    {
        bool wantAllOn = false;
        for (int i = 0; i < n; ++i)
            if (names[i] && !strcmp(names[i], "ALL_ON") && states[i] == ISS_ON)
                wantAllOn = true;

        for (size_t i = 0; i < Ports.size(); ++i)
        {
            if (!setPortPower(static_cast<int>(i + 1), wantAllOn))
                LOGF_ERROR("Command failed on port %d", static_cast<int>(i + 1));
        }

        IUResetSwitch(&AllSVP);
        AllSW[wantAllOn ? 0 : 1].s = ISS_ON;
        IDSetSwitch(&AllSVP, nullptr);

        cachedBits.assign(kNumPorts, wantAllOn ? '1' : '0');
        lastReadMs = nowMs();
        emitStatesFromBits(cachedBits);
        return true;
    }

    int idx = portIndexByName(name);
    if (idx < 0)
        return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);

    bool wantOn = false;
    for (int i = 0; i < n; ++i)
        if (names[i] && !strcmp(names[i], "ON") && states[i] == ISS_ON)
            wantOn = true;

    if (!setPortPower(idx + 1, wantOn))
    {
        LOG_ERROR("Serial command failed.");
        return false;
    }

    if (cachedBits.size() < kNumPorts)
        cachedBits.assign(kNumPorts, '0');
    cachedBits[static_cast<size_t>(idx)] = wantOn ? '1' : '0';
    lastReadMs = nowMs();
    emitStatesFromBits(cachedBits);
    return true;
}

int StarTechHub::portIndexByName(const char *name) const
{
    for (size_t i = 0; i < Ports.size(); ++i)
        if (!strcmp(Ports[i]->svp.name, name))
            return static_cast<int>(i);
    return -1;
}

bool StarTechHub::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    IUSaveConfigText(fp, &CtrlPortTP);
    if (AliasVP.name[0] != '\0')
        IUSaveConfigText(fp, &AliasVP);
    if (PollVP.name[0] != '\0')
        IUSaveConfigText(fp, &PollVP);
    return true;
}

bool StarTechHub::sendCommand(const std::string &command, std::string *response)
{
    const std::string dev = std::string(zc(CtrlPortT[0].text));

    int fd = -1;
    int rc = tty_connect(dev.c_str(), kBaudRate, kDataBits, kParity, kStopBits, &fd);
    if (rc != TTY_OK)
    {
        char errMsg[MAXRBUF] = {0};
        tty_error_msg(rc, errMsg, MAXRBUF);
        LOGF_ERROR("Failed to connect to port %s. Error: %s", dev.c_str(), errMsg);
        return false;
    }

    (void)set_dtr_rts(fd, true);

    tcflush(fd, TCIOFLUSH);
    usleep(50 * 1000);

    const std::string payload = command + kCommandTerminator;

    int bytesWritten = 0;
    rc = tty_write_string(fd, payload.c_str(), &bytesWritten);
    if (rc != TTY_OK)
    {
        char errMsg[MAXRBUF] = {0};
        tty_error_msg(rc, errMsg, MAXRBUF);
        LOGF_ERROR("Serial write failed: %s", errMsg);
        tty_disconnect(fd);
        return false;
    }

    if (!response)
    {
        tty_disconnect(fd);
        return true;
    }

    char buffer[256] = {0};
    int bytesRead = 0;

    rc = tty_nread_section(fd, buffer, sizeof(buffer) - 1, kLineTerminator, 3, &bytesRead);
    if (rc != TTY_OK)
    {
        char errMsg[MAXRBUF] = {0};
        tty_error_msg(rc, errMsg, MAXRBUF);
        LOGF_ERROR("Serial read failed: %s", errMsg);
        tty_disconnect(fd);
        return false;
    }

    buffer[bytesRead] = '\0';
    *response = trim(std::string(buffer));

    tty_disconnect(fd);
    return true;
}

bool StarTechHub::getMask(uint32_t &mask)
{
    std::string resp;
    if (!sendCommand(kCmdGetMask, &resp))
        return false;

    if (!parse_hex_u32(resp, mask))
    {
        LOGF_ERROR("Invalid GP response: '%s'", resp.c_str());
        return false;
    }
    return true;
}

bool StarTechHub::setMask(uint32_t mask)
{
    const std::string cmd = std::string(kCmdSetMask) + pass8(std::string(kPasswordDefault)) + hex8(mask);

    std::string ack;
    if (!sendCommand(cmd, &ack))
        return false;

    const std::string expected = std::string("G") + hex8(mask);
    if (ack != expected)
    {
        LOGF_ERROR("Invalid ACK. Expected '%s' got '%s'", expected.c_str(), ack.c_str());
        return false;
    }
    return true;
}

bool StarTechHub::setPortPower(int index, bool enable)
{
    if (index < 1 || index > kNumPorts)
        return false;

    uint32_t mask = 0;
    if (!getMask(mask))
        return false;

    const uint32_t bit = bit_for_port(index);
    const uint32_t newMask = enable ? (mask | bit) : (mask & ~bit);

    if (newMask == mask)
        return true;

    if (setMask(newMask))
        return true;

    usleep(100 * 1000);
    return setMask(newMask);
}

bool StarTechHub::readStateFromHub(std::string &outBits)
{
    uint32_t mask = 0;
    if (!getMask(mask))
        return false;

    std::string bits;
    bits.resize(kNumPorts, '0');
    for (int port = 1; port <= kNumPorts; ++port)
    {
        const bool on = (mask & bit_for_port(port)) != 0;
        bits[port - 1] = on ? '1' : '0';
    }

    outBits = bits;
    return true;
}

void StarTechHub::emitStatesFromBits(const std::string &bits)
{
    for (auto &p : Ports)
    {
        const int i0 = p->index - 1;
        bool targetOn = (i0 >= 0 && i0 < static_cast<int>(bits.size()) && bits[static_cast<size_t>(i0)] == '1');
        bool curOn = (p->sw[0].s == ISS_ON);
        if (targetOn == curOn)
            continue;

        IUResetSwitch(&p->svp);
        p->sw[0].s = targetOn ? ISS_ON : ISS_OFF;
        p->sw[1].s = targetOn ? ISS_OFF : ISS_ON;
        IDSetSwitch(&p->svp, nullptr);
    }
}

void StarTechHub::applyAliases()
{
    // Rebuild ALL port properties in order so the UI order is stable and labels reflect aliases.
    // This also avoids the "only one port moved to bottom" effect.
    rebuildPortPropertiesInOrder();
}

void StarTechHub::rebuildPortPropertiesInOrder()
{
    // Preserve current on/off states (best effort).
    std::vector<bool> wasOn;
    wasOn.reserve(Ports.size());
    for (auto &p : Ports)
        wasOn.push_back(p->sw[0].s == ISS_ON);

    // Delete all PORT_* properties first.
    for (auto &p : Ports)
        deleteProperty(p->svp.name);

    // Redefine PORT_1..PORT_7 in order with label including alias (if any).
    for (size_t i = 0; i < Ports.size(); ++i)
    {
        auto &p = *Ports[i];
        const int idx = static_cast<int>(i) + 1;

        std::string alias = (i < AliasT.size()) ? trim(std::string(zc(AliasT[i].text))) : "";
        // Keep labels short to leave UI space for the On/Off buttons.
        // Format: "P7 pippo" (or just "P7")
        std::string base  = "P" + std::to_string(idx);
        std::string label = alias.empty() ? base : (base + " " + alias);
if (label.size() >= MAXINDILABEL)
            label.resize(MAXINDILABEL - 1);

        IUFillSwitch(&p.sw[0], "ON",  "On",  wasOn[i] ? ISS_ON : ISS_OFF);
        IUFillSwitch(&p.sw[1], "OFF", "Off", wasOn[i] ? ISS_OFF : ISS_ON);

        std::string pname = "PORT_" + std::to_string(idx);
        IUFillSwitchVector(&p.svp, p.sw, 2,
                           getDeviceName(), pname.c_str(), label.c_str(),
                           MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

        defineProperty(&p.svp);
        IDSetSwitch(&p.svp, nullptr);
    }
}

void StarTechHub::refreshStates()
{
    const uint64_t now = nowMs();
    if (now - lastReadMs < static_cast<uint64_t>(stateRefreshMs))
    {
        if (!cachedBits.empty())
            emitStatesFromBits(cachedBits);
        return;
    }

    std::string bits;
    if (readStateFromHub(bits))
    {
        cachedBits = bits;
        lastReadMs = now;
        emitStatesFromBits(cachedBits);
    }
}
