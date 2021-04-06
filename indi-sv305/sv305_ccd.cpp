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

#include "libsv305/SVBCameraSDK.h"

#include "sv305_ccd.h"

// streaming mutex
static pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

// cameras storage
static int cameraCount;
static Sv305CCD *cameras[SVBCAMERA_ID_MAX];



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
        cameraCount = 0;

        // enumerate cameras
        cameraCount=SVBGetNumOfConnectedCameras();
        if(cameraCount < 1)
        {
            IDLog("Error, no camera found\n");
            return;
        }

        IDLog("Camera(s) found\n");

        // create Sv305CCD object for each camera
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
    num = numCamera;

    // set driver version
    setVersion(SV305_VERSION_MAJOR, SV305_VERSION_MINOR);

    // Get camera informations
    status = SVBGetCameraInfo(&cameraInfo, num);
    if(status!=SVB_SUCCESS)
    {
        LOG_ERROR("Error, can't get camera's informations\n");
    }

    cameraID=cameraInfo.CameraID;

    // Set camera name
    snprintf(this->name, 32, "%s %d", cameraInfo.FriendlyName, numCamera);
    setDeviceName(this->name);

    // mutex init
    pthread_mutex_init(&cameraID_mutex, NULL);
    pthread_mutex_init(&streaming_mutex, NULL);
}


//
Sv305CCD::~Sv305CCD()
{
    // mutex destroy
    pthread_mutex_destroy(&cameraID_mutex);
    pthread_mutex_destroy(&streaming_mutex);
}


//
const char *Sv305CCD::getDefaultName()
{
    return "SVBONY SV305";
}


//
bool Sv305CCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    // base capabilities
    uint32_t cap = /* CCD_CAN_ABORT | */ CCD_HAS_BAYER | CCD_CAN_SUBFRAME | CCD_CAN_BIN | CCD_HAS_STREAMING;

    // SV305 Pro has an ST4 port
    if(strcmp(cameraInfo.FriendlyName, "SVBONY SV305PRO")==0)
    {
        cap|= CCD_HAS_ST4_PORT;
    }

    SetCCDCapability(cap);

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

        // define controls
        defineProperty(&ControlsNP[CCD_GAIN_N]);
        defineProperty(&ControlsNP[CCD_CONTRAST_N]);
        defineProperty(&ControlsNP[CCD_SHARPNESS_N]);
        defineProperty(&ControlsNP[CCD_SATURATION_N]);
        defineProperty(&ControlsNP[CCD_WBR_N]);
        defineProperty(&ControlsNP[CCD_WBG_N]);
        defineProperty(&ControlsNP[CCD_WBB_N]);
        defineProperty(&ControlsNP[CCD_GAMMA_N]);
        defineProperty(&ControlsNP[CCD_DOFFSET_N]);

        // define frame format
        defineProperty(&FormatSP);
        defineProperty(&SpeedSP);

        // stretch factor
        defineProperty(&StretchSP);

        timerID = SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        rmTimer(timerID);

        // delete controls
        deleteProperty(ControlsNP[CCD_GAIN_N].name);
        deleteProperty(ControlsNP[CCD_CONTRAST_N].name);
        deleteProperty(ControlsNP[CCD_SHARPNESS_N].name);
        deleteProperty(ControlsNP[CCD_SATURATION_N].name);
        deleteProperty(ControlsNP[CCD_WBR_N].name);
        deleteProperty(ControlsNP[CCD_WBG_N].name);
        deleteProperty(ControlsNP[CCD_WBB_N].name);
        deleteProperty(ControlsNP[CCD_GAMMA_N].name);
        deleteProperty(ControlsNP[CCD_DOFFSET_N].name);

        // delete frame format
        deleteProperty(FormatSP.name);
        deleteProperty(SpeedSP.name);

        // stretch factor
        deleteProperty(StretchSP.name);
    }

    return true;
}


