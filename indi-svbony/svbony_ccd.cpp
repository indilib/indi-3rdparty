/*
 SVBONY CCD
 SVBONY CCD Camera driver
 Copyright (C) 2020-2021 Blaise-Florentin Collin (thx8411@yahoo.fr)

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
#include <deque>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "config.h"
#include "indidevapi.h"
#include "eventloop.h"
#include "stream/streammanager.h"

#include "libsvbony/SVBCameraSDK.h"

#include "svbony_ccd.h"

static class Loader
{
        std::deque<std::unique_ptr<SVBONYCCD>> cameras;
    public:
        Loader()
        {
            // enumerate cameras
            int cameraCount = SVBGetNumOfConnectedCameras();
            if(cameraCount < 1)
            {
                IDLog("Error, no camera found\n");
                return;
            }

            IDLog("Camera(s) found\n");

            // create SVBONYCCD object for each camera
            for(int i = 0; i < cameraCount; i++)
            {
                cameras.push_back(std::unique_ptr<SVBONYCCD>(new SVBONYCCD(i)));
            }
        }
} loader;

//////////////////////////////////////////////////
// SVBONY CLASS
//


//
SVBONYCCD::SVBONYCCD(int numCamera)
{
    num = numCamera;

    // set driver version
    setVersion(SVBONY_VERSION_MAJOR, SVBONY_VERSION_MINOR);

    // Get camera informations
    SVB_ERROR_CODE status = SVBGetCameraInfo(&cameraInfo, num);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, can't get camera's informations\n");
    }

    cameraID = cameraInfo.CameraID;

    // Set camera name
    snprintf(this->name, 32, "%s %d", cameraInfo.FriendlyName, numCamera);
    setDeviceName(this->name);
}


//
SVBONYCCD::~SVBONYCCD()
{
}


//
const char *SVBONYCCD::getDefaultName()
{
    return "SVBONY CCD";
}


//
bool SVBONYCCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    // base capabilities
    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME | CCD_CAN_BIN | CCD_HAS_STREAMING;

    SetCCDCapability(cap);

    addConfigurationControl();
    addDebugControl();
    return true;
}


//
void SVBONYCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}


//
bool SVBONYCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {

        // cooler enable
        defineProperty(&CoolerSP);
        defineProperty(&CoolerNP);

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

        // a switch for automatic correction of dynamic dead pixels
        defineProperty(&CorrectDDPSP);

        // define frame rate
        defineProperty(&SpeedSP);

        // stretch factor
        defineProperty(&StretchSP);

        timerID = SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        rmTimer(timerID);

        // delete cooler enable
        deleteProperty(CoolerSP.name);
        deleteProperty(CoolerNP.name);

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

        // a switch for automatic correction of dynamic dead pixels
        deleteProperty(CorrectDDPSP.name);

        // delete frame rate
        deleteProperty(SpeedSP.name);

        // stretch factor
        deleteProperty(StretchSP.name);
    }

    return true;
}


//
bool SVBONYCCD::Connect()
{
    // boolean init
    streaming = false;

    LOG_INFO("Attempting to find the SVBONY CCD...\n");

    // init mutex and cond
    pthread_mutex_init(&cameraID_mutex, NULL);
    pthread_mutex_init(&streaming_mutex, NULL);
    pthread_mutex_init(&condMutex, NULL);
    pthread_cond_init(&cv, NULL);

    pthread_mutex_lock(&cameraID_mutex);

    // open camera
    SVB_ERROR_CODE status = SVBOpenCamera(cameraID);
    if (status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, open camera failed.\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // Get Camera Firmware Version
    status = SVBGetCameraFirmwareVersion(cameraID, cameraFirmwareVersion);
    if (status == SVB_SUCCESS)
    {
        LOGF_INFO("Camera Firmware Version:%s", cameraFirmwareVersion);
    }
    else {
        LOG_ERROR("Error, getting Camera Firmware Version failed.");
    }
    // Get SVBONY Camera SDK Version
    SDKVersion = SVBGetSDKVersion();
    LOGF_INFO("SVBONY Camera SDK Version:%s", SDKVersion);

    // wait a bit for the camera to get ready
    usleep(0.5 * 1e6);

    // Restore default parameters of SVBONY CCD Camera
    status = SVBRestoreDefaultParam(cameraID);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, restore default parameters failed.:%d", status);
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // disable suto save param
    status = SVBSetAutoSaveParam(cameraID, SVB_FALSE);
    if (status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, disable auto save param failed.");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // get camera properties
    status = SVBGetCameraProperty(cameraID, &cameraProperty);
    if (status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, get camera property failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    if (isDebug())
    {
        // Output camera properties to log
        LOGF_DEBUG("Camera Property:\n WxH= %ldx%ld, Color:%d, BayerPattern:%d, MaxBitDepth:%d, IsTriggerCam:%d",
                   cameraProperty.MaxWidth, cameraProperty.MaxHeight,
                   cameraProperty.IsColorCam,
                   cameraProperty.BayerPattern,
                   cameraProperty.MaxBitDepth,
                   cameraProperty.IsTriggerCam);
        for (int i = 0; (i < (int)(sizeof(cameraProperty.SupportedBins) / sizeof(cameraProperty.SupportedBins[0]))) && cameraProperty.SupportedBins[i] != 0; i++)
        {
            LOGF_DEBUG(" Bin %d", cameraProperty.SupportedBins[i]);
        }
        for (int i = 0; (i < (int)(sizeof(cameraProperty.SupportedVideoFormat) / sizeof(cameraProperty.SupportedVideoFormat[0]))) && cameraProperty.SupportedVideoFormat[i] != SVB_IMG_END; i++)
        {
            LOGF_DEBUG(" Supported Video Format: %d", cameraProperty.SupportedVideoFormat[i]);
        }
    }

    // get camera properties ex
    status = SVBGetCameraPropertyEx(cameraID, &cameraPropertyEx);
    if (status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, get camera property ex failed");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // output camera properties ex to log
    LOGF_DEBUG("Camera Property Ex:\n SupportPulseGuide:%d, SupportControlTemp:%d",
               cameraPropertyEx.bSupportPulseGuide,
               cameraPropertyEx.bSupportControlTemp);

    // Set CCD Capability
    uint32_t cap = GetCCDCapability();
    if (cameraProperty.IsColorCam)
    {
        cap |= CCD_HAS_BAYER;
    }
    else
    {
        cap &= ~CCD_HAS_BAYER;
    }
    if (cameraPropertyEx.bSupportPulseGuide)
    {
        cap |= CCD_HAS_ST4_PORT;
    }
    else
    {
        cap &= ~CCD_HAS_ST4_PORT;
    }
    if (cameraPropertyEx.bSupportControlTemp)
    {
        cap |= CCD_HAS_COOLER;
    }
    else
    {
        cap &= ~CCD_HAS_COOLER;
    }
    SetCCDCapability(cap);

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
    SVBSetControlValue(cameraID, SVB_EXPOSURE, (long)(1 * 1000000L), SVB_FALSE);

    // read controls and feed UI
    for(int i = 0; i < controlsNum; i++)
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
                IUFillNumberVector(&ControlsNP[CCD_GAIN_N], &ControlsN[CCD_GAIN_N], 1, getDeviceName(), "CCD_GAIN", "Gain",
                                   MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_GAIN, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set gain failed\n");
                }
                break;

            case SVB_CONTRAST :
                // Contrast
                IUFillNumber(&ControlsN[CCD_CONTRAST_N], "CONTRAST", "Contrast", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue / 10,
                             caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_CONTRAST_N], &ControlsN[CCD_CONTRAST_N], 1, getDeviceName(), "CCD_CONTRAST", "Contrast",
                                   MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_CONTRAST, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set contrast failed\n");
                }
                break;

            case SVB_SHARPNESS :
                // Sharpness
                IUFillNumber(&ControlsN[CCD_SHARPNESS_N], "SHARPNESS", "Sharpness", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue / 10,
                             caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_SHARPNESS_N], &ControlsN[CCD_SHARPNESS_N], 1, getDeviceName(), "CCD_SHARPNESS",
                                   "Sharpness", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_SHARPNESS, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set sharpness failed\n");
                }
                break;

            case SVB_SATURATION :
                // Saturation
                IUFillNumber(&ControlsN[CCD_SATURATION_N], "SATURATION", "Saturation", "%.f", caps.MinValue, caps.MaxValue,
                             caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_SATURATION_N], &ControlsN[CCD_SATURATION_N], 1, getDeviceName(), "CCD_SATURATION",
                                   "Saturation", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_SATURATION, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set saturation failed\n");
                }
                break;

            case SVB_WB_R :
                // Red White Balance
                IUFillNumber(&ControlsN[CCD_WBR_N], "WBR", "Red White Balance", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue / 10,
                             caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_WBR_N], &ControlsN[CCD_WBR_N], 1, getDeviceName(), "CCD_WBR", "Red White Balance",
                                   MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_WB_R, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set red WB failed\n");
                }
                break;

            case SVB_WB_G :
                // Green White Balance
                IUFillNumber(&ControlsN[CCD_WBG_N], "WBG", "Green White Balance", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue / 10,
                             caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_WBG_N], &ControlsN[CCD_WBG_N], 1, getDeviceName(), "CCD_WBG", "Green White Balance",
                                   MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_WB_G, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set green WB failed\n");
                }
                break;

            case SVB_WB_B :
                // Blue White Balance
                IUFillNumber(&ControlsN[CCD_WBB_N], "WBB", "Blue White Balance", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue / 10,
                             caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_WBB_N], &ControlsN[CCD_WBB_N], 1, getDeviceName(), "CCD_WBB", "Blue White Balance",
                                   MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_WB_B, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set blue WB failed\n");
                }
                break;

            case SVB_GAMMA :
                // Gamma
                IUFillNumber(&ControlsN[CCD_GAMMA_N], "GAMMA", "Gamma", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue / 10,
                             caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_GAMMA_N], &ControlsN[CCD_GAMMA_N], 1, getDeviceName(), "CCD_GAMMA", "Gamma",
                                   MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_GAMMA, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set gamma failed\n");
                }
                break;

            case SVB_BLACK_LEVEL :
                // Dark Offset
                IUFillNumber(&ControlsN[CCD_DOFFSET_N], "OFFSET", "Offset", "%.f", caps.MinValue, caps.MaxValue, caps.MaxValue / 10,
                             caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_DOFFSET_N], &ControlsN[CCD_DOFFSET_N], 1, getDeviceName(), "CCD_OFFSET", "Offset",
                                   MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(cameraID, SVB_BLACK_LEVEL, caps.DefaultValue, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOG_ERROR("Error, camera set offset failed\n");
                }
                break;

            case SVB_BAD_PIXEL_CORRECTION_ENABLE :
                // a switch for automatic correction of dynamic dead pixels
                // set the status to disable
                IUFillSwitch(&CorrectDDPS[CORRECT_DDP_ENABLE], "CORRECT_DDP_ENABLE", "ENABLE", ISS_OFF);
                IUFillSwitch(&CorrectDDPS[CORRECT_DDP_DISABLE], "CORRECT_DDP_DISABLE", "DISABLE", ISS_ON);
                IUFillSwitchVector(&CorrectDDPSP, CorrectDDPS, 2, getDeviceName(), "CORRECT_DDP", "Correct Dead pixel", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 60, IPS_IDLE);

                status = SVBSetControlValue(cameraID, SVB_BAD_PIXEL_CORRECTION_ENABLE, 0, SVB_FALSE);
                if(status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, set a switch for automatic correction of dynamic dead pixels:%d", status);
                }
                break;

            default :
                break;
        }
    }

    // set frame speed
    IUFillSwitch(&SpeedS[SPEED_SLOW], "SPEED_SLOW", "Slow", ISS_OFF);
    IUFillSwitch(&SpeedS[SPEED_NORMAL], "SPEED_NORMAL", "Normal", ISS_ON);
    IUFillSwitch(&SpeedS[SPEED_FAST], "SPEED_FAST", "Fast", ISS_OFF);
    IUFillSwitchVector(&SpeedSP, SpeedS, 3, getDeviceName(), "FRAME_RATE", "Frame rate", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY,
                       60, IPS_IDLE);
    frameSpeed = SPEED_NORMAL;
    status = SVBSetControlValue(cameraID, SVB_FRAME_SPEED_MODE, SPEED_NORMAL, SVB_FALSE);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set frame speed failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // set frame format and feed UI
    nFrameFormat = 0;
    // initialize frameFormatDefinitions from cameraProperty
    defaultMaxBitDepth = 0; // max pixel bit depth
    for (int i = 0; (i < (int)(sizeof(cameraProperty.SupportedVideoFormat) / sizeof(cameraProperty.SupportedVideoFormat[0]))) && cameraProperty.SupportedVideoFormat[i] != SVB_IMG_END; i++)
    {
        int svb_img_fmt = cameraProperty.SupportedVideoFormat[i];

        if (svb_img_fmt != SVB_IMG_RGB24 && svb_img_fmt != SVB_IMG_RGB32) // INDI not support RGB24,RGB32
        {
            frameFormatDefinitions[svb_img_fmt].isIndex = i; // Set the index of the ISwitch

            if (HasBayer() == frameFormatDefinitions[svb_img_fmt].isColor) // either HasBayer and color frame format or not HasBayer and grayscale format.
            {
                // For color CCDs, find the maximum color format bits
                // For monochrome CCDs, find the maximum bits in grayscale format.
                if (defaultMaxBitDepth < frameFormatDefinitions[svb_img_fmt].isBits)
                    defaultMaxBitDepth = frameFormatDefinitions[svb_img_fmt].isBits;
            }
            ++nFrameFormat; // count up number of ISwitch
        }
    }

    // initialize ISwitchs
    if (!(switch2frameFormatDefinitionsIndex = (SVB_IMG_TYPE*)calloc(nFrameFormat, sizeof(int))))
    {
        LOG_ERROR("Error, memory allocation for image format switches index\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    defaultFrameFormatIndex = SVB_IMG_END;
    for (int i = 0; i < (int)(sizeof(frameFormatDefinitions) / sizeof(FrameFormatDefinition)); i++)
    {
        FrameFormatDefinition *pFrameFormatDef = &frameFormatDefinitions[i];
        if (pFrameFormatDef->isIndex != -1)
        {
            if (HasBayer() == pFrameFormatDef->isColor && defaultMaxBitDepth == pFrameFormatDef->isBits)
            {
                // Switch on the maximum number of bits. For color cameras, the number of bits for color; for monochrome cameras, the number of bits for grayscale.
                pFrameFormatDef->isStateDefault = ISS_ON;
                defaultFrameFormatIndex = (SVB_IMG_TYPE)i;
            }
            switch2frameFormatDefinitionsIndex[pFrameFormatDef->isIndex] = (SVB_IMG_TYPE)i;
            // Setup Capture Format
            CaptureFormat format =
            {
                pFrameFormatDef->isName,
                pFrameFormatDef->isLabel,
                (uint8_t)(pFrameFormatDef->isBits),
                pFrameFormatDef->isStateDefault == ISS_ON ? true : false
            };
            addCaptureFormat(format);
        }
    }
    // Set ISS_ON for default switch cause addCapture cannot set ISS_ON when config.xml 'CCD_CAPTURE_FORMAT' is old format.
    if (CaptureFormatSP.findOnSwitchIndex() == -1)
    {
        FrameFormatDefinition *pFrameFormatDef = &frameFormatDefinitions[defaultFrameFormatIndex];
        CaptureFormatSP[pFrameFormatDef->isIndex].setState(ISS_ON);
        CaptureFormatSP.apply();
    }

    if(HasBayer())
    {
        IUSaveText(&BayerT[0], "0");
        IUSaveText(&BayerT[1], "0");
        IUSaveText(&BayerT[2], bayerPatternMapping[cameraProperty.BayerPattern]);
    }
    status = SVBSetOutputImageType(cameraID, defaultFrameFormatIndex);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set frame format failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    bitDepth = defaultMaxBitDepth;
    frameFormat = defaultFrameFormatIndex;
    LOG_INFO("Camera set frame format mode\n");

    // set bit stretching and feed UI
    IUFillSwitch(&StretchS[STRETCH_OFF], "STRETCH_OFF", "Off", ISS_ON);
    IUFillSwitch(&StretchS[STRETCH_X2], "STRETCH_X2", "x2", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X4], "STRETCH_X4", "x4", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X8], "STRETCH_X8", "x8", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X16], "STRETCH_X16", "x16", ISS_OFF);
    IUFillSwitchVector(&StretchSP, StretchS, 5, getDeviceName(), "STRETCH_BITS", "12 bits 16 bits stretch", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    bitStretch = 0;

    // Cooler Enable
    if (HasCooler())
    {
        // set initial target temperature
        IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 25.);

        // default target temperature is 0. Setting to 25.
        if (SVB_SUCCESS != (status = SVBSetControlValue(cameraID, SVB_TARGET_TEMPERATURE, (long)(25 * 10), SVB_FALSE)))
        {
            LOGF_INFO("Setting default target temperature failed. (SVB_TARGET_TEMPERATURE:%d)", status);
        }
        TemperatureRequest = 25;

        // set cooler status to disable
        IUFillSwitch(&CoolerS[COOLER_ENABLE], "COOLER_ON", "ON", ISS_OFF);
        IUFillSwitch(&CoolerS[COOLER_DISABLE], "COOLER_OFF", "OFF", ISS_ON);
        IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 60, IPS_IDLE);

        // cooler power info
        IUFillNumber(&CoolerN[0], "CCD_COOLER_POWER_VALUE", "Cooler power (%)", "%3.f", 0.0, 100.0, 1., 0.);
        IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooler power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    }
    coolerEnable = COOLER_DISABLE;

    // set camera ROI and BIN
    binning = false;
    status = SVBSetROIFormat(cameraID, 0, 0, cameraProperty.MaxWidth, cameraProperty.MaxHeight, 1);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set ROI failed");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    int x, y, w, h, bin;
    status = SVBGetROIFormat(cameraID, &x, &y, &w, &h, &bin); // Get Actual ROI
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera get ROI failed");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOGF_DEBUG("Actual ROI x=%d, y=%d, w=%d, h=%d, bin=%d", x, y, w, h, bin);
    SetCCDParams(w, h, bitDepth, pixelSize, pixelSize);
    x_offset = x;
    y_offset = y;
    ROI_width = w;
    ROI_height = h;
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
bool SVBONYCCD::Disconnect()
{
    // destroy streaming
    pthread_mutex_lock(&condMutex);
    streaming = true;
    terminateThread = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    //pthread_mutex_lock(&cameraID_mutex); // *1

    // stop camera
    SVB_ERROR_CODE status = SVBStopVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, stop camera failed\n");
        //pthread_mutex_unlock(&cameraID_mutex); // *1 has been comment outed, so this line comment outed too
        return false;
    }

    // destroy camera
    status = SVBCloseCamera(cameraID);
    LOG_INFO("CCD is offline.\n");

    // free map for frame format Switch to frame format definition array
    free(switch2frameFormatDefinitionsIndex);

    //pthread_mutex_unlock(&cameraID_mutex); // *1 has been comment outed, so this line comment outed too

    // destroy mutex, cond and streaming thread
    pthread_mutex_destroy(&cameraID_mutex);
    pthread_mutex_destroy(&streaming_mutex);
    pthread_mutex_destroy(&condMutex);
    pthread_cond_destroy(&cv);

    pthread_cancel(primary_thread);

    return true;
}


// set CCD parameters
bool SVBONYCCD::updateCCDParams()
{
    // set CCD parameters
    PrimaryCCD.setBPP(bitDepth);

    // Let's calculate required buffer
    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    LOGF_INFO("PrimaryCCD buffer size : %d\n", nbuf);

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Set camera temperature
///////////////////////////////////////////////////////////////////////////////////////
int SVBONYCCD::SetTemperature(double temperature)
{
    /**********************************************************
     *  We return 0 if setting the temperature will take some time
     *  If the requested is the same as current temperature, or very
     *  close, we return 1 and INDI::CCD will mark the temperature status as OK
     *  If we return 0, INDI::CCD will mark the temperature status as BUSY
     **********************************************************/
    SVB_ERROR_CODE ret;

    // If below threshold, do nothing
    if (fabs(temperature - TemperatureN[0].value) < TemperatureRampNP[RAMP_THRESHOLD].value)
    {
        return 1; // The requested temperature is the same as current temperature, or very close
    }

    pthread_mutex_lock(&cameraID_mutex);
    // Set target temperature
    if (SVB_SUCCESS != (ret = SVBSetControlValue(cameraID, SVB_TARGET_TEMPERATURE, (long)(temperature * 10), SVB_FALSE)))
    {
        LOGF_INFO("Setting target temperature failed. (SVB_TARGET_TEMPERATURE:%d)", ret);
        pthread_mutex_unlock(&cameraID_mutex);
        return -1;
    }

    pthread_mutex_unlock(&cameraID_mutex);

    // Enable Cooler
    pthread_mutex_lock(&cameraID_mutex);
    if (SVB_SUCCESS != (ret = SVBSetControlValue(cameraID, SVB_COOLER_ENABLE, 1, SVB_FALSE)))
    {
        LOGF_INFO("Enabling cooler is fail.(SVB_COOLER_ENABLE:%d)", ret);
        pthread_mutex_unlock(&cameraID_mutex);
        return -1;
    }

    pthread_mutex_unlock(&cameraID_mutex);

    CoolerS[COOLER_ENABLE].s = ISS_ON;
    CoolerS[COOLER_DISABLE].s = ISS_OFF;
    CoolerSP.s   = IPS_OK;
    IDSetSwitch(&CoolerSP, NULL);

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);

    return 0;
}

