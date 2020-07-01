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

#include "libsv305/CKCameraInterface.h"

///////////////////////////////////////////////////
// DEFAULT SETTINGS
//

// picture
#define CAM_X_RESOLUTION	1920
#define CAM_Y_RESOLUTION	1080
#define CAM_DEPTH		16
#define CAM_IMAGE_FORMAT	IMAGEOUT_MODE_1920X1080
#define CAM_BAYER_PATTERN	"GRBG"

// sensor pixel size
#define CAM_X_PIXEL	2.9
#define CAM_Y_PIXEL	2.9

// default grab timeout (ms)
#define CAM_DEFAULT_GRAB_TIMEOUT	100
// default grab loops # if grab failed
#define CAM_DEFAULT_GRAB_LOOPS	10


/* I don't trust SDK get capabilities values : */
/* -> hard settings */

// reported exposure time :
// min = 1 us : useless
// max = 60 s : wrong, can expose much more
// so, min exposure (s) hard setting :
#define CAM_MIN_EXPOSURE		0.01

// analog gain hard settings (1 to 30)
#define CAM_MIN_GAIN	1
#define CAM_MAX_GAIN	30
#define CAM_STEP_GAIN	1
#define CAM_DEFAULT_GAIN	1

// max camera number
#define CAM_MAX_DEVICES    8



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

    protected:
        // INDI periodic grab query
        void TimerHit() override;

    private:
        // camera API return status
        CameraSdkStatus status;
        // hCamera mutex protection
        pthread_mutex_t hCamera_mutex;
        // camera API handler
        HANDLE hCamera;
        // camera #
        int num;
        // camera name
        char name[32];

        // image offsets and size
        int x_1, y_1, x_2, y_2;
        // do we use subframes ?
        bool subFrame;
        // do we bin ?
        bool binning;

        // streaming
        bool streaming;
        pthread_mutex_t streaming_mutex;
        pthread_t primary_thread;
        bool terminateThread;

        // gain setting
        INumber GainN[1];
        INumberVectorProperty GainNP;
        enum
        {
            CCD_GAIN_N
        };

        // exposure timing
        int timerID;
        struct timeval ExpStart;
        float ExposureRequest;
        float CalcTimeLeft();

        // setups
        bool setupParams();

        // reads a junk frame and drops it
        void GrabJunkFrame();

        // save settings
        virtual bool saveConfigItems(FILE *fp) override;

        // add FITS fields
        virtual void addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip) override;

        // INDI Callbacks
        friend void ::ISGetProperties(const char *dev);
        friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
        friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
        friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
        friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
                                char *names[], int n);
};

#endif // SV305_CCD_H
