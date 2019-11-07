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
void ISGetProperties(const char *dev)
{
    nexDome->ISGetProperties(dev);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    nexDome->ISNewSwitch(dev, name, states, names, num);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    nexDome->ISNewText(dev, name, texts, names, num);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    nexDome->ISNewNumber(dev, name, values, names, num);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISSnoopDevice (XMLEle *root)
{
    nexDome->ISSnoopDevice(root);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
NexDome::NexDome()
{
    setVersion(INDI_NEXDOME_VERSION_MAJOR, INDI_NEXDOME_VERSION_MINOR);

    SetDomeCapability(DOME_CAN_ABORT |
                      DOME_CAN_ABS_MOVE |
                      DOME_CAN_PARK |
                      DOME_HAS_SHUTTER |
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
    IUFillSwitch(&GoHomeS[0], "HOME_GO", "Go", ISS_OFF);
    IUFillSwitchVector(&GoHomeSP, GoHomeS, 1, getDeviceName(), "DOME_HOMING", "Homing", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Home Position
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&HomePositionN[0], "POSITON", "degrees", "%.f", 0.0, 360.0, 0.0, 0);
    IUFillNumberVector(&HomePositionNP, HomePositionN, 1, getDeviceName(), "HOME_POSITION", "Home Az", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Battery
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&ShutterBatteryLevelN[ND::ROTATOR], "VOLTS", "Voltage", "%.2f", 0.0, 16.0, 0.0, 0);
    IUFillNumberVector(&ShutterBatteryLevelNP, ShutterBatteryLevelN, 1, getDeviceName(), "BATTERY", "Battery Level", ND::SHUTTER_TAB.c_str(), IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Rotator Firmware Info
    ///////////////////////////////////////////////////////////////////////////////
    IUFillText(&RotatorFirmwareVersionT[0], "FIRMWARE_VERSION", "Version", "");
    IUFillTextVector(&RotatorFirmwareVersionTP, RotatorFirmwareVersionT, 1, getDeviceName(), "FIRMWARE", "Firmware", ND::ROTATOR_TAB.c_str(), IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Shutter Firmware Info
    ///////////////////////////////////////////////////////////////////////////////
    IUFillText(&ShutterFirmwareVersionT[0], "FIRMWARE_VERSION", "Version", "");
    IUFillTextVector(&ShutterFirmwareVersionTP, ShutterFirmwareVersionT, 1, getDeviceName(), "FIRMWARE", "Firmware", ND::SHUTTER_TAB.c_str(), IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Close Shutter on Park?
    ///////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&CloseShutterOnParkS[ND::ENABLED], "ENABLED", "Enabled", ISS_ON);
    IUFillSwitch(&CloseShutterOnParkS[ND::DISABLED], "DISABLED", "Disabled", ISS_OFF);
    IUFillSwitchVector(&CloseShutterOnParkSP, CloseShutterOnParkS, 2, getDeviceName(), "DOME_CLOSE_SHUTTER_ON_PARK", "Close Shutter on Park",
                       ND::SHUTTER_TAB.c_str(), IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Rotator Settings
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&RotatorSettingsN[S_RAMP], "S_RAMP", "Acceleration Ramp (ms)", "%.f", 0.0, 5000, 1000.0, 0);
    IUFillNumber(&RotatorSettingsN[S_VELOCITY], "S_VELOCITY", "Velocity (steps/s)", "%.f", 0.0, 5000, 1000.0, 0);
    IUFillNumber(&RotatorSettingsN[S_ZONE], "S_ZONE", "Dead Zone (steps)", "%.f", 0.0, 32000, 1000.0, 2400);
    IUFillNumber(&RotatorSettingsN[S_RANGE], "S_RANGE", "Travel Range (steps)", "%.f", 0.0, 55080, 1000.0, 55080);
    IUFillNumberVector(&RotatorSettingsNP, RotatorSettingsN, 4, getDeviceName(), "ROTATOR_SETTINGS", "Rotator", ND::ROTATOR_TAB.c_str(),
                       IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Shutter Settings
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&ShutterSettingsN[S_RAMP], "S_RAMP", "Acceleration Ramp (ms)", "%.f", 0.0, 5000, 1000.0, 0);
    IUFillNumber(&ShutterSettingsN[S_VELOCITY], "S_VELOCITY", "Velocity (step/s)", "%.f", 0.0, 5000, 1000.0, 0);
    IUFillNumberVector(&ShutterSettingsNP, ShutterSettingsN, 2, getDeviceName(), "SHUTTER_SETTINGS", "Shutter", ND::SHUTTER_TAB.c_str(),
                       IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Rotator Factory Settings
    ///////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&RotatorFactoryS[FACTORY_DEFAULTS], "FACTORY_DEFAULTS", "Defaults", ISS_OFF);
    IUFillSwitch(&RotatorFactoryS[FACTORY_LOAD], "FACTORY_LOAD", "Load", ISS_OFF);
    IUFillSwitch(&RotatorFactoryS[FACTORY_SAVE], "FACTORY_SAVE", "Save", ISS_OFF);
    IUFillSwitchVector(&RotatorFactorySP, RotatorFactoryS, 3, getDeviceName(), "ROTATOR_FACTORY_SETTINGS", "Factory",
                       ND::ROTATOR_TAB.c_str(), IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Shutter Factory Settings
    ///////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&ShutterFactoryS[FACTORY_DEFAULTS], "FACTORY_DEFAULTS", "Defaults", ISS_OFF);
    IUFillSwitch(&ShutterFactoryS[FACTORY_LOAD], "FACTORY_LOAD", "Load", ISS_OFF);
    IUFillSwitch(&ShutterFactoryS[FACTORY_SAVE], "FACTORY_SAVE", "Save", ISS_OFF);
    IUFillSwitchVector(&ShutterFactorySP, ShutterFactoryS, 3, getDeviceName(), "SHUTTER_FACTORY_SETTINGS", "Factory",
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
    bool rc1 = false, rc2 = false;

    if (getParameter(ND::SEMANTIC_VERSION, ND::ROTATOR, value))
    {
        LOGF_INFO("Detected rotator firmware version %s", value.c_str());
        if (value < ND::MINIMUM_VERSION)
        {
            LOGF_ERROR("Rotator version %s is not supported. Please upgrade to version %s or higher.", value.c_str(), ND::MINIMUM_VERSION.c_str());
            return false;
        }

        rc1 = true;
        RotatorFirmwareVersionTP.s = IPS_OK;
        IUSaveText(&RotatorFirmwareVersionT[0], value.c_str());
    }

    if (getParameter(ND::SEMANTIC_VERSION, ND::SHUTTER, value))
    {
        LOGF_INFO("Detected shutter firmware version %s", value.c_str());
        if (value < ND::MINIMUM_VERSION)
        {
            LOGF_ERROR("Shutter version %s is not supported. Please upgrade to version %s or higher.", value.c_str(), ND::MINIMUM_VERSION.c_str());
            return false;
        }

        rc2 = true;
        ShutterFirmwareVersionTP.s = IPS_OK;
        IUSaveText(&ShutterFirmwareVersionT[0], value.c_str());
    }

    return (rc1 && rc2);
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

        defineSwitch(&GoHomeSP);
        defineNumber(&HomePositionNP);

        // Rotator
        defineNumber(&RotatorSettingsNP);
        defineSwitch(&RotatorFactorySP);
        defineText(&RotatorFirmwareVersionTP);

        // Shutter
        defineNumber(&ShutterSettingsNP);
        defineNumber(&ShutterBatteryLevelNP);
        defineSwitch(&CloseShutterOnParkSP);
        defineSwitch(&ShutterFactorySP);
        defineText(&ShutterFirmwareVersionTP);
    }
    else
    {
        deleteProperty(GoHomeSP.name);
        deleteProperty(HomePositionNP.name);

        // Rotator
        deleteProperty(RotatorSettingsNP.name);
        deleteProperty(RotatorFactorySP.name);
        deleteProperty(RotatorFirmwareVersionTP.name);

        // Shutter
        deleteProperty(ShutterSettingsNP.name);
        deleteProperty(ShutterBatteryLevelNP.name);
        deleteProperty(CloseShutterOnParkSP.name);
        deleteProperty(ShutterFactorySP.name);
        deleteProperty(ShutterFirmwareVersionTP.name);
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
        if(!strcmp(name, GoHomeSP.name))
        {
            if (setParameter(ND::GOTO_HOME, ND::ROTATOR))
            {
                GoHomeS[0].s = ISS_ON;
                GoHomeSP.s = IPS_BUSY;
                LOG_INFO("Finding home position...");
            }
            else
            {
                GoHomeSP.s = IPS_ALERT;
            }

            IDSetSwitch(&GoHomeSP, nullptr);
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Close Shutter on Park
        ///////////////////////////////////////////////////////////////////////////////
        if (!strcmp(name, CloseShutterOnParkSP.name))
        {
            IUUpdateSwitch(&CloseShutterOnParkSP, states, names, n);
            CloseShutterOnParkSP.s = IPS_OK;
            IDSetSwitch(&CloseShutterOnParkSP, nullptr);
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Rotator Factory
        ///////////////////////////////////////////////////////////////////////////////
        if (!strcmp(name, RotatorFactorySP.name))
        {
            const char *requestedOP = IUFindOnSwitchName(states, names, n);
            bool rc = false;
            if (!strcmp(requestedOP, RotatorFactoryS[FACTORY_DEFAULTS].name))
                rc = executeFactoryCommand(FACTORY_DEFAULTS, ND::ROTATOR);
            else if (!strcmp(requestedOP, RotatorFactoryS[FACTORY_LOAD].name))
                rc = executeFactoryCommand(FACTORY_LOAD, ND::ROTATOR);
            else if (!strcmp(requestedOP, RotatorFactoryS[FACTORY_SAVE].name))
                rc = executeFactoryCommand(FACTORY_SAVE, ND::ROTATOR);

            RotatorFactorySP.s = (rc ? IPS_OK : IPS_ALERT);
            IDSetSwitch(&RotatorFactorySP, nullptr);
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Shutter Factory
        ///////////////////////////////////////////////////////////////////////////////
        if (!strcmp(name, ShutterFactorySP.name))
        {
            const char *requestedOP = IUFindOnSwitchName(states, names, n);
            bool rc = false;
            if (!strcmp(requestedOP, ShutterFactoryS[FACTORY_DEFAULTS].name))
                rc = executeFactoryCommand(FACTORY_DEFAULTS, ND::SHUTTER);
            else if (!strcmp(requestedOP, ShutterFactoryS[FACTORY_LOAD].name))
                rc = executeFactoryCommand(FACTORY_LOAD, ND::SHUTTER);
            else if (!strcmp(requestedOP, ShutterFactoryS[FACTORY_SAVE].name))
                rc = executeFactoryCommand(FACTORY_SAVE, ND::SHUTTER);

            ShutterFactorySP.s = (rc ? IPS_OK : IPS_ALERT);
            IDSetSwitch(&ShutterFactorySP, nullptr);
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
        if (!strcmp(name, HomePositionNP.name))
        {
            if (setParameter(ND::HOME_POSITION, ND::ROTATOR, values[0] * ND::STEPS_PER_DEGREE))
            {
                LOGF_INFO("Home position is updated to %.2f degrees.", values[0]);
                HomePositionN[0].value = values[0];
                HomePositionNP.s = IPS_OK;

                setDomeState(DOME_MOVING);
            }
            else
                HomePositionNP.s = IPS_ALERT;

            IDSetNumber(&HomePositionNP, nullptr);
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Rotator Settings
        ///////////////////////////////////////////////////////////////////////////////
        if (!strcmp(name, RotatorSettingsNP.name))
        {
            std::vector<double> currentSettings(RotatorSettingsNP.nnp);
            std::vector<bool> rc(RotatorSettingsNP.nnp, true);
            for (int i = 0; i < RotatorSettingsNP.nnp; i++)
                currentSettings[i] = RotatorSettingsN[i].value;

            for (int i = 0; i < RotatorSettingsNP.nnp; i++)
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
                            rc[i] = setParameter(ND::DEAD_ZONE, ND::ROTATOR, values[i]);
                            break;

                        case S_RANGE:
                            rc[i] = setParameter(ND::RANGE, ND::ROTATOR, values[i]);
                            break;
                    }
                }
            }

            bool result = true;
            for (int i = 0; i < RotatorSettingsNP.nnp; i++)
                result &= rc[i];

            if (result)
            {
                IUUpdateNumber(&RotatorSettingsNP, values, names, n);
                RotatorSettingsNP.s = IPS_OK;
            }
            else
                RotatorSettingsNP.s = IPS_ALERT;

            IDSetNumber(&RotatorSettingsNP, nullptr);
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Shutter Settings
        ///////////////////////////////////////////////////////////////////////////////
        if (!strcmp(name, ShutterSettingsNP.name))
        {
            std::vector<double> currentSettings(ShutterSettingsNP.nnp);
            std::vector<bool> rc(ShutterSettingsNP.nnp, true);
            for (int i = 0; i < ShutterSettingsNP.nnp; i++)
                currentSettings[i] = ShutterSettingsN[i].value;


            for (int i = 0; i < ShutterSettingsNP.nnp; i++)
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
            for (int i = 0; i < ShutterSettingsNP.nnp; i++)
                result &= rc[i];

            if (result)
            {
                IUUpdateNumber(&ShutterSettingsNP, values, names, n);
                ShutterSettingsNP.s = IPS_OK;
            }
            else
                ShutterSettingsNP.s = IPS_ALERT;

            IDSetNumber(&ShutterSettingsNP, nullptr);
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
    return setParameter(ND::POSITION, ND::ROTATOR, az * ND::STEPS_PER_DEGREE);
}

///////////////////////////////////////////////////////////////////////////////
/// Timer
///////////////////////////////////////////////////////////////////////////////
void NexDome::TimerHit()
{
    std::string response;

    //    if (getParameter(ND::REPORT, ND::ROTATOR, response))
    //        processRotatorReport(response);

    //    if (getParameter(ND::REPORT, ND::SHUTTER, response))
    //        processShutterReport(response);

    //    while (checkEvents(response))
    //        processEvent(response);

    if (checkEvents(response))
        processEvent(response);

    SetTimer(POLLMS);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::MoveAbs(double az)
{
    if (setParameter(ND::GOTO_AZ, ND::ROTATOR, az))
        return IPS_BUSY;
    else
        return IPS_ALERT;
}

IPState NexDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        double nextTarget = range360(DomeAbsPosN[0].value + (dir == DOME_CW ? 10 : -10));
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

    if (HasShutter() && IUFindOnSwitchIndex(&CloseShutterOnParkSP) == ND::ENABLED)
    {
        LOG_INFO("Closing shutter on parking...");
        ControlShutter(ShutterOperation::SHUTTER_CLOSE);
        DomeShutterS[SHUTTER_OPEN].s = ISS_OFF;
        DomeShutterS[SHUTTER_CLOSE].s = ISS_ON;
        setShutterState(SHUTTER_MOVING);
    }

    return IPS_BUSY;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::UnPark()
{
    SetParked(false);
    return IPS_OK;
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
    bool rc = setParameter(ND::EMERGENCY_STOP, ND::ROTATOR);
    if (rc && GoHomeSP.s == IPS_BUSY)
    {
        GoHomeS[0].s = ISS_OFF;
        GoHomeSP.s = IPS_IDLE;
        IDSetSwitch(&GoHomeSP, nullptr);
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosN[0].value);
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

    // Rotator Settings
    if (getParameter(ND::ACCELERATION_RAMP, ND::ROTATOR, value))
        RotatorSettingsN[S_RAMP].value = std::stoi(value);
    if (getParameter(ND::VELOCITY, ND::ROTATOR, value))
        RotatorSettingsN[S_VELOCITY].value = std::stoi(value);
    if (getParameter(ND::DEAD_ZONE, ND::ROTATOR, value))
        RotatorSettingsN[S_ZONE].value = std::stoi(value);
    if (getParameter(ND::RANGE, ND::ROTATOR, value))
        RotatorSettingsN[S_RANGE].value = std::stoi(value);

    // Shutter Settings
    if (getParameter(ND::ACCELERATION_RAMP, ND::SHUTTER, value))
        ShutterSettingsN[S_RAMP].value = std::stoi(value);
    if (getParameter(ND::VELOCITY, ND::SHUTTER, value))
        ShutterSettingsN[S_VELOCITY].value = std::stoi(value);

    // Home Setting
    if (getParameter(ND::HOME_POSITION, ND::ROTATOR, value))
        HomePositionN[0].value = std::stoi(value) / ND::STEPS_PER_DEGREE;

    // Rotator State
    if (getParameter(ND::REPORT, ND::ROTATOR, value))
        processEvent(value);

    // Shutter State
    if (getParameter(ND::REPORT, ND::SHUTTER, value))
        processEvent(value);

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

    IUSaveConfigNumber(fp, &RotatorSettingsNP);
    IUSaveConfigNumber(fp, &ShutterSettingsNP);
    IUSaveConfigSwitch(fp, &CloseShutterOnParkSP);
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
            re = (verb + ((target == ND::ROTATOR) ? "R" : "S") + "(.+[^#])");
        else
            re = (verb + "(.+[^#])");

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

    if (rc != TTY_OK)
        return false;

    if (nbytes_read < 3)
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
        std::regex re(kv.second + "(.+[^#])");
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
                // 153 = full_steps_circumference / 360 = 55080 / 360
                double newAngle = range360(std::stoi(value) / ND::STEPS_PER_DEGREE);
                if (std::fabs(DomeAbsPosN[0].value - newAngle) > 0.001)
                {
                    DomeAbsPosN[0].value = newAngle;
                    IDSetNumber(&DomeAbsPosNP, nullptr);
                }
            }
            return true;

            case ND::SHUTTER_POSITION:
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
                uint32_t battery_adu = std::stoul(value);
                double vref = battery_adu * ND::ADU_TO_VREF;
                if (std::fabs(vref - ShutterBatteryLevelN[0].value) > 0.01)
                {
                    ShutterBatteryLevelN[0].value = vref;
                    // TODO: Must check if batter is OK, warning, or in critical level
                    ShutterBatteryLevelNP.s = IPS_OK;
                    IDSetNumber(&ShutterBatteryLevelNP, nullptr);
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
        uint32_t position = std::stoul(match.str(1));
        uint32_t at_home = std::stoul(match.str(2));
        uint32_t cirumference = std::stoul(match.str(3));
        uint32_t home_position = std::stoul(match.str(4));
        uint32_t dead_zone = std::stoul(match.str(5));

        double posAngle = range360(position / ND::STEPS_PER_DEGREE);
        if (std::fabs(posAngle - DomeAbsPosN[0].value) > 0.01)
        {
            DomeAbsPosN[0].value = posAngle;
            IDSetNumber(&DomeAbsPosNP, nullptr);
        }

        if (GoHomeSP.s == IPS_BUSY && at_home == 1)
        {
            LOG_INFO("Rotator reached home position.");
            GoHomeS[0].s = ISS_OFF;
            GoHomeSP.s = IPS_OK;
            IDSetSwitch(&GoHomeSP, nullptr);
        }

        double homeAngle = range360(static_cast<double>(home_position) / cirumference);
        if (std::fabs(homeAngle - HomePositionN[0].value) > 0.001)
        {
            HomePositionN[0].value = homeAngle;
            IDSetNumber(&HomePositionNP, nullptr);
        }

        if (dead_zone != static_cast<uint32_t>(RotatorSettingsN[S_ZONE].value))
        {
            RotatorSettingsN[S_ZONE].value = dead_zone;
            IDSetNumber(&RotatorSettingsNP, nullptr);
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
        int32_t position = std::stoi(match.str(1));
        int32_t travel_limit = std::stoi(match.str(2));
        bool open_limit_switch = std::stoul(match.str(3)) == 1;
        bool close_limit_switch = std::stoul(match.str(4)) == 1;

        if (getShutterState() == SHUTTER_MOVING || getShutterState() == SHUTTER_UNKNOWN)
        {
            if (open_limit_switch || position == travel_limit)
            {
                setShutterState(SHUTTER_OPENED);
                LOG_INFO("Shutter is fully opened.");
            }
            else if (close_limit_switch || position < 0)
            {
                setShutterState(SHUTTER_CLOSED);
                LOG_INFO("Shutter is fully closed.");
            }
        }
        else if (open_limit_switch == 0 && close_limit_switch == 0 && getShutterState() != SHUTTER_UNKNOWN)
        {
            setShutterState(SHUTTER_UNKNOWN);
            LOG_WARN("Unknown shutter state. All limit switches are off.");
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
