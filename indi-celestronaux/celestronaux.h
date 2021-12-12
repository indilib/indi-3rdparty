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

        //virtual bool HandleGetAutoguideRate(INDI_HO_AXIS axis, uint8_t rate);
        //virtual bool HandleSetAutoguideRate(INDI_EQ_AXIS axis);
        //virtual bool HandleGuidePulse(INDI_EQ_AXIS axis);
        //virtual bool HandleGuidePulseDone(INDI_EQ_AXIS axis, bool done);

        // TODO: Switch to AltAz from N-S/W-E
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        virtual bool ReadScopeStatus() override;
        virtual void TimerHit() override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        /////////////////////////////////////////////////////////////////////////////////////
        /// Motion Control
        /////////////////////////////////////////////////////////////////////////////////////
        bool stopAxis(INDI_HO_AXIS axis);
        bool isSlewing();

        /**
         * @brief SlewTo Go to a 24bit encoder position.
         * @param axis AZ or ALT
         * @param steps Encoder microsteps
         * @param fast If true, use fast command to reach target. If false, use slow command.
         * @return True if successful, false otherwise.
         */
        bool slewTo(INDI_HO_AXIS axis, uint32_t steps, bool fast = true);

        /**
         * @brief SlewByRate Slew an axis using variable rate speed.
         * @param axis AZ or ALT
         * @param rate -9 to +9. 0 means stop.
         * For AZ, negative means left while positive means right.
         * For Alt, negative is down while positive is up.
         * @return True if successful, false otherwise.
         */
        bool slewByRate(INDI_HO_AXIS axis, int8_t rate);

        /////////////////////////////////////////////////////////////////////////////////////
        /// Tracking
        /////////////////////////////////////////////////////////////////////////////////////
        bool SetTrackEnabled(bool enabled) override;
        bool SetTrackMode(uint8_t mode) override;
        bool SetTrackRate(double raRate, double deRate) override;
        void resetTracking();

        /**
         * @brief TrackByRate Set axis tracking rate in arcsecs/sec.
         * @param axis AZ or ALT
         * @param rate arcsecs/s. Zero would stop tracking.
         * For AZ, negative means left while positive means right.
         * For Alt, negative is down while positive is up.
         * @return True if successful, false otherwise.
         */
        bool trackByRate(INDI_HO_AXIS axis, int32_t rate);

        /**
         * @brief trackByRate Track using specific mode (sidereal, solar, or lunar)
         * @param axis AZ or ALT
         * @param mode sidereal, solar, or lunar
         * @return True if successful, false otherwise.
         */
        bool trackByMode(INDI_HO_AXIS axis, uint8_t mode);
        bool trackingRequested();

        bool getStatus(INDI_HO_AXIS axis);
        bool getEncoder(INDI_HO_AXIS axis);

        /////////////////////////////////////////////////////////////////////////////////////
        /// Coord Wrap
        /////////////////////////////////////////////////////////////////////////////////////
        bool setCordWrapEnabled(bool enable);
        bool getCordWrapEnabled();
        bool setCordWrapPosition(uint32_t steps);
        uint32_t getCordWrapPosition();

    private:
        /////////////////////////////////////////////////////////////////////////////////////
        /// Misc
        /////////////////////////////////////////////////////////////////////////////////////
        double getNorthAz();
        bool getVersion(AUXTargets target);
        void getVersions();
        void hex_dump(char *buf, AUXBuffer data, size_t size);
        double MicrostepsToDegrees(INDI_HO_AXIS axis, uint32_t steps);
        uint32_t DegreesToMicrosteps(INDI_HO_AXIS axis, double degrees);
        bool getCurrentRADE(INDI::IHorizontalCoordinates altaz, INDI::IEquatorialCoordinates &rade);
        int32_t clampStepsPerRevolution(int32_t);

        /////////////////////////////////////////////////////////////////////////////////////
        /// Guiding
        /////////////////////////////////////////////////////////////////////////////////////
        bool guidePulse(INDI_EQ_AXIS axis, uint32_t ms, int8_t rate);


    private:
        // Axis Information
        AxisStatus m_AxisStatus[2] {STOPPED, STOPPED};
        AxisDirection m_AxisDirection[2] {FORWARD, FORWARD};

        // Guiding offset in steps
        // For each pulse, we modify the offset so that we can add it to our current tracking traget
        int32_t m_GuideOffset[2] = {0, 0};
        double m_TrackRates[2] = {TRACKRATE_SIDEREAL, 0};

        // approach distance
        double Approach;

        // Tracking
        INDI::IEquatorialCoordinates m_SkyTrackingTarget { 0, 0 };
        INDI::IEquatorialCoordinates m_SkyCurrentRADE {0, 0};
        INDI::IHorizontalCoordinates m_MountCoordinates {0, 0};
        INDI::ElapsedTimer m_TrackingElapsedTimer;


        /////////////////////////////////////////////////////////////////////////////////////
        /// Auxiliary Command Communication
        /////////////////////////////////////////////////////////////////////////////////////
        bool sendAUXCommand(AUXCommand &command);
        void closeConnection();
        void emulateGPS(AUXCommand &m);
        bool serialReadResponse(AUXCommand c);
        bool tcpReadResponse();
        bool readAUXResponse(AUXCommand c);
        bool processResponse(AUXCommand &cmd);
        int sendBuffer(AUXBuffer buf);
        void formatVersionString(char *s, int n, uint8_t *verBuf);

        // GPS Emulation
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

        // Manual Slewing NSWE
        bool m_ManualMotionActive { false };

        // Debug
        uint32_t DBG_CAUX {0};
        uint32_t DBG_SERIAL {0};

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication
        ///////////////////////////////////////////////////////////////////////////////
        int m_ModemControl {0};
        void setRTS(bool rts);
        bool waitCTS(float timeout);
        bool detectRTSCTS();
        bool detectHC(char *version, size_t size);
        int response_data_size;
        int aux_tty_read(char *buf, int bufsiz, int timeout, int *n);
        int aux_tty_write (char *buf, int bufsiz, float timeout, int *n);
        bool tty_set_speed(speed_t speed);

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

        // Encoders
        INDI::PropertyNumber EncoderNP {2};
        // Angles
        INDI::PropertyNumber AngleNP {2};

        ///////////////////////////////////////////////////////////////////////////////
        /// Static Const Private Variables
        ///////////////////////////////////////////////////////////////////////////////

    private:

        // One definition rule (ODR) constants
        // AUX commands use 24bit integer as a representation of angle in units of
        // fractional revolutions. Thus 2^24 steps makes full revolution.
        static constexpr int32_t STEPS_PER_REVOLUTION {16777216};
        static constexpr double STEPS_PER_DEGREE {STEPS_PER_REVOLUTION / 360.0};
        static constexpr double DEGREES_PER_STEP {360.0 / STEPS_PER_REVOLUTION};

        static constexpr double DEFAULT_SLEW_RATE {STEPS_PER_DEGREE * 2.0};
        static constexpr double MAX_ALT {90.0 * STEPS_PER_DEGREE};
        static constexpr double MIN_ALT {-90.0 * STEPS_PER_DEGREE};

        // MC_SET_POS_GUIDERATE & MC_SET_NEG_GUIDERATE use 24bit number rate in
        // units of 0.25 arc sec per sec. So 1 arcsec/s ==> rate = 4
        static constexpr uint8_t RATE_PER_ARCSEC {4};

        static constexpr uint32_t BUFFER_SIZE {10240};
        // seconds
        static constexpr uint8_t READ_TIMEOUT {1};
        // ms
        static constexpr uint8_t CTS_TIMEOUT {100};
        // Coord Wrap
        static constexpr const char *CORDWRAP_TAB {"Coord Wrap"};
        static constexpr const char *MOUNTINFO_TAB {"Mount info"};
        // Track modes
        static constexpr uint16_t AUX_SIDEREAL {0xffff};
        static constexpr uint16_t AUX_SOLAR {0xfffe};
        static constexpr uint16_t AUX_LUNAR {0xfffd};


};
