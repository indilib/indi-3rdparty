/*
    Driver type: GPhoto Camera INDI Driver

    Copyright (C) 2009 Geoffrey Hausheer
    Copyright (C) 2013-2024 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

*/

#pragma once

#include "gphoto_driver.h"

#include <indiccd.h>
#include <indifocuserinterface.h>

#include <map>
#include <future>
#include <string>

#define MAXEXPERR 10 /* max err in exp time we allow, secs */
#define OPENDT    5  /* open retry delay, secs */

typedef struct _Camera Camera;

enum
{
    ON_S,
    OFF_S
};

typedef struct
{
    gphoto_widget * widget;
    union
    {
        INumber num;
        ISwitch * sw;
        IText text;
    } item;
    union
    {
        INumberVectorProperty num;
        ISwitchVectorProperty sw;
        ITextVectorProperty text;
    } prop;
} cam_opt;

class GPhotoCCD : public INDI::CCD, public INDI::FocuserInterface
{
    public:
        explicit GPhotoCCD();
        explicit GPhotoCCD(const char * model, const char * port);
        virtual ~GPhotoCCD() override;

        const char * getDefaultName() override;

        bool initProperties() override;
        void ISGetProperties(const char * dev) override;
        bool updateProperties() override;

        bool Connect() override;
        bool Disconnect() override;

        bool StartExposure(float duration) override;
        bool AbortExposure() override;
        bool UpdateCCDFrame(int x, int y, int w, int h) override;

        // enable binning
        bool UpdateCCDBin(int hor, int ver) override;

        virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;

        static void ExposureUpdate(void * vp);
        void ExposureUpdate();

        static void UpdateExtendedOptions(void * vp);
        void UpdateExtendedOptions(bool force = false);

        static void UpdateFocusMotionHelper(void *context);
        void UpdateFocusMotionCallback();

    protected:
        // Misc.
        bool saveConfigItems(FILE * fp) override;
        void addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;
        void TimerHit() override;

        // Capture format
        bool SetCaptureFormat(uint8_t index) override;

        // Simulation Triggered
        void simulationTriggered(bool enabled) override;

        // Upload Mode
        bool UpdateCCDUploadMode(CCD_UPLOAD_MODE mode) override;

        // Focusing
#if 0
        bool SetFocuserSpeed(int speed) override;
        IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
#endif
        /**
         * \brief MoveFocuser the focuser to an relative position.
         * \param dir Direction of focuser, either FOCUS_INWARD or FOCUS_OUTWARD.
         * \param ticks The relative ticks to move.
         * \return Return IPS_OK if motion is completed and focuser reached requested position. Return
         * IPS_BUSY if focuser started motion to requested position and is in progress.
         * Return IPS_ALERT if there is an error.
         */
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;

        // Streaming
        bool StartStreaming() override;
        bool StopStreaming() override;
        void streamLiveView();

        std::mutex liveStreamMutex;
        bool m_RunLiveStream;

    private:
        void createSwitch(INDI::PropertySwitch &property, const char *baseName, char ** options, int max_opts, int setidx);
        ISwitch * createLegacySwitch(const char * basestr, char ** options, int max_opts, int setidx);
        void AddWidget(gphoto_widget * widget);
        void UpdateWidget(cam_opt * opt);
        void ShowExtendedOptions(void);
        void HideExtendedOptions(void);

        double CalcTimeLeft();
        bool grabImage();

        char name[MAXINDIDEVICE];
        char model[MAXINDINAME];
        char port[MAXINDINAME];

        struct timeval ExpStart;
        double ExposureRequest;

        gphoto_driver * gphotodrv;
        std::map<std::string, cam_opt *> CamOptions;
        int expTID; /* exposure callback timer id, if any */
        int optTID; /* callback for exposure timer id */
        int focusSpeed {0};

        char * on_off[2];
        int timerID;
        bool frameInitialized;
        bool isTemperatureSupported { false };
        int m_CaptureTarget {-1};

        // Focus
        bool m_CanFocus { false };
        int32_t m_TargetLargeStep {0}, m_TargetMedStep {0}, m_TargetLowStep {0}, m_FocusTimerID {-1};

        int liveVideoWidth  {-1};
        int liveVideoHeight {-1};

        // binning ?
        bool binning { false };

        // Shutter Port
        INDI::PropertyText PortTP {1};
        // Mirror Lock Toggle
        INDI::PropertyNumber MirrorLockNP {1};
        // ISO List
        INDI::PropertySwitch ISOSP {0};
        // Capture Target selection
        INDI::PropertySwitch CaptureTargetSP {2};
        enum
        {
            CAPTURE_INTERNAL_RAM,
            CAPTURE_SD_CARD
        };
        // What happens to SD card image?
        INDI::PropertySwitch SDCardImageSP {3};
        enum
        {
            SD_CARD_SAVE_IMAGE,
            SD_CARD_DELETE_IMAGE,
            SD_CARD_IGNORE_IMAGE,

        };
        // Autofocus Set
        INDI::PropertySwitch AutoFocusSP {1};
        // Live Preview Toggle
        //INDI::PropertySwitch LivePreviewSP {2};
        // Exposure Presets
        INDI::PropertySwitch ExposurePresetSP {0};
        // Force BULB mode (vs predefined exposure indexes) when capturing
        INDI::PropertySwitch ForceBULBSP {2};
        // Wait this many seconds before giving up on exposure download
        INDI::PropertyNumber DownloadTimeoutNP {1};
        // Upload file, used for testing purposes under simulation under native mode
        INDI::PropertyText UploadFileTP {1};
        INDI::PropertyBlob imageBP {INDI::Property()};

        Camera * camera = nullptr;

        // Threading
        std::thread m_LiveViewThread;

        std::map <uint8_t, uint8_t> m_CaptureFormatMap;

        static constexpr double MINUMUM_CAMERA_TEMPERATURE = -60.0;

        // Ratio from far 3 to far 2
        static constexpr double FOCUS_HIGH_MED_RATIO = 7.33;
        // Ratio from far 2 to far 1
        static constexpr double FOCUS_MED_LOW_RATIO = 6.36;
        // Do not accept switches more than this
        static constexpr uint8_t MAX_SWITCHES = 200;

        friend void ::ISSnoopDevice(XMLEle * root);
        friend void ::ISGetProperties(const char * dev);
        friend void ::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int num);
        friend void ::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int num);
        friend void ::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int num);
        friend void ::ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[],
                                char * formats[], char * names[], int n);
};
