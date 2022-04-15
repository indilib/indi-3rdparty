/*
    ZWO EFA Focuser
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

#include "asi_focuser.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <deque>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define FOCUS_SETTINGS_TAB "Settings"

static class Loader
{
        std::deque<std::unique_ptr<ASIEAF>> focusers;
    public:
        Loader()
        {
            int iAvailableFocusersCount = EAFGetNum();

            if (iAvailableFocusersCount <= 0)
            {
                IDLog("No ASI EAF detected.");
                return;
            }

            int iAvailableFocusersCount_ok = 0;
            for (int i = 0; i < iAvailableFocusersCount; i++)
            {
                int id;
                EAF_ERROR_CODE result = EAFGetID(i, &id);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ASI EAF %d EAFGetID error %d.", i + 1, result);
                    continue;
                }

                // Open device
                result = EAFOpen(id);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ASI EAF %d Failed to open device %d.", i + 1, result);
                    continue;
                }

                EAF_INFO info;
                result = EAFGetProperty(id, &info);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ASI EAF %d EAFGetProperty error %d.", i + 1, result);
                    continue;
                }
                EAFClose(id);

                std::string name = "ASI EAF";

                // If we only have a single device connected
                // then favor the INDIDEV driver label over the auto-generated name above
                if (iAvailableFocusersCount == 1)
                {
                    char *envDev = getenv("INDIDEV");
                    if (envDev && envDev[0])
                        name = envDev;
                }
                else
                    name += " " + std::to_string(i);

                focusers.push_back(std::unique_ptr<ASIEAF>(new ASIEAF(info, name.c_str())));
                iAvailableFocusersCount_ok++;
            }
            IDLog("%d ASI EAF attached out of %d detected.", iAvailableFocusersCount_ok, iAvailableFocusersCount);
        }
} loader;

ASIEAF::ASIEAF(const EAF_INFO &info, const char *name)
    : m_ID(info.ID)
    , m_MaxSteps(info.MaxStep)
{
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

    FocusAbsPosN[0].max = m_MaxSteps;
}

bool ASIEAF::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focus motion beep
    IUFillSwitch(&BeepS[BEEP_ON], "ON", "On", ISS_ON);
    IUFillSwitch(&BeepS[BEEL_OFF], "OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&BeepSP, BeepS, 2, getDeviceName(), "FOCUS_BEEP", "Beep", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware version
    IUFillText(&VersionInfoS[0], "VERSION_FIRMWARE", "Firmware", "Unknown");
    IUFillTextVector(&VersionInfoSP, VersionInfoS, 1, getDeviceName(), "VERSION", "Version", INFO_TAB, IP_RO, 60, IPS_IDLE);

    //
    // Temperature compensation
    //
    // Switch : enable or disable temperature compensation
    IUFillSwitch(&TempCS[TEMPC_ON], "ON", "On", ISS_OFF);
    IUFillSwitch(&TempCS[TEMPC_OFF], "OFF", "Off", ISS_ON);
    IUFillSwitchVector(&TempCSP, TempCS, 2, getDeviceName(), "TEMPC_SWITCH", "Temperature Compensation", TEMPC_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Numbers :
    //
    // STEPS : number of steps to move (inward ou outward) per C degree variation (usually negative)
    // HYSTERESIS : minimal temperature variation before triggering moves
    // SAMPLES : for better and consistents readings, sum up the temperature samples
    IUFillNumber(&TempCN[TEMPC_STEPS], "STEPS", "Steps per Celsius", "%.f", -200, 200, 1, 0);
    IUFillNumber(&TempCN[TEMPC_HYSTER], "HYSTERESIS", "Delta in Celsius", "%.1f", 0, 10, 0.1, 1.0);
    IUFillNumber(&TempCN[TEMPC_SAMPLES], "SAMPLES", "Number of samples", "%.f", 1, 120, 1, 5);
    IUFillNumber(&TempCN[TEMPC_MEAN], "MEAN", "Celsius", "%.2f", -274, 100, 0.1, 0);
    IUFillNumberVector(&TempCNP[TEMPC_STEPS], &TempCN[TEMPC_STEPS], 1, getDeviceName(), "TEMPC_STEPSDEG", "Steps", TEMPC_TAB, IP_RW, 0, IPS_IDLE);
    IUFillNumberVector(&TempCNP[TEMPC_HYSTER], &TempCN[TEMPC_HYSTER], 1, getDeviceName(), "TEMPC_HYSTERESIS", "Hysteresis", TEMPC_TAB, IP_RW, 0, IPS_IDLE);
    IUFillNumberVector(&TempCNP[TEMPC_SAMPLES], &TempCN[TEMPC_SAMPLES], 1, getDeviceName(), "TEMPC_SAMPLES", "Samples", TEMPC_TAB, IP_RW, 0, IPS_IDLE);
    IUFillNumberVector(&TempCNP[TEMPC_MEAN], &TempCN[TEMPC_MEAN], 1, getDeviceName(), "TEMPC_MEAN", "Mean temperature", TEMPC_TAB, IP_RO, 0, IPS_IDLE);
    //

    FocusBacklashN[0].min = 0;
    FocusBacklashN[0].max = 9999;
    FocusBacklashN[0].step = 100;
    FocusBacklashN[0].value = 0;

    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = m_MaxSteps / 2.0;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = FocusRelPosN[0].max / 20;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = m_MaxSteps;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = m_MaxSteps / 20.0;

    setDefaultPollingPeriod(500);

    addDebugControl();

    return true;
}

bool ASIEAF::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        float temperature = -273;
        EAFGetTemp(m_ID, &temperature);

        if (temperature != -273)
        {
            TemperatureN[0].value = temperature;
            TemperatureNP.s = IPS_OK;
            defineProperty(&TemperatureNP);
        }

        defineProperty(&BeepSP);

        //        defineProperty(&FocuserBacklashSP);
        //        defineProperty(&BacklashNP);

        char firmware[12];
        unsigned char major, minor, build;
        EAFGetFirmwareVersion(m_ID, &major, &minor, &build);
        snprintf(firmware, sizeof(firmware), "%d.%d.%d", major, minor, build);
        IUSaveText(&VersionInfoS[0], firmware);
        defineProperty(&VersionInfoSP);

	//
	// Temperature compensation
	defineProperty(&TempCSP);
	defineProperty(&TempCNP[TEMPC_STEPS]);
	defineProperty(&TempCNP[TEMPC_HYSTER]);
	defineProperty(&TempCNP[TEMPC_SAMPLES]);
	defineProperty(&TempCNP[TEMPC_MEAN]);
	//

        GetFocusParams();

        LOG_INFO("ASI EAF parameters updated, focuser ready for use.");

        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        if (TemperatureNP.s != IPS_IDLE)
            deleteProperty(TemperatureNP.name);
        deleteProperty(BeepSP.name);
        deleteProperty(VersionInfoSP.name);

        //        deleteProperty(FocuserBacklashSP.name);
        //        deleteProperty(BacklashNP.name);

	//
	// Temperature compensation
	deleteProperty(TempCSP.name);
	deleteProperty(TempCNP[TEMPC_STEPS].name);
	deleteProperty(TempCNP[TEMPC_HYSTER].name);
	deleteProperty(TempCNP[TEMPC_SAMPLES].name);
	deleteProperty(TempCNP[TEMPC_MEAN].name);
	//
    }

    return true;
}

const char * ASIEAF::getDefaultName()
{
    return "ASI EAF";
}

bool ASIEAF::Connect()
{
    EAF_ERROR_CODE rc = EAFOpen(m_ID);

    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to connect to ASI EAF focuser ID: %d (%d)", m_ID, rc);
        return false;
    }
    AbortFocuser();
    return readMaxPosition();
}

bool ASIEAF::Disconnect()
{
    return (EAFClose(m_ID) == EAF_SUCCESS);
}

bool ASIEAF::readTemperature()
{
    float temperature = 0;
    EAF_ERROR_CODE rc = EAFGetTemp(m_ID, &temperature);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read temperature. Error: %d", rc);
        return false;
    }

    TemperatureN[0].value = temperature;
    return true;
}

bool ASIEAF::readPosition()
{
    int step = 0;
    EAF_ERROR_CODE rc = EAFGetPosition(m_ID, &step);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read position. Error: %d", rc);
        return false;
    }
    FocusAbsPosN[0].value = step;
    return true;
}

bool ASIEAF::readMaxPosition()
{
    int max = 0;
    EAF_ERROR_CODE rc = EAFGetMaxStep(m_ID, &max);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read max step. Error: %d", rc);
        return false;
    }
    FocusAbsPosN[0].max = max;
    return true;
}

bool ASIEAF::SetFocuserMaxPosition(uint32_t ticks)
{
    EAF_ERROR_CODE rc = EAFSetMaxStep(m_ID, ticks);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set max step. Error: %d", rc);
        return false;
    }
    return true;
}

bool ASIEAF::readReverse()
{
    bool reversed = false;
    EAF_ERROR_CODE rc = EAFGetReverse(m_ID, &reversed);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read reversed status. Error: %d", rc);
        return false;
    }

    FocusReverseS[INDI_ENABLED].s  = reversed ? ISS_ON : ISS_OFF;
    FocusReverseS[INDI_DISABLED].s = reversed ? ISS_OFF : ISS_ON;
    FocusReverseSP.s = IPS_OK;
    return true;
}

bool ASIEAF::readBacklash()
{
    int backv = 0;
    EAF_ERROR_CODE rc = EAFGetBacklash(m_ID, &backv);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read backlash. Error: %d", rc);
        return false;
    }
    FocusBacklashN[0].value = backv;
    FocusBacklashNP.s = IPS_OK;
    return true;
}

//bool ASIEAF::setBacklash(uint32_t ticks)
bool ASIEAF::SetFocuserBacklash(int32_t steps)
{
    EAF_ERROR_CODE rc = EAFSetBacklash(m_ID, steps);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set backlash compensation. Error: %d", rc);
        return false;
    }
    return true;
}

bool ASIEAF::readBeep()
{
    bool beep = false;
    EAF_ERROR_CODE rc = EAFGetBeep(m_ID, &beep);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read beep status. Error: %d", rc);
        return false;
    }

    BeepS[INDI_ENABLED].s  = beep ? ISS_ON : ISS_OFF;
    BeepS[INDI_DISABLED].s = beep ? ISS_OFF : ISS_ON;
    BeepSP.s = IPS_OK;

    return true;
}

bool ASIEAF::ReverseFocuser(bool enabled)
{
    EAF_ERROR_CODE rc = EAFSetReverse(m_ID, enabled);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set reversed status. Error: %d", rc);
        return false;
    }

    return true;
}

bool ASIEAF::isMoving()
{
    bool moving = false;
    bool handcontroller = false;
    EAF_ERROR_CODE rc = EAFIsMoving(m_ID, &moving, &handcontroller);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read motion status. Error: %d", rc);
        return false;
    }
    return moving;
}

bool ASIEAF::SyncFocuser(uint32_t ticks)
{
    EAF_ERROR_CODE rc = EAFResetPostion(m_ID, ticks);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to sync focuser. Error: %d", rc);
        return false;
    }
    return true;
}

bool ASIEAF::gotoAbsolute(uint32_t position)
{
    EAF_ERROR_CODE rc = EAFMove(m_ID, position);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set position. Error: %d", rc);
        return false;
    }
    return true;
}


bool ASIEAF::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Turn on/off beep
        if (!strcmp(name, BeepSP.name))
        {
            EAF_ERROR_CODE rc = EAF_SUCCESS;
            if (!strcmp(BeepS[BEEP_ON].name, IUFindOnSwitchName(states, names, n)))
                rc = EAFSetBeep(m_ID, true);
            else
                rc = EAFSetBeep(m_ID, false);

            if (rc == EAF_SUCCESS)
            {
                IUUpdateSwitch(&BeepSP, states, names, n);
                BeepSP.s = IPS_OK;
            }
            else
            {
                BeepSP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set beep state. Error: %d", rc);
            }

            IDSetSwitch(&BeepSP, nullptr);
            return true;
        }

        // Backlash
        //        else if (!strcmp(name, FocuserBacklashSP.name))
        //        {
        //            IUUpdateSwitch(&FocuserBacklashSP, states, names, n);
        //            bool rc = false;
        //            if (IUFindOnSwitchIndex(&FocuserBacklashSP) == BACKLASH_ENABLED)
        //                rc = setBacklash(BacklashN[0].value);
        //            else
        //                rc = setBacklash(0);

        //            FocuserBacklashSP.s = rc ? IPS_OK : IPS_ALERT;
        //            IDSetSwitch(&FocuserBacklashSP, nullptr);
        //            return true;
        //        }

	//
	// Temperature compensation
	if (!strcmp(name, TempCSP.name))
        {
		IUUpdateSwitch(&TempCSP, states, names, n);
		if (!strcmp(TempCS[TEMPC_ON].name, IUFindOnSwitchName(states, names, n)))
		{
			TempCEnabled=true;
			LOG_INFO("Temperature compensation enabled");
		} else {
			TempCEnabled=false;
			// reset temp control
			TempCTotalTemp=0;
			TempCCounter=0;
			LOG_INFO("Temperature compensation disabled");
		}
                TempCSP.s = IPS_OK;
		IDSetSwitch(&TempCSP, nullptr);
	}
	//

    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool ASIEAF::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

//        if (strcmp(name, BacklashNP.name) == 0)
//        {
//            IUUpdateNumber(&BacklashNP, values, names, n);
//            bool rc = setBacklash(BacklashN[0].value);
//            BacklashNP.s = rc ? IPS_OK : IPS_ALERT;

//            IDSetNumber(&BacklashNP, nullptr);
//            return true;
//        }

	//
	// Temperature compensastion
	if (strcmp(name, TempCNP[TEMPC_STEPS].name) == 0)
	{
		IUUpdateNumber(&TempCNP[TEMPC_STEPS], values, names, n);
		TempCSteps=TempCN[TEMPC_STEPS].value;
		TempCNP[TEMPC_STEPS].s = IPS_OK;
                IDSetNumber(&TempCNP[TEMPC_STEPS], nullptr);
		LOGF_INFO("Step/C set to : %d",TempCSteps);
		return true;
	}
	if (strcmp(name, TempCNP[TEMPC_HYSTER].name) == 0)
        {
		IUUpdateNumber(&TempCNP[TEMPC_HYSTER], values, names, n);
                TempCHyster=TempCN[TEMPC_HYSTER].value;
		TempCNP[TEMPC_HYSTER].s = IPS_OK;
                IDSetNumber(&TempCNP[TEMPC_HYSTER], nullptr);
		LOGF_INFO("Hysteresis set to : %f",TempCHyster);
		return true;
        }
	if (strcmp(name, TempCNP[TEMPC_SAMPLES].name) == 0)
        {
		IUUpdateNumber(&TempCNP[TEMPC_SAMPLES], values, names, n);
                TempCSamples=TempCN[TEMPC_SAMPLES].value;
		TempCNP[TEMPC_SAMPLES].s = IPS_OK;
                IDSetNumber(&TempCNP[TEMPC_SAMPLES], nullptr);
		// reset
		TempCTotalTemp=0;
		TempCCounter=0;
		TempCLastTemp=-274; // zero Kelvin
                LOGF_INFO("Samples set to : %d",TempCSamples);
		return true;
        }
	//

    }
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void ASIEAF::GetFocusParams()
{
    if (readPosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (readReverse())
        IDSetSwitch(&FocusReverseSP, nullptr);

    if (readBeep())
        IDSetSwitch(&BeepSP, nullptr);

    if (readBacklash())
        IDSetNumber(&FocusBacklashNP, nullptr);
}

IPState ASIEAF::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (!gotoAbsolute(targetPos))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState ASIEAF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
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

void ASIEAF::TimerHit()
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

    if (TemperatureNP.s != IPS_IDLE)
    {
        rc = readTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.1)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                lastTemperature = TemperatureN[0].value;
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

    //
    // Temperature compensation
    if(TempCEnabled) {
        // stack the samples values
	if(TempCCounter<TempCSamples) {
	    TempCCounter++;
	    TempCTotalTemp+=TemperatureN[0].value;
	} else {
            // get the new mean temp value
	    double meanTemp=TempCTotalTemp/(double)TempCSamples;
	    // and publish
	    TempCN[TEMPC_MEAN].value=meanTemp;
            TempCNP[TEMPC_MEAN].s = IPS_OK;
            IDSetNumber(&TempCNP[TEMPC_MEAN], nullptr);

	    // wait for 2 measures (last and current)
	    if(TempCLastTemp==-274.0 /* zero Kelvin : no first value */) {
		TempCLastTemp=meanTemp;
            } else {
            	// if temp delta is over hysteris value
            	if(abs(meanTemp-TempCLastTemp)>TempCHyster) {
		    // adjust position
                    int steps=(int)((meanTemp-TempCLastTemp)*(double)TempCSteps);
		    LOGF_INFO("Last Temp. : %.2f New Temp. : %.2f Delta : %.2f, Moving %d steps", TempCLastTemp, meanTemp, meanTemp-TempCLastTemp, steps);
		    FocusDirection dir= steps<0 ? FOCUS_INWARD : FOCUS_OUTWARD ;
		    MoveRelFocuser(dir,abs(steps));
		    // store last measure
		    TempCLastTemp=meanTemp;
	        }
            }

	    // reset
            TempCTotalTemp=0;
            TempCCounter=0;
	}
    }
    //

    SetTimer(getCurrentPollingPeriod());
}

bool ASIEAF::AbortFocuser()
{
    EAF_ERROR_CODE rc = EAFStop(m_ID);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to stop focuser. Error: %d", rc);
        return false;
    }
    return true;
}

//
// Temperature compensation
bool ASIEAF::saveConfigItems(FILE * fp)
{
    // Save Temperature compensation Config
    INDI::Focuser::saveConfigItems(fp);

    // numbers
    IUSaveConfigNumber(fp, &TempCNP[TEMPC_STEPS]);
    IUSaveConfigNumber(fp, &TempCNP[TEMPC_HYSTER]);
    IUSaveConfigNumber(fp, &TempCNP[TEMPC_SAMPLES]);

    // switch
    IUSaveConfigSwitch(fp, &TempCSP);

    return true;
}
//
