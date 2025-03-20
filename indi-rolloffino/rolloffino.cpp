/*
 Edited version of the Dome Simulator
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.
 Copyright(c) 2023 Jasem Mutlaq. All rights reserved.
 Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Uses a simple text string protocol to send messages to an Arduino. The Arduino code
 * determines how the open/close commands are enacted. Might use relays, linear actuators
 * or variable speed motors. Stopping roof movement is the responsibilty of the Arduino or
 * controllers that it in turn uses.
 */

#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <regex>
#include "rolloffino.h"

// We declare an auto pointer to RollOffIno.
static std::unique_ptr<RollOffIno> rollOffIno(new RollOffIno());

void ISSnoopDevice(XMLEle *root)
{
    rollOffIno->ISSnoopDevice(root);
}

bool RollOffIno::ISSnoopDevice(XMLEle *root)
{
    return INDI::Dome::ISSnoopDevice(root);
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
RollOffIno::RollOffIno() : INDI::InputInterface(this), INDI::OutputInterface(this)
{
    setVersion(1, 0);
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);           // Need the DOME_CAN_PARK capability for the scheduler
}

////////////////////////////////////////////////////////////////////////////////////////
// INDI is asking us for our default device name.
// Check that it matches Ekos selection menu and ParkData.xml names
////////////////////////////////////////////////////////////////////////////////////////
const char *RollOffIno::getDefaultName()
{
    return (const char *)"RollOff ino";
}

