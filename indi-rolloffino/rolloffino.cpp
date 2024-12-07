/*
 Edited version of the Dome Simulator
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.
 Copyright(c) 2023 Jasem Mutlaq. All rights reserved.
 Copyright(c) 2024 Jasem Mutlaq. All rights reserved.

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
 * Modified version of the rolloff roof simulator.
 * Uses a simple text string protocol to send messages to an Arduino. The Arduino's USB connection is set
 * by default to 38400 baud. The Arduino code determines which pins open/close a relay to start/stop the
 * roof motor, and read state of switches indicating if the roof is  opened or closed.
 */
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <regex>
#include <array>
#include "rolloffino.h"

#define ROLLOFF_DURATION 60               // Seconds until Roof is fully opened or closed
#define INACTIVE_STATUS  4                // Seconds between updating status lights
#define MAX_CNTRL_COM_ERR 10              // Maximum consecutive errors communicating with Arduino
#define MAXINOLINE       63          // Sized to contain outgoing command requests
#define MAXINOBUF        255         // Sized for maximum overall input / output
#define MAXINOERR        255         // System call error message buffer
#define MAXINOWAIT       3           // seconds

// We declare an auto pointer to RollOffIno.
std::unique_ptr<RollOffIno> rollOffIno(new RollOffIno());

void ISSnoopDevice(XMLEle *root)
{
    rollOffIno->ISSnoopDevice(root);
}

bool RollOffIno::ISSnoopDevice(XMLEle *root)
{
    return INDI::Dome::ISSnoopDevice(root);
}

RollOffIno::RollOffIno()
{
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);           // Need the DOME_CAN_PARK capability for the scheduler
}

/**************************************************************************************
** INDI is asking us for our default device name.
** Check that it matches Ekos selection menu and ParkData.xml names
***************************************************************************************/
const char *RollOffIno::getDefaultName()
{
    return (const char *)"RollOff ino";
}
/**************************************************************************************
** INDI request to init properties. Connected Define properties to Ekos
***************************************************************************************/
bool RollOffIno::initProperties()
{
    char var_act [MAX_LABEL];
    char var_lab [MAX_LABEL];
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

    // Options tab
    addAuxControls();         // This is for the standard controls
    RoofTimeoutNP[0].fill("ROOF_TIMEOUT", "Timeout in Seconds", "%3.0f", 1, 300, 1, 40);
    RoofTimeoutNP.fill(getDeviceName(), "ROOF_MOVEMENT", "Roof Movement", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Action labels
    for (int i = 0; i < MAX_ACTIONS; i++)
    {
        sprintf(var_act, "Action %d", i+1);
        sprintf(var_lab, "Label  %d", i+1);
        actionLabels_tp[i][0].fill(action_labels[i], var_lab, var_act);
        actionLabels_tp[i].fill(getDeviceName(), label_init[i],  var_act,   ACTION_LABEL_TAB, IP_RW, 60, IPS_IDLE);
        defineProperty(actionLabels_tp[i]);
    }
    loadConfig(true);

    // Action Switches
    for (int i = 0; i < MAX_ACTIONS; i++)
    {
        char* aLabel = actionLabels_tp[i][0].text;
        sprintf(var_act, "Action %d", i+1);
        sprintf(var_lab, "Label %d", i+1);
        if (strlen(aLabel) == 0)
        {
            aLabel = var_act;       // sprintf(aLabel, "Empty Label %d", i);
        }
        else
        {
            for (int j = 0; j < i; j++)
            {
                if (strcmp(aLabel, actionLabels_tp[j][0].text) == 0)
                {
                    sprintf(aLabel, "Duplicate Label %d", j);
                    break;
                }
            }
        }
        actionSwitches_sp[i][ACT_ENABLE].fill("ACTION_ENABLE", "On", ISS_OFF);
        actionSwitches_sp[i][ACT_DISABLE].fill("ACTION_DISABLE", "Off", ISS_ON);
        actionSwitches_sp[i].fill(getDeviceName(),actionSwitchesText[i], aLabel,   ACTION_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

        // Action Status Lights
        ActionStatusLP[i].fill(actionSwitchesText[i], aLabel, IPS_IDLE);
    }
    ActionStatusLP.fill(getDeviceName(), "ACTION STATUS","Returned State", ACTION_CONTROL_TAB, IPS_BUSY);

    SetParkDataType(PARK_NONE);
    loadConfig(true);
    return true;
}
/********************************************************************************************
** INDI request to update the properties because there is a change in CONNECTION status
** This function is called whenever the device is connected or disconnected.
*********************************************************************************************/
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
        for (int i = 0; i < MAX_ACTIONS; i++)
            defineProperty(actionLabels_tp[i]);  // Labels
        for (int i = 0; i < MAX_ACTIONS; i++)
            defineProperty(actionSwitches_sp[i]);  // Actions
        defineProperty(ActionStatusLP);      // Action status lights
        checkConditions();
    }
    else
    {
        deleteProperty(LockSP);              // Delete the Lock Switch buttons
        deleteProperty(AuxSP);               // Delete the Auxiliary Switch buttons
        deleteProperty(RoofStatusLP);        // Delete the roof status lights
        deleteProperty(RoofTimeoutNP);       // Roof timeout
        for (int i = 0; i < MAX_ACTIONS; i++)
            deleteProperty(actionLabels_tp[i]);  // Labels
        for (int i = 0; i < MAX_ACTIONS; i++)
            deleteProperty(actionSwitches_sp[i]);  // Actions
        deleteProperty(ActionStatusLP);      // Delete the action status lights
    }
    return true;
}

