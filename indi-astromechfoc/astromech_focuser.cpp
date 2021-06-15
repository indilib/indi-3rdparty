/*******************************************************************************
 Copyright(c) 2019 Christian Liska. All rights reserved.

 Implementation based on Lacerta MFOC driver
 (written 2018 by Franck Le Rhun and Christian Liska).

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

#include "astromech_focuser.h"
#include "config.h"

#include <indicom.h>
#include <connectionplugins/connectionserial.h>

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>

static std::unique_ptr<astromechanics_foc> Astromechanics_foc(new astromechanics_foc());

// Delay for receiving messages
#define FOCUS_TIMEOUT  1000
#define FOC_POSMAX_HARDWARE 9999
#define FOC_POSMIN_HARDWARE 0

bool astromechanics_foc::Disconnect()
{
    SetApperture(0);
    return true;
}

void ISSnoopDevice(XMLEle *root)
{
    Astromechanics_foc->ISSnoopDevice(root);
}

/************************************************************************************
 *
************************************************************************************/
astromechanics_foc::astromechanics_foc()
{
    setVersion(INDI_ASTROMECHFOC_VERSION_MAJOR, INDI_ASTROMECHFOC_VERSION_MINOR);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
}

/************************************************************************************
 *
************************************************************************************/
const char *astromechanics_foc::getDefaultName()
{
    return "Astromechanics FOC";
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::initProperties()
{
    INDI::Focuser::initProperties();

    FocusMaxPosN[0].min = FOC_POSMIN_HARDWARE;
    FocusMaxPosN[0].max = FOC_POSMAX_HARDWARE;
    FocusMaxPosN[0].step = 500;
    FocusMaxPosN[0].value = FOC_POSMAX_HARDWARE;

    FocusAbsPosN[0].min = FOC_POSMIN_HARDWARE;
    FocusAbsPosN[0].max = FOC_POSMAX_HARDWARE;
    FocusAbsPosN[0].step = 500;
    FocusAbsPosN[0].value = 0;

    FocusRelPosN[0].min = FocusAbsPosN[0].min;
    FocusRelPosN[0].max = FocusAbsPosN[0].max / 2;
    FocusRelPosN[0].step = 250;
    FocusRelPosN[0].value = 0;

    // Aperture
    IUFillNumber(&AppertureN[0], "LENS_APP", "Index", "%2d", 0, 22, 1, 0);
    IUFillNumberVector(&AppertureNP, AppertureN, 1, getDeviceName(), "LENS_APP_SETTING", "Apperture", MAIN_CONTROL_TAB, IP_RW,
                       60, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_38400);
    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::updateProperties()
{
    // Get Initial Position before we define it in the INDI::Focuser class
    FocusAbsPosN[0].value = GetAbsFocuserPosition();

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&AppertureNP);
    }
    else
    {
        deleteProperty(AppertureNP.name);
    }

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::Handshake()
{
    char FOC_cmd[32] = "P#";
    char FOC_res[32] = {0};
    int FOC_pos_measd = 0;
    int nbytes_written = 0;
    int nbytes_read = 0;

    LOG_DEBUG("Handshake");

    tty_write_string(PortFD, FOC_cmd, &nbytes_written);
    LOGF_INFO("CMD <%s>", FOC_cmd);
    if (tty_read_section(PortFD, FOC_res, '#', FOCUS_TIMEOUT, &nbytes_read) == TTY_OK)
    {
        LOGF_DEBUG("RES (%s)", FOC_res);
        sscanf(FOC_res, "%d#", &FOC_pos_measd);
        LOGF_INFO("Set to absolute focus position (%d)", FOC_pos_measd);
        FocusAbsPosN[0].value = FOC_pos_measd;
        FocusAbsPosNP.s = IPS_OK;

        SetApperture(0);
        return true;
    }
    else
    {
        LOG_ERROR("ERROR HANDSHAKE");
    }

    return false;
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "LENS_APP_SETTING") == 0)
        {
            AppertureNP.s = IPS_OK;
            IUUpdateNumber(&AppertureNP, values, names, n);

            IDSetNumber(&AppertureNP, nullptr);
            SetApperture(AppertureN[0].value);

            return true;
        }
    }

    // Let INDI::Focuser handle any other number properties
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
************************************************************************************/
IPState astromechanics_foc::MoveAbsFocuser(uint32_t targetTicks)
{
    LOGF_DEBUG("MoveAbsFocuser (%d)", targetTicks);

    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        LOG_ERROR("Error, requested position is out of range!");
        return IPS_ALERT;
    }

    char FOC_cmd[32]  = "M";
    char abs_pos_char[32]  = {0};
    int nbytes_written = 0;

    sprintf(abs_pos_char, "%u", targetTicks);
    strcat(abs_pos_char, "#");
    strcat(FOC_cmd, abs_pos_char);

    LOGF_DEBUG("CMD <%s>", FOC_cmd);
    tty_write_string(PortFD, FOC_cmd, &nbytes_written);

    FocusAbsPosN[0].value = GetAbsFocuserPosition();

    return IPS_OK;
}

/************************************************************************************
 *
************************************************************************************/
IPState astromechanics_foc::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    // Clamp
    int32_t offset = ((dir == FOCUS_INWARD) ? -1 : 1) * static_cast<int32_t>(ticks);
    int32_t newPosition = FocusAbsPosN[0].value + offset;
    newPosition = std::max(static_cast<int32_t>(FocusAbsPosN[0].min), std::min(static_cast<int32_t>(FocusAbsPosN[0].max),
                           newPosition));

    FocusAbsPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, nullptr);

    return MoveAbsFocuser(newPosition);
}

/************************************************************************************
 *
************************************************************************************/
void astromechanics_foc::SetApperture(uint32_t index)
{
    LOGF_DEBUG("SetApperture(%d)", index);
    char FOC_cmd[32] = "A";
    char app_index_char[32]  = {0};
    int nbytes_written = 0;

    sprintf(app_index_char, "%u", index);
    strcat(app_index_char, "#");
    strcat(FOC_cmd, app_index_char);

    tty_write_string(PortFD, FOC_cmd, &nbytes_written);
}

/************************************************************************************
 *
************************************************************************************/
uint32_t astromechanics_foc::GetAbsFocuserPosition()
{
    LOGF_DEBUG("GetAbsFocuserPosition", 0);
    char FOC_cmd[32] = "P#";
    char FOC_res[32] = {0};
    int FOC_pos_measd = 0;

    int nbytes_written = 0;
    int nbytes_read = 0;

    tty_write_string(PortFD, FOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD (%s)", FOC_cmd);
    if (tty_read_section(PortFD, FOC_res, '#', FOCUS_TIMEOUT, &nbytes_read) == TTY_OK)
    {
        sscanf(FOC_res, "%d#", &FOC_pos_measd);

        LOGF_DEBUG("RES (%s)", FOC_res);
        LOGF_INFO("current position: %d", FOC_pos_measd);
    }

    return static_cast<uint32_t>(FOC_pos_measd);
}
