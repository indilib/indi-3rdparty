/*
    Avalon Star GO Focuser
    Copyright (C) 2018 Christopher Contaxis (chrconta@gmail.com) and
    Wolfgang Reissenberger (sterne-jaeger@t-online.de)

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

#include "lx200stargofocuser.h"

#include <cstring>

#define AVALON_FOCUSER_POSITION_OFFSET                  500000

/**
 * @brief Constructor
 * @param defaultDevice the telescope
 * @param name device name
 */
LX200StarGoFocuser::LX200StarGoFocuser(LX200StarGo* defaultDevice, const char *name) : INDI::FocuserInterface(defaultDevice)
{
    baseDevice = defaultDevice;
    deviceName = name;
    focuserActivated = false;
}

/**
 * @brief Initialize the focuser UI controls
 * @param groupName tab where the UI controls are grouped
 */
void LX200StarGoFocuser::initProperties(const char *groupName)
{
    INDI::FocuserInterface::initProperties(groupName);
    // set default values
    FocusAbsPosNP[0].setMin(0.0);
    FocusAbsPosNP[0].setMax(100000.0);
    FocusAbsPosNP[0].setStep(1000.0);
    FocusRelPosNP[0].setStep(1000.0);
    FocusSyncNP[0].setStep(1000.0);
    FocusSpeedNP[0].setMin(0.0);
    FocusSpeedNP[0].setMax(10.0);
    FocusSpeedNP[0].setValue(1.0);

}

/**
 * @brief Fill the UI controls with current values
 * @return true iff everything went fine
 */

bool LX200StarGoFocuser::updateProperties()
{
    if (isConnected()) {
        baseDevice->defineProperty(FocusSpeedNP);
        baseDevice->defineProperty(FocusMotionSP);
        baseDevice->defineProperty(FocusTimerNP);
        baseDevice->defineProperty(FocusAbsPosNP);
        baseDevice->defineProperty(FocusRelPosNP);
        baseDevice->defineProperty(FocusAbortSP);
        baseDevice->defineProperty(FocusSyncNP);
        baseDevice->defineProperty(FocusReverseSP);
    }
    else {
        baseDevice->deleteProperty(FocusSpeedNP);
        baseDevice->deleteProperty(FocusMotionSP);
        baseDevice->deleteProperty(FocusTimerNP);
        baseDevice->deleteProperty(FocusAbsPosNP);
        baseDevice->deleteProperty(FocusRelPosNP);
        baseDevice->deleteProperty(FocusAbortSP);
        baseDevice->deleteProperty(FocusSyncNP);
        baseDevice->deleteProperty(FocusReverseSP);
    }
    return true;

}


/***************************************************************************
 * Reaction to UI commands
 ***************************************************************************/

bool LX200StarGoFocuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (FocusMotionSP.isNameMatch(name))
        {
            return changeFocusMotion(states, names, n);
        }
        else if (FocusAbortSP.isNameMatch(name))
        {
            return changeFocusAbort(states, names, n);
        }
        else if (FocusReverseSP.isNameMatch(name))
        {
            return setFocuserDirection(states, names, n);
        }
    }

    return true;
}


bool LX200StarGoFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(values);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (FocusSpeedNP.isNameMatch(name))
        {
            return changeFocusSpeed(values, names, n);
        } else if (FocusTimerNP.isNameMatch(name))
        {
            return changeFocusTimer(values, names, n);
        } else if (FocusAbsPosNP.isNameMatch(name))
        {
            return changeFocusAbsPos(values, names, n);
        } else if (FocusRelPosNP.isNameMatch(name))
        {
            return changeFocusRelPos(values, names, n);
        } else if (FocusSyncNP.isNameMatch(name))
        {
            return changeFocusSyncPos(values, names, n);
        }
    }

    return true;
}

/***************************************************************************
 *
 ***************************************************************************/

bool LX200StarGoFocuser::changeFocusTimer(double values[], char* names[], int n) {
    int time = static_cast<int>(values[0]);
    if (validateFocusTimer(time)) {
        FocusTimerNP.update(values, names, n);
        FocusTimerNP.setState(MoveFocuser(FocusMotionSP[0].getState() == ISS_ON ? FOCUS_INWARD : FOCUS_OUTWARD,
                static_cast<int>(FocusSpeedNP[0].getValue()), static_cast<uint16_t>(FocusTimerNP[0].getValue())));
        FocusTimerNP.apply();
    }
    return true;
}


