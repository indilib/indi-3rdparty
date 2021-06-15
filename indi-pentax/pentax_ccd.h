/*
 Pentax CCD Driver for Indi (using Ricoh Camera SDK)
 Copyright (C) 2020 Karl Rees

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

#include <indiccd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <ricoh_camera_sdk.hpp>
#pragma GCC diagnostic pop

#include <stream/streammanager.h>

#include "config.h"
#include "eventloop.h"
#include "pentax_event_handler.h"

using namespace std;
using namespace Ricoh::CameraController;

class PentaxEventHandler;

class PentaxCCD : public INDI::CCD
{
  public:
    PentaxCCD(std::shared_ptr<CameraDevice> camera);
    virtual ~PentaxCCD();

    const char *getDefaultName() override;

    bool initProperties() override;
    void ISGetProperties(const char *dev) override;
    bool updateProperties() override;

    bool Connect() override;
    bool Disconnect() override;

    bool StartExposure(float duration) override;
    bool AbortExposure() override;


protected:
    friend class PentaxEventHandler;
    void TimerHit() override;

    virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;

    bool StartStreaming() override;
    bool StopStreaming() override;

    bool bufferIsBayered;

    string getUploadFilePrefix();

  private:

    std::shared_ptr<CameraDevice> device;
    std::shared_ptr<const Capture> pendingCapture;
    std::shared_ptr<PentaxEventHandler> listener;

    char name[32];

    int timerID;

    bool InDownload;

    INDI::CCDChip::CCD_FRAME imageFrameType;

    struct timeval ExpStart;

    float ExposureRequest;

    float CalcTimeLeft();
    bool setupParams();

    ISO iso;
    ShutterSpeed shutter;
    FNumber aperture;
    ExposureCompensation exposurecomp;
    WhiteBalance whitebalance;
    StillImageQuality imagequality;
    StillImageCaptureFormat imageformat;
    ExposureProgram exposureprogram;
    StorageWriting storagewriting;
    UserCaptureSettingsMode usercapturesettingsmode;

    void getCaptureSettingsState();

    std::vector<const CaptureSetting *> updatedCaptureSettings;

    float updateShutterSpeed(float requestedSpeed);
    void updateCaptureSetting(CaptureSetting *setting, string newiso);

    ISwitchVectorProperty mIsoSP,mApertureSP,mExpCompSP,mWhiteBalanceSP,mIQualitySP,mFormatSP,mStorageWritingSP;

    ISwitch transferFormatS[2];
    ISwitchVectorProperty transferFormatSP;

    ISwitch preserveOriginalS[2];
    ISwitchVectorProperty preserveOriginalSP;

    ISwitch autoFocusS[2];
    ISwitchVectorProperty autoFocusSP;

    ISwitch * create_switch(const char * basestr, std::vector<string> options, int setidx);

    IText DeviceInfoT[7] {};
    ITextVectorProperty DeviceInfoTP;

    bool saveConfigItems(FILE * fp) override;

    bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;
    void addFITSKeywords(fitsfile * fptr, INDI::CCDChip * targetChip) override;

    void buildCaptureSettingSwitch(ISwitchVectorProperty *control, CaptureSetting *setting, const char *label = nullptr, const char *name = nullptr);
    void updateCaptureSettingSwitch(CaptureSetting *setting, ISwitchVectorProperty *sw, ISState *states, char *names[], int n);

    void deleteCaptureSwitches();
    void buildCaptureSwitches();
    void refreshBatteryStatus();

    friend void ::ISGetProperties(const char *dev);
    friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
    friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
    friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
    friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                            char *formats[], char *names[], int n);

};

#endif // PENTAX_CCD_H
