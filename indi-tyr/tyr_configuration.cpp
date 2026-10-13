/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Prometheus Astro
 * See COPYING for the license text. Distributed without any warranty.
 */
#include "tyr_telescope.h"
#include <connectionplugins/connectionserial.h>
#include <indicom.h>
#include <indilogger.h>
#include <arpa/inet.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <regex>
#include <sstream>
#include <termios.h>

using Json = nlohmann::json;
namespace
{
struct Field { const char *name, *label; double minimum, maximum, step, initial; };
std::string decimal(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(12) << value;
    return out.str();
}
std::string timezoneText(double value) {
    const long minutes = std::lround(std::abs(value) * 60);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%c%02ld:%02ld", value < 0 ? '-' : '+', minutes / 60, minutes % 60);
    return buffer;
}
bool validIP(const std::string &value) {
    in_addr address {};
    return inet_pton(AF_INET, value.c_str(), &address) == 1;
}
}

void TyrTelescope::initConfiguration()
{
    auto numbers = [this](const char *name, const char *label, const char *tab,
                         std::initializer_list<Field> fields, std::vector<std::string> reads,
                         std::vector<std::string> writes, std::vector<double> factors, int axis = 0) {
        NumberConfiguration group {INDI::PropertyNumber(fields.size()), std::move(reads), std::move(writes), std::move(factors)};
        group.axis = axis;
        size_t i = 0;
        for (const auto &field : fields)
            group.property[i++].fill(field.name, field.label, "%.6f", field.minimum, field.maximum, field.step, field.initial);
        group.property.fill(getDeviceName(), name, label, tab, IP_RW, 30, IPS_IDLE);
        configurationNumbers.push_back(std::move(group));
    };
    const std::initializer_list<Field> axisFields = {
        {"STEPS_PER_DEGREE", "Steps per degree", 0.001, 10000000, 0.001, 26666.667},
        {"REVERSE", "Direction: -1 normal / -2 reversed", -2, -1, 1, -1},
        {"MINIMUM", "Minimum position (degrees)", -360, 360, 1, -180},
        {"MAXIMUM", "Maximum position (degrees)", -360, 360, 1, 180},
        {"PARAMETER_1", "Tracking microsteps / tracking P", -1, 100000, 1, 64},
        {"PARAMETER_2", "GoTo microsteps / tracking I", -1, 100000, 1, 1},
        {"PARAMETER_3", "Hold current (mA) / tracking D", -1, 100000, 1, 500},
        {"PARAMETER_4", "Tracking current (mA) / GoTo P", -1, 100000, 1, 900},
        {"PARAMETER_5", "GoTo current (mA) / GoTo I", -1, 100000, 1, 1200},
        {"PARAMETER_6", "Driver parameter 6 / GoTo D", -100000, 100000, 1, -1}
    };
    numbers("TYR_AXIS_RA", "RA saved motor settings", "Axes", axisFields, {"GXA1"}, {}, {}, 1);
    numbers("TYR_AXIS_DEC", "Dec saved motor settings", "Axes", axisFields, {"GXA2"}, {}, {}, 2);
    numbers("TYR_ALTITUDE_LIMITS", "Altitude limits (degrees)", "Limits",
        {{"HORIZON", "Horizon", -30, 30, 1, 0}, {"OVERHEAD", "Overhead", 60, 90, 1, 90}},
        {"Gh", "Go"}, {"Sh", "So"}, {1, 1});
    numbers("TYR_MERIDIAN_LIMITS", "Meridian limits (degrees)", "Limits",
        {{"EAST", "East", -270, 270, 0.25, 0}, {"WEST", "West", -270, 270, 0.25, 0}},
        {"GXE9", "GXEA"}, {"SXE9,", "SXEA,"}, {4, 4});
    numbers("TYR_BACKLASH", "Backlash (arcseconds)", "Limits",
        {{"RA", "RA", 0, 3600, 1, 0}, {"DEC", "Dec", 0, 3600, 1, 0}},
        {"%BR", "%BD"}, {"$BR", "$BD"}, {1, 1});
    numbers("TYR_HOME_OFFSETS", "Home offsets (arcseconds)", "Home",
        {{"RA", "RA", -648000, 648000, 1, 0}, {"DEC", "Dec", -648000, 648000, 1, 0}},
        {"h?"}, {"hC1,", "hC2,"}, {1, 1});
    numbers("TYR_STANDARD_TIME_OFFSET", "Tyr UTC minus STANDARD time (no DST)", SITE_TAB,
        {{"HOURS", "Mount offset; Athens -2 year-round", -14, 14, 0.25, -2}},
        {"GG"}, {"SG"}, {1});
    configurationNumbers.back().stopped = false;
    numbers("TYR_TRACKING_OFFSETS", "Tracking offsets (arcsec/sidereal second)", "Tracking",
        {{"RA", "RA offset", -1800, 1800, 0.01, 0}, {"DEC", "Dec offset", -1800, 1800, 0.01, 0}},
        {"GXTR", "GXTD"}, {"SXTR,", "SXTD,"}, {1, 1});
    configurationNumbers.back().stopped = false;

    auto switches = [this](const char *name, const char *label, const char *tab,
                           std::vector<std::string> labels, std::vector<std::string> values,
                           std::vector<std::string> writes, const char *read, char flag = 0, bool stopped = false) {
        SwitchConfiguration group {INDI::PropertySwitch(labels.size()), std::move(values), std::move(writes), read, flag, stopped};
        for (size_t i = 0; i < labels.size(); ++i)
            group.property[i].fill(group.values[i].c_str(), labels[i].c_str(), i == 0 ? ISS_ON : ISS_OFF);
        group.property.fill(getDeviceName(), name, label, tab, IP_RW, ISR_1OFMANY, 30, IPS_IDLE);
        configurationSwitches.push_back(std::move(group));
    };
    switches("TYR_ADVANCED_SETTINGS", "Saved motor configuration", "Axes", {"Firmware defaults", "Editable saved settings"},
        {"0", "1"}, {"SXAC,1", "SXAC,0"}, "GXA0", 0, true);
    switches("TYR_GEOMETRY", "Geometry on next restart", "Axes", {"Firmware default", "German equatorial", "Alt-Azimuth"},
        {"0", "1", "3"}, {"SXEM,0", "SXEM,1", "SXEM,3"}, "GXEM", 0, true);
    switches("TYR_AUTO_HOME", "Automatic Home on boot", "Home", {"Off", "On"}, {"OFF", "ON"}, {"hA0", "hA1"}, "", 'B', true);
    switches("TYR_AUTO_FLIP", "Automatic meridian flip", "Meridian", {"Off", "On"}, {"OFF", "ON"}, {"SX95,0", "SX95,1"}, "", 'a');
    switches("TYR_PAUSE_HOME", "Pause flip at Home", "Meridian", {"Off", "On"}, {"OFF", "ON"}, {"SX98,0", "SX98,1"}, "", 'u');
    switches("TYR_BUZZER", "Buzzer", "Meridian", {"Off", "On"}, {"OFF", "ON"}, {"SX97,0", "SX97,1"}, "", 'z');
    switches("TYR_PREFERRED_PIER", "Preferred GoTo pier side", "Meridian", {"East", "West", "Best"},
        {"E", "W", "B"}, {"SX96,E", "SX96,W", "SX96,B"}, "GX96");

    const char *sections[] = {"ALL", "MOUNT", "AXES", "NETWORK", "BLUETOOTH"};
    for (int i = 0; i < 5; ++i) RefreshConfigurationSP[i].fill(sections[i], sections[i], ISS_OFF);
    RefreshConfigurationSP.fill(getDeviceName(), "TYR_READ_SETTINGS", "Read mount settings", "Tyr", IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    const char *actions[] = {"ALIGN_START", "ALIGN_ACCEPT", "ALIGN_SAVE", "CONTINUE_FLIP", "RESTART"};
    const char *actionLabels[] = {"Start alignment", "Accept current star", "Save model", "Continue paused flip", "Restart controller"};
    for (int i = 0; i < 5; ++i) ControllerActionsSP[i].fill(actions[i], actionLabels[i], ISS_OFF);
    ControllerActionsSP.fill(getDeviceName(), "TYR_CONTROLLER_ACTIONS", "Controller actions", "Tyr", IP_RW, ISR_ATMOST1, 30, IPS_IDLE);
    AlignmentStarsNP[0].fill("STARS", "Alignment stars", "%.0f", 1, 9, 1, 3);
    AlignmentStarsNP.fill(getDeviceName(), "TYR_ALIGNMENT_STARS", "Manual alignment", "Alignment", IP_RW, 30, IPS_IDLE);
    ConfigurationInfoTP[0].fill("AXIS_RA", "RA driver", "Read settings");
    ConfigurationInfoTP[1].fill("AXIS_DEC", "Dec driver", "Read settings");
    ConfigurationInfoTP[2].fill("ALIGNMENT", "Alignment status", "");
    ConfigurationInfoTP[3].fill("APPLICATION", "Settings application", "Motor/geometry/network changes require restart. UTC offset excludes DST.");
    ConfigurationInfoTP.fill(getDeviceName(), "TYR_CONFIGURATION_INFO", "Configuration state", "Tyr", IP_RO, 0, IPS_IDLE);
    BootDiagnosticTP[0].fill("UPTIME_MS", "Uptime (ms)", "Unavailable");
    BootDiagnosticTP[1].fill("RESET_REASON", "Last reset reason", "Unavailable");
    BootDiagnosticTP[2].fill("LAST_ERROR", "Last command error", "");
    BootDiagnosticTP.fill(getDeviceName(), "TYR_BOOT_DIAGNOSTICS", "Controller diagnostics", "Tyr", IP_RO, 0, IPS_IDLE);

    const char *bleNames[] = {"ENABLE", "DISABLE", "SCAN", "FORGET"};
    for (int i = 0; i < 4; ++i) BluetoothActionsSP[i].fill(bleNames[i], bleNames[i], ISS_OFF);
    BluetoothActionsSP.fill(getDeviceName(), "TYR_BLUETOOTH_ACTION", "Magicsee R1", "Bluetooth", IP_RW, ISR_ATMOST1, 30, IPS_IDLE);
    BluetoothSelectionTP[0].fill("ADDRESS", "Controller MAC address", "");
    BluetoothSelectionTP[1].fill("ADDRESS_TYPE", "Address type (0-3)", "0");
    BluetoothSelectionTP.fill(getDeviceName(), "TYR_BLUETOOTH_SELECTION", "Save controller selection", "Bluetooth", IP_RW, 30, IPS_IDLE);
    BluetoothStateTP[0].fill("STATUS", "Connection and controls", "");
    BluetoothStateTP[1].fill("SELECTED", "Saved controller", "");
    BluetoothStateTP[2].fill("DEVICES", "Scanned controllers", "");
    BluetoothStateTP.fill(getDeviceName(), "TYR_BLUETOOTH_STATE", "Controller state", "Bluetooth", IP_RO, 0, IPS_IDLE);

    const char *flagNames[] = {"AP_ENABLED", "STATION_ENABLED", "AP_FALLBACK"};
    for (int i = 0; i < 3; ++i) {
        NetworkFlagsSP[i].fill(flagNames[i], flagNames[i], ISS_OFF);
        const auto name = "STATION_" + std::to_string(i + 1);
        NetworkDhcpSP[i].fill(name.c_str(), name.c_str(), ISS_ON);
    }
    NetworkFlagsSP.fill(getDeviceName(), "TYR_NETWORK_MODES", "Wi-Fi modes (pending until Apply)", "Network", IP_RW, ISR_NOFMANY, 30, IPS_IDLE);
    NetworkDhcpSP.fill(getDeviceName(), "TYR_NETWORK_DHCP", "Station DHCP (pending until Apply)", "Network", IP_RW, ISR_NOFMANY, 30, IPS_IDLE);
    const char *apFields[] = {"SSID", "IP", "GATEWAY", "SUBNET"};
    for (int i = 0; i < 4; ++i) NetworkAPTP[i].fill(apFields[i], apFields[i], "");
    NetworkAPTP.fill(getDeviceName(), "TYR_NETWORK_AP", "Access point (pending until Apply)", "Network", IP_RW, 30, IPS_IDLE);
    NetworkChannelNP[0].fill("CHANNEL", "AP channel", "%.0f", 1, 13, 1, 1);
    NetworkChannelNP.fill(getDeviceName(), "TYR_NETWORK_CHANNEL", "AP channel (pending until Apply)", "Network", IP_RW, 30, IPS_IDLE);
    const char *stationFields[] = {"HOST", "SSID", "IP", "GATEWAY", "SUBNET", "TARGET"};
    for (int station = 0; station < 3; ++station) {
        for (int i = 0; i < 6; ++i) NetworkStationTP[station][i].fill(stationFields[i], stationFields[i], "");
        const auto name = "TYR_NETWORK_STATION_" + std::to_string(station + 1);
        NetworkStationTP[station].fill(getDeviceName(), name.c_str(), "Station (pending until Apply)", "Network", IP_RW, 30, IPS_IDLE);
    }
    const char *passwordNames[] = {"AP", "STATION_1", "STATION_2", "STATION_3", "WEB_ADMIN"};
    for (int i = 0; i < 5; ++i) NetworkPasswordsTP[i].fill(passwordNames[i], passwordNames[i], "");
    NetworkPasswordsTP.fill(getDeviceName(), "TYR_NETWORK_NEW_PASSWORDS", "New passwords; blank retains saved value", "Network", IP_WO, 30, IPS_IDLE);
    NetworkApplySP[0].fill("APPLY", "Save all pending network settings", ISS_OFF);
    NetworkApplySP.fill(getDeviceName(), "TYR_NETWORK_APPLY", "Apply network (restart required)", "Network", IP_RW, ISR_ATMOST1, 30, IPS_IDLE);

    simulationSettings = {{"GXA0", "1"}, {"GXA1", "26666.667,-1,-180,180,64,1,500,900,1200,-1,T"},
        {"GXA2", "28444.444,-1,-90,90,64,1,500,900,1200,0,T"}, {"Gh", "0*"}, {"Go", "90*"},
        {"GXE9", "0"}, {"GXEA", "0"}, {"%BR", "0"}, {"%BD", "0"}, {"h?", "1,0,0"},
        {"GG", "-02:00"}, {"GXTR", "0"}, {"GXTD", "0"}, {"GXEM", "1"}, {"GX96", "B"},
        {"GU", "nNpBaEW260"}, {"A?", "900"}, {"GE", "0"}};
    simulationNetwork = {{"apEnabled", true}, {"stationEnabled", false}, {"fallback", true},
        {"apSsid", "Tyr"}, {"apChannel", 1}, {"apIp", "192.168.0.1"}, {"apGateway", "192.168.0.1"},
        {"apSubnet", "255.255.255.0"}, {"stations", Json::array()}};
    for (int i = 0; i < 3; ++i)
        simulationNetwork["stations"].push_back({{"host", "tyr"}, {"ssid", ""}, {"dhcp", true},
            {"ip", "0.0.0.0"}, {"gateway", "0.0.0.0"}, {"subnet", "255.255.255.0"}, {"target", "0.0.0.0"}});
}

std::string TyrTelescope::hexText(const std::string &value)
{
    static const char digits[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char byte : value) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}

bool TyrTelescope::usbOperation(const std::string &operation, Json &data, bool quiet)
{
    if (operation.empty() || operation.size() > 375 || operation.find_first_of("#\r\n") != std::string::npos) return false;
    if (isSimulation()) {
        if (operation == "N") data = simulationNetwork;
        else if (operation == "D") data = {{"uptimeMs", 1000}, {"resetReason", "simulation"}};
        else if (operation == "B") data = {{"enabled", false}, {"connected", false}, {"armed", false}, {"message", "Simulation"},
            {"selected", {{"address", ""}, {"name", ""}}}, {"devices", Json::array()}};
        else data = nullptr;
        return true;
    }
    const bool privateNetwork = operation.front() == 'n';
    const std::string frame = ":TY" + operation + "#";
    int bytes = 0;
    if (getActiveConnection() == serialConnection) tcflush(PortFD, TCIFLUSH);
    if (tty_write_string(PortFD, frame.c_str(), &bytes) != TTY_OK) return false;
    if (!quiet) LOGF_DEBUG("TX %s", privateNetwork ? "[network transaction redacted]" : frame.c_str());
    char response[8192] {};
    if (tty_nread_section(PortFD, response, sizeof(response) - 1, '#', 5, &bytes) != TTY_OK || bytes < 2 || response[bytes - 1] != '#') {
        if (!quiet) LOG_ERROR("Tyr management transaction timed out or returned incomplete framing.");
        return false;
    }
    response[bytes - 1] = 0;
    try {
        auto result = Json::parse(response);
        if (!result.at("ok").get<bool>()) {
            if (!quiet) LOGF_ERROR("Tyr rejected settings transaction (error %d).", result.value("error", -1));
            return false;
        }
        data = result.value("data", Json());
        return true;
    } catch (const Json::exception &) {
        if (!quiet) LOG_ERROR("Invalid Tyr management JSON response.");
        return false;
    }
}

bool TyrTelescope::managedCommand(const std::string &body, std::string *response)
{
    if (isSimulation()) {
        if (response) *response = simulationSettings.value(body, std::string());
        if (body == "GU" && response) {
            response->erase(std::remove(response->begin(), response->end(), 'n'), response->end());
            if (!simTracking) *response += 'n';
            if (TrackState == SCOPE_SLEWING || TrackState == SCOPE_PARKING)
                response->erase(std::remove(response->begin(), response->end(), 'N'), response->end());
        }
        if (body.rfind("SXA", 0) == 0 && body.size() > 5 && body[3] >= '1' && body[3] <= '2')
            simulationSettings["GXA" + body.substr(3, 1)] = body.substr(5) + ",T";
        for (const auto &group : configurationNumbers)
            for (size_t i = 0; i < group.writes.size(); ++i)
                if (body.rfind(group.writes[i], 0) == 0 && group.reads.size() == group.writes.size())
                    simulationSettings[group.reads[i]] = body.substr(group.writes[i].size());
        if (body.rfind("hC1,", 0) == 0 || body.rfind("hC2,", 0) == 0) {
            auto offsets = simulationSettings["h?"].get<std::string>();
            const auto first = offsets.find(','), last = offsets.rfind(',');
            simulationSettings["h?"] = body[2] == '1' ? offsets.substr(0, first + 1) + body.substr(4) + offsets.substr(last) :
                offsets.substr(0, last + 1) + body.substr(4);
        }
        for (const auto &group : configurationSwitches)
            for (size_t i = 0; i < group.writes.size(); ++i) if (body == group.writes[i]) {
                if (!group.read.empty()) simulationSettings[group.read] = group.values[i];
                else if (group.statusFlag) {
                    auto state = simulationSettings["GU"].get<std::string>();
                    state.erase(std::remove(state.begin(), state.end(), group.statusFlag), state.end());
                    if (i == 1) state += group.statusFlag;
                    simulationSettings["GU"] = state;
                }
            }
        return true;
    }
    if (body.empty() || body.size() > 177 || body.find_first_of("#;\r\n") != std::string::npos || body.rfind("TY", 0) == 0) return false;
    Json data;
    if (!usbOperation("C" + hexText(body), data) || !data.is_string()) return false;
    if (response) *response = data.get<std::string>();
    return true;
}

bool TyrTelescope::finiteNumber(const std::string &text, double &number)
{
    char *end = nullptr;
    number = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) return false;
    while (*end == '*' || *end == ' ') ++end;
    return *end == 0 && std::isfinite(number);
}

