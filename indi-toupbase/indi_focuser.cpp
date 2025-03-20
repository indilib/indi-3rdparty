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
                if (CP(FLAG_AUTOFOCUSER) & pAAFInfo[i].model->flag)
                {
                    std::string name = std::string(DNAME) + " AAF";
                    if (iConnectedCount > 1)
                        name += " " + std::to_string(i + 1);
                    focusers.push_back(std::unique_ptr<ToupAAF>(new ToupAAF(&pAAFInfo[i], name.c_str())));
                }
            }
            if (focusers.empty())
                IDLog("No focuser detected.");
        }
} loader;

ToupAAF::ToupAAF(const XP(DeviceV2) *instance, const char *name) : m_Instance(instance)
{
    setVersion(TOUPBASE_VERSION_MAJOR, TOUPBASE_VERSION_MINOR);
	
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and can reverse.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_HAS_BACKLASH);

    // Just USB
    setSupportedConnections(CONNECTION_NONE);
	
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
	
    FocusBacklashNP[0].setMin(0.);
    FocusRelPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMin(0.);

    FocusBacklashNP[0].setValue(0.);
    FocusRelPosNP[0].setValue(0.);
    FocusAbsPosNP[0].setValue(0.);

    FocusBacklashNP[0].setStep(1.);
    FocusRelPosNP[0].setStep(1.);
    FocusAbsPosNP[0].setStep(1.);
	
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
		defineProperty(BeepSP);
		
		readTemperature();
		TemperatureNP.setState(IPS_OK);
		defineProperty(TemperatureNP);
		
	    if (readPosition())
			FocusAbsPosNP.apply();
		if (readReverse())
			FocusReverseSP.apply();
		if (readBeep())
			BeepSP.apply();
		if (readBacklash())
			FocusBacklashNP.apply();

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
        LOG_ERROR("Failed to connect focuser");
        return false;
    }
	
    LOGF_INFO("%s is connected.", getDeviceName());
	return readMaxPosition();
}

bool ToupAAF::Disconnect()
{
	FP(Close(m_Handle));
	return true;
}

bool ToupAAF::SetFocuserMaxPosition(uint32_t ticks)
{
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_SETMAXSTEP), ticks, nullptr));
    if (FAILED(rc))
    {
        LOGF_ERROR("SetFocuserMaxPosition failed. %s", errorCodes(rc).c_str());
        return false;
    }
    return true;
}

bool ToupAAF::SetFocuserBacklash(int32_t steps)
{
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_SETBACKLASH), steps, nullptr));
    if (FAILED(rc))
    {
        LOGF_ERROR("SetFocuserBacklash failed. %s", errorCodes(rc).c_str());
        return false;
    }
    return true;
}

bool ToupAAF::ReverseFocuser(bool enabled)
{
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_SETDIRECTION), enabled ? 1 : 0, nullptr));
    if (FAILED(rc))
    {
        LOGF_ERROR("SyncFocuser failed. %s", errorCodes(rc).c_str());
        return false;
    }
    return true;
}

bool ToupAAF::SyncFocuser(uint32_t ticks)
{
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_SETZERO), ticks, nullptr));
    if (FAILED(rc))
    {
        LOGF_ERROR("SyncFocuser failed. %s", errorCodes(rc).c_str());
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
        HRESULT rc = FP(AAF(m_Handle, CP(AAF_SETBUZZER), (BeepSP.findOnSwitchIndex() == BEEP_ON) ? 1 : 0, nullptr));
        if (SUCCEEDED(rc))
            BeepSP.setState(IPS_OK);
        else
        {
            BeepSP.setState(IPS_ALERT);
            LOGF_ERROR("Failed to set beep. %s", errorCodes(rc).c_str());
        }

        BeepSP.apply();
        return true;
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

IPState ToupAAF::MoveAbsFocuser(uint32_t targetTicks)
{
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_SETPOSITION), targetTicks, nullptr));
    if (FAILED(rc))
    {
        LOGF_ERROR("MoveAbsFocuser failed. %s", errorCodes(rc).c_str());
        return IPS_ALERT;
    }

    return IPS_BUSY;
}

