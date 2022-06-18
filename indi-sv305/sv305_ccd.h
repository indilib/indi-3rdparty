/*
 SV305 CCD
 SVBONY SV305 Camera driver
 Copyright (C) 2020 Blaise-Florentin Collin (thx8411@yahoo.fr)

 Generic CCD skeleton Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef SV305_CCD_H
#define SV305_CCD_H

#include <indiccd.h>
#include <iostream>

#include "libsv305/SVBCameraSDK.h"


using namespace std;


/////////////////////////////////////////////////
// SV305CCD CLASS
//

class Sv305CCD : public INDI::CCD
{
    public:
        Sv305CCD(int numCamera);
        virtual ~Sv305CCD();

        // INDI BASE
        const char *getDefaultName() override;

        void ISGetProperties(const char *dev) override;

        bool initProperties() override;
        bool updateProperties() override;

        bool Connect() override;
        bool Disconnect() override;

        int SetTemperature(double temperature) override;
        bool StartExposure(float duration) override;
        bool AbortExposure() override;

        // streaming
        virtual bool StartStreaming() override;
        virtual bool StopStreaming() override;
        static void* streamVideoHelper(void *context);
        void* streamVideo();

        // subframe
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;

        // binning
        virtual bool UpdateCCDBin(int hor, int ver) override;

        // handle UI settings
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

    protected:
        // INDI periodic grab query
        void TimerHit() override;

    private:
        // camera #
        int num;
        // camera name
        char name[32];
        // camera API return status
        SVB_ERROR_CODE status;
        // camera infos
        SVB_CAMERA_INFO cameraInfo;
        // camera API handler
        int cameraID;
        // camera property
        SVB_CAMERA_PROPERTY cameraProperty;
        // number of camera control
        int controlsNum;
        // exposure limits
        double minExposure;
        double maxExposure;
        // pixel size
        float pixelSize;

        // hCamera mutex protection
        pthread_mutex_t cameraID_mutex;

        // binning ?
        bool binning;
        // bit per pixel
        int bitDepth;
        // stretch factor x2, x4, x8, x16 (bit shift)
        int bitStretch;
        ISwitch StretchS[5];
        ISwitchVectorProperty StretchSP;
        enum { STRETCH_OFF, STRETCH_X2, STRETCH_X4, STRETCH_X8, STRETCH_X16 };

        // ROI offsets
        int x_offset;
	    int y_offset;

        // streaming ?
        bool streaming;
        // streaming mutex and thread control
        pthread_mutex_t streaming_mutex;
        pthread_t primary_thread;
        bool terminateThread;

        // for cooling control
        double TemperatureRequest;

        // controls settings
        enum
        {
            CCD_GAIN_N,
            CCD_CONTRAST_N,
            CCD_SHARPNESS_N,
            CCD_SATURATION_N,
            CCD_WBR_N,
            CCD_WBG_N,
            CCD_WBB_N,
            CCD_GAMMA_N,
            CCD_DOFFSET_N
        };
        INumber ControlsN[9];
        INumberVectorProperty ControlsNP[9];
        // control helper
        bool updateControl(int ControlType, SVB_CONTROL_TYPE SVB_Control, double values[], char *names[], int n);

        // frame speed
        ISwitch SpeedS[3];
        ISwitchVectorProperty SpeedSP;
        enum { SPEED_SLOW, SPEED_NORMAL, SPEED_FAST};
        int frameSpeed;

        // cooler enable
        ISwitch CoolerS[2];
        ISwitchVectorProperty CoolerSP;
        enum { COOLER_ENABLE = 0, COOLER_DISABLE = 1 };
        int coolerEnable; // 0:Enable, 1:Disable

        // output frame format
        // the camera is able to output RGB24, but not supported by INDI
        // -> ignored
	// NOTE : SV305M PRO d'ont support RAW8 and RAW16, only Y8 and Y16
        ISwitch FormatS[2];
        ISwitchVectorProperty FormatSP;
        enum { FORMAT_RAW16, FORMAT_RAW8, FORMAT_Y16, FORMAT_Y8};
        SVB_IMG_TYPE frameFormatMapping[4] = {SVB_IMG_RAW16, SVB_IMG_RAW8, SVB_IMG_Y16, SVB_IMG_Y8};
        int frameFormat;
        const char* bayerPatternMapping[4] = {"RGGB", "BGGR", "GRBG", "GBRG"};

        // exposure timing
        int timerID;
        struct timeval ExpStart;
        float ExposureRequest;
        float CalcTimeLeft();

        // update CCD Params
        bool updateCCDParams();

        // save settings
        virtual bool saveConfigItems(FILE *fp) override;

        // add FITS fields
        virtual void addFITSKeywords(INDI::CCDChip *targetChip) override;

        // INDI Callbacks
        friend void ::ISGetProperties(const char *dev);
        friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
        friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
        friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
        friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
                                char *names[], int n);

        // Tolerance for cooling temperature differences
        static constexpr double TEMP_THRESHOLD {0.01};

        // Threading - streaming mutex
        pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
};

#endif // SV305_CCD_H
