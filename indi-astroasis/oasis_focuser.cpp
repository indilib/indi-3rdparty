/*
    Astroasis Oasis Focuser
    Copyright (C) 2013-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2023 Frank Chen (frank.chen@astroasis.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
    USA

*/

#include "oasis_focuser.h"
#include "AOFocus.h"
#include "indicom.h"

static class Loader
{
        std::deque<std::unique_ptr<OasisFocuser>> focusers;
    public:
        Loader()
        {
            focusers.push_back(std::unique_ptr<OasisFocuser>(new OasisFocuser()));
        }
} loader;

OasisFocuser::OasisFocuser()
{
    setVersion(1, 0);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_HAS_BACKLASH);

    // We use USB connection
    setSupportedConnections(CONNECTION_NONE);
}

bool OasisFocuser::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focus motion beep
    BeepOnMoveSP[INDI_ENABLED].fill("ON", "On", ISS_ON);
    BeepOnMoveSP[INDI_DISABLED].fill("OFF", "Off", ISS_OFF);
    BeepOnMoveSP.fill(getDeviceName(), "FOCUS_BEEP", "Beep", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware version
    VersionSP[0].fill("VERSION_FIRMWARE", "Firmware", "Unknown");
    VersionSP[1].fill("VERSION_SDK", "SDK", "Unknown");
    VersionSP.fill(getDeviceName(), "VERSION", "Version", INFO_TAB, IP_RO, 60, IPS_IDLE);

    FocusBacklashN[0].min = 0;
    FocusBacklashN[0].max = 0x7fffffff;
    FocusBacklashN[0].value = 0;
    FocusBacklashN[0].step = 1;

    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 0x7fffffff;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 0x7fffffff;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1;

    FocusMaxPosN[0].min = 0;
    FocusMaxPosN[0].max = 0x7fffffff;
    FocusMaxPosN[0].value = 0x7fffffff;
    FocusMaxPosN[0].step = 1;

    setDefaultPollingPeriod(500);
    addDebugControl();

    return true;
}

bool OasisFocuser::updateProperties()
{
    if (isConnected())
    {
        GetConfig();
	GetStatus();

        TemperatureNP.setState(IPS_OK);
        defineProperty(TemperatureNP);

        defineProperty(BeepOnMoveSP);

	// Update version info
        AOFocuserVersion version;
        char ver[AO_FOCUSER_VERSION_LEN];

	if (AOFocuserGetVersion(mID, &version) == AO_SUCCESS)
	{
	    unsigned int firmware = version.firmware;

            snprintf(ver, sizeof(ver), "%d.%d.%d",
                firmware >> 24, (firmware >> 16) & 0xff, (firmware >> 8) & 0xff);

            VersionSP[0].setText(ver);
	}

	AOFocuserGetSDKVersion(ver);
        VersionSP[1].setText(ver);

        defineProperty(VersionSP);

        IDSetNumber(&FocusAbsPosNP, nullptr);
        IDSetSwitch(&FocusReverseSP, nullptr);
        IDSetNumber(&FocusBacklashNP, nullptr);

        LOG_INFO("Oasis Focuser parameters updated, focuser ready for use.");

        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(BeepOnMoveSP);
        deleteProperty(VersionSP);
    }

    return INDI::Focuser::updateProperties();
}

const char * OasisFocuser::getDefaultName()
{
    return "Oasis Focuser";
}

bool OasisFocuser::Connect()
{
    int number, ids[AO_FOCUSER_MAX_NUM];

    AOFocuserScan(&number, ids);

    if (number <= 0)
        return false;

    // For now we always use the first found Oasis Focuser
    mID = ids[0];

    return (AOFocuserOpen(mID) == AO_SUCCESS);
}

bool OasisFocuser::Disconnect()
{
    AOFocuserClose(mID);

    return true;
}

bool OasisFocuser::GetConfig()
{
    AOFocuserConfig config;

    AOReturn ret = AOFocuserGetConfig(mID, &config);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to get oasis focuser configration, ret = %d\n", ret);
        return false;
    }

    // Update reverse settings
    FocusReverseS[INDI_ENABLED].s = config.reverseDirection ? ISS_ON : ISS_OFF;
    FocusReverseS[INDI_DISABLED].s = config.reverseDirection ? ISS_OFF : ISS_ON;
    FocusReverseSP.s = IPS_OK;

    // Update max step
    FocusAbsPosN[0].max = config.maxStep;

    // Update backlash
    FocusBacklashN[0].value = config.backlash;
    FocusBacklashNP.s = IPS_OK;

    // Update beep settings
    BeepOnMoveSP[INDI_ENABLED].setState(config.beepOnMove ? ISS_ON : ISS_OFF);
    BeepOnMoveSP[INDI_DISABLED].setState(config.beepOnMove ? ISS_OFF : ISS_ON);
    BeepOnMoveSP.setState(IPS_OK);

    return true;
}