//
bool Sv305CCD::Connect()
{
    // boolean init
    streaming = false;

    LOG_INFO("Attempting to find the SVBONY SV305 CCD...\n");

    pthread_mutex_lock(&cameraID_mutex);

    // open camera
    status = SVBOpenCamera(cameraID);
    if (status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, open camera failed.\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // wait a bit for the camera to get ready
    usleep(0.5 * 1e6);

    // get camera properties
    status = SVBGetCameraProperty(cameraID, &cameraProperty);
    if (status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, get camera property failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // get camera pixel size
    status = SVBGetSensorPixelSize(cameraID, &pixelSize);
    if (status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, get camera pixel size failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // get num of controls
    status = SVBGetNumOfControls(cameraID, &controlsNum);
    if (status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, get camera controls failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // fix for SDK gain error issue
    // set exposure time
    SVBSetControlValue(cameraID, SVB_EXPOSURE , (double)(1 * 1000000), SVB_FALSE);

    // read controls and feed UI
    for(int i=0; i<controlsNum; i++)
    {
         // read control
         SVB_CONTROL_CAPS caps;
         status = SVBGetControlCaps(cameraID, i, &caps);
         if(status != SVB_SUCCESS)
         {
             LOG_ERROR("Error, get camera controls caps failed\n");
             pthread_mutex_unlock(&cameraID_mutex);
             return false;
         }
         switch(caps.ControlType)
         {
             case SVB_EXPOSURE :
                 // Exposure
                 minExposure = (double)caps.MinValue / 1000000.0;
                 maxExposure = (double)caps.MaxValue / 1000000.0;
                 PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", minExposure, maxExposure, 1, true);
                 break;

             case SVB_GAIN :
                 // Gain
                 IUFillNumber(&ControlsN[CCD_GAIN_N], "GAIN", "Gain", "%.f", caps.MinValue, caps.MaxValue, 10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_GAIN_N], &ControlsN[CCD_GAIN_N], 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_GAIN , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set gain failed\n");
                 }
                 break;

             case SVB_CONTRAST :
                 // Contrast
                 IUFillNumber(&ControlsN[CCD_CONTRAST_N], "CONTRAST", "Contrast", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue/10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_CONTRAST_N], &ControlsN[CCD_CONTRAST_N], 1, getDeviceName(), "CCD_CONTRAST", "Contrast", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_CONTRAST , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set contrast failed\n");
                 }
                 break;

             case SVB_SHARPNESS :
                 // Sharpness
                 IUFillNumber(&ControlsN[CCD_SHARPNESS_N], "SHARPNESS", "Sharpness", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue/10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_SHARPNESS_N], &ControlsN[CCD_SHARPNESS_N], 1, getDeviceName(), "CCD_SHARPNESS", "Sharpness", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_SHARPNESS , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set sharpness failed\n");
                 }
                 break;

             case SVB_SATURATION :
                 // Saturation
                 IUFillNumber(&ControlsN[CCD_SATURATION_N], "SATURATION", "Saturation", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue/10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_SATURATION_N], &ControlsN[CCD_SATURATION_N], 1, getDeviceName(), "CCD_SATURATION", "Saturation", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_SATURATION , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set saturation failed\n");
                 }
                 break;

             case SVB_WB_R :
                 // Red White Balance
                 IUFillNumber(&ControlsN[CCD_WBR_N], "WBR", "Red White Balance", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue/10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_WBR_N], &ControlsN[CCD_WBR_N], 1, getDeviceName(), "CCD_WBR", "Red White Balance", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_WB_R , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set red WB failed\n");
                 }
                 break;

             case SVB_WB_G :
                 // Green White Balance
                 IUFillNumber(&ControlsN[CCD_WBG_N], "WBG", "Green White Balance", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue/10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_WBG_N], &ControlsN[CCD_WBG_N], 1, getDeviceName(), "CCD_WBG", "Green White Balance", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_WB_G , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set green WB failed\n");
                 }
                 break;

             case SVB_WB_B :
                 // Blue White Balance
                 IUFillNumber(&ControlsN[CCD_WBB_N], "WBB", "Blue White Balance", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue/10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_WBB_N], &ControlsN[CCD_WBB_N], 1, getDeviceName(), "CCD_WBB", "Blue White Balance", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_WB_B , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set blue WB failed\n");
                 }
                 break;

             case SVB_GAMMA :
                 // Gamma
                 IUFillNumber(&ControlsN[CCD_GAMMA_N], "GAMMA", "Gamma", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue/10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_GAMMA_N], &ControlsN[CCD_GAMMA_N], 1, getDeviceName(), "CCD_GAMMA", "Gamma", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_GAMMA , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set gamma failed\n");
                 }
                 break;

             case SVB_BLACK_LEVEL :
                 // Dark Offset
                 IUFillNumber(&ControlsN[CCD_DOFFSET_N], "DOFFSET", "Dark Offset", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue/10, caps.DefaultValue);
                 IUFillNumberVector(&ControlsNP[CCD_DOFFSET_N], &ControlsN[CCD_DOFFSET_N], 1, getDeviceName(), "CCD_DOFFSET", "Dark Offset", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                 status = SVBSetControlValue(cameraID, SVB_BLACK_LEVEL , caps.DefaultValue, SVB_FALSE);
                 if(status != SVB_SUCCESS)
                 {
                     LOG_ERROR("Error, camera set dark offset failed\n");
                 }

             default :
                 break;
         }
    }

    // set frame speed
    IUFillSwitch(&SpeedS[SPEED_SLOW], "SPEED_SLOW", "Slow", ISS_OFF);
    IUFillSwitch(&SpeedS[SPEED_NORMAL], "SPEED_NORMAL", "Normal", ISS_ON);
    IUFillSwitch(&SpeedS[SPEED_FAST], "SPEED_FAST", "Fast", ISS_OFF);
    IUFillSwitchVector(&SpeedSP, SpeedS, 3, getDeviceName(), "FRAME_RATE", "Frame rate", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    frameSpeed=SPEED_NORMAL;
    status = SVBSetControlValue(cameraID, SVB_FRAME_SPEED_MODE , SPEED_NORMAL, SVB_FALSE);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set frame speed failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // set frame format and feed UI
    IUFillSwitch(&FormatS[FORMAT_RAW8], "FORMAT_RAW8", "Raw 8 bits", ISS_OFF);
    IUFillSwitch(&FormatS[FORMAT_RAW12], "FORMAT_RAW12", "Raw 12 bits", ISS_ON);
    IUFillSwitchVector(&FormatSP, FormatS, 2, getDeviceName(), "FRAME_FORMAT", "Frame Format", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    frameFormat=FORMAT_RAW12;
    bitDepth=16;
    status = SVBSetOutputImageType(cameraID, frameFormatMapping[frameFormat]);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set frame format failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    IUSaveText(&BayerT[0], "0");
    IUSaveText(&BayerT[1], "0");
    IUSaveText(&BayerT[2], bayerPatternMapping[cameraProperty.BayerPattern]);
    LOG_INFO("Camera set frame format mode\n");

    // set bit stretching and feed UI
    IUFillSwitch(&StretchS[STRETCH_OFF], "STRETCH_OFF", "Off", ISS_ON);
    IUFillSwitch(&StretchS[STRETCH_X2], "STRETCH_X2", "x2", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X4], "STRETCH_X4", "x4", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X8], "STRETCH_X8", "x8", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X16], "STRETCH_X16", "x16", ISS_OFF);
    IUFillSwitchVector(&StretchSP, StretchS, 5, getDeviceName(), "STRETCH_BITS", "12 bits 16 bits stretch", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    bitStretch=0;

    // set camera ROI and BIN
    binning = false;
    SetCCDParams(cameraProperty.MaxWidth, cameraProperty.MaxHeight, bitDepth, pixelSize, pixelSize);
    status = SVBSetROIFormat(cameraID, 0, 0, cameraProperty.MaxWidth, cameraProperty.MaxHeight, 1);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set ROI failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Camera set ROI\n");

    // set camera soft trigger mode
    status = SVBSetCameraMode(cameraID, SVB_MODE_TRIG_SOFT);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    // start framing
    status = SVBStartVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, start camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    pthread_mutex_unlock(&cameraID_mutex);

    // set CCD up
    updateCCDParams();

    // create streaming thread
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

    //pthread_mutex_lock(&cameraID_mutex);

    // stop camera
    status = SVBStopVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, stop camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // destroy camera
    status = SVBCloseCamera(cameraID);
    LOG_INFO("CCD is offline.\n");

    pthread_mutex_unlock(&cameraID_mutex);

    return true;
}


