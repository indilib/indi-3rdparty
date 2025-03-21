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

#pragma once
#include "indidome.h"
#include "indicom.h"
#include "termios.h"
#include "indiinputinterface.h"
#include "indioutputinterface.h"
#include "inditimer.h"

class RollOffIno : public INDI::Dome, public INDI::InputInterface, public INDI::OutputInterface
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
    virtual bool UpdateDigitalInputs() override;
    virtual bool UpdateAnalogInputs() override;
    virtual bool UpdateDigitalOutputs() override;
    virtual bool CommandOutput(uint32_t, OutputState) override;
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual void TimerHit() override;
    virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
    virtual IPState Park() override;
    virtual IPState UnPark() override;
    virtual bool Abort() override;

private:
    bool Handshake() override;
    void updateRoofStatus();
    bool getRoofSwitch(const char *, bool *, ISState *);
    bool sendRoofCommand(const char *, bool, bool);
    bool initialContact();
    bool evaluateResponse(char *, char *, bool *);
    bool writeIno(const char *);
    bool readIno(char *);
    void msSleep(int);
    bool checkConditions();
    void roofTimerExpired();

#define MAX_CNTRL_COM_ERR 10         // Maximum consecutive errors communicating with Arduino
#define MAXOUTBUF        64          // Sized to contain outgoing command requests
#define MAXINPBUF        256         // Sized for maximum overall input
#define MAXINOWAIT       3           // seconds
#define MAX_ACTIONS 8
#define POLLING_PERIOD    3000

// Main tab Roof controls
#define ROOF_OPENED_SWITCH "OPENED"
#define ROOF_CLOSED_SWITCH "CLOSED"
#define ROOF_LOCKED_SWITCH "LOCKED"
#define ROOF_AUX_SWITCH    "AUXSTATE"
#define ROOF_OPEN_CMD     "OPEN"
#define ROOF_CLOSE_CMD    "CLOSE"
#define ROOF_ABORT_CMD    "ABORT"
#define ROOF_LOCK_CMD     "LOCK"
#define ROOF_AUX_CMD      "AUXSET"

// Actions (digital outputs)
const char* outRoRino[MAX_ACTIONS] = {
        "(SET:ACT1SET:ON)",
        "(SET:ACT2SET:ON)",
        "(SET:ACT3SET:ON)",
        "(SET:ACT4SET:ON)",
        "(SET:ACT5SET:ON)",
        "(SET:ACT6SET:ON)",
        "(SET:ACT7SET:ON)",
        "(SET:ACT8SET:ON)"
};

// Action responses (digital inputs)
const char* inpRoRino[MAX_ACTIONS] = {
        "(GET:ACT1STATE:0)",
        "(GET:ACT2STATE:0)",
        "(GET:ACT3STATE:0)",
        "(GET:ACT4STATE:0)",
        "GET:ACT5STATE:0)",
        "(GET:ACT6STATE:0)",
        "(GET:ACT7STATE:0)",
        "(GET:ACT8STATE:0)"
};

//////////////////////////////////////
// Switch
//////////////////////////////////////
    // Roof Lock Switch
    INDI::PropertySwitch LockSP {2};
    enum {LOCK_ENABLE, LOCK_DISABLE};

    // Auxiliary Switch
    INDI::PropertySwitch AuxSP {2};
    enum {AUX_ENABLE, AUX_DISABLE};

//////////////////////////////////////
// Text
//////////////////////////////////////

//////////////////////////////////////
// Number
//////////////////////////////////////
    INDI::PropertyNumber RoofTimeoutNP {1};

//////////////////////////////////////
// Lights.
//////////////////////////////////////
    INDI::PropertyLight RoofStatusLP {5};
    enum {ROOF_STATUS_OPENED, ROOF_STATUS_CLOSED, ROOF_STATUS_MOVING, ROOF_STATUS_LOCKED, ROOF_STATUS_AUXSTATE};
    INDI::PropertyLight ActionStatusLP {MAX_ACTIONS};

    static const char RORINO_STOP_CHAR {0x29};         // ')'
    enum {EXPIRED_CLEAR, EXPIRED_OPEN, EXPIRED_CLOSE, EXPIRED_ABORT};
    unsigned int roofTimedOut = EXPIRED_CLEAR;
    INDI::Timer roofMoveTimer;
    bool contactEstablished = false;
    bool roofOpening = false;
    bool roofClosing = false;
    unsigned int communicationErrors = 0; // Added for WiFi benefit
    unsigned int actionCount = 0;         // # of optional input/output Actions set by Arduino
    ISState fullyOpenedLimitSwitch{ISS_OFF};
    ISState fullyClosedLimitSwitch{ISS_OFF};
    ISState roofLockedSwitch{ISS_OFF};
    ISState roofAuxiliarySwitch{ISS_OFF};
};
