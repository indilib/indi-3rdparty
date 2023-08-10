/**
* \license
*    MIT License
*
*    libahp_gt library to drive the AHP GT controllers
*    Copyright (C) 2021  Ilia Platone
*
*    Permission is hereby granted, free of charge, to any person obtaining a copy
*    of this software and associated documentation files (the "Software"), to deal
*    in the Software without restriction, including without limitation the rights
*    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*    copies of the Software, and to permit persons to whom the Software is
*    furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in all
*    copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*    SOFTWARE.
*/

#ifndef _AHP_GT_H
#define _AHP_GT_H

#ifdef  __cplusplus
extern "C" {
#endif
#ifdef _WIN32
#include <windows.h>
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT extern
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

/**
* \mainpage AHP® GT Controllers driver library API Documentation
* \section Introduction
*
* The AHP GT controllers series drive stepping motors for usage in astronomy.<br>
* A GT controller can drive small NEMA14 or NEMA17 stepper motors, can drive
* Step/Dir external drives and can be reconfigured to fit gear ratios, maximum
* speed and acceleration rates of various mounts. Can also receive commands and
* instructions from a SynScan handset.<br>
* The Sky-Watcher command protocol used by the GT controllers was extended to allow
* reconfiguration of the controller parameters, keeping itself backwards compatible.<br>
* <br>
* This software is meant to work with the GT series motor controllers
* visit https://www.iliaplatone.com/gt1 for more informations and purchase options.
*
* \author Ilia Platone
* \version 1.6.3
* \date 2017-2021
* \copyright MIT License.
*/

/**
 * \defgroup GT_API AHP® GT Controllers API
 *
 * This library contains functions for direct low-level usage of the AHP motor controllers.<br>
 *
 * This documentation describes utility, applicative and hardware control functions included into the library.<br>
 * Each section and component is documented for general usage.
*
* \{
* \defgroup Debug Debug features
* \{*/

#ifndef AHP_DEBUG
#define AHP_DEBUG
#define AHP_DEBUG_INFO 0
#define AHP_DEBUG_ERROR 1
#define AHP_DEBUG_WARNING 2
#define AHP_DEBUG_DEBUG 3
/**
* \brief set the debug level
* \param value the debug level
*/
DLL_EXPORT void ahp_set_debug_level(int value);
/**
* \brief get the debug level
* \return The current debug level
*/
DLL_EXPORT int ahp_get_debug_level();
/**
* \brief set the application name
* \param name the application name to be printed on logs
*/
DLL_EXPORT void ahp_set_app_name(char* name);
/**
* \brief get the application name
* \return The current application name printed on logs
*/
DLL_EXPORT char* ahp_get_app_name();
/**
* \brief set the output log stream
* \param f The FILE stream pointer to set as standard output
*/
DLL_EXPORT void ahp_set_stdout(FILE *f);
/**
* \brief set the error log stream
* \param f The FILE stream pointer to set as standard error
*/
DLL_EXPORT void ahp_set_stderr(FILE *f);
#endif

/** \}
* \defgroup Types Types
* \{*/

///Motor coils phase winding configuration
typedef enum {
///AABB Motor winding
    AABB             = 0,
///ABAB Motor winding
    ABAB             = 1,
///ABBA Motor winding
    ABBA             = 2,
} GT1SteppingConfiguration;

///Stepping mode
typedef enum {
///Microstepping in low speed, Half-stepping in high speed
Mixed            = 0,
///Microstepping in low speed, Microstepping in high speed
Microstep        = 1,
///Half-stepping in low speed, Half-stepping in high speed
HalfStep         = 2,
} GT1SteppingMode;

///ST-4 port configuration
typedef enum  {
///The ST4 port will remain unused
GpioUnused             = 0x0000,
///The ST4 port will work as autoguider
GpioAsST4              = 0x0001,
///The ST4 port will be connected to an encoder
GpioAsEncoder          = 0x0002,
///The ST4 port will drive an external Step/Dir power drive
GpioAsPulseDrive       = 0x0003,
} GT1Feature;

///GT1 custom flags
typedef enum {
///Fork mount, will avoid meridian flip
isForkMount = 0x1,
///Half-current high-speed on RA
halfCurrentRA = 0x2,
///Half-current high-speed on Dec
halfCurrentDec = 0x4,
///high Baud Rate 115200
bauds_115200 = 0x8,
} GT1Flags;

///Skywatcher default features - EQ8/AZEQ6/AZEQ5 only
typedef enum {
///PPEC training in progress
inPPECTraining         = 0x000010,
///PPEC correction in progress
inPPEC                 = 0x000020,
///Mount has an encoder
hasEncoder             = 0x000001,
///Mount has PPEC
hasPPEC                = 0x000002,
///Mount has an home indexer
hasHomeIndexer         = 0x000004,
///Mount is an AZEQ
isAZEQ                 = 0x000008,
///Mount has a polar scope led
hasPolarLed            = 0x001000,
///Mount has a common slew start
hasCommonSlewStart     = 0x002000,
///Mount allows half-current tracking
hasHalfCurrentTracking = 0x004000,
///Mount provides a WiFi communication
hasWifi                = 0x008000,
} SkywatcherFeature;

///Default Mount types
typedef enum {
///Sky-Watcher EQ6
isEQ6 = 0x00,
///Sky-Watcher HEQ5
isHEQ5 = 0x01,
///Sky-Watcher EQ5
isEQ5 = 0x02,
///Sky-Watcher EQ3
isEQ3 = 0x03,
///Sky-Watcher EQ8
isEQ8 = 0x04,
///Sky-Watcher AZEQ6
isAZEQ6 = 0x05,
///Sky-Watcher AZEQ5
isAZEQ5 = 0x06,
///Sky-Watcher GT
isGT = 0x80,
///Fork Mount
isMF = 0x81,
///114GT
is114GT = 0x82,
///Dobsonian mount
isDOB = 0x90,
///Custom mount
isCustom = 0xF0,
} MountType;

typedef enum {
    Null                      = '\0',
    Initialize                = 'F',
    InquireMotorBoardVersion  = 'e',
    InquireGridPerRevolution  = 'a',
    InquireTimerInterruptFreq = 'b',
    InquireHighSpeedRatio     = 'g',
    InquirePECPeriod          = 's',
    InstantAxisStop           = 'L',
    NotInstantAxisStop        = 'K',
    SetAxisPositionCmd        = 'E',
    GetAxisPosition           = 'j',
    GetAxisStatus             = 'f',
    SetSwitch                 = 'O',
    SetMotionMode             = 'G',
    SetGotoTargetIncrement    = 'H',
    SetBreakPointIncrement    = 'M',
    SetGotoTarget             = 'S',
    SetBreakStep              = 'U',
    SetStepPeriod             = 'I',
    StartMotion               = 'J',
    GetStepPeriod             = 'D', // See Merlin protocol http://www.papywizard.org/wiki/DevelopGuide
    ActivateMotor             = 'B', // See eq6direct implementation http://pierre.nerzic.free.fr/INDI/
    SetST4GuideRateCmd        = 'P',
    SetFeatureCmd             = 'W', // EQ8/AZEQ6/AZEQ5 only
    GetFeatureCmd             = 'q', // EQ8/AZEQ6/AZEQ5 only
    InquireAuxEncoder         = 'd', // EQ8/AZEQ6/AZEQ5 only
    SetVars                   = '@',
    GetVars                   = '?',
    ReloadVars                = '$',
    Flash                     = '#',
    FlashEnable               = '!',
    SetAddress                = '=',
} SkywatcherCommand;

/// Commands for the SynScan protocol implementation
typedef enum {
/// Get RA/DEC
    GetRaDec = 'E', // < '34AB,12CE#'
/// Get precise RA/DEC
    GetPreciseRaDec = 'e', // < 34AB0500,12CE0500#
/// Get AZM-ALT
    GetAzAlt = 'Z', // < 12AB,4000#
/// Get precise AZM-ALT
    GetPreciseAzAlt = 'z', // < 12AB0500,40000500#
/// GOTO RA/DEC
    GotoRaDec = 'R', // > 'R34AB,12CE' < #
/// GOTO precise RA/DEC
    GotoPreciseRaDec = 'r', // > r'34AB0500,12CE0500', < #
/// GOTO AZM-ALT
    GotoAzAlt = 'B', // > 'B12AB,4000', < '#'
/// GOTO precise AZM-ALT
    GotoPreciseAzAlt = 'b', // >  'b12AB0500,40000500', < #
/// Sync RA/DEC
    SyncRaDec = 'S', // > 'S34AB,12CE', < '#'
/// Sync precise RA/DEC
    SyncPreciseRaDec = 's', // > 's34AB0500,12CE0500', < #
/// Get Tracking Mode
    GetTrackingMode = 't', // < chr:mode #
/// Set Tracking Mode
    SetTrackingMode = 'T', // > chr:mode #
/// Variable rate Azm (or RA) slew in positive direction
/// 'P' & chr:3 & chr:16 & chr:6 & chr:trackRateHigh & chr:trackRateLow & chr:0 & chr:0 < '#'
/// Variable rate Azm (or RA) slew in negative direction
/// 'P' & chr:3 & chr:16 & chr:7 & chr:trackRateHigh & chr:trackRateLow  & chr:0 & chr:0 < '#'
/// Variable rate Alt (or Dec) slew in positive direction
/// 'P' & chr:3 & chr:17 & chr:6 & chr:trackRateHigh & chr:trackRateLow  & chr:0 & chr:0 < '#'
/// Variable rate Alt (or Dec) slew in negative direction
/// 'P' & chr:3 & chr:17 & chr:7 & chr:trackRateHigh & chr:trackRateLow  & chr:0 & chr:0 < '#'
/// Fixed rate Azm (or RA) slew in positive direction
/// 'P' & chr:2 & chr:16 & chr:36 & chr:rate & chr:0 & chr:0 & chr:0 < '#'
/// Fixed rate Azm (or RA) slew in negative direction
/// 'P' & chr:2 & chr:16 & chr:37 & '#' < chr:rate & chr:0 & chr:0 & chr:0
/// Fixed rate ALT (or DEC) slew in positive direction
/// 'P' & chr:2 & chr:17 & chr:36 & chr:rate & chr:0 & chr:0 & chr:0 < '#'
/// Fixed rate ALT (or DEC) slew in negative direction
/// 'P' & chr:2 & chr:17 & chr:37 & chr:rate & chr:0 & chr:0 & chr:0 < '#'
/// Get Device Version Devices include: 16 = AZM/RA Motor 17 = ALT/DEC Motor
/// 'P' & chr:1 & chr:dev & chr:254 & chr:0 & chr:0 & chr:0 & chr:2 chr:major & chr:minor & '#' Hand Control responses the motor controller firmware version.
    Slew = 'P',
/// Get Location
    GetLocation = 'w', // < chr:A & chr:B & chr:C & chr:D & chr:E & chr:F & chr:G & chr:H & '#'
/// Set Location
    SetLocation = 'W', // > & chr:A & chr:B & chr:C & chr:D & chr:E & chr:F & chr:G & chr:H '#'
/// Get Time
    GetTime = 'h', // < chr:Q & chr:R & chr:S & chr:T & chr:U & chr:V & chr:W & chr:X & '#'
/// Set Time
    SetTime = 'H', // > & chr:Q & chr:R & chr:S & chr:T & chr:U & chr:V & chr:W & chr:X
/// Get Version
    GetSynScanVersion = 'V', // Replies 6 hexadecimal digits in ASCII and ends with '#', i.e. if the version is 04.37.07, then the hand control will responses '042507#', Hand Control responses its firmware version in 6 hexadecimal digits in ASCII. Each hexadecimal digits will be one of ‘0’~ ‘9’ and ‘A’~ ‘F’.
/// Get Model 0 = EQ6 GOTO Series 1 = HEQ5 GOTO Series 2 = EQ5 GOTO Series 3 = EQ3 GOTO Series 4 = EQ8 GOTO Series 5 = AZ-EQ6 GOTO Series 6 = AZ-EQ5 GOTO Series 128 ~ 143 = AZ GOTO Series 144 ~ 159 = DOB GOTO Series 160 = AllView GOTO Series
    GetModel = 'm', // < chr:model & '#' Hand Control responses the model of mount.
/// Echo - useful to check communication
    Echo = 'K', // > & chr:x chr:x & '#'
/// Is Alignment Complete? - align=1 if aligned and 0 if not
    AlignmentComplete = 'J', // chr:align & #
/// Is GOTO in Progress? - Response is ASCII'0' or '1'& '#'
    GOTOinProgress = 'L',
/// Cancel GOTO
    CancelGOTO = 'M', // < '#'
/// Get Mount Pointing State
    GetMountPointingState = 'p', // < 'E' or 'W' & '#' Hand Control responses the mount current pointing state. For northern 'E' means no flipping (OTA is on the eastern side of meridian), 'W' means flipped (OTA is on the western side). For southern hemisphere, 'E' means flipped, 'W' means not flipped.
} SynscanCommand;

///Motion Mode
typedef enum {
///High-speed (half-stepping mostly) Goto
MODE_GOTO_HISPEED = 0x00,
///Low-speed (microstepping possibly) Slew
MODE_SLEW_LOSPEED = 0x10,
///Low-speed (microstepping possibly) Goto
MODE_GOTO_LOSPEED = 0x20,
///High-speed (half-stepping mostly) Slew
MODE_SLEW_HISPEED = 0x30,
} SkywatcherMotionMode;

///Slew Mode
typedef enum {
///Slew, no target, will stop upon request only
MODE_SLEW = 0x1,
///Goto, targeted, will stop upon request or on target reached
MODE_GOTO = 0x0,
} SkywatcherSlewMode;

///Speed Mode
typedef enum {
///Low-speed (microstepping possibly)
SPEED_LOW = 0x0,
///High-speed (half-stepping mostly)
SPEED_HIGH = 0x1,
} SkywatcherSpeedMode;

///Direction
typedef enum {
///Move forward
DIRECTION_FORWARD = 0x00,
///Move backward
DIRECTION_BACKWARD = 0x01,
} SkywatcherDirection;

///Axis Status
typedef struct {
///Motor was initialized
int Initialized;
///Motor is running
int Running;
///Current slew mode
SkywatcherSlewMode Mode;
///Current speed mode
SkywatcherSpeedMode Speed;
///Current direction
SkywatcherDirection Direction;
//Current position
double position;
//timestamp
double timestamp;
} SkywatcherAxisStatus;

/**\}
 * \defgroup Defines Defines
 *\{*/
///AHP_GT_VERSION This library version
#define AHP_GT_VERSION 0x163

/**\}
 * \defgroup Conn Connection
 * \{*/

 /**
 * \brief Obtain the current libahp-gt version
 * \return The current API version
 */
 DLL_EXPORT inline unsigned int ahp_gt_get_version(void) { return AHP_GT_VERSION; }

/**
* \brief Connect to the GT controller
* \param port The serial port filename
* \return non-zero on failure
*/
DLL_EXPORT int ahp_gt_connect(const char* port);

/**
* \brief Connect to the GT controller using an existing file descriptor
* \param fd The serial stream file descriptor
* \return non-zero on failure
*/
DLL_EXPORT int ahp_gt_connect_fd(int fd);

/**
* \brief Connect to the GT controller throught an UDP connection
* \param address The ip address of the server
* \param port The port on which the server should connect
* \return non-zero on failure
*/
DLL_EXPORT int ahp_gt_connect_udp(const char* address, int port);

/**
* \brief Return the file descriptor of the port connected to the GT controllers
* \return The serial stream file descriptor
*/
DLL_EXPORT int ahp_gt_get_fd();

/**
* \brief Disconnect from the GT controller
*/
DLL_EXPORT void ahp_gt_disconnect();

/**
* \brief Set the file descriptor that links to the controller
* \param fd The file descriptor
* \sa ahp_gt_connect
*/
DLL_EXPORT void ahp_gt_set_fd(int fd);

/**
* \brief Report connection status
* \return non-zero if already connected
* \sa ahp_gt_connect
* \sa ahp_gt_connect_fd
* \sa ahp_gt_disconnect
*/
DLL_EXPORT unsigned int ahp_gt_is_connected();

/**
* \brief Report detection status
* \param index the address to check
* \return non-zero if already detected
* \sa ahp_gt_detect_device
* \sa ahp_gt_get_current_device
* \sa ahp_gt_connect
* \sa ahp_gt_connect_fd
* \sa ahp_gt_disconnect
*/
DLL_EXPORT unsigned int ahp_gt_is_detected(int index);

/**
* \brief Get the GT firmware version
* \return The GT controller firmware
*/
DLL_EXPORT int ahp_gt_get_mc_version(void);

/**\}
 * \defgroup SG Parametrization
 * \{*/

/**
* \brief Get the current GT mount type
* \return The GT controller MountType configuration
*/
DLL_EXPORT MountType ahp_gt_get_mount_type(void);

/**
* \brief Get the current GT features
* \param axis The motor to query
* \return The GT controller GT1Feature configuration
*/
DLL_EXPORT GT1Feature ahp_gt_get_feature(int axis);

/**
* \brief Get the current SkyWatcher features
* \param axis The motor to query
* \return The GT controller SkywatcherFeature configuration
*/
DLL_EXPORT SkywatcherFeature ahp_gt_get_features(int axis);

/**
* \brief Get the current motor steps number
* \param axis The motor to query
* \return The GT controller configured motor steps
*/
DLL_EXPORT double ahp_gt_get_motor_steps(int axis);

/**
* \brief Get the current motor gear teeth number
* \param axis The motor to query
* \return The GT controller configured motor teeth
*/
DLL_EXPORT double ahp_gt_get_motor_teeth(int axis);

/**
* \brief Get the current worm gear teeth number
* \param axis The motor to query
* \return The GT controller configured worm gear teeth
*/
DLL_EXPORT double ahp_gt_get_worm_teeth(int axis);

/**
* \brief Get the current crown gear teeth number
* \param axis The motor to query
* \return The GT controller configured crown gear teeth
*/
DLL_EXPORT double ahp_gt_get_crown_teeth(int axis);

/**
* \brief Get the divider in the current configuration
* \param axis The motor to query
* \return The calculated stepping divider
*/
DLL_EXPORT double ahp_gt_get_divider(int axis);

/**
* \brief Get the multiplier in the current configuration
* \param axis The motor to query
* \return The calculated stepping multiplier
*/
DLL_EXPORT double ahp_gt_get_multiplier(int axis);

/**
* \brief Get the total number of steps
* \param axis The motor to query
* \return The calculated total steps number
*/
DLL_EXPORT int ahp_gt_get_totalsteps(int axis);

/**
* \brief Get the worm number of steps
* \param axis The motor to query
* \return The calculated worm steps number
*/
DLL_EXPORT int ahp_gt_get_wormsteps(int axis);

/**
* \brief Get the guiding rate
* \param axis The motor to query
* \return The current ST4 port guide rate in sidereal speeds
*/
DLL_EXPORT double ahp_gt_get_guide_steps(int axis);

/**
* \brief Get the acceleration increment steps number
* \param axis The motor to query
* \return The calculated acceleration steps
*/
DLL_EXPORT double ahp_gt_get_acceleration_steps(int axis);

/**
* \brief Get the acceleration angle
* \param axis The motor to query
* \return The angle in radians the GT controller will cover to reach the desired rate
*/
DLL_EXPORT double ahp_gt_get_acceleration_angle(int axis);

/**
* \brief Get the rs232 port polarity
* \return Non-zero if inverted polarity was configured for the communication port
*/
DLL_EXPORT int ahp_gt_get_rs232_polarity(void);

/**
* \brief Get the microstepping pwm frequency
* \return The PWM frequency index - microstepping only
*/
DLL_EXPORT int ahp_gt_get_pwm_frequency(void);

/**
* \brief Get the forward direction
* \param axis The motor to query
* \return Non-zero if the direction of this axis is inverted
*/
DLL_EXPORT int ahp_gt_get_direction_invert(int axis);

/**
* \brief Get the mount flags
* \return Custom 10bit GT1Flags - for future usage
*/
DLL_EXPORT GT1Flags ahp_gt_get_mount_flags();

/**
* \brief Get the stepping configuration
* \param axis The motor to query
* \return The GT1SteppingConfiguration of the given axis - the coil polarization order
*/
DLL_EXPORT GT1SteppingConfiguration ahp_gt_get_stepping_conf(int axis);

/**
* \brief Get the stepping mode
* \param axis The motor to query
* \return The GT1SteppingMode of the given axis
*/
DLL_EXPORT GT1SteppingMode ahp_gt_get_stepping_mode(int axis);

/**
* \brief Get the maximum speed
* \param axis The motor to query
* \return The maximum speed in sidereal rates
*/
DLL_EXPORT double ahp_gt_get_max_speed(int axis);

/**
* \brief Get the speed limit
* \param axis The motor to query
* \return The speed limit allowed by autoconfiguration, in sidereal rates
*/
DLL_EXPORT double ahp_gt_get_speed_limit(int axis);

/**
* \brief Get the timing value of the axis
* \param axis The motor to query
* \return The timing value
*/
DLL_EXPORT double ahp_gt_get_timing(int axis);

/**
* \brief Set the timing value of the axis
* \param axis The motor to query
* \param value The timing value
*/
DLL_EXPORT void ahp_gt_set_timing(int axis, int value);

/**
* \brief Set the mount type
* \param value The MountType after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_mount_type(MountType value);

/**
* \brief Set the Skywatcher features
* \param axis The motor to reconfigure
* \param value The SkywatcherFeature after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_features(int axis, SkywatcherFeature value);

/**
* \brief Set the GT features
* \param axis The motor to reconfigure
* \param value The GT1Feature after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_feature(int axis, GT1Feature value);

/**
* \brief Set the motor steps number
* \param axis The motor to reconfigure
* \param value The motor steps for autoconfiguration after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_motor_steps(int axis, double value);

/**
* \brief Set the motor gear teeth number
* \param axis The motor to reconfigure
* \param value The motor teeth for autoconfiguration after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_motor_teeth(int axis, double value);

/**
* \brief Set the worm gear teeth number
* \param axis The motor to reconfigure
* \param value The worm gear teeth for autoconfiguration after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_worm_teeth(int axis, double value);

/**
* \brief Set the crown gear teeth number
* \param axis The motor to reconfigure
* \param value The crown gear teeth for autoconfiguration after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_crown_teeth(int axis, double value);

/**
* \brief Set the divider in the current configuration
* \param axis The motor to reconfigure
* \param value The step divider value after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_divider(int axis, int value);

/**
* \brief Set the multiplier in the current configuration
* \param axis The motor to reconfigure
* \param value The microsteps each step in high-speed mode after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_multiplier(int axis, int value);

/**
* \brief Set the total number of steps
* \param axis The motor to reconfigure
* \param value The total number of steps after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_totalsteps(int axis, int value);

/**
* \brief Set the worm number of steps
* \param axis The motor to reconfigure
* \param value The worm number of steps after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_wormsteps(int axis, int value);

/**
* \brief Set the guiding speed
* \param axis The motor to reconfigure
* \param value The guiding speed in sidereal rates after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_guide_steps(int axis, double value);

/**
* \brief Set the acceleration in high speed mode
* \param axis The motor to reconfigure
* \param value The angle in radians to cover to reach full speed after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_acceleration_angle(int axis, double value);

/**
* \brief Set the rs232 port polarity
* \param value If 1 is passed communication port polarity will be inverted after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_rs232_polarity(int value);

/**
* \brief Set the microstepping pwm frequency
* \param value The microstepping PWM frequency index after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_pwm_frequency(int value);

/**
* \brief Set the forward direction
* \param axis The motor to reconfigure
* \param value If 1 is passed the direction of this axis will be inverted after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_direction_invert(int axis, int value);

/**
* \brief Set the mount flags
* \param value Only isForkMount is supported at the moment
*/
DLL_EXPORT void ahp_gt_set_mount_flags(GT1Flags value);

/**
* \brief Set the stepping configuration
* \param axis The motor to reconfigure
* \param value The GT1SteppingConfiguration of this axis after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_stepping_conf(int axis, GT1SteppingConfiguration value);

/**
* \brief Set the stepping mode
* \param axis The motor to reconfigure
* \param value The GT1SteppingMode of this axis after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_stepping_mode(int axis, GT1SteppingMode value);

/**
* \brief Set the maximum goto speed
* \param axis The motor to reconfigure
* \param value The maximum speed of this axis in sidereal rates after ahp_gt_write_values
*/
DLL_EXPORT void ahp_gt_set_max_speed(int axis, double value);

/**\}
 * \defgroup Adr Multi-device addressing
 * \{*/

/**
* \brief Detect the currently selected device
* \return -1 if no devices were detected, 0 if a device with the current address is present
*/
DLL_EXPORT int ahp_gt_detect_device();

/**
* \brief Select a device on a serial bus
* \param address The address to query on bus
*/
DLL_EXPORT void ahp_gt_select_device(int address);

/**
* \brief Obtain the current device address
* \return -1 if no devices were detected at all, or currently selected device address
*/
DLL_EXPORT int ahp_gt_get_current_device();

/**
* \brief Change the current device address
* \param address The bus address to set on the connected GT Controller
*/
DLL_EXPORT void ahp_gt_set_address(int address);

/**
* \brief Get the current device address
* \return the address of the connected GT Controller
*/
DLL_EXPORT int ahp_gt_get_address();

/**\}
* \defgroup Cfg Configuration
* \{*/

/**
* \brief Write values from the GT controller
* \param axis The motor to configure
* \param percent Operation progress indication int32_t poiner.
* \param finished Operation completed flag int32_t poiner.
*/
DLL_EXPORT void ahp_gt_write_values(int axis, int *percent, int *finished);

/**
* \brief Read values from the GT controller
* \param axis The motor to query
*/
DLL_EXPORT void ahp_gt_read_values(int axis);

/**\}
* \defgroup Move Movement control
* \{*/

/**
* \brief Get an axis status
* \param axis The motor to query
* \return The current SkywatcherAxisStatus of this axis
*/
DLL_EXPORT SkywatcherAxisStatus ahp_gt_get_status(int axis);

/**
* \brief Get the axis position
* \param axis The motor to query
* \param timestamp The timestamp of the response
* \return The position of the specified axis in radians
*/
DLL_EXPORT double ahp_gt_get_position(int axis, double *timestamp);

/**
* \brief Set the axis position in radians
* \param axis The motor to configure
* \param value The position to set on the specified axis in radians
*/
DLL_EXPORT void ahp_gt_set_position(int axis, double value);

/**
* \brief Determine if an axis is moving
* \param axis The motor to query
* \return 1 if the axis is in motion, 0 if it's stopped
*/
DLL_EXPORT int ahp_gt_is_axis_moving(int axis);

/**
* \brief Stop an axis motion
* \param axis The motor to stop
* \param wait If 1 this function will block until the axis stops completely, 0 code flow will continue
*/
DLL_EXPORT void ahp_gt_stop_motion(int axis, int wait);

/**
* \brief Move an axis
* \param axis The motor to move
* \param speed The radial speed in sidereal rates
*/
DLL_EXPORT void ahp_gt_start_motion(int axis, double speed);

/**
* \brief Move an axis by an offset
* \param axis The motor to move
* \param increment The position offset to cover by the specified axis in radians
* \param speed The radial speed in sidereal rates
*/
DLL_EXPORT void ahp_gt_goto_relative(int axis, double increment, double speed);

/**
* \brief Move an axis to a position
* \param axis The motor to move
* \param target The position to reach by the specified axis in radians
* \param speed The radial speed in sidereal rates
*/
DLL_EXPORT void ahp_gt_goto_absolute(int axis, double target, double speed);

/** \defgroup Astronomy Astronomy specific
*\{*/

/**
* \brief Start an TCP server on the given port and stop after interrupt equals to 1
* \param port The TCP port of the server
* \param interrupt Stop when interrupt is non-zero - passed by reference
* \return non-zero on failure
*/
DLL_EXPORT int ahp_gt_start_synscan_server(int port, int *interrupt);

/**
* \brief Set the alignment state of the current device
* \param aligned 1 if aligned, 0 if not yet aligned
* \return non-zero on failure
*/
DLL_EXPORT void ahp_gt_set_aligned(int aligned);

/**
* \brief Get the alignment state of the current device
* \return 1 if aligned, 0 if not yet aligned
*/
DLL_EXPORT int ahp_gt_is_aligned();

/**
* \brief Set current time
* \param seconds Current time
*/
DLL_EXPORT void ahp_gt_set_time(double seconds);

/**
* \brief Get current time
* \return Current time
*/
DLL_EXPORT double ahp_gt_get_time();


/**
* \brief Set current time offset
* \param seconds Current time offset
*/
DLL_EXPORT void ahp_gt_set_time_offset(double offset);

/**
* \brief Get current time offset
* \return Current time offset
*/
DLL_EXPORT double ahp_gt_get_time_offset();

/**
* \brief Set geographic coordinates
* \param latitude The latitude coordinate in degrees
* \param longitude The longitude coordinate in degrees
* \param elevation The elevation on sea level
*/
DLL_EXPORT void ahp_gt_set_location(double latitude, double longitude, double elevation);

/**
* \brief Get geographic coordinates
* \param latitude The latitude coordinate in degrees
* \param longitude The longitude coordinate in degrees
* \param elevation The elevation on sea level
*/
DLL_EXPORT void ahp_gt_get_location(double *latitude, double *longitude, double *elevation);

/**
* \brief Move both axes to horizontal coordinates
* \param alt Altitude in degrees
* \param az Azimuth in degrees
*/
DLL_EXPORT void ahp_gt_goto_altaz(double alt, double az);

/**
* \brief Get the altitude tracking multiplier for AZ mounts
* \param Alt Altitude
* \param Az Azimuth
* \param Lat Latitude of the current location
* \return Altitude tracking offset multiplier
*/
DLL_EXPORT double ahp_gt_tracking_sine(double Alt, double Az, double Lat);

/**
* \brief Get the azimuth tracking multiplier for AZ mounts
* \param Alt Altitude
* \param Az Azimuth
* \param Lat Latitude of the current location
* \return Altitude tracking offset multiplier
*/
DLL_EXPORT double ahp_gt_tracking_cosine(double Alt, double Az, double Lat);

/**
* \brief Move both axes to celestial coordinates
* \param ra Right ascension in hours
* \param dec Declination in degrees
*/
DLL_EXPORT void ahp_gt_goto_radec(double ra, double dec);

/**
* \brief Set both axes positions to celestial coordinates
* \param ra Right ascension in hours
* \param dec Declination in degrees
*/
DLL_EXPORT void ahp_gt_sync_radec(double ra, double dec);

/**
* \brief Start a tracking motion correction
* \param axis The motor to tune at sidereal speed
* \param target_period The target sidereal period
* \param interrupt if non-zero stop training before ending this session
*/
DLL_EXPORT void ahp_gt_correct_tracking(int axis, double target_period, int *interrupt);

/**
* \brief Start the tracking thread
*/
DLL_EXPORT void ahp_gt_start_tracking_thread();

/**
* \brief Set the tracking mode
* \param mode The tracking mode - 0: no tracking 1: EQ mode 2: AZ mode
*/
DLL_EXPORT void ahp_gt_set_tracking_mode(int mode);

/**
* \brief Start a test tracking motion
* \param axis The motor to drive at sidereal speed
*/
DLL_EXPORT void ahp_gt_start_tracking(int axis);

/**
* \brief Get the azimuth tracking multiplier for AZ mounts
* \param Alt Altitude
* \param Az Azimuth
* \param Ra Right ascension filled by reference
* \param Dec Declination filled by reference
*/
DLL_EXPORT void ahp_gt_get_ra_dec_coordinates(double Alt, double Az, double *Ra, double *Dec);

/**
* \brief Get current altitude and azimuth
* \param Ra Right ascension
* \param Dec Declination
* \param Alt Altitude filled by reference
* \param Az Azimuth filled by reference
*/
DLL_EXPORT void ahp_gt_get_alt_az_coordinates(double Ra, double Dec, double* Alt, double *Az);

/**
* \brief Get the current hour angle
* \return The current hour angle
*/
DLL_EXPORT double ahp_gt_get_ha();

/**
* \brief Get the current right ascension
* \return The current right ascension
*/
DLL_EXPORT double ahp_gt_get_ra();

/**
* \brief Get the current declination
* \return The current declination
*/
DLL_EXPORT double ahp_gt_get_dec();

/**\}
 * \}
 * \}*/

#ifdef __cplusplus
} // extern "C"
#endif

#endif //_AHP_GT_H
