/*
 Pentax CCD Driver for Indi (using PkTriggerCord)
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

#ifndef PKTRIGGERCORD_CCD_H
#define PKTRIGGERCORD_CCD_H

#include <indiccd.h>
#include <stream/streammanager.h>
#include <unistd.h>
#include <regex>
#include <future>

#include "config.h"
#include "eventloop.h"

#include "gphoto_readimage.h"

extern "C" {
#include "libpktriggercord.h"
}

using namespace std;

class PkTriggerCordCCD : public INDI::CCD
{
  public:
    PkTriggerCordCCD(const char *name);
    virtual ~PkTriggerCordCCD();

    const char *getDefaultName();

    bool initProperties();
    void ISGetProperties(const char *dev);
    bool updateProperties();

    bool Connect();
    bool Disconnect();

    bool StartExposure(float duration);
    bool AbortExposure();


protected:

    void TimerHit();
    bool SetCaptureFormat(uint8_t index) override;
    virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType);   

  private:

    char name[32];
    pslr_handle_t device;
    pslr_status status;
    user_file_format uff;
    int quality;
    bool InDownload, need_bulb_new_cleanup;
    bool bufferIsBayered;

    int timerID;

    INDI::CCDChip::CCD_FRAME imageFrameType;

    struct timeval ExpStart;

    float ExposureRequest;

    float CalcTimeLeft();
    bool setupParams();

    bool getCaptureSettingsState();

    ISwitchVectorProperty mIsoSP,mApertureSP,mExpCompSP,mWhiteBalanceSP,mIQualitySP;

    ISwitch transferFormatS[2];
    ISwitchVectorProperty transferFormatSP;

    ISwitch preserveOriginalS[2];
    ISwitchVectorProperty preserveOriginalSP;

    ISwitch autoFocusS[2];
    ISwitchVectorProperty autoFocusSP;

    ISwitch * create_switch(const char * basestr, string options[], size_t numOptions, int setidx);

    IText DeviceInfoT[6] {};
    ITextVectorProperty DeviceInfoTP;

    bool saveConfigItems(FILE * fp);

    bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n);
    void addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords);

    friend void ::ISGetProperties(const char *dev);
    friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
    friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
    friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
    friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                            char *formats[], char *names[], int n);

    void updateCaptureSettingSwitch(ISwitchVectorProperty *sw, ISState *states, char *names[], int n);
    bool grabImage();
    string getUploadFilePrefix();
    const char * getFormatFileExtension(user_file_format format);
    void refreshBatteryStatus();
    void buildCaptureSwitches();
    void deleteCaptureSwitches();
    void buildCaptureSettingSwitch(ISwitchVectorProperty *control, string optionList[], size_t numOptions, const char *label, const char *name, string currentsetting = "");

    bool shutterPress(pslr_rational_t shutter_speed);
    std::future<bool> shutter_result;
};

#endif // PKTRIGGERCORD_CCD_H
