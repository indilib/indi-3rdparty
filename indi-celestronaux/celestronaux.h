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
#include <indiguiderinterface.h>
#include <inditelescope.h>
#include <indielapsedtimer.h>
#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>
#include <alignment/AlignmentSubsystemForDrivers.h>
#include <indipropertyswitch.h>
#include <indipropertynumber.h>
#include <indipropertytext.h>
#include <termios.h>

#include "auxproto.h"

class CelestronAUX :
    public INDI::Telescope,
    public INDI::GuiderInterface,
    public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
    public:
        CelestronAUX();
        ~CelestronAUX();

        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

        // Type defs
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

        // Public to enable setting from ISSnoop
        void syncCoordWrapPosition();

    protected:
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool saveConfigItems(FILE *fp) override;
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

        virtual bool HandleGetAutoguideRate(INDI_EQ_AXIS axis, uint8_t rate);
        virtual bool HandleSetAutoguideRate(INDI_EQ_AXIS axis);
        virtual bool HandleGuidePulse(INDI_EQ_AXIS axis);
        virtual bool HandleGuidePulseDone(INDI_EQ_AXIS axis, bool done);

        // TODO: Switch to AltAz from N-S/W-E
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        virtual bool ReadScopeStatus() override;
        virtual void TimerHit() override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        /////////////////////////////////////////////////////////////////////////////////////
        /// Motion Control
        /////////////////////////////////////////////////////////////////////////////////////
        bool isSlewing();
        bool Slew(AUXTargets target, int rate);
        bool SlewALT(int32_t rate);
        bool SlewAZ(int32_t rate);
        bool GoToFast(int32_t alt, int32_t az, bool track);
        bool GoToSlow(int32_t alt, int32_t az, bool track);

        /////////////////////////////////////////////////////////////////////////////////////
        /// Tracking
        /////////////////////////////////////////////////////////////////////////////////////
        bool Track(int32_t altRate, int32_t azRate);
        bool SetTrackEnabled(bool enabled) override;
        bool trackingRequested();

        /////////////////////////////////////////////////////////////////////////////////////
        /// Coord Wrap
        /////////////////////////////////////////////////////////////////////////////////////
        bool setCordWrapEnabled(bool enable);
        bool getCordWrapEnabled();
        bool setCordWrapPosition(int32_t pos);
        uint32_t getCordWrapPosition();

private:
        /////////////////////////////////////////////////////////////////////////////////////
        /// Misc
        /////////////////////////////////////////////////////////////////////////////////////
        double getNorthAz();
        bool getVersion(AUXTargets target);
        void getVersions();
        void hex_dump(char *buf, AUXBuffer data, size_t size);
        int32_t clampStepsPerRevolution(int32_t);

        /////////////////////////////////////////////////////////////////////////////////////
        /// Guiding
        /////////////////////////////////////////////////////////////////////////////////////
        bool TimerTick(double dt);
        bool GuidePulse(INDI_EQ_AXIS axis, uint32_t ms, int8_t rate);


    private:        
        // Axis Information
        AxisStatus AxisStatusALT;
        AxisDirection AxisDirectionALT;
        AxisStatus AxisStatusAZ;
        AxisDirection AxisDirectionAZ;

        // Motion Control

        // approach distance
        double Approach;

        // Tracking
        INDI::IEquatorialCoordinates m_SkyTrackingTarget { 0, 0 };
        INDI::IEquatorialCoordinates m_SkyCurrentRADE {0, 0};
        INDI::IHorizontalCoordinates m_MountAltAz {0, 0};
        long OldTrackingTarget[2] { 0, 0 };
        INDI::ElapsedTimer m_TrackingElapsedTimer;


        void closeConnection();
        void emulateGPS(AUXCommand &m);
        bool serialReadResponse(AUXCommand c);
        bool tcpReadResponse();
        bool readAUXResponse(AUXCommand c);
        bool processResponse(AUXCommand &cmd);
        void queryStatus();
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

        bool m_SlewingAlt {false}, m_SlewingAz {false};
        bool m_GPSEmulation {false};

        // Firmware
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
        uint32_t m_RequestedCordwrapPos;

        // Debug
        uint32_t DBG_CAUX {0};
        uint32_t DBG_SERIAL {0};

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication
        ///////////////////////////////////////////////////////////////////////////////
        int modem_ctrl;
        void setRTS(bool rts);
        bool waitCTS(float timeout);
        bool detectRTSCTS();
        bool detectHC(char *version, size_t size);
        int response_data_size;
        int aux_tty_read(int PortFD, char *buf, int bufsiz, int timeout, int *n);
        int aux_tty_write (int PortFD, char *buf, int bufsiz, float timeout, int *n);
        bool tty_set_speed(int PortFD, speed_t speed);

        // connection
        bool m_IsRTSCTS;
        bool m_isHandController;

        ///////////////////////////////////////////////////////////////////////////////
        /// Celestron AUX Properties
        ///////////////////////////////////////////////////////////////////////////////

        // Firmware
        INDI::PropertyText FirmwareTP {7};
        enum {FW_HC, FW_MB, FW_AZM, FW_ALT, FW_WiFi, FW_BAT, FW_GPS};
        // Mount type
        INDI::PropertySwitch MountTypeSP {2};
        enum
        {
            MOUNT_EQUATORIAL,
            MOUNT_ALTAZ
        };

        // Mount Cord wrap Toogle
        INDI::PropertySwitch CordWrapToggleSP {2};
        enum { CORDWRAP_OFF, CORDWRAP_ON };

        // Mount Coord wrap Position
        INDI::PropertySwitch CordWrapPositionSP {4};
        enum { CORDWRAP_N, CORDWRAP_E, CORDWRAP_S, CORDWRAP_W };

        // Cordwrap base (0-encoder/True directions)
        // Use 0-encoders / Sky directions as base for parking and cordwrap
        INDI::PropertySwitch CordWrapBaseSP {2};
        enum {CW_BASE_ENC, CW_BASE_SKY};

        // GPS emulator
        INDI::PropertySwitch GPSEmuSP {2};
        enum { GPSEMU_OFF, GPSEMU_ON };

        // Guide Rate
        INDI::PropertyNumber GuideRateNP {2};

        ///////////////////////////////////////////////////////////////////////////////
        /// Static Const Private Variables
        ///////////////////////////////////////////////////////////////////////////////

    private:

        // One definition rule (ODR) constants
        // AUX commands use 24bit integer as a representation of angle in units of
        // fractional revolutions. Thus 2^24 steps makes full revolution.
        static constexpr int32_t STEPS_PER_REVOLUTION {16777216};
        static constexpr double STEPS_PER_DEGREE {STEPS_PER_REVOLUTION / 360.0};

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
        static constexpr uint32_t BUFFER_SIZE {10240};
        // seconds
        static constexpr uint8_t READ_TIMEOUT {1};
        // ms
        static constexpr uint8_t CTS_TIMEOUT {100};
        // ms
        static constexpr uint8_t RTS_DELAY {50};
        // Coord Wrap
        static constexpr const char *CORDWRAP_TAB {"Coord Wrap"};
        static constexpr const char *MOUNTINFO_TAB {"Mount info"};

};
