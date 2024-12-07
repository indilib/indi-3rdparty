/*
 Edited version of the Dome Simulator
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.
 Copyright(c) 2023 Jasem Mutlaq. All rights reserved.

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

#pragma once
#include "indidome.h"
#include "indicom.h"
#include "termios.h"

class RollOffIno : public INDI::Dome
{
public:
    RollOffIno();
    virtual ~RollOffIno() = default;
        
    virtual bool initProperties() override;
    const char *getDefaultName() override;
    bool updateProperties() override;
    void ISGetProperties(const char *dev) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual bool ISSnoopDevice(XMLEle *root) override;

protected:
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual void TimerHit() override;
    virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
    virtual IPState Park() override;
    virtual IPState UnPark() override;
    virtual bool Abort() override;

private:
    bool Handshake();
    void updateRoofStatus();
    void updateActionStatus();
    bool getRoofSwitch(const char *, bool *, ISState *);
    bool sendRoofCommand(const char *, bool, bool);
    bool initialContact();
    bool evaluateResponse(char *, char *, bool *);
    bool writeIno(const char *);
    bool readIno(char *);
    void msSleep(int);
    bool checkConditions();
    float CalcTimeLeft(timeval);
    bool actionStateUsed(const char*);
    bool actionCmdUsed(const char*);

// Roof controls
#define ROOF_OPENED_SWITCH "OPENED"
#define ROOF_CLOSED_SWITCH "CLOSED"
#define ROOF_LOCKED_SWITCH "LOCKED"
#define ROOF_AUX_SWITCH    "AUXSTATE"
#define ROOF_OPEN_CMD     "OPEN"
#define ROOF_CLOSE_CMD    "CLOSE"
#define ROOF_ABORT_CMD    "ABORT"
#define ROOF_LOCK_CMD     "LOCK"
#define ROOF_AUX_CMD      "AUXSET"

// Actions
#define MAX_ACTIONS 8     // Maximum actions supported
#define ACTION_ACT1_CMD   "ACT1SET"
#define ACTION_ACT2_CMD   "ACT2SET"
#define ACTION_ACT3_CMD   "ACT3SET"
#define ACTION_ACT4_CMD   "ACT4SET"
#define ACTION_ACT5_CMD   "ACT5SET"
#define ACTION_ACT6_CMD   "ACT6SET"
#define ACTION_ACT7_CMD   "ACT7SET"
#define ACTION_ACT8_CMD   "ACT8SET"

    // Action Labels.
#define MAX_LABEL 32
    const char* ACTION_LABEL_TAB = "Action Labels";
    const char* action_labels[MAX_ACTIONS] = {"LABEL1", "LABEL2", "LABEL3", "LABEL4", "LABEL5", "LABEL6", "LABEL7", "LABEL8"};
    const char* label_init[MAX_ACTIONS] = {"Action 1", "Action 2", "Action 3", "Action 4", "Action 5", "Action 6", "Action 7", "Action 8"};

    // Actions
    const char* ACTION_CONTROL_TAB = "Action Controls";
    const char* act_cmd_used[MAX_ACTIONS] = {ACTION_ACT1_CMD, ACTION_ACT2_CMD, ACTION_ACT3_CMD, ACTION_ACT4_CMD, ACTION_ACT5_CMD, ACTION_ACT6_CMD, ACTION_ACT7_CMD, ACTION_ACT8_CMD};
    const char* actionSwitchesText[MAX_ACTIONS] = {"Act1", "Act2", "Act3", "Act4", "Act5", "Act6", "Act7", "Act8"};

    // Action responses, if any. All that is done is to set the action light indicating on status
    const char* action_state_used[MAX_ACTIONS] = {"ACT1STATE", "ACT2STATE", "ACT3STATE", "ACT4STATE", "ACT5STATE", "ACT6STATE", "ACT7STATE", "ACT8STATE"};
    ISState actionStatusState[MAX_ACTIONS] = {ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF};

//////////////////////////////////////
// Properties
//////////////////////////////////////

/////////////////
// Switches.

    // Roof Lock Switch
    INDI::PropertySwitch LockSP {2};
    enum {LOCK_ENABLE, LOCK_DISABLE};

    // Auxiliary Switch
    INDI::PropertySwitch AuxSP {2};
    enum {AUX_ENABLE, AUX_DISABLE};

    // Action Switches
    INDI::PropertySwitch Act1SP {2};
    INDI::PropertySwitch Act2SP {2};
    INDI::PropertySwitch Act3SP {2};
    INDI::PropertySwitch Act4SP {2};
    INDI::PropertySwitch Act5SP {2};
    INDI::PropertySwitch Act6SP {2};
    INDI::PropertySwitch Act7SP {2};
    INDI::PropertySwitch Act8SP {2};
    enum {ACT_ENABLE, ACT_DISABLE};
    INDI::PropertySwitch actionSwitches_sp [MAX_ACTIONS] = {Act1SP, Act2SP, Act3SP, Act4SP, Act5SP, Act6SP, Act7SP, Act8SP};

/////////////////
// Texts.
    // Action Labels
    INDI::PropertyText Label1TP {1};
    INDI::PropertyText Label2TP {1};
    INDI::PropertyText Label3TP {1};
    INDI::PropertyText Label4TP {1};
    INDI::PropertyText Label5TP {1};
    INDI::PropertyText Label6TP {1};
    INDI::PropertyText Label7TP {1};
    INDI::PropertyText Label8TP {1};
    INDI::PropertyText actionLabels_tp [MAX_ACTIONS] = {Label1TP, Label2TP, Label3TP, Label4TP, Label5TP, Label6TP, Label7TP, Label8TP};

/////////////////
// Numbers
    INDI::PropertyNumber RoofTimeoutNP {1};
    enum {EXPIRED_CLEAR, EXPIRED_OPEN, EXPIRED_CLOSE };

/////////////////
// Lights.
    INDI::PropertyLight RoofStatusLP {5};
    enum {ROOF_STATUS_OPENED, ROOF_STATUS_CLOSED, ROOF_STATUS_MOVING, ROOF_STATUS_LOCKED, ROOF_STATUS_AUXSTATE};

    INDI::PropertyLight ActionStatusLP {MAX_ACTIONS};

////////////////////////////////////////////////

    double MotionRequest{0};
    struct timeval MotionStart{0, 0};
    bool contactEstablished = false;
    bool roofOpening = false;
    bool roofClosing = false;
    unsigned int roofTimedOut = EXPIRED_CLEAR;
    unsigned int communicationErrors = 0;
    unsigned int actionCount = 0;                        // From remote connection
    bool actionStatus[MAX_ACTIONS]{false};
    ISState fullyOpenedLimitSwitch{ISS_OFF};
    ISState fullyClosedLimitSwitch{ISS_OFF};
    ISState roofLockedSwitch{ISS_OFF};
    ISState roofAuxiliarySwitch{ISS_OFF};

};
