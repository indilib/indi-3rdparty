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

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "config.h"
#include "indidevapi.h"
#include "eventloop.h"
#include "stream/streammanager.h"

#include "libsv305/CKCameraInterface.h"

#include "sv305_ccd.h"

// streaming mutex
static pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

// cameras storage
static int cameraCount;
static Sv305CCD *cameras[CAM_MAX_DEVICES];



////////////////////////////////////////////////////////////
// GLOBAL INDI DRIVER API
//


// clear all cameras
static void cleanup()
{
    for (int i = 0; i < cameraCount; i++)
    {
        delete cameras[i];
    }
}


// driver init
void ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        CameraSdkStatus status;
        cameraCount = 0;

        // enumerate cameras
        status = CameraEnumerateDevice(&cameraCount);
        if(status != CAMERA_STATUS_SUCCESS)
        {
            IDLog("Error, enumerate camera failed\n");
            return;
        }

        IDLog("Camera(s) found\n");

        if(cameraCount == 0)
        {
            return;
        }

        for(int i = 0; i < cameraCount; i++)
        {
            cameras[i] = new Sv305CCD(i);
        }

        atexit(cleanup);
        isInit = true;
    }
}


// forwarder
void ISGetProperties(const char *dev)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        Sv305CCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}


// forwarder
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        Sv305CCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}


// forwarder
void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        Sv305CCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}


// forwarder
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        Sv305CCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
}


//
void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}


//
void ISSnoopDevice(XMLEle *root)
{
    ISInit();

    for (int i = 0; i < cameraCount; i++)
    {
        Sv305CCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}


//////////////////////////////////////////////////
// SV305 CLASS
//


//
Sv305CCD::Sv305CCD(int numCamera)
{
    snprintf(this->name, 32, "SVBONY SV305 CCD %d", numCamera);
    setDeviceName(this->name);
    num = numCamera;

    setVersion(SV305_VERSION_MAJOR, SV305_VERSION_MINOR);

    // mutex init
    pthread_mutex_init(&hCamera_mutex, NULL);
    pthread_mutex_init(&streaming_mutex, NULL);
}


//
Sv305CCD::~Sv305CCD()
{
    // mutex destroy
    pthread_mutex_destroy(&hCamera_mutex);
    pthread_mutex_destroy(&streaming_mutex);
}


//
const char *Sv305CCD::getDefaultName()
{
    return "SVBONY SV305 CCD";
}


//
bool Sv305CCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    SetCCDCapability(CCD_CAN_ABORT | CCD_HAS_BAYER | CCD_CAN_SUBFRAME | CCD_CAN_BIN | CCD_HAS_STREAMING);

    // Bayer settings
    IUSaveText(&BayerT[0], "0");
    IUSaveText(&BayerT[1], "0");
    IUSaveText(&BayerT[2], CAM_BAYER_PATTERN);

    // Gain
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%.f", CAM_MIN_GAIN, CAM_MAX_GAIN, CAM_STEP_GAIN, CAM_DEFAULT_GAIN);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    addConfigurationControl();
    addDebugControl();
    return true;
}


//
void Sv305CCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}


//
bool Sv305CCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {

        // gain
        defineNumber(&GainNP);

        // Let's get parameters now from CCD
        setupParams();

        timerID = SetTimer(POLLMS);
    }
    else
    {
        rmTimer(timerID);

        // gain
        deleteProperty(GainNP.name);
    }

    return true;
}