// set CCD parameters
bool Sv305CCD::updateCCDParams()
{
    // set CCD parameters
    PrimaryCCD.setBPP(bitDepth);

    // Let's calculate required buffer
    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    LOGF_INFO("PrimaryCCD buffer size : %d\n", nbuf);

    return true;
}


//
bool Sv305CCD::StartExposure(float duration)
{
    // checks for time limits
    if (duration < minExposure)
    {
        LOGF_WARN("Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.\n", duration, minExposure);
        duration = minExposure;
    }

    if (duration > maxExposure)
    {
        LOGF_WARN("Exposure greater than minimum duration %g s requested. \n Setting exposure time to %g s.\n", duration, maxExposure);
        duration = maxExposure;
    }

    pthread_mutex_lock(&cameraID_mutex);

    // set exposure time (s -> us)
    status = SVBSetControlValue(cameraID, SVB_EXPOSURE , (double)(duration * 1000000), SVB_FALSE);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set exposure failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // soft trigger
    status = SVBSendSoftTrigger(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, soft trigger failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    pthread_mutex_unlock(&cameraID_mutex);

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    gettimeofday(&ExpStart, nullptr);
    LOGF_DEBUG("Taking a %g seconds frame...\n", ExposureRequest);

    InExposure = true;

    return true;
}


//
bool Sv305CCD::AbortExposure()
{

    LOG_INFO("Abort exposure\n");

    InExposure = false;

    pthread_mutex_lock(&cameraID_mutex);

    // *********
    // TODO
    // *********

    // stop camera
    status = SVBStopVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, stop camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // start camera
    status = SVBStartVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, start camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // *********

    pthread_mutex_unlock(&cameraID_mutex);

    return true;
}


//
bool Sv305CCD::StartStreaming()
{
    LOG_INFO("framing\n");

    // stream init
    Streamer->setPixelFormat(INDI_BAYER_GRBG, bitDepth);
    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    // streaming exposure time
    ExposureRequest = 1.0 / Streamer->getTargetFPS();

    pthread_mutex_lock(&cameraID_mutex);

    // set exposure time (s -> us)
    status = SVBSetControlValue(cameraID, SVB_EXPOSURE , (double)(ExposureRequest * 1000000), SVB_FALSE);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set exposure failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // set camera normal mode
    status = SVBSetCameraMode(cameraID, SVB_MODE_NORMAL);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    pthread_mutex_unlock(&cameraID_mutex);

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
    LOG_INFO("stop framing\n");

    pthread_mutex_lock(&cameraID_mutex);

    // set camera back to trigger mode
    status = SVBSetCameraMode(cameraID, SVB_MODE_TRIG_SOFT);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    pthread_mutex_unlock(&cameraID_mutex);

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
            // ???
            ExposureRequest = 1.0 / Streamer->getTargetFPS();
        }

        if (terminateThread)
            break;

        pthread_mutex_unlock(&condMutex);

        unsigned char* imageBuffer = PrimaryCCD.getFrameBuffer();

        pthread_mutex_lock(&cameraID_mutex);

        // get the frame
        status = SVBGetVideoData(cameraID, imageBuffer, PrimaryCCD.getFrameBufferSize(), 100000 );

        pthread_mutex_unlock(&cameraID_mutex);

        finish = std::chrono::high_resolution_clock::now();

        // stretching 12bits depth to 16bits depth
        if(bitDepth==16 && (bitStretch != 0))
        {
            u_int16_t* tmp=(u_int16_t*)imageBuffer;
            for(int i=0; i<PrimaryCCD.getFrameBufferSize()/2; i++)
            {
                tmp[i]<<=bitStretch;
            }
        }

        if(binning)
        {
            PrimaryCCD.binFrame();
        }

        uint32_t size = PrimaryCCD.getFrameBufferSize() / (PrimaryCCD.getBinX() * PrimaryCCD.getBinY());
        Streamer->newFrame(PrimaryCCD.getFrameBuffer(), size);

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
    if((x + w) > cameraProperty.MaxWidth || (y + h) > cameraProperty.MaxHeight)
    {
        LOG_ERROR("Error : Subframe out of range");
        return false;
    }

    pthread_mutex_lock(&cameraID_mutex);

    status = SVBSetROIFormat(cameraID, x, y, w, h, 1);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set subframe failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Subframe set\n");

    pthread_mutex_unlock(&cameraID_mutex);

    // update streamer
    long bin_width  = w / PrimaryCCD.getBinX();
    long bin_height = h / PrimaryCCD.getBinY();

    bin_width  = bin_width - (bin_width % 2);
    bin_height = bin_height - (bin_height % 2);

    if (Streamer->isBusy())
    {
        LOG_WARN("Cannot change binning while streaming/recording.\n");
    }
    else
    {
        Streamer->setSize(bin_width, bin_height);
    }

    LOG_INFO("Subframe changed\n");

    return INDI::CCD::UpdateCCDFrame(x, y, w, h);
}