bool LX200StarGoFocuser::changeFocusMotion(ISState* states, char* names[], int n) {
    FocusMotionSP.update(states, names, n);
    FocusMotionSP.setState(IPS_OK);
    FocusMotionSP.apply();
    return true;
}


bool LX200StarGoFocuser::changeFocusAbsPos(double values[], char* names[], int n) {
    uint32_t absolutePosition = static_cast<uint32_t>(values[0]);
    if (validateFocusAbsPos(absolutePosition)) {
        double currentPosition = FocusAbsPosNP[0].getValue();
        FocusAbsPosNP.update(values, names, n);
        // After updating the property the current position is temporarily reset to
        // the target position, I personally didn't like that so I am going to have
        // it only display the last known focuser position
        FocusAbsPosNP[0].setValue(currentPosition);
        FocusAbsPosNP.setState(MoveAbsFocuser(absolutePosition));
        FocusAbsPosNP.apply();
    }
    return true;
}

bool LX200StarGoFocuser::changeFocusRelPos(double values[], char* names[], int n) {
    int relativePosition = static_cast<int>(values[0]);
    if (validateFocusRelPos(relativePosition)) {
        FocusRelPosNP.update(values, names, n);
        FocusRelPosNP.setState(moveFocuserRelative(relativePosition));
        FocusRelPosNP.apply();
        // reflect the relative position status to the absolute position
        FocusAbsPosNP.setState(FocusRelPosNP.getState());
        FocusAbsPosNP.apply();
    }
    return true;
}

bool LX200StarGoFocuser::changeFocusSpeed(double values[], char* names[], int n) {
    int speed = static_cast<int>(values[0]);
    if (validateFocusSpeed(speed)) {
        FocusSpeedNP.update(values, names, n);
        FocusSpeedNP.setState(SetFocuserSpeed(speed) ? IPS_OK : IPS_ALERT);

        FocusSpeedNP.apply();
    }
    return true;
}

bool LX200StarGoFocuser::setFocuserDirection(ISState* states, char* names[], int n) {

    if (FocusReverseSP.update(states, names, n))
        return false;

    focuserReversed = (FocusReverseSP.findOnSwitchIndex() == 0 ? INDI_ENABLED : INDI_DISABLED);

    FocusReverseSP.setState(IPS_OK);
    FocusReverseSP.apply();

    return true;
}


bool LX200StarGoFocuser::changeFocusAbort(ISState* states, char* names[], int n) {
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
    FocusAbortSP.reset();
    FocusAbortSP.setState(AbortFocuser() ? IPS_OK : IPS_ALERT);
    FocusAbsPosNP.setState(IPS_OK);
    FocusAbsPosNP.apply();
    FocusRelPosNP.setState(IPS_OK);
    FocusRelPosNP.apply();
    FocusAbortSP.apply();
    return true;
}


bool LX200StarGoFocuser::changeFocusSyncPos(double values[], char* names[], int n) {
    int absolutePosition = static_cast<int>(values[0]);
    if (validateFocusSyncPos(absolutePosition)) {
        FocusSyncNP.update(values, names, n);
        FocusSyncNP.setState(syncFocuser(absolutePosition));
        FocusSyncNP.apply();
    }
    return true;
}

bool LX200StarGoFocuser::validateFocusSpeed(int speed) {
    int minSpeed = static_cast<int>(FocusSpeedNP[0].getMin());
    int maxSpeed = static_cast<int>(FocusSpeedNP[0].getMax());
    if (speed < minSpeed || speed > maxSpeed) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Cannot set focuser speed to %d, it is outside the valid range of [%d, %d]", getDeviceName(), speed, minSpeed, maxSpeed);
        return false;
    }
    return true;
}

bool LX200StarGoFocuser::validateFocusTimer(int time) {
    int minTime = static_cast<int>(FocusTimerNP[0].getMin());
    int maxTime = static_cast<int>(FocusTimerNP[0].getMax());
    if (time < minTime || time > maxTime) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Cannot set focuser timer to %d, it is outside the valid range of [%d, %d]", getDeviceName(), time, minTime, maxTime);
        return false;
    }
    return true;
}

bool LX200StarGoFocuser::validateFocusAbsPos(uint32_t absolutePosition) {
    uint32_t minPosition = static_cast<uint32_t>(FocusAbsPosNP[0].getMin());
    uint32_t maxPosition = static_cast<uint32_t>(FocusAbsPosNP[0].getMax());
    if (absolutePosition < minPosition || absolutePosition > maxPosition) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Cannot set focuser absolute position to %d, it is outside the valid range of [%d, %d]", getDeviceName(), absolutePosition, minPosition, maxPosition);
        return false;
    }
    return true;
}