bool TyrTelescope::requireStopped(bool trackingOff)
{
    std::string state;
    if (!managedCommand("GU", &state) || !has(state, 'N') || state.find_first_of("ghGI") != std::string::npos ||
        (trackingOff && !has(state, 'n'))) {
        LOG_ERROR("Stop mount motion and guiding before changing these settings; motor/Home changes also require tracking off.");
        return false;
    }
    return true;
}

void TyrTelescope::updateConfigurationProperties()
{
    if (isConnected()) {
        if (isSimulation()) networkSupported = bluetoothSupported = true;
        for (auto &group : configurationNumbers) defineProperty(group.property);
        for (auto &group : configurationSwitches) defineProperty(group.property);
        defineProperty(RefreshConfigurationSP); defineProperty(ControllerActionsSP);
        defineProperty(AlignmentStarsNP); defineProperty(ConfigurationInfoTP); defineProperty(BootDiagnosticTP);
        if (bluetoothSupported) { defineProperty(BluetoothActionsSP); defineProperty(BluetoothSelectionTP); defineProperty(BluetoothStateTP); }
        if (networkSupported) {
            defineProperty(NetworkFlagsSP); defineProperty(NetworkDhcpSP); defineProperty(NetworkAPTP);
            defineProperty(NetworkChannelNP); defineProperty(NetworkPasswordsTP); defineProperty(NetworkApplySP);
            for (auto &property : NetworkStationTP) defineProperty(property);
        }
        refreshConfiguration();
    } else {
        for (auto &group : configurationNumbers) { deleteProperty(group.property); group.loaded = false; }
        for (auto &group : configurationSwitches) deleteProperty(group.property);
        deleteProperty(RefreshConfigurationSP); deleteProperty(ControllerActionsSP); deleteProperty(AlignmentStarsNP);
        deleteProperty(ConfigurationInfoTP); deleteProperty(BootDiagnosticTP); deleteProperty(BluetoothActionsSP);
        deleteProperty(BluetoothSelectionTP); deleteProperty(BluetoothStateTP); deleteProperty(NetworkFlagsSP);
        deleteProperty(NetworkDhcpSP); deleteProperty(NetworkAPTP); deleteProperty(NetworkChannelNP);
        deleteProperty(NetworkPasswordsTP); deleteProperty(NetworkApplySP);
        for (auto &property : NetworkStationTP) deleteProperty(property);
        networkLoaded = false;
        pendingNetworkPasswords.fill("");
        for (auto &entry : NetworkPasswordsTP) entry.setText("");
    }
}