// binning
bool Sv305CCD::UpdateCCDBin(int hor, int ver)
{
    if(hor == 1 && ver == 1)
        binning = false;
    else
        binning = true;

    // update streamer
    uint32_t bin_width  = PrimaryCCD.getSubW() / hor;
    uint32_t bin_height = PrimaryCCD.getSubH() / ver;

    if (Streamer->isBusy())
    {
        LOG_WARN("Cannot change binning while streaming/recording.\n");
    }
    else
    {
        Streamer->setSize(bin_width, bin_height);
    }

    LOG_INFO("Binning changed");

    // hardware binning not supported. Using software binning

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
                    pthread_mutex_lock(&cameraID_mutex);

                    unsigned char* imageBuffer = PrimaryCCD.getFrameBuffer();
                    status = SVBGetVideoData(cameraID, imageBuffer, PrimaryCCD.getFrameBufferSize(), 100 );
                    while(status != SVB_SUCCESS)
                    {
                        pthread_mutex_unlock(&cameraID_mutex);
                        usleep(100000);
                        pthread_mutex_lock(&cameraID_mutex);
			status = SVBGetVideoData(cameraID, imageBuffer, PrimaryCCD.getFrameBufferSize(), 100 );
                        LOG_DEBUG("Wait...");
                    }

                    pthread_mutex_unlock(&cameraID_mutex);

                    // exposing done
                    PrimaryCCD.setExposureLeft(0);
                    InExposure = false;

                    // stretching 12bits depth to 16bits depth
                    if(bitDepth==16 && (bitStretch != 0))
                    {
                        u_int16_t* tmp=(u_int16_t*)imageBuffer;
                        for(int i=0; i<PrimaryCCD.getFrameBufferSize()/2; i++)
                        {
                                tmp[i]<<=bitStretch;
                        }
                    }

                    // binning if needed
                    if(binning)
                        PrimaryCCD.binFrame();

                    // exposure done
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
        SetTimer(getCurrentPollingPeriod());
    return;
}


