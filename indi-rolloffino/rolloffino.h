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

class RollOffIno : public INDI::Dome
{
public:
    RollOffIno();
    ~RollOffIno() override = default;
    bool initProperties() override;
    void ISGetProperties(const char *dev) override;
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    const char *getDefaultName() override;
    bool updateProperties() override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool saveConfigItems(FILE *fp) override;
    bool ISSnoopDevice(XMLEle *root) override;
    bool Handshake() override;

protected:
    bool Connect() override;
    bool Disconnect() override;
    void TimerHit() override;
    IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
    IPState Park() override;
    IPState UnPark() override;
    bool Abort() override;

private:
    bool getFullOpenedLimitSwitch(bool *);
    bool getFullClosedLimitSwitch(bool *);
    void updateRoofStatus();
    void
    updateActionStatus();
    bool getRoofLockedSwitch(bool *);
    bool getRoofAuxSwitch(bool *);
    bool getActionSwitch(const char *, bool *);
    bool setRoofLock(bool);
    bool setRoofAux(bool);
    bool setAction(const char *, bool);
    bool readRoofSwitch(const char *, bool *);
    bool roofOpen();
    bool roofClose();
    bool roofAbort();
    bool pushRoofButton(const char *, bool, bool);
    bool initialContact();
    bool evaluateResponse(char *, bool *);
    bool writeIno(const char *);
    bool readIno(char *);
    void msSleep(int);
    bool setupConditions();
    void restoreLabels();
    void storeLabels();
    float CalcTimeLeft(timeval);
    bool actionSwitchUsed(const char*);
    bool actionCmdUsed(const char*);

////////////////
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

    ISwitch LockS[2];
    ISwitchVectorProperty LockSP;
    enum {LOCK_ENABLE, LOCK_DISABLE};

    ISwitch AuxS[2];
    ISwitchVectorProperty AuxSP;
    enum {AUX_ENABLE, AUX_DISABLE};

    INumber RoofTimeoutN[1]{};
    INumberVectorProperty RoofTimeoutNP;
    enum {EXPIRED_CLEAR, EXPIRED_OPEN, EXPIRED_CLOSE };

    ISState fullyOpenedLimitSwitch{ISS_OFF};
    ISState fullyClosedLimitSwitch{ISS_OFF};
    ISState roofLockedSwitch{ISS_OFF};
    ISState roofAuxiliarySwitch{ISS_OFF};

    ILight RoofStatusL[5];
    ILightVectorProperty RoofStatusLP;
    enum {ROOF_STATUS_OPENED, ROOF_STATUS_CLOSED, ROOF_STATUS_MOVING, ROOF_STATUS_LOCKED, ROOF_STATUS_AUXSTATE};

///////////
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

    const char *ACTION_CONTROL_TAB = "Action Controls";
    const char* act_cmd_used[MAX_ACTIONS] = {ACTION_ACT1_CMD, ACTION_ACT2_CMD, ACTION_ACT3_CMD, ACTION_ACT4_CMD, ACTION_ACT5_CMD, ACTION_ACT6_CMD, ACTION_ACT7_CMD, ACTION_ACT8_CMD};
    enum {ACTION_ENABLE, ACTION_DISABLE};
    const char *actionSwitchesText[MAX_ACTIONS] = {"Act1", "Act2", "Act3", "Act4", "Act5", "Act6", "Act7", "Act8"};
    ISwitch *actionSwitches[MAX_ACTIONS] = {Act1S, Act2S, Act3S, Act4S, Act5S, Act6S, Act7S, Act8S};
    ISwitchVectorProperty *actionSwitchesVP [MAX_ACTIONS] = {&Act1SP, &Act2SP, &Act3SP, &Act4SP, &Act5SP, &Act6SP, &Act7SP, &Act8SP};
    ISwitch Act1S[2];
    ISwitchVectorProperty Act1SP;
    ISwitch Act2S[2];
    ISwitchVectorProperty Act2SP;
    ISwitch Act3S[2];
    ISwitchVectorProperty Act3SP;
    ISwitch Act4S[2];
    ISwitchVectorProperty Act4SP;
    ISwitch Act5S[2];
    ISwitchVectorProperty Act5SP;
    ISwitch Act6S[2];
    ISwitchVectorProperty Act6SP;
    ISwitch Act7S[2];
    ISwitchVectorProperty Act7SP;
    ISwitch Act8S[2];
    ISwitchVectorProperty Act8SP;

// Action responses, if any. All that is done is to set the action light indicating on status
    const char* action_state_used[MAX_ACTIONS] = {"ACT1STATE", "ACT2STATE", "ACT3STATE", "ACT4STATE", "ACT5STATE", "ACT6STATE", "ACT7STATE", "ACT8STATE"};
    ISState actionStatusState[MAX_ACTIONS] = {ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF};
    ILight ActionStatusL[MAX_ACTIONS];
    ILightVectorProperty ActionStatusLP;

/////////////////
// Action Labels.

    // Some way to provide a meaningful description
#define MAX_LABEL 32
    const char *ACTION_LABEL_TAB = "Action Label";
    const char* action_labels[MAX_ACTIONS] = {"LABEL1", "LABEL2", "LABEL3", "LABEL4", "LABEL5", "LABEL6", "LABEL7", "LABEL8"};
    const char* label_names[MAX_ACTIONS] = {"Label 1", "Label 2", "Label 3", "Label 4", "Label 5", "Label 6", "Label 7", "Label 8"};
    const char* label_init[MAX_ACTIONS] = {"Action 1", "Action 2", "Action 3", "Action 4", "Action 5", "Action 6", "Action 7", "Action 8"};
    IText labelsT[MAX_ACTIONS] = {Label1T, Label2T, Label3T, Label4T};
    IText Label1T {};
    IText Label2T {};
    IText Label3T {};
    IText Label4T {};
    IText Label5T {};
    IText Label6T {};
    IText Label7T {};
    IText Label8T {};

    ITextVectorProperty *labelsTP[MAX_ACTIONS] = {&Label1TP, &Label2TP, &Label3TP, &Label4TP, &Label5TP, &Label6TP, &Label7TP, &Label8TP};
    ITextVectorProperty Label1TP;
    ITextVectorProperty Label2TP;
    ITextVectorProperty Label3TP;
    ITextVectorProperty Label4TP;
    ITextVectorProperty Label5TP;
    ITextVectorProperty Label6TP;
    ITextVectorProperty Label7TP;
    ITextVectorProperty Label8TP;

    char current_labels [MAX_ACTIONS][MAXINDILABEL];
    const char *label_file = "rolloff-labels.txt";
    const char *custom_name;
    double MotionRequest{0};
    struct timeval MotionStart{0, 0};
    bool contactEstablished = false;
    bool roofOpening = false;
    bool roofClosing = false;
    unsigned int roofTimedOut;
    bool simRoofOpen = false;
    bool simRoofClosed = true;
    unsigned int communicationErrors = 0;
    int actionCount = 0;                        // From remote connection
    bool actionState[MAX_ACTIONS];

};