bool LX200StarGoFocuser::validateFocusRelPos(int relativePosition) {
    int minRelativePosition = static_cast<int>(FocusRelPosNP[0].getMin());
    int maxRelativePosition = static_cast<int>(FocusRelPosNP[0].getMax());
    if (relativePosition < minRelativePosition || relativePosition > maxRelativePosition) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Cannot set focuser relative position to %d, it is outside the valid range of [%d, %d]", getDeviceName(), relativePosition, minRelativePosition, maxRelativePosition);
        return false;
    }
    uint32_t absolutePosition = getAbsoluteFocuserPositionFromRelative(relativePosition);
    return validateFocusAbsPos(absolutePosition);
}

bool LX200StarGoFocuser::validateFocusSyncPos(int absolutePosition) {
    int minPosition = static_cast<int>(FocusAbsPosNP[0].getMin());
    int maxPosition = static_cast<int>(FocusAbsPosNP[0].getMax());
    if (absolutePosition < minPosition || absolutePosition > maxPosition) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Cannot sync focuser to position %d, it is outside the valid range of [%d, %d]", getDeviceName(), absolutePosition, minPosition, maxPosition);
        return false;
    }
    return true;
}

uint32_t LX200StarGoFocuser::getAbsoluteFocuserPositionFromRelative(int relativePosition) {
    bool inward = FocusMotionSP[0].getState() == ISS_ON;
    if (inward) {
        relativePosition *= -1;
    }
    return static_cast<uint32_t>(FocusAbsPosNP[0].getValue() + relativePosition);
}


bool LX200StarGoFocuser::ReadFocuserStatus() {
    // do nothing if not active
    if (!isConnected())
        return true;

    int absolutePosition = 0;
    if (sendQueryFocuserPosition(&absolutePosition)) {
        FocusAbsPosNP[0].setValue((focuserReversed == INDI_DISABLED) ? absolutePosition : -absolutePosition);
        FocusAbsPosNP.apply();
    }
    else
        return false;

    if (isFocuserMoving() && atFocuserTargetPosition()) {
        FocusAbsPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
        FocusRelPosNP.setState(IPS_OK);
        FocusRelPosNP.apply();
    }

    return true;
}

bool LX200StarGoFocuser::SetFocuserSpeed(int speed) {
    return sendNewFocuserSpeed(speed);
}



IPState LX200StarGoFocuser::MoveFocuser(FocusDirection dir, int speed, uint16_t duration) {
    INDI_UNUSED(speed);
    if (duration == 0) {
        return IPS_OK;
    }
    uint32_t position = static_cast<uint32_t>(FocusAbsPosNP[0].getMin());
    if (dir == FOCUS_INWARD) {
        position = static_cast<uint32_t>(FocusAbsPosNP[0].getMax());
    }
    moveFocuserDurationRemaining = duration;
    bool result = sendMoveFocuserToPosition(position);
    if (!result) {
        return IPS_ALERT;
    }
    return IPS_BUSY;
}

IPState LX200StarGoFocuser::MoveAbsFocuser(uint32_t absolutePosition) {
    bool result = sendMoveFocuserToPosition(absolutePosition);
    if (!result) {
        return IPS_ALERT;
    }
    return IPS_BUSY;
}

IPState LX200StarGoFocuser::moveFocuserRelative(int relativePosition) {
    if (relativePosition == 0) {
        return IPS_OK;
    }
    uint32_t absolutePosition = getAbsoluteFocuserPositionFromRelative(relativePosition);
    return MoveAbsFocuser(absolutePosition);
}



bool LX200StarGoFocuser::AbortFocuser() {
    return sendAbortFocuser();
}

IPState LX200StarGoFocuser::syncFocuser(int absolutePosition) {
    bool result = sendSyncFocuserToPosition(absolutePosition);
    if (!result) {
        return IPS_ALERT;
    }
    return IPS_OK;
}


/***************************************************************************
 *
 ***************************************************************************/

bool LX200StarGoFocuser::isConnected() {
    if (baseDevice == nullptr) return false;
    return focuserActivated;
}

const char *LX200StarGoFocuser::getDeviceName() {
    if (baseDevice == nullptr) return "";
    return baseDevice->getDeviceName();
}

const char *LX200StarGoFocuser::getDefaultName()
{
    return deviceName;
}