//
bool Sv305CCD::Connect()
{
    // boolean init
    subFrame = false;
    binning = false;
    streaming = false;

    LOG_INFO("Attempting to find the SVBONY SV305 CCD...\n");

    pthread_mutex_lock(&hCamera_mutex);

    // init camera
    status = CameraInit(&hCamera, num);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, open camera failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera init\n");

    // set slow framerate
    status = CameraSetFrameSpeed(hCamera, FRAME_SPEED_LOW);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set frame speed failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera slow frame speed\n");

    // disable autoexposure
    status = CameraSetAeState(hCamera, FALSE);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set manual mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera manual mode\n");

    // disable flicker
    status = CameraSetAntiFlick(hCamera, FALSE);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set flicker mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera flicker off\n");

    // disable white balance
    status = CameraSetWbMode(hCamera, FALSE);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set white balance mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera white balance off\n");

    // default analog gain
    status = CameraSetAnalogGain(hCamera, CAM_DEFAULT_GAIN * 1000);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set analog gain failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera set default analog gain\n");

    // default exposure (us)
    status = CameraSetExposureTime(hCamera, (double)(CAM_MIN_EXPOSURE * 1000000));
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set exposure failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera set default exposure\n");

    // set to bayer 16 bis depth
    status = CameraSetSensorOutPixelFormat(hCamera, CAMERA_MEDIA_TYPE_BAYGR12);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set image format failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    status = CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_BAYGR12);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set image format failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera image format set\n");

    // set default resolution
    status = CameraSetResolution(hCamera, IMAGEOUT_MODE_1920X1080);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set resolution failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera resolution set\n");

    // set camera soft trigger mode
    status = CameraSetTriggerMode(hCamera, TRIGGER_MODE_SOFT);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    // start camera
    status = CameraPlay(hCamera);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera start failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera start\n");

    pthread_mutex_unlock(&hCamera_mutex);

    // setting trigger mode gives at first a junk frame
    // we drop it
    GrabJunkFrame();

    // streaming thread
    terminateThread = false;
    pthread_create(&primary_thread, nullptr, &streamVideoHelper, this);

    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.\n");
    return true;
}


//
bool Sv305CCD::Disconnect()
{
    // destroy streaming
    pthread_mutex_lock(&condMutex);
    streaming = true;
    terminateThread = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    pthread_mutex_lock(&hCamera_mutex);

    // pause camera
    status = CameraPause(hCamera);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, pause camera failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }

    // destroy camera
    status = CameraUnInit(hCamera);
    LOG_INFO("CCD is offline.\n");
    return true;

    pthread_mutex_unlock(&hCamera_mutex);
}


// set CCD parameters
bool Sv305CCD::setupParams()
{
    float x_pixel_size, y_pixel_size;
    int bit_depth;

    subFrame = false;
    binning = false;
    streaming = false;

    // pixel size
    x_pixel_size = CAM_X_PIXEL;
    y_pixel_size = CAM_Y_PIXEL;;

    // frame offsets and size
    x_1 = y_1 = 0;
    x_2       = CAM_X_RESOLUTION;
    y_2       = CAM_Y_RESOLUTION;

    // pixel depth
    bit_depth = CAM_DEPTH;
    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    // Let's calculate required buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8; //  this is pixel cameraCount
    // add some spare space
    nbuf += 512;
    PrimaryCCD.setFrameBufferSize(nbuf);

    LOGF_INFO("PrimaryCCD buffer size : %d\n", nbuf);

    // stream init
    Streamer->setPixelFormat(INDI_BAYER_GRBG, CAM_DEPTH);
    Streamer->setSize(PrimaryCCD.getXRes() / 2, PrimaryCCD.getYRes() / 2);

    return true;
}


//
bool Sv305CCD::StartExposure(float duration)
{
    // checks for min time
    if (duration < CAM_MIN_EXPOSURE)
    {
        LOGF_WARN(
               "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.\n", duration,
               CAM_MIN_EXPOSURE);
        duration = CAM_MIN_EXPOSURE;
    }

    pthread_mutex_lock(&hCamera_mutex);

    // set exposure time (s -> us)
    status = CameraSetExposureTime(hCamera, (double)(duration * 1000000));
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set exposure failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return -1;
    }

    // soft trigger
    status = CameraSoftTrigger(hCamera);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, soft trigger failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return -1;
    }

    pthread_mutex_unlock(&hCamera_mutex);

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    gettimeofday(&ExpStart, nullptr);
    LOGF_INFO("Taking a %g seconds frame...\n", ExposureRequest);

    InExposure = true;

    return true;
}