void RollOffIno::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);
    defineProperty(LockSP);        // Lock Switch,
    defineProperty(AuxSP);         // Aux Switch,
    defineProperty(RoofTimeoutNP); // Roof timeout
    for (int i = 0; i < MAX_ACTIONS; i++)
        defineProperty(actionLabels_tp[i]);  // Labels
    for (int i = 0; i < MAX_ACTIONS; i++)
        defineProperty(actionSwitches_sp[i]);  // Actions
}

bool RollOffIno::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);
    LockSP.save(fp);
    AuxSP.save(fp);
    RoofTimeoutNP.save(fp);
    for (int i = 0; i < MAX_ACTIONS; i++)
        actionLabels_tp[i].save(fp);  // Labels
    for (int i = 0; i < MAX_ACTIONS; i++)
        actionSwitches_sp[i].save(fp);  // Actions
    return true;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool RollOffIno::Connect()
{
    bool status = INDI::Dome::Connect();
    return status;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool RollOffIno::Disconnect()
{
    bool status = INDI::Dome::Disconnect();
    return status;
}

/********************************************************************************************
** Establish conditions on a connect.
*********************************************************************************************/
bool RollOffIno::checkConditions()
{
    updateRoofStatus();
    updateActionStatus();
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

bool RollOffIno::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        for (int i = 0; i < MAX_ACTIONS; i++)
        {
            if (actionLabels_tp[i].isNameMatch(name))
            {
                actionLabels_tp[i].update(texts, names, n);
                actionLabels_tp[i].setState(IPS_OK);
                actionLabels_tp[i].apply();
                saveConfig(true);
                return true;
            }
        }
    }
    return INDI::Dome::ISNewText(dev, name, texts, names, n);
}

