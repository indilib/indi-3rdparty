/*******************************************************************************
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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

#pragma once

#include "indidome.h"
#include "indiweather.h"
#include "connectionplugins/connectiontcp.h"
#include "connectionplugins/connectionserial.h"
#include "indipropertyswitch.h"
#include "inditimer.h"

#define RB_MAX_LEN 64
#define CMD_MAX_LEN 32
enum ResponseErrors {RES_ERR_FORMAT = -1001};

/**********************************************************************
OCS lexicon
Extracted from OCS 3.03i
Note all commands sent and responses returned terminate with a # symbol
These are stripped from returned char* by their retrieving functions
An unterminated 0 is returned from unconfigured items
**********************************************************************/

// General commands
//-----------------

// Get Product (compatibility)
#define OCS_handshake ":IP#"
// Returns: OCS#

// Get firmware version number
#define OCS_get_firmware ":IN#"
// Returns: firmware_string# for example 3.03i#

// Get timeouts
#define OCS_get_timeouts ":IT#"
// Returns: n.n,m.m#
// where n.n is ROOF_TIME_PRE_MOTION and m.m ROOF_TIME_POST_MOTION

// Get safety status
// Info only, Indi generates it's own saety status
#define OCS_get_safety_status ":Gs#"
// Returns: SAFE#, UNSAFE#

// Set the watchdog reset flag - forces OCS software reboot
#define OCS_set_watchdog_flag ":SW#"
// Returns: Rebooting in a few seconds...# or 23# "CE_SLEW_IN_MOTION" for roof/shutter/dome in motion blocking error

// Set the UTC Date and Time
// ":SU[MM/DD/YYYY,HH:MM:SS]#"
// Example: SU03/31/2023,13:22:00#
// Returns: 0# on failure, 1# on success

// Get the power status
#define OCS_get_power_status ":GP#"
// Returns: OK#, OUT#, or N/A#

// Get the internal MCU temperature in deg. C
#define OCS_get_MCU_temperature ":GX9F#"
// Returns: +/-n.n# if supported, 0 if unsupported

// Set USB Baud Rate where n is an ASCII digit (1..9) with the following interpertation
// 0=115.2K, 1=56.7K, 2=38.4K, 3=28.8K, 4=19.2K, 5=14.4K, 6=9600, 7=4800, 8=2400, 9=1200
// ":SB[n]#"
// Returns: 1# (at the current baud rate and then changes to the new rate for further communication)

// Roof/shutter commands
//----------------------
// note: Roll off roof style observatory or shutter control for dome style observatory

// Command the roof/shutter to close
#define OCS_roof_close ":RC#"
// Returns: nothing

// Command the roof/shutter to open
#define OCS_roof_open ":RO#"
// Returns: nothing

// Command the roof/shutter movement to stop
#define OCS_roof_stop ":RH#"
// Returns: nothing

// Set the roof/shutter safety override - ignore stuck limit switches and timeout
#define OCS_roof_safety_override ":R!#"
// Returns: 1# on success

// Set the roof/shutter high power mode - forces motor pwm to 100%
#define OCS_roof_high_power_mode ":R+#"
// Returns: 1# on success

// Get the roof/shutter status
#define OCS_get_roof_status ":RS#"
// Returns:
// OPEN#, CLOSED#, c,Travel: n%# (for closing), o,Travel: n%# for opening,
// i,No Error# for idle or i,Waiting for mount to park#

// Get the roof/shutter last status error
#define OCS_get_roof_last_error ":RSL#"
// Returns:
// Error: Open safety interlock#
// Error: Close safety interlock#
// Error: Open unknown error#
// Error: Open limit sw fail#
// Error: Open over time#
// Error: Open under time#
// Error: Close unknown error#
// Error: Close limit sw fail#
// Error: Close over time#
// Error: Close under time#
// Error: Limit switch malfunction#
// Error: Closed/opened limit sw on#
// Warning: Already closed#
// Error: Close location unknown#
// Error: Motion direction unknown#
// Error: Close already in motion#
// Error: Opened/closed limit sw on#
// Warning: Already open#
// Error: Open location unknown#
// Error: Open already in motion#
// Error: Close mount not parked#
// or nothing if never errored

