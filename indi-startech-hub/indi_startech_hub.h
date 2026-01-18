#pragma once

#include <libindi/defaultdevice.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * INDI 3rdparty driver: StarTech Industrial USB Hub (Linux only)
 * Model: 5G7AINDRM-USB-A-HUB
 *
 * Serial protocol (summary):
 *   ?Q\r                      -> ID (string, CRLF)
 *   GP\r                      -> state: 8 hex (e.g. FFFFFFFF) + CRLF
 *   SP<pass8><mask8>\r         -> set mask, ACK: G<mask8> + CRLF
 *
 * Password: "pass" (hardcoded). No known command to change it.
 *
 * Robust design:
 *  - No dynamic defineProperty/deleteProperty: all properties are created once.
 *  - Serial port is opened/closed per transaction, avoiding stale open handles.
 */
class StarTechHub : public INDI::DefaultDevice
{
public:
    StarTechHub();
    ~StarTechHub() override;

    const char *getDefaultName() override;
    bool initProperties() override;
    bool updateProperties() override;

    bool Connect() override;
    bool Disconnect() override;

    void TimerHit() override;

    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    bool saveConfigItems(FILE *fp) override;

private:
    struct PortProp
    {
        ISwitch sw[2];
        ISwitchVectorProperty svp;
        int index;
    };

    static uint64_t nowMs();

    bool validPort(const std::string &port) const;
    std::string normalizePort(const std::string &port) const;
    // One-transaction serial exchange (opens/closes the TTY internally).
    bool sendCommand(const std::string &command, std::string *response);

    // Hub protocol helpers
    bool getMask(uint32_t &mask);
    bool setMask(uint32_t mask);

    bool setPortPower(int index, bool enable);       // index: 1..7
    bool readStateFromHub(std::string &outBits);     // returns "0101010" (len==7)

    void emitStatesFromBits(const std::string &bits);
    void refreshStates();
    int portIndexByName(const char *name) const;

    void definePortsOnce();
    void syncSettingsFromTexts();
    void applyAliases();
    void rebuildPortPropertiesInOrder();

    // ---- INDI properties ----
    IText CtrlPortT[1] {};
    ITextVectorProperty CtrlPortTP {};

    std::vector<std::unique_ptr<PortProp>> Ports;

    ISwitch AllSW[2] {};
    ISwitchVectorProperty AllSVP {};

    std::vector<IText> AliasT;        // ALIAS_1..ALIAS_7
    ITextVectorProperty AliasVP {};

    IText PollT[1] {};
    ITextVectorProperty PollVP {};

    // ---- State cache ----
    std::string cachedBits;
    uint64_t lastReadMs { 0 };
    int stateRefreshMs { 5000 };

    uint32_t minPollingMs { 1000 };

    bool configLoaded { false };
};
