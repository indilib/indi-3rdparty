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
}


Sv305CCD::~Sv305CCD()
{
}


const char *Sv305CCD::getDefaultName()
{
    return "SVBONY SV305 CCD";
}


bool Sv305CCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    SetCCDCapability(CCD_CAN_ABORT);

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


bool Sv305CCD::Connect()
{
    LOG_INFO("Attempting to find the SVBONY SV305 CCD...");

    status = CameraInit(&hCamera, num);
    if(status != CAMERA_STATUS_SUCCESS) {
        LOG_INFO("Error, open camera failed\n");
        return false;
    }
    LOG_INFO("Camera init\n");

    /* basic settings */


    // set slow framerate
    status = CameraSetFrameSpeed(hCamera, 0);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set frame speed failed\n");
        return false;
    }
    LOG_INFO("Camera frame speed\n");

    // disable autoexposure
    status = CameraSetAeState(hCamera, FALSE);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set manual mode failed\n");
        return false;
    }
    LOG_INFO("Camera manual mode\n");

    // default to 1s exposure
    status = CameraSetExposureTime(hCamera, (double)(1000 * 1000));
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set exposure failed\n");
        return false;
    }
    LOG_INFO("Camera set exposure\n");

    // set to 16 bis depth
    status = CameraSetSensorOutPixelFormat(hCamera, CAMERA_MEDIA_TYPE_BAYGR12);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set image format failed\n");
        return false;
    }
    LOG_INFO("Camera image format\n");

    // set default resolution
    status = CameraSetResolution(hCamera, IMAGEOUT_MODE_1920X1080);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set resolution failed\n");
        return false;
    }
    LOG_INFO("Camera resolution\n");

    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.");
    return true;
}


bool Sv305CCD::Disconnect()
{
    // pause camera
    status = CameraPause(hCamera);
    if(status != CAMERA_STATUS_SUCCESS) {
        LOG_INFO("Error, pause camera failed\n");
        return false;
    }

    // destroy camera
    status = CameraUnInit(hCamera);
    LOG_INFO("CCD is offline.");
    return true;
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
    // Actucal CALL to CCD to get pixel size here
    x_pixel_size = 2.9;
    y_pixel_size = 2.9;

    ///////////////////////////
    // 2. Get Frame
    ///////////////////////////

    // Actucal CALL to CCD to get frame information here
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
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8 * 3; //  this is pixel cameraCount
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

    status = CameraSetExposureTime(hCamera, (double)(duration * 1000 * 1000));
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera set exposure failed\n");
        return -1;
    }
    LOG_INFO("Exposure time set\n");


    status = CameraPlay(hCamera);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera start failed\n");
        return -1;
    }
    LOG_INFO("Camera start\n");

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    gettimeofday(&ExpStart, nullptr);
    LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;

    return true;
}


bool Sv305CCD::AbortExposure()
{


    status = CameraPause(hCamera);
    if(status != CAMERA_STATUS_SUCCESS) {
        LOG_INFO("Error, abort camera failed\n");
        return false;
    }
    LOG_INFO("Exposure aborted");

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


/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int Sv305CCD::grabImage()
{
    stImageInfo imgInfo;
    BYTE* pRawBuf;

    imageBuffer = PrimaryCCD.getFrameBuffer();


    pRawBuf = CameraGetImageInfo(hCamera, hRawBuf, &imgInfo);

    status = CameraGetOutImageBuffer(hCamera, &imgInfo, pRawBuf, imageBuffer);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera get image failed\n");
        return -1;
    }
    LOG_INFO("Got image");


    status = CameraReleaseFrameHandle(hCamera, hRawBuf);
    if(status != CAMERA_STATUS_SUCCESS){
        LOG_INFO("Error, camera release buffer failed\n");
        return -1;
    }
    LOG_INFO("Buffer released");

    status = CameraPause(hCamera);
    if(status != CAMERA_STATUS_SUCCESS) {
        LOG_INFO("Error, pause camera failed\n");
    }
    LOG_INFO("Camera stop");

    LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);

    return 0;
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
                    //  it's real close now, so spin on it
                    status = CameraGetRawImageBuffer(hCamera, &hRawBuf, 1000);
                    LOGF_INFO("%d",status);
                    while (status != CAMERA_STATUS_SUCCESS)
                    {
                        LOG_INFO("Wait loop\n");
                        status = CameraGetRawImageBuffer(hCamera, &hRawBuf, 1000);
                        LOGF_INFO("%d",status);
                    }
                    LOG_INFO("Success, camera get buffer\n");

                    /* We're done exposing */
                    LOG_INFO("Exposure done, downloading image...");

                    PrimaryCCD.setExposureLeft(0);
                    InExposure = false;
                    /* grab and save image */
                    grabImage();
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