bool TyrTelescope::readNumberConfiguration(NumberConfiguration &group, bool publish)
{
    std::vector<double> values;
    std::string reply;
    bool ok = true;
    if (group.axis) {
        ok = managedCommand(group.reads[0], &reply);
        std::istringstream parts(reply);
        std::string field;
        for (int i = 0; i < 10 && ok; ++i) {
            double value;
            ok = bool(std::getline(parts, field, ',')) && finiteNumber(field, value);
            if (ok) values.push_back(value);
        }
        ok = ok && bool(std::getline(parts, field, ',')) && field.size() == 1 && std::string("TSP").find(field) != std::string::npos;
        if (ok) ConfigurationInfoTP[group.axis - 1].setText(field == "P" ? "Servo PID: six PID parameters" : "Step/direction: microsteps and motor currents");
    } else if (group.reads[0] == "h?") {
        ok = managedCommand("h?", &reply);
        std::istringstream parts(reply);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(parts, field, ',')) fields.push_back(field);
        ok = ok && (fields.size() == 3 || fields.size() == 4);
        for (size_t i = fields.size() >= 2 ? fields.size() - 2 : 0; ok && i < fields.size(); ++i) {
            double value;
            ok = finiteNumber(fields[i], value);
            if (ok) values.push_back(value);
        }
    } else {
        for (size_t i = 0; ok && i < group.reads.size(); ++i) {
            double value = 0;
            ok = managedCommand(group.reads[i], &reply) &&
                (group.reads[i] == "GG" ? parseSexagesimal(reply, value) : finiteNumber(reply, value));
            if (ok) values.push_back(value / group.factors[i]);
        }
    }
    if (ok && values.size() == group.property.size()) {
        for (size_t i = 0; i < values.size(); ++i) group.property[i].setValue(values[i]);
        group.loaded = true;
    } else ok = false;
    group.property.setState(ok ? IPS_OK : IPS_ALERT);
    if (publish) group.property.apply();
    return ok;
}