//
bool Sv305CCD::AbortExposure()
{
    // trick : we switch trigger mode to abort exposure

    LOG_INFO("Abort exposure\n");

    InExposure = false;

    pthread_mutex_lock(&hCamera_mutex);

    // set camera continuous trigger mode
    status = CameraSetTriggerMode(hCamera, TRIGGER_MODE_CONTINUOUS);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }

    // set camera soft trigger mode
    status = CameraSetTriggerMode(hCamera, TRIGGER_MODE_SOFT);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }

    pthread_mutex_unlock(&hCamera_mutex);

    // switching trigger mode gives at first a junk frame
    // we drop it
    GrabJunkFrame();

    return true;
}


//
bool Sv305CCD::StartStreaming()
{
    // streaming exposure time
    // getTargetExposure only available since indi core 1.8
    //ExposureRequest = Streamer->getTargetExposure();
    ExposureRequest = 1.0 / Streamer->getTargetFPS();

    pthread_mutex_lock(&hCamera_mutex);

    // set camera continuous trigger mode
    status = CameraSetTriggerMode(hCamera, TRIGGER_MODE_CONTINUOUS);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }

    // set exposure time (s -> us)
    status = CameraSetExposureTime(hCamera, (double)(ExposureRequest * 1000000));
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera set exposure failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return -1;
    }

    pthread_mutex_unlock(&hCamera_mutex);

    pthread_mutex_lock(&condMutex);
    streaming = true;
    pthread_mutex_unlock(&condMutex);

    pthread_cond_signal(&cv);

    LOG_INFO("Streaming started\n");

    return true;
}


//
bool Sv305CCD::StopStreaming()
{
    pthread_mutex_lock(&hCamera_mutex);

    // set camera soft trigger mode back
    status = CameraSetTriggerMode(hCamera, TRIGGER_MODE_SOFT);
    if(status != CAMERA_STATUS_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }

    pthread_mutex_unlock(&hCamera_mutex);

    // grab the junk frame
    GrabJunkFrame();

    pthread_mutex_lock(&condMutex);
    streaming = false;
    pthread_mutex_unlock(&condMutex);

    pthread_cond_signal(&cv);

    LOG_INFO("Streaming stopped\n");

    return true;
}


//
void* Sv305CCD::streamVideoHelper(void * context)
{
    return static_cast<Sv305CCD *>(context)->streamVideo();
}


//
void* Sv305CCD::streamVideo()
{
    auto start = std::chrono::high_resolution_clock::now();
    auto finish = std::chrono::high_resolution_clock::now();

    while (true)
    {
        pthread_mutex_lock(&condMutex);

        while (!streaming)
        {
            pthread_cond_wait(&cv, &condMutex);
            // getTargetExposure only available since indi core 1.8
            //ExposureRequest = Streamer->getTargetExposure();
            ExposureRequest = 1.0 / Streamer->getTargetFPS();
        }

        if (terminateThread)
            break;

        pthread_mutex_unlock(&condMutex);

        stImageInfo imgInfo;
        HANDLE hRawBuf;
        BYTE* pRawBuf;
        BYTE* imageBuffer = PrimaryCCD.getFrameBuffer();

        pthread_mutex_lock(&hCamera_mutex);

        // get the frame
        status = CameraGetRawImageBuffer(hCamera, &hRawBuf, CAM_DEFAULT_GRAB_TIMEOUT);
        if(status == CAMERA_STATUS_SUCCESS)
        {

            // get frame informations
            pRawBuf = CameraGetImageInfo(hCamera, hRawBuf, &imgInfo);

            // copy full frame
            memcpy(imageBuffer, pRawBuf, imgInfo.TotalBytes);

            // release camera frame buffer
            status = CameraReleaseFrameHandle(hCamera, hRawBuf);
            if(status != CAMERA_STATUS_SUCCESS)
            {
                LOG_ERROR("Error, camera release buffer failed\n");
            }

            pthread_mutex_unlock(&hCamera_mutex);

            if(binning)
                PrimaryCCD.binFrame();

            finish = std::chrono::high_resolution_clock::now();

            uint32_t size = PrimaryCCD.getFrameBufferSize() / (PrimaryCCD.getBinX() * PrimaryCCD.getBinY());
            Streamer->newFrame(PrimaryCCD.getFrameBuffer(), size);

        }
        else
        {
            pthread_mutex_unlock(&hCamera_mutex);
        }

        std::chrono::duration<double> elapsed = finish - start;
        if (elapsed.count() < ExposureRequest)
            usleep(fabs(ExposureRequest - elapsed.count()) * 1e6);

        start = std::chrono::high_resolution_clock::now();
    }

    return nullptr;
}