// helper : update camera control depending on control type
bool Sv305CCD::updateControl(int ControlType, SVB_CONTROL_TYPE SVB_Control, double values[], char *names[], int n)
{
    IUUpdateNumber(&ControlsNP[ControlType], values, names, n);

    pthread_mutex_unlock(&cameraID_mutex);

    // set control
    status = SVBSetControlValue(cameraID, SVB_Control , ControlsN[ControlType].value, SVB_FALSE);
    if(status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set control %d failed\n", ControlType);
        return false;
    }
    LOGF_INFO("Camera control %d to %.f\n", ControlType, ControlsN[ControlType].value);

    pthread_mutex_unlock(&cameraID_mutex);

    ControlsNP[ControlType].s = IPS_OK;
    IDSetNumber(&ControlsNP[ControlType], nullptr);
    return true;
}


//
bool Sv305CCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    // look for gain settings
    if (!strcmp(name, ControlsNP[CCD_GAIN_N].name))
    {
        return updateControl(CCD_GAIN_N, SVB_GAIN, values, names, n);
    }

    // look for contrast settings
    if (!strcmp(name, ControlsNP[CCD_CONTRAST_N].name))
    {
        return updateControl(CCD_CONTRAST_N, SVB_CONTRAST, values, names, n);
    }

    // look for sharpness settings
    if (!strcmp(name, ControlsNP[CCD_SHARPNESS_N].name))
    {
        return updateControl(CCD_SHARPNESS_N, SVB_SHARPNESS, values, names, n);
    }

    // look for saturation settings
    if (!strcmp(name, ControlsNP[CCD_SATURATION_N].name))
    {
        return updateControl(CCD_SATURATION_N, SVB_SATURATION, values, names, n);
    }

    // look for red WB settings
    if (!strcmp(name, ControlsNP[CCD_WBR_N].name))
    {
        return updateControl(CCD_WBR_N, SVB_WB_R, values, names, n);
    }

    // look for green WB settings
    if (!strcmp(name, ControlsNP[CCD_WBG_N].name))
    {
        return updateControl(CCD_WBG_N, SVB_WB_G, values, names, n);
    }

    // look for blue WB settings
    if (!strcmp(name, ControlsNP[CCD_WBB_N].name))
    {
        return updateControl(CCD_WBB_N, SVB_WB_B, values, names, n);
    }

    // look for gamma settings
    if (!strcmp(name, ControlsNP[CCD_GAMMA_N].name))
    {
        return updateControl(CCD_GAMMA_N, SVB_GAMMA, values, names, n);
    }

    // look for dark offset settings
    if (!strcmp(name, ControlsNP[CCD_DOFFSET_N].name))
    {
        return updateControl(CCD_DOFFSET_N, SVB_BLACK_LEVEL, values, names, n);
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}