bool LX200StarGoFocuser::activate(bool activate)
{
    bool result = true;
    if (activate == true && focuserActivated == false)
    {
        initProperties(deviceName);
        focuserActivated = activate;
        result = updateProperties();
    }
    else if (activate == false)
    {
        focuserActivated = activate;
        result = updateProperties();
    }
    return result;
}

bool LX200StarGoFocuser::saveConfigItems(FILE *fp)
{
    if (focuserActivated)
    {
        FocusReverseSP.save(fp);
        FocusSpeedNP.save(fp);
    }

    return true;
}


/***************************************************************************
 * LX200 queries, sent to baseDevice
 ***************************************************************************/


bool LX200StarGoFocuser::sendNewFocuserSpeed(int speed) {
    // Command  - :X1Caaaa*bb#
    // Response - Unknown
    bool valid = false;
    switch(speed) {
    case 1: valid = baseDevice->transmit(":X1C9000*01#"); break;
    case 2: valid = baseDevice->transmit(":X1C6000*01#"); break;
    case 3: valid = baseDevice->transmit(":X1C4000*01#"); break;
    case 4: valid = baseDevice->transmit(":X1C2500*01#"); break;
    case 5: valid = baseDevice->transmit(":X1C1000*05#"); break;
    case 6: valid = baseDevice->transmit(":X1C0750*10#"); break;
    case 7: valid = baseDevice->transmit(":X1C0500*20#"); break;
    case 8: valid = baseDevice->transmit(":X1C0250*30#"); break;
    case 9: valid = baseDevice->transmit(":X1C0100*40#"); break;
    case 10: valid = baseDevice->transmit(":X1C0060*50#"); break;
    default: DEBUGF(INDI::Logger::DBG_ERROR, "%s: Invalid focuser speed %d specified.", getDeviceName(), speed);
    }
    if (!valid) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to send new focuser speed command.", getDeviceName());
        return false;
    }
    return valid;
}



bool LX200StarGoFocuser::sendSyncFocuserToPosition(int position) {
    // Command  - :X0Cpppppp#
    // Response - Nothing
    char command[AVALON_COMMAND_BUFFER_LENGTH] = {0};
    sprintf(command, ":X0C%06d#", AVALON_FOCUSER_POSITION_OFFSET + ((focuserReversed == INDI_DISABLED) ? position : -position));
    if (!baseDevice->transmit(command)) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to send AUX1 sync command.", getDeviceName());
        return false;
    }
    return true;
}

bool LX200StarGoFocuser::sendQueryFocuserPosition(int* position) {
    // Command  - :X0BAUX1AS#
    // Response - AX1=ppppppp#
    baseDevice->flush();
    if(!baseDevice->transmit(":X0BAUX1AS#")) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to send AUX1 position request.", getDeviceName());
        return false;
    }
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    int bytesReceived = 0;
    if (!baseDevice->receive(response, &bytesReceived)) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to receive AUX1 position response.", getDeviceName());
        return false;
    }
    int tempPosition = 0;
    int returnCode = sscanf(response, "%*c%*c%*c%*c%07d", &tempPosition);
    if (returnCode <= 0) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to parse AUX1 position response '%s'.", getDeviceName(), response);
        return false;
    }
    (*position) = (tempPosition - AVALON_FOCUSER_POSITION_OFFSET);
    return true;
}

bool LX200StarGoFocuser::sendMoveFocuserToPosition(uint32_t position) {
    // Command  - :X16pppppp#
    // Response - Nothing
    targetFocuserPosition = (focuserReversed == INDI_DISABLED) ? position : -position;
    char command[AVALON_COMMAND_BUFFER_LENGTH] = {0};
    sprintf(command, ":X16%06d#", AVALON_FOCUSER_POSITION_OFFSET + targetFocuserPosition);
    if (!baseDevice->transmit(command)) {
        LOGF_ERROR("%s: Failed to send AUX1 goto command.", getDeviceName());
        return false;
    }
    return true;
}

bool LX200StarGoFocuser::sendAbortFocuser() {
    // Command  - :X0AAUX1ST#
    // Response - Nothing
    if (!baseDevice->transmit(":X0AAUX1ST#")) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to send AUX1 stop command.", getDeviceName());
        return false;
    }
    return true;
}


/************************************************************************
 * helper functions
 ************************************************************************/

bool LX200StarGoFocuser::isFocuserMoving() {
    return FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY;
}

bool LX200StarGoFocuser::atFocuserTargetPosition() {
    return static_cast<uint32_t>(FocusAbsPosNP[0].getValue()) == (focuserReversed == INDI_DISABLED) ? targetFocuserPosition : -targetFocuserPosition;
}