// subframing
bool Sv305CCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    if((x + w) > CAM_X_RESOLUTION || (y + h) > CAM_Y_RESOLUTION)
    {
        LOG_ERROR("Error : Subframe out of range");
        return false;
    }

    // full frame or subframe ?
    if(x == 0 && y == 0 && w == CAM_X_RESOLUTION && h == CAM_Y_RESOLUTION)
        subFrame = false;
    else
        subFrame = true;

    // update frame offsets and size
    x_1 = x;
    x_2 = x_1 + w;
    y_1 = y;
    y_2 = y_1 + h;

    LOG_INFO("Subframe changed\n");

    return INDI::CCD::UpdateCCDFrame(x, y, w, h);;
}


// binning
bool Sv305CCD::UpdateCCDBin(int hor, int ver)
{
    if(hor == 1 && ver == 1)
        binning = false;
    else
        binning = true;

    LOG_INFO("Binning changed");

    return INDI::CCD::UpdateCCDBin(hor, ver);
}


//
float Sv305CCD::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;
    return timeleft;
}


// after trigger mode switching, first frame is a junk frame
// we drop it
void Sv305CCD::GrabJunkFrame()
{
    HANDLE hRawBuf;

    pthread_mutex_lock(&hCamera_mutex);

    status = CameraSetExposureTime(hCamera, (double)(CAM_MIN_EXPOSURE * 20 * 1000000));
    status = CameraSoftTrigger(hCamera);
    status = CameraGetRawImageBuffer(hCamera, &hRawBuf, CAM_DEFAULT_GRAB_TIMEOUT);
    int c = CAM_DEFAULT_GRAB_LOOPS;
    while (status != CAMERA_STATUS_SUCCESS && c > 0)
    {
        c--;
        status = CameraGetRawImageBuffer(hCamera, &hRawBuf, CAM_DEFAULT_GRAB_TIMEOUT);
    }
    status = CameraReleaseFrameHandle(hCamera, hRawBuf);

    pthread_mutex_unlock(&hCamera_mutex);

    LOG_INFO("Junk frame dropped");
}


