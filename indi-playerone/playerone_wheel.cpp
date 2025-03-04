/*
 PlayerOne Phenix Filter Wheel INDI Driver

 Copyright (C) 2023 Hiroshi Saito (hiro3110g@gmail.com)

 Code is based on ASI Filter Wheel INDI Driver by Rumen G.Bogdanovski
 Copyright (c) 2016 by Rumen G.Bogdanovski.
 All Rights Reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include "playerone_wheel.h"

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <deque>
#include <memory>

//#define SIMULATION
#define WAIT_AFTER_OPEN_DEVICE

static class Loader
{
        std::deque<std::unique_ptr<POAWHEEL>> wheels;
    public:
        Loader()
        {
#ifdef SIMULATION
            PWProperties info;
            info.Handle = 1;
            strncpy(info.Name, "Simulated EFW8", 64);
            info.slotNum = 0;
            wheels.push_back(std::unique_ptr<POAWHEEL>(new POAWHEEL(info, info.Name)));
#else
            int num_wheels = POAGetPWCount();

            if (num_wheels <= 0)
            {
                IDLog("No PlayerOne EFW detected.");
                return;
            }
            int num_wheels_ok = 0;
#ifndef WAIT_AFTER_OPEN_DEVICE
            int interval = 250;       // millisecond
            int elapsed_time = 0;
#endif // WAIT_AFTER_OPEN_DEVICE
            for (int i = 0; i < num_wheels; i++)
            {
                PWProperties info;
                PWErrors result = POAGetPWProperties(i, &info);
                if (result != PW_OK)
                {
                    IDLog("ERROR: PlayerOne EFW %d POAGetPWProperties error %d.", i + 1, result);
                    continue;
                }
                int id = info.Handle;
                result = POAGetPWPropertiesByHandle(id, &info);
                if (result != PW_OK /*&& result != EFW_ERROR_CLOSED*/)   // TODO: remove the ERROR_CLOSED hack
                {
                    IDLog("ERROR: PlayerOne EFW %d POAGetPWPropertiesByHandle error %d.", i + 1, result);
                    continue;
                }

#ifndef WAIT_AFTER_OPEN_DEVICE
                PWState state;
                result = POAGetPWState(id, &state);
                if (result != PW_OK)
                {
                    IDLog("ERROR: PlayerOne EFW %d POAGetPWState error %d.", i + 1, result);
                    return;
                }

                // Wait for initial moving in case of just after plugged-in the device
                while (state == PW_STATE_MOVING && elapsed_time < POA_EFW_TIMEOUT)
                {
                    usleep(interval * 1000);
                    result = POAGetPWState(id, &state);
                    if (result != PW_OK)
                    {
                        IDLog("ERROR: PlayerOne EFW %d POAGetPWState error %d.", i + 1, result);
                        return;
                    }
                    elapsed_time += interval;
                }

                if (state == PW_STATE_MOVING)
                {
                    IDLog("ERROR: PlayerOne EFW %d time out initial moving. state = %d.", i + 1, state);
                    return;
                }
#endif // WAIT_AFTER_OPEN_DEVICE

                std::string name = "PlayerOne " + std::string(info.Name);

                // If we only have a single device connected
                // then favor the INDIDEV driver label over the auto-generated name above
                if (num_wheels == 1)
                {
                    char *envDev = getenv("INDIDEV");
                    if (envDev && envDev[0])
                        name = envDev;
                }
                else
                    name += " " + std::to_string(i);
                wheels.push_back(std::unique_ptr<POAWHEEL>(new POAWHEEL(info, name.c_str())));
                num_wheels_ok++;
            }
            IDLog("%d PlayerOne EFW attached out of %d detected.", num_wheels_ok, num_wheels);
#endif
        }
} loader;

POAWHEEL::POAWHEEL(const PWProperties &info, const char *name)
{
    fw_id              = info.Handle;
    CurrentFilter      = 0;
    FilterSlotNP[0].setMin(0);
    FilterSlotNP[0].setMax(0);
    setDeviceName(name);
    setVersion(PLAYERONE_VERSION_MAJOR, PLAYERONE_VERSION_MINOR);
}

POAWHEEL::~POAWHEEL()
{
    Disconnect();
}

const char *POAWHEEL::getDefaultName()
{
    return "PlayerOne EFW";
}