bool TyrTelescope::readSwitchConfiguration(SwitchConfiguration &group, const std::string &status)
{
    std::string reply;
    bool ok = group.statusFlag ? !status.empty() : managedCommand(group.read, &reply);
    int index = group.statusFlag ? (has(status, group.statusFlag) ? 1 : 0) : -1;
    if (!group.statusFlag) for (size_t i = 0; i < group.values.size(); ++i) if (reply == group.values[i]) index = int(i);
    if (ok && index >= 0) { group.property.reset(); group.property[index].setState(ISS_ON); }
    else ok = false;
    group.property.setState(ok ? IPS_OK : IPS_ALERT);
    group.property.apply();
    return ok;
}

bool TyrTelescope::refreshConfiguration(const std::string &section)
{
    bool ok = true;
    if (section == "ALL" || section == "MOUNT" || section == "AXES") {
        std::string status;
        ok = managedCommand("GU", &status);
        for (auto &group : configurationNumbers) if (section != "AXES" || group.axis) ok = readNumberConfiguration(group) && ok;
        for (auto &group : configurationSwitches) if (section != "AXES" || group.read == "GXA0" || group.read == "GXEM")
            ok = readSwitchConfiguration(group, status) && ok;
        std::string alignment;
        if (managedCommand("A?", &alignment) && alignment.size() == 3) {
            const int maximum = alignment[0] - '0';
            const int current = alignment[1] - '0', required = alignment[2] - '0';
            const std::string description = current == 0 ? "No alignment sequence in progress" :
                "Accepted " + std::to_string(std::max(0, current - 1)) + " of " + std::to_string(required);
            ConfigurationInfoTP[2].setText(description.c_str());
            if (maximum >= 1 && maximum <= 9) { AlignmentStarsNP[0].setMax(maximum); AlignmentStarsNP.apply(); }
        }
        ConfigurationInfoTP.setState(ok ? IPS_OK : IPS_ALERT); ConfigurationInfoTP.apply();
        Json diagnostic;
        if (usbOperation("D", diagnostic, true) && diagnostic.is_object() && diagnostic.contains("uptimeMs")) {
            BootDiagnosticTP[0].setText(diagnostic["uptimeMs"].dump().c_str());
            BootDiagnosticTP[1].setText(diagnostic.value("resetReason", "unknown").c_str());
        }
        std::string error;
        if (managedCommand("GE", &error)) BootDiagnosticTP[2].setText(error.c_str());
        BootDiagnosticTP.setState(IPS_OK); BootDiagnosticTP.apply();
    }
    if (networkSupported && (section == "ALL" || section == "NETWORK")) ok = readNetworkConfiguration() && ok;
    if (bluetoothSupported && (section == "ALL" || section == "BLUETOOTH")) ok = readBluetoothConfiguration() && ok;
    return ok;
}

