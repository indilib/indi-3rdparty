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
#include "playerone_wheel_hotplug_handler.h"
#include "config.h"

#include <hotplugmanager.h>
#include <cstring>
#include <memory>

namespace
{
constexpr int STARTUP_WAIT_MS = 20000;
constexpr int MOVE_TIMEOUT_MS = 60000;
constexpr int CALIBRATION_TIMEOUT_MS = 300000;

#ifndef PLAYERONE_WHEEL_DISABLE_LOADER
static class Loader
{
        std::shared_ptr<PlayerOneWheelHotPlugHandler> mHandler;
    public:
        Loader()
        {
            mHandler = std::make_shared<PlayerOneWheelHotPlugHandler>();
            auto &manager = INDI::HotPlugManager::getInstance();
            manager.registerHandler(mHandler);
            manager.setNonUdevPollingDuration(0);
            manager.start(1000);
        }
} loader;
#endif
}

POAWHEEL::POAWHEEL(const PWProperties &info, const char *name)
    : fw_id(info.Handle), mSerialNumber(info.SN, strnlen(info.SN, sizeof(info.SN)))
{
    CurrentFilter = 0;
    FilterSlotNP[0].setMin(0);
    FilterSlotNP[0].setMax(0);
    if (mSerialNumber.empty() || !setDeviceNicknameFromId(mSerialNumber.c_str()))
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
        FilterSlotNP[0].setMin(1);
        FilterSlotNP[0].setMax(8);
        CurrentFilter = TargetFilter = 1;
        mPhase = Phase::Ready;
        return true;
    }

    if (fw_id < 0)
    {
        LOG_ERROR("No filter wheel handle available.");
        return false;
    }
    if (mOpen)
    {
        LOG_ERROR("The filter wheel is still open after a failed close.");
        return false;
    }

    PWErrors result = POAOpenPW(fw_id);
    if (result != PW_OK)
    {
        LOGF_ERROR("POAOpenPW failed: %d", result);
        return false;
    }

    mOpen = true;
    mPhase = Phase::Starting;
    mStartupWaitMs = 0;
    mScheduledWaitMs = getCurrentPollingPeriod();
    SetTimer(mScheduledWaitMs);
    LOG_INFO("Waiting for the filter wheel's initial rotation.");
    return true;
}

bool POAWHEEL::Disconnect()
{
    RemoveTimer(0);
    mPhase = Phase::Idle;
    CalibrateSP.s = IPS_IDLE;
    FilterSlotNP.setState(IPS_IDLE);
    if (!mOpen)
        return true;

    PWErrors result = POAClosePW(fw_id);
    if (result != PW_OK)
    {
        LOGF_ERROR("POAClosePW failed: %d", result);
        return false;
    }
    mOpen = false;
    return true;
}

void POAWHEEL::unplug()
{
    Disconnect();
}

