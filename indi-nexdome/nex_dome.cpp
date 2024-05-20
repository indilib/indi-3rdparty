/*******************************************************************************
 NexDome

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
#include "nex_dome.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <memory>
#include <regex>
#include <termios.h>

#include <indicom.h>
#include <cmath>

#include "config.h"

static std::unique_ptr<NexDome> nexDome(new NexDome());

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
NexDome::NexDome()
{
    setVersion(INDI_NEXDOME_VERSION_MAJOR, INDI_NEXDOME_VERSION_MINOR);

    SetDomeCapability(DOME_CAN_ABORT |
                      DOME_CAN_ABS_MOVE |
                      DOME_CAN_PARK |
                      DOME_CAN_SYNC);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_AZ);

    ///////////////////////////////////////////////////////////////////////////////
    /// Homeing
    ///////////////////////////////////////////////////////////////////////////////
    GoHomeSP[HOME_FIND].fill("HOME_FIND", "Find", ISS_OFF);
    GoHomeSP[HOME_GOTO].fill("HOME_GOTO", "Go", ISS_OFF);
    GoHomeSP.fill(getDeviceName(), "DOME_HOMING", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Home Position
    ///////////////////////////////////////////////////////////////////////////////
    HomePositionNP[0].fill("POSITON", "degrees", "%.2f", 0.0, 360.0, 0.0, 0);
    HomePositionNP.fill(getDeviceName(), "HOME_POSITION", "Home Az", MAIN_CONTROL_TAB, IP_RW,
                       60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Battery
    ///////////////////////////////////////////////////////////////////////////////
    ShutterBatteryLevelNP[ND::ROTATOR].fill("VOLTS", "Voltage", "%.2f", 0.0, 16.0, 0.0, 0);
    ShutterBatteryLevelNP.fill(getDeviceName(), "BATTERY", "Battery Level",
                       ND::SHUTTER_TAB.c_str(), IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Rotator Firmware Info
    ///////////////////////////////////////////////////////////////////////////////
    RotatorFirmwareVersionTP[0].fill("FIRMWARE_VERSION", "Version", "");
    RotatorFirmwareVersionTP.fill(getDeviceName(), "FIRMWARE", "Firmware",
                     ND::ROTATOR_TAB.c_str(), IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Shutter Firmware Info
    ///////////////////////////////////////////////////////////////////////////////
    ShutterFirmwareVersionTP[0].fill("FIRMWARE_VERSION", "Version", "");
    ShutterFirmwareVersionTP.fill(getDeviceName(), "FIRMWARE", "Firmware",
                     ND::SHUTTER_TAB.c_str(), IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Rotator Settings
    ///////////////////////////////////////////////////////////////////////////////
    RotatorSettingsNP[S_RAMP].fill("S_RAMP", "Acceleration Ramp (ms)", "%.f", 0.0, 5000, 1000.0, 0);
    RotatorSettingsNP[S_VELOCITY].fill("S_VELOCITY", "Velocity (steps/s)", "%.f", 0.0, 5000, 1000.0, 0);
    RotatorSettingsNP[S_ZONE].fill("S_ZONE", "Dead Zone (steps)", "%.f", 0.0, 32000, 1000.0, 2400);
    RotatorSettingsNP[S_RANGE].fill("S_RANGE", "Travel Range (steps)", "%.f", 0.0, 55080, 1000.0, 55080);
    RotatorSettingsNP.fill(getDeviceName(), "ROTATOR_SETTINGS", "Rotator",
                       ND::ROTATOR_TAB.c_str(), IP_RW, 60, IPS_IDLE);


    ///////////////////////////////////////////////////////////////////////////////
    /// Rotator Sync
    ///////////////////////////////////////////////////////////////////////////////
    RotatorSyncNP[0].fill("ROTATOR_SYNC_VALUE", "Steps", "%.f", 0.0, 55080, 1000.0, 0);
    RotatorSyncNP.fill(getDeviceName(), "ROTATOR_SYNC", "Sync",
                       ND::ROTATOR_TAB.c_str(), IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Shutter Settings
    ///////////////////////////////////////////////////////////////////////////////
    ShutterSettingsNP[S_RAMP].fill("S_RAMP", "Acceleration Ramp (ms)", "%.f", 0.0, 5000, 1000.0, 0);
    ShutterSettingsNP[S_VELOCITY].fill("S_VELOCITY", "Velocity (step/s)", "%.f", 0.0, 5000, 1000.0, 0);
    ShutterSettingsNP.fill(getDeviceName(), "SHUTTER_SETTINGS", "Shutter",
                       ND::SHUTTER_TAB.c_str(), IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Shutter Sync
    ///////////////////////////////////////////////////////////////////////////////
    ShutterSyncNP[0].fill("SHUTTER_SYNC_VALUE", "Steps", "%.f", 0.0, 46000, 1000.0, 0);
    ShutterSyncNP.fill(getDeviceName(), "SHUTTER_SYNC", "Sync",
                       ND::SHUTTER_TAB.c_str(), IP_RW, 60, IPS_IDLE);
    ///////////////////////////////////////////////////////////////////////////////
    /// Rotator Factory Settings
    ///////////////////////////////////////////////////////////////////////////////
    RotatorFactorySP[FACTORY_DEFAULTS].fill("FACTORY_DEFAULTS", "Defaults", ISS_OFF);
    RotatorFactorySP[FACTORY_LOAD].fill("FACTORY_LOAD", "Load", ISS_OFF);
    RotatorFactorySP[FACTORY_SAVE].fill("FACTORY_SAVE", "Save", ISS_OFF);
    RotatorFactorySP.fill(getDeviceName(), "ROTATOR_FACTORY_SETTINGS", "Factory",
                       ND::ROTATOR_TAB.c_str(), IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Shutter Factory Settings
    ///////////////////////////////////////////////////////////////////////////////
    ShutterFactorySP[FACTORY_DEFAULTS].fill("FACTORY_DEFAULTS", "Defaults", ISS_OFF);
    ShutterFactorySP[FACTORY_LOAD].fill("FACTORY_LOAD", "Load", ISS_OFF);
    ShutterFactorySP[FACTORY_SAVE].fill("FACTORY_SAVE", "Save", ISS_OFF);
    ShutterFactorySP.fill(getDeviceName(), "SHUTTER_FACTORY_SETTINGS", "Factory",
                       ND::SHUTTER_TAB.c_str(), IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Check every 250ms.
    setDefaultPollingPeriod(250);

    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::Handshake()
{
    std::string value;
    bool rotatorOK = false;

    if (getParameter(ND::SEMANTIC_VERSION, ND::ROTATOR, value))
    {
        LOGF_INFO("Detected rotator firmware version %s", value.c_str());
        if (value < ND::MINIMUM_VERSION)
        {
            LOGF_ERROR("Rotator version %s is not supported. Please upgrade to version %s or higher.", value.c_str(),
                       ND::MINIMUM_VERSION.c_str());
            return false;
        }

        rotatorOK = true;
        RotatorFirmwareVersionTP.setState(IPS_OK);
        RotatorFirmwareVersionTP[0].setText(value.c_str());
    }

    if (getParameter(ND::SEMANTIC_VERSION, ND::SHUTTER, value))
    {
        LOGF_INFO("Detected shutter firmware version %s", value.c_str());
        if (value < ND::MINIMUM_VERSION)
        {
            LOGF_ERROR("Shutter version %s is not supported. Please upgrade to version %s or higher.", value.c_str(),
                       ND::MINIMUM_VERSION.c_str());
            return false;
        }

        SetDomeCapability(GetDomeCapability() | DOME_HAS_SHUTTER);

        ShutterFirmwareVersionTP.setState(IPS_OK);
        ShutterFirmwareVersionTP[0].setText(value.c_str());
    }
    else
        LOG_WARN("No shutter detected.");

    return rotatorOK;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
const char * NexDome::getDefaultName()
{
    return "NexDome";
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        getStartupValues();

        defineProperty(GoHomeSP);
        defineProperty(HomePositionNP);

        // Rotator
        defineProperty(RotatorSettingsNP);
        defineProperty(RotatorSyncNP);
        defineProperty(RotatorFactorySP);
        defineProperty(RotatorFirmwareVersionTP);

        // Shutter
        if (HasShutter())
        {
            defineProperty(ShutterSettingsNP);
            defineProperty(ShutterSyncNP);
            defineProperty(ShutterBatteryLevelNP);
            defineProperty(ShutterFactorySP);
            defineProperty(ShutterFirmwareVersionTP);
        }
    }
    else
    {
        deleteProperty(GoHomeSP);
        deleteProperty(HomePositionNP);

        // Rotator
        deleteProperty(RotatorSettingsNP);
        deleteProperty(RotatorSyncNP);
        deleteProperty(RotatorFactorySP);
        deleteProperty(RotatorFirmwareVersionTP);

        // Shutter
        if (HasShutter())
        {
            deleteProperty(ShutterSettingsNP);
            deleteProperty(RotatorSyncNP);
            deleteProperty(ShutterBatteryLevelNP);
            deleteProperty(ShutterFactorySP);
            deleteProperty(ShutterFirmwareVersionTP);
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
/// Switch handling
//////////////////////////////////////////////////////////////////////////////
bool NexDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        ///////////////////////////////////////////////////////////////////////////////
        /// Home Command
        ///////////////////////////////////////////////////////////////////////////////
        if(GoHomeSP.isNameMatch(name))
        {
            GoHomeSP.update(states, names, n);
            if (GoHomeSP[HOME_FIND].getState() == ISS_ON)
            {
                if (setParameter(ND::GOTO_HOME, ND::ROTATOR))
                {
                    setDomeState(DOME_MOVING);
                    GoHomeSP.setState(IPS_BUSY);
                    LOG_INFO("Finding home position...");
                }
                else
                    GoHomeSP.setState(IPS_ALERT);
            }
            else if (GoHomeSP[HOME_GOTO].getState() == ISS_ON)
            {
                if (MoveAbs(HomePositionNP[0].getValue()) == IPS_BUSY)
                {

                    setDomeState(DOME_MOVING);
                    GoHomeSP.setState(IPS_BUSY);
                    LOGF_INFO("Going to home position %.2f degrees.", HomePositionNP[0].getValue());
                }
                else
                    GoHomeSP.setState(IPS_ALERT);
            }

            GoHomeSP.apply();
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Rotator Factory
        ///////////////////////////////////////////////////////////////////////////////
        if (RotatorFactorySP.isNameMatch(name))
        {
            const char *requestedOP = IUFindOnSwitchName(states, names, n);
            bool rc = false;
            if (RotatorFactorySP[FACTORY_DEFAULTS].isNameMatch(requestedOP))
                rc = executeFactoryCommand(FACTORY_DEFAULTS, ND::ROTATOR);
            else if (RotatorFactorySP[FACTORY_LOAD].isNameMatch(requestedOP))
            {
                rc = executeFactoryCommand(FACTORY_LOAD, ND::ROTATOR);
                try
                {
                    std::string value;
                    if (getParameter(ND::ACCELERATION_RAMP, ND::ROTATOR, value))
                        RotatorSettingsNP[S_RAMP].setValue(std::stoi(value));
                    if (getParameter(ND::VELOCITY, ND::ROTATOR, value))
                        RotatorSettingsNP[S_VELOCITY].setValue(std::stoi(value));
                    if (getParameter(ND::DEAD_ZONE, ND::ROTATOR, value))
                    {
                        RotatorSettingsNP[S_ZONE].setValue(std::stoi(value));
                    }
                    if (getParameter(ND::RANGE, ND::ROTATOR, value))
                    {
                        RotatorSettingsNP[S_RANGE].setValue(std::stoi(value));
                        RotatorSyncNP[0].setMax(RotatorSettingsNP[S_RANGE].getValue());
                        StepsPerDegree = RotatorSettingsNP[S_RANGE].getValue() / 360.0;
                    }

                    RotatorSettingsNP.apply();
                }
                catch (...)
                {
                    LOG_WARN("Failed to parse rotator settings.");
                }

            }
            else if (RotatorFactorySP[FACTORY_SAVE].isNameMatch(requestedOP))
                rc = executeFactoryCommand(FACTORY_SAVE, ND::ROTATOR);

            RotatorFactorySP.setState((rc ? IPS_OK : IPS_ALERT));
            RotatorFactorySP.apply();
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Shutter Factory
        ///////////////////////////////////////////////////////////////////////////////
        if (ShutterFactorySP.isNameMatch(name))
        {
            const char *requestedOP = IUFindOnSwitchName(states, names, n);
            bool rc = false;
            if (ShutterFactorySP[FACTORY_DEFAULTS].isNameMatch(requestedOP))
                rc = executeFactoryCommand(FACTORY_DEFAULTS, ND::SHUTTER);
            else if (ShutterFactorySP[FACTORY_LOAD].isNameMatch(requestedOP))
            {
                rc = executeFactoryCommand(FACTORY_LOAD, ND::SHUTTER);
                try
                {
                    std::string value;
                    if (getParameter(ND::ACCELERATION_RAMP, ND::SHUTTER, value))
                        ShutterSettingsNP[S_RAMP].setValue(std::stoi(value));
                    if (getParameter(ND::VELOCITY, ND::SHUTTER, value))
                        ShutterSettingsNP[S_VELOCITY].setValue(std::stoi(value));
                    ShutterSettingsNP.apply();
                }
                catch (...)
                {
                    LOG_WARN("Failed to parse shutter settings.");
                }
            }
            else if (ShutterFactorySP[FACTORY_SAVE].isNameMatch(requestedOP))
                rc = executeFactoryCommand(FACTORY_SAVE, ND::SHUTTER);

            ShutterFactorySP.setState((rc ? IPS_OK : IPS_ALERT));
            ShutterFactorySP.apply();
            return true;
        }
    }
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////////////
/// Number handling
//////////////////////////////////////////////////////////////////////////////
bool NexDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        ///////////////////////////////////////////////////////////////////////////////
        /// Home Position
        ///////////////////////////////////////////////////////////////////////////////
        if (HomePositionNP.isNameMatch(name))
        {
            if (setParameter(ND::HOME_POSITION, ND::ROTATOR, values[0] * StepsPerDegree))
            {
                LOGF_INFO("Home position is updated to %.2f degrees.", values[0]);
                HomePositionNP[0].setValue(values[0]);
                HomePositionNP.setState(IPS_OK);
            }
            else
                HomePositionNP.setState(IPS_ALERT);

            HomePositionNP.apply();
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Dome Autosync Threshold
        /// Override INDI::Dome implementaion since we need to update dead-zone
        /// To be compatible with this.
        ///////////////////////////////////////////////////////////////////////////////
        else if (DomeParamNP.isNameMatch(name))
        {
            DomeParamNP.update(values, names, n);
            DomeParamNP.setState(IPS_OK);
            DomeParamNP.apply();

            double minDeadZone = round((DomeParamNP[0].getValue() - 0.1) * StepsPerDegree);
            if (minDeadZone < RotatorSettingsNP[S_ZONE].getValue())
            {
                if (setParameter(ND::DEAD_ZONE, ND::ROTATOR, minDeadZone))
                {
                    RotatorSettingsNP[S_ZONE].setValue(minDeadZone);
                    LOGF_INFO("Updating dead-zone to %.f steps since autosync threshold was set to %.2f degrees.", minDeadZone,
                              DomeParamNP[0].getValue());
                    RotatorSettingsNP.apply();
                }
            }
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Rotator Settings
        ///////////////////////////////////////////////////////////////////////////////
        else if (RotatorSettingsNP.isNameMatch(name))
        {
            std::vector<double> currentSettings(RotatorSettingsNP.count());
            std::vector<bool> rc(RotatorSettingsNP.count(), true);
            for (size_t i = 0; i < RotatorSettingsNP.count(); i++)
                currentSettings[i] = RotatorSettingsNP[i].getValue();

            for (size_t i = 0; i < RotatorSettingsNP.count(); i++)
            {
                if (std::fabs(values[i] - currentSettings[i]) > 0)
                {
                    switch (i)
                    {
                        case S_RAMP:
                            rc[i] = setParameter(ND::ACCELERATION_RAMP, ND::ROTATOR, values[i]);
                            break;

                        case S_VELOCITY:
                            rc[i] = setParameter(ND::VELOCITY, ND::ROTATOR, values[i]);
                            break;

                        case S_ZONE:
                            rc[i] = true;
                            LOG_INFO("Cannot directly change dead-zone to prevent conflict with Autosync threshold in Slaving tab.");
                            //rc[i] = setParameter(ND::DEAD_ZONE, ND::ROTATOR, values[i]);
                            break;

                        case S_RANGE:
                            rc[i] = setParameter(ND::RANGE, ND::ROTATOR, values[i]);
                            break;
                    }
                }
            }

            bool result = true;
            for (size_t i = 0; i < RotatorSettingsNP.count(); i++)
                result &= rc[i];

            if (result)
            {
                RotatorSettingsNP.update(values, names, n);
                RotatorSettingsNP.setState(IPS_OK);
            }
            else
                RotatorSettingsNP.setState(IPS_ALERT);

            if (std::abs(RotatorSettingsNP[S_RANGE].getValue() - RotatorSyncNP[0].getMax()) > 0)
            {
                RotatorSyncNP[0].max = RotatorSettingsNP[S_RANGE].getValue();
                StepsPerDegree = RotatorSettingsNP[S_RANGE].getValue() / 360.0;
                RotatorSyncNP.updateMinMax();
            }

            RotatorSettingsNP.apply();
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Rotator Sync
        ///////////////////////////////////////////////////////////////////////////////
        else if (RotatorSyncNP.isNameMatch(name))
        {
            if (setParameter(ND::POSITION, ND::ROTATOR, values[0]))
            {
                RotatorSyncNP[0].setValue(values[0]);
                RotatorSyncNP.setState(IPS_OK);
            }
            else
                RotatorSyncNP.setState(IPS_ALERT);

            RotatorSyncNP.apply();
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Shutter Sync
        ///////////////////////////////////////////////////////////////////////////////
        else if (ShutterSyncNP.isNameMatch(name))
        {
            if (setParameter(ND::POSITION, ND::SHUTTER, values[0]))
            {
                ShutterSyncNP[0].setValue(values[0]);
                ShutterSyncNP.setState(IPS_OK);
            }
            else
                ShutterSyncNP.setState(IPS_ALERT);

            RotatorSyncNP.apply();
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Shutter Settings
        ///////////////////////////////////////////////////////////////////////////////
        else if (ShutterSettingsNP.isNameMatch(name))
        {
            std::vector<double> currentSettings(ShutterSettingsNP.count());
            std::vector<bool> rc(ShutterSettingsNP.count(), true);
            for (size_t i = 0; i < ShutterSettingsNP.count(); i++)
                currentSettings[i] = ShutterSettingsNP[i].getValue();


            for (size_t i = 0; i < ShutterSettingsNP.count(); i++)
            {
                if (std::fabs(values[i] - currentSettings[i]) > 0)
                {
                    switch (i)
                    {
                        case S_RAMP:
                            rc[i] = setParameter(ND::ACCELERATION_RAMP, ND::SHUTTER, values[i]);
                            break;

                        case S_VELOCITY:
                            rc[i] = setParameter(ND::VELOCITY, ND::SHUTTER, values[i]);
                            break;
                    }
                }
            }

            bool result = true;
            for (size_t i = 0; i < ShutterSettingsNP.count(); i++)
                result &= rc[i];

            if (result)
            {
                ShutterSettingsNP.update(values, names, n);
                ShutterSettingsNP.setState(IPS_OK);
            }
            else
                ShutterSettingsNP.setState(IPS_ALERT);

            ShutterSettingsNP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

///////////////////////////////////////////////////////////////////////////////
/// Sync
///////////////////////////////////////////////////////////////////////////////
bool NexDome::Sync(double az)
{
    return setParameter(ND::POSITION, ND::ROTATOR, az * StepsPerDegree);
}

///////////////////////////////////////////////////////////////////////////////
/// Timer
///////////////////////////////////////////////////////////////////////////////
void NexDome::TimerHit()
{
    std::string response;

    if (checkEvents(response))
        processEvent(response);

    if (getDomeState() == DOME_MOVING || getDomeState() == DOME_PARKING)
    {
        std::string value;
        if (getParameter(ND::REPORT, ND::ROTATOR, value))
            processEvent(value);
    }

    if (HasShutter() && getShutterState() == SHUTTER_MOVING)
    {
        std::string value;
        if (getParameter(ND::POSITION, ND::SHUTTER, value))
            processEvent(value);
    }


    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::MoveAbs(double az)
{
    int32_t target = static_cast<uint32_t>(round(az * StepsPerDegree));
    if (setParameter(ND::GOTO_STEP, ND::ROTATOR, target))
        //if (setParameter(ND::GOTO_AZ, ND::ROTATOR, static_cast<int32_t>(round(az))))
    {
        m_TargetAZSteps = target;
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState NexDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        double nextTarget = range360(DomeAbsPosNP[0].getValue() + (dir == DOME_CW ? 10 : -10));
        LOGF_INFO("Moving %s by 10 degrees...", (dir == DOME_CW ? "CW" : "CCW"));
        return MoveAbs(nextTarget);
    }
    else
    {
        return (Abort() ? IPS_OK : IPS_ALERT);
    }
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::Park()
{
    MoveAbs(GetAxis1Park());

    LOGF_INFO("Parking to %.2f azimuth...", GetAxis1Park());

    if (HasShutter() && ShutterParkPolicySP[SHUTTER_CLOSE_ON_PARK].getState() == ISS_ON)
    {
        LOG_INFO("Closing shutter on parking...");
        ControlShutter(ShutterOperation::SHUTTER_CLOSE);
        DomeShutterSP[SHUTTER_OPEN].setState(ISS_OFF);
        DomeShutterSP[SHUTTER_CLOSE].setState(ISS_ON);
        setShutterState(SHUTTER_MOVING);
    }

    return IPS_BUSY;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::UnPark()
{
    if (HasShutter() && ShutterParkPolicySP[SHUTTER_OPEN_ON_UNPARK].getState() == ISS_ON)
    {
        LOG_INFO("Opening shutter on unparking...");
        ControlShutter(ShutterOperation::SHUTTER_OPEN);
        DomeShutterSP[SHUTTER_OPEN].setState(ISS_ON);
        DomeShutterSP[SHUTTER_CLOSE].setState(ISS_OFF);
        setShutterState(SHUTTER_MOVING);
        return IPS_BUSY;
    }
    else
    {
        SetParked(false);
        return IPS_OK;
    }
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::ControlShutter(ShutterOperation operation)
{
    if (!m_ShutterConnected)
    {
        LOG_ERROR("Shutter is not connected. Check battery and XBEE connection.");
        return IPS_ALERT;
    }

    // Check if shutter is open or close.
    switch (operation)
    {
        case SHUTTER_OPEN:
            if (setParameter(ND::OPEN_SHUTTER, ND::SHUTTER))
            {
                LOG_INFO("Shutter is opening...");
                return IPS_BUSY;
            }
            break;

        case SHUTTER_CLOSE:
            if (setParameter(ND::CLOSE_SHUTTER, ND::SHUTTER))
            {
                LOG_INFO("Shutter is closing...");
                return IPS_BUSY;
            }
            break;
    }

    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::Abort()
{
    bool rcRotator = setParameter(ND::EMERGENCY_STOP, ND::ROTATOR);
    if (rcRotator && GoHomeSP.getState() == IPS_BUSY)
    {
        GoHomeSP[HOME_FIND].setState(ISS_OFF);
        GoHomeSP.setState(IPS_IDLE);
        GoHomeSP.apply();
    }

    bool rcShutter = setParameter(ND::EMERGENCY_STOP, ND::SHUTTER);
    if (rcShutter && getShutterState() == SHUTTER_MOVING)
        setShutterState(SHUTTER_UNKNOWN);

    return rcRotator && rcShutter;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosNP[0].getValue());
    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::SetDefaultPark()
{
    // default park position is pointed south
    SetAxis1Park(0);
    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::getStartupValues()
{
    std::string value;

    try
    {
        // Rotator Position
        if (getParameter(ND::POSITION, ND::ROTATOR, value))
            RotatorSyncNP[0].setValue(std::stoi(value));

        // Rotator Settings
        if (getParameter(ND::ACCELERATION_RAMP, ND::ROTATOR, value))
            RotatorSettingsNP[S_RAMP].setValue(std::stoi(value));
        if (getParameter(ND::VELOCITY, ND::ROTATOR, value))
            RotatorSettingsNP[S_VELOCITY].setValue(std::stoi(value));
        if (getParameter(ND::DEAD_ZONE, ND::ROTATOR, value))
        {
            RotatorSettingsNP[S_ZONE].setValue(std::stoi(value));
            //            double minAutoSyncThreshold = RotatorSettingsN[S_ZONE].value / StepsPerDegree;
            //            if (DomeParamN[0].value < minAutoSyncThreshold)
            //            {
            //                // Add 0.1 degrees as buffer so that dome doesn't get stuck thinking it's
            //                // still within the dead-zone.
            //                DomeParamN[0].value = minAutoSyncThreshold + 0.1;
            //                LOGF_INFO("Setting Autosync threshold to %.2f degrees since the dead-zone limit is set at %.f steps.",
            //                          DomeParamN[0].value, RotatorSettingsN[S_ZONE].value);
            //            }
        }
        if (getParameter(ND::RANGE, ND::ROTATOR, value))
        {
            RotatorSettingsNP[S_RANGE].setValue(std::stoi(value));
            RotatorSyncNP[0].setMax(RotatorSettingsNP[S_RANGE].getValue());
            StepsPerDegree = RotatorSettingsNP[S_RANGE].getValue() / 360.0;
        }

        // Shutter Settings
        if (HasShutter())
        {
            if (getParameter(ND::POSITION, ND::SHUTTER, value))
                ShutterSyncNP[0].setValue(std::stoi(value));

            if (getParameter(ND::ACCELERATION_RAMP, ND::SHUTTER, value))
                ShutterSettingsNP[S_RAMP].setValue(std::stoi(value));
            if (getParameter(ND::VELOCITY, ND::SHUTTER, value))
                ShutterSettingsNP[S_VELOCITY].setValue(std::stoi(value));
        }

        // Home Setting
        if (getParameter(ND::HOME_POSITION, ND::ROTATOR, value))
            HomePositionNP[0].setValue(std::stoi(value) / StepsPerDegree);

    }
    catch(...)
    {
        return false;
    }

    // Rotator State
    if (getParameter(ND::REPORT, ND::ROTATOR, value))
        processEvent(value);

    // Shutter State
    if (HasShutter())
    {
        if (getParameter(ND::REPORT, ND::SHUTTER, value))
            processEvent(value);
    }

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(0);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(0);
        SetAxis1ParkDefault(0);
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::saveConfigItems(FILE * fp)
{
    INDI::Dome::saveConfigItems(fp);

    RotatorSettingsNP.save(fp);
    ShutterSettingsNP.save(fp);
    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::setParameter(ND::Commands command, ND::Targets target, int32_t value)
{
    std::string verb = ND::CommandsMap.at(command);
    std::ostringstream cmd;
    cmd << "@";
    cmd << verb;
    // Commands with two letters do not need Write (W)
    if (verb.size() == 1)
        cmd << "W";
    cmd << ((target == ND::ROTATOR) ? "R" : "S");

    if (value != -1e6)
    {
        cmd << ",";
        cmd << value;
    }

    char res[ND::DRIVER_LEN] = {0};
    return sendCommand(cmd.str().c_str(), res);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::executeFactoryCommand(uint8_t command, ND::Targets target)
{
    std::ostringstream cmd;
    cmd << "@";
    switch (command)
    {
        case FACTORY_DEFAULTS:
            cmd << "ZD";
            LOGF_INFO("%s: Loading factory defaults...", (target == ND::ROTATOR) ? "Rotator" : "Shutter");
            break;

        case FACTORY_LOAD:
            cmd << "ZR";
            LOGF_INFO("%s: Loading EEPROM settings...", (target == ND::ROTATOR) ? "Rotator" : "Shutter");
            break;

        case FACTORY_SAVE:
            cmd << "ZW";
            LOGF_INFO("%s: Saving settings to EEPROM...", (target == ND::ROTATOR) ? "Rotator" : "Shutter");
            break;
    }

    cmd << ((target == ND::ROTATOR) ? "R" : "S");

    return sendCommand(cmd.str().c_str());
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::getParameter(ND::Commands command, ND::Targets target, std::string &value)
{
    char res[ND::DRIVER_LEN] = {0};
    bool response_found = false;

    std::string verb = ND::CommandsMap.at(command) + "R";

    std::ostringstream cmd;
    // Magic start character
    cmd << "@";
    // Command verb
    cmd << verb;
    // Target (Rotator or Shutter)
    cmd << ((target == ND::ROTATOR) ? "R" : "S");

    if (sendCommand(cmd.str().c_str(), res))
    {
        std::string response(res);

        // Since we can get many unrelated responses from the firmware
        // i.e. events, we need to parse all responses, and see which
        // one is related to our get command.
        std::vector<std::string> all = split(response, "\r\n");

        // Let's find our match using this regex
        std::regex re;

        // Firmware is exception since the response does not include the target
        // for everything else, the echo back includes the target.
        if (command != ND::SEMANTIC_VERSION)
            re = (verb + ((target == ND::ROTATOR) ? "R" : "S") + "([^#]+)");
        else
            re = (verb + "([^#]+)");

        std::smatch match;

        // Not iterate over all responses
        for (auto &oneEvent : all)
        {
            std::string trimmedEvent = trim(oneEvent);

            // If we find the match, tag it.
            if (std::regex_search(trimmedEvent, match, re))
            {
                value = match.str(1);
                response_found = true;
            }
            // Otherwise process the event
            else
                processEvent(trimmedEvent);
        }
    }

    return response_found;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::checkEvents(std::string &response)
{
    int nbytes_read = 0;
    char res[ND::DRIVER_LEN] = {0};

    int rc = tty_nread_section(PortFD, res, ND::DRIVER_LEN, ND::DRIVER_EVENT_CHAR, ND::DRIVER_EVENT_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK || nbytes_read < 3)
        return false;

    std::string raw_response = res;

    // Trim
    response = trim(raw_response);

    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::processEvent(const std::string &event)
{
    for (const auto &kv : ND::EventsMap)
    {
        std::regex re(kv.second + "([^#]+)");
        std::smatch match;
        std::string value;

        if (event == kv.second)
            value = event;
        else if (std::regex_search(event, match, re))
            value = match.str(1);
        else
            continue;

        LOGF_DEBUG("Processing event <%s> with value <%s>", event.c_str(), value.c_str());

        switch (kv.first)
        {
            case ND::XBEE_STATE:
                if (!m_ShutterConnected && value == "Online")
                {
                    m_ShutterConnected = true;
                    LOG_INFO("Shutter is connected.");
                }
                else if (m_ShutterConnected && value != "Online")
                {
                    m_ShutterConnected = false;
                    LOG_WARN("Lost connection to the shutter!");
                }
                return true;

            case ND::ROTATOR_POSITION:
            {
                try
                {
                    // 153 = full_steps_circumference / 360 = 55080 / 360
                    double newAngle = range360(std::stoi(value) / StepsPerDegree);
                    if (std::abs(DomeAbsPosNP[0].getValue() - newAngle) > 0.001)
                    {
                        DomeAbsPosNP[0].setValue(newAngle);
                        DomeAbsPosNP.apply();
                    }
                }
                catch (...)
                {
                    return false;
                }
            }
            return true;

            case ND::SHUTTER_POSITION:
            {
                try
                {
                    int32_t position = std::stoi(value);
                    if (std::abs(position - ShutterSyncNP[0].getValue()) > 0)
                    {
                        ShutterSyncNP[0].setValue(position);
                        ShutterSyncNP.apply();
                    }
                }
                catch (...)
                {
                    return false;
                }
            }
            return true;

            case ND::ROTATOR_REPORT:
                return processRotatorReport(value);

            case ND::SHUTTER_REPORT:
                return processShutterReport(value);

            case ND::ROTATOR_LEFT:
            case ND::ROTATOR_RIGHT:
                if (getDomeState() != DOME_MOVING && getDomeState() != DOME_PARKING)
                {
                    setDomeState(DOME_MOVING);
                    LOGF_INFO("Dome is rotating %s.", ((kv.first == ND::ROTATOR_LEFT) ? "counter-clock wise" : "clock-wise"));
                }
                return true;

            case ND::ROTATOR_STOPPED:
                if (getDomeState() == DOME_MOVING)
                {
                    LOG_INFO("Dome reached target position.");
                    setDomeState(DOME_SYNCED);
                }
                else if (getDomeState() == DOME_PARKING)
                {
                    LOG_INFO("Dome is parked.");
                    setDomeState(DOME_PARKED);
                }
                else
                    setDomeState(DOME_IDLE);
                return true;

            case ND::SHUTTER_OPENING:
                if (getShutterState() != SHUTTER_MOVING)
                {
                    setShutterState(SHUTTER_MOVING);
                    LOG_INFO("Shutter is opening...");
                    break;
                }
                return true;

            case ND::SHUTTER_CLOSING:
                if (getShutterState() != SHUTTER_MOVING)
                {
                    setShutterState(SHUTTER_MOVING);
                    LOG_INFO("Shutter is closing...");
                    break;
                }
                return true;

            case ND::SHUTTER_BATTERY:
            {
                try
                {
                    uint32_t battery_adu = std::stoul(value);
                    double vref = battery_adu * ND::ADU_TO_VREF;
                    if (std::fabs(vref - ShutterBatteryLevelNP[0].getValue()) > 0.01)
                    {
                        ShutterBatteryLevelNP[0].setValue(vref);
                        // TODO: Must check if batter is OK, warning, or in critical level
                        ShutterBatteryLevelNP.setState(IPS_OK);
                        ShutterBatteryLevelNP.apply();
                    }
                }
                catch(...)
                {
                    return false;
                }
            }
            break;

            default:
                LOGF_DEBUG("Unhandled event: %s", value.c_str());
                break;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool NexDome::processRotatorReport(const std::string &report)
{
    std::regex re(R"((\d+),(\d+),(\d+),(\d+),(\d+))");
    std::smatch match;
    if (std::regex_search(report, match, re))
    {
        try
        {
            uint32_t position = std::stoul(match.str(1));
            uint32_t at_home = std::stoul(match.str(2));
            uint32_t cirumference = std::stoul(match.str(3));
            uint32_t home_position = std::stoul(match.str(4));
            uint32_t dead_zone = std::stoul(match.str(5));

            double newStepsPerDegree = cirumference / 360.0;
            if (std::abs(newStepsPerDegree - StepsPerDegree) > 0.01)
                StepsPerDegree = newStepsPerDegree;

            if (std::abs(position - RotatorSyncNP[0].getValue()) > 0)
            {
                RotatorSyncNP[0].setValue(position);
                RotatorSyncNP.apply(nullptr);
            }

            double posAngle = range360(position / StepsPerDegree);
            if (std::fabs(posAngle - DomeAbsPosNP[0].getValue()) > 0.01)
            {
                DomeAbsPosNP[0].setValue(posAngle);
                DomeAbsPosNP.apply();
            }

            double homeAngle = range360(home_position / StepsPerDegree);
            if (std::fabs(homeAngle - HomePositionNP[0].getValue()) > 0.01)
            {
                HomePositionNP[0].setValue(homeAngle);
                HomePositionNP.apply(nullptr);
            }

            double homeDiff = std::abs(homeAngle - posAngle);
            if (GoHomeSP.getState() == IPS_BUSY &&
                ((GoHomeSP[HOME_FIND].getState() == ISS_ON && at_home == 1) ||
                 (GoHomeSP[HOME_GOTO].getState() == ISS_ON && homeDiff <= 0.1)))
            {
                LOG_INFO("Rotator reached home position.");
                GoHomeSP.reset();
                GoHomeSP.setState(IPS_OK);
                GoHomeSP.apply();
            }

            if (dead_zone != static_cast<uint32_t>(RotatorSettingsNP[S_ZONE].getValue()))
            {
                RotatorSettingsNP[S_ZONE].setValue(dead_zone);
                RotatorSettingsNP.apply();
            }

            // update to fix issue with movement across 0 degrees
            // for example, if the dead zone is 0.5 degrees, then NexDome won't move if going from 0.1 to 359.9 degrees.
            // however driver expects response from rotator unless difference calculation is modified, and movement stalls
            // e.g. the angles 0.1 and -0.1 should be compared instead
            int a = position;
            int b = m_TargetAZSteps;

            if (getDomeState() == DOME_MOVING || getDomeState() == DOME_PARKING)
            {
                // if a > 0 and b < 360 and both within dead zone, make b negative equivalent angle
                if(a >= 0 && a <= int(dead_zone) && b >= (int(cirumference) - int(dead_zone)))
                {
                    b -= int(cirumference);
                }  // if opposite case then make a the negative angle equivalent
                else if(b >= 0 && b <=int(dead_zone) && a >= (int(cirumference) - int(dead_zone)))
                {
                    a -= int(cirumference);
                }

                // If we reach target position.  (now calculation is correct)
                if (std::abs(a - b) <= int(dead_zone) )
                {                    
                    if (getDomeState() == DOME_MOVING)
                    {
                        LOG_INFO("Dome reached target position.");
                        setDomeState(DOME_SYNCED);
                    }
                    else if (getDomeState() == DOME_PARKING)
                    {
                        LOG_INFO("Dome is parked.");
                        SetParked(true);
                        //setDomeState(DOME_PARKED);
                    }
                }
            }
        }
        catch(...)
        {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool NexDome::processShutterReport(const std::string &report)
{
    std::regex re(R"((-?\d+),(\d+),(\d+),(\d+))");
    std::smatch match;
    if (std::regex_search(report, match, re))
    {
        try
        {
            int32_t position = std::stoi(match.str(1));
            int32_t travel_limit = std::stoi(match.str(2));
            bool open_limit_switch = std::stoul(match.str(3)) == 1;
            bool close_limit_switch = std::stoul(match.str(4)) == 1;

            if (std::abs(position - ShutterSyncNP[0].getValue()) > 0)
            {
                ShutterSyncNP[0].setValue(position);
                ShutterSyncNP.apply();
            }

            INDI_UNUSED(travel_limit);

            if (getShutterState() == SHUTTER_MOVING || getShutterState() == SHUTTER_UNKNOWN)
            {
                //if (position == travel_limit || open_limit_switch)
                if (open_limit_switch)
                {
                    setShutterState(SHUTTER_OPENED);
                    LOG_INFO("Shutter is fully opened.");

                    if (getDomeState() == DOME_UNPARKING)
                        SetParked(false);
                }
                //else if (position == 0 || close_limit_switch)
                else if (close_limit_switch)
                {
                    setShutterState(SHUTTER_CLOSED);
                    LOG_INFO("Shutter is fully closed.");
                }

            }

        }
        catch(...)
        {
            return false;
        }
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[ND::DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        char cmd_terminated[ND::DRIVER_LEN * 2] = {0};
        snprintf(cmd_terminated, ND::DRIVER_LEN * 2, "%s\r\n", cmd);
        rc = tty_write_string(PortFD, cmd_terminated, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, ND::DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, ND::DRIVER_LEN, ND::DRIVER_STOP_CHAR, ND::DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[ND::DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void NexDome::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> NexDome::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::string &NexDome::ltrim(std::string &str, const std::string &chars)
{
    str.erase(0, str.find_first_not_of(chars));
    return str;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::string &NexDome::rtrim(std::string &str, const std::string &chars)
{
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::string &NexDome::trim(std::string &str, const std::string &chars)
{
    return ltrim(rtrim(str, chars), chars);
}
