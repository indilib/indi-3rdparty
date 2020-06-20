/*
 SV305 CCD
 SVBONY SV305 Camera driver
 Copyright (C) 2020 Blaise-Florentin Collin (thx8411@yahoo.fr)

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

#include "../libsv305/CKCameraInterface.h"

#include "sv305_ccd.h"


#define MAX_DEVICES    8   /* Max device cameraCount */

static int cameraCount;
static Sv305CCD *cameras[MAX_DEVICES];


////////////////////////////////////////////////////////////
// GLOBAL
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
        cameraCount=0;
        status = CameraEnumerateDevice(&cameraCount);
        if(status != CAMERA_STATUS_SUCCESS){
            IDLog("Error, enumerate camera failed\n");
            return;
        }
        IDLog("Camera(s) found\n");
        if(cameraCount == 0) {
            return;
        }

        for(int i=0; i<cameraCount; i++) {
            cameras[i]= new Sv305CCD(i);
        }

        atexit(cleanup);
        isInit = true;
    }
}

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


Sv305CCD::Sv305CCD(int numCamera)
{
    snprintf(this->name, 32, "SVBONY SV305 CCD %d", numCamera);
    setDeviceName(this->name);
    num=numCamera;

    setVersion(SV305_VERSION_MAJOR, SV305_VERSION_MINOR);

    pthread_mutex_init(&hCamera_mutex, NULL);
}


Sv305CCD::~Sv305CCD()
{
    pthread_mutex_destroy(&hCamera_mutex);
}


const char *Sv305CCD::getDefaultName()
{
    return "SVBONY SV305 CCD";
}


bool Sv305CCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    SetCCDCapability(CCD_CAN_ABORT|CCD_HAS_BAYER);

    // Bayer settings
    IUSaveText(&BayerT[0], "0");
    IUSaveText(&BayerT[1], "0");
    IUSaveText(&BayerT[2], "GRBG");

    addConfigurationControl();
    addDebugControl();
    return true;
}


void Sv305CCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}


bool Sv305CCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        timerID = SetTimer(POLLMS);
    }
    else
    {
        rmTimer(timerID);
    }

    return true;
}


bool Sv305CCD::Init()
{
    LOG_INFO("Attempting to find the SVBONY SV305 CCD...");

    pthread_mutex_lock(&hCamera_mutex);

    status = CameraInit(&hCamera, num);
    if(status != CAMERA_STATUS_SUCCESS) {
        LOG_INFO("Error, open camera failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera init\n");

    /* basic settings */

    // set slow framerate
    status = CameraSetFrameSpeed(hCamera, 0);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set frame speed failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera frame speed\n");

    // disable autoexposure
    status = CameraSetAeState(hCamera, FALSE);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set manual mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera manual mode\n");

    // disable flicker
    status = CameraSetAntiFlick(hCamera, FALSE);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set flicker mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera flicker off\n");

    // disable white balance
    status = CameraSetWbMode(hCamera, FALSE);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set white balance mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera white balance off\n");

    // default to 1s exposure
    status = CameraSetExposureTime(hCamera, (double)(1000 * 1000));
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set exposure failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera set exposure\n");

    // set to bayer 16 bis depth
    status = CameraSetSensorOutPixelFormat(hCamera, CAMERA_MEDIA_TYPE_BAYGR12);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set image format failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    status = CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_BAYGR12);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set image format failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera image format\n");

    // set default resolution
    status = CameraSetResolution(hCamera, IMAGEOUT_MODE_1920X1080);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set resolution failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera resolution\n");

    // start camera
    status = CameraPlay(hCamera);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera start failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera start\n");

    // set camera soft trigger mode
    status = CameraSetTriggerMode(hCamera,1);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    pthread_mutex_unlock(&hCamera_mutex);

    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.");
    return true;
}