//Dome commands
//-------------

// Command the dome to goto the home position
#define OCS_dome_home ":DC#"
// Returns: nothing

// Reset that the dome is at home
#define OCS_reset_dome_home ":DF#"
// Returns: nothing

// Command the dome to goto the park position
#define OCS_dome_park ":DP#"
// Returns: 0# on failure, 1# on success

// Set the dome park position
#define OCS_set_dome_park ":DQ#"
// Returns: 0# on failure, 1# on success

// Restore the dome park position
#define OCS_restore_dome_park ":DR#"
// Returns: 0# on failure, 1# on success

// Command the dome movement to stop
#define OCS_dome_stop ":DH#"
// Returns: nothing

// Get the dome Azimuth (0 to 360 degrees)
#define OCS_get_dome_azimuth ":DZ#"
// Returns: D.DDD#

// Set the dome Azimuth target (0 to 360 degrees)
#define OCS_set_dome_azimuth_part ":Dz"
// Example: ":Dz[D.D]#"
// Returns: nothing

// Unused, not supported by Indi
// Get the dome Altitude (0 to 90 degrees)
// #define OCS_get_dome_altitude ":DA#"
// Returns: D.D#, NAN# is no second axis

// Unused, not supported by Indi
// Set the dome Altitude target (0 to 90 degrees)
// ":Da[D.D]#"
// Returns: nothing

// Set the dome to sync with target (Azimuth only)
#define OCS_dome_sync_target ":DN#"
// Returns: See :DS# command below

// Command the dome to goto target
#define OCS_dome_goto_taget ":DS#"
// Returns:
enum {
    GOTO_IS_POSSIBLE,
    BELOW_HORIZON_LIMIT,
    ABOVE_OVERHEAD_LIMIT,
    CONTROLLER_IN_STANDBY,
    DOME_IS_PARKED,
    GOTO_IN_PROGRESS,
    OUTSIDE_LIMITS,
    HARDWARE_FAULT,
    ALREADY_IN_MOTION,
    UNSPECIFIED_ERROR,
    COUNT_DOME_GOTO_RETURNS
};

//  0# = Goto is possible
//  1# = below the horizon limit
//  2# = above overhead limit
//  3# = controller in standby
//  4# = dome is parked
//  5# = Goto in progress
//  6# = outside limits (AXIS2_LIMIT_MAX, AXIS2_LIMIT_MIN, AXIS1_LIMIT_MIN/MAX, MERIDIAN_E/W)
//  7# = hardware fault
//  8# = already in motion
//  9# = unspecified error

// Get dome status
#define OCS_get_dome_status ":DU#"
// Returns: P# if parked, K#, if parking, H# if at Home, I# if idle

// Axis commands

// Axis1 is Dome Azimuth - required if dome = true
// Axis2 is Dome Altitude - optional

// Curently unused
// Get the axis/driver configuration for axis [n]
// #define OCS_get_axis_configuration_part ":GXA"
// ":GXA[n]#"
// Returns: s,s,s,s#
// where s,s,s,s... comprises:
// parameter [0] = steps per degree,
// parameter [1] = reverse axis
// parameter [2] = minimum limit
// parameter [3] = maximum limit

// Currently unused
// Get the stepper driver status for axis [n]
// #define OCS_get_driver_status_part ":GXU"
// ":GXU[n]#"
// Returns:
//  ST# = At standstill
//  OA# = Output A open load
//  OB# = Output B open load
//  GA# = Output A short to ground
//  GB# = Output B short to ground
//  OT# = Over temperature (>150 deg. C)
//  PW# = Over temperature warning (>120 deg. C)
//  GF# = Fault

// Not used, superset definition?
// Set the axis/driver configuration for axis [n]
// ":SXA[n]#"

