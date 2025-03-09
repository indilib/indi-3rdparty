/******************************************************************************
 Copyright(c) 2014/2023 Jasem Mutlaq/Ed Lee. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

/******************************************************************************
Driver for the Observatory Control System (OCS), an open source project created
by Howard Dutton. Refer to:
https://onstep.groups.io/g/onstep-ocs/wiki
https://github.com/hjd1964/OCS

Capabilites include: Roll off roof, dome roof, weather monitoring,
themostat control, power device control, lighting control.
Hardware communication is via a simple text protocol similar to the LX200.
USB and network connections supported.
*******************************************************************************/

// Debug only

// #include <signal.h>
// #include <unistd.h>

// Debug only end

#include "ocs.h"
#include "termios.h"

#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>

// Custom tabs
#define STATUS_TAB "Status"
#define THERMOSTAT_TAB "Thermostat"
#define POWER_TAB "Power"
#define LIGHTS_TAB "Lights"
#define WEATHER_TAB "Weather"
#define MANUAL_TAB "Manual"

// Mutex for communications
std::mutex ocsCommsLock;

// Declare an auto pointer to OCS.
std::unique_ptr<OCS> ocs(new OCS());

OCS::OCS() : INDI::Dome(), WI(this)
{
    // Debug only
    // Halts the process at this point. Allows remote debugger to attach which is required
    // when launching the driver from a client eg. Ekos
    // kill(getpid(), SIGSTOP);
    // Debug only end

    setVersion(1, 1);
    SetDomeCapability(DOME_CAN_ABORT | DOME_HAS_SHUTTER);
    SlowTimer.callOnTimeout(std::bind(&OCS::SlowTimerHit, this));
}

/*******************************************************
 * INDI is asking us for our default device name.
 * Must match Ekos selection menu and ParkData.xml names
 *******************************************************/
const char *OCS::getDefaultName()
{
    return (const char *)"OCS";
}

/***************************************************************
 * Called from Dome, BaseDevice to establish contact with device
 **************************************************************/
bool OCS::Handshake()
{
    bool handshake_status = false;

    if (PortFD > 0) {
        Connection::Interface *activeConnection = getActiveConnection();
        if (!activeConnection->name().compare("CONNECTION_TCP")) {
            LOG_INFO("Network based connection, detection timeouts set to 1 second");
            OCSTimeoutMicroSeconds = 0;
            OCSTimeoutSeconds = 1;
        }
        else {
            LOG_INFO("Non-Network based connection, detection timeouts set to 0.1 seconds");
            OCSTimeoutMicroSeconds = 100000;
            OCSTimeoutSeconds = 0;
        }

        char handshake_response[RB_MAX_LEN] = {0};
        handshake_status = getCommandSingleCharErrorOrLongResponse(PortFD, handshake_response,
                                                                   OCS_handshake);
        if (strcmp(handshake_response, "OCS") == 0)
        {
            LOG_DEBUG("OCS handshake established");
            handshake_status = true;
            GetCapabilites();
            SlowTimer.start(60000);
        }
        else {
            LOGF_DEBUG("OCS handshake error, reponse was: %s", handshake_response);
        }
    }
    else {
        LOG_ERROR("OCS can't handshake, device not connected");
    }

    return handshake_status;
}

/**************************************************************
 * Query connected OCS for capabilities - called from Handshake
 **************************************************************/
void OCS::GetCapabilites()
{
    // Get firmware version
    char OCS_firmware_response[RB_MAX_LEN] = {0};
    int OCS_firmware_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, OCS_firmware_response,
                                                                             OCS_get_firmware);
    if (OCS_firmware_error_or_fail > 1) {
        IUSaveText(&Status_ItemsT[STATUS_FIRMWARE], OCS_firmware_response);
        IDSetText(&Status_ItemsTP, nullptr);
        LOGF_DEBUG("OCS version: %s", OCS_firmware_response);
    } else {
        LOG_ERROR("OCS version not retrieved");
    }
    if (std::stof(OCS_firmware_response) < minimum_OCS_fw) {
        LOGF_WARN("OCS version %s is lower than this driver expects (%1.1f). Behaviour is unknown.", OCS_firmware_response, minimum_OCS_fw);
    }

    // Get dome presence
    char OCS_dome_present_response[RB_MAX_LEN] = {0};
    int OCS_dome_present_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, OCS_dome_present_response,
                                                                                 OCS_get_dome_status);
    if (OCS_dome_present_error_or_fail > 0) {
        SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK | DOME_CAN_ABS_MOVE | DOME_CAN_SYNC | DOME_HAS_SHUTTER);
        setDomeState(DOME_UNKNOWN);
        hasDome = true;
        LOG_INFO("OCS has dome");
    } else {
        LOG_INFO("OCS does not have dome");
    }

    // Get roof delays
    char roof_timeout_response[RB_MAX_LEN] = {0};
    int roof_timeout_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, roof_timeout_response,
                                                                              OCS_get_timeouts);
    if (roof_timeout_error_or_fail > 1) {
        char *split;
        split = strtok(roof_timeout_response, ",");
        if (charToInt(split) != conversion_error) {
            ROOF_TIME_PRE_MOTION = charToInt(split);
        }
        split = strtok(NULL, ",");
        if (charToInt(split) != conversion_error) {
            ROOF_TIME_POST_MOTION = charToInt(split);
        }
    }
    else {
        LOGF_WARN("Communication error on get roof delays %s", OCS_get_timeouts);
    }

    // Get the Obsy Thermostat presence
    char thermostat_status_response[RB_MAX_LEN] = {0};
    int thermostat_status_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, thermostat_status_response,
                                                                                   OCS_get_thermostat_status);
    if (thermostat_status_error_or_fail > 1) { //> 1 as an OCS error would be 1 char in response
        if (strcmp(thermostat_status_response, "nan,nan") == 0) {
            thermostat_controls_enabled = false;
            LOG_INFO("OCS does not have a thermostat, disabling tab");
        } else {
            thermostat_controls_enabled = true;
            LOG_INFO("OCS has a thermostat, enabling tab");

            // Get thermostat relay definitions
            char thermostat_relay_definitions_response[RB_MAX_LEN] = {0};
            int thermostat_relay_definitions_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, thermostat_relay_definitions_response,
                                                                                                     OCS_get_thermostat_definitions);
            if (thermostat_relay_definitions_error_or_fail > 1) {
                char *split;
                split = strtok(thermostat_relay_definitions_response, ",");
                for (int relayNo = 0; relayNo < THERMOSTAT_RELAY_COUNT; relayNo ++) {
                    if (charToInt(split) != conversion_error) {
                        thermostat_relays[relayNo] = charToInt(split);
                    }
                    split = strtok(NULL, ",");
                }
            }
        }
    } else if (strcmp(thermostat_status_response, "0") == 0) {
        LOG_INFO("OCS does not have a thermostat, disabling tab");
    }

    // Get power relay definitions
    char power_relay_definitions_response[RB_MAX_LEN] = {0};
    int power_relay_definitions_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, power_relay_definitions_response,
                                                                                        OCS_get_power_definitions);
    if (power_relay_definitions_error_or_fail > 1) {
        char *split;
        split = strtok(power_relay_definitions_response, ",");
        for (int deviceNo = 0; deviceNo < POWER_DEVICE_COUNT; deviceNo ++) {
            if (charToInt(split) != conversion_error) {
                power_device_relays[deviceNo] = charToInt(split);
            }
            split = strtok(NULL, ",");
        }
        // Defined devices have a positive integer relay definition, undefined return -1
        // so we can sum these to check if any are defined, if not then keep tab hidden
        int powerDisabled = 0;
        for (int deviceNo = 1; deviceNo < POWER_DEVICE_COUNT; deviceNo ++) {
            powerDisabled += power_device_relays[deviceNo];
        }
        if (powerDisabled != (-1 * POWER_DEVICE_COUNT)) {
            power_tab_enabled = true;
            LOG_INFO("OCS has power device(s), enabling tab");
            for (int deviceNo = 1; deviceNo < POWER_DEVICE_COUNT; deviceNo ++) {
                if (power_device_relays[(deviceNo - 1)] != -1) {
                    char power_relay_name_response[RB_MAX_LEN] = {0};
                    char get_power_device_name_command[CMD_MAX_LEN] = {0};
                    sprintf(get_power_device_name_command, "%s%i%s",
                            OCS_get_power_names_part, deviceNo, OCS_command_terminator);
                    int power_relay_name_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, power_relay_name_response,
                                                                                                 get_power_device_name_command);
                    if (power_relay_name_error_or_fail > 0) {
                        switch(deviceNo) {
                            case 1:
                                indi_strlcpy(POWER_DEVICE1_NAME, power_relay_name_response, sizeof(POWER_DEVICE1_NAME));
                                IUSaveText(&Power_Device_Name1T[0], POWER_DEVICE1_NAME);
                                IDSetText(&Power_Device_Name1TP, nullptr);
                                break;
                            case 2:
                                indi_strlcpy(POWER_DEVICE2_NAME, power_relay_name_response, sizeof(POWER_DEVICE2_NAME));
                                IUSaveText(&Power_Device_Name2T[0], POWER_DEVICE2_NAME);
                                IDSetText(&Power_Device_Name2TP, nullptr);
                                break;
                            case 3:
                                indi_strlcpy(POWER_DEVICE3_NAME, power_relay_name_response, sizeof(POWER_DEVICE3_NAME));
                                IUSaveText(&Power_Device_Name3T[0], POWER_DEVICE3_NAME);
                                IDSetText(&Power_Device_Name3TP, nullptr);
                                break;
                            case 4:
                                indi_strlcpy(POWER_DEVICE4_NAME, power_relay_name_response, sizeof(POWER_DEVICE4_NAME));
                                IUSaveText(&Power_Device_Name4T[0], POWER_DEVICE4_NAME);
                                IDSetText(&Power_Device_Name4TP, nullptr);
                                break;
                            case 5:
                                indi_strlcpy(POWER_DEVICE5_NAME, power_relay_name_response, sizeof(POWER_DEVICE5_NAME));
                                IUSaveText(&Power_Device_Name5T[0], POWER_DEVICE5_NAME);
                                IDSetText(&Power_Device_Name5TP, nullptr);
                                break;
                            case 6:
                                indi_strlcpy(POWER_DEVICE6_NAME, power_relay_name_response, sizeof(POWER_DEVICE6_NAME));
                                IUSaveText(&Power_Device_Name6T[0], POWER_DEVICE6_NAME);
                                IDSetText(&Power_Device_Name6TP, nullptr);
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        } else {
            LOG_INFO("OCS does not have power device(s), disabling tab");
        }
    } else if (strcmp(power_relay_definitions_response, "0") == 0) {
        LOG_INFO("OCS does not have power device(s), disabling tab");
    }

    // Get light relay definitions
    char light_relay_definitions_response[RB_MAX_LEN] = {0};
    int light_relay_definitions_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, light_relay_definitions_response,
                                                                                        OCS_get_light_definitions);
    if (light_relay_definitions_error_or_fail > 1) {
        char *split;
        split = strtok(light_relay_definitions_response, ",");
        for (int lrelay = 0; lrelay < LIGHT_COUNT; lrelay ++) {
            if (charToInt(split) != conversion_error) {
                light_relays[lrelay] = charToInt(split);
            }
            split = strtok(NULL, ",");
        }
        // Defined lights have a positive integer relay definition, undefined return -1
        // so we can sum these to check if any are defined, if not then keep tab hidden
        int lightsDisabled = 0;
        for (int lrelay = 1; lrelay < LIGHT_COUNT; lrelay ++) {
            lightsDisabled += light_relays[lrelay];
        }
        if (lightsDisabled != (-1 * LIGHT_COUNT)) {
            lights_tab_enabled = true;
            LOG_INFO("OCS has light(s), enabling tab");
        } else {
            LOG_INFO("OCS does not have light(s), disabling tab");
        }
    } else if (strcmp(light_relay_definitions_response, "0") == 0) {
        LOG_INFO("OCS does not have light(s), disabling tab");
    }

    // Get available weather measurements
    for (int measurement = 0; measurement < WEATHER_MEASUREMENTS_COUNT; measurement ++) {
        char measurement_reponse[RB_MAX_LEN];
        char measurement_command[CMD_MAX_LEN];
        switch(measurement) {
            case WEATHER_TEMPERATURE:
                indi_strlcpy(measurement_command, OCS_get_outside_temperature, sizeof(measurement_command));
                break;
            case WEATHER_PRESSURE:
                indi_strlcpy(measurement_command, OCS_get_pressure, sizeof(measurement_command));
                break;
            case WEATHER_HUMIDITY:
                indi_strlcpy(measurement_command, OCS_get_humidity, sizeof(measurement_command));
                break;
            case WEATHER_WIND:
                indi_strlcpy(measurement_command, OCS_get_wind_speed, sizeof(measurement_command));
                break;
            case WEATHER_RAIN:
                indi_strlcpy(measurement_command, OCS_get_rain_sensor_status, sizeof(measurement_command));
                break;
            case WEATHER_DIFF_SKY_TEMP:
                indi_strlcpy(measurement_command, OCS_get_sky_diff_temperature, sizeof(measurement_command));
                break;
            case WEATHER_CLOUD:
                indi_strlcpy(measurement_command, OCS_get_cloud_description, sizeof(measurement_command));
                break;
            case WEATHER_SKY:
                indi_strlcpy(measurement_command, OCS_get_sky_quality, sizeof(measurement_command));
                break;
            case WEATHER_SKY_TEMP:
                indi_strlcpy(measurement_command, OCS_get_sky_IR_temperature, sizeof(measurement_command));
                break;
            default:
                break;
        }

        int measurement_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, measurement_reponse,
                                                                                measurement_command);
        if (measurement_error_or_fail > 1 && strcmp(measurement_reponse, "N/A") != 0 &&
            strcmp(measurement_reponse, "NAN") != 0 && strcmp(measurement_reponse, "0") != 0) {
            weather_enabled[measurement] = 1;
        } else {
            weather_enabled[measurement] = 0;
        }
    }

    // Available weather measurements are now defined as = 1, unavailable as = 0
    // so we can sum these to check if any are defined, if not then keep tab disabled
    int weatherDisabled = 0;
    for (int wmeasure = 1; wmeasure < WEATHER_MEASUREMENTS_COUNT; wmeasure ++) {
        weatherDisabled += weather_enabled[wmeasure];
    }
    if (weatherDisabled > 0) {
        weather_tab_enabled = true;
        LOG_INFO("OCS has weather sensor(s), enabling tab");
        // If a weather measurement that has a safety limit set in OCS is active then get that limit
        if (weather_enabled[WEATHER_WIND] || weather_enabled[WEATHER_DIFF_SKY_TEMP]) {
            char threshold_reponse[RB_MAX_LEN];
            int threshold_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, threshold_reponse,
                                                                                  OCS_get_weather_thresholds);
            if (threshold_error_or_fail > 1 ) { //> 1 as an OCS error would be 1 char in response
                char *split;
                split = strtok(threshold_reponse, ",");
                if (strcmp(split, "N/A") != 0) {
                    if (charToInt(split) != conversion_error) {
                        wind_speed_threshold = charToInt(split);
                    }
                }
                split = strtok(NULL, ",");
                if (strcmp(split, "N/A") != 0) {
                    if (charToInt(split) != conversion_error) {
                        diff_temp_threshold = charToInt(split);
                    }
                }
            } else {
                LOGF_WARN("Communication error on get Weather thresholds %s",
                          OCS_get_weather_thresholds);
            }
        }

        // Loop through only the first 6 measurements rather than WEATHER_MEASUREMENTS_COUNT
        // as only these are usable for safety status with limits
        for (int measurement = 0; measurement < 6; measurement ++) {
            if (weather_enabled[measurement] == 1) {
                switch(measurement) {
                    case WEATHER_TEMPERATURE:
                        addParameter("WEATHER_TEMPERATURE", "Temperature °C", -10, 40, 15);
                        setCriticalParameter("WEATHER_TEMPERATURE");
                        break;
                    case WEATHER_PRESSURE:
                        addParameter("WEATHER_PRESSURE", "Pressure mbar", 970, 1050, 10);
                        setCriticalParameter("WEATHER_PRESSURE");
                        break;
                    case WEATHER_HUMIDITY:
                        addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 95, 15);
                        setCriticalParameter("WEATHER_HUMIDITY");
                        break;
                    case WEATHER_WIND:
                        addParameter("WEATHER_WIND", "Wind kph", 0, wind_speed_threshold, 15);
                        setCriticalParameter("WEATHER_WIND");
                        break;
                    case WEATHER_RAIN:
                        addParameter("WEATHER_RAIN", "Rain state", 3, 3, 67);
                        setCriticalParameter("WEATHER_RAIN");
                        break;
                    case WEATHER_DIFF_SKY_TEMP:
                        addParameter("WEATHER_SKY_DIFF_TEMP", "Sky vs Cloud °C", -50, diff_temp_threshold, 15);
                        setCriticalParameter("WEATHER_SKY_DIFF_TEMP");
                            break;
                    default:
                        break;
                }
            }
        }
    } else {
        LOG_INFO("OCS does not have weather sensor(s), disabling tab");
    }

    // Call the slow property update once as this is startup and we want to populate now
    SlowTimerHit();
}