bool POAWHEEL::initProperties()
{
    INDI::FilterWheel::initProperties();

    IUFillSwitch(&UniDirectionalS[INDI_ENABLED], "INDI_ENABLED", "Enable", ISS_OFF);
    IUFillSwitch(&UniDirectionalS[INDI_DISABLED], "INDI_DISABLED", "Disable", ISS_ON);
    IUFillSwitchVector(&UniDirectionalSP, UniDirectionalS, 2, getDeviceName(), "FILTER_UNIDIRECTIONAL_MOTION", "Uni Direction",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&CalibrateS[0], "CALIBRATE", "Calibrate", ISS_OFF);
    IUFillSwitchVector(&CalibrateSP, CalibrateS, 1, getDeviceName(), "FILTER_CALIBRATION", "Calibrate",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    SerialNumberTP[0].fill("SN", "SN", mSerialNumber);
    SerialNumberTP.fill(getDeviceName(), "Serial Number", "Serial Number", INFO_TAB, IP_RO, 60, IPS_IDLE);
    addAuxControls();
    addNicknameControl();
    setDefaultPollingPeriod(250);
    return true;
}

bool POAWHEEL::updateProperties()
{
    if (mPhase == Phase::Starting)
    {
        // DefaultDevice treats BUSY as disconnected, so defer filter properties.
        INDI::DefaultDevice::updateProperties();
        setConnected(true, IPS_BUSY);
        return true;
    }

    INDI::FilterWheel::updateProperties();
    if (isConnected())
    {
        int isUniDirection = 0;
        if (!isSimulation())
        {
            PWErrors result = POAGetOneWay(fw_id, &isUniDirection);
            if (result != PW_OK)
                LOGF_WARN("POAGetOneWay failed: %d", result);
        }
        UniDirectionalS[INDI_ENABLED].s = isUniDirection ? ISS_ON : ISS_OFF;
        UniDirectionalS[INDI_DISABLED].s = isUniDirection ? ISS_OFF : ISS_ON;
        defineProperty(&UniDirectionalSP);
        defineProperty(&CalibrateSP);
        if (!mSerialNumber.empty())
            defineProperty(SerialNumberTP);
    }
    else
    {
        deleteProperty(UniDirectionalSP.name);
        deleteProperty(CalibrateSP.name);
        if (!mSerialNumber.empty())
            deleteProperty(SerialNumberTP);
    }
    return true;
}

bool POAWHEEL::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if ((mPhase == Phase::Starting || (mOpen && !isConnected())) && !strcmp(name, "CONNECTION"))
        {
            for (int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], "DISCONNECT") && states[i] == ISS_ON)
                {
                    if (!Disconnect())
                    {
                        setConnected(true, IPS_ALERT);
                        return true;
                    }
                    setConnected(false, IPS_IDLE);
                    return true;
                }
                if (mPhase == Phase::Starting && !strcmp(names[i], "CONNECT") && states[i] == ISS_ON)
                {
                    setConnected(true, IPS_BUSY);
                    return true;
                }
            }
        }
        if (!strcmp(name, UniDirectionalSP.name))
        {
            const char *selected = IUFindOnSwitchName(states, names, n);
            if (!selected)
            {
                LOG_ERROR("No direction selected.");
                UniDirectionalSP.s = IPS_ALERT;
            }
            else
            {
                PWErrors result = isSimulation() ? PW_OK : POASetOneWay(fw_id, !strcmp(selected, UniDirectionalS[INDI_ENABLED].name));
                if (result == PW_OK)
                {
                    IUUpdateSwitch(&UniDirectionalSP, states, names, n);
                    UniDirectionalSP.s = IPS_OK;
                }
                else
                {
                    LOGF_ERROR("POASetOneWay failed: %d", result);
                    UniDirectionalSP.s = IPS_ALERT;
                }
            }
            IDSetSwitch(&UniDirectionalSP, nullptr);
            return true;
        }
        if (!strcmp(name, CalibrateSP.name))
        {
            CalibrateS[0].s = ISS_OFF;
            if (mPhase != Phase::Ready)
            {
                LOG_ERROR("Cannot calibrate while the wheel is busy.");
                CalibrateSP.s = IPS_ALERT;
                IDSetSwitch(&CalibrateSP, nullptr);
                return true;
            }
            PWErrors result = isSimulation() ? PW_OK : POAResetPW(fw_id);
            if (result != PW_OK)
            {
                LOGF_ERROR("POAResetPW failed: %d", result);
                CalibrateSP.s = IPS_ALERT;
                IDSetSwitch(&CalibrateSP, nullptr);
                return true;
            }
            CalibrateSP.s = IPS_BUSY;
            IDSetSwitch(&CalibrateSP, nullptr);
            FilterSlotNP.setState(IPS_BUSY);
            FilterSlotNP.apply();
            mPhase = Phase::Calibrating;
            mOperationTimer.start();
            SetTimer(getCurrentPollingPeriod());
            return true;
        }
    }
    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool POAWHEEL::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, "FILTER_SLOT") && mPhase != Phase::Ready)
    {
        LOG_ERROR("Cannot select a filter while the wheel is busy.");
        FilterSlotNP.apply();
        return true;
    }
    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

bool POAWHEEL::readPosition(int &position)
{
    PWErrors result = POAGetCurrentPosition(fw_id, &position);
    if (result != PW_OK)
    {
        if (result != PW_ERROR_IS_MOVING)
            LOGF_ERROR("POAGetCurrentPosition failed: %d", result);
        return false;
    }
    ++position;
    if (position < 1 || position > FilterSlotNP[0].getMax())
    {
        LOGF_ERROR("Invalid filter position: %d", position);
        return false;
    }
    return true;
}

int POAWHEEL::QueryFilter()
{
    if (isSimulation())
        return CurrentFilter;
    int position = 0;
    if (!readPosition(position))
        return 0;
    CurrentFilter = position;
    return CurrentFilter;
}

bool POAWHEEL::SelectFilter(int f)
{
    if (mPhase != Phase::Ready)
    {
        LOG_ERROR("Cannot select a filter while the wheel is busy.");
        return false;
    }
    PWErrors result = isSimulation() ? PW_OK : POAGotoPosition(fw_id, f - 1);
    if (result != PW_OK)
    {
        LOGF_ERROR("POAGotoPosition failed: %d", result);
        return false;
    }
    TargetFilter = f;
    if (isSimulation())
        CurrentFilter = f;
    mPhase = Phase::Moving;
    mOperationTimer.start();
    SetTimer(getCurrentPollingPeriod());
    return true;
}