//
bool SVBONYCCD::StartExposure(float duration)
{
    // checks for time limits
    if (duration < minExposure)
    {
        LOGF_WARN("Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.\n", duration,
                  minExposure);
        duration = minExposure;
    }

    if (duration > maxExposure)
    {
        LOGF_WARN("Exposure greater than minimum duration %g s requested. \n Setting exposure time to %g s.\n", duration,
                  maxExposure);
        duration = maxExposure;
    }

    pthread_mutex_lock(&cameraID_mutex);

#ifdef WORKAROUND_latest_image_can_be_getten_next_time
    // Discard unretrieved exposure data
    discardVideoData();
#endif
    // set exposure time (s -> us)
    SVB_ERROR_CODE status = SVBSetControlValue(cameraID, SVB_EXPOSURE, (long)(duration * 1000000L), SVB_FALSE);
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

#ifdef WORKAROUND_latest_image_can_be_getten_next_time
// Discard unretrieved exposure data
void SVBONYCCD::discardVideoData()
{
    unsigned char* imageBuffer = PrimaryCCD.getFrameBuffer();
    SVB_ERROR_CODE status = SVBGetVideoData(cameraID, imageBuffer, PrimaryCCD.getFrameBufferSize(),  1000);
    LOGF_DEBUG("Discard unretrieved exposure data: SVBGetVideoData:result=%d", status);
}
#endif

//
bool SVBONYCCD::AbortExposure()
{

    LOG_INFO("Abort exposure\n");

    InExposure = false;

    pthread_mutex_lock(&cameraID_mutex);

    // *********
    // TODO
    // *********

    // stop camera
    SVB_ERROR_CODE status = SVBStopVideoCapture(cameraID);
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
bool SVBONYCCD::StartStreaming()
{
    LOG_INFO("framing\n");

    // stream init
    // Check monochrome camera or binning
    // if binning, no more bayer
    if(!HasBayer() || binning)
    {
        Streamer->setPixelFormat(INDI_MONO, bitDepth);
    }
    else
    {
        Streamer->setPixelFormat(INDI_BAYER_GRBG, bitDepth);
    }
    Streamer->setSize(PrimaryCCD.getSubW() / PrimaryCCD.getBinX(), PrimaryCCD.getSubH() / PrimaryCCD.getBinY());

    // streaming exposure time
    ExposureRequest = 1.0 / Streamer->getTargetFPS();

    pthread_mutex_lock(&cameraID_mutex);

    // stop camera
    SVB_ERROR_CODE status = SVBStopVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, stop camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // set exposure time (s -> us)
    status = SVBSetControlValue(cameraID, SVB_EXPOSURE, (long)(ExposureRequest * 1000000L), SVB_FALSE);
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
        LOG_ERROR("Error, camera normal mode failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Camera normal mode\n");

    // set ROI back
    status = SVBSetROIFormat(cameraID, x_offset, y_offset, PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), 1);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set subframe failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Subframe set\n");

    // start camera
    status = SVBStartVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, start camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    pthread_mutex_unlock(&cameraID_mutex);

    pthread_mutex_lock(&condMutex);
    streaming = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    LOG_INFO("Streaming started\n");

    return true;
}


//
bool SVBONYCCD::StopStreaming()
{
    LOG_INFO("stop framing\n");

    pthread_mutex_lock(&cameraID_mutex);

    // stop camera
    SVB_ERROR_CODE status = SVBStopVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, stop camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // set camera back to trigger mode
    status = SVBSetCameraMode(cameraID, SVB_MODE_TRIG_SOFT);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera soft trigger mode failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    // set ROI back
    status = SVBSetROIFormat(cameraID, x_offset, y_offset, PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), 1);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set subframe failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOG_INFO("Subframe set\n");

    // start camera
    status = SVBStartVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, start camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    pthread_mutex_unlock(&cameraID_mutex);

    pthread_mutex_lock(&condMutex);
    streaming = false;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    LOG_INFO("Streaming stopped\n");

    return true;
}