bool OasisFocuser::GetStatus()
{
    AOFocuserStatus status;

    AOReturn ret = AOFocuserGetStatus(mID, &status);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to get oasis focuser status, ret = %d\n", ret);
        return false;
    }

    FocusAbsPosN[0].value = status.position;
    TemperatureNP[0].setValue(status.temperatureExt / 100.0);

    return true;
}

bool OasisFocuser::SetFocuserMaxPosition(uint32_t ticks)
{
    AOFocuserConfig config;

    config.mask = MASK_MAX_STEP;
    config.maxStep = ticks;

    AOReturn ret = AOFocuserSetConfig(mID, &config);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to set Oasis Focuser max position, ret = %d\n", ret);
        return false;
    }

    return true;
}

bool OasisFocuser::SetFocuserBacklash(int32_t steps)
{
    AOFocuserConfig config;

    config.mask = MASK_BACKLASH;
    config.backlash = steps;

    AOReturn ret = AOFocuserSetConfig(mID, &config);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to set Oasis Focuser backlash, ret = %d\n", ret);
        return false;
    }

    return true;
}

bool OasisFocuser::ReverseFocuser(bool enabled)
{
    AOFocuserConfig config;

    config.mask = MASK_REVERSE_DIRECTION;
    config.reverseDirection = enabled ? 1 : 0;

    AOReturn ret = AOFocuserSetConfig(mID, &config);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to set Oasis Focuser direction, ret = %d\n", ret);
        return false;
    }

    return true;
}

bool OasisFocuser::isMoving()
{
    AOFocuserStatus status;

    AOReturn ret = AOFocuserGetStatus(mID, &status);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to get oasis focuser status, ret = %d\n", ret);
        return false;
    }

    return (status.moving != 0);
}

bool OasisFocuser::SyncFocuser(uint32_t ticks)
{
    AOReturn ret = AOFocuserSyncPosition(mID, ticks);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to sync Oasis Focuser position, ret = %d\n", ret);
        return false;
    }

    return true;
}

bool OasisFocuser::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()) != 0)
    {
        return false;
    }

    // Trun on/off beep
    if (BeepOnMoveSP.isNameMatch(name))
    {
        BeepOnMoveSP.update(states, names, n);

	AOFocuserConfig config;

	config.mask = MASK_BEEP_ON_MOVE;
	config.beepOnMove = (BeepOnMoveSP.findOnSwitchIndex() == INDI_ENABLED) ? 1 : 0;

	AOReturn ret = AOFocuserSetConfig(mID, &config);

	if (ret == AO_SUCCESS)
        {
            BeepOnMoveSP.setState(IPS_OK);
        }
        else
        {
            BeepOnMoveSP.setState(IPS_ALERT);
            LOGF_ERROR("Failed to set Oasis Focuser BeepOnMove, ret = %d\n", ret);
        }

        BeepOnMoveSP.apply();

	return true;
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool OasisFocuser::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState OasisFocuser::MoveAbsFocuser(uint32_t targetTicks)
{
    AOReturn ret = AOFocuserMoveTo(mID, targetTicks);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("MoveAbsFocuser() failed, ret = %d\n", ret);
        return IPS_ALERT;
    }

    return IPS_BUSY;
}

IPState OasisFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));

    AOReturn ret = AOFocuserMoveTo(mID, newPosition);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("MoveRelFocuser() failed, ret = %d\n", ret);
        return IPS_ALERT;
    }

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

void OasisFocuser::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (GetStatus())
    {
        IDSetNumber(&FocusAbsPosNP, nullptr);

        if (TemperatureNP.getState() != IPS_IDLE)
            TemperatureNP.apply();
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool OasisFocuser::AbortFocuser()
{
    AOReturn ret = AOFocuserStopMove(mID);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to stop Oasis Focuser, ret = %d\n", ret);
        return false;
    }

    return true;
}

bool OasisFocuser::saveConfigItems(FILE * fp)
{
    return INDI::Focuser::saveConfigItems(fp);
}
