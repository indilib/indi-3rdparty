/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Prometheus Astro
 * See COPYING for the license text. Distributed without any warranty.
 */
#include "tyr_telescope.h"

#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>
#include <indicom.h>
#include <indilogger.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <regex>
#include <thread>
#include <sys/ioctl.h>
#include <termios.h>

namespace
{
constexpr int ReplyBytes = 2048;
constexpr int CommandTimeoutSeconds = 3;
constexpr double SiderealArcsecondsPerSecond = 15.041067;
std::unique_ptr<TyrTelescope> tyr(new TyrTelescope());
}

TyrTelescope::TyrTelescope() : INDI::GuiderInterface(this)
{
    setVersion(TYR_DRIVER_VERSION_MAJOR, TYR_DRIVER_VERSION_MINOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_RATE | TELESCOPE_HAS_PIER_SIDE |
                           TELESCOPE_CAN_HOME_GO | TELESCOPE_CAN_HOME_SET, 10);
    setTelescopeConnection(CONNECTION_SERIAL | CONNECTION_TCP);
}

const char *TyrTelescope::getDefaultName()
{
    return "Prometheus Astro Tyr";
}

bool TyrTelescope::initProperties()
{
    INDI::Telescope::initProperties();
    // TIME_UTC is an INDI standard property: its offset belongs to the client,
    // not to Tyr's opposite-sign, standard-time-only firmware convention.
    TimeTP.setLabel("Client UTC time");
    TimeTP[1].setLabel("Client offset (includes DST)");

    for (int i = 0; i < 10; ++i)
    {
        static constexpr std::array<const char *, 10> labels =
            {"0.25x", "0.5x", "1x", "2x", "4x", "8x", "20x", "48x", "Half maximum", "Maximum"};
        SlewRateSP[i].setName(std::to_string(i));
        SlewRateSP[i].setLabel(labels[i]);
        SlewRateSP[i].setState(i == 6 ? ISS_ON : ISS_OFF);
    }

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_KING", "King");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    FirmwareTP[0].fill("PRODUCT", "Product", "");
    FirmwareTP[1].fill("VERSION", "Firmware", "");
    FirmwareTP.fill(getDeviceName(), "TYR_FIRMWARE", "Controller", "Tyr", IP_RO, 0, IPS_IDLE);

    StatusTP[0].fill("STATUS", "Mount status", "");
    StatusTP[1].fill("PIER", "Pier side", "Unknown");
    StatusTP.fill(getDeviceName(), "TYR_STATUS", "Live state", "Tyr", IP_RO, 0, IPS_IDLE);

    CompensationSP[0].fill("NONE", "No compensation", ISS_ON);
    CompensationSP[1].fill("REFRACTION", "Refraction", ISS_OFF);
    CompensationSP[2].fill("MODEL", "Pointing model", ISS_OFF);
    CompensationSP.fill(getDeviceName(), "TYR_COMPENSATION", "Tracking compensation", "Tyr", IP_RW,
                        ISR_1OFMANY, 30, IPS_IDLE);

    CompensationAxesSP[0].fill("ONE_AXIS", "One axis", ISS_OFF);
    CompensationAxesSP[1].fill("BOTH_AXES", "Both axes", ISS_ON);
    CompensationAxesSP.fill(getDeviceName(), "TYR_COMPENSATION_AXES", "Compensation axes", "Tyr", IP_RW,
                            ISR_1OFMANY, 30, IPS_IDLE);

    HomeInputsSP[0].fill("NORMAL", "Normal RA/Dec inputs", ISS_ON);
    HomeInputsSP[1].fill("SWAPPED", "Swap RA/Dec inputs", ISS_OFF);
    HomeInputsSP.fill(getDeviceName(), "TYR_HOME_INPUTS", "Home sensor inputs", "Tyr", IP_RW,
                      ISR_1OFMANY, 30, IPS_IDLE);

    PreciseSlewNP[0].fill("DEGREES_PER_SECOND", "Manual and GoTo speed", "%.3f", 0.01, 10, 0.01, 1);
    PreciseSlewNP.fill(getDeviceName(), "TYR_SLEW_SPEED", "Slew speed (degrees/second)", MOTION_TAB,
                       IP_RW, 30, IPS_IDLE);

    GuideRateNP[0].fill("GUIDE_RATE_WE", "RA rate", "%.2f", 0.25, 1, 0.25, 0.5);
    GuideRateNP[1].fill("GUIDE_RATE_NS", "Dec rate", "%.2f", 0.25, 1, 0.25, 0.5);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Pulse guide rate", MOTION_TAB, IP_RW, 30, IPS_IDLE);

    INDI::GuiderInterface::initProperties(MOTION_TAB);
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);
    SetParkDataType(PARK_SIMPLE);
    addAuxControls();
    setDefaultPollingPeriod(1000);
    initConfiguration();
    if (serialConnection)
        serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    if (tcpConnection) {
        tcpConnection->setDefaultHost("192.168.0.1");
        tcpConnection->setDefaultPort(9999);
        tcpConnection->setConnectionType(Connection::TCP::TYPE_TCP);
        tcpConnection->setLANSearchEnabled(false);
    }
    return true;
}

