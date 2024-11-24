/*
    MaxDome II
    Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)

    Migrated to INDI::Dome by Jasem Mutlaq (2014)

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

#include "config.h"
#include "maxdomeii.h"
#include "maxdomeiidriver.h"  // MaxDome II Command Set

#include <connectionplugins/connectionserial.h>

#include <memory>
#include <math.h>
#include <string.h>
#include <unistd.h>


// We declare an auto pointer to dome.
std::unique_ptr<MaxDomeII> dome(new MaxDomeII());

MaxDomeII::MaxDomeII()
{
    nTicksPerTurn               = 360;
    nCurrentTicks               = 0;
    nShutterOperationPosition   = 0.0;
    nHomeAzimuth                = 0.0;
    nHomeTicks                  = 0;
    nMoveDomeBeforeOperateShutter = 0;
    nTimeSinceShutterStart      = -1; // No movement has started
    nTimeSinceAzimuthStart      = -1; // No movement has started
    nTargetAzimuth              = -1; //Target azimuth not established
    nTimeSinceLastCommunication = 0;

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_HAS_SHUTTER | DOME_CAN_PARK);

    setVersion(INDI_MAXDOMEII_VERSION_MAJOR, INDI_MAXDOMEII_VERSION_MINOR);
}

bool MaxDomeII::SetupParms()
{
    DomeAbsPosNP[0].setValue(0);

    DomeAbsPosNP.apply();
    DomeParamNP.apply();
    
    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(180);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(0);
        SetAxis1ParkDefault(180);
    }

    return true;
}

bool MaxDomeII::Handshake()
{
    driver.SetDevice(getDeviceName());
    driver.SetPortFD(PortFD);

    return !driver.Ack();
}

MaxDomeII::~MaxDomeII()
{
    prev_az = prev_alt = 0;
}

const char *MaxDomeII::getDefaultName()
{
    return (char *)"MaxDome II";
}

bool MaxDomeII::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_AZ);
    
    HomeAzimuthNP[0].fill("HOME_AZIMUTH", "Home azimuth", "%5.2f", 0., 360., 0., nHomeAzimuth);
    HomeAzimuthNP.fill(getDeviceName(), "HOME_AZIMUTH", "Home azimuth", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Ticks per turn
    TicksPerTurnNP[0].fill("TICKS_PER_TURN", "Ticks per turn", "%5.2f", 100., 2000., 0., nTicksPerTurn);
    TicksPerTurnNP.fill(getDeviceName(), "TICKS_PER_TURN", "Ticks per turn", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Shutter operation position
    ShutterOperationAzimuthNP[0].fill("SOp_AZIMUTH", "Azimuth", "%5.2f", 0., 360., 0., nShutterOperationPosition);
    ShutterOperationAzimuthNP.fill(getDeviceName(), "SHUTTER_OPERATION_AZIMUTH", "Shutter operation azimuth", OPTIONS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Move to a shutter operation position before moving shutter?
    ShutterConflictSP[MOVE].fill("MOVE", "Move", ISS_ON);
    ShutterConflictSP[NO_MOVE].fill("NO_MOVE", "No move", ISS_OFF);
    ShutterConflictSP.fill(getDeviceName(),
                       "AZIMUTH_ON_SHUTTER", "Azimuth on operating shutter", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Shutter mode
    ShutterModeSP[FULL].fill("FULL", "Open full", ISS_ON);
    ShutterModeSP[UPPER].fill("UPPER", "Open upper only", ISS_OFF);
    ShutterModeSP.fill(getDeviceName(),
                       "SHUTTER_MODE", "Shutter open mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Home - Home command
    HomeSP[0].fill("HOME", "Home", ISS_OFF);
    HomeSP.fill(getDeviceName(), "HOME_MOTION", "Home dome", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Watch Dog
    WatchDogNP[0].fill("WATCH_DOG_TIME", "Watch dog time", "%5.2f", 0., 3600., 0., 0.);
    WatchDogNP.fill(getDeviceName(), "WATCH_DOG_TIME_SET",
                       "Watch dog time set", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Set default baud rate to 19200
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    return true;
}

bool MaxDomeII::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineProperty(HomeAzimuthNP);
        defineProperty(TicksPerTurnNP);
        defineProperty(ShutterOperationAzimuthNP);
        defineProperty(ShutterConflictSP);
        defineProperty(ShutterModeSP);
        defineProperty(HomeSP);
        defineProperty(WatchDogNP);

        SetupParms();
    }
    else
    {
        deleteProperty(HomeAzimuthNP);
        deleteProperty(TicksPerTurnNP);
        deleteProperty(ShutterOperationAzimuthNP);
        deleteProperty(ShutterConflictSP);
        deleteProperty(ShutterModeSP);
        deleteProperty(HomeSP);
        deleteProperty(WatchDogNP);
    }

    return true;
}

bool MaxDomeII::saveConfigItems(FILE *fp)
{
    HomeAzimuthNP.save(fp);
    TicksPerTurnNP.save(fp);
    ShutterOperationAzimuthNP.save(fp);
    ShutterConflictSP.save(fp);
    ShutterModeSP.save(fp);

    return INDI::Dome::saveConfigItems(fp);
}

bool MaxDomeII::Disconnect()
{
    driver.Disconnect();

    return INDI::Dome::Disconnect();
}

void MaxDomeII::TimerHit()
{
    ShStatus shutterSt;
    AzStatus nAzimuthStatus;
    unsigned nHomePosition;
    float nAz;
    int nError;
    int nRetry = 1;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    nError = driver.Status(&shutterSt, &nAzimuthStatus, &nCurrentTicks, &nHomePosition);
    handle_driver_error(&nError, &nRetry); // This is a timer, we will not repeat in order to not delay the execution.

    // Increment movment time counter
    if (nTimeSinceShutterStart >= 0)
        nTimeSinceShutterStart++;
    if (nTimeSinceAzimuthStart >= 0)
        nTimeSinceAzimuthStart++;

    // Watch dog
    nTimeSinceLastCommunication++;
    if (WatchDogNP[0].getValue() > 0 && WatchDogNP[0].getValue() <= nTimeSinceLastCommunication)
    {
        // Close Shutter if it is not
        if (shutterSt != SS_CLOSED)
        {
            DomeShutterSP.setState(ControlShutter(SHUTTER_CLOSE));
            LOG_INFO("Closing shutter due watch dog");
            DomeShutterSP.apply();
        }
    }

    //    if (getWeatherState() == IPS_ALERT)
    //    {
    //        // Close Shutter if it is not
    //        if (shutterSt != SS_CLOSED)
    //        {
    //            DomeShutterSP.s = ControlShutter(SHUTTER_CLOSE);
    //            IDSetSwitch(&DomeShutterSP, "Closing shutter due Weather conditions");
    //        }
    //    }

    if (!nError)
    {
        // Shutter
        switch (shutterSt)
        {
            case SS_CLOSED:
                if (DomeShutterSP[1].getState() == ISS_ON) // Close Shutter
                {
                    if (DomeShutterSP.getState() == IPS_BUSY || DomeShutterSP.getState() == IPS_ALERT)
                    {
                        DomeShutterSP.setState(IPS_OK); // Shutter close movement ends.
                        nTimeSinceShutterStart = -1;
                        LOG_INFO("Shutter is closed");
                        DomeShutterSP.apply();
                    }
                }
                else
                {
                    if (nTimeSinceShutterStart >= 0)
                    {
                        // A movement has started. Warn but don't change
                        if (nTimeSinceShutterStart >= 4)
                        {
                            DomeShutterSP.setState(IPS_ALERT); // Shutter close movement ends.
                            LOG_INFO("Shutter still closed");
                            DomeShutterSP.apply();
                        }
                    }
                    else
                    {
                        // For some reason (manual operation?) the shutter has closed
                        DomeShutterSP.setState(IPS_IDLE);
                        DomeShutterSP[1].setState(ISS_ON);
                        DomeShutterSP[0].setState(ISS_OFF);
                        LOG_ERROR("Unexpected shutter closed");
                        DomeShutterSP.apply();
                    }
                }
                break;
            case SS_OPENING:
                if (DomeShutterSP[0].getState() == ISS_OFF) // not opening Shutter
                {
                    // For some reason the shutter is opening (manual operation?)
                    DomeShutterSP.setState(IPS_ALERT);
                    DomeShutterSP[1].setState(ISS_OFF);
                    DomeShutterSP[0].setState(ISS_OFF);
                    LOG_INFO("Unexpected shutter opening");
                    DomeShutterSP.apply();
                }
                else if (nTimeSinceShutterStart < 0)
                {
                    // For some reason the shutter is opening (manual operation?)
                    DomeShutterSP.setState(IPS_ALERT);
                    nTimeSinceShutterStart = 0;
                    LOG_INFO("Unexpected shutter opening");
                    DomeShutterSP.apply();
                }
                else if (DomeShutterSP.getState() == IPS_ALERT)
                {
                    // The alert has corrected
                    DomeShutterSP.setState(IPS_BUSY);
                    LOG_INFO("Shutter is opening");
                    DomeShutterSP.apply();
                }
                break;
            case SS_OPEN:
                if (DomeShutterSP[0].getState() == ISS_ON) // Open Shutter
                {
                    if (DomeShutterSP.getState() == IPS_BUSY || DomeShutterSP.getState() == IPS_ALERT)
                    {
                        DomeShutterSP.setState(IPS_OK); // Shutter open movement ends.
                        nTimeSinceShutterStart = -1;
                        DomeShutterSP.apply();
                    }
                }
                else
                {
                    if (nTimeSinceShutterStart >= 0)
                    {
                        // A movement has started. Warn but don't change
                        if (nTimeSinceShutterStart >= 4)
                        {
                            DomeShutterSP.setState(IPS_ALERT); // Shutter open movement alert.
                            LOG_INFO("Shutter still open");
                            DomeShutterSP.apply();
                        }
                    }
                    else
                    {
                        // For some reason (manual operation?) the shutter has open
                        DomeShutterSP.setState(IPS_IDLE);
                        DomeShutterSP[1].setState(ISS_ON);
                        DomeShutterSP[0].setState(ISS_OFF);
                        LOG_INFO("Unexpected shutter open");
                        DomeShutterSP.apply();
                    }
                }
                break;
            case SS_CLOSING:
                if (DomeShutterSP[1].getState() == ISS_OFF) // Not closing Shutter
                {
                    // For some reason the shutter is closing (manual operation?)
                    DomeShutterSP.setState(IPS_ALERT);
                    DomeShutterSP[1].setState(ISS_ON);
                    DomeShutterSP[0].setState(ISS_OFF);
                    LOG_INFO("Unexpected shutter closing");
                    DomeShutterSP.apply();
                }
                else if (nTimeSinceShutterStart < 0)
                {
                    // For some reason the shutter is opening (manual operation?)
                    DomeShutterSP.setState(IPS_ALERT);
                    nTimeSinceShutterStart = 0;
                    LOG_INFO("Unexpected shutter closing");
                    DomeShutterSP.apply();
                }
                else if (DomeShutterSP.getState() == IPS_ALERT)
                {
                    // The alert has corrected
                    DomeShutterSP.setState(IPS_BUSY);
                    LOG_INFO("Shutter is closing");
                    DomeShutterSP.apply();
                }
                break;
            case SS_ERROR:
                DomeShutterSP.setState(IPS_ALERT);
                DomeShutterSP[1].setState(ISS_OFF);
;
                DomeShutterSP[0].setState(ISS_OFF);
                LOG_ERROR("Shutter error");
                DomeShutterSP.apply();
                break;
            case SS_ABORTED:
            default:
                if (nTimeSinceShutterStart >= 0)
                {
                    DomeShutterSP.setState(IPS_ALERT); // Shutter movement aborted.
                    DomeShutterSP[1].setState(ISS_OFF);
                    DomeShutterSP[0].setState(ISS_OFF);
                    nTimeSinceShutterStart = -1;
                    LOG_ERROR("Unknown shutter status");
                    DomeShutterSP.apply();
                }
                break;
        }

        // Azimuth
        nAz = TicksToAzimuth(nCurrentTicks);
        if (DomeAbsPosNP[0].getValue() != nAz)
        {
            // Only refresh position if it changed
            DomeAbsPosNP[0].setValue(nAz);
            //sprintf(buf,"%d", nCurrentTicks);
            DomeAbsPosNP.apply();
        }

        switch (nAzimuthStatus)
        {
            case AS_IDLE:
            case AS_IDLE2:
                if (nTimeSinceAzimuthStart > 3)
                {
                    if (nTargetAzimuth >= 0 &&
                            AzimuthDistance(nTargetAzimuth, nCurrentTicks) > 3) // Maximum difference allowed: 3 ticks
                    {
                        DomeAbsPosNP.setState(IPS_ALERT);
                        nTimeSinceAzimuthStart = -1;
                        LOG_ERROR("Could not position right");
                        DomeAbsPosNP.apply();
                    }
                    else
                    {
                        // Succesfull end of movement
                        if (DomeAbsPosNP.getState() != IPS_OK)
                        {
                            setDomeState(DOME_SYNCED);
                            nTimeSinceAzimuthStart = -1;
                            LOG_INFO("Dome is on target position");
                        }
                        if (HomeSP[0].getState() == ISS_ON)
                        {
                            HomeSP[0].setState(ISS_OFF);
                            HomeSP.setState(IPS_OK);
                            nTimeSinceAzimuthStart = -1;
                            LOG_INFO("Dome is homed");
                            HomeSP.apply();
                        }
                        if (ParkSP.getState() != IPS_OK)
                        {
                            if (ParkSP[0].getState() == ISS_ON)
                            {
                                SetParked(true);
                            }
                            if (ParkSP[1].getState() == ISS_ON)
                            {
                                SetParked(false);
                            }
                        }
                    }
                }
                break;
            case AS_MOVING_WE:
            case AS_MOVING_EW:
                if (nTimeSinceAzimuthStart < 0)
                {
                    nTimeSinceAzimuthStart = 0;
                    nTargetAzimuth         = -1;
                    DomeAbsPosNP.setState(IPS_ALERT);
                    LOG_INFO("Unexpected dome moving");
                    DomeAbsPosNP.apply();
                }
                break;
            case AS_ERROR:
                if (nTimeSinceAzimuthStart >= 0)
                {
                    DomeAbsPosNP.setState(IPS_ALERT);
                    nTimeSinceAzimuthStart = -1;
                    nTargetAzimuth         = -1;
                    LOG_ERROR("Dome Error");
                    DomeAbsPosNP.apply();
                }
            default:
                break;
        }
    }
    else
    {
        LOGF_DEBUG("Error: %s. Please reconnect and try again.", ErrorMessages[-nError]);
        return;
    }

    SetTimer(getCurrentPollingPeriod());
    return;
}

IPState MaxDomeII::MoveAbs(double newAZ)
{
    double currAZ = 0;
    int newPos = 0, nDir = 0;
    int error;
    int nRetry = 3;

    currAZ = DomeAbsPosNP[0].getValue();

    // Take the shortest path
    if (newAZ > currAZ)
    {
        if (newAZ - currAZ > 180.0)
            nDir = MAXDOMEII_WE_DIR;
        else
            nDir = MAXDOMEII_EW_DIR;
    }
    else
    {
        if (currAZ - newAZ > 180.0)
            nDir = MAXDOMEII_EW_DIR;
        else
            nDir = MAXDOMEII_WE_DIR;
    }

    newPos = AzimuthToTicks(newAZ);

    while (nRetry)
    {
        error = driver.GotoAzimuth(nDir, newPos);
        handle_driver_error(&error, &nRetry);
    }

    if (error != 0)
        return IPS_ALERT;

    nTargetAzimuth = newPos;
    nTimeSinceAzimuthStart = 0; // Init movement timer

    // It will take a few cycles to reach final position
    return IPS_BUSY;
}

IPState MaxDomeII::Move(DomeDirection dir, DomeMotionCommand operation)
{
    int error;
    int nRetry = 3;

    if (operation == MOTION_START)
    {
        LOGF_DEBUG("Move dir=%d", dir);
        double currAZ = DomeAbsPosNP[0].getValue();
        double newAZ = currAZ > 180 ? currAZ - 180 : currAZ + 180;
        int newPos = AzimuthToTicks(newAZ);
        int nDir = dir ? MAXDOMEII_WE_DIR : MAXDOMEII_EW_DIR;

        while (nRetry)
        {
            error = driver.GotoAzimuth(nDir, newPos);
            handle_driver_error(&error, &nRetry);
        }

        if (error != 0)
            return IPS_ALERT;

        nTargetAzimuth = newPos;
        nTimeSinceAzimuthStart = 0; // Init movement timer
        return IPS_BUSY;
    }
    else
    {
        LOG_DEBUG("Stop movement");
        while (nRetry)
        {
            error = driver.AbortAzimuth();
            handle_driver_error(&error, &nRetry);
        }

        if (error != 0)
            return IPS_ALERT;

        DomeAbsPosNP.setState(IPS_IDLE);
        DomeAbsPosNP.apply();
        nTimeSinceAzimuthStart = -1;
    }

    return IPS_OK;
}

bool MaxDomeII::Abort()
{
    int error  = 0;
    int nRetry = 3;

    while (nRetry)
    {
        error = driver.AbortAzimuth();
        handle_driver_error(&error, &nRetry);
    }

    while (nRetry)
    {
        error = driver.AbortShutter();
        handle_driver_error(&error, &nRetry);
    }

    DomeAbsPosNP.setState(IPS_IDLE);
    DomeAbsPosNP.apply();

    // If we abort while in the middle of opening/closing shutter, alert.
    if (DomeShutterSP.getState() == IPS_BUSY)
    {
        DomeShutterSP.setState(IPS_ALERT);
        LOG_INFO("Shutter operation aborted.");
        DomeShutterSP.apply();
        return false;
    }

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool MaxDomeII::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Ignore if not ours
    if (strcmp(dev, getDeviceName()))
        return false;

    nTimeSinceLastCommunication = 0;

    // ===================================
    // TicksPerTurn
    // ===================================
    if (TicksPerTurnNP.isNameMatch(name))
    {
        double nVal;
        char cLog[255];
        int error;
        int nRetry = 3;

        if (TicksPerTurnNP.update(values, names, n) == false)
            return false;

        nVal = values[0];
        if (nVal >= 100 && nVal <= 2000)
        {
            while (nRetry)
            {
                error = driver.SetTicksPerTurn(nVal);
                handle_driver_error(&error, &nRetry);
            }
            if (error >= 0)
            {
                sprintf(cLog, "New Ticks Per Turn set: %lf", nVal);
                nTicksPerTurn    = nVal;
                nHomeTicks       = floor(0.5 + nHomeAzimuth * nTicksPerTurn / 360.0); // Calculate Home ticks again
                TicksPerTurnNP.setState(IPS_OK);
                TicksPerTurnNP[0].setValue(nVal);
                LOGF_INFO("%s", cLog);
                TicksPerTurnNP.apply();
                return true;
            }
            else
            {
                LOGF_ERROR("MAX DOME II: %s", ErrorMessages[-error]);
                TicksPerTurnNP.setState(IPS_ALERT);
                TicksPerTurnNP.apply();
            }

            return false;
        }

        // Incorrect value.
        TicksPerTurnNP.setState(IPS_ALERT);
        LOG_ERROR("Invalid Ticks Per Turn");
        TicksPerTurnNP.apply();

        return false;
    }

    // ===================================
    // HomeAzimuth
    // ===================================
    if (HomeAzimuthNP.isNameMatch(name))
    {
        double nVal;
        char cLog[255];

        if (HomeAzimuthNP.update(values, names, n) == false)
            return false;

        nVal = values[0];
        if (nVal >= 0 && nVal <= 360)
        {
            sprintf(cLog, "New home azimuth set: %lf", nVal);
            nHomeAzimuth              = nVal;
            nHomeTicks                = floor(0.5 + nHomeAzimuth * nTicksPerTurn / 360.0);
            HomeAzimuthNP.setState(IPS_OK);
            HomeAzimuthNP[0].setValue(nVal);
            LOGF_INFO("%s", cLog);
            HomeAzimuthNP.apply();
            return true;
        }
        // Incorrect value.
        HomeAzimuthNP.setState(IPS_ALERT);
        LOG_ERROR("Invalid home azimuth");
        HomeAzimuthNP.apply();

        return false;
    }

    // ===================================
    // Watch dog
    // ===================================
    if (WatchDogNP.isNameMatch(name))
    {
        double nVal;
        char cLog[255];

        if (WatchDogNP.update(values, names, n) == false)
            return false;

        nVal = values[0];
        if (nVal >= 0 && nVal <= 3600)
        {
            sprintf(cLog, "New watch dog set: %lf", nVal);
            WatchDogNP.setState(IPS_OK);
            WatchDogNP[0].setValue(nVal);
            LOGF_INFO("%s", cLog);
            WatchDogNP.apply();
            return true;
        }
        // Incorrect value.
        WatchDogNP.setState(IPS_ALERT);
        LOG_ERROR("Invalid watch dog time");
        WatchDogNP.apply();

        return false;
    }

    // ===================================
    // Shutter operation azimuth
    // ===================================
    if (ShutterOperationAzimuthNP.isNameMatch(name))
    {
        double nVal;
        IPState error;

        if (ShutterOperationAzimuthNP.update(values, names, n) == false)
            return false;

        nVal = values[0];
        if (nVal >= 0 && nVal < 360)
        {
            error = ConfigureShutterOperation(nMoveDomeBeforeOperateShutter, nVal);

            if (error == IPS_OK)
            {
                nShutterOperationPosition             = nVal;
                ShutterOperationAzimuthNP.setState(IPS_OK);
                ShutterOperationAzimuthNP[0].setValue(nVal);
                LOG_INFO("New shutter operation azimuth set");
                ShutterOperationAzimuthNP.apply();
            }
            else
            {
                ShutterOperationAzimuthNP.setState(IPS_ALERT);
                LOGF_ERROR("%s", ErrorMessages[-error]);
                ShutterOperationAzimuthNP.apply();
            }

            return true;
        }
        // Incorrect value.
        ShutterOperationAzimuthNP.setState(IPS_ALERT);
        LOG_ERROR("Invalid shutter operation azimuth position");
        ShutterOperationAzimuthNP.apply();

        return false;
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool MaxDomeII::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // ignore if not ours //
    if (strcmp(getDeviceName(), dev))
        return false;

    nTimeSinceLastCommunication = 0;

    // ===================================
    // Home
    // ===================================
    if (HomeSP.isNameMatch(name))
    {
        if (HomeSP.update(states, names, n) == false)
            return false;

        int error;
        int nRetry = 3;

        while (nRetry)
        {
            error = driver.HomeAzimuth();
            handle_driver_error(&error, &nRetry);
        }
        nTimeSinceAzimuthStart = 0;
        nTargetAzimuth         = -1;
        if (error)
        {
            LOGF_ERROR("Error Homing Azimuth (%s).", ErrorMessages[-error]);
            HomeSP.setState(IPS_ALERT);
            HomeSP.apply();
            return false;
        }
        HomeSP.setState(IPS_BUSY);
        LOG_INFO("Homing dome");
        HomeSP.apply();

        return true;
    }

    // ===================================
    // Conflict on Shutter operation
    // ===================================
    if (ShutterConflictSP.isNameMatch(name))
    {
        if (ShutterConflictSP.update(states, names, n) == false)
            return false;

        int nCSBP = ShutterConflictSP[MOVE].getState() == ISS_ON ? 1 : 0;
        int error = ConfigureShutterOperation(nCSBP, nShutterOperationPosition);

        if (error == IPS_OK)
        {
            ShutterConflictSP.setState(IPS_OK);
            LOG_INFO("New shutter operation conflict set");
            ShutterConflictSP.apply();
        }
        else
        {
            ShutterConflictSP.setState(IPS_ALERT);
            LOGF_ERROR("%s", ErrorMessages[-error]);
            ShutterConflictSP.apply();
        }
        return true;
    }

    if (ShutterModeSP.isNameMatch(name))
    {
        if (ShutterModeSP.update(states, names, n) == false)
            return false;

        ShutterModeSP.setState(IPS_OK);
        LOG_INFO("Shutter opening mode set");
        ShutterModeSP.apply();
        return true;
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
int MaxDomeII::AzimuthDistance(int nPos1, int nPos2)
{
    int nDif = std::abs(nPos1 - nPos2);

    if (nDif > nTicksPerTurn / 2)
        nDif = nTicksPerTurn - nDif;

    return nDif;
}

/**************************************************************************************
 **
 ***************************************************************************************/