bool TyrTelescope::handleConfigurationNumber(const char *name, double values[], char *names[], int count)
{
    if (AlignmentStarsNP.isNameMatch(name)) {
        bool ok = count == 1 && std::isfinite(values[0]) && values[0] == std::round(values[0]) &&
            values[0] >= 1 && values[0] <= AlignmentStarsNP[0].getMax();
        if (ok) AlignmentStarsNP.update(values, names, count);
        AlignmentStarsNP.setState(ok ? IPS_OK : IPS_ALERT); AlignmentStarsNP.apply(); return true;
    }
    if (NetworkChannelNP.isNameMatch(name)) {
        bool ok = networkLoaded && count == 1 && values[0] >= 1 && values[0] <= 13 && values[0] == std::round(values[0]);
        if (ok) NetworkChannelNP.update(values, names, count);
        NetworkChannelNP.setState(ok ? IPS_BUSY : IPS_ALERT); NetworkChannelNP.apply(); return true;
    }
    for (auto &group : configurationNumbers) if (group.property.isNameMatch(name)) {
        bool ok = count > 0 && isConnected() && (!group.stopped || requireStopped(group.axis || group.reads[0] == "h?"));
        if (ok) ok = readNumberConfiguration(group, false);
        std::vector<double> candidate;
        std::vector<bool> changed(group.property.size(), false);
        for (const auto &entry : group.property) candidate.push_back(entry.getValue());
        for (int i = 0; i < count && ok; ++i) {
            int index = -1;
            for (size_t j = 0; j < group.property.size(); ++j) if (group.property[j].isNameMatch(names[i])) index = int(j);
            ok = index >= 0 && std::isfinite(values[i]);
            if (!ok) break;
            const auto &field = group.property[index];
            ok = values[i] >= field.getMin() && values[i] <= field.getMax();
            if (ok) { candidate[index] = values[i]; changed[index] = true; }
        }
        if (ok && group.axis) {
            ok = candidate[2] < candidate[3] && (candidate[1] == -1 || candidate[1] == -2);
            std::ostringstream commandText;
            commandText.imbue(std::locale::classic());
            commandText << "SXA" << group.axis << ',';
            for (size_t i = 0; i < candidate.size(); ++i) { if (i) commandText << ','; commandText << std::setprecision(12) << candidate[i]; }
            if (ok) ok = managedCommand(commandText.str());
        } else if (ok) {
            // Validate every requested field before sending any write.
            std::vector<std::string> commands;
            for (size_t i = 0; i < candidate.size() && ok; ++i) if (changed[i]) {
                double scaled = candidate[i] * group.factors[i];
                const bool clock = group.reads[0] == "GG";
                const bool floating = group.reads[0] == "GXTR";
                if (!clock && !floating && std::abs(scaled - std::round(scaled)) > 1e-6) { ok = false; break; }
                if (clock && std::abs(candidate[i] * 4 - std::round(candidate[i] * 4)) > 1e-6) { ok = false; break; }
                commands.push_back(group.writes[i] + (clock ? timezoneText(candidate[i]) : decimal(scaled)));
            }
            if (ok) for (const auto &command : commands) if (!managedCommand(command)) { ok = false; break; }
        }
        if (isConnected()) ok = readNumberConfiguration(group) && ok;
        group.property.setState(ok ? IPS_OK : IPS_ALERT); group.property.apply();
        if (ok && group.axis) LOG_INFO("Axis settings saved. Restart Tyr to apply motor changes.");
        return true;
    }
    return false;
}