bool TyrTelescope::updateProperties()
{
    INDI::Telescope::updateProperties();
    INDI::GuiderInterface::updateProperties();
    if (isConnected())
    {
        defineProperty(FirmwareTP);
        defineProperty(StatusTP);
        defineProperty(CompensationSP);
        defineProperty(CompensationAxesSP);
        defineProperty(HomeInputsSP);
        defineProperty(PreciseSlewNP);
        defineProperty(GuideRateNP);

        std::string product = controllerProduct, version = controllerVersion;
        if (isSimulation())
        {
            product = "Prometheus Astro Tyr (simulation)";
            version = "0.0-sim";
        }
        if ((!product.empty() && !version.empty()) || (command("GVP", &product) && command("GVN", &version)))
        {
            FirmwareTP[0].setText(product.c_str());
            FirmwareTP[1].setText(version.c_str());
            FirmwareTP.setState(IPS_OK);
            FirmwareTP.apply();
        }
        double steps = 0, minimum = 0, maximum = 0, current = 0;
        if (readSlewConfiguration(steps, minimum, maximum, current))
        {
            slewStepsPerDegree = steps;
            slewMinimumDegreesPerSecond = minimum;
            slewMaximumDegreesPerSecond = maximum;
            PreciseSlewNP[0].setMin(minimum);
            PreciseSlewNP[0].setMax(maximum);
            PreciseSlewNP[0].setValue(current);
            PreciseSlewNP.setState(IPS_OK);
            PreciseSlewNP.apply();
        }
        std::string inputs;
        if (command("hI?", &inputs))
        {
            std::array<int, 4> values {};
            if (std::sscanf(inputs.c_str(), "%d,%d,%d,%d", &values[0], &values[1], &values[2], &values[3]) == 4 && values[0] == 1)
            {
                HomeInputsSP.reset();
                HomeInputsSP[values[1] ? 1 : 0].setState(ISS_ON);
                HomeInputsSP.setState(IPS_OK);
                HomeInputsSP.apply();
            }
        }
    }
    else
    {
        deleteProperty(FirmwareTP);
        deleteProperty(StatusTP);
        deleteProperty(CompensationSP);
        deleteProperty(CompensationAxesSP);
        deleteProperty(HomeInputsSP);
        deleteProperty(PreciseSlewNP);
        deleteProperty(GuideRateNP);
    }
    updateConfigurationProperties();
    return true;
}