////////////////////////////////////////////////////////////////////////////////////////
// INDI request to init properties. Connected Define properties to Ekos
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::initProperties()
{
    INDI::Dome::initProperties();

    // Main tab
    LockSP[LOCK_ENABLE].fill("LOCK_ENABLE", "On", ISS_OFF);
    LockSP[LOCK_DISABLE].fill("LOCK_DISABLE", "Off", ISS_ON);
    LockSP.fill(getDeviceName(), "LOCK", "Lock", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    AuxSP[AUX_ENABLE].fill("AUX_ENABLE",   "On",  ISS_OFF);
    AuxSP[AUX_DISABLE].fill("AUX_DISABLE",  "Off", ISS_ON);
    AuxSP.fill(getDeviceName(), "AUX",  "Auxiliary", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //Roof status lights
    RoofStatusLP[0].fill("ROOF_OPENED", "Opened", IPS_IDLE);
    RoofStatusLP[1].fill("ROOF_CLOSED", "Closed", IPS_IDLE);
    RoofStatusLP[2].fill("ROOF_MOVING", "Moving", IPS_IDLE);
    RoofStatusLP[3].fill("ROOF_LOCK",   "Roof Lock", IPS_IDLE);
    RoofStatusLP[4].fill("ROOF_AUXILIARY", "Roof Auxiliary", IPS_IDLE);
    RoofStatusLP.fill(getDeviceName(), "ROOF STATUS","Roof Status", MAIN_CONTROL_TAB, IPS_OK);

    RoofTimeoutNP[0].fill("ROOF_TIMEOUT", "Timeout in Seconds", "%3.0f", 1, 300, 1, 120);
    RoofTimeoutNP.fill(getDeviceName(), "ROOF_MOVEMENT", "Roof Movement", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    SetParkDataType(PARK_NONE);
    loadConfig(true);
    setDefaultPollingPeriod(POLLING_PERIOD);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// INDI request to update the properties because there is a change in CONNECTION status
// This function is called whenever the device is connected or disconnected.
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::updateProperties()
{
    INDI::Dome::updateProperties();
    if (isConnected())
    {
        if (!InitPark())
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Dome parking data was not obtained");
        }
        defineProperty(LockSP);              // Lock Switch,
        defineProperty(AuxSP);               // Aux Switch,
        defineProperty(RoofStatusLP);        // Roof status lights
        defineProperty(RoofTimeoutNP);       // Roof timeout
        INDI::OutputInterface::updateProperties();
        INDI::InputInterface::updateProperties();
        checkConditions();
    }
    else
    {
        deleteProperty(LockSP);              // Delete the Lock Switch buttons
        deleteProperty(AuxSP);               // Delete the Auxiliary Switch buttons
        deleteProperty(RoofStatusLP);        // Delete the roof status lights
        deleteProperty(RoofTimeoutNP);       // Roof timeout
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void RollOffIno::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);
    defineProperty(LockSP);        // Lock Switch,
    defineProperty(AuxSP);         // Aux Switch,
    defineProperty(RoofTimeoutNP); // Roof timeout
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);
    LockSP.save(fp);
    AuxSP.save(fp);
    RoofTimeoutNP.save(fp);
    INDI::InputInterface::saveConfigItems(fp);
    INDI::OutputInterface::saveConfigItems(fp);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Client is asking us to establish connection to the device
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::Connect()
{
    bool status = INDI::Dome::Connect();
    INDI::InputInterface::initProperties("Inputs", actionCount, 0, "Input");
    INDI::OutputInterface::initProperties("Outputs", actionCount, "Output");
    setDriverInterface(AUX_INTERFACE | INPUT_INTERFACE | OUTPUT_INTERFACE);
    roofMoveTimer.stop();
    roofMoveTimer.callOnTimeout(std::bind(&RollOffIno::roofTimerExpired, this));
    return status;
}

////////////////////////////////////////////////////////////////////////////////////////
// Client is asking us to terminate connection to the device
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::Disconnect()
{
    bool status = INDI::Dome::Disconnect();
    roofMoveTimer.stop();
    return status;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (RoofTimeoutNP.isNameMatch(name))
        {
            RoofTimeoutNP.update(values, names, n);
            RoofTimeoutNP.setState(IPS_OK);
            RoofTimeoutNP.apply();
            return true;
        }
    }
    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (INDI::OutputInterface::processText(dev, name, texts, names, n))
        return true;
    if (INDI::InputInterface::processText(dev, name, texts, names, n))
        return true;
    return INDI::Dome::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////
// Client has changed the state of a switch, update
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Make sure the call is for our device
    if(dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LockSP.isNameMatch(name))
        {
            auto previous = LockSP.findOnSwitchIndex();
            LockSP.update(states, names, n);
            auto requested = LockSP.findOnSwitchIndex();
            if (requested < 0)
                return true;
            if (previous == requested)
            {
                LockSP.reset();
                LockSP.setState(IPS_OK);
                LockSP[previous].setState(ISS_ON);
                LockSP.apply();
                return true;
            }
            if (requested == 0)
            {
                LockSP.setState(IPS_OK);
                LockSP[requested].s = ISS_ON;
                LockSP.apply();
                sendRoofCommand(ROOF_LOCK_CMD, true, true);
                updateRoofStatus();
                return true;
            }
            else
            {
                LockSP.setState(IPS_IDLE);
                LockSP[requested].s = ISS_OFF;
                LockSP.apply();
                sendRoofCommand(ROOF_LOCK_CMD, false, true);
                updateRoofStatus();
                return true;
            }
        }

        if (AuxSP.isNameMatch(name))
        {
            auto previous = AuxSP.findOnSwitchIndex();
            AuxSP.update(states, names, n);
            auto requested = AuxSP.findOnSwitchIndex();
            if (requested < 0)
                return true;
            if (previous == requested)
            {
                AuxSP.reset();
                AuxSP.setState(IPS_OK);
                AuxSP[previous].setState(ISS_ON);
                AuxSP.apply();
                return true;
            }
            if (requested == 0)
            {
                AuxSP.setState(IPS_OK);
                //    AuxSP.reset();
                AuxSP[requested].s = ISS_ON;
                AuxSP.apply();
                sendRoofCommand(ROOF_AUX_CMD, true, true);
                updateRoofStatus();
                return true;
            }
            else
            {
                AuxSP.setState(IPS_IDLE);
                AuxSP[requested].s = ISS_OFF;
                sendRoofCommand(ROOF_AUX_CMD, false, true);
                AuxSP.apply();
                updateRoofStatus();
                return true;
            }
        }
    }
    if (INDI::OutputInterface::processSwitch(dev, name, states, names, n))
        return true;
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}


////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void RollOffIno::roofTimerExpired()
{
    roofMoveTimer.stop();
    setDomeState(DOME_IDLE);
    SetParked(false);

   if (roofOpening)
   {
       LOG_ERROR("Time allowed for opening the roof has expired");
       roofOpening = false;
       roofTimedOut = EXPIRED_OPEN;
   }
   else if (roofClosing)
   {
       LOG_ERROR("Time allowed for closing the roof has expired");
       roofClosing = false;
       roofTimedOut = EXPIRED_CLOSE;
   }
    LOG_INFO("Does the Timeout setting in the Options tab need extending?");
}

////////////////////////////////////////////////////////////////////////////////////////
// Each 1 second timer tick, if roof active
////////////////////////////////////////////////////////////////////////////////////////
void RollOffIno::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    uint32_t delay = getPollingPeriod();
    updateRoofStatus();
    if (DomeMotionSP.getState() == IPS_BUSY)
    {
        // Abort called to stop movement.
        if (roofTimedOut == EXPIRED_ABORT)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof motion is stopped");
            setDomeState(DOME_IDLE);
            roofTimedOut = EXPIRED_CLEAR;
            roofMoveTimer.stop();
        }
        else
        {
            delay = 1000;
            // Roll off is opening
            if (DomeMotionSP[DOME_CW].getState() == ISS_ON)
            {
                if (fullyOpenedLimitSwitch == ISS_ON)
                {
                    DEBUG(INDI::Logger::DBG_DEBUG, "Roof is open");
                    SetParked(false);
                    roofMoveTimer.stop();
                }
            }
            // Roll Off is closing
            else if (DomeMotionSP[DOME_CCW].getState() == ISS_ON)
            {
                if (fullyClosedLimitSwitch == ISS_ON)
                {
                    DEBUG(INDI::Logger::DBG_DEBUG, "Roof is closed");
                    SetParked(true);
                    roofMoveTimer.stop();
                }
            }
        }
    }
    else
    {
        checkConditions();  // In case external / manually moved
    }

    // Added to highlight WiFi issues, not able to recover lost connection without a reconnect
    if (communicationErrors >= MAX_CNTRL_COM_ERR)
    {
        LOG_ERROR("Too many errors communicating with Arduino");
        LOG_ERROR("Try a fresh connect. Check communication equipment and operation of Arduino controller.");
    }
    UpdateDigitalInputs();
    UpdateDigitalOutputs();
    SetTimer(delay);
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void RollOffIno::msSleep (int mSec)
{
    struct timespec req = {0, 0};
    req.tv_sec = 0;
    req.tv_nsec = mSec * 1000000L;
    nanosleep(&req, (struct timespec *)nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////
// Establish conditions on a connect.
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::checkConditions()
{
    updateRoofStatus();
    Dome::DomeState curState = getDomeState();

    // If the roof is clearly fully opened or fully closed, set the Dome::IsParked status to match.
    // Otherwise if Dome:: park status different from roof status, o/p message (the roof might be or need to be operating manually)
    // If park status is the same, and Dome:: state does not match, change the Dome:: state.
    if (isParked())
    {
        if (fullyOpenedLimitSwitch == ISS_ON)
        {
            SetParked(false);
        }
        else if (fullyClosedLimitSwitch == ISS_OFF)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Dome indicates it is parked but roof closed switch not set, manual intervention needed");
        }
        else
        {
            // When Dome status agrees with roof status (closed), but Dome state differs, set Dome state to parked
            if (curState != DOME_PARKED)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Setting Dome state to DOME_PARKED.");
                setDomeState(DOME_PARKED);
            }
        }
    }
    else
    {
        if (fullyClosedLimitSwitch == ISS_ON)
        {
            SetParked(true);
        }
        else if (fullyOpenedLimitSwitch == ISS_OFF)
        {
            DEBUG(INDI::Logger::DBG_WARNING,
                  "Dome indicates it is unparked but roof open switch is not set, manual intervention needed");
        }
        else
            // When Dome status agrees with roof status (open), but Dome state differs, set Dome state to unparked
        {
            if (curState != DOME_UNPARKED)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Setting Dome state to DOME_UNPARKED.");
                setDomeState(DOME_UNPARKED);
            }
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Direction: DOME_CW Clockwise = Open; DOME-CCW Counter clockwise = Close
// Operation: MOTION_START, | MOTION_STOP
////////////////////////////////////////////////////////////////////////////////////////
IPState RollOffIno::Move(DomeDirection dir, DomeMotionCommand operation)
{
    updateRoofStatus();
    if (operation == MOTION_START)
    {
        if (roofLockedSwitch)
        {
            LOG_WARN("Roof is externally locked, no movement possible");
            return IPS_ALERT;
        }
        if (roofOpening)
        {
            LOG_WARN("Roof is in process of opening, wait for completion.");
            return IPS_OK;
        }
        if (roofClosing)
        {
            LOG_WARN("Roof is in process of closing, wait for completion.");
            return IPS_OK;
        }

        // Open Roof
        // DOME_CW --> OPEN. If we are asked to "open" while we are fully opened as the
        // limit switch indicates, then we simply return false.
        if (dir == DOME_CW)
        {
            if (fullyOpenedLimitSwitch == ISS_ON)
            {
                LOG_WARN("DOME_CW directive received but roof is already fully opened");
                SetParked(false);
                return IPS_IDLE;
            }

            // Initiate action
            if (sendRoofCommand(ROOF_OPEN_CMD, true, false))
            {
                roofOpening = true;
                roofClosing = false;
                LOG_INFO("Roof is opening...");
            }
            else
            {
                LOG_WARN("Failed to operate controller to open roof");
                return IPS_ALERT;
            }
        }

        // Close Roof
        else if (dir == DOME_CCW)
        {
            if (fullyClosedLimitSwitch == ISS_ON)
            {
                SetParked(true);
                LOG_WARN("DOME_CCW directive received but roof is already fully closed");
                return IPS_IDLE;
            }
            else if (INDI::Dome::isLocked())
            {
                DEBUG(INDI::Logger::DBG_WARNING,
                      "Cannot close dome when mount is locking. See: Telescope parkng policy, in options tab");
                return IPS_ALERT;
            }
            // Initiate action
            if (sendRoofCommand(ROOF_CLOSE_CMD, true, false))
            {
                roofClosing = true;
                roofOpening = false;
                LOG_INFO("Roof is closing...");
            }
            else
            {
                LOG_WARN("Failed to operate controller to close roof");
                return IPS_ALERT;
            }
        }

        // Roof is moving
        roofTimedOut = EXPIRED_CLEAR;
        auto timeOut = (int)RoofTimeoutNP[0].getValue();
        roofMoveTimer.start(timeOut* 1000);
        LOGF_DEBUG("Roof motion timeout setting: %d", (int)timeOut);
        return IPS_BUSY;
    }
    return    IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////
// Close Roof
////////////////////////////////////////////////////////////////////////////////////////
IPState RollOffIno::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);

    if (rc == IPS_BUSY)
    {
        LOG_INFO("RollOff ino is parking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////
// Open Roof
////////////////////////////////////////////////////////////////////////////////////////
IPState RollOffIno::UnPark()
{
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("RollOff ino is unparking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////
// Abort motion
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::Abort()
{
    bool lockState;
    bool openState;
    bool closeState;

    updateRoofStatus();
    lockState = (roofLockedSwitch == ISS_ON);
    openState = (fullyOpenedLimitSwitch == ISS_ON);
    closeState = (fullyClosedLimitSwitch == ISS_ON);

    if (lockState)
    {
        LOG_WARN("Roof is externally locked, no action taken on abort request");
        return true;
    }

    if (closeState)
    {
        LOG_WARN("Roof appears to be closed and stationary, no action taken on abort request");
        return true;
    }
    else if (openState)
    {
        LOG_WARN("Roof appears to be open and stationary, no action taken on abort request");
        return true;
    }
    else if (DomeMotionSP.getState() != IPS_BUSY)
    {
        LOG_WARN("Dome appears to be partially open and stationary, no action taken on abort request");
    }
    else if (DomeMotionSP.getState() == IPS_BUSY)
    {
        if (DomeMotionSP[DOME_CW].getState() == ISS_ON)
        {
            LOG_WARN("Abort roof action requested while the roof was opening. Direction correction may be needed on the next move request.");
        }
        else if (DomeMotionSP[DOME_CCW].getState() == ISS_ON)
        {
            LOG_WARN("Abort roof action requested while the roof was closing. Direction correction may be needed on the next move request.");
        }
        roofClosing = false;
        roofOpening = false;
        //MotionRequest = -1;
        roofTimedOut = EXPIRED_ABORT;
        sendRoofCommand(ROOF_ABORT_CMD, true, false);
    }

    // If both limit switches are off, then we're neither parked nor unparked.
    if (fullyOpenedLimitSwitch == ISS_OFF && fullyClosedLimitSwitch == ISS_OFF)
    {
        ParkSP.reset();
        ParkSP.setState(IPS_IDLE);
        ParkSP.apply();
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void RollOffIno::updateRoofStatus()
{
    bool lockedStatus = false;
    bool auxiliaryStatus = false;
    bool openedStatus = false;
    bool closedStatus = false;
    getRoofSwitch(ROOF_OPENED_SWITCH, &openedStatus, &fullyOpenedLimitSwitch);
    getRoofSwitch(ROOF_CLOSED_SWITCH, &closedStatus, &fullyClosedLimitSwitch);
    getRoofSwitch(ROOF_LOCKED_SWITCH, &lockedStatus, &roofLockedSwitch);
    getRoofSwitch(ROOF_AUX_SWITCH, &auxiliaryStatus, &roofAuxiliarySwitch);

    if (!openedStatus && !closedStatus && !roofOpening && !roofClosing)
    {
        RoofStatusLP.setState(IPS_ALERT);
        DEBUG(INDI::Logger::DBG_WARNING, "Roof stationary, neither opened or closed, adjust to match PARK button");
    }
    if (openedStatus && closedStatus)
    {
        RoofStatusLP.setState(IPS_ALERT);
        DEBUG(INDI::Logger::DBG_WARNING, "Roof showing it is both opened and closed according to the controller");
    }
    RoofStatusLP[ROOF_STATUS_LOCKED].setState(IPS_IDLE);
    RoofStatusLP[ROOF_STATUS_AUXSTATE].setState(IPS_IDLE);
    RoofStatusLP[ROOF_STATUS_OPENED].setState(IPS_IDLE);
    RoofStatusLP[ROOF_STATUS_CLOSED].setState(IPS_IDLE);
    RoofStatusLP[ROOF_STATUS_MOVING].setState(IPS_IDLE);
    RoofStatusLP.apply();

    if (auxiliaryStatus)
    {
        RoofStatusLP[ROOF_STATUS_AUXSTATE].setState(IPS_OK);
    }

    if (lockedStatus)
    {
        RoofStatusLP[ROOF_STATUS_LOCKED].setState(IPS_ALERT);         // Red to indicate lock is on
        if (closedStatus)
        {
            RoofStatusLP[ROOF_STATUS_CLOSED].setState(IPS_OK);            // Closed and locked roof status is normal
            RoofStatusLP.setState(IPS_OK);
        }

            // An actual roof lock would not be expected unless roof was closed.
            // However the controller might be using it to prevent motion for some other reason.
        else if (openedStatus)
        {
            RoofStatusLP[ROOF_STATUS_OPENED].setState(IPS_OK);         // Possible, rely on open/close lights to indicate situation
        }
        else if (roofOpening || roofClosing)
        {
            RoofStatusLP[ROOF_STATUS_MOVING].setState(IPS_ALERT);         // Should not be moving while locked
            RoofStatusLP.setState(IPS_ALERT);
        }
    }
    else
    {
        if (openedStatus || closedStatus)
        {
            if (openedStatus && !closedStatus)
            {
                roofOpening = false;
                RoofStatusLP[ROOF_STATUS_OPENED].setState(IPS_OK);
            }
            if (closedStatus && !openedStatus)
            {
                roofClosing = false;
                RoofStatusLP[ROOF_STATUS_CLOSED].setState(IPS_OK);
            }
            RoofStatusLP.setState(IPS_OK);
        }
        else if (roofOpening || roofClosing)
        {
            if (roofOpening)
            {
                RoofStatusLP[ROOF_STATUS_OPENED].setState(IPS_BUSY);
                RoofStatusLP[ROOF_STATUS_MOVING].setState(IPS_BUSY);
            }
            else if (roofClosing)
            {
                RoofStatusLP[ROOF_STATUS_CLOSED].setState(IPS_BUSY);
                RoofStatusLP[ROOF_STATUS_MOVING].setState(IPS_BUSY);
            }
            RoofStatusLP.setState(IPS_BUSY);
        }

            // Roof is stationary, neither opened or closed
        else
        {
            if (roofTimedOut == EXPIRED_OPEN)
                RoofStatusLP[ROOF_STATUS_OPENED].setState(IPS_ALERT);
            else if (roofTimedOut == EXPIRED_CLOSE)
                RoofStatusLP[ROOF_STATUS_CLOSED].setState(IPS_ALERT);
            RoofStatusLP.setState(IPS_ALERT);
        }
    }
    RoofStatusLP.apply();
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::getRoofSwitch(const char *switchId, bool *result,  ISState *switchState)
{
    char readBuffer[MAXINPBUF] = {};
    char writeBuffer[MAXOUTBUF] = {};
    if (!contactEstablished)
    {
        if (communicationErrors < MAX_CNTRL_COM_ERR)
            LOG_WARN("No contact with the roof controller has been established");
        return false;
    }
    if (switchId == nullptr || result == nullptr || switchState == nullptr)
    {
        LOG_ERROR("Internal Error");
        return false;
    }

    strcpy(writeBuffer, "(GET:");
    strcat(writeBuffer, switchId);
    strcat(writeBuffer, ":0)");
    if (!writeIno(writeBuffer))
        return false;
    if (!readIno(readBuffer))
        return false;

    if (evaluateResponse(writeBuffer, readBuffer, result))
    {
        if (*result)
            *switchState = ISS_ON;
        else
            *switchState = ISS_OFF;
        return true;
    }
    else
    {
        if (communicationErrors < MAX_CNTRL_COM_ERR)
            LOGF_WARN("Unable to obtain from the controller status: %s, errors: %d", switchId, ++communicationErrors);
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// Type of roof controller and whether roof is moving or stopped, along with the command sent will
// determine the effect on the roof. This could mean stopping or starting in a reversed direction.
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::sendRoofCommand(const char* button, bool switchOn, bool ignoreLock)
{
    char readBuffer[MAXINPBUF] = {};
    char writeBuffer[MAXOUTBUF] = {};
    bool status;
    bool responseState = false;

    if (!contactEstablished)
    {
        if (communicationErrors < MAX_CNTRL_COM_ERR)
            LOG_WARN("No contact with the roof controller has been established");
        return false;
    }
    if ((roofLockedSwitch == ISS_OFF) || ignoreLock)
    {
        strcpy(writeBuffer, "(SET:");
        strcat(writeBuffer, button);
        if (switchOn)
            strcat(writeBuffer, ":ON)");
        else
            strcat(writeBuffer, ":OFF)");
        LOGF_DEBUG("Button pushed: %s", writeBuffer);
        if (!writeIno(writeBuffer))
            return false;
        if ((status = readIno(readBuffer)))
            status = evaluateResponse(writeBuffer, readBuffer, &responseState);
        return status;
    }
    else
    {
        LOG_WARN("Roof external lock state prevents roof movement");
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::evaluateResponse(char* request, char* response, bool* result)
{
    std::string s_response = response;
    std::string ack = "ACK";
    std::string on = "ON";
    // If successful response, confirm its match.
    if (s_response.find(ack))
    {
        // Return ON/OFF as true/false
        size_t pos = s_response.find(on);
        *result = (pos != std::string::npos ? true : false);
        return true;
    }
    LOGF_WARN("The request %s, returned failed response %s", request, response);
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
// Called from Dome, BaseDevice to establish contact with device
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::Handshake()
{
    LOG_INFO("Documentation: https://github.com/indilib/indi-3rdparty [indi-rolloffino]");
    if (PortFD <= 0)
    {
        LOG_WARN("The connection port has not been established");
        return false;
    }

    if (!initialContact())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Initial controller contact failed, retrying");
        msSleep(1000);
        if (!initialContact())
        {
            LOG_ERROR("Unable to contact the roof controller");
            return false;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
//  See if the controller is running
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::initialContact()
{
    char readBuffer[MAXINPBUF] = {};
    char init[MAXOUTBUF] = {"(CON:0:0)"};
    contactEstablished = false;
    actionCount = 0;
    if (!writeIno(init))
    {
        return false;
    }
    if (!readIno(readBuffer))
    {
        LOGF_WARN("Failed reading initial contact reponse to %s", init);
        return false;
    }
    // "(ACK:0:V1.3-0  [ACTn])",
    std::string s_response = readBuffer;
    std::string ack = "ACK";
    // If successful response, See if it is from updated client supporting Actions.
    if (s_response.find(ack))
    {
        std::regex regexp("[[ACT(\\d)]]");
        std::smatch match;
        if (std::regex_search(s_response, match, regexp))
        {
            std::string result = match.str();
            actionCount = std::stoi(result);
        }
        contactEstablished = true;
        //DEBUGF(INDI::Logger::DBG_SESSION, "Initial contact response: %s", readBuffer);
        LOGF_INFO("Number of Action commands enabled by controller. %d", actionCount);
        return true;
    }
    LOGF_WARN("Initial contact returned a negative acknowledgement %s", readBuffer);
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::UpdateDigitalInputs()
{
    char get[MAXOUTBUF] = {0};
    char response[MAXINPBUF] = {0};
    for (unsigned int i = 0; i < MAX_ACTIONS; i++)
    {
        std::string ack = "ACK";
        std::string on = "ON";
        // Do not get Action values beyond what controller indicates it will support
        if (i < actionCount)
        {
            auto state = ISS_OFF;
            strncpy(get, inpRoRino[i], MAXOUTBUF-1);
            if (strlen(get) == 0)
                continue;
            if (!writeIno(get))
            {
                LOGF_WARN("Failed %s request", get);
                return false;
            }
            if (!readIno(response))
            {
                LOGF_WARN("Failed %s reply %s", get, response);
                return false;
            }
            std::string s_resp = response;
            size_t pos = s_resp.find(ack);
            if (pos == std::string::npos)
                return false;
            pos = s_resp.find(on);
            if (pos != std::string::npos)
                state = ISS_ON;
            if (DigitalInputsSP[i].findOnSwitchIndex() != state)
            {
                DigitalInputsSP[i].reset();
                DigitalInputsSP[i][state].setState(ISS_ON);
                DigitalInputsSP[i].setState(IPS_OK);
                DigitalInputsSP[i].apply();
            }
        }
    }         // End for
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::UpdateAnalogInputs()
{
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::UpdateDigitalOutputs()
{
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::CommandOutput(uint32_t index, OutputState command)
{
    char response[MAXINPBUF] = {0};
    if (index >= MAX_ACTIONS)
    {
        LOGF_WARN("Invalid output index %d. Valid range from 0 to %d.", MAX_ACTIONS - 1);
        return false;
    }
    char cmd[MAXOUTBUF] = {0};
    uint8_t enabled = (command == OutputInterface::On) ? 1 : 0;
    std::string on = "ON";
    std::string off = "OFF";
    std::string s_cmd = outRoRino[index];
    size_t pos = s_cmd.find(on);
    // Do not send Action command beyond what controller indicates it will support
    if (index >= actionCount)
        return true;
    snprintf(cmd, MAXOUTBUF-1, "%s", s_cmd.c_str());
    if (enabled == 0)
    {
        if (pos == std::string::npos)
        {
            LOGF_WARN("Failed obtaining command to turn off %s", cmd);
            return false;
        }
        s_cmd.replace(pos, on.length(), off);
        strncpy(cmd, s_cmd.c_str(), MAXOUTBUF-1);
    }
    if (!writeIno(cmd))
    {
        LOGF_WARN("Failed issuing %s command", cmd);
        return false;
    }
    if (!readIno(response))
    {
        LOGF_WARN("Failed reading response to %s command", cmd);
        return false;
    }

    std::string s_response = response;
    std::string ack = "ACK";
    // successful response, confirm its match.
    if (s_response.find(ack))
    {
        std::regex regexp(":([(A-Z])+(\\d)*([(A-Z])*:");
        std::smatch match;
        if (std::regex_search(s_response, match, regexp))
        {
            std::string result = match.str();;
            pos = s_cmd.find(result);
            if (pos != std::string::npos)
                return true;
            else
            {
                LOGF_WARN("Command %s confirmation matching failed %s", cmd, response);
                return false;
            }
        }
    }
    LOGF_WARN("Command %s negative acknowledgement returned %s", cmd, response);
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::readIno(char* retBuf)
{
    const char stop_char {RORINO_STOP_CHAR};
    int nbytes_read = 0;
    int rc = TTY_OK;

    for (int i= 0; i < 2; i++)
    {
        rc = tty_nread_section(PortFD, retBuf, MAXINPBUF-1, stop_char, MAXINOWAIT, &nbytes_read);
        if (rc != TTY_OK)
        {
            msSleep(1000);
            continue;
        }
        LOGF_DEBUG("Read from roof controller: %s", retBuf);
        return true;
    }
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF-1);
        LOGF_ERROR("Arduino connection read error: %s.", errstr);
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool RollOffIno::writeIno(const char* msg)
{
    int retMsgLen = 0;
    int status;

    if (strlen(msg) >= MAXOUTBUF-1)
    {
        LOG_ERROR("Roof controller command message too long");
        return false;
    }
    LOGF_DEBUG("Sent to roof controller: %s", msg);
    tcflush(PortFD, TCIOFLUSH);
    status = tty_write_string(PortFD, msg, &retMsgLen);
    if (status != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(status, errstr, MAXRBUF);
        LOGF_DEBUG("Arduino Connection write error: %s", errstr);
        return false;
    }
    return true;
}