//
void* SVBONYCCD::streamVideoHelper(void * context)
{
    return static_cast<SVBONYCCD *>(context)->streamVideo();
}


//
void* SVBONYCCD::streamVideo()
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

        pthread_mutex_unlock(&condMutex);

        if (terminateThread)
            break;

        unsigned char* imageBuffer = PrimaryCCD.getFrameBuffer();

        pthread_mutex_lock(&cameraID_mutex);

        // get the frame
        // Note.
        // The original code continues the same process as when it succeeded, regardless of any errors in SVBGetVideoData.
        // The code that assigns the return value to "status" raises a "warning" at compile time, so it should be commented out.
        /* SVB_ERROR_CODE status = */ SVBGetVideoData(cameraID, imageBuffer, PrimaryCCD.getFrameBufferSize(), 1000 );

        pthread_mutex_unlock(&cameraID_mutex);

        finish = std::chrono::high_resolution_clock::now();

        // stretching 12bits depth to 16bits depth
        if(bitDepth == 16 && (bitStretch != 0))
        {
            u_int16_t* tmp = (u_int16_t*)imageBuffer;
            for(int i = 0; i < PrimaryCCD.getFrameBufferSize() / 2; i++)
            {
                tmp[i] <<= bitStretch;
            }
        }

        if(binning)
        {
            PrimaryCCD.binFrame();
        }

        uint32_t size = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getSubH() / PrimaryCCD.getBinY() * bitDepth / 8;
        Streamer->newFrame(PrimaryCCD.getFrameBuffer(), size);

        std::chrono::duration<double> elapsed = finish - start;
        if (elapsed.count() < ExposureRequest)
            usleep(fabs(ExposureRequest - elapsed.count()) * 1e6);

        start = std::chrono::high_resolution_clock::now();
    }

    return nullptr;
}