bool TyrTelescope::Handshake()
{
    if (getActiveConnection() == tcpConnection && tcpConnection->connectionType() != Connection::TCP::TYPE_TCP) {
        LOG_ERROR("Tyr requires TCP, not UDP, for network connections.");
        return false;
    }
    if (isSimulation())
        return true;
    // CP2102 boards may reset on open. Release boot/reset control lines and
    // keep the same descriptor open while firmware finishes initialization.
    int lines = TIOCM_DTR | TIOCM_RTS;
    if (getActiveConnection() == serialConnection) ioctl(PortFD, TIOCMBIC, &lines);
    LOG_INFO("Waiting for Tyr controller startup (up to 30 seconds)...");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    std::string capabilities;
    bool ready = false;
    do
    {
        if (command("TYV", &capabilities))
        {
            ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    } while (std::chrono::steady_clock::now() < deadline);
    if (!ready)
        return false;
    std::smatch product, version;
    const std::regex productPattern("\"product\"\\s*:\\s*\"([^\"]+)\"");
    const std::regex versionPattern("\"version\"\\s*:\\s*\"([^\"]+)\"");
    const std::regex successPattern("\"ok\"\\s*:\\s*true");
    if (!std::regex_search(capabilities, successPattern) ||
        !std::regex_search(capabilities, product, productPattern) ||
        (product[1].str().find("Tyr") == std::string::npos && product[1].str().find("TYR") == std::string::npos &&
         product[1].str() != "Prometheus Astro T\xc3\xbdr"))
    {
        LOG_ERROR("Controller did not return valid Tyr USB capabilities.");
        return false;
    }
    controllerProduct = product[1].str();
    if (std::regex_search(capabilities, version, versionPattern))
        controllerVersion = version[1].str();
    try
    {
        const auto data = nlohmann::json::parse(capabilities).at("data");
        networkSupported = data.value("network", false);
        bluetoothSupported = data.value("bluetooth", false);
    }
    catch (const nlohmann::json::exception &)
    {
        LOG_ERROR("Malformed Tyr capabilities.");
        return false;
    }
    return true;
}

bool TyrTelescope::command(const std::string &body, std::string *response, Reply reply)
{
    if (isSimulation())
    {
        if (response)
            response->clear();
        return true;
    }
    if (body.empty() || body.find_first_of("#\r\n") != std::string::npos)
        return false;
    std::string frame = ":" + body + "#";
    if (getActiveConnection() == serialConnection) tcflush(PortFD, TCIFLUSH);
    int written = 0;
    int result = tty_write_string(PortFD, frame.c_str(), &written);
    if (result != TTY_OK)
    {
        char error[MAXRBUF] {};
        tty_error_msg(result, error, MAXRBUF);
        LOGF_ERROR("Command %s write failed: %s", frame.c_str(), error);
        return false;
    }
    LOGF_DEBUG("TX %s", frame.c_str());
    if (reply == Reply::None)
    {
        if (getActiveConnection() == serialConnection) tcdrain(PortFD);
        return true;
    }

    char buffer[ReplyBytes] {};
    int read = 0;
    if (reply == Reply::Single)
        result = tty_read(PortFD, buffer, 1, CommandTimeoutSeconds, &read);
    else
        result = tty_nread_section(PortFD, buffer, ReplyBytes - 1, '#', CommandTimeoutSeconds, &read);
    if (result != TTY_OK || read <= 0)
    {
        char error[MAXRBUF] {};
        tty_error_msg(result, error, MAXRBUF);
        LOGF_ERROR("Command %s read failed: %s", frame.c_str(), error);
        return false;
    }
    if (reply == Reply::Terminated && buffer[read - 1] == '#')
        --read;
    buffer[read] = 0;
    LOGF_DEBUG("RX %s", buffer);
    if (response)
        response->assign(buffer, static_cast<size_t>(read));
    return true;
}

bool TyrTelescope::commandAccepted(const std::string &body)
{
    if (isSimulation())
        return true;
    std::string response;
    return command(body, &response, Reply::Single) && response == "1";
}

bool TyrTelescope::ReadScopeStatus()
{
    if (isSimulation())
    {
        if (TrackState == SCOPE_SLEWING || TrackState == SCOPE_PARKING)
        {
            const double step = 0.5;
            auto approach = [step](double &value, double target)
            {
                double delta = target - value;
                value += std::clamp(delta, -step, step);
                return std::abs(delta) <= step;
            };
            bool done = approach(simRA, simTargetRA) & approach(simDec, simTargetDec);
            if (done)
            {
                if (TrackState == SCOPE_PARKING)
                {
                    simParked = true;
                    SetParked(true);
                    TrackState = SCOPE_PARKED;
                }
                else
                    TrackState = simTracking ? SCOPE_TRACKING : SCOPE_IDLE;
            }
        }
        NewRaDec(simRA, simDec);
        return true;
    }

    std::string status, raText, decText, pier;
    if (!command("GU", &status) || !command("GR", &raText) || !command("GD", &decText))
        return false;
    double ra = 0, dec = 0;
    if (!parseCoordinates(raText, decText, ra, dec))
    {
        LOGF_ERROR("Invalid coordinates from Tyr: RA '%s', Dec '%s'.", raText.c_str(), decText.c_str());
        return false;
    }

    if (has(status, 'P'))
        TrackState = SCOPE_PARKED;
    else if (!has(status, 'N'))
        TrackState = has(status, 'I') ? SCOPE_PARKING : SCOPE_SLEWING;
    else if (!has(status, 'n'))
        TrackState = SCOPE_TRACKING;
    else
        TrackState = SCOPE_IDLE;

    bool parked = has(status, 'P');
    if (parked != isParked())
        SetParked(parked);
    NewRaDec(ra, dec);

    if (command("Gm", &pier))
    {
        if (pier == "E")
            setPierSide(PIER_EAST);
        else if (pier == "W")
            setPierSide(PIER_WEST);
        StatusTP[1].setText(pier == "E" ? "East" : pier == "W" ? "West" : "Unknown");
    }
    StatusTP[0].setText(status.c_str());
    StatusTP.setState(IPS_OK);
    StatusTP.apply();
    return readTyrOptions(status);
}

bool TyrTelescope::readTyrOptions(const std::string &status)
{
    CompensationSP.reset();
    CompensationSP[has(status, 'r') ? 1 : has(status, 't') ? 2 : 0].setState(ISS_ON);
    CompensationSP.setState(IPS_OK);
    CompensationSP.apply();
    CompensationAxesSP.reset();
    CompensationAxesSP[has(status, 's') ? 0 : 1].setState(ISS_ON);
    CompensationAxesSP.setState(IPS_OK);
    CompensationAxesSP.apply();

    int mode = has(status, '(') ? TRACK_LUNAR : has(status, 'O') ? TRACK_SOLAR : has(status, 'k') ? 3 : TRACK_SIDEREAL;
    if (mode < static_cast<int>(TrackModeSP.size()))
    {
        TrackModeSP.reset();
        TrackModeSP[mode].setState(ISS_ON);
        TrackModeSP.setState(IPS_OK);
        TrackModeSP.apply();
    }
    return true;
}

bool TyrTelescope::Goto(double ra, double dec)
{
    if (isSimulation())
    {
        simTargetRA = ra;
        simTargetDec = dec;
        TrackState = SCOPE_SLEWING;
        return true;
    }
    if (!setTarget(ra, dec))
        return false;
    std::string response;
    if (!command("MS", &response, Reply::Single))
        return false;
    if (response != "0")
    {
        LOGF_ERROR("Tyr rejected GoTo with code %s.", response.c_str());
        return false;
    }
    TrackState = SCOPE_SLEWING;
    return true;
}

bool TyrTelescope::Sync(double ra, double dec)
{
    if (isSimulation())
    {
        simRA = ra;
        simDec = dec;
        return true;
    }
    if (!setTarget(ra, dec))
        return false;
    // Tyr's :CS# follows the LX200 silent-Sync contract. Pier-side validation
    // still happens in firmware, but this command intentionally has no reply.
    return command("CS", nullptr, Reply::None);
}

bool TyrTelescope::setTarget(double ra, double dec)
{
    return commandAccepted("Sr" + hoursToText(ra)) && commandAccepted("Sd" + degreesToText(dec));
}

bool TyrTelescope::Abort()
{
    if (isSimulation())
    {
        TrackState = simTracking ? SCOPE_TRACKING : SCOPE_IDLE;
        return true;
    }
    return command("Q", nullptr, Reply::None);
}

bool TyrTelescope::MoveNS(INDI_DIR_NS direction, TelescopeMotionCommand motion)
{
    if (isSimulation())
        return true;
    if (motion == MOTION_STOP)
        return command("Qn", nullptr, Reply::None);
    std::ostringstream rate;
    rate << "RE" << std::fixed << std::setprecision(9) << PreciseSlewNP[0].getValue();
    return command(rate.str(), nullptr, Reply::None) &&
           command(direction == DIRECTION_NORTH ? "Mn" : "Ms", nullptr, Reply::None);
}

bool TyrTelescope::MoveWE(INDI_DIR_WE direction, TelescopeMotionCommand motion)
{
    if (isSimulation())
        return true;
    if (motion == MOTION_STOP)
        return command("Qe", nullptr, Reply::None);
    std::ostringstream rate;
    rate << "RA" << std::fixed << std::setprecision(9) << PreciseSlewNP[0].getValue();
    return command(rate.str(), nullptr, Reply::None) &&
           command(direction == DIRECTION_WEST ? "Mw" : "Me", nullptr, Reply::None);
}

bool TyrTelescope::SetTrackMode(uint8_t mode)
{
    static constexpr std::array<const char *, 5> commands = {"TQ", "TS", "TL", "TK", "TQ"};
    if (mode >= commands.size())
        return false;
    return isSimulation() || command(commands[mode], nullptr, Reply::None);
}

bool TyrTelescope::SetTrackEnabled(bool enabled)
{
    if (isSimulation())
    {
        simTracking = enabled;
        TrackState = enabled ? SCOPE_TRACKING : SCOPE_IDLE;
        return true;
    }
    return commandAccepted(enabled ? "Te" : "Td");
}

bool TyrTelescope::SetTrackRate(double raRate, double decRate)
{
    if (!std::isfinite(raRate) || !std::isfinite(decRate))
        return false;
    std::ostringstream ra, dec;
    ra << "SXTR," << std::fixed << std::setprecision(7) << (raRate - SiderealArcsecondsPerSecond);
    dec << "SXTD," << std::fixed << std::setprecision(7) << decRate;
    return isSimulation() || (commandAccepted(ra.str()) && commandAccepted(dec.str()));
}

bool TyrTelescope::Park()
{
    if (isSimulation())
    {
        simTargetRA = 0;
        simTargetDec = 90;
        TrackState = SCOPE_PARKING;
        return true;
    }
    if (!commandAccepted("hP"))
        return false;
    TrackState = SCOPE_PARKING;
    return true;
}

bool TyrTelescope::UnPark()
{
    if (isSimulation())
    {
        simParked = false;
        SetParked(false);
        TrackState = SCOPE_IDLE;
        return true;
    }
    return commandAccepted("hR");
}

bool TyrTelescope::SetCurrentPark()
{
    return isSimulation() || commandAccepted("hQ");
}

bool TyrTelescope::SetDefaultPark()
{
    LOG_WARN("Tyr has no generic default Park position; use Set Park at a tested physical position.");
    return false;
}

IPState TyrTelescope::ExecuteHomeAction(TelescopeHomeAction action)
{
    if (isSimulation())
    {
        if (action == HOME_GO)
        {
            simTargetRA = 0;
            simTargetDec = 90;
            TrackState = SCOPE_SLEWING;
            return IPS_BUSY;
        }
        if (action == HOME_SET)
        {
            simRA = 0;
            simDec = 90;
            return IPS_OK;
        }
        return IPS_ALERT;
    }
    if (action == HOME_GO)
        return command("hC", nullptr, Reply::None) ? IPS_BUSY : IPS_ALERT;
    if (action == HOME_SET)
        return command("hF", nullptr, Reply::None) ? IPS_OK : IPS_ALERT;
    return IPS_ALERT;
}

IPState TyrTelescope::guide(char direction, uint32_t milliseconds)
{
    if (milliseconds == 0)
        return IPS_OK;
    if (milliseconds > 32767)
        return IPS_ALERT;
    if (isSimulation())
        return IPS_BUSY;
    std::ostringstream request;
    request << "MG" << direction << std::setw(5) << std::setfill('0') << milliseconds;
    return commandAccepted(request.str()) ? IPS_BUSY : IPS_ALERT;
}

IPState TyrTelescope::GuideNorth(uint32_t ms) { return guide('n', ms); }
IPState TyrTelescope::GuideSouth(uint32_t ms) { return guide('s', ms); }
IPState TyrTelescope::GuideEast(uint32_t ms) { return guide('e', ms); }
IPState TyrTelescope::GuideWest(uint32_t ms) { return guide('w', ms); }

bool TyrTelescope::updateLocation(double latitude, double longitude, double elevation)
{
    if (isSimulation())
        return true;
    std::ostringstream height;
    height << "Sv" << std::fixed << std::setprecision(0) << elevation;
    return commandAccepted("St" + degreesToText(latitude)) &&
           commandAccepted("Sg" + degreesToText(-longitude, 3)) && commandAccepted(height.str());
}

bool TyrTelescope::updateTime(ln_date *utc, double utcOffset)
{
    // Never infer a standard zone by subtracting one hour from the client:
    // DST rules vary worldwide. GG is the authoritative Tyr standard offset.
    INDI_UNUSED(utcOffset);
    if (!utc)
        return false;
    if (isSimulation())
        return true;

    std::string savedOffset;
    if (!command("GG", &savedOffset))
        return false;
    double tyrOffset = 0;
    if (!parseSexagesimal(savedOffset, tyrOffset))
        return false;

    std::tm time {};
    time.tm_year = utc->years - 1900;
    time.tm_mon = utc->months - 1;
    time.tm_mday = utc->days;
    time.tm_hour = utc->hours;
    time.tm_min = utc->minutes;
    time.tm_sec = static_cast<int>(utc->seconds);
    time_t utcSeconds = timegm(&time);
    utcSeconds -= static_cast<time_t>(std::llround(tyrOffset * 3600.0));
    std::tm standard {};
    gmtime_r(&utcSeconds, &standard);
    char date[16] {}, clock[16] {};
    std::strftime(date, sizeof(date), "%m/%d/%y", &standard);
    std::strftime(clock, sizeof(clock), "%H:%M:%S", &standard);
    return commandAccepted(std::string("SC") + date) && commandAccepted(std::string("SL") + clock);
}

bool TyrTelescope::ISNewSwitch(const char *device, const char *name, ISState *states, char *names[], int count)
{
    if (device && std::strcmp(device, getDeviceName()) == 0)
    {
        if (handleConfigurationSwitch(name, states, names, count))
            return true;
        if (CompensationSP.isNameMatch(name))
        {
            CompensationSP.update(states, names, count);
            int selected = CompensationSP.findOnSwitchIndex();
            static constexpr std::array<const char *, 3> commands = {"Tn", "Tr", "To"};
            bool ok = selected >= 0 && commandAccepted(commands[selected]);
            CompensationSP.setState(ok ? IPS_OK : IPS_ALERT);
            CompensationSP.apply();
            return true;
        }
        if (CompensationAxesSP.isNameMatch(name))
        {
            CompensationAxesSP.update(states, names, count);
            int selected = CompensationAxesSP.findOnSwitchIndex();
            bool ok = selected >= 0 && commandAccepted(selected == 0 ? "T1" : "T2");
            CompensationAxesSP.setState(ok ? IPS_OK : IPS_ALERT);
            CompensationAxesSP.apply();
            return true;
        }
        if (HomeInputsSP.isNameMatch(name))
        {
            HomeInputsSP.update(states, names, count);
            int selected = HomeInputsSP.findOnSwitchIndex();
            bool ok = selected >= 0 && commandAccepted(std::string("hI") + std::to_string(selected));
            HomeInputsSP.setState(ok ? IPS_OK : IPS_ALERT);
            HomeInputsSP.apply();
            return true;
        }
    }
    return INDI::Telescope::ISNewSwitch(device, name, states, names, count);
}

bool TyrTelescope::ISNewNumber(const char *device, const char *name, double values[], char *names[], int count)
{
    if (device && std::strcmp(device, getDeviceName()) == 0)
    {
        if (handleConfigurationNumber(name, values, names, count))
            return true;
        if (GuideRateNP.isNameMatch(name))
        {
            GuideRateNP.update(values, names, count);
            double value = GuideRateNP[0].getValue();
            int preset = value < 0.375 ? 0 : value < 0.75 ? 1 : 2;
            bool ok = command(std::string("R") + std::to_string(preset), nullptr, Reply::None);
            if (ok)
            {
                double applied = std::array<double, 3>{0.25, 0.5, 1.0}[preset];
                GuideRateNP[0].setValue(applied);
                GuideRateNP[1].setValue(applied);
            }
            GuideRateNP.setState(ok ? IPS_OK : IPS_ALERT);
            GuideRateNP.apply();
            return true;
        }
        if (PreciseSlewNP.isNameMatch(name))
        {
            PreciseSlewNP.update(values, names, count);
            const double degreesPerSecond = PreciseSlewNP[0].getValue();
            std::ostringstream period;
            period << "SX92," << std::fixed << std::setprecision(6)
                   << (1000000.0 / (slewStepsPerDegree * degreesPerSecond));
            bool ok = std::isfinite(degreesPerSecond) && slewStepsPerDegree >= 1 &&
                      degreesPerSecond >= slewMinimumDegreesPerSecond &&
                      degreesPerSecond <= slewMaximumDegreesPerSecond && commandAccepted(period.str());
            PreciseSlewNP.setState(ok ? IPS_OK : IPS_ALERT);
            PreciseSlewNP.apply();
            return true;
        }
    }
    if (INDI::GuiderInterface::processNumber(device, name, values, names, count))
        return true;
    return INDI::Telescope::ISNewNumber(device, name, values, names, count);
}

bool TyrTelescope::ISNewText(const char *device, const char *name, char *texts[], char *names[], int count)
{
    if (device && std::strcmp(device, getDeviceName()) == 0 && handleConfigurationText(name, texts, names, count))
        return true;
    return INDI::Telescope::ISNewText(device, name, texts, names, count);
}

bool TyrTelescope::readSlewConfiguration(double &stepsPerDegree, double &minimumDegreesPerSecond,
                                          double &maximumDegreesPerSecond, double &currentDegreesPerSecond)
{
    if (isSimulation())
    {
        stepsPerDegree = 26666.667;
        minimumDegreesPerSecond = 0.01;
        maximumDegreesPerSecond = 4;
        currentDegreesPerSecond = 1;
        return true;
    }

    std::array<std::string, 5> replies;
    static constexpr std::array<const char *, 5> commands = {"GXE4", "GX92", "GX93", "GX99", "GX97"};
    for (size_t i = 0; i < commands.size(); ++i)
        if (!command(commands[i], &replies[i]))
            return false;

    std::array<double, 5> value {};
    for (size_t i = 0; i < value.size(); ++i)
    {
        char *end = nullptr;
        value[i] = std::strtod(replies[i].c_str(), &end);
        if (end == replies[i].c_str() || *end != '\0' || !std::isfinite(value[i]) || value[i] <= 0)
            return false;
    }
    if (value[0] < 1)
        return false;

    const double currentDegreesPerMinute = 60000000.0 / (value[0] * value[1]);
    const double uncertainty = currentDegreesPerMinute * (0.5 / value[0] + 0.0005 / value[1]) + 3.01;
    if (std::abs(currentDegreesPerMinute - value[4] * 60.0) > uncertainty)
    {
        LOG_ERROR("Tyr returned inconsistent active motor timing values.");
        return false;
    }

    const double fastestInterval = std::max(value[2] / 2.0, value[3]);
    const double minimumDegreesPerMinute = 60000000.0 / (value[0] * value[2] * 2.0);
    const double maximumDegreesPerMinute = 60000000.0 / (value[0] * fastestInterval);
    if (fastestInterval > value[2] * 2.0 || currentDegreesPerMinute < minimumDegreesPerMinute - uncertainty ||
        currentDegreesPerMinute > maximumDegreesPerMinute + uncertainty)
    {
        LOG_ERROR("Tyr returned invalid slew speed limits.");
        return false;
    }

    stepsPerDegree = value[0];
    minimumDegreesPerSecond = minimumDegreesPerMinute / 60.0;
    maximumDegreesPerSecond = maximumDegreesPerMinute / 60.0;
    currentDegreesPerSecond = currentDegreesPerMinute / 60.0;
    return true;
}

bool TyrTelescope::saveConfigItems(FILE *file)
{
    INDI::Telescope::saveConfigItems(file);
    CompensationSP.save(file);
    CompensationAxesSP.save(file);
    HomeInputsSP.save(file);
    PreciseSlewNP.save(file);
    GuideRateNP.save(file);
    return true;
}

bool TyrTelescope::parseCoordinates(const std::string &ra, const std::string &dec, double &raHours, double &decDegrees) const
{
    return parseSexagesimal(ra, raHours) && parseSexagesimal(dec, decDegrees);
}

bool TyrTelescope::parseSexagesimal(const std::string &text, double &value)
{
    std::string normalized = text;
    std::replace(normalized.begin(), normalized.end(), '*', ':');
    std::replace(normalized.begin(), normalized.end(), '\'', ':');
    const bool negative = !normalized.empty() && normalized.front() == '-';
    if (!normalized.empty() && (normalized.front() == '-' || normalized.front() == '+'))
        normalized.erase(normalized.begin());
    std::replace(normalized.begin(), normalized.end(), ':', ' ');
    std::istringstream input(normalized);
    double major = 0, minutes = 0, seconds = 0;
    if (!(input >> major))
        return false;
    if (!(input >> minutes))
    {
        input.clear();
        minutes = 0;
    }
    if (!(input >> seconds))
    {
        input.clear();
        seconds = 0;
    }
    std::string trailing;
    if (input >> trailing)
        return false;
    if (minutes < 0 || minutes >= 60 || seconds < 0 || seconds >= 60)
        return false;
    value = major + minutes / 60.0 + seconds / 3600.0;
    if (negative)
        value = -value;
    return std::isfinite(value);
}

std::string TyrTelescope::hoursToText(double hours)
{
    long total = std::lround(std::fmod(hours + 24.0, 24.0) * 3600.0) % 86400;
    char text[16] {};
    std::snprintf(text, sizeof(text), "%02ld:%02ld:%02ld", total / 3600, total / 60 % 60, total % 60);
    return text;
}

std::string TyrTelescope::degreesToText(double degrees, int degreeWidth)
{
    long total = std::lround(std::abs(degrees) * 3600.0);
    char text[24] {};
    std::snprintf(text, sizeof(text), degreeWidth == 3 ? "%c%03ld*%02ld:%02ld" : "%c%02ld*%02ld:%02ld",
                  degrees < 0 ? '-' : '+', total / 3600, total / 60 % 60, total % 60);
    return text;
}

bool TyrTelescope::has(const std::string &value, char flag)
{
    return value.find(flag) != std::string::npos;
}