double MaxDomeII::TicksToAzimuth(int nTicks)
{
    double nAz;

    nAz = nHomeAzimuth + nTicks * 360.0 / nTicksPerTurn;
    while (nAz < 0)
        nAz += 360;
    while (nAz >= 360)
        nAz -= 360;

    return nAz;
}
/**************************************************************************************
 **
 ***************************************************************************************/
int MaxDomeII::AzimuthToTicks(double nAzimuth)
{
    int nTicks;

    nTicks = floor(0.5 + (nAzimuth - nHomeAzimuth) * nTicksPerTurn / 360.0);
    while (nTicks > nTicksPerTurn)
        nTicks -= nTicksPerTurn;
    while (nTicks < 0)
        nTicks += nTicksPerTurn;

    return nTicks;
}

/**************************************************************************************
 **
 ***************************************************************************************/

int MaxDomeII::handle_driver_error(int *error, int *nRetry)
{
    (*nRetry)--;
    switch (*error)
    {
        case 0: // No error. Continue
            *nRetry = 0;
            break;
        case -5: // can't connect
            // This error can happen if port connection is lost, i.e. a usb-serial reconnection
            // Reconnect
            LOG_ERROR("MAX DOME II: Reconnecting ...");
            Connect();
            if (PortFD < 0)
                *nRetry = 0; // Can't open the port. Don't retry anymore.
            break;

        default: // Do nothing in all other errors.
            LOGF_ERROR("Error on command: (%s).", ErrorMessages[-*error]);
            break;
    }

    return *nRetry;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState MaxDomeII::ConfigureShutterOperation(int nMDBOS, double ShutterOperationAzimuth)
{
    int error  = 0;
    int nRetry = 3;

    // Only update shutter operating position if there is change
    if (ShutterOperationAzimuth != nShutterOperationPosition || nMDBOS != nMoveDomeBeforeOperateShutter)
    {
        while (nRetry)
        {
            error = driver.SetPark(nMDBOS, AzimuthToTicks(ShutterOperationAzimuth));
            handle_driver_error(&error, &nRetry);
        }
        if (error >= 0)
        {
            nShutterOperationPosition = ShutterOperationAzimuth;
            nMoveDomeBeforeOperateShutter = nMDBOS;
            LOGF_INFO("New shutter operatig position set. %d %d", nMDBOS, AzimuthToTicks(ShutterOperationAzimuth));
        }
        else
        {
            LOGF_ERROR("MAX DOME II: %s", ErrorMessages[-error]);
            return IPS_ALERT;
        }
    }

    return IPS_OK;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool MaxDomeII::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosNP[0].getValue());
    return true;
}
/************************************************************************************
 *
* ***********************************************************************************/

