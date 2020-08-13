/*******************************************************************************
 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 NexDome Driver for Firmware v3+

 Change Log:

 2019.10.07: Driver is completely re-written to work with Firmware v3 since
 Firmware v1 is obsolete from NexDome.
 2017.01.01: Driver for Firmware v1 is developed by Rozeware Development Ltd.

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

#include <map>
#include <string>

namespace ND
{
enum
{
    ENABLED,
    DISABLED,
};

typedef enum
{
    ROTATOR,
    SHUTTER
} Targets;

typedef enum
{
    ACCELERATION_RAMP,
    DEAD_ZONE,
    SEMANTIC_VERSION,
    GOTO_AZ,
    GOTO_HOME,
    GOTO_STEP,
    HOME_POSITION,
    POSITION,
    RANGE,
    REPORT,
    EMERGENCY_STOP,
    VELOCITY,
    EEPROM,
    OPEN_SHUTTER,
    CLOSE_SHUTTER,
}  Commands;

// Command Strings
static const std::map<Commands, std::string> CommandsMap =
{
    {ACCELERATION_RAMP, "A"},
    {DEAD_ZONE,         "D"},
    {SEMANTIC_VERSION,  "F"},
    {GOTO_AZ,           "GA"},
    {GOTO_HOME,         "GH"},
    {GOTO_STEP,         "GS"},
    {HOME_POSITION,     "H"},
    {POSITION,          "P"},
    {RANGE,             "R"},
    {REPORT,            "S"},
    {EMERGENCY_STOP,    "S"},
    {VELOCITY,          "V"},
    {EEPROM,            "Z"},
    {OPEN_SHUTTER,      "OP"},
    {CLOSE_SHUTTER,     "CL"},
};

typedef enum
{
    XBEE_START,
    XBEE_WAIT,
    XBEE_CONFIG,
    XBEE_DETECT,
    XBEE_ONLINE
} XBee;

static const std::map<XBee, std::string> XBeeMap =
{
    {XBEE_START, "Start"},
    {XBEE_WAIT,  "WaitAt"},
    {XBEE_CONFIG, "Config"},
    {XBEE_DETECT, "Detect"},
    {XBEE_ONLINE, "Online"},
};

typedef enum
{
    XBEE_STATE,
    ROTATOR_REPORT,
    SHUTTER_REPORT,
    ROTATOR_LEFT,
    ROTATOR_RIGHT,
    SHUTTER_OPENING,
    SHUTTER_CLOSING,
    SHUTTER_BATTERY,
    RAIN_DETECTED,
    RAIN_STOPPED,
    ROTATOR_STOPPED,
    ROTATOR_POSITION,
    SHUTTER_POSITION,
} Events;

static const std::map<Events, std::string> EventsMap =
{
    {XBEE_STATE,        "XB->"},
    {ROTATOR_REPORT,    "SER,"},
    {SHUTTER_REPORT,    "SES,"},
    {ROTATOR_LEFT,      "left"},
    {ROTATOR_RIGHT,     "right"},
    {SHUTTER_OPENING,   "open"},
    {SHUTTER_CLOSING,   "close"},
    {SHUTTER_BATTERY,   "BV"},
    {RAIN_DETECTED,     "Rain"},
    {RAIN_STOPPED,      "RainStopped"},
    {ROTATOR_STOPPED,   "STOP"},
    {ROTATOR_POSITION,  "P"},
    {SHUTTER_POSITION,  "S"},
};

// # is the stop char
const char DRIVER_STOP_CHAR { '#' };
// \n is the event stop char
const char DRIVER_EVENT_CHAR { '\n' };
// Wait up to a maximum of 3 seconds for serial input
const uint8_t DRIVER_TIMEOUT {3};
// Wait up to a maximum of 1 for event input
const uint8_t DRIVER_EVENT_TIMEOUT {1};
// Maximum buffer for sending/receving.
const uint16_t DRIVER_LEN {512};
// ADU to VRef
const double ADU_TO_VREF { 5.0 / 1023 * 3.0 };
// Minimim supported version
const std::string MINIMUM_VERSION { "3.1.0" };
// Tabs
const std::string SHUTTER_TAB {"Shutter"};
const std::string ROTATOR_TAB {"Rotator"};
}