bool TyrTelescope::handleConfigurationSwitch(const char *name, ISState states[], char *names[], int count)
{
    if (RefreshConfigurationSP.isNameMatch(name)) {
        RefreshConfigurationSP.update(states, names, count);
        const int selected = RefreshConfigurationSP.findOnSwitchIndex();
        const bool ok = isConnected() && selected >= 0 && refreshConfiguration(RefreshConfigurationSP[selected].getName());
        RefreshConfigurationSP.reset(); RefreshConfigurationSP.setState(ok ? IPS_OK : IPS_ALERT); RefreshConfigurationSP.apply(); return true;
    }
    if (ControllerActionsSP.isNameMatch(name)) {
        ControllerActionsSP.update(states, names, count);
        const int selected = ControllerActionsSP.findOnSwitchIndex();
        bool ok = isConnected() && selected >= 0;
        if (ok && selected == 0) ok = requireStopped() && managedCommand("A" + std::to_string(int(AlignmentStarsNP[0].getValue())));
        if (ok && selected == 1) ok = managedCommand("A+");
        if (ok && selected == 2) ok = managedCommand("AW");
        if (ok && selected == 3) ok = managedCommand("SX99,1");
        if (ok && selected == 4) { Json data; ok = requireStopped(true) && usbOperation("R", data); }
        ControllerActionsSP.reset(); ControllerActionsSP.setState(ok ? IPS_OK : IPS_ALERT); ControllerActionsSP.apply();
        if (ok && selected == 4) { Disconnect(); setConnected(false); updateProperties(); }
        else refreshConfiguration("MOUNT");
        return true;
    }
    for (auto &group : configurationSwitches) if (group.property.isNameMatch(name)) {
        const int previous = group.property.findOnSwitchIndex();
        group.property.update(states, names, count);
        const int selected = group.property.findOnSwitchIndex();
        bool ok = isConnected() && selected >= 0 && (!group.stopped || requireStopped(true)) && managedCommand(group.writes[selected]);
        std::string status;
        if (managedCommand("GU", &status)) ok = readSwitchConfiguration(group, status) && ok;
        else ok = false;
        if (!ok && status.empty() && previous >= 0) { group.property.reset(); group.property[previous].setState(ISS_ON); }
        group.property.setState(ok ? IPS_OK : IPS_ALERT); group.property.apply(); return true;
    }
    if (NetworkFlagsSP.isNameMatch(name) || NetworkDhcpSP.isNameMatch(name)) {
        auto &property = NetworkFlagsSP.isNameMatch(name) ? NetworkFlagsSP : NetworkDhcpSP;
        if (networkLoaded) property.update(states, names, count);
        property.setState(networkLoaded ? IPS_BUSY : IPS_ALERT); property.apply(); return true;
    }
    if (NetworkApplySP.isNameMatch(name)) {
        NetworkApplySP.update(states, names, count);
        const bool ok = NetworkApplySP.findOnSwitchIndex() == 0 && applyNetworkConfiguration();
        NetworkApplySP.reset(); NetworkApplySP.setState(ok ? IPS_OK : IPS_ALERT); NetworkApplySP.apply(); return true;
    }
    if (BluetoothActionsSP.isNameMatch(name)) {
        BluetoothActionsSP.update(states, names, count);
        const int selected = BluetoothActionsSP.findOnSwitchIndex();
        static const char *commands[] = {"benable", "bdisable", "bscan", "bforget"};
        Json data;
        const bool ok = bluetoothSupported && isConnected() && selected >= 0 && usbOperation(commands[selected], data);
        BluetoothActionsSP.reset(); BluetoothActionsSP.setState(ok ? IPS_OK : IPS_ALERT); BluetoothActionsSP.apply();
        if (ok) readBluetoothConfiguration();
        return true;
    }
    return false;
}

