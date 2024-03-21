/*
    Avalon Unified Driver Telescope

    Copyright (C) 2020,2023

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
    virtual bool Connect() override;
    virtual bool Disconnect() override;

    const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;

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

    enum {
        MM_EQUATORIAL,
        MM_ALTAZ,
        MM_N
    };
    INDI::PropertySwitch MountModeSP {MM_N};
    INDI::PropertyText ConfigTP {1};
    enum {
        LEQ_HA,
        LEQ_DEC,
        LEQ_N
    };
    INDI::PropertyNumber LocalEqNP {LEQ_N};
    enum {
        ALTAZ_AZ,
        ALTAZ_ALT,
        ALTAZ_N
    };
    INDI::PropertyNumber AltAzNP {ALTAZ_N};
    enum {
        TTIME_JD,
        TTIME_UTC,
        TTIME_LST,
        TTIME_N
    };
    INDI::PropertyNumber TTimeNP {TTIME_N};
    enum {
        HOME_SYNC,
        HOME_SLEW,
        HOME_N
    };
    INDI::PropertySwitch HomeSP {HOME_N};
    enum {
        MFLIP_ON,
        MFLIP_OFF,
        MFLIP_N
    };
    INDI::PropertySwitch MeridianFlipSP {MFLIP_N};
    INDI::PropertyNumber MeridianFlipHANP {1};

    INDI::PropertyText HWTypeTP {1};
    INDI::PropertyText HWModelTP {1};
    INDI::PropertyText HWIdentifierTP {1};
    enum {
        LLSW_NAME,
        LLSW_VERSION,
        LLSW_N
    };
    INDI::PropertyText LowLevelSWTP {LLSW_N};
    enum {
        HLSW_NAME,
        HLSW_VERSION,
        HLSW_N
    };
    INDI::PropertyText HighLevelSWTP {HLSW_N};

    bool Slew(double, double, int);
    bool meridianFlipEnable(int);
    bool setMeridianFlipHA(double);
    bool SyncHome();
    bool SlewToHome();

    // Variables
    bool fFirstTime,fTracking;
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
