/*
    ZWO EAF Focuser
    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2023 Jarno Paananen (jarno.paananen@gmail.com)

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
                IDLog("No ZWO EAF detected.");
                return;
            }

            int iAvailableFocusersCount_ok = 0;
            char *envDev = getenv("INDIDEV");
            for (int i = 0; i < iAvailableFocusersCount; i++)
            {
                int id;
                EAF_ERROR_CODE result = EAFGetID(i, &id);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ZWO EAF %d EAFGetID error %d.", i + 1, result);
                    continue;
                }

                // Open device
                result = EAFOpen(id);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ZWO EAF %d Failed to open device %d.", i + 1, result);
                    continue;
                }

                EAF_INFO info;
                result = EAFGetProperty(id, &info);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ZWO EAF %d EAFGetProperty error %d.", i + 1, result);
                    continue;
                }
                EAFClose(id);

                std::string name = "ZWO EAF";
                if (envDev && envDev[0])
                    name = envDev;

                // If we only have a single device connected
                // then favor the INDIDEV driver label over the auto-generated name above
                if (iAvailableFocusersCount > 1)
                    name += " " + std::to_string(i + 1);

                focusers.push_back(std::unique_ptr<ASIEAF>(new ASIEAF(info, name.c_str())));
                iAvailableFocusersCount_ok++;
            }
            IDLog("%d ZWO EAF attached out of %d detected.", iAvailableFocusersCount_ok, iAvailableFocusersCount);
        }
} loader;

ASIEAF::ASIEAF(const EAF_INFO &info, const char *name)
    : m_ID(info.ID)
    , m_MaxSteps(info.MaxStep)
{
    setVersion(1, 2);

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

    FocusAbsPosNP[0].setMax(m_MaxSteps);
}

bool ASIEAF::initProperties()
{
    INDI::Focuser::initProperties();

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

    //
    // Temperature compensation
    //
    // Switch : enable or disable temperature compensation
    TempCSP[TEMPC_ON].fill("ON", "On", ISS_OFF);
    TempCSP[TEMPC_OFF].fill("OFF", "Off", ISS_ON);
    TempCSP.fill(getDeviceName(), "TEMPC_SWITCH", "Temperature Compensation", TEMPC_TAB, IP_RW,
                 ISR_1OFMANY, 0, IPS_IDLE);

    // Numbers :
    //
    // STEPS : number of steps to move (inward ou outward) per C degree variation (usually negative)
    // HYSTERESIS : minimal temperature variation before triggering moves
    // SAMPLES : for better and consistents readings, sum up the temperature samples
    TempCNP[TEMPC_STEPS].fill("STEPS", "Steps per Celsius", "%.f", -200, 200, 1, 0);
    TempCNP[TEMPC_HYSTER].fill("HYSTERESIS", "Delta in Celsius", "%.1f", 0, 10, 0.1, 1.0);
    TempCNP[TEMPC_SAMPLES].fill("SAMPLES", "Number of samples", "%.f", 1, 120, 1, 5);
    TempCNP[TEMPC_MEAN].fill("MEAN", "Celsius", "%.2f", -274, 100, 0.1, 0);
    TempCNP.fill(getDeviceName(), "TEMP_COMPENSATION", "Temperature compensation", TEMPC_TAB,
                 IP_RW, 0, IPS_IDLE);
    FocusBacklashNP[0].setMin(0);
    FocusBacklashNP[0].setMax(9999);
    FocusBacklashNP[0].setStep(100);
    FocusBacklashNP[0].setValue(0);

    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(m_MaxSteps / 2.0);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(FocusRelPosNP[0].getMax() / 20);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(m_MaxSteps);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(m_MaxSteps / 20.0);

    PresetNP[0].setMax(m_MaxSteps);
    PresetNP[0].setStep(m_MaxSteps / 20.0);
    PresetNP[1].setMax(m_MaxSteps);
    PresetNP[1].setStep(m_MaxSteps / 20.0);
    PresetNP[2].setMax(m_MaxSteps);
    PresetNP[2].setStep(m_MaxSteps / 20.0);

    setDefaultPollingPeriod(500);

    addDebugControl();

    return true;
}

bool ASIEAF::updateProperties()
{
    if (isConnected())
    {
        float temperature = -273;
        EAFGetTemp(m_ID, &temperature);

        if (temperature != -273)
        {
            TemperatureNP[0].setValue(temperature);
            TemperatureNP.setState(IPS_OK);
            defineProperty(TemperatureNP);
        }

        defineProperty(BeepSP);

        char firmware[12];
        unsigned char major, minor, build;
        EAFGetFirmwareVersion(m_ID, &major, &minor, &build);
        snprintf(firmware, sizeof(firmware), "%d.%d.%d", major, minor, build);
        VersionInfoSP[0].setText(firmware);
        VersionInfoSP[1].setText(EAFGetSDKVersion());
        defineProperty(VersionInfoSP);

        //
        // Temperature compensation
        defineProperty(TempCSP);
        defineProperty(TempCNP);
        //

        GetFocusParams();

        LOG_INFO("ZWO EAF parameters updated, focuser ready for use.");

        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        if (TemperatureNP.getState() != IPS_IDLE)
            deleteProperty(TemperatureNP);
        deleteProperty(BeepSP);
        deleteProperty(VersionInfoSP);

        //
        // Temperature compensation
        deleteProperty(TempCSP);
        deleteProperty(TempCNP);
        //
    }

    return INDI::Focuser::updateProperties();
}

const char * ASIEAF::getDefaultName()
{
    return "ZWO EAF";
}

bool ASIEAF::Connect()
{
    EAF_ERROR_CODE rc = EAFOpen(m_ID);

    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to connect to ZWO EAF focuser ID: %d (%d)", m_ID, rc);
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

    TemperatureNP[0].setValue(temperature);
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
    FocusAbsPosNP[0].setValue(step);
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
    FocusAbsPosNP[0].setMax(max);

    int stepRange;
    rc = EAFStepRange(m_ID, &stepRange);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read max step range. Error: %d", rc);
        return false;
    }
    FocusMaxPosNP[0].setMax(stepRange);

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

    FocusReverseSP[INDI_ENABLED].setState(reversed ? ISS_ON : ISS_OFF);
    FocusReverseSP[INDI_DISABLED].setState(reversed ? ISS_OFF : ISS_ON);
    FocusReverseSP.setState(IPS_OK);
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
    FocusBacklashNP[0].setValue(backv);
    FocusBacklashNP.setState(IPS_OK);
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

    BeepSP[INDI_ENABLED].setState(beep ? ISS_ON : ISS_OFF);
    BeepSP[INDI_DISABLED].setState(beep ? ISS_OFF : ISS_ON);
    BeepSP.setState(IPS_OK);

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

    //
    // Temperature compensation
    if (TempCSP.isNameMatch(name))
    {
        TempCSP.update(states, names, n);
        if (TempCSP.findOnSwitchIndex() == TEMPC_ON)
        {
            TempCEnabled = true;
            LOG_INFO("Temperature compensation enabled");
        }
        else
        {
            TempCEnabled = false;
            // reset temp control
            TempCTotalTemp = 0;
            TempCCounter = 0;
            LOG_INFO("Temperature compensation disabled");
        }
        TempCSP.setState(IPS_OK);
        TempCSP.apply();
    }
    //

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool ASIEAF::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()) != 0)
    {
        return false;
    }

    //
    // Temperature compensastion
    if (TempCNP.isNameMatch(name))
    {
        TempCNP.update(values, names, n);
        TempCSteps = TempCNP[TEMPC_STEPS].getValue();
        TempCHyster = TempCNP[TEMPC_HYSTER].getValue();
        TempCSamples = TempCNP[TEMPC_SAMPLES].getValue();
        TempCNP.setState(IPS_OK);
        TempCNP.apply();
        LOGF_INFO("Step/C set to : %d", TempCSteps);
        LOGF_INFO("Hysteresis set to : %f", TempCHyster);
        LOGF_INFO("Samples set to : %d", TempCSamples);
        // reset
        TempCTotalTemp = 0;
        TempCCounter = 0;
        TempCLastTemp = -274; // zero Kelvin
        return true;
    }
    //

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void ASIEAF::GetFocusParams()
{
    if (readPosition())
        FocusAbsPosNP.apply();

    if (readReverse())
        FocusReverseSP.apply();

    if (readBeep())
        BeepSP.apply();

    if (readBacklash())
        FocusBacklashNP.apply();
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
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()), newPosition));
    if (!gotoAbsolute(newPosition))
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

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
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
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

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
            LOG_INFO("Focuser reached requested position.");
        }
    }

    //
    // Temperature compensation
    if(TempCEnabled)
    {
        // stack the samples values
        if(TempCCounter < TempCSamples)
        {
            TempCCounter++;
            TempCTotalTemp += TemperatureNP[0].getValue();
        }
        else
        {
            // get the new mean temp value
            double meanTemp = TempCTotalTemp / (double)TempCSamples;
            // and publish
            TempCNP[TEMPC_MEAN].setValue(meanTemp);
            TempCNP.setState(IPS_OK);
            TempCNP.apply();

            // wait for 2 measures (last and current)
            if(TempCLastTemp == -274.0 /* zero Kelvin : no first value */)
            {
                TempCLastTemp = meanTemp;
            }
            else
            {
                // if temp delta is over hysteris value
                if(abs(meanTemp - TempCLastTemp) > TempCHyster)
                {
                    // adjust position
                    int steps = (int)((meanTemp - TempCLastTemp) * (double)TempCSteps);
                    LOGF_INFO("Last Temp. : %.2f New Temp. : %.2f Delta : %.2f, Moving %d steps", TempCLastTemp, meanTemp,
                              meanTemp - TempCLastTemp, steps);
                    FocusDirection dir = steps < 0 ? FOCUS_INWARD : FOCUS_OUTWARD ;
                    MoveRelFocuser(dir, abs(steps));
                    // store last measure
                    TempCLastTemp = meanTemp;
                }
            }

            // reset
            TempCTotalTemp = 0;
            TempCCounter = 0;
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
    TempCNP.save(fp);

    // switch
    TempCSP.save(fp);

    return true;
}
//
