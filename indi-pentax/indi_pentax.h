/*
 Pentax CCD
 CCD Template for INDI Developers
 Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

 Multiple device support Copyright (C) 2013 Peter Polakovic (peter.polakovic@cloudmakers.eu)

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

#ifndef PENTAX_CCD_H
#define PENTAX_CCD_H

#include <ricoh_camera_sdk.hpp>

#include <indiccd.h>
#include <indiccdchip.h>

#include <iostream>

using namespace std;
using namespace Ricoh::CameraController;

class PentaxCCD;

class PentaxEventListener : public CameraEventListener
{
public:
    PentaxEventListener(PentaxCCD *device); //const char *devname, INDI::CCDChip *ccd, INDI::StreamManager *streamer);
    PentaxCCD *device;

    //INDI::CCDChip *ccd;
    //const char *deviceName;
    const char *getDeviceName(); //so we can use the logger

    void imageStored(const std::shared_ptr<const CameraDevice>& sender, const std::shared_ptr<const CameraImage>& image) override;

    //INDI::StreamManager *streamer;
    virtual void liveViewFrameUpdated(const std::shared_ptr<const CameraDevice>& sender, const std::shared_ptr<const unsigned char>& liveViewFrame, uint64_t frameSize) override;
};

class PentaxCCD : public INDI::CCD
{
  public:
    PentaxCCD(std::shared_ptr<CameraDevice> camera);
    virtual ~PentaxCCD();

    const char *getDefaultName();

    bool initProperties();
    void ISGetProperties(const char *dev);
    bool updateProperties();

    bool Connect();
    bool Disconnect();

    bool StartExposure(float duration);
    bool AbortExposure();

  protected:
    friend class PentaxEventListener;
    void TimerHit();
    virtual bool UpdateCCDFrame(int x, int y, int w, int h);
    virtual bool UpdateCCDBin(int binx, int biny);
    virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType);

    // Guide Port
    virtual IPState GuideNorth(uint32_t ms);
    virtual IPState GuideSouth(uint32_t ms);
    virtual IPState GuideEast(uint32_t ms);
    virtual IPState GuideWest(uint32_t ms);

    bool StartStreaming() override;
    bool StopStreaming() override;
    void streamLiveView();

    //std::mutex liveStreamMutex;
    //bool m_RunLiveStream;

    bool bufferIsBayered;
  private:
    std::shared_ptr<CameraDevice> device;
    std::shared_ptr<const Capture> pendingCapture;
    std::shared_ptr<PentaxEventListener> listener;

    char name[32];

    double ccdTemp;
    double minDuration;
    unsigned short *imageBuffer;

    int timerID;

    INDI::CCDChip::CCD_FRAME imageFrameType;

    struct timeval ExpStart;

    float ExposureRequest;
    float TemperatureRequest;

    float CalcTimeLeft();
    int grabImage();
    bool setupParams();
    bool sim;

    ISwitch * mIsoS = nullptr;
    ISwitchVectorProperty mIsoSP;
    ISwitch * mFormatS = nullptr;
    ISwitchVectorProperty mFormatSP;

    ISwitch transferFormatS[2];
    ISwitchVectorProperty transferFormatSP;
    ISwitch SDCardImageS[2];
    ISwitchVectorProperty SDCardImageSP;
    enum
    {
        SD_CARD_SAVE_IMAGE,
        SD_CARD_DELETE_IMAGE
    };

    ISwitch autoFocusS[1];
    ISwitchVectorProperty autoFocusSP;

    ISwitch livePreviewS[2];
    ISwitchVectorProperty livePreviewSP;

    ISwitch * mExposurePresetS = nullptr;
    ISwitchVectorProperty mExposurePresetSP;
    friend void ::ISGetProperties(const char *dev);
    friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
    friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
    friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
    friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                            char *formats[], char *names[], int n);
};

#endif // PENTAX_CCD_H