bool POAWHEEL::Connect()
{
    if (isSimulation())
    {
        LOG_INFO("Simulation connected.");
        fw_id = 0;
        FilterSlotNP[0].setMin(1);
        FilterSlotNP[0].setMax(8);
    }
    else if (fw_id >= 0)
    {
        PWErrors result = POAOpenPW(fw_id);
        if (result != PW_OK)
        {
            LOGF_ERROR("%s(): POAOpenPW() = %d", __FUNCTION__, result);
            return false;
        }

#ifdef WAIT_AFTER_OPEN_DEVICE
        PWState state;
        result = POAGetPWState(fw_id, &state);
        if (result != PW_OK)
        {
            LOGF_ERROR("%s(): POAGetPWState() = %d", __FUNCTION__, result);
            return false;
        }

        // Wait for initial moving in case of just after plugged-in the device
        int interval = getCurrentPollingPeriod();       // millisecond
        int elapsed_time = 0;
        while (state != PW_STATE_OPENED && elapsed_time < POA_EFW_TIMEOUT)
        {
            usleep(interval * 1000);
            result = POAGetPWState(fw_id, &state);
            if (result != PW_OK)
            {
                LOGF_ERROR("%s(): POAGetPWState() = %d", __FUNCTION__, result);
                return false;
            }
            elapsed_time += interval;
        }

        if (state != PW_STATE_OPENED)
        {
            LOGF_ERROR("%s(): Can't open device. state = %d", __FUNCTION__, state);
            return false;
        }
#endif // WAIT_AFTER_OPEN_DEVICE

        PWProperties info;
        result = POAGetPWPropertiesByHandle(fw_id, &info);
        if (result != PW_OK)
        {
            LOGF_ERROR("%s(): POAGetPWPropertiesByHandle() = %d", __FUNCTION__, result);
            return false;
        }

        LOGF_INFO("Detected %d-position filter wheel.", info.PositionCount);

        FilterSlotNP[0].setMin(1);
        FilterSlotNP[0].setMax(info.PositionCount);

        // get current filter
        int current;
        result = POAGetCurrentPosition(fw_id, &current);
        if (result != PW_OK)
        {
            LOGF_ERROR("%s(): POAGetCurrentPosition() = %d", __FUNCTION__, result);
            return false;
        }

        SelectFilter(current + 1);
        LOGF_DEBUG("%s(): current filter position %d", __FUNCTION__, CurrentFilter);
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    return true;
}

bool POAWHEEL::Disconnect()
{
    if (isSimulation())
    {
        LOG_INFO("Simulation disconnected.");
    }
    else if (fw_id >= 0)
    {
        PWErrors result = POAClosePW(fw_id);
        if (result != PW_OK)
        {
            LOGF_ERROR("%s(): POAClosePW() = %d", __FUNCTION__, result);
            return false;
        }
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    // NOTE: do not unset fw_id here, otherwise we cannot reconnect without reloading the driver
    return true;
}

bool POAWHEEL::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Unidirectional motion
    IUFillSwitch(&UniDirectionalS[INDI_ENABLED], "INDI_ENABLED", "Enable", ISS_OFF);
    IUFillSwitch(&UniDirectionalS[INDI_DISABLED], "INDI_DISABLED", "Disable", ISS_ON);
    IUFillSwitchVector(&UniDirectionalSP, UniDirectionalS, 2, getDeviceName(), "FILTER_UNIDIRECTIONAL_MOTION", "Uni Direction",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&CalibrateS[0], "CALIBRATE", "Calibrate", ISS_OFF);
    IUFillSwitchVector(&CalibrateSP, CalibrateS, 1, getDeviceName(), "FILTER_CALIBRATION", "Calibrate",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    addAuxControls();
    setDefaultPollingPeriod(250);
    return true;
}

bool POAWHEEL::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        int isUniDirection = false;
        if (!isSimulation() && POAGetOneWay(fw_id, &isUniDirection) == PW_OK)
        {
            UniDirectionalS[INDI_ENABLED].s = (isUniDirection == 1 ? ISS_ON : ISS_OFF);
            UniDirectionalS[INDI_DISABLED].s = (isUniDirection == 1 ? ISS_OFF : ISS_ON);
        }
        defineProperty(&UniDirectionalSP);
        defineProperty(&CalibrateSP);
    }
    else
    {
        deleteProperty(UniDirectionalSP.name);
        deleteProperty(CalibrateSP.name);
    }

    return true;
}