bool MaxDomeII::SetDefaultPark()
{
    // By default set position to 0
    SetAxis1Park(0);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState MaxDomeII::ControlShutter(ShutterOperation operation)
{
    int error  = 0;
    int nRetry = 3;

    if (operation == SHUTTER_CLOSE)
    {
        while (nRetry)
        {
            error = driver.CloseShutter();
            handle_driver_error(&error, &nRetry);
        }
        nTimeSinceShutterStart = 0; // Init movement timer
        if (error)
        {
            LOGF_ERROR("Error closing shutter (%s).", ErrorMessages[-error]);
            return IPS_ALERT;
        }
        return IPS_BUSY;
    }
    else
    {
        if (ShutterModeSP[MOVE].getState() == ISS_ON)
        {
            // Open Shutter
            while (nRetry)
            {
                error = driver.OpenShutter();
                handle_driver_error(&error, &nRetry);
            }
            nTimeSinceShutterStart = 0; // Init movement timer
            if (error)
            {
                LOGF_ERROR("Error opening shutter (%s).", ErrorMessages[-error]);
                return IPS_ALERT;
            }
            return IPS_BUSY;
        }
        else
        {
            // Open upper shutter only
            while (nRetry)
            {
                error = driver.OpenUpperShutterOnly();
                handle_driver_error(&error, &nRetry);
            }
            nTimeSinceShutterStart = 0; // Init movement timer
            if (error)
            {
                LOGF_ERROR("Error opening upper shutter only (%s).", ErrorMessages[-error]);
                return IPS_ALERT;
            }
            return IPS_BUSY;
        }
    }

    return IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState MaxDomeII::Park()
{
    int targetAz = GetAxis1Park();
    
    LOGF_INFO("Parking to %.2f azimuth...", targetAz);
    MoveAbs(targetAz);

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

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState MaxDomeII::UnPark()
{
    int error;
    int nRetry = 3;

    while (nRetry)
    {
        error = driver.HomeAzimuth();
        handle_driver_error(&error, &nRetry);
    }
    nTimeSinceAzimuthStart = 0;
    nTargetAzimuth         = -1;
    
    if (HasShutter() && ShutterParkPolicySP[SHUTTER_OPEN_ON_UNPARK].getState() == ISS_ON)
    {
        LOG_INFO("Opening shutter on unparking...");
        ControlShutter(ShutterOperation::SHUTTER_OPEN);
        DomeShutterSP[SHUTTER_OPEN].setState(ISS_ON);
        DomeShutterSP[SHUTTER_CLOSE].setState(ISS_OFF);
        setShutterState(SHUTTER_MOVING);
    }
    return IPS_BUSY;
}
