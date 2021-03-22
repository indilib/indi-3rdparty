/*
    ASI CCD Driver

    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)

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

#include <ASICamera2.h>

#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include "indipropertytext.h"
#include "indisinglethreadpool.h"

#include <vector>

#include <indiccd.h>
#include <inditimer.h>

class SingleWorker;
class ASICCD : public INDI::CCD
{
public:
    explicit ASICCD(const ASI_CAMERA_INFO &camInfo, const std::string &cameraName);
    ~ASICCD() override;

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

    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int binx, int biny) override;

    // Guide Port
    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;

    // ASI specific keywords
    virtual void addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip) override;

    // Save config
    virtual bool saveConfigItems(FILE *fp) override;

private:
    /** Get the current Bayer string used */
    const char *getBayerString() const;

private:
    INDI::SingleThreadPool mWorker;
    void workerStreamVideo(const std::atomic_bool &isAboutToQuit);
    void workerBlinkExposure(const std::atomic_bool &isAboutToQuit, int blinks, float duration);
    void workerExposure(const std::atomic_bool &isAboutToQuit, float duration);

    /** Get image from CCD and send it to client */
    int grabImage(float duration);

private:
    double mTargetTemperature;
    double mCurrentTemperature;
    INDI::Timer mTimerTemperature;
    void temperatureTimerTimeout();

    /** Timers for NS/WE guiding */
    INDI::Timer mTimerNS;
    INDI::Timer mTimerWE;

    IPState guidePulse(INDI::Timer &timer, float ms, ASI_GUIDE_DIRECTION dir);
    void stopGuidePulse(INDI::Timer &timer);

    /** Get initial parameters from camera */
    void setupParams();

    /** Create number and switch controls for camera by querying the API */
    void createControls(int piNumberOfControls);

    /** Update control values from camera */
    void updateControls();

    /** Return user selected image type */
    ASI_IMG_TYPE getImageType() const;

    /** Update SER recorder video format */
    void updateRecorderFormat();

    /** Control cooler */
    bool activateCooler(bool enable);

    /** Set Video Format */
    bool setVideoFormat(uint8_t index);

    /** Get if MonoBin is active, thus Bayer is irrelevant */
    bool isMonoBinActive();

private:
    /** Additional Properties to INDI::CCD */
    INDI::PropertyNumber  CoolerNP {1};
    INDI::PropertySwitch  CoolerSP {2};

    INDI::PropertyNumber  ControlNP {0};
    INDI::PropertySwitch  ControlSP {0};
    INDI::PropertySwitch  VideoFormatSP {0};

    INDI::PropertyNumber  ADCDepthNP   {1};
    INDI::PropertyText    SDKVersionSP {1};

    INDI::PropertyNumber  BlinkNP {2};
    enum {
        BLINK_COUNT,
        BLINK_DURATION
    };

private:
    std::string mCameraName;
    uint8_t mExposureRetry {0};

    ASI_IMG_TYPE                  mCurrentVideoFormat;
    std::vector<ASI_CONTROL_CAPS> mControlCaps;
    ASI_CAMERA_INFO               mCameraInfo;
};