/**********************************************************************
** INDI request to init properties. Connected Define properties to Ekos
***********************************************************************/
bool OCS::initProperties()
{
    INDI::Dome::initProperties();

    setDriverInterface(DOME_INTERFACE | WEATHER_INTERFACE);

    // Main control tab controls
    //--------------------------
    IUFillTextVector(&ShutterStatusTP, ShutterStatusT, 1, getDeviceName(), "SHUTTER_STATUS", "Status",
               MAIN_CONTROL_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&ShutterStatusT[0], "ROOF_SHUTTER_STATUS", "Roof/Shutter", "---");
    IUFillSwitchVector(&DomeControlsSP, DomeControlsS, DOME_CONTROL_COUNT, getDeviceName(), "DOME", "Additional controls",
                       MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&DomeControlsS[DOME_SET_PARK], "SET_PARK_SW", "Set Park", ISS_OFF);
    IUFillSwitch(&DomeControlsS[DOME_RETURN_HOME], "RETURN_HOME_SW", "Return  Home", ISS_OFF);
    IUFillSwitch(&DomeControlsS[DOME_SET_HOME], "RESET_HOME_SW", "At Home (Reset)", ISS_OFF);
    IUFillTextVector(&DomeStatusTP, DomeStatusT, 1, getDeviceName(), "DOME_STATUS", "Status",
               MAIN_CONTROL_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&DomeStatusT[0], "DOME_STATUS", "Dome", "---");

    // Status tab controls
    //--------------------
    IUFillTextVector(&Status_ItemsTP, Status_ItemsT, STATUS_ITEMS_COUNT, getDeviceName(), "Status", "OCS Status",
                     STATUS_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Status_ItemsT[STATUS_FIRMWARE], "FIRMWARE_VERSION", "Firmware version", "---");
    IUFillText(&Status_ItemsT[STATUS_ROOF_LAST_ERROR], "ROOF_LAST_ERROR", "Roof last error", "---");
    IUFillText(&Status_ItemsT[STATUS_MAINS], "MAINS_STATUS", "Mains status", "---");
    IUFillText(&Status_ItemsT[STATUS_OCS_SAFETY], "OCS_SAFETY_STATUS", "OCS safety", "---");
    IUFillText(&Status_ItemsT[STATUS_MCU_TEMPERATURE], "MCU_TEMPERATURE", "MCU temperature °C", "---");

    // Thermostat tab controls
    //------------------------
    IUFillTextVector(&Thermostat_StatusTP, Thermostat_StatusT, THERMOSTAT_COUNT, getDeviceName(), "THERMOSTAT_STATUS", "Obsy Status",
                     THERMOSTAT_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Thermostat_StatusT[THERMOSTAT_TEMERATURE], "THERMOSTAT_TEMPERATURE", "Temperature °C", "---");
    IUFillText(&Thermostat_StatusT[THERMOSTAT_HUMIDITY], "THERMOSTAT_HUMIDITY", "Humidity %", "---");

    IUFillNumberVector(&Thermostat_heat_setpointNP, Thermostat_heat_setpointN, 1, getDefaultName(), "THERMOSTAT_HEAT_SETPOINT", "Heat setpoint",
                       THERMOSTAT_TAB, IP_RW, 60, IPS_OK);
    IUFillNumber(&Thermostat_heat_setpointN[1], "THERMOSTAT_HEAT_SEPOINT", "Heat °C (0=OFF)", "%.0f", 0, 40, 1, 0);
    IUFillNumberVector(&Thermostat_cool_setpointNP, Thermostat_cool_setpointN, 1, getDefaultName(), "THERMOSTAT_COOL_SETPOINT", "Cool setpoint",
                       THERMOSTAT_TAB, IP_RW, 60, IPS_OK);
    IUFillNumber(&Thermostat_cool_setpointN[1], "THERMOSTAT_COOL_SEPOINT", "Cool °C (0=OFF)", "%.0f", 0, 40, 1, 0);
    IUFillNumberVector(&Thermostat_humidity_setpointNP, Thermostat_humidity_setpointN, 1, getDefaultName(), "THERMOSTAT_HUMIDITY_SETPOINT", "Humidity setpoint",
                       THERMOSTAT_TAB, IP_RW, 60, IPS_OK);
    IUFillNumber(&Thermostat_humidity_setpointN[1], "THERMOSTAT_HUMIDITY_SEPOINT", "Dehumidify % (0=OFF)", "%.0f", 0, 80, 1, 0);

    IUFillSwitchVector(&Thermostat_heat_relaySP, Thermostat_heat_relayS, SWITCH_TOGGLE_COUNT, getDeviceName(), "Thermo_heat_relay", "Heat Relay",
                       THERMOSTAT_TAB, IP_RO, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Thermostat_heat_relayS[ON_SWITCH], "Heat_Relay_On", "ON", ISS_OFF);
    IUFillSwitch(&Thermostat_heat_relayS[OFF_SWITCH], "Heat_Relay_Off", "OFF", ISS_ON);
    IUFillSwitchVector(&Thermostat_cool_relaySP, Thermostat_cool_relayS, SWITCH_TOGGLE_COUNT, getDeviceName(), "Thermo_cool_relay", "Cool Relay",
                       THERMOSTAT_TAB, IP_RO, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Thermostat_cool_relayS[ON_SWITCH], "Cool_Relay_On", "ON", ISS_OFF);
    IUFillSwitch(&Thermostat_cool_relayS[OFF_SWITCH], "Cool_Relay_Off", "OFF", ISS_ON);
    IUFillSwitchVector(&Thermostat_humidity_relaySP, Thermostat_humidity_relayS, SWITCH_TOGGLE_COUNT, getDeviceName(), "Thermo_humidity_relay", "Rh Relay",
                       THERMOSTAT_TAB, IP_RO, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Thermostat_humidity_relayS[ON_SWITCH], "Humidity_Relay_On", "ON", ISS_OFF);
    IUFillSwitch(&Thermostat_humidity_relayS[OFF_SWITCH], "Humidity_Relay_Off", "OFF", ISS_ON);

    // Power devices tab controls
    //---------------------------
    IUFillSwitchVector(&Power_Device1SP, Power_Device1S, SWITCH_TOGGLE_COUNT, getDeviceName(), "POWER_DEVICE1", "Device 1",
                       POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Power_Device1S[ON_SWITCH], "POWER_DEVICE1_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power_Device1S[OFF_SWITCH], "POWER_DEVICE1_OFF", "OFF", ISS_ON);
    IUFillTextVector(&Power_Device_Name1TP, Power_Device_Name1T, 1, getDeviceName(), "POWER_DEVICE_1_NAME", "Device 1",
               POWER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Power_Device_Name1T[0], "DEVICE_1_NAME", "Name", "");

    IUFillSwitchVector(&Power_Device2SP, Power_Device2S, SWITCH_TOGGLE_COUNT, getDeviceName(), "POWER_DEVICE2", "Device 2",
                       POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Power_Device2S[ON_SWITCH], "POWER_DEVICE2_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power_Device2S[OFF_SWITCH], "POWER_DEVICE2_OFF", "OFF", ISS_ON);
    IUFillTextVector(&Power_Device_Name2TP, Power_Device_Name2T, 1, getDeviceName(), "POWER_DEVICE_2_NAME", "Device 2",
               POWER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Power_Device_Name2T[0], "DEVICE_2_NAME", "Name", "");

    IUFillSwitchVector(&Power_Device3SP, Power_Device3S, SWITCH_TOGGLE_COUNT, getDeviceName(), "POWER_DEVICE3", "Device 3",
                       POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Power_Device3S[ON_SWITCH], "POWER_DEVICE3_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power_Device3S[OFF_SWITCH], "POWER_DEVICE3_OFF", "OFF", ISS_ON);
    IUFillTextVector(&Power_Device_Name3TP, Power_Device_Name3T, 1, getDeviceName(), "POWER_DEVICE_3_NAME", "Device 3",
               POWER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Power_Device_Name3T[0], "DEVICE_3_NAME", "Name", "");

    IUFillSwitchVector(&Power_Device4SP, Power_Device4S, SWITCH_TOGGLE_COUNT, getDeviceName(), "POWER_DEVICE4", "Device 4",
                       POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Power_Device4S[ON_SWITCH], "POWER_DEVICE4_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power_Device4S[OFF_SWITCH], "POWER_DEVICE4_OFF", "OFF", ISS_ON);
    IUFillTextVector(&Power_Device_Name4TP, Power_Device_Name4T, 1, getDeviceName(), "POWER_DEVICE_4_NAME", "Device 4",
               POWER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Power_Device_Name4T[0], "DEVICE_4_NAME", "Name", "");

    IUFillSwitchVector(&Power_Device5SP, Power_Device5S, SWITCH_TOGGLE_COUNT, getDeviceName(), "POWER_DEVICE5", "Device 5",
                       POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Power_Device5S[ON_SWITCH], "POWER_DEVICE5_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power_Device5S[OFF_SWITCH], "POWER_DEVICE5_OFF", "OFF", ISS_ON);
    IUFillTextVector(&Power_Device_Name5TP, Power_Device_Name5T, 1, getDeviceName(), "POWER_DEVICE_5_NAME", "Device 5",
               POWER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Power_Device_Name5T[0], "DEVICE_5_NAME", "Name", "");

    IUFillSwitchVector(&Power_Device6SP, Power_Device6S, SWITCH_TOGGLE_COUNT, getDeviceName(), "POWER_DEVICE6", "Device 6",
                       POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&Power_Device6S[ON_SWITCH], "POWER_DEVICE6_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power_Device6S[OFF_SWITCH], "POWER_DEVICE6_OFF", "OFF", ISS_ON);
    IUFillTextVector(&Power_Device_Name6TP, Power_Device_Name6T, 1, getDeviceName(), "POWER_DEVICE_6_NAME", "Device 6",
               POWER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Power_Device_Name6T[0], "DEVICE_6_NAME", "Name", "");

    // Lights tab controls
    //--------------------
    IUFillSwitchVector(&LIGHT_WRWSP, LIGHT_WRWS, SWITCH_TOGGLE_COUNT, getDeviceName(), "LIGHT_WRW", "Warm Room White",
                       LIGHTS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&LIGHT_WRWS[ON_SWITCH], "WRW_ON", "ON", ISS_OFF);
    IUFillSwitch(&LIGHT_WRWS[OFF_SWITCH], "WRW_OFF", "OFF", ISS_ON);

    IUFillSwitchVector(&LIGHT_WRRSP, LIGHT_WRRS, SWITCH_TOGGLE_COUNT, getDeviceName(), "LIGHT_WRR", "Warm Room Red",
                       LIGHTS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&LIGHT_WRRS[ON_SWITCH], "WRR_ON", "ON", ISS_OFF);
    IUFillSwitch(&LIGHT_WRRS[OFF_SWITCH], "WRR_OFF", "OFF", ISS_ON);

    IUFillSwitchVector(&LIGHT_ORWSP, LIGHT_ORWS, SWITCH_TOGGLE_COUNT, getDeviceName(), "LIGHT_ORW", "Obsy White",
                       LIGHTS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&LIGHT_ORWS[ON_SWITCH], "ORW_ON", "ON", ISS_OFF);
    IUFillSwitch(&LIGHT_ORWS[OFF_SWITCH], "ORW_OFF", "OFF", ISS_ON);

    IUFillSwitchVector(&LIGHT_ORRSP, LIGHT_ORRS, SWITCH_TOGGLE_COUNT, getDeviceName(), "LIGHT_ORR", "Obsy Red",
                       LIGHTS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&LIGHT_ORRS[ON_SWITCH], "ORR_ON", "ON", ISS_OFF);
    IUFillSwitch(&LIGHT_ORRS[OFF_SWITCH], "ORR_OFF", "OFF", ISS_ON);

    IUFillSwitchVector(&LIGHT_OUTSIDESP, LIGHT_OUTSIDES, SWITCH_TOGGLE_COUNT, getDeviceName(), "LIGHT_OUTSIDE", "Outside",
                       LIGHTS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    IUFillSwitch(&LIGHT_OUTSIDES[ON_SWITCH], "OUTSIDE_ON", "ON", ISS_OFF);
    IUFillSwitch(&LIGHT_OUTSIDES[OFF_SWITCH], "OUTSIDE_OFF", "OFF", ISS_ON);

    // Weather tab controls - in addition to the WI managed controls - these are for display only
    WI::initProperties(WEATHER_TAB, WEATHER_TAB);

    IUFillTextVector(&Weather_CloudTP, Weather_CloudT, 1, getDeviceName(), "WEATHER_CLOUD", "Cloud",
                     WEATHER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Weather_CloudT[0], "WEATHER_CLOUD", "Desciption","---");
    IUFillTextVector(&Weather_SkyTP, Weather_SkyT, 1, getDeviceName(), "WEATHER_SKY", "Sky quality",
                     WEATHER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Weather_SkyT[0], "WEATHER_SKY", "mag/arc-sec\u00b2","---");
    IUFillTextVector(&Weather_Sky_TempTP, Weather_Sky_TempT, 1, getDeviceName(), "WEATHER_SKY_TEMP", "Sky temp",
                     WEATHER_TAB, IP_RO, 60, IPS_OK);
    IUFillText(&Weather_Sky_TempT[0], "WEATHER_SKY_TEMP", "°C","---");

    // Manual tab controls
    //--------------------
    IUFillTextVector(&Manual_WarningTP, Manual_WarningT, 2, getDeviceName(), "MANUAL_WARNINGS", "NOTE",
                     MANUAL_TAB, IP_RO, 60, IPS_ALERT);
    IUFillText(&Manual_WarningT[0], "WARNING_LINE1", "CAUTION:", "THESE CONTROLS ARE POTENTIALLY HAZARDOUS");
    IUFillText(&Manual_WarningT[1], "WARNING_LINE2", "CAUTION:", "UNDERSTAND THE IMPLICATIONS BEFORE USING");

    IUFillSwitchVector(&Safety_Interlock_OverrideSP, Safety_Interlock_OverrideS, 1, getDeviceName(), "SAFETY_INTERLOCK_OVERRIDE", "Interlocks",
                       MANUAL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitch(&Safety_Interlock_OverrideS[0], "Safety_Interlock_Override", "OVERRIDE", ISS_OFF);

    IUFillSwitchVector(&Roof_High_PowerSP, Roof_High_PowerS, 1, getDeviceName(), "ROOF_HIGH_POWER", "Roof Power",
                       MANUAL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitch(&Roof_High_PowerS[0], "Roof High Power", "MAX", ISS_OFF);
    IUFillSwitchVector(&Watchdog_ResetSP, Watchdog_ResetS, 1, getDeviceName(), "WATCHDOG_RESET", "Watchdog",
                       MANUAL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitch(&Watchdog_ResetS[0], "Watchdog Reset", "REBOOT", ISS_OFF);

    // Debug only
    // IUFillTextVector(&Arbitary_CommandTP, Arbitary_CommandT, 1, getDeviceName(), "ARBITARY_COMMAND", "Command",
    //                  MANUAL_TAB, IP_RW, 60, IPS_IDLE);
    // IUFillText(&Arbitary_CommandT[0], "ARBITARY_COMMANDT", "Response:", ":IP#");
    // Debug only end

    // Standard Indi aux controls
    //---------------------------
    addAuxControls();

    return true;
}

/**************************************************************************************
** INDI request to update the properties because there is a change in CONNECTION status
** This function is called whenever the device is connected or disconnected.
***************************************************************************************/
bool OCS::updateProperties()
{
    INDI::Dome::updateProperties();
    WI::updateProperties();

    // Remove unsupported derived controls
    //------------------------------------
    deleteProperty(DomeMotionSP);

    if (isConnected()) {
        defineProperty(&ShutterStatusTP);
        defineProperty(&DomeControlsSP);
        defineProperty(&DomeStatusTP);
        defineProperty(&Status_ItemsTP);

        // Dynamically defined properties
        //-------------------------------
        if (thermostat_controls_enabled) {
            defineProperty(&Thermostat_StatusTP);
            defineProperty(&Thermostat_heat_setpointNP);
            defineProperty(&Thermostat_cool_setpointNP);
            defineProperty(&Thermostat_humidity_setpointNP);
        }
        if (thermostat_relays[THERMOSTAT_HEAT_RELAY] > 0) {
            defineProperty(&Thermostat_heat_relaySP);
        }
        if (thermostat_relays[THERMOSTAT_COOL_RELAY] > 0) {
            defineProperty(&Thermostat_cool_relaySP);
        }
        if (thermostat_relays[THERMOSTAT_HUMIDITY_RELAY] > 0) {
            defineProperty(&Thermostat_humidity_relaySP);
        }
        if (power_device_relays[0] > 0) {
            defineProperty(&Power_Device1SP);
            defineProperty(&Power_Device_Name1TP);
        }
        if (power_device_relays[1] > 0) {
            defineProperty(&Power_Device2SP);
            defineProperty(&Power_Device_Name2TP);
        }
        if (power_device_relays[2] > 0) {
            defineProperty(&Power_Device3SP);
            defineProperty(&Power_Device_Name3TP);
        }
        if (power_device_relays[3] > 0) {
            defineProperty(&Power_Device4SP);
            defineProperty(&Power_Device_Name4TP);
        }
        if (power_device_relays[4] > 0) {
            defineProperty(&Power_Device5SP);
            defineProperty(&Power_Device_Name5TP);
        }
        if (power_device_relays[5] > 0) {
            defineProperty(&Power_Device6SP);
            defineProperty(&Power_Device_Name6TP);
        }
        if (light_relays[LIGHT_WRW_RELAY] > 0) {
            defineProperty(&LIGHT_WRWSP);
        }
        if (light_relays[LIGHT_WRR_RELAY] > 0) {
            defineProperty(&LIGHT_WRRSP);
        }
        if (light_relays[LIGHT_ORW_RELAY] > 0) {
            defineProperty(&LIGHT_ORWSP);
        }
        if (light_relays[LIGHT_ORR_RELAY] > 0) {
            defineProperty(&LIGHT_ORRSP);
        }
        if (light_relays[LIGHT_OUTSIDE_RELAY] > 0) {
            defineProperty(&LIGHT_OUTSIDESP);
        }
        if (weather_enabled[WEATHER_CLOUD]) {
            defineProperty(&Weather_CloudTP);
        }
        if (weather_enabled[WEATHER_SKY]) {
            defineProperty(&Weather_SkyTP);
        }
        if (weather_enabled[WEATHER_SKY_TEMP]) {
            defineProperty(&Weather_Sky_TempTP);
        }
        //------------------------------------------
        defineProperty(&Manual_WarningTP);
        defineProperty(&Safety_Interlock_OverrideSP);
        defineProperty(&Roof_High_PowerSP);
        defineProperty(&Watchdog_ResetSP);

        // Debug only
        // defineProperty(&Arbitary_CommandTP);
        // Debug only end
    }
    else {
        deleteProperty(ShutterStatusTP.name);
        deleteProperty(DomeControlsSP.name);
        deleteProperty(DomeStatusTP.name);
        deleteProperty(Status_ItemsTP.name);

        // Dynamically defined properties
        //-------------------------------
        if (thermostat_controls_enabled) {
            deleteProperty(Thermostat_StatusTP.name);
            deleteProperty(Thermostat_heat_setpointNP.name);
            deleteProperty(Thermostat_cool_setpointNP.name);
            deleteProperty(Thermostat_humidity_setpointNP.name);
        }
        if (thermostat_relays[THERMOSTAT_HEAT_RELAY] > 0) {
            deleteProperty(Thermostat_heat_relaySP.name);
        }
        if (thermostat_relays[THERMOSTAT_COOL_RELAY] > 0) {
            deleteProperty(Thermostat_cool_relaySP.name);
        }
        if (thermostat_relays[THERMOSTAT_HUMIDITY_RELAY] > 0) {
            deleteProperty(Thermostat_humidity_relaySP.name);
        }
        if (power_device_relays[0] > 0) {
            deleteProperty(Power_Device1SP.name);
            deleteProperty(Power_Device_Name1TP.name);
        }
        if (power_device_relays[1] > 0) {
            deleteProperty(Power_Device2SP.name);
            deleteProperty(Power_Device_Name2TP.name);
        }
        if (power_device_relays[2] > 0) {
            deleteProperty(Power_Device3SP.name);
            deleteProperty(Power_Device_Name3TP.name);
        }
        if (power_device_relays[3] > 0) {
            deleteProperty(Power_Device4SP.name);
            deleteProperty(Power_Device_Name4TP.name);
        }
        if (power_device_relays[4] > 0) {
            deleteProperty(Power_Device5SP.name);
            deleteProperty(Power_Device_Name5TP.name);
        }
        if (power_device_relays[5] > 0) {
            deleteProperty(Power_Device6SP.name);
            deleteProperty(Power_Device_Name6TP.name);
        }
        if (light_relays[LIGHT_WRW_RELAY] > 0) {
            deleteProperty(LIGHT_WRWSP.name);
        }
        if (light_relays[LIGHT_WRR_RELAY] > 0) {
            deleteProperty(LIGHT_WRRSP.name);
        }
        if (light_relays[LIGHT_ORW_RELAY] > 0) {
            deleteProperty(LIGHT_ORWSP.name);
        }
        if (light_relays[LIGHT_ORR_RELAY] > 0) {
            deleteProperty(LIGHT_ORRSP.name);
        }
        if (light_relays[LIGHT_OUTSIDE_RELAY] > 0) {
            deleteProperty(LIGHT_OUTSIDESP.name);
        }
        if (weather_enabled[WEATHER_CLOUD]) {
            deleteProperty(Weather_CloudTP.name);
        }
        if (weather_enabled[WEATHER_SKY]) {
            deleteProperty(Weather_SkyTP.name);
        }
        if (weather_enabled[WEATHER_SKY_TEMP]) {
            deleteProperty(Weather_Sky_TempTP.name);
        }
        //----------------------------------------------
        deleteProperty(Manual_WarningTP.name);
        deleteProperty(Safety_Interlock_OverrideSP.name);
        deleteProperty(Roof_High_PowerSP.name);
        deleteProperty(Watchdog_ResetSP.name);

        // Debug only
        // deleteProperty(Arbitary_CommandTP.name);
        // Debug only end

        // As we're disconnected, stop calling one minute updates
        SlowTimer.stop();
    }

    return true;
}

/************************************************************
* Poll properties for updates - period set by Options polling
*************************************************************/
void OCS::TimerHit()
{
    // Get the roof/shutter status
    char roof_status_response[RB_MAX_LEN] = {0};
    int roof_status_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, roof_status_response,
                                                                             OCS_get_roof_status);
    if (roof_status_error_or_fail > 1) {
        bool roof_was_in_error = (getShutterState() == SHUTTER_ERROR);

        LOGF_DEBUG("roof_was_in_error, %d", roof_was_in_error);

        char *split;
        char roof_message[30];
        split = strtok(roof_status_response, ",");
        if (strcmp(split, "o") == 0) {
            if (getShutterState() != SHUTTER_MOVING) {
                setShutterState(SHUTTER_MOVING);
            }
            split = strtok(NULL, ",");
            sprintf(roof_message, "Opening, travel %s", split);
        } else if (strcmp(split, "c") == 0) {
            if (getShutterState() != SHUTTER_MOVING) {
                setShutterState(SHUTTER_MOVING);
            }
            split = strtok(NULL, ",");
            sprintf(roof_message, "Closing, travel %s", split);
        } else if (strcmp(split, "i") == 0) {
            split = strtok(NULL, ",");
            if (strcmp(split, "OPEN") == 0) {
                if (getShutterState() != SHUTTER_OPENED) {
                    setShutterState(SHUTTER_OPENED);
                }
                sprintf(roof_message, "Idle - Open");
            } else if (strcmp(split, "CLOSED") == 0) {
                if (getShutterState() != SHUTTER_CLOSED) {
                    setShutterState(SHUTTER_CLOSED);
                }
                sprintf(roof_message, "Idle - Closed");
            } else if (strcmp(split, "No Error") == 0) {
                sprintf(roof_message, "Idle - No Error");
            } else if (strcmp(split, "Waiting for mount to park") == 0) {
                sprintf(roof_message, "Waiting for mount to park");
            } else {
                // Must be an error message
                sprintf(roof_message, "Roof/shutter: %s", split);
                if (getShutterState() != SHUTTER_ERROR) {
                    setShutterState(SHUTTER_ERROR);
                }
            }
        }

        if (strcmp(last_shutter_status, roof_message) != 0) {
            if (getShutterState() == SHUTTER_ERROR) {
                LOGF_ERROR("Roof/shutter error - %s", roof_message);
            } else {
                LOGF_DEBUG("Roof/shutter is %s", roof_message);
                if (roof_was_in_error) {
                    LOG_INFO("Roof/shutter error cleared");
                }
            }
            sprintf(last_shutter_status, "%s", roof_message);
        }

        IUSaveText(&ShutterStatusT[0], roof_message);
        IDSetText(&ShutterStatusTP, nullptr);
    }

    // Dome updates
    if (hasDome) {
        // Get the dome status
        char dome_message[10];
        char dome_status_response[RB_MAX_LEN] = {0};
        int dome_status_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, dome_status_response,
                                                                                 OCS_get_dome_status);
        if (dome_status_error_or_fail > 1) { //> 1 as an OCS error would be 1 char in response
            if (strcmp(dome_status_response, "H") == 0) {
                if (getDomeState() != DOME_IDLE) {
                    setDomeState(DOME_IDLE);
                    ParkSP[0].setState(ISS_OFF);
                    ParkSP[1].setState(ISS_ON);
                    ParkSP.setState(IPS_OK);
                    ParkSP.apply();
                }
                sprintf(dome_message, "Home");
            } else if (strcmp(dome_status_response, "P") == 0) {
                if (getDomeState() != DOME_PARKED) {
                    setDomeState(DOME_PARKED);
                    ParkSP[0].setState(ISS_ON);
                    ParkSP[1].setState(ISS_OFF);
                    ParkSP.setState(IPS_OK);
                    ParkSP.apply();
                }
                sprintf(dome_message, "Parked");
            } else if (strcmp(dome_status_response, "K") == 0) {
                if (getDomeState() != DOME_PARKING) {
                    setDomeState(DOME_PARKING);
                    ParkSP[0].setState(ISS_OFF);
                    ParkSP[1].setState(ISS_OFF);
                    ParkSP.setState(IPS_BUSY);
                    ParkSP.apply();
                }
                sprintf(dome_message, "Parking");
            } else if (strcmp(dome_status_response, "S") == 0) {
                if (getDomeState() != DOME_MOVING) {
                    setDomeState(DOME_MOVING);
                    ParkSP[0].setState(ISS_OFF);
                    ParkSP[1].setState(ISS_ON);
                    ParkSP.setState(IPS_OK);
                    ParkSP.apply();
                }
                sprintf(dome_message, "Slewing");
            } else if (strcmp(dome_status_response, "I") == 0) {
                if (getDomeState() != DOME_IDLE) {
                    setDomeState(DOME_IDLE);
                    ParkSP[0].setState(ISS_OFF);
                    ParkSP[1].setState(ISS_ON);
                    ParkSP.setState(IPS_OK);
                    ParkSP.apply();
                }
                sprintf(dome_message, "Idle");
            }
            IUSaveText(&DomeStatusT[0], dome_message);
            IDSetText(&DomeStatusTP, nullptr);
        } else {
            LOGF_WARN("Communication error on get Dome status %s, this update aborted, will try again...", OCS_get_dome_status);
            LOGF_WARN("Received %S", dome_status_response);
        }

        // Get the dome position
        char dome_position_response[RB_MAX_LEN] = {0};
        double position = conversion_error ;
        int dome_position_error_or_fail = getCommandDoubleResponse(PortFD, &position, dome_position_response,
                                                                   OCS_get_dome_azimuth);
        if (dome_position_error_or_fail > 1 && position != conversion_error) {
            // DomeAbsPosN->value = position;
            DomeAbsPosNP[0].setValue(position);
            DomeAbsPosNP.apply();
        } else {
            LOGF_WARN("Communication error on get Dome position %s, this update aborted, will try again...", OCS_get_dome_azimuth);
            LOGF_WARN("Received %d", position);
        }
    }

    IDSetText(&Status_ItemsTP, nullptr);

    // Timer loop control
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    SetTimer(getCurrentPollingPeriod());
}

/***************************************
* Poll properties for updates per minute
****************************************/
void OCS::SlowTimerHit()
{
    // Status tab
    char power_status_response[RB_MAX_LEN] = {0};
    int power_status_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, power_status_response,
                                                                              OCS_get_power_status);
    if (power_status_error_or_fail > 1) {
        IUSaveText(&Status_ItemsT[STATUS_MAINS], power_status_response);
        IDSetText(&Status_ItemsTP, nullptr);
    } else {
        LOGF_WARN("Communication error on get Power Status %s, this update aborted, will try again...", OCS_get_power_status);
    }

    char safety_status_response[RB_MAX_LEN] = {0};
    int safety_status_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, safety_status_response,
                                                                                     OCS_get_safety_status);
    if (safety_status_error_or_fail > 1) {
        IUSaveText(&Status_ItemsT[STATUS_OCS_SAFETY], safety_status_response);
        IDSetText(&Status_ItemsTP, nullptr);
    } else {
        LOGF_WARN("Communication error on get OCS Safety Status %s, this update aborted, will try again...", OCS_get_safety_status);
    }

    char MCU_temp_response[RB_MAX_LEN] = {0};
    int MCU_temp_status_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, MCU_temp_response,
                                                                                 OCS_get_MCU_temperature);
    if (MCU_temp_status_error_or_fail > 1) {
        IUSaveText(&Status_ItemsT[STATUS_MCU_TEMPERATURE], MCU_temp_response);
        IDSetText(&Status_ItemsTP, nullptr);
    } else {
        LOGF_WARN("Communication error on get MCU temperature %s, this update aborted, will try again...", OCS_get_thermostat_status);
    }

    // Get the last roof error (if any)
    // This is here because although the 1 second polled get roof status would return any error flagged
    // at the time it could miss a transient condition that has been cleared in-between poll periods.
    // Last roof error holds the condition until cleared by a shutter/roof action.
    char roof_error_response[RB_MAX_LEN] = {0};
    int roof_error_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, roof_error_response,
                                                                            OCS_get_roof_last_error);
    if (roof_error_error_or_fail > 1) {
        if (strcmp(roof_error_response, "Error: Open safety interlock") == 0 &&
                strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Open safety interlock");
        } else if (strcmp(roof_error_response, "Error: Close safety interlock") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Close safety interlock");
        } else if (strcmp(roof_error_response, "Error: Open unknown error") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Open unknown");
        } else if (strcmp(roof_error_response, "Error: Open limit sw fail") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Open limit switch fail");
        } else if (strcmp(roof_error_response, "Error: Open over time") == 0 &&
            strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Open max time exceeded");
        } else if (strcmp(roof_error_response, "Error: Open under time") == 0 &&
            strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Open min time not reached");
        } else if (strcmp(roof_error_response, "Error: Close unknown error") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Close unknow");
        } else if (strcmp(roof_error_response, "Error: Close limit sw fail") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Close limit switch");
        } else if (strcmp(roof_error_response, "Error: Close over time") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Close max time exceeded");
        } else if (strcmp(roof_error_response, "Error: Close under tim") == 0 &&
            strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            LOG_WARN("Roof/shutter error - Close min time not reached");
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
        } else if (strcmp(roof_error_response, "Error: Limit switch malfunction") == 0 &&
                strcmp(roof_error_response, last_shutter_error) != 0) {
            indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
            if (getShutterState() != SHUTTER_ERROR) {
                setShutterState(SHUTTER_ERROR);
            }
            LOG_WARN("Roof/shutter error - Both open & close limit switches active together");
        } else if (strcmp(roof_error_response, "Error: Closed/opened limit sw on") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               if (getShutterState() != SHUTTER_ERROR) {
                   setShutterState(SHUTTER_ERROR);
               }
               LOG_WARN("Roof/shutter error - Closed/opened limit switch on");
        } else if (strcmp(roof_error_response, "Warning: Already closed") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               LOG_WARN("Roof/shutter warning - Roof/shutter is already closed");
        } else if (strcmp(roof_error_response, "Error: Close location unknown") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               if (getShutterState() != SHUTTER_ERROR) {
                   setShutterState(SHUTTER_ERROR);
               }
               LOG_WARN("Roof/shutter error - Close location unknown");
        } else if (strcmp(roof_error_response, "Error: Motion direction unknown") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               if (getShutterState() != SHUTTER_ERROR) {
                   setShutterState(SHUTTER_ERROR);
               }
               LOG_WARN("Roof/shutter error - Motion direction unknown");
        } else if (strcmp(roof_error_response, "Error: Close already in motion") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               if (getShutterState() != SHUTTER_ERROR) {
                   setShutterState(SHUTTER_ERROR);
               }
               LOG_WARN("Roof/shutter error - Close already in motion");
        } else if (strcmp(roof_error_response, "Error: Opened/closed limit sw on") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               if (getShutterState() != SHUTTER_ERROR) {
                   setShutterState(SHUTTER_ERROR);
               }
               LOG_WARN("Roof/shutter error - Opened/closed limit switch on");
        } else if (strcmp(roof_error_response, "Warning: Already open") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               LOG_WARN("Roof/shutter warning - Roof/shutter is already open");
        } else if (strcmp(roof_error_response, "Error: Open location unknow") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               if (getShutterState() != SHUTTER_ERROR) {
                   setShutterState(SHUTTER_ERROR);
               }
               LOG_WARN("Roof/shutter error - Open location unknow");
        } else if (strcmp(roof_error_response, "Error: Open already in motion") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               if (getShutterState() != SHUTTER_ERROR) {
                   setShutterState(SHUTTER_ERROR);
               }
               LOG_WARN("Roof/shutter error - Open already in motion");
        } else if (strcmp(roof_error_response, "Error: Close mount not parked") == 0 &&
                   strcmp(roof_error_response, last_shutter_error) != 0) {
               indi_strlcpy(last_shutter_error,roof_error_response, RB_MAX_LEN);
               if (getShutterState() != SHUTTER_ERROR) {
                   setShutterState(SHUTTER_ERROR);
               }
               LOG_WARN("Roof/shutter error - Timeout waiting for mount to park before closing");
        }
        IUSaveText(&Status_ItemsT[STATUS_ROOF_LAST_ERROR], last_shutter_error);
    } else if (roof_error_error_or_fail == 1) {
        LOGF_WARN("Communication error on get Roof/Shutter last error %s, this update aborted, will try again...", OCS_get_roof_last_error);
    }

    // Thermostat tab
    if (thermostat_controls_enabled) {
        // Get the Obsy Thermostat readings
        char thermostat_status_response[RB_MAX_LEN] = {0};
        int thermostat_status_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, thermostat_status_response,
                                                                                       OCS_get_thermostat_status);
        if (thermostat_status_error_or_fail > 1) {
            char *split;
            split = strtok(thermostat_status_response, ",");
            IUSaveText(&Thermostat_StatusT[THERMOSTAT_TEMERATURE], split);
            split = strtok(NULL, ",");
            IUSaveText(&Thermostat_StatusT[THERMOSTAT_HUMIDITY], split);
            IDSetText(&Thermostat_StatusTP, nullptr);
        } else {
            LOGF_WARN("Communication error on get Thermostat Status %s, this update aborted, will try again...", OCS_get_thermostat_status);
        }

        // Get the Thermostat setpoints
        if (thermostat_relays[THERMOSTAT_HEAT_RELAY] > 0) {
            char heat_response[RB_MAX_LEN] = {0};
            int heat_int_response = 0;
            int heat_setpoint_error_or_fail = getCommandIntFromCharResponse(PortFD, heat_response, &heat_int_response,
                                                                            OCS_get_thermostat_heat_setpoint);
            if (heat_setpoint_error_or_fail >= 0 && heat_int_response != conversion_error) { // errors are negative
                Thermostat_heat_setpointN[0].value = heat_int_response;
            } else {
                LOGF_WARN("Communication error on get Thermostat Heat Setpoint %d, this update aborted, will try again...", heat_int_response);
            }
            IDSetNumber(&Thermostat_heat_setpointNP, nullptr);
        }

        if (thermostat_relays[THERMOSTAT_COOL_RELAY] > 0) {
            char cool_response[RB_MAX_LEN] = {0};
            int cool_int_response = 0;
            int cool_setpoint_error_or_fail = getCommandIntFromCharResponse(PortFD, cool_response, &cool_int_response,
                                                                            OCS_get_thermostat_cool_setpoint);
            if (cool_setpoint_error_or_fail >= 0 && cool_int_response != conversion_error) { // errors are negative
                Thermostat_cool_setpointN[0].value =cool_int_response;
            } else {
                LOGF_WARN("Communication error on get Thermostat Cool Setpoint %d, this update aborted, will try again...", cool_int_response);
            }
            IDSetNumber(&Thermostat_cool_setpointNP, nullptr);
        }

        if (thermostat_relays[THERMOSTAT_HUMIDITY_RELAY] > 0) {
            char humidity_response[RB_MAX_LEN] = {0};
            int humidity_int_response = 0;
            int humidity_setpoint_error_or_fail = getCommandIntFromCharResponse(PortFD, humidity_response, &humidity_int_response,
                                                                                OCS_get_thermostat_humidity_setpoint);
            if (humidity_setpoint_error_or_fail >= 0 && humidity_int_response != conversion_error) { // errors are negative
                Thermostat_humidity_setpointN[0].value = humidity_int_response;
            } else {
                LOGF_WARN("Communication error on get Thermostat Humidity Setpoint %d, this update aborted, will try again...", humidity_int_response);
            }
            IDSetNumber(&Thermostat_humidity_setpointNP, nullptr);
        }

        // Get the Thermostat relay status'
        for (int relay = 0; relay < THERMOSTAT_RELAY_COUNT; relay++) {
            if (thermostat_relays[relay] > 0) {
                char thermo_relay_response[RB_MAX_LEN] = {0};
                char thermo_relay_command[RB_MAX_LEN] = {0};
                sprintf(thermo_relay_command, "%s%d%s", OCS_get_relay_part, thermostat_relays[relay], OCS_command_terminator);
                int thermo_relay_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, thermo_relay_response,
                                                                                         thermo_relay_command);
                if (thermo_relay_error_or_fail > 1) {
                    switch(relay) {
                        case THERMOSTAT_HEAT_RELAY:
                            if (strcmp(thermo_relay_response, "ON") == 0) {
                                Thermostat_heat_relayS[ON_SWITCH].s = ISS_ON;
                                Thermostat_heat_relayS[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(thermo_relay_response, "OFF") == 0) {
                                Thermostat_heat_relayS[ON_SWITCH].s = ISS_OFF;
                                Thermostat_heat_relayS[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Thermostat_heat_relaySP, nullptr);
                            break;
                        case THERMOSTAT_COOL_RELAY:
                            if (strcmp(thermo_relay_response, "ON") == 0) {
                                Thermostat_cool_relayS[ON_SWITCH].s = ISS_ON;
                                Thermostat_cool_relayS[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(thermo_relay_response, "OFF") == 0) {
                                Thermostat_cool_relayS[ON_SWITCH].s = ISS_OFF;
                                Thermostat_cool_relayS[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Thermostat_cool_relaySP, nullptr);
                            break;
                        case THERMOSTAT_HUMIDITY_RELAY:
                            if (strcmp(thermo_relay_response, "ON") == 0) {
                                Thermostat_humidity_relayS[ON_SWITCH].s = ISS_ON;
                                Thermostat_humidity_relayS[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(thermo_relay_response, "OFF") == 0) {
                                Thermostat_humidity_relayS[ON_SWITCH].s = ISS_OFF;
                                Thermostat_humidity_relayS[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Thermostat_humidity_relaySP, nullptr);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }

    // Power tab
    if (power_tab_enabled) {
        // Get the Power relay status'
        for (int relay = 0; relay < POWER_DEVICE_COUNT; relay++) {
            if (power_device_relays[relay] > 0) {
                char power_relay_response[RB_MAX_LEN] = {0};
                char power_relay_command[RB_MAX_LEN] = {0};
                sprintf(power_relay_command, "%s%d%s", OCS_get_relay_part, power_device_relays[relay], OCS_command_terminator);
                int power_relay_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, power_relay_response,
                                                                                        power_relay_command);
                if (power_relay_error_or_fail > 1) {
                    switch(relay) {
                        case POWER_DEVICE1:
                            if (strcmp(power_relay_response, "ON") == 0) {
                                Power_Device1S[ON_SWITCH].s = ISS_ON;
                                Power_Device1S[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(power_relay_response, "OFF") == 0) {
                                Power_Device1S[ON_SWITCH].s = ISS_OFF;
                                Power_Device1S[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Power_Device1SP, nullptr);
                            break;
                        case POWER_DEVICE2:
                            if (strcmp(power_relay_response, "ON") == 0) {
                                Power_Device2S[ON_SWITCH].s = ISS_ON;
                                Power_Device2S[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(power_relay_response, "OFF") == 0) {
                                Power_Device2S[ON_SWITCH].s = ISS_OFF;
                                Power_Device2S[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Power_Device2SP, nullptr);
                            break;
                        case POWER_DEVICE3:
                            if (strcmp(power_relay_response, "ON") == 0) {
                                Power_Device3S[ON_SWITCH].s = ISS_ON;
                                Power_Device3S[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(power_relay_response, "OFF") == 0) {
                                Power_Device3S[ON_SWITCH].s = ISS_OFF;
                                Power_Device3S[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Power_Device3SP, nullptr);
                            break;
                        case POWER_DEVICE4:
                            if (strcmp(power_relay_response, "ON") == 0) {
                                Power_Device4S[ON_SWITCH].s = ISS_ON;
                                Power_Device4S[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(power_relay_response, "OFF") == 0) {
                                Power_Device4S[ON_SWITCH].s = ISS_OFF;
                                Power_Device4S[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Power_Device4SP, nullptr);
                            break;
                        case POWER_DEVICE5:
                            if (strcmp(power_relay_response, "ON") == 0) {
                                Power_Device5S[ON_SWITCH].s = ISS_ON;
                                Power_Device5S[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(power_relay_response, "OFF") == 0) {
                                Power_Device5S[ON_SWITCH].s = ISS_OFF;
                                Power_Device5S[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Power_Device5SP, nullptr);
                            break;
                        case POWER_DEVICE6:
                            if (strcmp(power_relay_response, "ON") == 0) {
                                Power_Device6S[ON_SWITCH].s = ISS_ON;
                                Power_Device6S[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(power_relay_response, "OFF") == 0) {
                                Power_Device6S[ON_SWITCH].s = ISS_OFF;
                                Power_Device6S[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&Power_Device6SP, nullptr);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }

    // Lights tab
    if (lights_tab_enabled) {
        // Get the Lights relay status'
        for (int relay = 0; relay < LIGHT_COUNT; relay++) {
            if (light_relays[relay] > 0) {
                char light_relay_response[RB_MAX_LEN] = {0};
                char light_relay_command[RB_MAX_LEN] = {0};
                sprintf(light_relay_command, "%s%d%s", OCS_get_relay_part, light_relays[relay], OCS_command_terminator);
                int light_relay_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, light_relay_response,
                                                                                        light_relay_command);
                if (light_relay_error_or_fail > 1) {
                    switch (relay) {
                        case LIGHT_WRW_RELAY:
                            if (strcmp(light_relay_response, "ON") == 0) {
                                LIGHT_WRWS[ON_SWITCH].s = ISS_ON;
                                LIGHT_WRWS[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(light_relay_response, "OFF") == 0) {
                                LIGHT_WRWS[ON_SWITCH].s = ISS_OFF;
                                LIGHT_WRWS[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&LIGHT_WRWSP, nullptr);
                            break;
                        case LIGHT_WRR_RELAY:
                            if (strcmp(light_relay_response, "ON") == 0) {
                                LIGHT_WRRS[ON_SWITCH].s = ISS_ON;
                                LIGHT_WRRS[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(light_relay_response, "OFF") == 0) {
                                LIGHT_WRRS[ON_SWITCH].s = ISS_OFF;
                                LIGHT_WRRS[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&LIGHT_WRRSP, nullptr);
                            break;
                        case LIGHT_ORW_RELAY:
                            if (strcmp(light_relay_response, "ON") == 0) {
                                LIGHT_ORWS[ON_SWITCH].s = ISS_ON;
                                LIGHT_ORWS[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(light_relay_response, "OFF") == 0) {
                                LIGHT_ORWS[ON_SWITCH].s = ISS_OFF;
                                LIGHT_ORWS[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&LIGHT_ORWSP, nullptr);
                            break;
                        case LIGHT_ORR_RELAY:
                            if (strcmp(light_relay_response, "ON") == 0) {
                                LIGHT_ORRS[ON_SWITCH].s = ISS_ON;
                                LIGHT_ORRS[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(light_relay_response, "OFF") == 0) {
                                LIGHT_ORRS[ON_SWITCH].s = ISS_OFF;
                                LIGHT_ORRS[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&LIGHT_ORRSP, nullptr);
                            break;
                        case LIGHT_OUTSIDE_RELAY:
                            if (strcmp(light_relay_response, "ON") == 0) {
                                LIGHT_OUTSIDES[ON_SWITCH].s = ISS_ON;
                                LIGHT_OUTSIDES[OFF_SWITCH].s = ISS_OFF;
                            } else if (strcmp(light_relay_response, "OFF") == 0) {
                                LIGHT_OUTSIDES[ON_SWITCH].s = ISS_OFF;
                                LIGHT_OUTSIDES[OFF_SWITCH].s = ISS_ON;
                            }
                            IDSetSwitch(&LIGHT_OUTSIDESP, nullptr);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
}

/*****************************************************************
* Poll Weather properties for updates - period set by Weather poll
******************************************************************/
IPState OCS::updateWeather() {
    if (weather_tab_enabled) {

        LOG_DEBUG("Weathe update called");

        for (int measurement = 0; measurement < WEATHER_MEASUREMENTS_COUNT; measurement ++) {
            if (weather_enabled[measurement] == 1) {
                char measurement_reponse[RB_MAX_LEN];
                char measurement_command[CMD_MAX_LEN];

                LOGF_DEBUG("In weather measurements loop, %u", measurement);

                switch (measurement) {
                    case WEATHER_TEMPERATURE:
                        indi_strlcpy(measurement_command, OCS_get_outside_temperature, sizeof(measurement_command));
                        break;
                    case WEATHER_PRESSURE:
                        indi_strlcpy(measurement_command, OCS_get_pressure, sizeof(measurement_command));
                        break;
                    case WEATHER_HUMIDITY:
                        indi_strlcpy(measurement_command, OCS_get_humidity, sizeof(measurement_command));
                        break;
                    case WEATHER_WIND:
                        indi_strlcpy(measurement_command, OCS_get_wind_speed, sizeof(measurement_command));
                        break;
                    case WEATHER_RAIN:
                        indi_strlcpy(measurement_command, OCS_get_rain_sensor_status, sizeof(measurement_command));
                        break;
                    case WEATHER_DIFF_SKY_TEMP:
                        indi_strlcpy(measurement_command, OCS_get_sky_diff_temperature, sizeof(measurement_command));
                        break;

                    // case WEATHER_CLOUD: Not handled in this switch as it returns a string so needs a different call

                    case WEATHER_SKY:
                        indi_strlcpy(measurement_command, OCS_get_sky_quality, sizeof(measurement_command));
                        break;
                    case WEATHER_SKY_TEMP:
                        indi_strlcpy(measurement_command, OCS_get_sky_IR_temperature, sizeof(measurement_command));
                        break;
                    default:
                        break;
                }

                double value = conversion_error;
                int measurement_error_or_fail = getCommandDoubleResponse(PortFD, &value, measurement_reponse,
                                                                         measurement_command);
                if ((measurement_error_or_fail >= 0) && (value != conversion_error) &&
                    (weather_enabled[measurement] == 1)) {
                    switch(measurement) {
                        case WEATHER_TEMPERATURE:
                            setParameterValue("WEATHER_TEMPERATURE", value);
                            break;
                        case WEATHER_PRESSURE:
                            setParameterValue("WEATHER_PRESSURE", value);
                            break;
                        case WEATHER_HUMIDITY:
                            setParameterValue("WEATHER_HUMIDITY", value);
                            break;
                        case WEATHER_WIND:
                            setParameterValue("WEATHER_WIND", value);
                            break;
                        case WEATHER_RAIN:
                            setParameterValue("WEATHER_RAIN", value);
                            break;
                        case WEATHER_DIFF_SKY_TEMP:
                            setParameterValue("WEATHER_SKY_DIFF_TEMP", value);
                            break;

                     // case WEATHER_CLOUD: Not handled in this switch as it returns a string so needs a different call

                        case WEATHER_SKY:
                            IUSaveText(&Weather_SkyT[0], measurement_reponse);
                            IDSetText(&Weather_SkyTP, nullptr);
                            break;
                        case WEATHER_SKY_TEMP:
                            IUSaveText(&Weather_Sky_TempT[0], measurement_reponse);
                            IDSetText(&Weather_Sky_TempTP, nullptr);
                            break;
                        default:
                            break;
                    }
                }

                // Separate because WEATHER_CLOUD is the only weather parameter that return a string
                if ((measurement == WEATHER_CLOUD) && (weather_enabled[WEATHER_CLOUD] == 1)) {
                    char measurement_reponse[RB_MAX_LEN];
                    int measurement_error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, measurement_reponse,
                                                                             OCS_get_cloud_description);
                    if (measurement_error_or_fail > 1) {
                        IUSaveText(&Weather_CloudT[0], measurement_reponse);
                        IDSetText(&Weather_CloudTP, nullptr);
                    }
                }
            }
            if (WI::syncCriticalParameters()) {
                LOG_DEBUG("SyncCriticalParameters = true");
            } else {
                LOG_DEBUG("SyncCriticalParameters = false");
            }
        }
    }

    return IPS_OK;
}

/*************************************
 * Stop any roof/shutter/dome movement
 * ***********************************/
bool OCS::Abort()
{
    sendOCSCommandBlind(OCS_roof_stop);
    sendOCSCommandBlind(OCS_dome_stop);
    return true;
}

/**********************
 * Roof/shutter control
 * ********************/
IPState OCS::ControlShutter(ShutterOperation operation)
{
    if (operation == SHUTTER_OPEN) {
        // Sending roof/shutter commands clears any OCS roof errors so we need to do the same here
        indi_strlcpy(last_shutter_error, "", RB_MAX_LEN);
        sendOCSCommandBlind(OCS_roof_open);
    }
    else if (operation == SHUTTER_CLOSE) {
        // Sending roof/shutter commands clears any OCS roof errors so we need to do the same here
        indi_strlcpy(last_shutter_error, "", RB_MAX_LEN);
        sendOCSCommandBlind(OCS_roof_close);
    }

    // We have to delay the polling timer to account for the delays built
    // into the functions feeding into the OCS get roof status function
    // that allow for the delays between roof/shutter start/end of travel
    // and the activation of the respective interlock switches
    // Delay from OCS in seconds, need to convert to ms and add 1/2 second
    SetTimer((ROOF_TIME_PRE_MOTION * 1000) + 500);

    return IPS_BUSY;
}

/**************
 * Dome control
 * ************/

/************************************
 * Send the dome to its park position
 * **********************************/
IPState OCS::Park()
{
    if (sendOCSCommand(OCS_dome_park)) {
        setDomeState(DOME_PARKING);
        return IPS_BUSY;
    } else {
        setDomeState(DOME_ERROR);
        return IPS_ALERT;
    }
}

/*********************************************************
 * Bring the dome out of parked status - doesn't move dome
 * *******************************************************/
IPState OCS::UnPark()
{
    if (sendOCSCommand(OCS_restore_dome_park)) {
        setDomeState(DOME_UNPARKING);
        return IPS_OK;
    } else {
        setDomeState(DOME_ERROR);
        return IPS_ALERT;
    }
}

/************************************************************
 * Set the current dome Azimuth position as the park position
 * **********************************************************/
bool OCS::SetCurrentPark()
{
    if (sendOCSCommand(OCS_set_dome_park)) {
        return true;
    } else {
        setDomeState(DOME_ERROR);
        LOG_ERROR("Failed to set park position");
        return false;
    }
}

/************************************
 * Send the dome to the Home position
 * **********************************/
bool OCS::ReturnHome()
{
    // This command has no return
    sendOCSCommandBlind(OCS_dome_home);
    return true;
}

/************************************************************
 * Set the current dome Azimuth position as the home position
 * **********************************************************/
bool OCS::ResetHome()
{
    // This command has no return
    sendOCSCommand(OCS_reset_dome_home);
    return true;
}

/**********************************
 * Move dome to an absoute position
 * ********************************/
IPState OCS::MoveAbs(double az)
{
    char set_dome_azimuth_command[CMD_MAX_LEN] = {0};
    sprintf(set_dome_azimuth_command, "%s%f%s",
            OCS_set_dome_azimuth_part, az, OCS_command_terminator);
    sendOCSCommandBlind(set_dome_azimuth_command);
    char dome_goto_target_response[RB_MAX_LEN] = {0};
    int dome_goto_target_int_response = 0;
    int dome_goto_target_error_or_fail = getCommandIntResponse(PortFD, &dome_goto_target_int_response, dome_goto_target_response,
                                                                       OCS_dome_goto_taget);
    if (dome_goto_target_error_or_fail >= 1) {
        switch (dome_goto_target_int_response) {
        case GOTO_IS_POSSIBLE:
            LOGF_INFO("Begin dome move to %1.1f°", az);
            return IPS_BUSY;
            break;
        case BELOW_HORIZON_LIMIT:
            // Should never get here - Indi doesn't support dome Alt
            LOGF_ERROR("Dome target (%1.1f°) is below the horizon limit", az);
            return IPS_ALERT;
            break;
        case ABOVE_OVERHEAD_LIMIT:
            // Should never get here - Indi doesn't support dome Alt
            LOGF_ERROR("Dome target (%1.1f°) is above the overhead limit", az);
            return IPS_ALERT;
            break;
        case CONTROLLER_IN_STANDBY:
            LOG_ERROR("Dome can not move, controller in standby");
            return IPS_ALERT;
            break;
        case DOME_IS_PARKED:
            LOG_ERROR("Dome can not move, dome is parked");
            return IPS_ALERT;
            break;
        case GOTO_IN_PROGRESS:
            LOG_ERROR("Can not ask dome to move, dome is already moving");
            return IPS_ALERT;
            break;
        case OUTSIDE_LIMITS:
            LOGF_ERROR("Dome target (%1.1f°) is outside safe limits", az);
            return IPS_ALERT;
            break;
        case HARDWARE_FAULT:
            LOG_ERROR("Dome can not move, hardware fault");
            return IPS_ALERT;
            break;
        case ALREADY_IN_MOTION:
            LOG_ERROR("Can not ask dome to move, dome is already moving");
            return IPS_ALERT;
            break;
        case UNSPECIFIED_ERROR:
            LOG_ERROR("Dome returned an unspecified error");
            return IPS_ALERT;
            break;
        default:
            return IPS_IDLE;
            break;
        }
    } else {
        LOGF_ERROR("Dome goto produced error %s", dome_goto_target_error_or_fail);
        return IPS_ALERT;
    }
}

/****************************************************
 * Sync domes actual position to supplied co-ordinate
 * **************************************************/
bool OCS::Sync(double az) {
    char set_dome_azimuth_command[CMD_MAX_LEN] = {0};
    sprintf(set_dome_azimuth_command, "%s%f%s",
            OCS_set_dome_azimuth_part, az, OCS_command_terminator);
    sendOCSCommandBlind(set_dome_azimuth_command);
    char dome_sync_target_response[RB_MAX_LEN] = {0};
    int dome_sync_target_int_response = 0;
    int dome_sync_target_error_or_fail  = getCommandIntResponse(PortFD, &dome_sync_target_int_response, dome_sync_target_response,
                                                                        OCS_dome_sync_target);
    if (dome_sync_target_error_or_fail >= 1) {
        switch (dome_sync_target_int_response) {
        case GOTO_IS_POSSIBLE:
            LOGF_INFO("Dome syncronised to %1.1f°", az);
            return true;
            break;
        case BELOW_HORIZON_LIMIT:
            // Should never get here - Indi doesn't support dome Alt
            LOGF_ERROR("Dome target (%1.1f°) is below the horizon limit", az);
            return false;
            break;
        case ABOVE_OVERHEAD_LIMIT:
            // Should never get here - Indi doesn't support dome Alt
            LOGF_ERROR("Dome target (%1.1f°) is above the overhead limit", az);
            return false;
            break;
        case CONTROLLER_IN_STANDBY:
            LOG_ERROR("Dome can not sync, controller in standby");
            return false;
            break;
        case DOME_IS_PARKED:
            LOG_ERROR("Dome can not sync, dome is parked");
            return false;
            break;
        case GOTO_IN_PROGRESS:
            LOG_ERROR("Can not ask dome to sync, dome is moving");
            return false;
            break;
        case OUTSIDE_LIMITS:
            LOGF_ERROR("Dome sync target (%1.1f°) is outside safe limits", az);
            return false;
            break;
        case HARDWARE_FAULT:
            LOG_ERROR("Dome can not sync, hardware fault");
            return false;
            break;
        case ALREADY_IN_MOTION:
            LOG_ERROR("Can not ask dome to sync, dome is moving");
            return false;
            break;
        case UNSPECIFIED_ERROR:
            LOG_ERROR("Dome returned an unspecified error");
            return false;
            break;
        default:
            return false;
            break;
        }
    } else {
        LOGF_ERROR("Dome sync to target produced error %s", dome_sync_target_response);
        return false;
    }
}

/***********************************************************
** Client is asking us to establish connection to the device
************************************************************/
bool OCS::Connect()
{
    bool status = INDI::Dome::Connect();
    return status;
}

/***********************************************************
** Client is asking us to terminate connection to the device
************************************************************/
bool OCS::Disconnect()
{
    bool status = INDI::Dome::Disconnect();
    return status;
}

//*******************
// Required overrides
//******************/
void ISPoll(void *p);

void ISSnoopDevice(XMLEle *root)
{
    ocs->ISSnoopDevice(root);
}

void OCS::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);
}

bool OCS::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    return true;
}

/**************************************************
 * Client has changed the state of a switch, update
 **************************************************/
bool OCS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0) {

        LOGF_DEBUG("Got an IsNewSwitch for: %s", name);

        // Power devices
        //--------------
        if (strcmp(Power_Device1SP.name, name) == 0) {
            IUUpdateSwitch(&Power_Device1SP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "POWER_DEVICE1_ON") == 0) {
                    char set_power_dev_1_on_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_1_on_cmd, "%s%d,ON%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE1], OCS_command_terminator);
                    IDSetSwitch(&Power_Device1SP, nullptr);
                    return sendOCSCommand(set_power_dev_1_on_cmd);
                } else if (strcmp(names[i], "POWER_DEVICE1_OFF") == 0) {
                    char set_power_dev_1_off_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_1_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE1], OCS_command_terminator);
                    IDSetSwitch(&Power_Device1SP, nullptr);
                    return sendOCSCommand(set_power_dev_1_off_cmd);
                }
            }
            IDSetSwitch(&Power_Device1SP, nullptr);
            return false;
        } else if (strcmp(Power_Device2SP.name, name) == 0) {
            IUUpdateSwitch(&Power_Device2SP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "POWER_DEVICE2_ON") == 0) {
                    char set_power_dev_2_on_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_2_on_cmd, "%s%d,ON%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE2], OCS_command_terminator);
                    IDSetSwitch(&Power_Device2SP, nullptr);
                    return sendOCSCommand(set_power_dev_2_on_cmd);
                } else if (strcmp(names[i], "POWER_DEVICE2_OFF") == 0) {
                    char set_power_dev_2_off_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_2_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE2], OCS_command_terminator);
                    IDSetSwitch(&Power_Device2SP, nullptr);
                    return sendOCSCommand(set_power_dev_2_off_cmd);
                }
            }
            IDSetSwitch(&Power_Device2SP, nullptr);
            return false;
        } else if (strcmp(Power_Device3SP.name, name) == 0) {
            IUUpdateSwitch(&Power_Device3SP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "POWER_DEVICE3_ON") == 0) {
                    char set_power_dev_3_on_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_3_on_cmd, "%s%d,ON%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE3], OCS_command_terminator);
                    IDSetSwitch(&Power_Device3SP, nullptr);
                    return sendOCSCommand(set_power_dev_3_on_cmd);
                } else if (strcmp(names[i], "POWER_DEVICE3_OFF") == 0) {
                    char set_power_dev_3_off_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_3_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE3], OCS_command_terminator);
                    IDSetSwitch(&Power_Device3SP, nullptr);
                    return sendOCSCommand(set_power_dev_3_off_cmd);
                }
            }
            IDSetSwitch(&Power_Device3SP, nullptr);
            return false;
        } else if (strcmp(Power_Device4SP.name, name) == 0) {
            IUUpdateSwitch(&Power_Device4SP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "POWER_DEVICE4_ON") == 0) {
                    char set_power_dev_4_on_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_4_on_cmd, "%s%d,ON%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE4], OCS_command_terminator);
                    IDSetSwitch(&Power_Device4SP, nullptr);
                    return sendOCSCommand(set_power_dev_4_on_cmd);
                } else if (strcmp(names[i], "POWER_DEVICE4_OFF") == 0) {
                    char set_power_dev_4_off_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_4_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE4], OCS_command_terminator);
                    IDSetSwitch(&Power_Device4SP, nullptr);
                    return sendOCSCommand(set_power_dev_4_off_cmd);
                }
            }
            IDSetSwitch(&Power_Device4SP, nullptr);
            return false;
        } else if (strcmp(Power_Device5SP.name, name) == 0) {
            IUUpdateSwitch(&Power_Device5SP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "POWER_DEVICE5_ON") == 0) {
                    char set_power_dev_5_on_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_5_on_cmd, "%s%d,ON%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE5], OCS_command_terminator);
                    IDSetSwitch(&Power_Device5SP, nullptr);
                    return sendOCSCommand(set_power_dev_5_on_cmd);
                } else if (strcmp(names[i], "POWER_DEVICE5_OFF") == 0) {
                    char set_power_dev_5_off_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_5_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE5], OCS_command_terminator);
                    IDSetSwitch(&Power_Device5SP, nullptr);
                    return sendOCSCommand(set_power_dev_5_off_cmd);
                }
            }
            IDSetSwitch(&Power_Device5SP, nullptr);
            return false;
        } else if (strcmp(Power_Device6SP.name, name) == 0) {
            IUUpdateSwitch(&Power_Device6SP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "POWER_DEVICE1_ON") == 0) {
                    char set_power_dev_6_on_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_6_on_cmd, "%s%d,ON%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE6], OCS_command_terminator);
                    IDSetSwitch(&Power_Device6SP, nullptr);
                    return sendOCSCommand(set_power_dev_6_on_cmd);
                } else if (strcmp(names[i], "POWER_DEVICE6_OFF") == 0) {
                    char set_power_dev_6_off_cmd[CMD_MAX_LEN];
                    sprintf(set_power_dev_6_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, power_device_relays[POWER_DEVICE6], OCS_command_terminator);
                    IDSetSwitch(&Power_Device6SP, nullptr);
                    return sendOCSCommand(set_power_dev_6_off_cmd);
                }
            }
            IDSetSwitch(&Power_Device6SP, nullptr);
            return false;

        // Lights
        //-------
        } else if (strcmp(LIGHT_WRWSP.name, name) == 0) {
            IUUpdateSwitch(&LIGHT_WRWSP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "WRW_ON") == 0) {
                    char set_light_wrw_on_cmd[CMD_MAX_LEN];
                    sprintf(set_light_wrw_on_cmd, "%s%d,ON%s", OCS_set_relay_part, light_relays[LIGHT_WRW_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_WRWSP, nullptr);
                    return sendOCSCommand(set_light_wrw_on_cmd);
                } else if (strcmp(names[i], "WRW_OFF") == 0) {
                    char set_light_wrw_off_cmd[CMD_MAX_LEN];
                    sprintf(set_light_wrw_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, light_relays[LIGHT_WRW_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_WRWSP, nullptr);
                    return sendOCSCommand(set_light_wrw_off_cmd);
                }
            }
            IDSetSwitch(&LIGHT_WRWSP, nullptr);
            return false;
        } else if (strcmp(LIGHT_WRRSP.name, name) == 0) {
            IUUpdateSwitch(&LIGHT_WRRSP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "WRR_ON") == 0) {
                    char set_light_wrr_on_cmd[CMD_MAX_LEN];
                    sprintf(set_light_wrr_on_cmd, "%s%d,ON%s", OCS_set_relay_part, light_relays[LIGHT_WRR_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_WRRSP, nullptr);
                    return sendOCSCommand(set_light_wrr_on_cmd);
                } else if (strcmp(names[i], "WRR_OFF") == 0) {
                    char set_light_wrr_off_cmd[CMD_MAX_LEN];
                    sprintf(set_light_wrr_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, light_relays[LIGHT_WRR_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_WRRSP, nullptr);
                    return sendOCSCommand(set_light_wrr_off_cmd);
                }
            }
            IDSetSwitch(&LIGHT_WRRSP, nullptr);
            return false;
        } else if (strcmp(LIGHT_ORWSP.name, name) == 0) {
            IUUpdateSwitch(&LIGHT_ORWSP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "ORW_ON") == 0) {
                    char set_light_orw_on_cmd[CMD_MAX_LEN];
                    sprintf(set_light_orw_on_cmd, "%s%d,ON%s", OCS_set_relay_part, light_relays[LIGHT_ORW_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_ORWSP, nullptr);
                    return sendOCSCommand(set_light_orw_on_cmd);
                } else if (strcmp(names[i], "ORW_OFF") == 0) {
                    char set_light_orw_off_cmd[CMD_MAX_LEN];
                    sprintf(set_light_orw_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, light_relays[LIGHT_ORW_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_ORWSP, nullptr);
                    return sendOCSCommand(set_light_orw_off_cmd);
                }
            }
            IDSetSwitch(&LIGHT_ORWSP, nullptr);
            return false;
        } else if (strcmp(LIGHT_ORRSP.name, name) == 0) {
            IUUpdateSwitch(&LIGHT_ORRSP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "ORR_ON") == 0) {
                    char set_light_orr_on_cmd[CMD_MAX_LEN];
                    sprintf(set_light_orr_on_cmd, "%s%d,ON%s", OCS_set_relay_part, light_relays[LIGHT_ORR_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_ORRSP, nullptr);
                    return sendOCSCommand(set_light_orr_on_cmd);
                } else if (strcmp(names[i], "ORR_OFF") == 0) {
                    char set_light_orr_off_cmd[CMD_MAX_LEN];
                    sprintf(set_light_orr_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, light_relays[LIGHT_ORR_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_ORRSP, nullptr);
                    return sendOCSCommand(set_light_orr_off_cmd);
                }
            }
            IDSetSwitch(&LIGHT_ORRSP, nullptr);
            return false;
        } else if (strcmp(LIGHT_OUTSIDESP.name, name) == 0) {
            IUUpdateSwitch(&LIGHT_OUTSIDESP, states, names, n);
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "OUTSIDE_ON") == 0) {
                    char set_light_outside_on_cmd[CMD_MAX_LEN];
                    sprintf(set_light_outside_on_cmd, "%s%d,ON%s", OCS_set_relay_part, light_relays[LIGHT_OUTSIDE_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_OUTSIDESP, nullptr);
                    return sendOCSCommand(set_light_outside_on_cmd);
                } else if (strcmp(names[i], "OUTSIDE_OFF") == 0) {
                    char set_light_outside_off_cmd[CMD_MAX_LEN];
                    sprintf(set_light_outside_off_cmd, "%s%d,OFF%s", OCS_set_relay_part, light_relays[LIGHT_OUTSIDE_RELAY], OCS_command_terminator);
                    IDSetSwitch(&LIGHT_OUTSIDESP, nullptr);
                    return sendOCSCommand(set_light_outside_off_cmd);
                }
            }
            IDSetSwitch(&LIGHT_OUTSIDESP, nullptr);
            return false;

        // Safety Override
        //----------------
        } else if (strcmp(Safety_Interlock_OverrideSP.name, name) == 0) {
            IUUpdateSwitch(&Safety_Interlock_OverrideSP, states, names, n);
            IUResetSwitch(&Safety_Interlock_OverrideSP);
            return sendOCSCommand(OCS_roof_safety_override);

        // Roof max power
        //---------------
        } else if (strcmp(Roof_High_PowerSP.name, name) == 0) {
            IUUpdateSwitch(&Roof_High_PowerSP, states, names, n);
            IUResetSwitch(&Roof_High_PowerSP);
            return sendOCSCommand(OCS_roof_high_power_mode);

        // Reset Watchdog
        //---------------
        } else if (strcmp(Watchdog_ResetSP.name, name) == 0) {
            char watchdog_response[RB_MAX_LEN] = {0};
            int watchdog_fail_or_error = getCommandSingleCharErrorOrLongResponse(PortFD, watchdog_response, OCS_set_watchdog_flag);
            (void) watchdog_fail_or_error;
            if (strcmp(watchdog_response, "Rebooting in a few seconds...") == 0) {
                LOG_WARN("Rebooting the OCS controller in a few seconds...");
                IDSetSwitch(&Safety_Interlock_OverrideSP, nullptr);
                return true;
            } else if (strcmp(watchdog_response, "23") == 0) {
                LOG_WARN("Unable to reboot, roof/shutter/dome in motion");
                IDSetSwitch(&Safety_Interlock_OverrideSP, nullptr);
                return false;
            } else if (strcmp(watchdog_response, "0") == 0) {
                LOGF_DEBUG("OCS watchdog reset error, reponse was: %s. Maybe watchdog is not enabled?", watchdog_response);
                IDSetSwitch(&Safety_Interlock_OverrideSP, nullptr);
                return false;
            }

        // Additional dome controls
        //-------------------------
        } else if (strcmp(DomeControlsSP.name, name) == 0) {
            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], "SET_PARK_SW") == 0) {
                    return SetCurrentPark();
                } else if (strcmp(names[i], "RETURN_HOME_SW") == 0) {
                    return ReturnHome();
                } else if (strcmp(names[i], "RESET_HOME_SW") == 0) {
                    return ResetHome();
                }
            }
        }
        return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
    } else {
        return false;
    }
}

/*************************************
 * Client has changed a number, update
 *************************************/
bool OCS::ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0) {

        LOGF_DEBUG("Got an IsNewNumber for: %s", name);

        if (!strcmp(Thermostat_heat_setpointNP.name, name)) {
            char thermostat_setpoint_command[CMD_MAX_LEN];
            sprintf(thermostat_setpoint_command, "%s%.0f%s",
                    OCS_set_thermostat_heat_setpoint_part, values[THERMOSTAT_HEAT_SETPOINT], OCS_command_terminator);
            char response[RB_MAX_LEN];
            int res = getCommandSingleCharResponse(PortFD, response, thermostat_setpoint_command);
            if(res < 0 || response[0] == '0') {
                LOGF_ERROR("Failed to set Thermostat heat setpoint %s", response);
                return false;
            } else {
                LOGF_INFO("Set Thermostat heat setpoint to: %.0f °C", values[THERMOSTAT_HEAT_SETPOINT]);
                IUUpdateNumber(&Thermostat_heat_setpointNP, &values[THERMOSTAT_HEAT_SETPOINT], &names[THERMOSTAT_HEAT_SETPOINT], n);
                return true;
            }
        }
        if (!strcmp(Thermostat_cool_setpointNP.name, name)) {
            char thermostat_setpoint_command[CMD_MAX_LEN];
            sprintf(thermostat_setpoint_command, "%s%.0f%s",
                    OCS_set_thermostat_cool_setpoint_part, values[THERMOSTAT_COOL_SETPOINT], OCS_command_terminator);
            char response[RB_MAX_LEN];
            int res = getCommandSingleCharResponse(PortFD, response, thermostat_setpoint_command);
            if(res < 0 || response[0] == '0') {
                LOGF_ERROR("Failed to set Thermostat cool setpoint %s", response);
                return false;
            } else {
                LOGF_INFO("Set Thermostat cool setpoint to: %.0f °C", values[THERMOSTAT_COOL_SETPOINT]);
                IUUpdateNumber(&Thermostat_heat_setpointNP, &values[THERMOSTAT_COOL_SETPOINT], &names[THERMOSTAT_COOL_SETPOINT], n);
                return true;
            }
        }
        if (!strcmp(Thermostat_humidity_setpointNP.name, name)) {
            char thermostat_setpoint_command[CMD_MAX_LEN];
            sprintf(thermostat_setpoint_command, "%s%.0f%s",
                    OCS_set_thermostat_humidity_setpoint_part, values[THERMOSTAT_HUMIDITY_SETPOINT], OCS_command_terminator);
            char response[RB_MAX_LEN];
            int res = getCommandSingleCharResponse(PortFD, response, thermostat_setpoint_command);
            if(res < 0 || response[0] == '0') {
                LOGF_ERROR("Failed to set Thermostat humidity setpoint %s", response);
                return false;
            } else {
                LOGF_INFO("Set Thermostat humidity setpoint to: %.0f °C", values[THERMOSTAT_HUMIDITY_SETPOINT]);
                IUUpdateNumber(&Thermostat_heat_setpointNP, &values[THERMOSTAT_HUMIDITY_SETPOINT], &names[THERMOSTAT_HUMIDITY_SETPOINT], n);
                return true;
            }
        }
    }

    if (strstr(name, "WEATHER_")) {
        return WI::processNumber(dev, name, values, names, n);
    }

    if (INDI::Dome::ISNewNumber(dev, name, values, names, n)) {
        return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

/*****************************************
 * Client has changed a text field, update
 *****************************************/
bool OCS::ISNewText(const char *dev,const char *name,char *texts[],char *names[],int n)
{
    // Debug only - Manual tab, Arbitary command
    // if (dev != nullptr && strcmp(dev, getDeviceName()) == 0) {
    //     if (!strcmp(Arbitary_CommandTP.name, name)) {
    //         if (1 == n) {
    //             char command_response[RB_MAX_LEN] = {0};
    //             int command_error_or_fail  = getCommandSingleCharErrorOrLongResponse(PortFD, command_response, texts[0]);
    //             if (command_error_or_fail > 0) {
    //                 if (strcmp(command_response, "") == 0) {
    //                     indi_strlcpy(command_response, "No response", sizeof(command_response));
    //                 }
    //             } else {
    //                 char error_code[RB_MAX_LEN] = {0};
    //                 if (command_error_or_fail == TTY_TIME_OUT) {
    //                     indi_strlcpy(command_response, "No response", sizeof(command_response));
    //                 } else {
    //                     sprintf(error_code, "Error: %d", command_error_or_fail);
    //                     indi_strlcpy(command_response, error_code, sizeof(command_response));
    //                 }
    //             }
    //                     // Replace the user entered string with the OCS response
    //             indi_strlcpy(texts[0], command_response, RB_MAX_LEN);
    //             IUUpdateText(&Arbitary_CommandTP, texts, names, n);
    //             IDSetText(&Arbitary_CommandTP, nullptr);
    //             return true;
    //         }
    //     }
    // }
    // Debug only end

    return INDI::Dome::ISNewText(dev,name,texts,names,n);
}

/***********************************************************
 * Client wants to know which devices to snoop, pass through
 * *********************************************************/
bool OCS::ISSnoopDevice(XMLEle *root)
{
    return INDI::Dome::ISSnoopDevice(root);
}

/********************************************************
 * OCS command functions, mostly copied from lx200_OnStep
 *******************************************************/

/*********************************************************************
 * Send command to OCS without checking (intended non-existent) return
 * *******************************************************************/
bool OCS::sendOCSCommandBlind(const char *cmd)
{
    // No need to block this command as there is no response

    int error_type;
    int nbytes_write = 0;
    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);
    flushIO(PortFD);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(ocsCommsLock);
    tcflush(PortFD, TCIFLUSH);
    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK) {
        LOGF_ERROR("CHECK CONNECTION: Error sending command %s", cmd);
        waitingForResponse = false;
        return 0; //Fail if we can't write
        //return error_type;
    }
    return 1;
}

/*********************************************************************
 * Send command to OCS that expects a 0 (sucess) or 1 (failure) return
 * *******************************************************************/
bool OCS::sendOCSCommand(const char *cmd)
{
    blockUntilClear();

    char response[1] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    flushIO(PortFD);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(ocsCommsLock);
    tcflush(PortFD, TCIFLUSH);

    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read_expanded(PortFD, response, 1, OCSTimeoutSeconds, OCSTimeoutMicroSeconds, &nbytes_read);

    tcflush(PortFD, TCIFLUSH);
    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%c>", response[0]);
    //waitingForResponse = false;
    clearBlock();

    if (nbytes_read < 1) {
        LOG_WARN("Timeout/Error on response. Check connection.");
        return false;
    }

    return (response[0] == '0'); //OCS uses 0 for success and non zero for failure, in *most* cases;
}

/************************************************************
 * Send command to OCS that expects a single character return
 * **********************************************************/
int OCS::getCommandSingleCharResponse(int fd, char *data, const char *cmd)
{
    blockUntilClear();

    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(ocsCommsLock);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read_expanded(fd, data, 1, OCSTimeoutSeconds, OCSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (error_type != TTY_OK)
        return error_type;

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RB_MAX_LEN) { //given this function that should always be true, as should nbytes_read always be 1
        data[nbytes_read] = '\0';
    } else {
        LOG_DEBUG("got RB_MAX_LEN bytes back (which should never happen), last byte set to null and possible overflow");
        data[RB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", data);
    //waitingForResponse = false;
    clearBlock();

    return nbytes_read;
}

/**************************************************
 * Send command to OCS that expects a double return
 * ************************************************/
int OCS::getCommandDoubleResponse(int fd, double *value, char *data, const char *cmd)
{
    blockUntilClear();

    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(ocsCommsLock);
    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read_section_expanded(fd, data, '#', OCSTimeoutSeconds, OCSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RB_MAX_LEN) { //If within buffer, terminate string with \0 (in case it didn't find the #)
        data[nbytes_read] = '\0'; //Indexed at 0, so this is the byte passed it
    } else {
        LOG_DEBUG("got RB_MAX_LEN bytes back, last byte set to null and possible overflow");
        data[RB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", data);
    //waitingForResponse = false;
    clearBlock();

    if (error_type != TTY_OK) {
        LOGF_DEBUG("Error %d", error_type);
        LOG_DEBUG("Flushing connection");
        tcflush(fd, TCIOFLUSH);
        return error_type;
    }

    if (sscanf(data, "%lf", value) != 1) {
        LOG_WARN("Invalid response, check connection");
        LOG_DEBUG("Flushing connection");
        tcflush(fd, TCIOFLUSH);
        return RES_ERR_FORMAT; //-1001, so as not to conflict with TTY_RESPONSE;
    }

    return nbytes_read;
}

/************************************************
 * Send command to OCS that expects an int return
 * **********************************************/
int OCS::getCommandIntResponse(int fd, int *value, char *data, const char *cmd)
{
    blockUntilClear();

    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(ocsCommsLock);
    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read_expanded(fd, data, sizeof(char), OCSTimeoutSeconds, OCSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RB_MAX_LEN) { //If within buffer, terminate string with \0 (in case it didn't find the #)
        data[nbytes_read] = '\0'; //Indexed at 0, so this is the byte passed it
    } else {
        LOG_DEBUG("got RB_MAX_LEN bytes back, last byte set to null and possible overflow");
        data[RB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", data);
    //waitingForResponse = false;
    clearBlock();

    if (error_type != TTY_OK) {
        LOGF_DEBUG("Error %d", error_type);
        LOG_DEBUG("Flushing connection");
        tcflush(fd, TCIOFLUSH);
        return error_type;
    }
    if (sscanf(data, "%i", value) != 1) {
        LOG_WARN("Invalid response, check connection");
        LOG_DEBUG("Flushing connection");
        tcflush(fd, TCIOFLUSH);
        return RES_ERR_FORMAT; //-1001, so as not to conflict with TTY_RESPONSE;
    }

    return nbytes_read;
}

/***************************************************************************
 * Send command to OCS that expects a char[] return (could be a single char)
 * *************************************************************************/
int OCS::getCommandSingleCharErrorOrLongResponse(int fd, char *data, const char *cmd)
{
    blockUntilClear();

    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(ocsCommsLock);
    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read_section_expanded(fd, data, '#', OCSTimeoutSeconds, OCSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RB_MAX_LEN) { //If within buffer, terminate string with \0 (in case it didn't find the #)
        data[nbytes_read] = '\0'; //Indexed at 0, so this is the byte passed it
    } else {
        LOG_DEBUG("got RB_MAX_LEN bytes back, last byte set to null and possible overflow");
        data[RB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", data);
    //waitingForResponse = false;
    clearBlock();

    if (error_type != TTY_OK) {
        LOGF_DEBUG("Error %d", error_type);
        return error_type;
    }

    return nbytes_read;
}

/********************************************************
 * Converts an OCS char[] return of a numeric into an int
 * ******************************************************/
int OCS::getCommandIntFromCharResponse(int fd, char *data, int *response, const char *cmd)
{
    int errorOrFail = getCommandSingleCharErrorOrLongResponse(fd, data, cmd);
    if (errorOrFail < 1) {
        waitingForResponse = false;
        return errorOrFail;
    } else {
        int value = conversion_error;
        try {
            value = std::stoi(data);
        } catch (const std::invalid_argument&) {
            LOGF_WARN("Invalid response to %s: %s", cmd, data);
        } catch (const std::out_of_range&) {
            LOGF_WARN("Invalid response to %s: %s", cmd, data);
        }
        *response = value;
        return errorOrFail;
    }
}


/**********************
 * Flush the comms port
 * ********************/
int OCS::flushIO(int fd)
{
    tcflush(fd, TCIOFLUSH);
    int error_type = 0;
    int nbytes_read;
    std::unique_lock<std::mutex> guard(ocsCommsLock);
    tcflush(fd, TCIOFLUSH);
    do {
        char discard_data[RB_MAX_LEN] = {0};
        error_type = tty_read_section_expanded(fd, discard_data, '#', 0, 1000, &nbytes_read);
        if (error_type >= 0) {
            LOGF_DEBUG("flushIO: Information in buffer: Bytes: %u, string: %s", nbytes_read, discard_data);
        }
        //LOGF_DEBUG("flushIO: error_type = %i", error_type);
    }
    while (error_type > 0);

    return 0;
}

int OCS::charToInt (char *inString)
{
    int value = conversion_error;
    try {
        value = std::stoi(inString);
    } catch (const std::invalid_argument&) {
    } catch (const std::out_of_range&) {
    }
    return value;
}

/*******************************************************
 * Block outgoing command until previous return is clear
 * *****************************************************/
void OCS::blockUntilClear()
{
    // Blocking wait for last command response to clear
    while (waitingForResponse) {
        usleep(((OCSTimeoutSeconds * 1000000) + OCSTimeoutMicroSeconds) / 10);
//        usleep(OCSTimeoutMicroSeconds / 10);
    }
    // Grab the response waiting command blocker
    waitingForResponse = true;
}

/*********************************************
 * Flush port and clear command sequence block
 * *******************************************/
void OCS::clearBlock()
{
    waitingForResponse = false;
}