IPState ToupAAF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()), newPosition));
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_SETPOSITION), newPosition, nullptr));
    if (FAILED(rc))
    {
        LOGF_ERROR("MoveRelFocuser failed. %s", errorCodes(rc).c_str());
        return IPS_ALERT;
    }

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void ToupAAF::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (readPosition())
        FocusAbsPosNP.apply();

    if (TemperatureNP.getState() != IPS_IDLE)
    {
        if (readTemperature())
            TemperatureNP.apply();
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool ToupAAF::AbortFocuser()
{
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_HALT), 0, nullptr));
    if (FAILED(rc))
    {
        LOGF_ERROR("AbortFocuser failed. %s", errorCodes(rc).c_str());
        return false;
    }
    return true;
}

bool ToupAAF::readPosition()
{
    int val = 0;
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_GETPOSITION), 0, &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("readPosition failed. %s", errorCodes(rc).c_str());
        return false;
    }
    FocusAbsPosNP[0].setValue(val);
    return true;
}

bool ToupAAF::readMaxPosition()
{
    int val = 0;
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_RANGEMAX), CP(AAF_GETMAXSTEP), &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("get range max for maxstep failed. %s", errorCodes(rc).c_str());
        return false;
    }
    FocusMaxPosNP[0].setMax(val);
	
    rc = FP(AAF(m_Handle, CP(AAF_GETMAXSTEP), 0, &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("get maxstep failed. %s", errorCodes(rc).c_str());
        return false;
    }
    FocusMaxPosNP[0].setValue(val);         
    FocusAbsPosNP[0].setMax(FocusMaxPosNP[0].getValue());
    FocusRelPosNP[0].setMax(val / 2.0);
	
    rc = FP(AAF(m_Handle, CP(AAF_RANGEMAX), CP(AAF_GETBACKLASH), &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("get range max for backlash failed. %s", errorCodes(rc).c_str());
        return false;
    }
    FocusBacklashNP[0].setMax(val);

    return true;
}

bool ToupAAF::readReverse()
{
	int val = 0;
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_GETDIRECTION), 0, &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("readReverse failed. %s", errorCodes(rc).c_str());
        return false;
    }

    FocusReverseSP[INDI_ENABLED].setState(val ? ISS_ON : ISS_OFF);
    FocusReverseSP[INDI_DISABLED].setState(val ? ISS_OFF : ISS_ON);
    FocusReverseSP.setState(IPS_OK);
    return true;
}

bool ToupAAF::readBacklash()
{
	int val = 0;
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_GETBACKLASH), 0, &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("readBacklash failed. %s", errorCodes(rc).c_str());
        return false;
    }
    FocusBacklashNP[0].setValue(val);
    FocusBacklashNP.setState(IPS_OK);
    return true;
}

bool ToupAAF::readBeep()
{
	int val = 0;
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_GETBUZZER), 0, &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("readBeep failed. %s", errorCodes(rc).c_str());
        return false;
    }

    BeepSP[INDI_ENABLED].setState(val ? ISS_ON : ISS_OFF);
    BeepSP[INDI_DISABLED].setState(val ? ISS_OFF : ISS_ON);
    BeepSP.setState(IPS_OK);

    return true;
}

bool ToupAAF::readTemperature()
{
	int curTemperature = 0;
    HRESULT rc = FP(AAF(m_Handle, CP(AAF_GETTEMP), 0, &curTemperature));
    if (FAILED(rc))
    {
        LOGF_ERROR("readTemperature failed. %s", errorCodes(rc).c_str());
        return false;
    }
	
	if (fabs(curTemperature / 10.0 - TemperatureNP[0].getValue()) >= 0.1)
		TemperatureNP[0].setValue(curTemperature / 10.0);
    return true;
}

bool ToupAAF::isMoving()
{
	int val = 0;
	HRESULT rc = FP(AAF(m_Handle, CP(AAF_ISMOVING), 0, &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("isMoving failed. %s", errorCodes(rc).c_str());
        return false;
    }
    return (val ? true : false);
}