void POAWHEEL::failOperation(const char *message)
{
    LOG_ERROR(message);
    if (mPhase == Phase::Starting)
    {
        Disconnect();
        setConnected(false, IPS_ALERT);
        return;
    }
    if (mPhase == Phase::Calibrating)
    {
        CalibrateSP.s = IPS_ALERT;
        IDSetSwitch(&CalibrateSP, nullptr);
    }
    mPhase = Phase::Ready;
    FilterSlotNP.setState(IPS_ALERT);
    FilterSlotNP.apply();
}

void POAWHEEL::TimerHit()
{
    if (mPhase == Phase::Idle || mPhase == Phase::Ready)
        return;

    if (mPhase == Phase::Starting)
    {
        mStartupWaitMs += mScheduledWaitMs;
        PWState state;
        PWErrors result = POAGetPWState(fw_id, &state);
        if (result != PW_OK)
        {
            LOGF_ERROR("POAGetPWState failed: %d", result);
            failOperation("Failed to initialize the filter wheel.");
            return;
        }
        if (state == PW_STATE_OPENED)
        {
            PWProperties info {};
            result = POAGetPWPropertiesByHandle(fw_id, &info);
            if (result != PW_OK || info.PositionCount <= 0)
            {
                LOGF_ERROR("Invalid wheel properties (result %d, slots %d).", result, info.PositionCount);
                failOperation("Failed to initialize the filter wheel.");
                return;
            }
            FilterSlotNP[0].setMin(1);
            FilterSlotNP[0].setMax(info.PositionCount);
            int position;
            result = POAGetCurrentPosition(fw_id, &position);
            if (result == PW_OK && position >= 0 && position < info.PositionCount)
            {
                CurrentFilter = TargetFilter = position + 1;
                FilterSlotNP[0].setValue(CurrentFilter);
                FilterSlotNP.setState(IPS_OK);
                mPhase = Phase::Ready;
                setConnected(true, IPS_OK);
                updateProperties();
                LOGF_INFO("Detected %d-position filter wheel at slot %d.", info.PositionCount, CurrentFilter);
                return;
            }
            if (result != PW_ERROR_IS_MOVING)
            {
                LOGF_ERROR("Invalid initial position (result %d, position %d).", result, result == PW_OK ? position : -1);
                failOperation("Failed to initialize the filter wheel.");
                return;
            }
        }
        if (mStartupWaitMs >= STARTUP_WAIT_MS)
        {
            failOperation("Timed out waiting for the filter wheel's initial rotation.");
            return;
        }
        mScheduledWaitMs = getCurrentPollingPeriod();
        SetTimer(mScheduledWaitMs);
        return;
    }

    if (!isConnected())
        return;

    const int timeout = mPhase == Phase::Calibrating ? CALIBRATION_TIMEOUT_MS : MOVE_TIMEOUT_MS;
    if (mOperationTimer.elapsed() >= timeout)
    {
        failOperation(mPhase == Phase::Calibrating ? "Filter wheel calibration timed out." : "Filter wheel movement timed out.");
        return;
    }

    int position = CurrentFilter;
    if (!isSimulation())
    {
        PWErrors result = POAGetCurrentPosition(fw_id, &position);
        if (result == PW_ERROR_IS_MOVING)
        {
            SetTimer(getCurrentPollingPeriod());
            return;
        }
        if (result != PW_OK)
        {
            LOGF_ERROR("POAGetCurrentPosition failed: %d", result);
            failOperation("Failed to read the filter wheel position.");
            return;
        }
        ++position;
    }
    if (position < 1 || position > FilterSlotNP[0].getMax())
    {
        failOperation("Filter wheel reported an invalid position.");
        return;
    }
    if (mPhase == Phase::Moving && position != TargetFilter)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }
    if (mPhase == Phase::Calibrating)
    {
        CalibrateSP.s = IPS_OK;
        IDSetSwitch(&CalibrateSP, nullptr);
    }
    CurrentFilter = TargetFilter = position;
    mPhase = Phase::Ready;
    SelectFilterDone(position);
}

bool POAWHEEL::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &UniDirectionalSP);
    return true;
}

void POAWHEEL::nicknameSet(const char *nickname)
{
    if (!mSerialNumber.empty())
        saveNicknameId(nickname, mSerialNumber.c_str());
}