// grab loop
void Sv305CCD::TimerHit()
{
    int timerID = -1;
    long timeleft;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft = CalcTimeLeft();

        if (timeleft < 1.0)
        {
            if (timeleft > 0.25)
            {
                //  a quarter of a second or more
                //  just set a tighter timer
                timerID = SetTimer(250);
            }
            else
            {
                if (timeleft > 0.07)
                {
                    //  use an even tighter timer
                    timerID = SetTimer(50);
                }
                else
                {
                    HANDLE hRawBuf;

                    pthread_mutex_lock(&hCamera_mutex);

                    // grab frame
                    status = CameraGetRawImageBuffer(hCamera, &hRawBuf, CAM_DEFAULT_GRAB_TIMEOUT);
                    // camera call already waited, no more than X loops needed
                    // or we get a timeout
                    int c = CAM_DEFAULT_GRAB_LOOPS;
                    while (status != CAMERA_STATUS_SUCCESS && c > 0)
                    {
                        c--;
                        status = CameraGetRawImageBuffer(hCamera, &hRawBuf, CAM_DEFAULT_GRAB_TIMEOUT);
                    }

                    // timeout : we return a buffer full of 0
                    if(c == 0)
                    {
                        LOG_ERROR("Camera get buffer timed out\n");
                        pthread_mutex_unlock(&hCamera_mutex);
                        PrimaryCCD.setExposureLeft(0);
                        InExposure = false;
                        BYTE* imageBuffer = PrimaryCCD.getFrameBuffer();
                        memset(imageBuffer, 0x00, PrimaryCCD.getFrameBufferSize());
                        ExposureComplete(&PrimaryCCD);
                        return;
                    }

                    /* We're done exposing */
                    PrimaryCCD.setExposureLeft(0);
                    InExposure = false;

                    // grab and save the buffer
                    stImageInfo imgInfo;
                    BYTE* pRawBuf;
                    BYTE* imageBuffer = PrimaryCCD.getFrameBuffer();

                    // get frame info
                    pRawBuf = CameraGetImageInfo(hCamera, hRawBuf, &imgInfo);

                    // we don't use CameraGetOutImageBuffer to avoid post processing
                    // we get raw datas

                    if(subFrame)
                    {
                        // copy sub frame
                        int k = 0;
                        for(int i = y_1; i < y_2; i++)
                        {
                            memcpy(imageBuffer + k, pRawBuf + (i * CAM_X_RESOLUTION * 2) + (x_1 * 2), (x_2 - x_1) * 2);
                            k += (x_2 - x_1) * 2;
                        }
                    }
                    else
                    {
                        // copy full frame
                        memcpy(imageBuffer, pRawBuf, imgInfo.TotalBytes);
                    }

                    // release camera frame buffer
                    status = CameraReleaseFrameHandle(hCamera, hRawBuf);
                    if(status != CAMERA_STATUS_SUCCESS)
                    {
                        LOG_ERROR("Error, camera release buffer failed\n");
                    }

                    pthread_mutex_unlock(&hCamera_mutex);

                    if(binning)
                        PrimaryCCD.binFrame();

                    // done
                    ExposureComplete(&PrimaryCCD);
                }
            }
        }
        else
        {
            if (isDebug())
            {
                IDLog("With time left %ld\n", timeleft);
                IDLog("image not yet ready....\n");
            }

            PrimaryCCD.setExposureLeft(timeleft);
        }
    }

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}


//
bool Sv305CCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    // look for gain settings
    if (!strcmp(name, GainNP.name))
    {
        IUUpdateNumber(&GainNP, values, names, n);

        pthread_mutex_unlock(&hCamera_mutex);

        // update camera get settings
        status = CameraSetAnalogGain(hCamera, GainN[CCD_GAIN_N].value * 1000);
        if(status != CAMERA_STATUS_SUCCESS)
            LOG_ERROR("Error, camera set analog gain failed\n");
        LOGF_INFO("Camera analog gain set to %.f\n", GainN[CCD_GAIN_N].value);

        pthread_mutex_unlock(&hCamera_mutex);

        // avoid a mid frame gain change
        GrabJunkFrame();

        GainNP.s = IPS_OK;
        IDSetNumber(&GainNP, nullptr);
        return true;
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}


//
bool Sv305CCD::saveConfigItems(FILE * fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);

    // Gain
    IUSaveConfigNumber(fp, &GainNP);

    return true;
}


//
void Sv305CCD::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    // report gain in FITS file
    int _status = 0;
    fits_update_key_dbl(fptr, "Gain", GainN[0].value, 3, "Gain", &_status);
}
