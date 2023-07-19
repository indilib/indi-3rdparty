/*
    PlayerOne CCD Driver

    Copyright (C) 2021 Hiroshi Saito (hiro3110g@gmail.com)

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

#include <PlayerOneCamera.h>

#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include "indipropertytext.h"
#include "indisinglethreadpool.h"

#include <vector>

#include <indiccd.h>
#include <inditimer.h>

class SingleWorker;
class POABase : public INDI::CCD
{
    public:
        POABase();
        ~POABase() override;

        virtual const char *getDefaultName() override;

        virtual void ISGetProperties(const char *dev) override;
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

        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        // PlayerOne specific keywords
        virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;

        // Save config
        virtual bool saveConfigItems(FILE *fp) override;

        virtual bool SetCaptureFormat(uint8_t index) override;

        /** Get the current Bayer string used */
        const char *getBayerString() const;

    protected:
        INDI::SingleThreadPool mWorker;
        void workerStreamVideo(const std::atomic_bool &isAbortToQuit);
        void workerBlinkExposure(const std::atomic_bool &isAbortToQuit, int blinks, float duration);
        void workerExposure(const std::atomic_bool &isAbortToQuit, float duration);

        /** Get image from CCD and send it to client */
        int grabImage(float duration);

    protected:
        double mTargetTemperature;
        double mCurrentTemperature;
        INDI::Timer mTimerTemperature;
        void temperatureTimerTimeout();

        /** Timers for NS/WE guiding */
        INDI::Timer mTimerNS;
        INDI::Timer mTimerWE;

        IPState guidePulse(INDI::Timer &timer, float ms, POAConfig dir);
        void stopGuidePulse(INDI::Timer &timer);

        /** Get initial parameters from camera */
        void setupParams();

        /** Create number and switch controls for camera by querying the API */
        void createControls(int piNumberOfControls);

        /** Update control values from camera */
        void updateControls();

        /** Return user selected image type */
        POAImgFormat getImageType() const;

        /** Update SER recorder video format */
        void updateRecorderFormat();

        /** Control cooler */
        bool activateCooler(bool enable);

        /** Set Video Format */
        bool setVideoFormat(uint8_t index);

        /** Get if MonoBin is active, thus Bayer is irrelevant */
        bool isMonoBinActive();

        /** Can the camera flip the image horizontally and vertically */
        bool hasFlipControl();

        /** Compatibilities for ZWO cameras */
        POAErrors POASetROIFormat(int CameraID, int width, int height, int bin, POAImgFormat imgType);
        POAErrors POAGetROIFormat(int CameraID, int *width, int *height, int *bin, POAImgFormat *imgType);
        POAErrors POAPulseGuideOn(int cameraID, POAConfig dir);
        POAErrors POAPulseGuideOff(int cameraID, POAConfig dir);
        /** Compensete bayer pattern by flip */
        POAErrors POABayerCompensationByFlip(POAConfig flip, char *dest);

        /** Additional Properties to INDI::CCD */
        INDI::PropertyNumber  CoolerNP {1};
        INDI::PropertySwitch  CoolerSP {2};

        INDI::PropertyNumber  ControlNP {0};
        INDI::PropertySwitch  ControlSP {0};
        INDI::PropertySwitch  VideoFormatSP {0};

        INDI::PropertyNumber  ADCDepthNP {1};
        INDI::PropertyText    SDKVersionSP {1};
        INDI::PropertyText    SerialNumberTP {1};
        INDI::PropertyText    NicknameTP {1};

        INDI::PropertySwitch  SensorModeSP {0};

        INDI::PropertyNumber  BlinkNP {2};
        enum
        {
            BLINK_COUNT,
            BLINK_DURATION
        };

        INDI::PropertySwitch  FlipSP {2};
        enum
        {
            FLIP_HORIZONTAL,
            FLIP_VERTICAL
        };

        std::string mCameraName, mCameraID, mSerialNumber, mNickname;
        POACameraProperties mCameraInfo;
        uint8_t mExposureRetry {0};
        POAImgFormat                      mCurrentVideoFormat;
        std::vector<POAConfigAttributes>  mControlCaps;
};