/********************************************************************************************
** Client has changed the state of a switch, update
*********************************************************************************************/
bool RollOffIno::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Make sure the call is for our device
    if(dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ////////////////////////////////////////
        // Check if the call for our Lock switch
        if (LockSP.isNameMatch(name))
        {
            // Find out if requested state is the same as previous100
            auto previous = LockSP.findOnSwitchIndex();
            LockSP.update(states, names, n);
            auto requested = LockSP.findOnSwitchIndex();

            if (requested < 0)
                return true;

            //DEBUGF(INDI::Logger::DBG_SESSION, "Prev %d, Cur %d", previous, requested);
            if (previous == requested )
            {
                //DEBUGF(INDI::Logger::DBG_SESSION, "Lock switch is already %s", (requested == 0 ? "On" : "Off"));
                LockSP.reset();
                LockSP.setState(IPS_OK);
                LockSP[previous].setState(ISS_ON);
                LockSP.apply();
                return true;
            }

            //DEBUGF(INDI::Logger::DBG_SESSION, "Lock switch requesting %s", (requested == 0 ? "On" : "Off"));
            // Update the switch state
            if (requested == 0)
            {
                LockSP.setState(IPS_OK);
            //    LockSP.reset();
                LockSP[requested].s = ISS_ON;
                //LOGF_INFO("Roof %s", (requested == 0 ? "Lock" : "Unlocked"));
                LockSP.apply();
                sendRoofCommand(ROOF_LOCK_CMD, true, true);
                updateRoofStatus();
                return true;
            }
            else
            {
                LockSP.setState(IPS_IDLE);
            //    LockSP.reset();
                LockSP[requested].s = ISS_OFF;
                //LOGF_INFO("Roof %s", (requested == 0 ? "Lock" : "UnLocked"));
                LockSP.apply();
                sendRoofCommand(ROOF_LOCK_CMD, false, true);
                updateRoofStatus();
                return true;
            }
        }
        
        ////////////////////////////////////////
        // Check if the call for our Aux switch
        if (AuxSP.isNameMatch(name))
        {
            // Find out if requested state is the same as previous
            auto previous = AuxSP.findOnSwitchIndex();
            AuxSP.update(states, names, n);
            auto requested = AuxSP.findOnSwitchIndex();

            if (requested < 0)
                return true;

            //DEBUGF(INDI::Logger::DBG_SESSION, "Prev %d, Cur %d", previous, requested);
            if (previous == requested)
            {
                //DEBUGF(INDI::Logger::DBG_SESSION, "Aux switch is already %s", (requested == 0 ? "On" : "Off"));
                AuxSP.reset();
                AuxSP.setState(IPS_OK);
                AuxSP[previous].setState(ISS_ON);
                AuxSP.apply();
                return true;
            }

            //DEBUGF(INDI::Logger::DBG_SESSION, "Aux switch requesting %s", (requested == 0 ? "On" : "Off"));
            // Update the switch state
            if (requested == 0)
            {
                AuxSP.setState(IPS_OK);
            //    AuxSP.reset();
                AuxSP[requested].s = ISS_ON;
                //LOGF_INFO("Aux Switch %s", (requested == 0 ? "On" : "Off"));
                AuxSP.apply();
                sendRoofCommand(ROOF_AUX_CMD, true, true);
                updateRoofStatus();
                return true;
            }
            else
            {
                AuxSP.setState(IPS_IDLE);
            //    AuxSP.reset();
                AuxSP[requested].s = ISS_OFF;
                //LOGF_INFO("Aux Switch %s", (requested == 0 ? "On" : "Off"));
                sendRoofCommand(ROOF_AUX_CMD, false, true);
                AuxSP.apply();
                updateRoofStatus();
                return true;
            }
        }

        ////////////////////////////////////////
        // Check if the call for one of the Action switches
        for (int i = 0; i < MAX_ACTIONS; i++)
        {
            if (actionSwitches_sp[i].isNameMatch(name))
            {
                // Find out if requested state is the same as previous
                auto previous = actionSwitches_sp[i].findOnSwitchIndex();
                actionSwitches_sp[i].update(states, names, n);
                auto requested = actionSwitches_sp[i].findOnSwitchIndex();
                if (requested < 0)
                    return true;

                //DEBUGF(INDI::Logger::DBG_SESSION, "Prev %d, Cur %d", previous, requested);
                if (previous == requested)
                {
                    //DEBUGF(INDI::Logger::DBG_SESSION, "Action%d switch is already %s", i+1, (requested == 0 ? "On" : "Off"));
                    actionSwitches_sp[i].reset();
                    actionSwitches_sp[i].setState(IPS_OK);
                    actionSwitches_sp[i][previous].setState(ISS_ON);
                    actionSwitches_sp[i].apply();
                    return true;
                }

                //DEBUGF(INDI::Logger::DBG_SESSION, "Action%d switch requesting %s", i+1, (requested == 0 ? "On" : "Off"));
                // Update the switch state
                if (requested == 0)
                {
                //    actionSwitches_sp[i].reset();
                    actionSwitches_sp[i].setState(IPS_OK);
                    actionSwitches_sp[i][requested].s = ISS_ON;
                    //LOGF_INFO("Action%d %s", i+1, (requested == 0 ? "On" : "Off"));
                    actionSwitches_sp[i].apply();
                    if (actionCmdUsed(act_cmd_used[i]))
                        return sendRoofCommand(act_cmd_used[i], true, true);
                    updateActionStatus();
                    return true;
                }
                else
                {
                    actionSwitches_sp[i].setState(IPS_IDLE);
                //    actionSwitches_sp[i].reset();
                    actionSwitches_sp[i][requested].s = ISS_ON;
                    //LOGF_INFO("Action%d %s", i+1, (requested == 0 ? "On" : "Off"));
                    actionSwitches_sp[i].apply();
                    if (actionCmdUsed(act_cmd_used[i]))
                        return sendRoofCommand(act_cmd_used[i], false, true);
                    updateActionStatus();
                    return true;
                }
            }
        }
    }
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