// Revert axis/driver configuration for axis [n] to defaults
// ":SXA[n],R#"

// Currently unused
// Set the axis/driver configuration for axis [n]
// #define OCS_set_axis_configuration_part ":SXA"
// :SXA[n],[s,s,s,s...]#
// where s,s,s,s... comprises:
// parameter [0] = steps per degree,
// parameter [1] = reverse axis
// parameter [2] = minimum limit
// parameter [3] = maximum limit

// Weather commands
//-----------------

// Get the outside temperature in deg. C
#define OCS_get_outside_temperature ":G1#"
// Returns: nnn.n#

// Get the sky IR temperature in deg. C
#define OCS_get_sky_IR_temperature ":G2#"
// Returns: nnn.n#

// Get the sky differential temperature
#define OCS_get_sky_diff_temperature ":G3#"
// Returns: nnn.n#
// where <= 21 is cloudy

// TBC?
// Get averaged sky differential temperature
#define OCS_get_av_sky_diff_temperature ":GS#"
// Returns: nnn.n#
// where <= 21 is cloudy

// Get the absolute barometric pressure as Float (mbar, sea-level compensated)
#define OCS_get_pressure ":Gb#"
// Returns: n.nnn#
// where n ranges from about 980.0 to 1050.0

// Get cloud description
#define OCS_get_cloud_description ":GC#"
// Returns: description_string#

// Get relative humidity reading as Float (% Rh)
#define OCS_get_humidity ":Gh#"
// Returns: n.n#
// where n ranges from 0.0 to 100.0

// Get sky quality in mag/arc-sec^2
#define OCS_get_sky_quality ":GQ#"
// Returns: nnn.n#

// Get rain sensor status
#define OCS_get_rain_sensor_status ":GR#"
// Returns: -1000# for invalid, 0# for N/A, 1# for Rain, 2# for Warn, and 3# for Dry

// Get wind status
#define OCS_get_wind_status ":GW#"
// Returns: OK#, HIGH#, or N/A#

// Get wind speed
#define OCS_get_wind_speed ":Gw#"
// Returns: n# kph, Invalid#, or N/A#

// Get the weather threshold #defines
#define OCS_get_weather_thresholds ":IW#"
// Returns: 20,-14#, WEATHER_WIND_SPD_THRESHOLD,WEATHER_SAFE_THRESHOLD, N/A if sensor == OFF

// Thermostat commands
//--------------------

// Get Thermostat relay #defines
#define OCS_get_thermostat_definitions ":It#"
// Returns: n,n,-1#les
// HEAT_RELAY, COOL_RELAY, HUMIDITY_RELAY
// -1 indicates function not defined

// Get thermostat status
#define OCS_get_thermostat_status ":GT#"
// Returns: n.n,m.m#
// where n.n is temperature in deg. C and m.m is % humidity

// Get heat setpoint in deg. C
#define OCS_get_thermostat_heat_setpoint ":GH#"
// Returns: n#, or 0# for invalid

// Set heat setpoint in deg. C
#define OCS_set_thermostat_heat_setpoint_part ":SH"
// Example: ":SH0#" turns heat off
// Example: ":SH21#" heat setpoint 21 deg. C
// Returns: 1# on success

// Get cool/vent setpoint in deg. C
#define OCS_get_thermostat_cool_setpoint ":GV#"
// Returns: n#, or 0# for invalid

//Set cool/vent setpoint in deg. C
#define OCS_set_thermostat_cool_setpoint_part ":SC"
// Example: ":SC0#" turns cooling off
// Example: ":SC30#" cool setpoint 30 deg. C
// Returns: 1# on success

// Get humidity setpoint in %
#define OCS_get_thermostat_humidity_setpoint ":GD#"
// Returns: n#, or 0# for invalid

// Set humidity setpoint in %
#define OCS_set_thermostat_humidity_setpoint_part ":SD"
// Example: ":SD0#" turns dehumidifying off
// Example: ":SD55#" humidity setpoint 55%
// Returns: 1# on success