// subframing
bool SVBONYCCD::UpdateCCDFrame(int x, int y, int w, int h)
{

    if((x + w) > cameraProperty.MaxWidth
            || (y + h) > cameraProperty.MaxHeight
            || w % 8 != 0
            || h % 2 != 0
      )
    {
        LOG_ERROR("Error : Subframe out of range");
        return false;
    }

    pthread_mutex_lock(&cameraID_mutex);

    // stop framing
    SVB_ERROR_CODE status = SVBStopVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, stop camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    // change ROI
    status = SVBSetROIFormat(cameraID, x, y, w, h, 1);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set subframe failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOGF_DEBUG("Given ROI x=%d, y=%d, w=%d, h=%d", x, y, w, h);
    int bin;
    status = SVBGetROIFormat(cameraID, &x, &y, &w, &h, &bin);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, get actual subframe failed");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOGF_DEBUG("Actual ROI x=%d, y=%d, w=%d, h=%d, bin=%d", x, y, w, h, bin);
    LOG_INFO("Subframe set");

    // start framing
    status = SVBStartVideoCapture(cameraID);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, start camera failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }

    pthread_mutex_unlock(&cameraID_mutex);

    x_offset = x;
    y_offset = y;
    ROI_width = w;
    ROI_height = h;

    return INDI::CCD::UpdateCCDFrame(x, y, w, h);
}


