/*
 SVBONY CCD
 SVBONY CCD Camera driver
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

#ifndef SVBONY_CCD_H
#define SVBONY_CCD_H

#include <indiccd.h>
#include <iostream>

#include "libsvbony/SVBCameraSDK.h"

// WORKAROUND for bug #655
// If defined following symbol, get buffered image data before calling StartExposure()
//#define WORKAROUND_latest_image_can_be_getten_next_time

using namespace std;


/////////////////////////////////////////////////
// SVBONYCCD CLASS
//

class SVBONYCCD : public INDI::CCD
{
    public:
        SVBONYCCD(int numCamera);
        virtual ~SVBONYCCD();

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
        // camera infos
        SVB_CAMERA_INFO cameraInfo;
        // camera API handler
        int cameraID;
        // camera property
        SVB_CAMERA_PROPERTY cameraProperty;
        // number of camera control
        int controlsNum;
        // camera propertyEx
        SVB_CAMERA_PROPERTY_EX cameraPropertyEx;
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
        // ROI actual size
        int ROI_width;
        int ROI_height;

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

        // cooler power
        INumber CoolerN[1];
        INumberVectorProperty CoolerNP;

        // a switch for automatic correction of dynamic dead pixels
        ISwitch CorrectDDPS[2];
        ISwitchVectorProperty CorrectDDPSP;
        enum { CORRECT_DDP_ENABLE = 0, CORRECT_DDP_DISABLE = 1 };
        int correctDDPEnable; // 0:Enable, 1:Disable

        // output frame format
        // the camera is able to output RGB24, but not supported by INDI
        // -> ignored
        // NOTE : SV305M PRO doesn't support RAW8 and RAW16, only Y8 and Y16
        size_t nFrameFormat; // number of frame format types without SVB_IMG_RGB24
        SVB_IMG_TYPE defaultFrameFormatIndex; // Index of Default ISwitch in frameFormatDefinions array. The index is the same as SVB_IMG_TYPE.
        int defaultMaxBitDepth; // Maximum bit depth in camera.
        typedef struct frameFormatDefinition
        {
            const char* isName; // Name of ISwitch
            const char* isLabel; // label of ISwitch
            int isBits; // bit depth
            bool isColor; // true:color, false:grayscale
            int isIndex; // index for ISwitch
            ISState isStateDefault; // default ISState
        } FrameFormatDefinition;
        FrameFormatDefinition frameFormatDefinitions[SVB_IMG_RGB24] =
        {
            { "FORMAT_RAW8", "RAW 8 bits", 8, true, -1, ISS_OFF },
            { "FORMAT_RAW10", "RAW 10 bits", 10, true, -1, ISS_OFF },
            { "FORMAT_RAW12", "RAW 12 bits", 12, true, -1, ISS_OFF },
            { "FORMAT_RAW14", "RAW 14 bits", 14, true, -1, ISS_OFF },
            { "FORMAT_RAW16", "RAW 16 bits", 16, true, -1, ISS_OFF },
            { "FORMAT_Y8", "Y 8 bits", 8, false, -1, ISS_OFF },
            { "FORMAT_Y10", "Y 10 bits", 10, false, -1, ISS_OFF },
            { "FORMAT_Y12", "Y 12 bits", 12, false, -1, ISS_OFF },
            { "FORMAT_Y14", "Y 14 bits", 14, false, -1, ISS_OFF },
            { "FORMAT_Y16", "Y 16 bits", 16, false, -1, ISS_OFF }
        };
        SVB_IMG_TYPE *switch2frameFormatDefinitionsIndex;
        SVB_IMG_TYPE frameFormat; // currenct Frame format
        const char* bayerPatternMapping[4] = {"RGGB", "BGGR", "GRBG", "GBRG"};

        virtual bool SetCaptureFormat(uint8_t index) override;

        // exposure timing
        int timerID;
        struct timeval ExpStart;
        float ExposureRequest;
        float CalcTimeLeft();

        // update CCD Params
        bool updateCCDParams();

        // Discard unretrieved exposure data
        void discardVideoData();

        // save settings
        virtual bool saveConfigItems(FILE *fp) override;

        // add FITS fields
        virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;

        // INDI Callbacks
        friend void ::ISGetProperties(const char *dev);
        friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
        friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
        friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
        friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);

        // Threading - streaming mutex
        pthread_cond_t cv;
        pthread_mutex_t condMutex;
};

#endif // SVBONY_CCD_H
