/*
    ASI CCD Driver

    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)

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

#include <vector>

#include <condition_variable>
#include <mutex>
#include <indiccd.h>
#include <inditimer.h>
class ASICCD : public INDI::CCD
{
public:
    explicit ASICCD(const ASI_CAMERA_INFO *camInfo, const std::string &cameraName);
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

    /* Imaging functions */
    static void *imagingHelper(void *context);
    void *imagingThreadEntry();
    void streamVideo();
    void getExposure();
    void exposureSetRequest(ImageState request);

    /**
     * @brief setThreadRequest Set the thread request
     * @param request Desired thread state
     * @warning condMutex must be UNLOCKED before calling this or the function will deadlock.
     */
    void setThreadRequest(ImageState request);

    /**
     * @brief waitUntil Block thread until CURRENT image state matches requested state
     * @param request state to match
     */
    void waitUntil(ImageState request);

    /* Timer for temperature */
    INDI::Timer timerTemperature;
    void temperatureTimerTimeout();

    /* Timer for NS guiding */
    INDI::Timer timerNS;
    IPState guidePulseNS(float ms, ASI_GUIDE_DIRECTION dir);
    void stopTimerNS();

    /* Timer for WE guiding */
    INDI::Timer timerWE;
    IPState guidePulseWE(float ms, ASI_GUIDE_DIRECTION dir);
    void stopTimerWE();

    /** Get image from CCD and send it to client */
    int grabImage();
    /** Get initial parameters from camera */
    void setupParams();
    /** Calculate time left in seconds after start_time */
    float calcTimeLeft(float duration, timeval *start_time);
    /** Create number and switch controls for camera by querying the API */
    void createControls(int piNumberOfControls);
    /** Get the current Bayer string used */
    const char *getBayerString();
    /** Update control values from camera */
    void updateControls();
    /** Return user selected image type */
    ASI_IMG_TYPE getImageType();
    /** Update SER recorder video format */
    void updateRecorderFormat();
    /** Control cooler */
    bool activateCooler(bool enable);
    /** Set Video Format */
    bool setVideoFormat(uint8_t index);
    /** Get if MonoBin is active, thus Bayer is irrelevant */
    bool isMonoBinActive();

    std::string cameraName;

    /** Additional Properties to INDI::CCD */
    INumber CoolerN[1];
    INumberVectorProperty CoolerNP;

    ISwitch CoolerS[2];
    ISwitchVectorProperty CoolerSP;

    std::vector<INumber>  ControlN;
    INumberVectorProperty ControlNP;

    std::vector<ISwitch>  ControlS;
    ISwitchVectorProperty ControlSP;

    std::vector<ISwitch>  VideoFormatS;
    ISwitchVectorProperty VideoFormatSP;
    uint8_t rememberVideoFormat = { 0 };
    ASI_IMG_TYPE currentVideoFormat;

    enum {
            BLINK_COUNT,
            BLINK_DURATION
    };

    INumber BlinkN[2];
    INumberVectorProperty BlinkNP;

    INumber ADCDepthN;
    INumberVectorProperty ADCDepthNP;

    IText SDKVersionS[1] = {};
    ITextVectorProperty SDKVersionSP;

    struct timeval ExpStart;
    double ExposureRequest;
    double TemperatureRequest;
    uint8_t m_ExposureRetry {0};

    const ASI_CAMERA_INFO *m_camInfo;
    std::vector<ASI_CONTROL_CAPS> m_controlCaps;

    // Imaging thread
    ImageState threadRequest;
    ImageState threadState;

    //        pthread_t imagingThread;
    //        pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
    //        pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

    std::thread imagingThread;
    std::mutex condMutex;
    std::condition_variable cv;

    // Camera ROI
    uint32_t m_SubX = 0, m_SubY = 0, m_SubW = 0, m_SubH = 0;
};