// binning
bool SVBONYCCD::UpdateCCDBin(int hor, int ver)
{
    if(hor == 1 && ver == 1)
        binning = false;
    else
        binning = true;

    LOG_INFO("Binning changed");

    // hardware binning not supported. Using software binning

    return INDI::CCD::UpdateCCDBin(hor, ver);
}


//
float SVBONYCCD::CalcTimeLeft()
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
void SVBONYCCD::TimerHit()
{
    int timerID = -1;
    double timeleft;

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
                    timerID = SetTimer((uint32_t)(timeleft * 1000));
                }
                else
                {
                    LOGF_DEBUG("Current timeleft:%.2lf sec.", timeleft);

                    pthread_mutex_lock(&cameraID_mutex);
                    unsigned char* imageBuffer = PrimaryCCD.getFrameBuffer();
                    SVB_ERROR_CODE status = SVBGetVideoData(cameraID, imageBuffer, PrimaryCCD.getFrameBufferSize(),  1000);
                    pthread_mutex_unlock(&cameraID_mutex);
                    LOGF_DEBUG("SVBGetVideoData:result=%d", status);

                    switch (status)
                    {
                        case SVB_SUCCESS:
                            // exposing done
                            PrimaryCCD.setExposureLeft(0);
                            InExposure = false;

                            // stretching 12bits depth to 16bits depth
                            if(bitDepth == 16 && (bitStretch != 0))
                            {
                                u_int16_t* tmp = (u_int16_t*)imageBuffer;
                                for(int i = 0; i < PrimaryCCD.getFrameBufferSize() / 2; i++)
                                {
                                    tmp[i] <<= bitStretch;
                                }
                            }

                            // binning if needed
                            if(binning)
                                PrimaryCCD.binFrame();

                            // exposure done
                            ExposureComplete(&PrimaryCCD);
                            break;

                        case SVB_ERROR_TIMEOUT:
                            LOG_DEBUG("Timeout for image data retrieval.");
                            // set retry timer for SVGGetVideoData
                            timerID = SetTimer((uint32_t)100); // Time until next image data acquisition: 100 ms
                            break;

                        default:
                            LOGF_INFO("Error retrieval image data (status:%d)", status);
                            // Exposure be aborted. Error in SVBGetVideoData
                            PrimaryCCD.setExposureFailed(); // The exposure will be restarted after calling PrimaryCCD.setExposureFailed().
                            PrimaryCCD.setExposureLeft(0);
                            InExposure = false;
                            break;
                    }
                }
            }
        }
        else
        {
            if (isDebug())
            {
                IDLog("With time left %.2lf\n", timeleft);
                IDLog("image not yet ready....\n");
            }

            PrimaryCCD.setExposureLeft(timeleft);
        }
    }


    if (HasCooler())
    {
        SVB_ERROR_CODE ret;
        long lValue;
        SVB_BOOL bAuto;

        // temperature readout
        pthread_mutex_lock(&cameraID_mutex);
        if (SVB_SUCCESS != (ret = SVBGetControlValue(cameraID, SVB_CURRENT_TEMPERATURE, &lValue, &bAuto)))
        {
            LOGF_INFO("Error, unable to get temp due to ...(SVB_CURRENT_TEMPERATURE:%d)", ret);
            TemperatureNP.s = IPS_ALERT;
        }
        else
        {
            TemperatureN[0].value = ((double)lValue) / 10;
            IDSetNumber(&TemperatureNP, nullptr);
        }
        pthread_mutex_unlock(&cameraID_mutex);

        // read cooler power
        pthread_mutex_lock(&cameraID_mutex);
        if (SVB_SUCCESS != (ret = SVBGetControlValue(cameraID, SVB_COOLER_POWER, &lValue, &bAuto)))
        {
            LOGF_INFO("Error, unable to get cooler power due to ...(SVB_COOLER_POWER:%d)", ret);
            CoolerNP.s = IPS_ALERT;
        }
        else
        {
            CoolerN[0].value = (double)lValue;
            CoolerNP.s = IPS_OK;
            IDSetNumber(&CoolerNP, nullptr);
        }
        pthread_mutex_unlock(&cameraID_mutex);
    }

    if (timerID == -1)
        SetTimer(getCurrentPollingPeriod());
    return;
}


