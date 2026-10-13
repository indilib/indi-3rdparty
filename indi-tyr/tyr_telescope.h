/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Prometheus Astro
 * See COPYING for the license text. Distributed without any warranty.
 */
#pragma once

#include <indiguiderinterface.h>
#include <inditelescope.h>
#include <indipropertynumber.h>
#include <indipropertyswitch.h>
#include <indipropertytext.h>

#include <libnova/ln_types.h>

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <nlohmann/json.hpp>

class TyrTelescope final : public INDI::Telescope, public INDI::GuiderInterface
{
  public:
    TyrTelescope();
    const char *getDefaultName() override;
    bool initProperties() override;
    bool updateProperties() override;
    bool Handshake() override;
    bool ReadScopeStatus() override;
    bool ISNewSwitch(const char *device, const char *name, ISState *states, char *names[], int count) override;
    bool ISNewNumber(const char *device, const char *name, double values[], char *names[], int count) override;
    bool ISNewText(const char *device, const char *name, char *texts[], char *names[], int count) override;

  protected:
    bool Goto(double ra, double dec) override;
    bool Sync(double ra, double dec) override;
    bool Abort() override;
    bool MoveNS(INDI_DIR_NS direction, TelescopeMotionCommand command) override;
    bool MoveWE(INDI_DIR_WE direction, TelescopeMotionCommand command) override;
    bool SetTrackMode(uint8_t mode) override;
    bool SetTrackEnabled(bool enabled) override;
    bool SetTrackRate(double raRate, double decRate) override;
    bool Park() override;
    bool UnPark() override;
    bool SetCurrentPark() override;
    bool SetDefaultPark() override;
    IPState ExecuteHomeAction(TelescopeHomeAction action) override;
    IPState GuideNorth(uint32_t milliseconds) override;
    IPState GuideSouth(uint32_t milliseconds) override;
    IPState GuideEast(uint32_t milliseconds) override;
    IPState GuideWest(uint32_t milliseconds) override;
    bool updateLocation(double latitude, double longitude, double elevation) override;
    bool updateTime(ln_date *utc, double utcOffset) override;
    bool saveConfigItems(FILE *file) override;

  private:
    enum class Reply { None, Single, Terminated };

    bool command(const std::string &body, std::string *response = nullptr, Reply reply = Reply::Terminated);
    bool commandAccepted(const std::string &body);
    bool setTarget(double ra, double dec);
    bool parseCoordinates(const std::string &ra, const std::string &dec, double &raHours, double &decDegrees) const;
    bool readSlewConfiguration(double &stepsPerDegree, double &minimumDegreesPerSecond,
                               double &maximumDegreesPerSecond, double &currentDegreesPerSecond);
    bool readTyrOptions(const std::string &status);
    IPState guide(char direction, uint32_t milliseconds);
    static std::string hoursToText(double hours);
    static std::string degreesToText(double degrees, int degreeWidth = 2);
    static bool parseSexagesimal(const std::string &text, double &value);
    static bool has(const std::string &value, char flag);
    struct NumberConfiguration
    {
        INDI::PropertyNumber property;
        std::vector<std::string> reads, writes;
        std::vector<double> factors;
        int axis = 0;
        bool stopped = true, loaded = false;
    };
    struct SwitchConfiguration
    {
        INDI::PropertySwitch property;
        std::vector<std::string> values, writes;
        std::string read;
        char statusFlag = 0;
        bool stopped = false;
    };
    void initConfiguration();
    void updateConfigurationProperties();
    bool refreshConfiguration(const std::string &section = "ALL");
    bool readNumberConfiguration(NumberConfiguration &group, bool publish = true);
    bool readSwitchConfiguration(SwitchConfiguration &group, const std::string &status);
    bool handleConfigurationNumber(const char *, double[], char *[], int);
    bool handleConfigurationSwitch(const char *, ISState[], char *[], int);
    bool handleConfigurationText(const char *, char *[], char *[], int);
    bool managedCommand(const std::string &, std::string * = nullptr);
    bool usbOperation(const std::string &, nlohmann::json &, bool quiet = false);
    bool requireStopped(bool trackingOff = false);
    bool readNetworkConfiguration();
    bool applyNetworkConfiguration();
    bool readBluetoothConfiguration();
    static std::string hexText(const std::string &);
    static bool finiteNumber(const std::string &, double &);

    std::vector<NumberConfiguration> configurationNumbers;
    std::vector<SwitchConfiguration> configurationSwitches;
    INDI::PropertySwitch RefreshConfigurationSP {5};
    INDI::PropertySwitch ControllerActionsSP {5};
    INDI::PropertyNumber AlignmentStarsNP {1};
    INDI::PropertyText ConfigurationInfoTP {4};
    INDI::PropertyText BootDiagnosticTP {3};
    INDI::PropertySwitch BluetoothActionsSP {4};
    INDI::PropertyText BluetoothSelectionTP {2};
    INDI::PropertyText BluetoothStateTP {3};
    INDI::PropertySwitch NetworkFlagsSP {3};
    INDI::PropertySwitch NetworkDhcpSP {3};
    INDI::PropertySwitch NetworkApplySP {1};
    INDI::PropertyText NetworkAPTP {4};
    INDI::PropertyNumber NetworkChannelNP {1};
    std::array<INDI::PropertyText, 3> NetworkStationTP {
        INDI::PropertyText(6), INDI::PropertyText(6), INDI::PropertyText(6)};
    INDI::PropertyText NetworkPasswordsTP {5};
    std::array<std::string, 5> pendingNetworkPasswords;
    bool networkSupported = false, bluetoothSupported = false, networkLoaded = false;
    nlohmann::json simulationSettings, simulationNetwork;

    INDI::PropertyText FirmwareTP {2};
    INDI::PropertyText StatusTP {2};
    INDI::PropertySwitch CompensationSP {3};
    INDI::PropertySwitch CompensationAxesSP {2};
    INDI::PropertySwitch HomeInputsSP {2};
    INDI::PropertyNumber PreciseSlewNP {1};
    INDI::PropertyNumber GuideRateNP {2};

    double simRA {0};
    double simDec {90};
    double simTargetRA {0};
    double simTargetDec {90};
    bool simTracking {false};
    bool simParked {false};
    double slewStepsPerDegree {0};
    std::string controllerProduct;
    std::string controllerVersion;
    double slewMinimumDegreesPerSecond {0.01};
    double slewMaximumDegreesPerSecond {10};
};
