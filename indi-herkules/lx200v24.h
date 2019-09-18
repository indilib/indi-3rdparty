

/*
    Herkules V24 driver
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

#ifndef HERKULES_H
#define HERKULES_H

#pragma once

#include <mounts/lx200telescope.h>
#include <indicom.h>
#include <indilogger.h>
#include <termios.h>

#include <cstring>
#include <string>
#include <unistd.h>

#define LX200_TIMEOUT                                 5 /* FD timeout in seconds */
#define RB_MAX_LEN                                   64
#define TCS_TIMEOUT                                   1 /* 50ms? */
#define TCS_COMMAND_BUFFER_LENGTH                    32
#define TCS_RESPONSE_BUFFER_LENGTH                   32
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

// Herkules specific tabs
extern const char *INFO_TAB; // Infotab for Versionummer?

class LX200Herkules : public LX200Telescope
{
    public:
        /*enum TrackMode
        enum MotorsState
        {
            MOTORS_OFF = 0,
            MOTORS_DEC_ONLY = 1,
            MOTORS_RA_ONLY = 2,
            MOTORS_ON = 3
        };
        MotorsState CurrentMotorsState {MOTORS_OFF};*/
        TelescopeSlewRate CurrentSlewRate {SLEW_MAX};

        LX200Herkules();
        ~LX200Herkules() {}

        virtual const char *getDefaultName() override;
        virtual bool Handshake() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool updateProperties() override;
        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev)override;

        // helper functions
        virtual bool receive(char* buffer, int wait = TCS_TIMEOUT);
        virtual bool receive(char* buffer, char end, int wait = TCS_TIMEOUT);
        virtual void flush();
        virtual bool transmit(const char* buffer);
        //virtual bool SetTrackMode(uint8_t mode) override;

    protected:

          // parking position
        ISwitchVectorProperty MountSetParkSP;
        ISwitch MountSetParkS[1];

        // Speed definitions
        INumber SystemSlewSpeedP[1];
        INumberVectorProperty SystemSlewSpeedNP;

        int controller_format { LX200_LONG_FORMAT };

        // override LX200Generic
        virtual void getBasicData() override;
        virtual bool ReadScopeStatus() override;
        virtual bool Park() override;
        virtual void SetParked(bool isparked);
        virtual bool UnPark() override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual bool Goto(double ra, double dec) override;
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        // Herkules stuff
        bool setParkPosition(ISState *states, char *names[], int n);
        bool getSystemSlewSpeed (int *xx);
        bool setSystemSlewSpeed (int xx);

        // location
        //virtual bool sendScopeLocation();
        //double LocalSiderealTime(double longitude);
        //bool setLocalSiderealTime(double longitude);
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        //virtual bool getSiteLatitude() {return true;};
        //virtual bool getSiteLongitude() {return true;};
        //virtual bool getLST_String(char* input);
        bool getTrackFrequency(double *value);


        // queries to the scope interface. Wait for specified end character
        // Unfortunately wait is only defined in seconds in tty_read -> tty_timout
        virtual bool sendQuery(const char* cmd, char* response, char end, int wait = TCS_TIMEOUT);
        // Wait for default "#' character
        virtual bool sendQuery(const char* cmd, char* response, int wait = TCS_TIMEOUT);
        virtual bool getFirmwareInfo();
        virtual bool setSiteLatitude(double Lat);
        virtual bool setSiteLongitude(double Long);
        virtual bool getJSONData_Y(int jindex, char *jstr);
        virtual bool getJSONData_gp(int jindex, char *jstr);
        virtual bool getMotorStatus(int *xSpeed, int *ySpeed);
        virtual bool getParkHomeStatus (char* status);
        //virtual bool setMountGotoHome();
        virtual bool setMountParkPosition();

        // meridian flip

        //virtual bool syncSideOfPier();
        bool checkLX200Format();
        // Guide Commands
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;
        virtual bool SetSlewRate(int index) override;
        virtual int SendPulseCmd(int8_t direction, uint32_t duration_msec) override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackRate(double raRate, double deRate) override;
        // NSWE Motion Commands
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool Sync(double ra, double dec) override;
        bool setObjectCoords(double ra, double dec);
        virtual bool setLocalDate(uint8_t days, uint8_t months, uint16_t years) override;
        virtual bool setLocalTime24(uint8_t hour, uint8_t minute, uint8_t second) override;
        virtual bool setUTCOffset(double offset) override;
        //virtual bool getLocalTime(char *timeString) override;
        //virtual bool getLocalDate(char *dateString) override;
        //virtual bool getUTFOffset(double *offset) override;

        // Abort ALL motion
        virtual bool Abort() override;
        int MoveTo(int direction);

        bool setSlewMode(int slewMode);

};
inline bool LX200Herkules::sendQuery(const char* cmd, char* response, int wait)
{
    return sendQuery(cmd, response, '#', wait);
}
inline bool LX200Herkules::receive(char* buffer, int wait)
{
    return receive(buffer, '#', wait);
}

#endif // Herkules_V24