// helper : update camera control depending on control type
bool SVBONYCCD::updateControl(int ControlType, SVB_CONTROL_TYPE SVB_Control, double values[], char *names[], int n)
{
    IUUpdateNumber(&ControlsNP[ControlType], values, names, n);

    pthread_mutex_lock(&cameraID_mutex);

    // set control
    SVB_ERROR_CODE status = SVBSetControlValue(cameraID, SVB_Control, ControlsN[ControlType].value, SVB_FALSE);
    if(status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set control %d failed\n", ControlType);
        pthread_mutex_unlock(&cameraID_mutex);
        return false;
    }
    LOGF_INFO("Camera control %d to %.f\n", ControlType, ControlsN[ControlType].value);

    pthread_mutex_unlock(&cameraID_mutex);

    ControlsNP[ControlType].s = IPS_OK;
    IDSetNumber(&ControlsNP[ControlType], nullptr);
    return true;
}


//
bool SVBONYCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
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

    bool result = INDI::CCD::ISNewNumber(dev, name, values, names, n);

    // look for ROI settings
    if (!strcmp(name, "CCD_FRAME") && result)
    {
        // Set actural ROI size
        PrimaryCCD.setFrame(x_offset, y_offset, ROI_width, ROI_height);
    }

    return result;
}


//
bool SVBONYCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Make sure the call is for our device
    if(!strcmp(dev, getDeviceName()))
    {
        // Check is the call for capture format
        if (CaptureFormatSP.isNameMatch(name))
        {
            // search capture format in frameFormatDefinitions
            int tempFormatIndex = -1; // index of matched frameFormatDefinitions.
            for (int i = 0; i < (int)nFrameFormat; i++)
            {
                int currentIndex = (int)switch2frameFormatDefinitionsIndex[i];

                // check to match this frameFormatDefinitions.
                for (int j = 0; j < n; j++)
                {
                    if (!strcmp(names[j], frameFormatDefinitions[currentIndex].isName))
                    {
                        tempFormatIndex = currentIndex; // found it.
                        break;
                    }
                }
            }
            if (tempFormatIndex == -1) // If it is not found, abort the process.
            {
                LOGF_ERROR("Error, %s is not exist in Format switches.", names[0]);
                return false;
            }
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
            SVB_ERROR_CODE status = SVBSetControlValue(cameraID, SVB_FRAME_SPEED_MODE, tmpSpeed, SVB_FALSE);
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

        // Check if the Cooler Enable
        if (!strcmp(name, CoolerSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If same state as actionName, then we do nothing
            int tmpCoolerEnable = IUFindOnSwitchIndex(&CoolerSP);
            if (!strcmp(actionName, CoolerS[tmpCoolerEnable].name))
            {
                LOGF_INFO("Cooler Enable is already %s", CoolerS[tmpCoolerEnable].label);
                CoolerSP.s = IPS_IDLE;
                IDSetSwitch(&CoolerSP, NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&CoolerSP, states, names, n);
            tmpCoolerEnable = IUFindOnSwitchIndex(&CoolerSP);

            LOGF_INFO("Cooler Power is now %s", CoolerS[tmpCoolerEnable].label);

            coolerEnable = tmpCoolerEnable;

            SVB_ERROR_CODE ret;
            // Change cooler state
            if (SVB_SUCCESS != (ret = SVBSetControlValue(cameraID, SVB_COOLER_ENABLE, (coolerEnable == COOLER_ENABLE ? 1 : 0), SVB_FALSE)))
            {
                LOGF_INFO("Enabling cooler is fail.(SVB_COOLER_ENABLE:%d)", ret);
            }

            CoolerSP.s = IPS_OK;
            IDSetSwitch(&CoolerSP, NULL);
            return true;
        }

        // a switch for automatic correction of dynamic dead pixels
        if (!strcmp(name, CorrectDDPSP.name))
        {
            SVB_ERROR_CODE ret;
            long tmpCorrectDDPEnable = 0;
            SVB_BOOL bAuto;

            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If same state as actionName, then we do nothing
            tmpCorrectDDPEnable = IUFindOnSwitchIndex(&CorrectDDPSP);
            if (!strcmp(actionName, CorrectDDPS[tmpCorrectDDPEnable].name))
            {
                LOGF_INFO("Automatic correction of dynamic dead pixels is already %s", CorrectDDPS[tmpCorrectDDPEnable].label);
                CorrectDDPSP.s = IPS_IDLE;
                IDSetSwitch(&CorrectDDPSP, NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&CorrectDDPSP, states, names, n);
            tmpCorrectDDPEnable = IUFindOnSwitchIndex(&CorrectDDPSP);

            LOGF_INFO("Automatic correction of dynamic dead pixels %s", CorrectDDPS[tmpCorrectDDPEnable].label);

            correctDDPEnable = tmpCorrectDDPEnable;

            // Change switch for automatic correction of dynamic dead pixels
            if (SVB_SUCCESS != (ret = SVBSetControlValue(cameraID, SVB_BAD_PIXEL_CORRECTION_ENABLE, (correctDDPEnable == CORRECT_DDP_ENABLE ? 1 : 0), SVB_FALSE)))
            {
                LOGF_INFO("Setting automatic correction of dynamic dead pixels is fail.(SVB_BAD_PIXEL_CORRECTION_ENABLE:%d)", ret);
            }

            CorrectDDPSP.s = IPS_OK;
            IDSetSwitch(&CorrectDDPSP, NULL);

            // Get switch for automatic correction of dynamic dead pixels
            if (SVB_SUCCESS == (ret = SVBGetControlValue(cameraID, SVB_BAD_PIXEL_CORRECTION_ENABLE, &tmpCorrectDDPEnable, &bAuto)))
            {
                LOGF_INFO("Automatic correction of dynamic dead pixels:%ld", tmpCorrectDDPEnable);                
            } 
            else
            {
                LOGF_INFO("Getting automatic correction of dynamic dead pixels is fail.(SVB_BAD_PIXEL_CORRECTION_ENABLE:%d)", ret);
            }

            return true;
        }
    }

    // If we did not process the switch, let us pass it to the parent class to process it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool SVBONYCCD::SetCaptureFormat(uint8_t index)
{
    if (index >= nFrameFormat) // if there is no ON switch, set index of default format.
    {
        LOG_ERROR("Error, No capture format selected.");
        return false;
    }
    SVB_IMG_TYPE newFrameFormat = switch2frameFormatDefinitionsIndex[index];

    pthread_mutex_lock(&cameraID_mutex);
    SVB_ERROR_CODE status = SVBSetOutputImageType(cameraID, newFrameFormat);
    pthread_mutex_unlock(&cameraID_mutex);

    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera set frame format failed");
        return false;
    }
    LOGF_INFO("Capture format is now %s", CaptureFormatSP[index].label);

    frameFormat = newFrameFormat;

    // pixel depth
    bitDepth = frameFormatDefinitions[newFrameFormat].isBits;
    PrimaryCCD.setBPP(bitDepth);

    // Change color/grascale mode of CCD
    if (HasBayer() != frameFormatDefinitions[newFrameFormat].isColor)
    {
        // Set CCD Capability
        uint32_t cap = GetCCDCapability();
        if (HasBayer())
        {
            cap &= ~CCD_HAS_BAYER; // if color mode exchange to monochrome
        }
        else
        {
            cap |= CCD_HAS_BAYER; // if monochrome mode exchange to color
        }
        SetCCDCapability(cap);
    }
    // update CCD parameters
    updateCCDParams();

    return true;
}


//
bool SVBONYCCD::saveConfigItems(FILE * fp)
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
    IUSaveConfigSwitch(fp, &CorrectDDPSP);

    IUSaveConfigSwitch(fp, &SpeedSP);

    // bit stretching
    IUSaveConfigSwitch(fp, &StretchSP);

    return true;
}


void SVBONYCCD::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    // report controls in FITS file
    fitsKeywords.push_back({"GAIN", ControlsN[CCD_GAIN_N].value, 3, "Gain"});
    fitsKeywords.push_back({"CONTRAST", ControlsN[CCD_CONTRAST_N].value, 3, "Contrast"});
    fitsKeywords.push_back({"SHARPNESS", ControlsN[CCD_SHARPNESS_N].value, 3, "Sharpness"});

    // Add items for color camera
    if(HasBayer())
    {
        fitsKeywords.push_back({"SATURATION", ControlsN[CCD_SATURATION_N].value, 3, "Saturation"});
        fitsKeywords.push_back({"RED WHITE BALANCE", ControlsN[CCD_WBR_N].value, 3, "Red White Balance"});
        fitsKeywords.push_back({"GREEN WHITE BALANCE", ControlsN[CCD_WBG_N].value, 3, "Green White Balance"});
        fitsKeywords.push_back({"BLUE WHITE BALANCE", ControlsN[CCD_WBB_N].value, 3, "Blue White Balance"});
    }

    fitsKeywords.push_back({"GAMMA", ControlsN[CCD_GAMMA_N].value, 3, "Gamma"});
    fitsKeywords.push_back({"FRAME SPEED", frameSpeed, "Frame Speed"});
    fitsKeywords.push_back({"OFFSET", ControlsN[CCD_DOFFSET_N].value, 3, "Offset"});
    fitsKeywords.push_back({"16 BITS STRETCH FACTOR (BIT SHIFT)", bitStretch, "Stretch factor"});
}


//
IPState SVBONYCCD::GuideNorth(uint32_t ms)
{
    pthread_mutex_lock(&cameraID_mutex);

    SVB_ERROR_CODE status = SVBPulseGuide(cameraID, SVB_GUIDE_NORTH, ms);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera guide North failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return IPS_ALERT;
    }
    LOG_INFO("Guiding North\n");

    pthread_mutex_unlock(&cameraID_mutex);

    return IPS_OK;
}


