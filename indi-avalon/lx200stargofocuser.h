#ifndef LX200STARGOFOCUSER_H
#define LX200STARGOFOCUSER_H

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

#include "lx200stargo.h"

#include "indifocuserinterface.h"
#include "defaultdevice.h"


class LX200StarGoFocuser : public INDI::DefaultDevice, public INDI::FocuserInterface
{
public:
    LX200StarGoFocuser(LX200StarGo* defaultDevice, const char* name);
    virtual ~LX200StarGoFocuser() override = default;

    void initProperties(const char *groupName);
    bool updateProperties() override;
    bool ReadFocuserStatus();

    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    const char *getDeviceName();
    const char *getDefaultName() override;

    bool isConnected();

    bool activate(bool enabled);

    bool saveConfigItems(FILE *fp) override;

protected:

    // Avalon specifics
    bool changeFocusSpeed(double values[], char* names[], int n);
    bool changeFocusMotion(ISState* states, char* names[], int n);
    bool changeFocusTimer(double values[], char* names[], int n);
    bool changeFocusAbsPos(double values[], char* names[], int n);
    bool changeFocusRelPos(double values[], char* names[], int n);
    bool changeFocusAbort(ISState* states, char* names[], int n);
    bool changeFocusSyncPos(double values[], char* names[], int n);
    bool setFocuserDirection(ISState *states, char *names[], int n);

    bool SetFocuserSpeed(int speed) override;
    IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
    IPState MoveAbsFocuser(uint32_t absolutePosition) override;
    IPState moveFocuserRelative(int relativePosition);
    bool AbortFocuser() override;
    IPState syncFocuser(int absolutePosition);

    INumberVectorProperty FocusSyncPosNP;
    INumber FocusSyncPosN[1];

    uint32_t targetFocuserPosition;
    bool startMovingFocuserInward;
    bool startMovingFocuserOutward;
    uint32_t moveFocuserDurationRemaining;
    bool focuserActivated;
    int focuserReversed = INDI_DISABLED;


    // LX200 commands
    bool sendNewFocuserSpeed(int speed);
    bool sendMoveFocuserToPosition(uint32_t position);
    bool sendAbortFocuser();
    bool sendSyncFocuserToPosition(int position);
    bool sendQueryFocuserPosition(int* position);

    // helper functions
    bool isFocuserMoving();
    bool atFocuserTargetPosition();

    bool validateFocusSpeed(int speed);
    bool validateFocusTimer(int time);
    bool validateFocusAbsPos(uint32_t absolutePosition);
    bool validateFocusRelPos(int relativePosition);
    bool validateFocusSyncPos(int absolutePosition);

    uint32_t getAbsoluteFocuserPositionFromRelative(int relativePosition);

private:
    LX200StarGo* baseDevice;
    const char* deviceName;


};

#endif // LX200STARGOFOCUSER_H
