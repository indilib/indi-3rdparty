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
#define DNAME "Toupcam"
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
#define DNAME "Bressercam"
#elif BUILD_MALLINCAM
#include <mallincam.h>
#define FP(x) Mallincam_##x
#define CP(x) MALLINCAM_##x
#define XP(x) Mallincam##x
#define THAND HMallincam
#define DNAME "Mallincam"
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
#define DNAME "Ogmacam"
#elif BUILD_OMEGONPROCAM
#include <omegonprocam.h>
#define FP(x) Omegonprocam_##x
#define CP(x) OMEGONPROCAM_##x
#define XP(x) Omegonprocam##x
#define THAND HOmegonprocam
#define DNAME "OmegonProCam"
#elif BUILD_STARSHOOTG
#include <starshootg.h>
#define FP(x) Starshootg_##x
#define CP(x) STARSHOOTG_##x
#define XP(x) Starshootg##x
#define THAND HStarshootg
#define DNAME "StarshootG"
#elif BUILD_TSCAM
#include <tscam.h>
#define FP(x) Tscam_##x
#define CP(x) TSCAM_##x
#define XP(x) Tscam##x
#define THAND HTscam
#define DNAME "Tscam"
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

        virtual int SetTemperature(double temperature) override;
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
        static std::map<int, std::string> errCodes;
		static std::string errorCodes(int rc);

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
            TRIGGER_SOFTWARE,
            TRIGGER_EXTERNAL,
        };

        //#############################################################################
        // Capture
        //#############################################################################
        void allocateFrameBuffer();
        timeval ExposureEnd;
        double ExposureRequest;

        //#############################################################################
        // Video Format & Streaming
        //#############################################################################
        void getVideoImage();
        bool setVideoFormat(uint8_t index);

        //#############################################################################
        // Guiding
        //#############################################################################
        // N/S Guiding
        static void TimerHelperNS(void *context);
        void TimerNS();
        void stopTimerNS();
        IPState guidePulseNS(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName);
        timeval NSPulseEnd;
        int NStimerID;
        eGUIDEDIRECTION NSDir;
        const char *NSDirName;

        // W/E Guiding
        static void TimerHelperWE(void *context);
        void TimerWE();
        void stopTimerWE();
        IPState guidePulseWE(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName);
        timeval WEPulseEnd;
        int WEtimerID;
        eGUIDEDIRECTION WEDir;
        const char *WEDirName;

        //#############################################################################
        // Temperature Control & Cooling
        //#############################################################################
        bool activateCooler(bool enable);
        double TemperatureRequest;

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
        // Dual conversion Gain
        //#############################################################################
        bool dualGainEnabled();
        double setDualGainMode(double gain);
        void setDualGainRange();

        //#############################################################################
        // Resolution
        //#############################################################################
        ISwitch ResolutionS[CP(MAX)];
        ISwitchVectorProperty ResolutionSP;

        //#############################################################################
        // Misc.
        //#############################################################################
        // Get the current Bayer string used
        const char *getBayerString();

        bool updateBinningMode(int binx, int mode);

        //#############################################################################
        // Callbacks
        //#############################################################################
        static void pushCB(const void* pData, const XP(FrameInfoV2)* pInfo, int bSnap, void* pCallbackCtx);
        void pushCallback(const void* pData, const XP(FrameInfoV2)* pInfo, int bSnap);

        static void eventCB(unsigned event, void* pCtx);
        void eventPullCallBack(unsigned event);

        static void TempTintCB(const int nTemp, const int nTint, void* pCtx);
        void TempTintChanged(const int nTemp, const int nTint);

        static void WhiteBalanceCB(const int aGain[3], void* pCtx);
        void WhiteBalanceChanged(const int aGain[3]);

        static void BlackBalanceCB(const unsigned short aSub[3], void* pCtx);
        void BlackBalanceChanged(const unsigned short aSub[3]);

        static void AutoExposureCB(void* pCtx);
        void AutoExposureChanged();

        // Handle capture timeout
        void captureTimeoutHandler();

        //#############################################################################
        // Camera Handle & Instance
        //#############################################################################
        THAND m_CameraHandle { nullptr };
        const XP(DeviceV2) *m_Instance;
        // Camera Display Name
        char name[MAXINDIDEVICE];

        //#############################################################################
        // Properties
        //#############################################################################
        ISwitchVectorProperty BinningModeSP;
        ISwitch BinningModeS[2];
        typedef enum
        {
            TC_BINNING_AVG,
            TC_BINNING_ADD,
        } BINNING_MODE;

        ISwitchVectorProperty HighFullwellModeSP;
        ISwitch HighFullwellModeS[2];
        typedef enum
        {
            TC_HIGHFULLWELL_ON,
            TC_HIGHFULLWELL_OFF,
        } HIGHFULLWELL_MODE;

        ISwitchVectorProperty CoolerSP;
        ISwitch CoolerS[2];
        enum
        {
            TC_COOLER_ON,
            TC_COOLER_OFF,
        };

        INumberVectorProperty ControlNP;
        INumber ControlN[8];
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

        ISwitch AutoControlS[3];
        ISwitchVectorProperty AutoControlSP;
        enum
        {
            TC_AUTO_TINT,
            TC_AUTO_WB,
            TC_AUTO_BB,
        };

        ISwitch AutoExposureS[2];
        ISwitchVectorProperty AutoExposureSP;
        enum
        {
            TC_AUTO_EXPOSURE_ON,
            TC_AUTO_EXPOSURE_OFF,
        };

        INumber BlackBalanceN[3];
        INumberVectorProperty BlackBalanceNP;
        enum
        {
            TC_BLACK_R,
            TC_BLACK_G,
            TC_BLACK_B,
        };

        INumberVectorProperty OffsetNP;
        INumber OffsetN[1];
        enum
        {
            TC_OFFSET,
        };

        // R/G/B/Gray low/high levels
        INumber LevelRangeN[8];
        INumberVectorProperty LevelRangeNP;
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

        // Temp/Tint White Balance
        INumber WBTempTintN[2];
        INumberVectorProperty WBTempTintNP;
        enum
        {
            TC_WB_TEMP,
            TC_WB_TINT,
        };

        // RGB White Balance
        INumber WBRGBN[3];
        INumberVectorProperty WBRGBNP;
        enum
        {
            TC_WB_R,
            TC_WB_G,
            TC_WB_B,
        };

        // Auto Balance Mode
        ISwitch WBAutoS[2];
        ISwitchVectorProperty WBAutoSP;
        enum
        {
            TC_AUTO_WB_TT,
            TC_AUTO_WB_RGB
        };

        // Fan control
        ISwitch FanControlS[2];
        ISwitchVectorProperty FanControlSP;
        enum
        {
            TC_FAN_ON,
            TC_FAN_OFF,
        };

        // Fan Speed
        ISwitch *FanSpeedS = nullptr;
        ISwitchVectorProperty FanSpeedSP;

        // Video Format
        ISwitch VideoFormatS[2];
        ISwitchVectorProperty VideoFormatSP;
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
        ISwitchVectorProperty LowNoiseSP;
        ISwitch LowNoiseS[2];

        // Heat Up
        ISwitchVectorProperty HeatUpSP;
        ISwitch HeatUpS[3];
        enum
        {
            TC_HEAT_OFF,
            TC_HEAT_ON,
            TC_HEAT_MAX,
        };

        // Firmware Info
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[5] = {};
        enum
        {
            TC_FIRMWARE_SERIAL,
            TC_FIRMWARE_SW_VERSION,
            TC_FIRMWARE_HW_VERSION,
            TC_FIRMWARE_DATE,
            TC_FIRMWARE_REV
        };

        // SDK Version
        ITextVectorProperty SDKVersionTP;
        IText SDKVersionT[1] = {};

        // ADC / Max Bitdepth
        INumberVectorProperty ADCNP;
        INumber ADCN[1];

        // Timeout factor
        INumberVectorProperty TimeoutFactorNP;
        INumber TimeoutFactorN[1];

        // Gain Conversion
        INumberVectorProperty GainConversionNP;
        INumber GainConversionN[2];
        enum
        {
            TC_HCG_THRESHOLD,
            TC_HCG_LCG_RATIO,
        };

        ISwitchVectorProperty GainConversionSP;
        ISwitch GainConversionS[3];
        enum
        {
            GAIN_LOW,
            GAIN_HIGH,
            GAIN_HDR
        };

        BINNING_MODE m_BinningMode = TC_BINNING_ADD;
        uint8_t m_CurrentVideoFormat = TC_VIDEO_COLOR_RGB;
        INDI_PIXEL_FORMAT m_CameraPixelFormat = INDI_RGB;
        eTriggerMode m_CurrentTriggerMode = TRIGGER_VIDEO;

        bool m_CanSnap { false };
        bool m_RAWHighDepthSupport { false };
        bool m_MonoCamera { false };
        bool m_hasDualGain { false };
        bool m_HasLowNoise { false };
        bool m_HasHighFullwellMode { false };
        bool m_HasHeatUp { false };

        INDI::Timer m_CaptureTimeout;
        uint32_t m_CaptureTimeoutCounter {0};
        // Download estimation in ms after exposure duration finished.
        double m_DownloadEstimation {5000};

        uint8_t m_BitsPerPixel { 8 };
        uint8_t m_RawBitsPerPixel { 8 };
        uint8_t m_MaxBitDepth { 8 };
        uint8_t m_Channels { 1 };

        uint32_t m_MaxGainNative { 0 };
        uint32_t m_MaxGainHCG { 0 };
        uint32_t m_NativeGain { 0 };

        int m_ConfigResolutionIndex {-1};

        static const uint32_t MIN_DOWNLOAD_ESTIMATION { 1000 };
};
