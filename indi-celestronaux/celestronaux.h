/*
    Celestron Aux Mount Driver.

    Copyright (C) 2020 Paweł T. Jochym
    Copyright (C) 2020 Fabrizio Pollastri
    Copyright (C) 2021 Jasem Mutlaq

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

#pragma once

#include <indicom.h>
#include <libindi/indiguiderinterface.h>
#include <inditelescope.h>
#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>
#include <alignment/AlignmentSubsystemForDrivers.h>

#include "auxproto.h"

class CelestronAUX :
    public INDI::Telescope,
    public INDI::GuiderInterface,
    public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
    public:
        CelestronAUX();
        ~CelestronAUX();

        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;
	long requestedCordwrapPos;
	double getNorthAz();

    protected:
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool saveConfigItems(FILE *fp) override;
        //virtual bool Connect() override;
        virtual bool Handshake() override;
        virtual bool Disconnect() override;

        virtual const char *getDefaultName() override;
        INDI::IHorizontalCoordinates AltAzFromRaDec(double ra, double dec, double ts);

        virtual bool Sync(double ra, double dec) override;
        virtual bool Goto(double ra, double dec) override;
        virtual bool Abort() override;
        virtual bool Park() override;
        virtual bool UnPark() override;

        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        virtual bool HandleGetAutoguideRate(INDI_EQ_AXIS axis,uint8_t rate);
        virtual bool HandleSetAutoguideRate(INDI_EQ_AXIS axis);
        virtual bool HandleGuidePulse(INDI_EQ_AXIS axis);
        virtual bool HandleGuidePulseDone(INDI_EQ_AXIS axis, bool done);

        // TODO: Switch to AltAz from N-S/W-E
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        virtual bool ReadScopeStatus() override;
        virtual void TimerHit() override;

        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        bool trackingRequested();

        int32_t GetALT();
        int32_t GetAZ();
        bool slewing();
        bool Slew(AUXTargets trg, int rate);
        bool SlewALT(int32_t rate);
        bool SlewAZ(int32_t rate);
        bool GoToFast(int32_t alt, int32_t az, bool track);
        bool GoToSlow(int32_t alt, int32_t az, bool track);
        bool setCordwrap(bool enable);
        bool getCordwrap();
    public:
        bool setCordwrapPos(int32_t pos);
        long getCordwrapPos();
        bool getCWBase();
    private:
        bool getVersion(AUXTargets trg);
        void getVersions();
        bool Track(int32_t altRate, int32_t azRate);
        bool SetTrackEnabled(bool enabled) override;
        bool TimerTick(double dt);
        bool GuidePulse(INDI_EQ_AXIS axis, uint32_t ms, int8_t rate);


    private:

        // mount type
        MountType_t requestedMountType;
        MountType_t currentMountType;

        bool pastAlignmentSubsystemStatus = true;
        bool currentAlignmentSubsystemStatus;

        enum ScopeStatus_t
        {
            IDLE,
            SLEWING_FAST,
            APPROACH,
            SLEWING_SLOW,
            SLEWING_MANUAL,
            TRACKING
        };
        ScopeStatus_t ScopeStatus;

        enum AxisStatus
        {
            STOPPED,
            SLEWING
        };

        enum AxisDirection
        {
            FORWARD,
            REVERSE
        };

        AxisStatus AxisStatusALT;
        AxisDirection AxisDirectionALT;

        AxisStatus AxisStatusAZ;
        AxisDirection AxisDirectionAZ;

        double Approach; // approach distance

        // Previous motion direction
        // TODO: Switch to AltAz from N-S/W-E
        typedef enum
        {
            PREVIOUS_NS_MOTION_NORTH   = DIRECTION_NORTH,
            PREVIOUS_NS_MOTION_SOUTH   = DIRECTION_SOUTH,
            PREVIOUS_NS_MOTION_UNKNOWN = -1
        } PreviousNSMotion_t;
        typedef enum
        {
            PREVIOUS_WE_MOTION_WEST    = DIRECTION_WEST,
            PREVIOUS_WE_MOTION_EAST    = DIRECTION_EAST,
            PREVIOUS_WE_MOTION_UNKNOWN = -1
        } PreviousWEMotion_t;

        // GoTo
        INDI::IEquatorialCoordinates GoToTarget;
        int slewTicks, maxSlewTicks;

        // Tracking
        INDI::IEquatorialCoordinates CurrentTrackingTarget;
        INDI::IEquatorialCoordinates NewTrackingTarget;

        // Tracing in timer tick
        int TraceThisTickCount;
        bool TraceThisTick;

        // connection
        bool isRTSCTS;
        bool isHC;

        uint32_t DBG_CAUX {0};
        uint32_t DBG_SERIAL {0};

        int32_t range360int(int32_t);
        void initScope(char const *ip, int port);
        void initScope();
        bool detectNetScope(bool set_ip);
        bool detectNetScope();
        void closeConnection();
        void emulateGPS(AUXCommand &m);
        bool serialReadResponse(AUXCommand c);
        bool tcpReadResponse();
        bool readAUXResponse(AUXCommand c);
        bool processResponse(AUXCommand &cmd);
        void querryStatus();
        int sendBuffer(int PortFD, AUXBuffer buf);
        bool sendAUXCommand(AUXCommand &c);
        void formatVersionString(char *s, int n, uint8_t *verBuf);

        // Current steps from controller 
        // AUX protocol uses signed 24bit integers for positions
        int32_t m_AltSteps {0};
        int32_t m_AzSteps {0};
        // FIXME: Current rate in steps per sec?
        int32_t m_AltRate {0};
        int32_t m_AzRate {0};
        // Desired target steps in both axis
        int32_t targetAlt {0};
        int32_t targetAz {0};
        // FIXME: Combined slew rate?
        int32_t slewRate {0};

        bool m_Tracking {false};
        bool m_SlewingAlt {false}, m_SlewingAz {false};
        bool gpsemu;
        bool cw_base_sky = false ;



        uint8_t m_MainBoardVersion[4] {0};
        uint8_t m_AltitudeVersion[4] {0};
        uint8_t m_AzimuthVersion[4] {0};
        uint8_t m_HCVersion[4] {0};
        uint8_t m_BATVersion[4] {0};
        uint8_t m_WiFiVersion[4] {0};
        uint8_t m_GPSVersion[4] {0};

        // Coord Wrap
        bool m_CordWrapActive {false};
        int32_t m_CordWrapPosition {0};

        // FP
        int modem_ctrl;
        void setRTS(bool rts);
        bool waitCTS(float timeout);
        bool detectRTSCTS();
        bool detectHC(char *version, size_t size);
        int response_data_size;
        int aux_tty_read(int PortFD, char *buf, int bufsiz, int timeout, int *n);
        int aux_tty_write (int PortFD, char *buf, int bufsiz, float timeout, int *n);
        bool tty_set_speed(int PortFD, speed_t speed);
        void hex_dump(char *buf, AUXBuffer data, size_t size);


        ///////////////////////////////////////////////////////////////////////////////
        /// Celestron AUX Properties
        ///////////////////////////////////////////////////////////////////////////////

        // Firmware
        IText FirmwareT[10] {};
        ITextVectorProperty FirmwareTP;
        enum {FW_HC, FW_MB, FW_AZM, FW_ALT, FW_WiFi, FW_BAT, FW_GPS};
        // Networked Mount autodetect
        ISwitch NetDetectS[1];
        ISwitchVectorProperty NetDetectSP;
        // Mount type
        ISwitch MountTypeS[2];
        ISwitchVectorProperty MountTypeSP;
        enum
        {
            MOUNT_EQUATORIAL,
            MOUNT_ALTAZ
        };
        // Mount Cordwrap
        ISwitch CordWrapS[2];
        ISwitchVectorProperty CordWrapSP;
        enum { CORDWRAP_OFF, CORDWRAP_ON };
        ISwitch CWPosS[4];
        ISwitchVectorProperty CWPosSP;
        enum { CORDWRAP_N, CORDWRAP_E, CORDWRAP_S, CORDWRAP_W };
        // Cordwrap base (0-encoder/True directions)
        ISwitch CWBaseS[2];
        ISwitchVectorProperty CWBaseSP;
        enum {CW_BASE_ENC, CW_BASE_SKY}; // Use 0-encoders / Sky directions as base for parking and cordwrap

        // GPS emulator
        ISwitch GPSEmuS[2];
        ISwitchVectorProperty GPSEmuSP;
        enum { GPSEMU_OFF, GPSEMU_ON };
        // guide
        INumber GuideRateN[2]{};
        INumberVectorProperty GuideRateNP;

        ///////////////////////////////////////////////////////////////////////////////
        /// Static Const Private Variables
        ///////////////////////////////////////////////////////////////////////////////

        // One definition rule (ODR) constants
        // AUX commands use 24bit integer as a representation of angle in units of
        // fractional revolutions. Thus 2^24 steps makes full revolution.
        static constexpr int32_t STEPS_PER_REVOLUTION {16777216};
    public:
        static constexpr double STEPS_PER_DEGREE {STEPS_PER_REVOLUTION / 360.0};
    private:
        static constexpr double DEFAULT_SLEW_RATE {STEPS_PER_DEGREE * 2.0};
        static constexpr double MAX_ALT {90.0 * STEPS_PER_DEGREE};
        static constexpr double MIN_ALT {-90.0 * STEPS_PER_DEGREE};

        // The guide rate is probably (???) measured in 1024 arcmin/min
        // This is based on experimentation and guesswork.
        // The rate is calculated in steps/min - thus conversion is required.
        // The best experimental value was 1.315 which is quite close
        // to 61440/STEPS_PER_DEGREE = 1.318359375.
        static constexpr double TRACK_SCALE {61440 / STEPS_PER_DEGREE};
        static constexpr double SIDERAL_RATE {1.002737909350795};
        static constexpr uint8_t MAX_SLEW_RATE {9};
        static constexpr uint8_t FIND_SLEW_RATE {7};
        static constexpr uint8_t CENTERING_SLEW_RATE {3};
        static constexpr uint8_t GUIDE_SLEW_RATE {2};
        static constexpr uint32_t BUFFER_SIZE {10240};
        // seconds
        static constexpr uint8_t READ_TIMEOUT {1};
        // ms
        static constexpr uint8_t CTS_TIMEOUT {100};
        // ms
        static constexpr uint8_t RTS_DELAY {50};

};
