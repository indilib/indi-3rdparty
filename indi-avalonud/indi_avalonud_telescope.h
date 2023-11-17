/*
    INDI Driver AVALON INSTRUMENTS StartGo2

    Copyright (C) 2020

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

#ifndef AUDTELESCOPE_H
#define AUDTELESCOPE_H

#include <string>

#include <indidevapi.h>
#include <inditelescope.h>
#include <indiguiderinterface.h>
#include <pthread.h>


#define MIN(a,b) (((a)<=(b))?(a):(b))


using namespace std;


class AUDTELESCOPE : public INDI::Telescope, public INDI::GuiderInterface
{

    enum FLIP { FLIP_ON, FLIP_OFF };
    enum
    {
        MOUNT_EQUATORIAL,
        MOUNT_ALTAZ
    };

public:
    AUDTELESCOPE();
    ~AUDTELESCOPE();

    // Standard INDI interface functions
    virtual bool Connect();
    virtual bool Disconnect();
    const char *getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool saveConfigItems(FILE *fp);
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

protected:
    virtual bool ReadScopeStatus() override;
    virtual bool Sync(double, double) override;
    virtual bool Goto(double, double) override;
    virtual bool Abort() override;
    virtual void TimerHit() override;

    // Parking
    virtual bool SetCurrentPark() override;
    virtual bool SetDefaultPark() override;
    virtual bool Park() override;
    virtual bool UnPark() override;

    virtual bool SetTrackMode(uint8_t mode) override;
    virtual bool SetTrackRate(double raRate, double deRate) override;
    virtual bool SetTrackEnabled(bool enabled) override;

    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

    virtual bool updateTime(ln_date *utc, double utc_offset) override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    // Guide
    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;

private:
    int tid;
    int mounttype;

    double targetRA;
    double targetDEC;
    double simulatedRA;
    double simulatedDEC;

    ISwitch MountTypeS[2];
    ISwitchVectorProperty MountTypeSP;

    IText ConfigT[1];
    ITextVectorProperty ConfigTP;

    INumber LocalEqN[2];
    INumberVectorProperty LocalEqNP;

    INumber AzAltN[2];
    INumberVectorProperty AzAltNP;

    INumber TimeN[3];
    INumberVectorProperty TimeNP;

    ISwitch HomeS[2];
    ISwitchVectorProperty HomeSP;

    ISwitch MeridianFlipS[2];
    ISwitchVectorProperty MeridianFlipSP;

    INumber MeridianFlipHAN[1];
    INumberVectorProperty MeridianFlipHANP;

    IText HWTypeT[1];
    ITextVectorProperty HWTypeTP;

    IText HWModelT[1];
    ITextVectorProperty HWModelTP;

    IText HWIdentifierT[1];
    ITextVectorProperty HWIdentifierTP;

    IText LowLevelSWT[2];
    ITextVectorProperty LowLevelSWTP;

    IText HighLevelSWT[2];
    ITextVectorProperty HighLevelSWTP;

    bool Slew(double, double, int);
    bool meridianFlipEnable(int);
    bool setMeridianFlipHA(double);
    bool SyncHome();
    bool SlewToHome();

    // Variables
    bool fAbort,fFirstTime,fTracking;
    int northernHemisphere;
    IPState slewState;
    TelescopeStatus previousTrackState;
    double trackspeedra,trackspeeddec;

    char* IPaddress;
    char* sendCommand(const char*,...);
    char* sendCommandOnce(const char*,...);
    char* sendRequest(const char*,...);

    void *context,*requester;
    char *lastErrorMsg;

    pthread_mutex_t connectionmutex;
};

#endif
