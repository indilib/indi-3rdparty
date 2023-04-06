/*******************************************************************************
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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

#pragma once

#include "indidome.h"

class RollOffIno : public INDI::Dome
{
  public:
    RollOffIno();
    virtual ~RollOffIno() override = default;

    virtual bool initProperties();
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n);
    const char *getDefaultName();
    bool updateProperties();
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool saveConfigItems(FILE *fp);
    virtual bool ISSnoopDevice(XMLEle *root);
    virtual bool Handshake();

  protected:
    bool Connect();
    bool Disconnect();

    void TimerHit();

    virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);
    virtual IPState Park();
    virtual IPState UnPark();
    virtual bool Abort();

    virtual bool getFullOpenedLimitSwitch(bool*);
    virtual bool getFullClosedLimitSwitch(bool*);

private:
    void updateRoofStatus();
    bool getRoofLockedSwitch(bool*);
    bool getRoofAuxSwitch(bool*);
    bool setRoofLock(bool switchOn);
    bool setRoofAux(bool switchOn);
    bool readRoofSwitch(const char* roofSwitchId, bool* result);
    bool roofOpen();
    bool roofClose();
    bool roofAbort();
    bool pushRoofButton(const char*, bool switchOn, bool ignoreLock);
    bool initialContact();
    bool evaluateResponse(char*, bool*);
    bool writeIno(const char*);
    bool readIno(char*);
    void msSleep(int);

    bool setupConditions();
    float CalcTimeLeft(timeval);
    double MotionRequest { 0 };
    struct timeval MotionStart { 0, 0 };
    bool contactEstablished = false;
    bool roofOpening = false;
    bool roofClosing = false;
    ILight RoofStatusL[5];
    ILightVectorProperty RoofStatusLP;
    enum { ROOF_STATUS_OPENED, ROOF_STATUS_CLOSED, ROOF_STATUS_MOVING, ROOF_STATUS_LOCKED, ROOF_STATUS_AUXSTATE };

    ISwitch LockS[2];
    ISwitchVectorProperty LockSP;
    enum { LOCK_ENABLE, LOCK_DISABLE };

    ISwitch AuxS[2];
    ISwitchVectorProperty AuxSP;
    enum { AUX_ENABLE, AUX_DISABLE };

    ISState fullyOpenedLimitSwitch {ISS_OFF};
    ISState fullyClosedLimitSwitch {ISS_OFF};
    ISState roofLockedSwitch {ISS_OFF};
    ISState roofAuxiliarySwitch {ISS_OFF};
    INumber RoofTimeoutN[1] {};
    INumberVectorProperty RoofTimeoutNP;
    enum { EXPIRED_CLEAR, EXPIRED_OPEN, EXPIRED_CLOSE };
    unsigned int roofTimedOut;
    bool simRoofOpen = false;
    bool simRoofClosed = true;
    unsigned int communicationErrors = 0;
    
};