// Power/GPIO commands
//--------------------

// Get Light relay #defines
#define OCS_get_light_definitions ":IL#"
// Returns: n,n,-1,n,n#
// LIGHT_WRW_RELAY,LIGHT_WRR_RELAY,LIGHT_ORW_RELAY,LIGHT_ORR_RELAY,LIGHT_OUTSIDE_RELAY
// -1 indicates function not defined

// Get Power device relays
#define OCS_get_power_definitions ":Ip#"
// Returns: n,n,-1,n,n,n#
// POWER_DEVICE1_RELAY...POWER_DEVICE6_RELAY

// Get Power device names
#define OCS_get_power_names_part ":Ip"
// Example: ":Ip1#"
// Returns: name_string#

// Get Relay n state
#define OCS_get_relay_part ":GR"
// Example: ":GR1#"
// Returns: ON#, OFF#, n# (pwm 0-9)

// Set Relay n [state] = ON, OFF, DELAY, n (pwm 0 to 10)
#define OCS_set_relay_part ":SR"
// Example: ":SR1,ON#"
// Returns: 1# on success

// Get Analog n state
#define OCS_get_analog_part ":GA"
// Example: ":GA1#"
// Returns: n# (0 to 1023, 0 to 5V)

// Get Digital Sense n state
#define OCS_get_digital_part ":GS"
// Example: ":GS1#"
// Returns: ON#, OFF#

// For dynamically assembled commands
//-----------------------------------
#define OCS_command_terminator "#"

/********************
OnCue OCS lexicon end
*********************/

class OCS : public INDI::Dome, public INDI::WeatherInterface
{
  public:
    OCS();
    virtual ~OCS() override = default;
    const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    bool updateProperties() override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual bool ISSnoopDevice(XMLEle *root) override;
    virtual bool Handshake() override;
    virtual bool Abort() override;

  protected:
    bool Connect() override;
    bool Disconnect() override;

    void TimerHit() override;
    void SlowTimerHit();
    virtual IPState updateWeather() override;

    bool sendOCSCommand(const char *cmd);
    bool sendOCSCommandBlind(const char *cmd);
    int flushIO(int fd);
    int getCommandSingleCharResponse(int fd, char *data, const char *cmd); //Reimplemented from getCommandString
    int getCommandSingleCharErrorOrLongResponse(int fd, char *data, const char *cmd); //Reimplemented from getCommandString
    int getCommandDoubleResponse(int fd, double *value, char *data,
                                 const char *cmd); //Reimplemented from getCommandString Will return a double, and raw value.
    int getCommandIntResponse(int fd, int *value, char *data, const char *cmd);
    int getCommandIntFromCharResponse(int fd, char *data, int *response, const char *cmd); //Calls getCommandSingleCharErrorOrLongResponse with conversion of return
    int charToInt(char *inString);
    void blockUntilClear();
    void clearBlock();

    long int OCSTimeoutSeconds = 0;
    long int OCSTimeoutMicroSeconds = 100000;

private:
    float minimum_OCS_fw = 3.08;
    int conversion_error = -10000;

    // Capability queries on connection
    void GetCapabilites();
    bool hasDome = false;

    // Timer for slow updates, once per minute
    INDI::Timer SlowTimer;

    // Command sequence enforcement
    bool waitingForResponse = false;

    // Roof/Shutter control
    //---------------------
    int ROOF_TIME_PRE_MOTION = 0;
    int ROOF_TIME_POST_MOTION = 0;
    char last_shutter_status[RB_MAX_LEN];
    char last_shutter_error[RB_MAX_LEN];
    IPState ControlShutter(ShutterOperation operation) override;

    // Dome control
    //-------------
    virtual IPState Park() override;
    virtual IPState UnPark() override;
    virtual bool SetCurrentPark() override;
    virtual IPState MoveAbs(double az) override;
    virtual bool Sync (double az) override;
    bool ReturnHome();
    bool ResetHome();

