/*
 Toupcam & oem CCD Driver

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

#include <indiccd.h>
#include <inditimer.h>
#include "libtoupbase.h"

class ToupBase : public INDI::CCD
{
    public:
        explicit ToupBase(const XP(DeviceV2) *instance, const std::string &name);
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
        IPState guidePulse(INDI::Timer &timer, float ms, eGUIDEDIRECTION dir);
        const char *toString(eGUIDEDIRECTION dir);
        void stopGuidePulse(INDI::Timer &timer);
        // Timers
        INDI::Timer mTimerNS;
        INDI::Timer mTimerWE;

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
        // Misc
        //#############################################################################
        // Get the current Bayer string used
        const char *getBayerString();

        bool updateBinningMode(int binx, int mode);

        //#############################################################################
        // Callbacks
        //#############################################################################
        static void eventCB(unsigned event, void* pCtx);
        void eventCallBack(unsigned event);

        //#############################################################################
        // Camera Handle & Instance
        //#############################################################################
        THAND m_Handle { nullptr };
        const XP(DeviceV2) *m_Instance;

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

        bool activateCooler(bool enable);

        ISwitchVectorProperty m_CoolerSP;
        ISwitch m_CoolerS[2];

        INDI::PropertyNumber m_CoolerNP {1};

        int32_t m_maxTecVoltage { -1 };

        INumberVectorProperty m_ControlNP;
        INumber m_ControlN[8];
        enum
        {
            TC_GAIN,
            TC_CONTRAST,
            TC_BRIGHTNESS,
            TC_GAMMA,
            TC_SPEED,
            TC_FRAMERATE_LIMIT,
            TC_HUE,
            TC_SATURATION
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

        // Offset (Black Level)
        INumberVectorProperty m_OffsetNP;
        INumber m_OffsetN [1];

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

        // Fan
        ISwitch *m_FanS { nullptr };
        ISwitchVectorProperty m_FanSP;

        // Low Noise
        ISwitchVectorProperty m_LowNoiseSP;
        ISwitch m_LowNoiseS[2];

        // Heat
        ISwitchVectorProperty m_HeatSP;
        ISwitch *m_HeatS { nullptr };

        // Tail Light
        ISwitchVectorProperty m_TailLightSP;
        ISwitch m_TailLightS[2];

        // Camera Info
        ITextVectorProperty m_CameraTP;
        IText m_CameraT[6];
        enum
        {
            TC_CAMERA_MODEL,
            TC_CAMERA_DATE,
            TC_CAMERA_SN,
            TC_CAMERA_FW_VERSION,
            TC_CAMERA_HW_VERSION,
            TC_CAMERA_REV
        };

        // SDK Version
        ITextVectorProperty m_SDKVersionTP;
        IText m_SDKVersionT;

        INDI::PropertyNumber  m_ADCDepthNP{1};

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
        uint8_t m_CurrentVideoFormat = 0;
        INDI_PIXEL_FORMAT m_CameraPixelFormat = INDI_RGB;
        eTriggerMode m_CurrentTriggerMode =
            TRIGGER_SOFTWARE; /* By default, we start the camera with software trigger mode, make it standby */

        bool m_MonoCamera { false };
        bool m_SupportTailLight { false };
        double m_LastTemperature {-100};
        double m_LastCoolerPower {-1};
        uint8_t m_BitsPerPixel { 8 };
        uint8_t m_maxBitDepth { 8 };
        uint8_t m_Channels { 1 };

        uint8_t *getRgbBuffer();
        uint8_t *m_rgbBuffer { nullptr };
        int32_t m_rgbBufferSize { 0 };

        int m_ConfigResolutionIndex {-1};
};
