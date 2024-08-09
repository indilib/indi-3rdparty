/*
    Toupcam & oem Focuser
    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "indi_focuser.h"
#include "config.h"
#include <unistd.h>
#include <deque>

static class Loader
{
        std::deque<std::unique_ptr<ToupAAF>> focusers;
		XP(DeviceV2) pAAFInfo[CP(MAX)];
    public:
        Loader()
        {
            const int iConnectedCount = FP(EnumV2(pAAFInfo));
            for (int i = 0; i < iConnectedCount; i++)
            {
                if (CP(TOUPCAM_FLAG_AUTOFOCUSER) & pAAFInfo[i].model->flag)
                {
                    std::string name = std::string(DNAME) + " AAF";
                    if (iConnectedCount > 1)
                        name += " " + std::to_string(i + 1);
                    wheels.push_back(std::unique_ptr<ToupAAF>(new ToupAAF(&pWheelInfo[i], name.c_str())));
                }
            }
            if (wheels.empty())
                IDLog("No focuser detected.");
        }
} loader;

ToupAAF::ToupAAF(const XP(DeviceV2) *instance, const char *name) : m_Instance(instance)
{
    setVersion(TOUPBASE_VERSION_MAJOR, TOUPBASE_VERSION_MINOR);
    setDeviceName(name);
}

const char *ToupAAF::getDefaultName()
{
    return DNAME;
}

bool ToupAAF::initProperties()
{
    INDI::Focuser::initProperties();

    VersionTP[TC_FW_VERSION].fill("FIRMWARE", "Firmware", nullptr);
    VersionTP[TC_HW_VERSION].fill("HARDWARE", "Hardware", nullptr);
    VersionTP[TC_REV].fill("REVISION", "Revision", nullptr);
    VersionTP[TC_SDK].fill("SDK", "SDK", FP(Version()));
    VersionTP.fill(getDeviceName(), "VERSION", "Version", INFO_TAB, IP_RO, 0, IPS_IDLE);
	
    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focus motion beep
    BeepSP[BEEP_ON].fill("ON", "On", ISS_ON);
    BeepSP[BEEP_OFF].fill("OFF", "Off", ISS_OFF);
    BeepSP.fill(getDeviceName(), "FOCUS_BEEP", "Beep", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware version
    VersionInfoSP[0].fill("VERSION_FIRMWARE", "Firmware", "Unknown");
    VersionInfoSP[1].fill("VERSION_SDK", "SDK", "Unknown");
    VersionInfoSP.fill(getDeviceName(), "VERSION", "Version", INFO_TAB, IP_RO, 60, IPS_IDLE);

    setDefaultPollingPeriod(500);

    addDebugControl();

    return true;
}

bool ToupAAF::updateProperties()
{
    if (isConnected())
    {
        char tmpBuffer[64] = {0};
        uint16_t pRevision = 0;
        FP(get_FwVersion(m_Handle, tmpBuffer));
        IUSaveText(&VersionTP[TC_FW_VERSION], tmpBuffer);
        FP(get_HwVersion(m_Handle, tmpBuffer));
        IUSaveText(&VersionTP[TC_HW_VERSION], tmpBuffer);
        FP(get_Revision(m_Handle, &pRevision));
        snprintf(tmpBuffer, 32, "%d", pRevision);
        IUSaveText(&VersionTP[TC_REV], tmpBuffer);

        defineProperty(VersionTP);

        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        if (TemperatureNP.getState() != IPS_IDLE)
            deleteProperty(TemperatureNP);
        deleteProperty(BeepSP);
        deleteProperty(VersionTP);
    }

    return INDI::Focuser::updateProperties();
}

bool ToupAAF::Connect()
{
    m_Handle = FP(Open(m_Instance->id));

    if (m_Handle == nullptr)
    {
        LOG_ERROR("Failed to connect filterwheel");
        return false;
    }
	
    LOGF_INFO("%s is connected.", getDeviceName());
    return true;	
}

bool ToupAAF::Disconnect()
{
	FP(Close(m_Handle));
	return true;
}

bool ToupAAF::SetFocuserMaxPosition(uint32_t ticks)
{
    EAF_ERROR_CODE rc = EAFSetMaxStep(m_ID, ticks);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set max step. Error: %d", rc);
        return false;
    }
    return true;
}

bool ToupAAF::SetFocuserBacklash(int32_t steps)
{
    EAF_ERROR_CODE rc = EAFSetBacklash(m_ID, steps);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set backlash compensation. Error: %d", rc);
        return false;
    }
    return true;
}

bool ToupAAF::ReverseFocuser(bool enabled)
{
    EAF_ERROR_CODE rc = EAFSetReverse(m_ID, enabled);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set reversed status. Error: %d", rc);
        return false;
    }

    return true;
}

bool ToupAAF::SyncFocuser(uint32_t ticks)
{
    EAF_ERROR_CODE rc = EAFResetPostion(m_ID, ticks);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to sync focuser. Error: %d", rc);
        return false;
    }
    return true;
}

bool ToupAAF::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()) != 0)
    {
        return false;
    }

    // Turn on/off beep
    if (BeepSP.isNameMatch(name))
    {
        BeepSP.update(states, names, n);
        EAF_ERROR_CODE rc = EAF_SUCCESS;
        if (BeepSP.findOnSwitchIndex() == BEEP_ON)
            rc = EAFSetBeep(m_ID, true);
        else
            rc = EAFSetBeep(m_ID, false);

        if (rc == EAF_SUCCESS)
        {
            BeepSP.setState(IPS_OK);
        }
        else
        {
            BeepSP.setState(IPS_ALERT);
            LOGF_ERROR("Failed to set beep state. Error: %d", rc);
        }

        BeepSP.apply();
        return true;
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

IPState ToupAAF::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (!gotoAbsolute(targetPos))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState ToupAAF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    if (!gotoAbsolute(newPosition))
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void ToupAAF::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = readPosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    if (TemperatureNP.getState() != IPS_IDLE)
    {
        rc = readTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureNP[0].getValue()) >= 0.1)
            {
                TemperatureNP.apply();
                lastTemperature = TemperatureNP[0].getValue();
            }
        }
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool ToupAAF::AbortFocuser()
{
    EAF_ERROR_CODE rc = EAFStop(m_ID);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to stop focuser. Error: %d", rc);
        return false;
    }
    return true;
}