bool TyrTelescope::handleConfigurationText(const char *name, char *texts[], char *names[], int count)
{
    if (BluetoothSelectionTP.isNameMatch(name)) {
        BluetoothSelectionTP.update(texts, names, count);
        const std::string address = BluetoothSelectionTP[0].getText(), type = BluetoothSelectionTP[1].getText();
        const std::regex mac("^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$");
        Json data;
        const bool ok = bluetoothSupported && std::regex_match(address, mac) && type.size() == 1 && type[0] >= '0' && type[0] <= '3' &&
            usbOperation("bselect," + address + ',' + type, data);
        BluetoothSelectionTP.setState(ok ? IPS_OK : IPS_ALERT); BluetoothSelectionTP.apply();
        if (ok) readBluetoothConfiguration();
        return true;
    }
    if (NetworkAPTP.isNameMatch(name) || NetworkPasswordsTP.isNameMatch(name)) {
        auto &property = NetworkAPTP.isNameMatch(name) ? NetworkAPTP : NetworkPasswordsTP;
        if (networkLoaded) property.update(texts, names, count);
        if (NetworkPasswordsTP.isNameMatch(name)) {
            for (int i = 0; i < count && networkLoaded; ++i)
                for (size_t j = 0; j < property.size(); ++j)
                    if (property[j].isNameMatch(names[i])) pendingNetworkPasswords[j] = texts[i];
            for (auto &entry : property) entry.setText("");
        }
        property.setState(networkLoaded ? IPS_BUSY : IPS_ALERT);
        // Write-only passwords must never be echoed as property values.
        if (!NetworkPasswordsTP.isNameMatch(name)) property.apply();
        return true;
    }
    for (auto &property : NetworkStationTP) if (property.isNameMatch(name)) {
        if (networkLoaded) property.update(texts, names, count);
        property.setState(networkLoaded ? IPS_BUSY : IPS_ALERT); property.apply(); return true;
    }
    return false;
}

