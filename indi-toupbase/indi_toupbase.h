/*
 INDI Altair Driver

 Copyright (C) 2018-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <map>
#include <indiccd.h>
#include <inditimer.h>

#ifdef BUILD_TOUPCAM
#include <toupcam.h>
#define FP(x) Toupcam_##x
#define CP(x) TOUPCAM_##x
#define XP(x) Toupcam##x
#define THAND HToupcam
#define DNAME "ToupTek"
#elif BUILD_ALTAIRCAM
#include <altaircam.h>
#define FP(x) Altaircam_##x
#define CP(x) ALTAIRCAM_##x
#define XP(x) Altaircam##x
#define THAND HAltaircam
#define DNAME "Altair"
#elif BUILD_BRESSERCAM
#include <bressercam.h>
#define FP(x) Bressercam_##x
#define CP(x) BRESSERCAM_##x
#define XP(x) Bressercam##x
#define THAND HBressercam
#define DNAME "Bresser"
#elif BUILD_MALLINCAM
#include <mallincam.h>
#define FP(x) Mallincam_##x
#define CP(x) MALLINCAM_##x
#define XP(x) Mallincam##x
#define THAND HMallincam
#define DNAME "MALLINCAM"
#elif BUILD_NNCAM
#include <nncam.h>
#define FP(x) Nncam_##x
#define CP(x) NNCAM_##x
#define XP(x) Nncam##x
#define THAND HNncam
#define DNAME "Nn"
#elif BUILD_OGMACAM
#include <ogmacam.h>
#define FP(x) Ogmacam_##x
#define CP(x) OGMACAM_##x
#define XP(x) Ogmacam##x
#define THAND HOgmacam
#define DNAME "OGMAVision"
#elif BUILD_OMEGONPROCAM
#include <omegonprocam.h>
#define FP(x) Omegonprocam_##x
#define CP(x) OMEGONPROCAM_##x
#define XP(x) Omegonprocam##x
#define THAND HOmegonprocam
#define DNAME "Astroshop"
#elif BUILD_STARSHOOTG
#include <starshootg.h>
#define FP(x) Starshootg_##x
#define CP(x) STARSHOOTG_##x
#define XP(x) Starshootg##x
#define THAND HStarshootg
#define DNAME "Orion"
#elif BUILD_TSCAM
#include <tscam.h>
#define FP(x) Tscam_##x
#define CP(x) TSCAM_##x
#define XP(x) Tscam##x
#define THAND HTscam
#define DNAME "Teleskop"
#endif

#define BITDEPTH_FLAG   (CP(FLAG_RAW10) | CP(FLAG_RAW12) | CP(FLAG_RAW14) | CP(FLAG_RAW16))

class ToupBase : public INDI::CCD
{
    public:
        explicit ToupBase(const XP(DeviceV2) *instance);
        virtual ~ToupBase() override;

        virtual const char *getDefaultName() override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual void checkTemperatureTarget() override {}
        virtual bool StartExposure(float duration) override;
        virtual bool AbortExposure() override;

    protected:
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        // Streaming
        virtual bool StartStreaming() override;
        virtual bool StopStreaming() override;

        virtual void TimerHit() override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;

        virtual bool SetCaptureFormat(uint8_t index) override;

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        // ASI specific keywords
        virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;

        // Save config
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        static std::map<HRESULT, std::string> errCodes;
        static std::string errorCodes(HRESULT rc);

        enum eGUIDEDIRECTION
        {
            TOUPBASE_NORTH,
            TOUPBASE_SOUTH,
            TOUPBASE_EAST,
            TOUPBASE_WEST,
            TOUPBASE_STOP,
        };

        enum eTriggerMode
        {
            TRIGGER_VIDEO,
            TRIGGER_SOFTWARE
        };

        //#############################################################################
        // Capture
        //#############################################################################
        void allocateFrameBuffer();
        timeval m_ExposureEnd;
        double m_ExposureRequest;

        //#############################################################################
        // Video Format & Streaming
        //#############################################################################
        void getVideoImage();

        //#############################################################################
        // Guiding
        //#############################################################################
        // N/S Guiding
        static void TimerHelperNS(void *context);
        void TimerNS();
        void stopTimerNS();
        IPState guidePulseNS(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName);
        int m_NStimerID { -1 };

        // W/E Guiding
        static void TimerHelperWE(void *context);
        void TimerWE();
        void stopTimerWE();
        IPState guidePulseWE(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName);
        int m_WEtimerID { -1 };

        //#############################################################################
        // Setup & Controls
        //#############################################################################
        // Get initial parameters from camera
        void setupParams();
        // Create number and switch controls for camera by querying the API
        void createControls(int piNumberOfControls);
        // Update control values from camera
        void refreshControls();

        //#############################################################################
        // Resolution
        //#############################################################################
        ISwitch m_ResolutionS[CP(MAX)];
        ISwitchVectorProperty m_ResolutionSP;

        //#############################################################################
        // Misc.
        //#############################################################################
        // Get the current Bayer string used
        const char *getBayerString();

        bool updateBinningMode(int binx, int mode);

        //#############################################################################
        // Callbacks
        //#############################################################################
        static void eventCB(unsigned event, void* pCtx);
        void eventCallBack(unsigned event);

        // Handle capture timeout
        void captureTimeoutHandler();

        //#############################################################################
        // Camera Handle & Instance
        //#############################################################################
        THAND m_CameraHandle { nullptr };
        const XP(DeviceV2) *m_Instance;
        // Camera Display Name
        char m_name[MAXINDIDEVICE];

        //#############################################################################
        // Properties
        //#############################################################################
        ISwitchVectorProperty m_BinningModeSP;
        ISwitch m_BinningModeS[2];
        typedef enum
        {
            TC_BINNING_AVG,
            TC_BINNING_ADD,
        } BINNING_MODE;

        ISwitchVectorProperty m_HighFullwellSP;
        ISwitch m_HighFullwellS[2];

        ISwitchVectorProperty m_CoolerSP;
        ISwitch m_CoolerS[2];
		
        IText m_CoolerT;
        ITextVectorProperty m_CoolerTP;
        int32_t m_maxTecVoltage { -1 };

        INumberVectorProperty m_ControlNP;
        INumber m_ControlN[8];
        enum
        {
            TC_GAIN,
            TC_CONTRAST,
            TC_HUE,
            TC_SATURATION,
            TC_BRIGHTNESS,
            TC_GAMMA,
            TC_SPEED,
            TC_FRAMERATE_LIMIT,
        };

        // Auto Black Balance
        ISwitch m_BBAutoS;
        ISwitchVectorProperty m_BBAutoSP;

        ISwitch m_AutoExposureS[2];
        ISwitchVectorProperty m_AutoExposureSP;

        INumber m_BlackBalanceN[3];
        INumberVectorProperty m_BlackBalanceNP;
        enum
        {
            TC_BLACK_R,
            TC_BLACK_G,
            TC_BLACK_B,
        };

        INumberVectorProperty m_OffsetNP;
        INumber m_OffsetN;

        // R/G/B/Gray low/high levels
        INumber m_LevelRangeN[8];
        INumberVectorProperty m_LevelRangeNP;
        enum
        {
            TC_LO_R,
            TC_HI_R,
            TC_LO_G,
            TC_HI_G,
            TC_LO_B,
            TC_HI_B,
            TC_LO_Y,
            TC_HI_Y,
        };

        // White Balance
        INumber m_WBN[3];
        INumberVectorProperty m_WBNP;
        enum
        {
            TC_WB_R,
            TC_WB_G,
            TC_WB_B,
        };

        // Auto White Balance
        ISwitch m_WBAutoS;
        ISwitchVectorProperty m_WBAutoSP;

        // Fan Speed
        ISwitch *m_FanSpeedS { nullptr };
        ISwitchVectorProperty m_FanSpeedSP;

        // Video Format
        ISwitch m_VideoFormatS[2];
        ISwitchVectorProperty m_VideoFormatSP;
        enum
        {
            TC_VIDEO_COLOR_RGB,
            TC_VIDEO_COLOR_RAW,
        };
        enum
        {

            TC_VIDEO_MONO_8,
            TC_VIDEO_MONO_16,
        };

        // Low Noise
        ISwitchVectorProperty m_LowNoiseSP;
        ISwitch m_LowNoiseS[2];

        // Heat Up
        ISwitchVectorProperty m_HeatUpSP;
        ISwitch *m_HeatUpS { nullptr };

        // Firmware Info
        ITextVectorProperty m_FirmwareTP;
        IText m_FirmwareT[5];
        enum
        {
            TC_FIRMWARE_SN,
            TC_FIRMWARE_FW_VERSION,
            TC_FIRMWARE_HW_VERSION,
            TC_FIRMWARE_DATE,
            TC_FIRMWARE_REV
        };

        // SDK Version
        ITextVectorProperty m_SDKVersionTP;
        IText m_SDKVersionT;

        // Timeout factor
        INumberVectorProperty m_TimeoutFactorNP;
        INumber m_TimeoutFactorN;

        ISwitchVectorProperty m_GainConversionSP;
        ISwitch m_GainConversionS[3];
        enum
        {
            GAIN_LOW,
            GAIN_HIGH,
            GAIN_HDR
        };

        BINNING_MODE m_BinningMode = TC_BINNING_ADD;
        uint8_t m_CurrentVideoFormat = TC_VIDEO_COLOR_RGB;
        INDI_PIXEL_FORMAT m_CameraPixelFormat = INDI_RGB;
        eTriggerMode m_CurrentTriggerMode = TRIGGER_SOFTWARE;

        bool m_MonoCamera { false };
        INDI::Timer m_CaptureTimeout;
        uint32_t m_CaptureTimeoutCounter {0};
        // Download estimation in ms after exposure duration finished.
        double m_DownloadEstimation {5000};

        uint8_t m_BitsPerPixel { 8 };
        uint8_t m_RawBitsPerPixel { 8 };
        uint8_t m_maxBitDepth { 8 };
        uint8_t m_Channels { 1 };
        
        uint8_t* getRgbBuffer();
        uint8_t *m_rgbBuffer { nullptr };
        int32_t m_rgbBufferSize { 0 };

        int m_ConfigResolutionIndex {-1};

        static const uint32_t MIN_DOWNLOAD_ESTIMATION { 1000 };
};