//
IPState SVBONYCCD::GuideSouth(uint32_t ms)
{
    pthread_mutex_lock(&cameraID_mutex);

    SVB_ERROR_CODE status = SVBPulseGuide(cameraID, SVB_GUIDE_SOUTH, ms);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera guide South failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return IPS_ALERT;
    }
    LOG_INFO("Guiding South\n");

    pthread_mutex_unlock(&cameraID_mutex);

    return IPS_OK;
}


//
IPState SVBONYCCD::GuideEast(uint32_t ms)
{
    pthread_mutex_lock(&cameraID_mutex);

    SVB_ERROR_CODE status = SVBPulseGuide(cameraID, SVB_GUIDE_EAST, ms);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera guide East failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return IPS_ALERT;
    }
    LOG_INFO("Guiding East\n");

    pthread_mutex_unlock(&cameraID_mutex);
    return IPS_OK;
}


//
IPState SVBONYCCD::GuideWest(uint32_t ms)
{
    pthread_mutex_lock(&cameraID_mutex);

    SVB_ERROR_CODE status = SVBPulseGuide(cameraID, SVB_GUIDE_WEST, ms);
    if(status != SVB_SUCCESS)
    {
        LOG_ERROR("Error, camera guide West failed\n");
        pthread_mutex_unlock(&cameraID_mutex);
        return IPS_ALERT;
    }
    LOG_INFO("Guiding North\n");

    pthread_mutex_unlock(&cameraID_mutex);
    return IPS_OK;
}