    enum {
        ON_SWITCH,
        OFF_SWITCH,
        SWITCH_TOGGLE_COUNT
    };

    // Main control tab controls
    //--------------------------
    ITextVectorProperty ShutterStatusTP;
    IText ShutterStatusT[1];
    ITextVectorProperty DomeStatusTP;
    IText DomeStatusT[1];
    enum {
        DOME_SET_PARK,
        DOME_RETURN_HOME,
        DOME_SET_HOME,
        DOME_CONTROL_COUNT
    };
    ISwitchVectorProperty DomeControlsSP;
    ISwitch DomeControlsS[DOME_CONTROL_COUNT];


    // Status tab controls
    //--------------------
    enum {
        STATUS_FIRMWARE,
        STATUS_ROOF_LAST_ERROR,
        STATUS_MAINS,
        STATUS_OCS_SAFETY,
        STATUS_MCU_TEMPERATURE,
        STATUS_ITEMS_COUNT
    };
    ITextVectorProperty Status_ItemsTP;
    IText Status_ItemsT[STATUS_ITEMS_COUNT] {};

    // Thermostat tab controls
    //------------------------
    bool thermostat_controls_enabled = false;


    enum {
        THERMOSTAT_TEMERATURE,
        THERMOSTAT_HUMIDITY,
        THERMOSTAT_COUNT
    };
    ITextVectorProperty Thermostat_StatusTP;
    IText Thermostat_StatusT[THERMOSTAT_COUNT] {};

    enum {
        THERMOSTAT_HEAT_SETPOINT,
        THERMOSTAT_COOL_SETPOINT,
        THERMOSTAT_HUMIDITY_SETPOINT,
        THERMOSTAT_SETPOINT_COUNT
    };

    INumberVectorProperty Thermostat_heat_setpointNP;
    INumber Thermostat_heat_setpointN[1];
    INumberVectorProperty Thermostat_cool_setpointNP;
    INumber Thermostat_cool_setpointN[1];
    INumberVectorProperty Thermostat_humidity_setpointNP;
    INumber Thermostat_humidity_setpointN[1];


