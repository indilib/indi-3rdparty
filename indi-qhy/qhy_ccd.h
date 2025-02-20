/*
 QHY INDI Driver

 Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2014 Zhirong Li (lzr@qhyccd.com)

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

#include <qhyccd.h>
#include <indiccd.h>
#include <indifilterinterface.h>
#include <unistd.h>
#include <functional>
#include <pthread.h>

#define DEVICE struct usb_device *

class QHYCCD : public INDI::CCD, public INDI::FilterInterface
{
    public:
        QHYCCD(const char *m_Name);
        virtual ~QHYCCD() override = default;

        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewNumber(const char *dev, const char *m_Name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *m_Name, char *texts[], char *names[], int num) override;
        virtual bool ISNewSwitch(const char *dev, const char *m_Name, ISState *states, char *names[], int n) override;

        const char *name() const
        {
            return m_Name;
        }

        INumberVectorProperty getLEDStartPosNP() const;
        void setLEDStartPosNP(const INumberVectorProperty &value);

    protected:
        // Properties
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        // Connection
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        // Temperature
        virtual int SetTemperature(double temperature) override;
        virtual bool StartExposure(float duration) override;
        virtual bool AbortExposure() override;

        // Debug
        virtual void debugTriggered(bool enable) override;

        // CCD
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        // Filter Wheel CFW
        virtual int QueryFilter() override;
        virtual bool SelectFilter(int position) override;

        // Streaming
        virtual bool StartStreaming() override;
        virtual bool StopStreaming() override;

        // Misc.
        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual const char *getDefaultName() override;
        void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;

        /////////////////////////////////////////////////////////////////////////////
        /// Camera Properties
        /////////////////////////////////////////////////////////////////////////////

        /////////////////////////////////////////////////////////////////////////////
        /// Properties: Camera Cooling Control
        /////////////////////////////////////////////////////////////////////////////
        // SDK Version
        ITextVectorProperty SDKVersionTP;
        IText SDKVersionT[1] {};
        /////////////////////////////////////////////////////////////////////////////
        /// Properties: Binning Support
        /////////////////////////////////////////////////////////////////////////////

        bool m_SupportedBins[4];
        enum
        {
            Bin1x1,
            Bin2x2,
            Bin3x3,
            Bin4x4,
        };


        // Cooler Switch
        ISwitchVectorProperty CoolerSP;
        ISwitch CoolerS[2];
        enum
        {
            COOLER_ON,
            COOLER_OFF,
        };

        // Cooler Power
        INumberVectorProperty CoolerNP;
        INumber CoolerN[1];

        ISwitchVectorProperty CoolerModeSP;
        ISwitch CoolerModeS[2];
        enum
        {
            COOLER_AUTOMATIC,
            COOLER_MANUAL,
        };


        // Overscan area Switch
        //NEW CODE - Add support for overscan/calibration area
        ISwitchVectorProperty OverscanAreaSP;
        ISwitch OverscanAreaS[2];
        enum
        {
            INDI_ENABLED,   //Overscan Area included
            INDI_DISABLED,  //Overscan Area excluded
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Properties: Image Adjustment Controls
        /////////////////////////////////////////////////////////////////////////////
        // Gain Control
        INumberVectorProperty GainNP;
        INumber GainN[1];

        // Offset Control
        INumberVectorProperty OffsetNP;
        INumber OffsetN[1];

        // Read mode Control
        INumber ReadModeN[1];
        INumberVectorProperty ReadModeNP;

        /////////////////////////////////////////////////////////////////////////////
        /// Properties: USB Controls
        /////////////////////////////////////////////////////////////////////////////
        // USB Speed Control
        INumberVectorProperty SpeedNP;
        INumber SpeedN[1];

        // USB Traffic Control
        INumber USBTrafficN[1];
        INumberVectorProperty USBTrafficNP;

        // USB Buffer Control
        INumber USBBufferN[1];
        INumberVectorProperty USBBufferNP;

        // Humidity Readout
        INumber HumidityN[1];
        INumberVectorProperty HumidityNP;
        /////////////////////////////////////////////////////////////////////////////
        /// Properties: Utility Controls
        /////////////////////////////////////////////////////////////////////////////
        // Amp glow control
        ISwitchVectorProperty AMPGlowSP;
        ISwitch AMPGlowS[3];
        enum
        {
            AMP_AUTO,
            AMP_ON,
            AMP_OFF
        };
        /////////////////////////////////////////////////////////////////////////////
        /// Properties: GPS Controls
        /////////////////////////////////////////////////////////////////////////////

        // Slaving Mode
        ISwitchVectorProperty GPSSlavingSP;
        ISwitch GPSSlavingS[2];
        enum
        {
            SLAVING_MASTER,
            SLAVING_SLAVE,
        };

        // Slaving Params (for slaves only)
        INumberVectorProperty GPSSlavingParamNP;
        INumber GPSSlavingParamN[5];
        enum
        {
            PARAM_TARGET_SEC,
            PARAM_TARGET_USEC,
            PARAM_DELTAT_SEC,
            PARAM_DELTAT_USEC,
            PARAM_EXP_TIME
        };

        // VCOX Frequency
        INumberVectorProperty VCOXFreqNP;
        INumber VCOXFreqN[1];

        // LED Calibration
        ISwitchVectorProperty GPSLEDCalibrationSP;
        ISwitch GPSLEDCalibrationS[2];

        // LED Pulse Position for Starting/Stopping Exposure
        INumberVectorProperty GPSLEDStartPosNP;
        INumberVectorProperty GPSLEDEndPosNP;
        INumber GPSLEDStartPosN[2];
        INumber GPSLEDEndPosN[2];
        enum
        {
            LED_PULSE_POSITION,
            LED_PULSE_WIDTH,
        };

        // GPS header On/Off
        ISwitchVectorProperty GPSControlSP;
        ISwitch GPSControlS[2];

        // GPS Status
        ILightVectorProperty GPSStateLP;
        ILight GPSStateL[4];

        // GPS Data Header
        ITextVectorProperty GPSDataHeaderTP;
        IText GPSDataHeaderT[6] {};
        enum
        {
            GPS_DATA_SEQ_NUMBER,
            GPS_DATA_WIDTH,
            GPS_DATA_HEIGHT,
            GPS_DATA_LONGITUDE,
            GPS_DATA_LATITUDE,
            GPS_DATA_MAX_CLOCK,
        };

        // GPS Data Start
        ITextVectorProperty GPSDataStartTP;
        IText GPSDataStartT[4] {};
        enum
        {
            GPS_DATA_START_FLAG,
            GPS_DATA_START_SEC,
            GPS_DATA_START_USEC,
            GPS_DATA_START_TS,
        };

        // GPS Data End
        ITextVectorProperty GPSDataEndTP;
        IText GPSDataEndT[4] {};
        enum
        {
            GPS_DATA_END_FLAG,
            GPS_DATA_END_SEC,
            GPS_DATA_END_USEC,
            GPS_DATA_END_TS,
        };

        // GPS Data Now
        ITextVectorProperty GPSDataNowTP;
        IText GPSDataNowT[4] {};
        enum
        {
            GPS_DATA_NOW_FLAG,
            GPS_DATA_NOW_SEC,
            GPS_DATA_NOW_USEC,
            GPS_DATA_NOW_TS,
        };


    private:
        /////////////////////////////////////////////////////////////////////////////
        /// Camera Structures
        /////////////////////////////////////////////////////////////////////////////
        typedef enum ImageState
        {
            StateNone = 0,
            StateIdle,
            StateStream,
            StateExposure,
            StateRestartExposure,
            StateAbort,
            StateTerminate,
            StateTerminated
        } ImageState;

        struct
        {
            uint32_t subX = 0;
            uint32_t subY = 0;
            uint32_t subW = 0;
            uint32_t subH = 0;
        } effectiveROI, sensorROI;  //NEW CODE - Add support for overscan/calibration area, obsolete overscanROI

        typedef struct QHYReadModeInfo
        {
            char label[128];  // Will be zero-initialized
            uint32_t id;
            uint32_t subW;
            uint32_t subH;
        } QHYReadModeInfo; // N.R. - Add support for read mode selection

        typedef enum GPSState
        {
            GPS_ON,
            GPS_SEARCHING,
            GPS_LOCKING,
            GPS_LOCKED
        } GPSState;

        struct
        {
            // Sequences
            uint32_t seqNumber = 0;
            uint32_t seqNumber_old = 0;
            uint8_t tempNumber = 0;

            // Dimension
            uint16_t width = 0;
            uint16_t height = 0;

            // Location
            double latitude = 0;
            double longitude = 0;

            // Start Time
            uint8_t start_flag = 0;
            uint32_t start_sec = 0;
            double start_us = 0;
            double start_jd = 0;
            char start_js_ts[MAXINDIDEVICE] = {0};

            // End Time
            uint8_t end_flag = 0;
            uint32_t end_sec = 0;
            double end_us = 0;
            double end_jd = 0;
            char end_js_ts[MAXINDIDEVICE] = {0};

            // Now time
            uint8_t now_flag = 0;
            uint32_t now_sec = 0;
            double now_us = 0;
            double now_jd = 0;
            char now_js_ts[MAXINDIDEVICE] = {0};

            // Clock
            uint32_t max_clock = 0;

            // GPS Status
            GPSState gps_status = GPS_ON;
        } GPSHeader;

        struct
        {
            double latitude = 0;
            double longitude = 0;
            time_t start_time;
            time_t end_time;
            time_t frame_time;
        } GPSData;


        /////////////////////////////////////////////////////////////////////////////
        /// Image Capture
        /////////////////////////////////////////////////////////////////////////////
        static void *imagingHelper(void *context);
        void *imagingThreadEntry();
        void streamVideo();
        void getExposure();
        void exposureSetRequest(ImageState request);
        int grabImage();

        /////////////////////////////////////////////////////////////////////////////
        /// Cooling
        /////////////////////////////////////////////////////////////////////////////
        // Set Cooler Mode
        void setCoolerMode(uint8_t mode);
        // Enable/disable cooler
        void setCoolerEnabled(bool enable);
        // Temperature update
        void updateTemperature();
        static void updateTemperatureHelper(void *);

        /////////////////////////////////////////////////////////////////////////////
        /// Misc
        /////////////////////////////////////////////////////////////////////////////
        double calcTimeLeft();
        // Setup basic CCD parameters on connection
        bool setupParams();
        // Check if the camera is QHY5PII-C model
        bool isQHY5PIIC();
        // Call when max filter count is known
        bool updateFilterProperties();
        // Decode GPS Header
        void decodeGPSHeader();
        /**
         * @brief JStoJD Convert Julian Second to Julian Date
         * @param JS Julian Second
         * @param us microsends
         * @return Julian Date
         */
        double JStoJD(uint32_t JS, double us);
        void JDtoISO8601(double JD, char *iso8601);

        /////////////////////////////////////////////////////////////////////////////
        /// Camera Capabilities
        /////////////////////////////////////////////////////////////////////////////
        bool HasUSBTraffic { false };
        bool HasUSBSpeed { false };
        bool HasGain { false };
        bool HasOffset { false };
        bool HasFilters { false };
        bool HasTransferBit { false };
        bool HasCoolerAutoMode { false };
        bool HasCoolerManualMode { false };
        bool HasReadMode { false };
        bool HasGPS { false };
        bool HasHumidity { false };
        bool HasAmpGlow { false };
        //NEW CODE - Add support for overscan/calibration area
        bool HasOverscanArea { false };
        bool IgnoreOverscanArea { true };

        /////////////////////////////////////////////////////////////////////////////
        /// Private Variables
        /////////////////////////////////////////////////////////////////////////////
        char m_Name[MAXINDIDEVICE];
        char m_CamID[MAXINDIDEVICE];
        // Requested target temperature
        double m_TemperatureRequest {0};
        // Requested target PWM
        double m_PWMRequest { -1 };
        // Max filter count.
        int m_MaxFilterCount { -1 };
        // Temperature Timer
        int m_TemperatureTimerID;
        // Camera Handle
        qhyccd_handle *m_CameraHandle {nullptr};
        // Camera Image Frame Type
        INDI::CCDChip::CCD_FRAME m_ImageFrameType;
        // Exposure progress
        double m_ExposureRequest;
        // Last exposure request in microseconds
        uint32_t m_LastExposureRequestuS;
        struct timeval ExpStart;
        // Gain
        double m_LastGainRequest = 1e6;
        // Filter Wheel Timeout
        uint16_t m_FilterCheckCounter = 0;
        // Camera's current stream operating mode (0: exposure mode, 1: stream mode)
        uint8_t currentQHYStreamMode = 0;
        // Number of read modes which the camera supports
        uint32_t numReadModes = 0;
        // currently set read mode
        uint32_t currentQHYReadMode;
        // dynamic array to hold read mode information
        QHYReadModeInfo *readModeInfo = nullptr;


        /////////////////////////////////////////////////////////////////////////////
        /// Threading
        /////////////////////////////////////////////////////////////////////////////
        ImageState m_ThreadRequest;
        ImageState m_ThreadState;
        pthread_t m_ImagingThread;
        pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

        void logQHYMessages(const std::string &message);
        std::function<void(const std::string &)> m_QHYLogCallback;

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * GPS_CONTROL_TAB = "GPS Control";
        static constexpr const char * GPS_DATA_TAB = "GPS Data";
        static constexpr uint64_t QHY_SER_US_EPOCH = 62948880000000000; // offset to SER epoch January 1, 1 AD
};