bool Sv305CCD::Uninit()
{
    pthread_mutex_lock(&hCamera_mutex);

    // pause camera
    status = CameraPause(hCamera);
    if(status != CAMERA_STATUS_SUCCESS) {
        LOG_INFO("Error, pause camera failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }

    // destroy camera
    status = CameraUnInit(hCamera);
    LOG_INFO("CCD is offline.");
    return true;

    pthread_mutex_unlock(&hCamera_mutex);
}


bool Sv305CCD::Connect()
{
    return(Init());
}

bool Sv305CCD::Disconnect()
{
    return(Uninit());
}


bool Sv305CCD::setupParams()
{
    float x_pixel_size, y_pixel_size;
    int bit_depth = 16;
    int x_1, y_1, x_2, y_2;

    minDuration = 0.01;

    ///////////////////////////
    // 1. Get Pixel size
    ///////////////////////////
    x_pixel_size = 2.9;
    y_pixel_size = 2.9;

    ///////////////////////////
    // 2. Get Frame
    ///////////////////////////

    x_1 = y_1 = 0;
    x_2       = 1920;
    y_2       = 1080;

    ///////////////////////////
    // 4. Get pixel depth
    ///////////////////////////
    bit_depth = 16;
    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    // Let's calculate required buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8; //  this is pixel cameraCount
    // add some spare space
    nbuf+=512;
    PrimaryCCD.setFrameBufferSize(nbuf);

    LOGF_INFO("PrimaryCCD buffer size : %d", nbuf);

    return true;
}


bool Sv305CCD::StartExposure(float duration)
{
    if (duration < minDuration)
    {
        DEBUGF(INDI::Logger::DBG_WARNING,
               "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", duration,
               minDuration);
        duration = minDuration;
    }

    LOG_INFO("Exposure start\n");

    pthread_mutex_lock(&hCamera_mutex);

    // set exposure time
    status = CameraSetExposureTime(hCamera, (double)(duration * 1000 * 1000));
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set exposure failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return -1;
    }
    LOG_INFO("Exposure time set\n");

    // soft trigger
    status = CameraSoftTrigger(hCamera);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, soft trigger failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return -1;
    }
    LOG_INFO("Soft trigger\n");

    pthread_mutex_unlock(&hCamera_mutex);

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    gettimeofday(&ExpStart, nullptr);
    LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;

    return true;
}


bool Sv305CCD::AbortExposure()
{
    // trick : we switch trigger mode to abort exposure
    // side effect : we get a junk frame
    // TODO : fix junk frame

    pthread_mutex_lock(&hCamera_mutex);

    // set camera continuous trigger mode
    status = CameraSetTriggerMode(hCamera,0);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    // set camera soft trigger mode
    status = CameraSetTriggerMode(hCamera,1);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&hCamera_mutex);
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    pthread_mutex_unlock(&hCamera_mutex);

    InExposure = false;
    return true;
}


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
            LOG_INFO("Less than 1s\n");
            if (timeleft > 0.25)
            {
                LOG_INFO("More than 0.25s\n");
                //  a quarter of a second or more
                //  just set a tighter timer
                timerID = SetTimer(250);
            }
            else
            {
                LOG_INFO("Less than 0.25s\n");
                if (timeleft > 0.07)
                {
                    LOG_INFO("More than 0.07s\n");
                    //  use an even tighter timer
                    timerID = SetTimer(50);
                }
                else
                {
                    LOG_INFO("Less than 0.07s\n");

                    HANDLE hRawBuf;

                    pthread_mutex_lock(&hCamera_mutex);

                    //  it's realy close now, so spin on it
                    status = CameraGetRawImageBuffer(hCamera, &hRawBuf, 100);
                    // TODO : add timeout to exit the loop
                    while (status != CAMERA_STATUS_SUCCESS)
                    {
                        status = CameraGetRawImageBuffer(hCamera, &hRawBuf, 100);
                    }
                    LOG_INFO("Success, camera get buffer\n");

                    /* We're done exposing */
                    LOG_INFO("Exposure done, downloading image...");

                    PrimaryCCD.setExposureLeft(0);
                    InExposure = false;

                    // grab and save image
                    stImageInfo imgInfo;
                    BYTE* pRawBuf;

                    imageBuffer = PrimaryCCD.getFrameBuffer();

                    pRawBuf = CameraGetImageInfo(hCamera, hRawBuf, &imgInfo);

                    // we don't use CameraGetOutImageBuffer to get raw datas
                    memcpy(imageBuffer, pRawBuf, imgInfo.TotalBytes);

                    status = CameraReleaseFrameHandle(hCamera, hRawBuf);
                    if(status != CAMERA_STATUS_SUCCESS){
                        LOG_INFO("Error, camera release buffer failed\n");
                    }
                    LOG_INFO("Buffer released");

                    pthread_mutex_unlock(&hCamera_mutex);

                    LOG_INFO("Download complete.");

                    ExposureComplete(&PrimaryCCD);
                }
            }
        }
        else
        {
            LOG_INFO("More than 1s\n");
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