bool TyrTelescope::readNetworkConfiguration()
{
    Json data;
    bool ok = usbOperation("N", data);
    try {
        if (!ok) throw std::runtime_error("Network read failed");
        const char *flags[] = {"apEnabled", "stationEnabled", "fallback"};
        const char *ap[] = {"apSsid", "apIp", "apGateway", "apSubnet"};
        const char *fields[] = {"host", "ssid", "ip", "gateway", "subnet", "target"};
        // Validate the entire response before modifying any visible property.
        if (!data.at("stations").is_array() || data.at("stations").size() != 3) throw std::runtime_error("Invalid station count");
        for (const auto *key : flags) data.at(key).get<bool>();
        for (const auto *key : ap) data.at(key).get<std::string>();
        data.at("apChannel").get<int>();
        for (const auto &station : data.at("stations")) {
            station.at("dhcp").get<bool>();
            for (const auto *key : fields) station.at(key).get<std::string>();
        }
        for (int i = 0; i < 3; ++i) NetworkFlagsSP[i].setState(data[flags[i]].get<bool>() ? ISS_ON : ISS_OFF);
        for (int i = 0; i < 4; ++i) NetworkAPTP[i].setText(data[ap[i]].get<std::string>().c_str());
        NetworkChannelNP[0].setValue(data["apChannel"].get<int>());
        for (int station = 0; station < 3; ++station) {
            NetworkDhcpSP[station].setState(data["stations"][station]["dhcp"].get<bool>() ? ISS_ON : ISS_OFF);
            for (int i = 0; i < 6; ++i) NetworkStationTP[station][i].setText(data["stations"][station][fields[i]].get<std::string>().c_str());
        }
        for (auto &entry : NetworkPasswordsTP) entry.setText("");
        pendingNetworkPasswords.fill("");
        networkLoaded = true;
    } catch (const std::exception &) { ok = false; }
    const auto state = ok ? IPS_OK : IPS_ALERT;
    NetworkFlagsSP.setState(state); NetworkFlagsSP.apply(); NetworkDhcpSP.setState(state); NetworkDhcpSP.apply();
    NetworkAPTP.setState(state); NetworkAPTP.apply(); NetworkChannelNP.setState(state); NetworkChannelNP.apply();
    for (auto &property : NetworkStationTP) { property.setState(state); property.apply(); }
    return ok;
}

bool TyrTelescope::applyNetworkConfiguration()
{
    if (!isConnected() || !networkSupported || !networkLoaded || !requireStopped(true)) return false;
    std::vector<std::pair<std::string, std::string>> fields;
    const char *flags[] = {"apEnabled", "stationEnabled", "fallback"};
    const char *ap[] = {"apSsid", "apIp", "apGateway", "apSubnet"};
    const char *stationFields[] = {"host", "ssid", "ip", "gateway", "subnet", "target"};
    for (int i = 0; i < 3; ++i) fields.emplace_back(flags[i], NetworkFlagsSP[i].getState() == ISS_ON ? "1" : "0");
    for (int i = 0; i < 4; ++i) {
        const std::string value = NetworkAPTP[i].getText();
        if ((i == 0 && (value.empty() || value.size() > 31)) || (i > 0 && !validIP(value))) return false;
        fields.emplace_back(ap[i], value);
    }
    fields.emplace_back("apChannel", std::to_string(int(NetworkChannelNP[0].getValue())));
    for (int station = 0; station < 3; ++station) {
        const auto prefix = "s" + std::to_string(station + 1) + '.';
        fields.emplace_back(prefix + "dhcp", NetworkDhcpSP[station].getState() == ISS_ON ? "1" : "0");
        for (int i = 0; i < 6; ++i) {
            const std::string value = NetworkStationTP[station][i].getText();
            if ((i <= 1 && value.size() > 31) || (i > 1 && !validIP(value))) return false;
            fields.emplace_back(prefix + stationFields[i], value);
        }
    }
    const char *passwordKeys[] = {"apPassword", "s1.password", "s2.password", "s3.password", "masterPassword"};
    for (int i = 0; i < 5; ++i) {
        const std::string value = pendingNetworkPasswords[i];
        if (value.size() > 31) return false;
        if (!value.empty()) fields.emplace_back(passwordKeys[i], value);
    }
    Json data;
    if (!usbOperation("nbegin", data)) return false;
    bool ok = true;
    for (const auto &[key, value] : fields) if (!usbOperation("n" + key + ',' + hexText(value), data)) { ok = false; break; }
    if (ok) ok = usbOperation("ncommit", data);
    if (!ok) usbOperation("nabort", data, true);
    for (auto &entry : NetworkPasswordsTP) entry.setText("");
    pendingNetworkPasswords.fill("");
    if (ok) {
        if (isSimulation()) {
            for (const auto &[key, value] : fields) {
                const bool boolean = key == "apEnabled" || key == "stationEnabled" || key == "fallback" || key.find(".dhcp") != std::string::npos;
                Json parsed = boolean ? Json(value == "1") : key == "apChannel" ? Json(std::stoi(value)) : Json(value);
                if (key.size() > 3 && key[0] == 's' && key[2] == '.') simulationNetwork["stations"][key[1] - '1'][key.substr(3)] = parsed;
                else simulationNetwork[key] = parsed;
            }
        }
        ok = readNetworkConfiguration(); LOG_INFO("Wi-Fi settings saved. Restart Tyr to apply them.");
    }
    return ok;
}

bool TyrTelescope::readBluetoothConfiguration()
{
    Json data;
    bool ok = usbOperation("B", data);
    try {
        if (!ok) throw std::runtime_error("Bluetooth read failed");
        const auto selected = data.at("selected");
        const auto summary = std::string(data.at("connected").get<bool>() ? "Connected" : data.at("enabled").get<bool>() ? "Enabled" : "Disabled") +
            (data.at("armed").get<bool>() ? "; controls armed" : "; release controls to arm") + "; " + data.value("message", "");
        BluetoothStateTP[0].setText(summary.c_str());
        BluetoothStateTP[1].setText((selected.at("name").get<std::string>() + " " + selected.at("address").get<std::string>()).c_str());
        BluetoothStateTP[2].setText(data.at("devices").dump().c_str());
    } catch (const std::exception &) { ok = false; }
    BluetoothStateTP.setState(ok ? IPS_OK : IPS_ALERT); BluetoothStateTP.apply();
    return ok;
}