void RollOffIno::updateRoofStatus()
{
    bool auxiliaryStatus = false;
    bool lockedStatus = false;
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
    RoofStatusLP[ROOF_STATUS_AUXSTATE].setState(IPS_IDLE);
    RoofStatusLP[ROOF_STATUS_LOCKED].setState(IPS_IDLE);
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

void RollOffIno::updateActionStatus()
{
    for (int i = 0; i < MAX_ACTIONS; i++)
    {
        actionStatus[i] = false;
        ActionStatusLP[i].setState(IPS_IDLE);

        if (actionStateUsed(action_state_used[i]))
        {
            getRoofSwitch(action_state_used[i], &actionStatus[i], &actionStatusState[i]);
        }
    }

    for (int i = 0; i < MAX_ACTIONS; i++)
    {
        if (actionStatus[i])
        {
            ActionStatusLP[i].setState(IPS_OK);
        }
    }
    ActionStatusLP.setState(IPS_OK);
    ActionStatusLP.apply();
}

/********************************************************************************************
** Each 1 second timer tick, if roof active
********************************************************************************************/
void RollOffIno::TimerHit()
{
    double timeleft = CalcTimeLeft(MotionStart);
    uint32_t delay = 1000 * INACTIVE_STATUS;   // inactive timer setting to maintain roof status lights
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    updateRoofStatus();
    updateActionStatus();

    if (DomeMotionSP.getState() == IPS_BUSY)
    {
        // Abort called stop movement.
        if (MotionRequest < 0)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof motion is stopped");
            setDomeState(DOME_IDLE);
        }
        else
        {
            // Roll off is opening
            if (DomeMotionSP[DOME_CW].getState() == ISS_ON)
            {
                if (fullyOpenedLimitSwitch == ISS_ON)
                {
                    DEBUG(INDI::Logger::DBG_DEBUG, "Roof is open");
                    SetParked(false);
                }
                // See if time to open has expired.
                else if (timeleft <= 0)
                {
                    LOG_WARN("Time allowed for opening the roof has expired?");
                    setDomeState(DOME_IDLE);
                    roofOpening = false;
                    SetParked(false);
                    roofTimedOut = EXPIRED_OPEN;
                }
                else
                {
                    delay = 1000;           // opening active
                }
            }
            // Roll Off is closing
            else if (DomeMotionSP[DOME_CCW].getState() == ISS_ON)
            {
                if (fullyClosedLimitSwitch == ISS_ON)
                {
                    DEBUG(INDI::Logger::DBG_DEBUG, "Roof is closed");
                    SetParked(true);
                }
                // See if time to open has expired.
                else if (timeleft <= 0)
                {
                    LOG_WARN("Time allowed for closing the roof has expired?");
                    setDomeState(DOME_IDLE);
                    roofClosing = false;
                    SetParked(false);
                    roofTimedOut = EXPIRED_CLOSE;
                }
                else
                {
                    delay = 1000;           // closing active
                }
            }
        }
    }
    // If the roof is moved outside of indi from parked to unparked. The driver's
    // fully opened switch lets it to detect this. The Dome/Observatory was
    // continued to show parked. This call was added to set parked/unparked in Dome.
    else
    {
        checkConditions();
    }
    // Added to highlight WiFi issues, not able to recover lost connection without a reconnect
    if (communicationErrors >= MAX_CNTRL_COM_ERR)
    {
        LOG_ERROR("Too many errors communicating with Arduino");
        LOG_ERROR("Try a fresh connect. Check communication equipment and operation of Arduino controller.");
        //communicationErrors = 0;
        //INDI::Dome::Disconnect();
    }

    // Even when no roof movement requested, will come through occasionally. Use timer to update roof status
    // in case roof has been operated externally by a remote control, locks applied...
    SetTimer(delay);
}