//
bool Sv305CCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Make sure the call is for our device
    if(!strcmp(dev,getDeviceName()))
    {
        // Check if the call for BPP switch
        if (!strcmp(name, FormatSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If same state as actionName, then we do nothing
            int tmpFormat = IUFindOnSwitchIndex(&FormatSP);
            if (!strcmp(actionName, FormatS[tmpFormat].name))
            {
                LOGF_INFO("Frame format is already %s", FormatS[tmpFormat].label);
                FormatSP.s = IPS_IDLE;
                IDSetSwitch(&FormatSP, NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&FormatSP, states, names, n);
            tmpFormat = IUFindOnSwitchIndex(&FormatSP);

            pthread_mutex_lock(&cameraID_mutex);

            // set new format
            status = SVBSetOutputImageType(cameraID, frameFormatMapping[tmpFormat]);
            if(status != SVB_SUCCESS)
            {
                LOG_ERROR("Error, camera set frame format failed\n");
            }
            LOGF_INFO("Frame format is now %s", FormatS[tmpFormat].label);

            pthread_mutex_unlock(&cameraID_mutex);

            frameFormat = tmpFormat;

            // pixel depth
            switch(frameFormat)
            {
                case FORMAT_RAW8 :
                    bitDepth = 8;
                    break;
                case FORMAT_RAW12 :
                    bitDepth = 16;
                    break;
                default :
                    bitDepth = 16;
            }
            // update CCD parameters
            updateCCDParams();

            FormatSP.s = IPS_OK;
            IDSetSwitch(&FormatSP, NULL);
            return true;
        }

        // Check if the call for frame rate switch
        if (!strcmp(name, SpeedSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If same state as actionName, then we do nothing
            int tmpSpeed = IUFindOnSwitchIndex(&SpeedSP);
            if (!strcmp(actionName, SpeedS[tmpSpeed].name))
            {
                LOGF_INFO("Frame rate is already %s", SpeedS[tmpSpeed].label);
                SpeedSP.s = IPS_IDLE;
                IDSetSwitch(&SpeedSP, NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&SpeedSP, states, names, n);
            tmpSpeed = IUFindOnSwitchIndex(&SpeedSP);

            pthread_mutex_lock(&cameraID_mutex);

            // set new frame rate
            status = SVBSetControlValue(cameraID, SVB_FRAME_SPEED_MODE , tmpSpeed, SVB_FALSE);
            if(status != SVB_SUCCESS)
            {
                LOG_ERROR("Error, camera set frame rate failed\n");
            }
            LOGF_INFO("Frame rate is now %s", SpeedS[tmpSpeed].label);

            pthread_mutex_unlock(&cameraID_mutex);

            frameSpeed = tmpSpeed;

            SpeedSP.s = IPS_OK;
            IDSetSwitch(&SpeedSP, NULL);
            return true;
        }

        // Check if the 16 bist stretch factor
        if (!strcmp(name, StretchSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If same state as actionName, then we do nothing
            int tmpStretch = IUFindOnSwitchIndex(&StretchSP);
            if (!strcmp(actionName, StretchS[tmpStretch].name))
            {
                LOGF_INFO("Stretch factor is already %s", StretchS[tmpStretch].label);
                StretchSP.s = IPS_IDLE;
                IDSetSwitch(&StretchSP, NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&StretchSP, states, names, n);
            tmpStretch = IUFindOnSwitchIndex(&StretchSP);

            LOGF_INFO("Stretch factor is now %s", StretchS[tmpStretch].label);

            bitStretch = tmpStretch;

            StretchSP.s = IPS_OK;
            IDSetSwitch(&StretchSP, NULL);
            return true;
        }

    }

    // If we did not process the switch, let us pass it to the parent class to process it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}


//
bool Sv305CCD::saveConfigItems(FILE * fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);

    // Controls
    IUSaveConfigNumber(fp, &ControlsNP[CCD_GAIN_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_CONTRAST_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_SHARPNESS_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_SATURATION_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_WBR_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_WBG_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_WBB_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_GAMMA_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_DOFFSET_N]);

    // Frame format
    IUSaveConfigSwitch(fp, &FormatSP);
    IUSaveConfigSwitch(fp, &SpeedSP);

    // bit stretching
    IUSaveConfigSwitch(fp, &StretchSP);

    return true;
}


//
void Sv305CCD::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    // report controls in FITS file
    int _status = 0;
    fits_update_key_dbl(fptr, "Gain", ControlsN[CCD_GAIN_N].value, 3, "Gain", &_status);
    fits_update_key_dbl(fptr, "Contrast", ControlsN[CCD_CONTRAST_N].value, 3, "Contrast", &_status);
    fits_update_key_dbl(fptr, "Sharpness", ControlsN[CCD_SHARPNESS_N].value, 3, "Sharpness", &_status);
    fits_update_key_dbl(fptr, "Saturation", ControlsN[CCD_SATURATION_N].value, 3, "Saturation", &_status);
    fits_update_key_dbl(fptr, "Red White Balance", ControlsN[CCD_WBR_N].value, 3, "Red White Balance", &_status);
    fits_update_key_dbl(fptr, "Green White Balance", ControlsN[CCD_WBG_N].value, 3, "Green White Balance", &_status);
    fits_update_key_dbl(fptr, "Blue White Balance", ControlsN[CCD_WBB_N].value, 3, "Blue White Balance", &_status);
    fits_update_key_dbl(fptr, "Gamma", ControlsN[CCD_GAMMA_N].value, 3, "Gamma", &_status);
    fits_update_key_dbl(fptr, "Frame Speed", frameSpeed, 3, "Frame Speed", &_status);
    fits_update_key_dbl(fptr, "Dark Offset", ControlsN[CCD_DOFFSET_N].value, 3, "Dark Offset", &_status);
    fits_update_key_dbl(fptr, "16 bits stretch factor (bit shift)", bitStretch, 3, "Stretch factor", &_status);
}


//
IPState Sv305CCD::GuideNorth(uint32_t ms)
{
    pthread_mutex_lock(&cameraID_mutex);

    status = SVBPulseGuide(cameraID, SVB_GUIDE_NORTH, ms);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera guide North failed\n");
        return IPS_ALERT;
    }
    LOG_INFO("Guiding North\n");

    pthread_mutex_unlock(&cameraID_mutex);

    return IPS_OK;
}


//
IPState Sv305CCD::GuideSouth(uint32_t ms)
{
    pthread_mutex_lock(&cameraID_mutex);

    status = SVBPulseGuide(cameraID, SVB_GUIDE_SOUTH, ms);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera guide South failed\n");
        return IPS_ALERT;
    }
    LOG_INFO("Guiding South\n");

    pthread_mutex_unlock(&cameraID_mutex);

    return IPS_OK;
}


//
IPState Sv305CCD::GuideEast(uint32_t ms)
{
    pthread_mutex_lock(&cameraID_mutex);

    status = SVBPulseGuide(cameraID, SVB_GUIDE_EAST, ms);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera guide East failed\n");
        return IPS_ALERT;
    }
    LOG_INFO("Guiding East\n");

    pthread_mutex_unlock(&cameraID_mutex);
    return IPS_OK;
}


//
IPState Sv305CCD::GuideWest(uint32_t ms)
{
    pthread_mutex_lock(&cameraID_mutex);

    status = SVBPulseGuide(cameraID, SVB_GUIDE_WEST, ms);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera guide West failed\n");
        return IPS_ALERT;
    }
    LOG_INFO("Guiding North\n");

    pthread_mutex_unlock(&cameraID_mutex);
    return IPS_OK;
}