bool POAWHEEL::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, UniDirectionalSP.name))
        {
            PWErrors rc = POASetOneWay(fw_id, !strcmp(IUFindOnSwitchName(states, names, n),
                                       UniDirectionalS[INDI_ENABLED].name));
            if (rc == PW_OK)
            {
                IUUpdateSwitch(&UniDirectionalSP, states, names, n);
                UniDirectionalSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("%s(): POASetOneWay = %d", __FUNCTION__, rc);
                UniDirectionalSP.s = IPS_ALERT;
            }
            IDSetSwitch(&UniDirectionalSP, nullptr);
            return true;
        }
        if (!strcmp(name, CalibrateSP.name))
        {
            CalibrateS[0].s = ISS_OFF;

            if (isSimulation())
            {
                return true;
            }

            CalibrateSP.s   = IPS_BUSY;
            IDSetSwitch(&CalibrateSP, nullptr);

            // make the set filter number busy
            FilterSlotNP.setState(IPS_BUSY);
            FilterSlotNP.apply();

            LOGF_DEBUG("Calibrating EFW %d", fw_id);
            PWErrors rc = POAResetPW(fw_id);

            if (rc == PW_OK)
            {
                IEAddTimer(getCurrentPollingPeriod(), POAWHEEL::TimerHelperCalibrate, this);
                return true;
            }
            else
            {
                LOGF_ERROR("%(): POAResetPW = %d", __FUNCTION__, rc);
                CalibrateSP.s = IPS_ALERT;
                IDSetSwitch(&CalibrateSP, nullptr);

                // reset filter slot state
                FilterSlotNP.setState(IPS_OK);
                FilterSlotNP.apply();
                return false;
            }
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

int POAWHEEL::QueryFilter()
{
    if (isSimulation())
        return CurrentFilter;

    if (fw_id >= 0)
    {
        PWErrors result;
        result = POAGetCurrentPosition(fw_id, &CurrentFilter);
        if (result != PW_OK)
        {
            LOGF_ERROR("%s(): POAGetCurrentPosition() = %d", __FUNCTION__, result);
            return 0;
        }
        CurrentFilter++;
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return 0;
    }

    return CurrentFilter;
}

bool POAWHEEL::SelectFilter(int f)
{
    TargetFilter = f;
    if (isSimulation())
    {
        CurrentFilter = TargetFilter;
        return true;
    }

    if (fw_id >= 0)
    {
        PWErrors result;
        result = POAGotoPosition(fw_id, f - 1);
        if (result == PW_OK)
        {
            int interval = getCurrentPollingPeriod();	// millisecond
            SetTimer(interval);
            int elapsed_time = 0;
            do
            {
                usleep(interval * 1000);
                result = POAGetCurrentPosition(fw_id, &CurrentFilter);
                elapsed_time += interval;
            }
            while (result != PW_OK && elapsed_time < POA_EFW_TIMEOUT);

            if (result != PW_OK)
            {
                LOGF_ERROR("%s(): POAGotoPosition() = %d", __FUNCTION__, result);
                return false;
            }
            CurrentFilter++;
            if (CurrentFilter != TargetFilter)
            {
                LOGF_ERROR("%s(): POAGotoPosition() CurrentFilter = %d is not TargetFilter", __FUNCTION__, CurrentFilter);
                return false;
            }
        }
        else
        {
            LOGF_ERROR("%s(): POAGotoPosition() = %d", __FUNCTION__, result);
            return false;
        }
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    return true;
}

void POAWHEEL::TimerHit()
{
    QueryFilter();

    if (CurrentFilter != TargetFilter)
    {
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        SelectFilterDone(CurrentFilter);
    }
}

bool POAWHEEL::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &UniDirectionalSP);
    return true;
}

void POAWHEEL::TimerHelperCalibrate(void *context)
{
    static_cast<POAWHEEL*>(context)->TimerCalibrate();
}

void POAWHEEL::TimerCalibrate()
{
    // check current state of calibration
    PWState state;
    PWErrors rc = POAGetPWState(fw_id, &state);

    if (rc == PW_OK)
    {
        if (state == PW_STATE_MOVING)
        {
            // while filterwheel is moving we're still calibrating
            IEAddTimer(getCurrentPollingPeriod(), POAWHEEL::TimerHelperCalibrate, this);
            return;
        }
        LOGF_DEBUG("Successfully calibrated EFW %d", fw_id);
        CalibrateSP.s   = IPS_OK;
        IDSetSwitch(&CalibrateSP, nullptr);
    }
    else
    {
        LOGF_ERROR("%(): EFWCalibrate = %d", __FUNCTION__, rc);
        CalibrateSP.s = IPS_ALERT;
        IDSetSwitch(&CalibrateSP, nullptr);
    }

    FilterSlotNP.setState(IPS_OK);
    FilterSlotNP.apply();
    return;
}