    ISwitchVectorProperty Thermostat_heat_relaySP;
    ISwitch Thermostat_heat_relayS[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty Thermostat_cool_relaySP;
    ISwitch Thermostat_cool_relayS[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty Thermostat_humidity_relaySP;
    ISwitch Thermostat_humidity_relayS[SWITCH_TOGGLE_COUNT];

    enum {
        THERMOSTAT_HEAT_RELAY,
        THERMOSTAT_COOL_RELAY,
        THERMOSTAT_HUMIDITY_RELAY,
        THERMOSTAT_RELAY_COUNT
    };
    int thermostat_relays[THERMOSTAT_RELAY_COUNT];

    // Power tab controls
    //-------------------
    bool power_tab_enabled = false;

    enum {
        POWER_DEVICE1,
        POWER_DEVICE2,
        POWER_DEVICE3,
        POWER_DEVICE4,
        POWER_DEVICE5,
        POWER_DEVICE6,
        POWER_DEVICE_COUNT
    };

    int power_device_relays[POWER_DEVICE_COUNT];
    char POWER_DEVICE1_NAME[RB_MAX_LEN];
    char POWER_DEVICE2_NAME[RB_MAX_LEN];
    char POWER_DEVICE3_NAME[RB_MAX_LEN];
    char POWER_DEVICE4_NAME[RB_MAX_LEN];
    char POWER_DEVICE5_NAME[RB_MAX_LEN];
    char POWER_DEVICE6_NAME[RB_MAX_LEN];

    enum {
        RELAY_1,
        RELAY_2,
        RELAY_3,
        RELAY_4,
        RELAY_5,
        RELAY_6,
        RELAY_7,
        RELAY_8,
        RELAY_9,
        RELAY_10,
        RELAY_11,
        RELAY_12,
        RELAY_13,
        RELAY_14,
        RELAY_15,
        RELAY_16,
        RELAY_17,
        RELAY_18,
        RELAY_COUNT
    };
    ISwitchVectorProperty Power_Device1SP;
    ISwitch Power_Device1S[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty Power_Device2SP;
    ISwitch Power_Device2S[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty Power_Device3SP;
    ISwitch Power_Device3S[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty Power_Device4SP;
    ISwitch Power_Device4S[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty Power_Device5SP;
    ISwitch Power_Device5S[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty Power_Device6SP;
    ISwitch Power_Device6S[SWITCH_TOGGLE_COUNT];
    ITextVectorProperty Power_Device_Name1TP;
    IText Power_Device_Name1T[1] {};
    ITextVectorProperty Power_Device_Name2TP;
    IText Power_Device_Name2T[1] {};
    ITextVectorProperty Power_Device_Name3TP;
    IText Power_Device_Name3T[1] {};
    ITextVectorProperty Power_Device_Name4TP;
    IText Power_Device_Name4T[1] {};
    ITextVectorProperty Power_Device_Name5TP;
    IText Power_Device_Name5T[1] {};
    ITextVectorProperty Power_Device_Name6TP;
    IText Power_Device_Name6T[1] {};

    // Lights tab controls
    //--------------------
    bool lights_tab_enabled = false;

    enum {
        LIGHT_WRW_RELAY,
        LIGHT_WRR_RELAY,
        LIGHT_ORW_RELAY,
        LIGHT_ORR_RELAY,
        LIGHT_OUTSIDE_RELAY,
        LIGHT_COUNT
    };

    int light_relays[LIGHT_COUNT];

    ISwitchVectorProperty LIGHT_WRWSP;
    ISwitch LIGHT_WRWS[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty LIGHT_WRRSP;
    ISwitch LIGHT_WRRS[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty LIGHT_ORWSP;
    ISwitch LIGHT_ORWS[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty LIGHT_ORRSP;
    ISwitch LIGHT_ORRS[SWITCH_TOGGLE_COUNT];
    ISwitchVectorProperty LIGHT_OUTSIDESP;
    ISwitch LIGHT_OUTSIDES[SWITCH_TOGGLE_COUNT];

    // Weather tab controls
    //---------------------
    bool weather_tab_enabled = false;
    int wind_speed_threshold = 0;
    int diff_temp_threshold = 0;

    enum {
        WEATHER_TEMPERATURE,
        WEATHER_PRESSURE,
        WEATHER_HUMIDITY,
        WEATHER_WIND,
        WEATHER_RAIN,
        WEATHER_DIFF_SKY_TEMP,
        WEATHER_CLOUD,
        WEATHER_SKY,
        WEATHER_SKY_TEMP,
        WEATHER_MEASUREMENTS_COUNT
    };

    int weather_enabled[WEATHER_MEASUREMENTS_COUNT];

    ITextVectorProperty Weather_CloudTP;
    IText Weather_CloudT[1];
    ITextVectorProperty Weather_SkyTP;
    IText Weather_SkyT[1];
    ITextVectorProperty Weather_Sky_TempTP;
    IText Weather_Sky_TempT[1];

    // Manual tab controls
    //--------------------
    enum {
        SAFETY_INTERLOCK_OVERRIDE,
        ROOF_HIGH_POWER,
        WATCHDOG_RESET,
        MANUAL_CONTROLS_COUNT
    };

    ITextVectorProperty Manual_WarningTP;
    IText Manual_WarningT[2] {};
    ISwitchVectorProperty Safety_Interlock_OverrideSP;
    ISwitch Safety_Interlock_OverrideS[1];
    ISwitchVectorProperty Roof_High_PowerSP;
    ISwitch Roof_High_PowerS[1];
    ISwitchVectorProperty Watchdog_ResetSP;
    ISwitch Watchdog_ResetS[1];

    // Debug only
    // ITextVectorProperty Arbitary_CommandTP;
    // IText Arbitary_CommandT[1];
    // Debug only end
};

