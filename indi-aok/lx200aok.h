/*
    AOK Skywalker driver
    (based on Avalon driver)

    Copyright (C) T. Schriber

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

#ifndef SKYWALKER_H
#define SKYWALKER_H

#pragma once

#include <mounts/lx200telescope.h>
#include <indicom.h>
#include <indilogger.h>
#include <termios.h>

#include <cstring>
#include <string>
// #include <regex> --> std::map
#include <unistd.h>

//#define LX200_TIMEOUT                               5 /* FD timeout in seconds */
#define RB_MAX_LEN                                   64
#define TCS_TIMEOUT                                   1 /* 50ms? */
#define TCS_COMMAND_BUFFER_LENGTH                    32
#define TCS_RESPONSE_BUFFER_LENGTH                   32
#define TCS_JSON_BUFFER_LENGTH                      256
#define TCS_NOANSWER                                  0

enum TDirection
{
    LX200_NORTH,
    LX200_WEST,
    LX200_EAST,
    LX200_SOUTH,
    LX200_ALL
};
enum TSlew
{
    LX200_SLEW_MAX,
    LX200_SLEW_FIND,
    LX200_SLEW_CENTER,
    LX200_SLEW_GUIDE
};
enum TFormat
{
    LX200_SHORT_FORMAT,
    LX200_LONG_FORMAT,
    LX200_LONGER_FORMAT
};

// Skywalker specific tabs
extern const char *INFO_TAB; // Infotab for versionumber, etc.

class LX200Skywalker : public LX200Telescope
{
    public:
        enum MountState
        {
            MOUNT_LOCKED = 0,
            MOUNT_UNLOCKED = 1
        };
        MountState CurrentMountState {MOUNT_LOCKED};
        TelescopeSlewRate CurrentSlewRate {SLEW_MAX};

        LX200Skywalker();
        ~LX200Skywalker() {}

        virtual const char *getDefaultName() override;
        virtual bool Handshake() override;
        virtual void ISGetProperties(const char *dev)override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool updateProperties() override;
        virtual bool initProperties() override;
        virtual bool isSlewComplete() override;

    protected:
        // parking position
        ISwitchVectorProperty MountSetParkSP;
        ISwitch MountSetParkS[1];

        // Speed definitions
        INumber SystemSlewSpeedP[1];
        INumberVectorProperty SystemSlewSpeedNP;

        // Mount locked/unlocked
        ISwitch MountStateS[2];
        ISwitchVectorProperty MountStateSP;

        // Info
        ITextVectorProperty FirmwareVersionTP;
        IText FirmwareVersionT[1] {};

        int controller_format { LX200_LONG_FORMAT };

        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual void getBasicData() override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool saveConfigItems(FILE *fp) override;
        bool checkLX200EquatorialFormat();
        void notifyPierSide(bool west);
        void notifyMountLock(bool locked);
        void notifyTrackState(INDI::Telescope::TelescopeStatus state);
        virtual bool setLocalDate(uint8_t days, uint8_t months, uint16_t years) override;
        virtual bool setLocalTime24(uint8_t hour, uint8_t minute, uint8_t second) override;
        virtual bool setUTCOffset(double offset) override;
        virtual bool setSiteLongitude(double Long);
        virtual bool setSiteLatitude(double Lat);
        bool SetMountLock(bool enable);
        bool SetCurrentPark() override;
        bool SetDefaultPark() override;
        bool getSystemSlewSpeed (int *xx);
        bool setSystemSlewSpeed (int xx);
        virtual bool SetSlewRate(int index) override;
        bool setSlewMode(int slewMode);
        bool setObjectCoords(double ra, double dec);
        bool MountTracking();
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackRate(double raRate, double deRate) override;
        bool getTrackFrequency(double *value);
        virtual bool Goto(double ra, double dec) override;
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool Sync(double ra, double dec) override;
        bool Park() override;
        bool UnPark() override;
        bool SavePark();
        bool SyncDefaultPark();
        virtual bool Abort() override;
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;
        virtual int SendPulseCmd(int8_t direction, uint32_t duration_msec) override;
        //virtual bool SetTrackMode(uint8_t mode) override;

    private:
        // helper functions
        enum class val : uint8_t // scoped enumeration of all JSON-parametervalues "val" (answer to ':gp')
                {
                    maxspeed,   // max slewspeed (encoded)
                    Rlp,        // motorforce x (for EQ: rightascension)
                    rb3,        // stiffness x
                    Dlp,        // motorforce y (for EQ: declination)
                    db3,        // stiffness y
                    IP1,        // IP address
                    IP2,
                    IP3,
                    IP4,
                    mask,       // IP mask
                    port,       // IP port
                    version,    // FW versionidentifier (EQ: equatorial, AA: horizontal, ...)
                    lock,       // motorlock
                    count
                };

                enum class V1 : uint8_t // scoped enumeration of - so far - required JSON-parametervalues "V1" (answer to ':Y#')
                {
                    posX,       // motorposition x
                    posY,       // motorposition y
                    stime,      // startime
                    ubatt,      // voltage (encoded)
                    db2,        // motorparameter
                    gstate,     // devicestatus (PierSide encoded)
                    bstate,     // busstatus
                    count
                };

                // std::map<std::string, std::string> JSONtopic{ {":gp", "\"val\""}, {":Y#", "\"V1\""} };

        bool getFirmwareInfo(char* vstring);
        bool MountLocked();
        bool PierSideWest();
        bool getJSONData(const char* cmd, const uint8_t tok_index, char *data);
        // queries to the scope interface. Wait for specified end character
        // Unfortunately wait is only defined in seconds in tty_read -> tty_timout
        bool sendQuery(const char* cmd, char* response, char end, int wait = TCS_TIMEOUT);
        // Wait for default "#' character
        bool sendQuery(const char* cmd, char* response, int wait = TCS_TIMEOUT);
        bool transmit(const char* buffer);
        bool receive(char* buffer, int wait = TCS_TIMEOUT);
        bool receive(char* buffer, char end, int wait = TCS_TIMEOUT);
};

inline bool LX200Skywalker::sendQuery(const char* cmd, char* response, int wait)
{
    return sendQuery(cmd, response, '#', wait);
}

inline bool LX200Skywalker::receive(char* buffer, int wait)
{
    return receive(buffer, '#', wait);
}

#endif