float RollOffIno::CalcTimeLeft(timeval start)
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince =
        (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = MotionRequest - timesince;
    return timeleft;
}

void RollOffIno::msSleep (int mSec)
{
    struct timespec req = {0, 0};
    req.tv_sec = 0;
    req.tv_nsec = mSec * 1000000L;
    nanosleep(&req, (struct timespec *)nullptr);
}

/*
 * Direction: DOME_CW Clockwise = Open; DOME-CCW Counter clockwise = Close
 * Operation: MOTION_START, | MOTION_STOP
 */
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
                return IPS_ALERT;
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
                return IPS_ALERT;
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
        roofTimedOut = EXPIRED_CLEAR;
        MotionRequest = (int)RoofTimeoutNP[0].getValue();
        LOGF_DEBUG("Roof motion timeout setting: %d", (int)MotionRequest);
        gettimeofday(&MotionStart, nullptr);
        SetTimer(1000);
        return IPS_BUSY;
    }
    return    IPS_ALERT;
}
/*
 * Close Roof
 *
 */
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

/*
 * Open Roof
 *
 */
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

/*
 * Abort motion
 */
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
        MotionRequest = -1;
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

bool RollOffIno::actionStateUsed(const char* action)
{
    if (actionCount > 0)
    {
        for (unsigned int i = 0; i < actionCount; i++)
        {
            if (!strcmp(action, action_state_used[i]))
            {
                return true;
            }
        }
    }
    return false;
}

bool RollOffIno::actionCmdUsed(const char* action)
{
    if (actionCount > 0)
    {
        for(unsigned int i = 0; i < actionCount; i++)
        {
            if (!strcmp(action, act_cmd_used[i]))
            {
                return true;
            }
        }
    }
    return false;
}

bool RollOffIno::getRoofSwitch(const char *switchId, bool *result,  ISState *switchState)
{
    char readBuffer[MAXINOBUF] = {};
    char writeBuffer[MAXINOLINE] = {};
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

/*
 * Type of roof controller and whether roof is moving or stopped, along with the command sent will
 * determine the effect on the roof. This could mean stopping, or starting in a reversed direction.
 */
bool RollOffIno::sendRoofCommand(const char* button, bool switchOn, bool ignoreLock)
{
    char readBuffer[MAXINOBUF] = {};
    char writeBuffer[MAXINOBUF] = {};
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


/************************************************************************************
 * Called from Dome, BaseDevice to establish contact with device
 ************************************************************************************/
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

/*
 * See if the controller is running
 */
bool RollOffIno::initialContact()
{
    char readBuffer[MAXINOBUF] = {};
    char init[MAXINOBUF] = {"(CON:0:0)"};
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

bool RollOffIno::readIno(char* retBuf)
{
    const char stop_char { 0x29 };
    int nbytes_read = 0;
    int rc = TTY_OK;

    for (int i= 0; i < 2; i++)
    {
        rc = tty_nread_section(PortFD, retBuf, MAXINOBUF-1, stop_char, MAXINOWAIT, &nbytes_read);
        if (rc != TTY_OK)
        {
            usleep(100000);
            continue;
        }
        //LOGF_DEBUG("Read from roof controller: %s", retBuf);
        return true;
    }
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
    }
    return false;
}

bool RollOffIno::writeIno(const char* msg)
{
    int retMsgLen = 0;
    int status;
    char errMsg[MAXINOERR];

    if (strlen(msg) >= MAXINOLINE)
    {
        LOG_ERROR("Roof controller command message too long");
        return false;
    }
    //LOGF_DEBUG("Sent to roof controller: %s", msg);
    tcflush(PortFD, TCIOFLUSH);
    status = tty_write_string(PortFD, msg, &retMsgLen);
    if (status != TTY_OK)
    {
        tty_error_msg(status, errMsg, MAXINOERR);
        LOGF_DEBUG("roof control connection error: %s", errMsg);
        return false;
    }
    return true;
}

