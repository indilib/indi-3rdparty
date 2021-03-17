/*
 ASI CCD Driver

 Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)

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

#include "asi_ccd.h"

#include "config.h"

#include <stream/streammanager.h>

#include <algorithm>
#include <cmath>
#include <unistd.h>
#include <vector>
#include <unordered_map>

#include "indielapsedtimer.h"

#define MAX_EXP_RETRIES         3
#define VERBOSE_EXPOSURE        3
#define TEMP_TIMER_MS           1000 /* Temperature polling time (ms) */
#define TEMP_THRESHOLD          .25  /* Differential temperature threshold (C)*/
#define MAX_DEVICES             4    /* Max device cameraCount */

#define CONTROL_TAB "Controls"

//#define USE_SIMULATION

static bool warn_roi_height = true;
static bool warn_roi_width = true;

#ifdef USE_SIMULATION
static int _ASIGetNumOfConnectedCameras()
{
    return 2;
}

static ASI_ERROR_CODE _ASIGetCameraProperty(ASI_CAMERA_INFO *pASICameraInfo, int iCameraIndex)
{
    INDI_UNUSED(iCameraIndex);
    strncpy(pASICameraInfo->Name, "    SIMULATE", sizeof(pASICameraInfo->Name));
    return ASI_SUCCESS;
}
#else
# define _ASIGetNumOfConnectedCameras ASIGetNumOfConnectedCameras
# define _ASIGetCameraProperty ASIGetCameraProperty
#endif

static class Loader
{
public:
    Loader()
    {
        int iAvailableCamerasCount = _ASIGetNumOfConnectedCameras();
        if (iAvailableCamerasCount <= 0)
        {
            IDMessage(nullptr, "No ASI cameras detected. Power on?");
            IDLog("No ASI Cameras detected. Power on?");
            return;
        }

        try {
            camerasInfo.resize(iAvailableCamerasCount);
        } catch(const std::bad_alloc& e) {
            IDLog("Failed to allocate memory.");
            return;
        }

        std::unordered_map<std::string, int> cameraNamesUsed;
        int i = 0;
        for(auto &cameraInfo: camerasInfo)
        {
            _ASIGetCameraProperty(&cameraInfo, i++);
            std::string cameraName = "ZWO CCD " + std::string(cameraInfo.Name + 4);

            if (cameraNamesUsed[cameraInfo.Name]++ != 0)
                cameraName += " " + std::to_string(cameraNamesUsed[cameraInfo.Name]);

            cameras.emplace(
                std::piecewise_construct,
                std::make_tuple(cameraName),
                std::make_tuple(&cameraInfo, cameraName)
            );
        }
    }

public:
    std::unordered_map<std::string, ASICCD> cameras;
    std::vector<ASI_CAMERA_INFO> camerasInfo;
} loader;

static const char *asiGuideDirectionAsString(ASI_GUIDE_DIRECTION dir)
{
    switch (dir)
    {
    case ASI_GUIDE_NORTH: return "North";
    case ASI_GUIDE_SOUTH: return "South";
    case ASI_GUIDE_EAST:  return "East";
    case ASI_GUIDE_WEST:  return "West";
    default:              return "Unknown";
    }
}

const char *ASICCD::getBayerString()
{
    switch (m_camInfo->BayerPattern)
    {
    case ASI_BAYER_BG: return "BGGR";
    case ASI_BAYER_GR: return "GRBG";
    case ASI_BAYER_GB: return "GBRG";
    default:           return "RGGB";
    }
}

void ASICCD::workerStreamVideo(const std::atomic_bool &isAboutToQuit)
{
    double ExposureRequest = 1.0 / Streamer->getTargetFPS();
    long uSecs = static_cast<long>(ExposureRequest * 950000.0);
    ASISetControlValue(m_camInfo->CameraID, ASI_EXPOSURE, uSecs, ASI_FALSE);
    ASIStartVideoCapture(m_camInfo->CameraID);

    while (!isAboutToQuit)
    {
        uint8_t *targetFrame = PrimaryCCD.getFrameBuffer();
        uint32_t totalBytes  = PrimaryCCD.getFrameBufferSize();
        int waitMS           = static_cast<int>((ExposureRequest * 2000.0) + 500);

        int ret = ASIGetVideoData(m_camInfo->CameraID, targetFrame, totalBytes, waitMS);
        if (ret != ASI_SUCCESS)
        {
            if (ret != ASI_ERROR_TIMEOUT)
            {
                Streamer->setStream(false);
                LOGF_ERROR("Error reading video data (%d)", ret);
                break;
            }

            usleep(100);
            continue;
        }

        if (currentVideoFormat == ASI_IMG_RGB24)
            for (uint32_t i = 0; i < totalBytes; i += 3)
                std::swap(targetFrame[i], targetFrame[i + 2]);

        Streamer->newFrame(targetFrame, totalBytes);
    }

    ASIStopVideoCapture(m_camInfo->CameraID);
}

void ASICCD::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    ASI_ERROR_CODE ret;

    // JM 2020-02-17 Special hack for older ASI120 cameras that fail on 16bit
    // images.
    if (getImageType() == ASI_IMG_RAW16 && strstr(getDeviceName(), "ASI120"))
    {
        LOG_INFO("Switching to 8-bit video.");
        setVideoFormat(ASI_IMG_RAW8);
    }

    long blinks = BlinkN[BLINK_COUNT].value;
    if (blinks > 0)
    {
        LOGF_DEBUG("Blinking %ld time(s) before exposure", blinks);

        const long blink_duration = BlinkN[BLINK_DURATION].value * 1000 * 1000;
        ret = ASISetControlValue(m_camInfo->CameraID, ASI_EXPOSURE, blink_duration, ASI_FALSE);
        if (ret != ASI_SUCCESS)
        {
            LOGF_ERROR("Failed to set blink exposure to %ldus, error %d", blink_duration, ret);
        }
        else
        {
            do
            {
                ret = ASIStartExposure(m_camInfo->CameraID, ASI_TRUE);
                if (ret != ASI_SUCCESS)
                {
                    LOGF_ERROR("Failed to start blink exposure, error %d", ret);
                    break;
                }

                ASI_EXPOSURE_STATUS status = ASI_EXP_IDLE;
                do
                {
                    usleep(100 * 1000);
                    ret = ASIGetExpStatus(m_camInfo->CameraID, &status);
                }
                while (ret == ASI_SUCCESS && status == ASI_EXP_WORKING);

                if (ret != ASI_SUCCESS || status != ASI_EXP_SUCCESS)
                {
                    LOGF_ERROR("Blink exposure failed, error %d, status %d", ret, status);
                    break;
                }
            }
            while (--blinks > 0);
        }

        if (blinks > 0)
        {
            LOGF_WARN("%ld blink exposure(s) NOT done", blinks);
        }
    }

    PrimaryCCD.setExposureDuration(duration);

    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);
    ASISetControlValue(m_camInfo->CameraID, ASI_EXPOSURE, duration * 1000 * 1000, ASI_FALSE);

    // Try exposure for 3 times
    ASI_BOOL isDark = (PrimaryCCD.getFrameType() == INDI::CCDChip::DARK_FRAME) ? ASI_TRUE : ASI_FALSE;

    for (int i = 0; i < 3; i++)
    {
        ret = ASIStartExposure(m_camInfo->CameraID, isDark);
        if (ret == ASI_SUCCESS)
            break;

        LOGF_ERROR("ASIStartExposure error (%d)", ret);
        // Wait 100ms before trying again
        usleep(100 * 1000);
    }

    if (ret != ASI_SUCCESS)
    {
        LOG_WARN("ASI firmware might require an update to *compatible mode. Check http://www.indilib.org/devices/ccds/zwo-optics-asi-cameras.html for details.");
        return;
    }

    int statRetry = 0;
    ASI_EXPOSURE_STATUS status = ASI_EXP_IDLE;
    INDI::ElapsedTimer exposureTimer;

    if (duration > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", duration);

    while (!isAboutToQuit)
    {
        int ret = ASIGetExpStatus(m_camInfo->CameraID, &status);
        if (ret != ASI_SUCCESS)
        {
            LOGF_DEBUG("ASIGetExpStatus error (%d)", ret);
            if (++statRetry < 10)
            {
                usleep(100);
                continue;
            }

            LOGF_ERROR("Exposure status timed out (%d)", ret);
            PrimaryCCD.setExposureFailed();
            return;
        }

        if (status == ASI_EXP_FAILED)
        {
            if (++m_ExposureRetry < MAX_EXP_RETRIES)
            {
                LOG_DEBUG("ASIGetExpStatus failed. Restarting exposure...");
                ASIStopExposure(m_camInfo->CameraID);
                workerExposure(isAboutToQuit, duration);
                return;
            }

            LOGF_ERROR("Exposure failed after %d attempts.", m_ExposureRetry);
            m_ExposureRetry = 0;
            ASIStopExposure(m_camInfo->CameraID);
            PrimaryCCD.setExposureFailed();
            return;
        }

        if (status == ASI_EXP_SUCCESS)
        {
            m_ExposureRetry = 0;
            PrimaryCCD.setExposureLeft(0.0);
            if (PrimaryCCD.getExposureDuration() > 3)
                LOG_INFO("Exposure done, downloading image...");

            grabImage(duration);
            return;
        }

        double delay = 0.1;
        double timeLeft = std::max(duration - exposureTimer.elapsed() / 1000.0, 0.0);

        /*
         * Check the status every second until the time left is
         * about one second, after which decrease the poll interval
         *
         * For expsoures with more than a second left try
         * to keep the displayed "exposure left" value at
         * a full second boundary, which keeps the
         * count down neat
         */
        if (timeLeft > 1.1)
            delay = std::max(timeLeft - static_cast<int>(timeLeft), 0.005);

        PrimaryCCD.setExposureLeft(timeLeft);
        usleep(delay * 1000 * 1000);
    }
}

ASICCD::ASICCD(const ASI_CAMERA_INFO *camInfo, const std::string &cameraName)
    : cameraName(cameraName)
    , m_camInfo(camInfo)
{
    setVersion(ASI_VERSION_MAJOR, ASI_VERSION_MINOR);
    setDeviceName(cameraName.c_str());

    timerWE.setSingleShot(true);
    timerNS.setSingleShot(true);
}

ASICCD::~ASICCD()
{
    if (isConnected())
    {
        Disconnect();
    }
}

const char *ASICCD::getDefaultName()
{
    return "ZWO CCD";
}

bool ASICCD::initProperties()
{
    INDI::CCD::initProperties();

    IUFillSwitch(&CoolerS[0], "COOLER_ON", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    IUFillNumberVector(&ControlNP, nullptr, 0, getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    IUFillSwitchVector(&ControlSP, nullptr, 0, getDeviceName(), "CCD_CONTROLS_MODE", "Set Auto", CONTROL_TAB, IP_RW,
                       ISR_NOFMANY, 60, IPS_IDLE);

    IUFillSwitchVector(&VideoFormatSP, nullptr, 0, getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&BlinkN[BLINK_COUNT], "BLINK_COUNT", "Blinks before exposure", "%2.0f", 0, 100, 1, 0);
    IUFillNumber(&BlinkN[BLINK_DURATION], "BLINK_DURATION", "Blink duration", "%2.3f", 0, 60, 0.001, 0);
    IUFillNumberVector(&BlinkNP, BlinkN, NARRAY(BlinkN), getDeviceName(), "BLINK", "Blink", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUSaveText(&BayerT[2], getBayerString());

    IUFillNumber(&ADCDepthN, "BITS", "Bits", "%2.0f", 0, 32, 1, 0);
    ADCDepthN.value = m_camInfo->BitDepth;
    IUFillNumberVector(&ADCDepthNP, &ADCDepthN, 1, getDeviceName(), "ADC_DEPTH", "ADC Depth", IMAGE_INFO_TAB, IP_RO, 60,
                       IPS_IDLE);

    IUFillText(&SDKVersionS[0], "VERSION", "Version", ASIGetSDKVersion());
    IUFillTextVector(&SDKVersionSP, SDKVersionS, 1, getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 60, IPS_IDLE);

    int maxBin = 1;

    for (const auto &supportedBin: m_camInfo->SupportedBins)
    {
        if (supportedBin != 0)
            maxBin = supportedBin;
        else
            break;
    }

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0, 3600, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, maxBin, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, maxBin, 1, false);

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;
    if (maxBin > 1)
        cap |= CCD_CAN_BIN;

    cap |= CCD_CAN_SUBFRAME;

    if (m_camInfo->IsCoolerCam)
        cap |= CCD_HAS_COOLER;

    if (m_camInfo->MechanicalShutter)
        cap |= CCD_HAS_SHUTTER;

    if (m_camInfo->ST4Port)
        cap |= CCD_HAS_ST4_PORT;

    if (m_camInfo->IsColorCam)
        cap |= CCD_HAS_BAYER;

    cap |= CCD_HAS_STREAMING;

#ifdef HAVE_WEBSOCKET
    cap |= CCD_HAS_WEB_SOCKET;
#endif

    SetCCDCapability(cap);

    addAuxControls();

    return true;
}

bool ASICCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        if (HasCooler())
        {
            defineProperty(&CoolerNP);
            loadConfig(true, CoolerNP.name);
            defineProperty(&CoolerSP);
            loadConfig(true, CoolerSP.name);
        }
        // Even if there is no cooler, we define temperature property as READ ONLY
        else
        {
            TemperatureNP.p = IP_RO;
            defineProperty(&TemperatureNP);
        }

        if (ControlNP.nnp > 0)
        {
            defineProperty(&ControlNP);
            loadConfig(true, ControlNP.name);
        }

        if (ControlSP.nsp > 0)
        {
            defineProperty(&ControlSP);
            loadConfig(true, ControlSP.name);
        }

        if (VideoFormatSP.nsp > 0)
        {
            defineProperty(&VideoFormatSP);

            // Try to set 16bit RAW by default.
            // It can get be overwritten by config value.
            // If config fails, we try to set 16 if exists.
            if (loadConfig(true, VideoFormatSP.name) == false)
            {
                for (int i = 0; i < VideoFormatSP.nsp; i++)
                {
                    if (m_camInfo->SupportedVideoFormat[i] == ASI_IMG_RAW16)
                    {
                        setVideoFormat(i);
                        break;
                    }
                }
            }
        }

        defineProperty(&BlinkNP);

        defineProperty(&ADCDepthNP);
        defineProperty(&SDKVersionSP);
    }
    else
    {
        if (HasCooler())
        {
            deleteProperty(CoolerNP.name);
            deleteProperty(CoolerSP.name);
        }
        else
            deleteProperty(TemperatureNP.name);

        if (ControlNP.nnp > 0)
            deleteProperty(ControlNP.name);

        if (ControlSP.nsp > 0)
            deleteProperty(ControlSP.name);

        if (VideoFormatSP.nsp > 0)
            deleteProperty(VideoFormatSP.name);

        deleteProperty(BlinkNP.name);
        deleteProperty(SDKVersionSP.name);
        deleteProperty(ADCDepthNP.name);
    }

    return true;
}

bool ASICCD::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", cameraName.c_str());

    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    if (isSimulation() == false)
        errCode = ASIOpenCamera(m_camInfo->CameraID);

    if (errCode != ASI_SUCCESS)
    {
        LOGF_ERROR("Error connecting to the CCD (%d)", errCode);
        return false;
    }

    if (isSimulation() == false)
        errCode = ASIInitCamera(m_camInfo->CameraID);

    if (errCode != ASI_SUCCESS)
    {
        LOGF_ERROR("Error Initializing the CCD (%d)", errCode);
        return false;
    }

    timerTemperature.start(TEMP_TIMER_MS);
    timerTemperature.callOnTimeout(std::bind(&ASICCD::temperatureTimerTimeout, this));

    LOG_INFO("Setting intital bandwidth to AUTO on connection.");
    if ((errCode = ASISetControlValue(m_camInfo->CameraID, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE)) != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set initial bandwidth: error (%d)", errCode);
    }
    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.");

    return true;
}

bool ASICCD::Disconnect()
{
    // Save all config before shutdown
    saveConfig(true);

    LOGF_DEBUG("Closing %s...", cameraName.c_str());

    stopGuidePulse(timerNS);
    stopGuidePulse(timerWE);
    timerTemperature.stop();

    worker.quit();
    worker.wait();

    if (isSimulation() == false)
    {
        ASIStopVideoCapture(m_camInfo->CameraID);
        ASIStopExposure(m_camInfo->CameraID);
        ASICloseCamera(m_camInfo->CameraID);
    }

    LOG_INFO("Camera is offline.");

    return true;
}

void ASICCD::setupParams()
{
    int piNumberOfControls = 0;
    ASI_ERROR_CODE errCode = ASIGetNumOfControls(m_camInfo->CameraID, &piNumberOfControls);

    if (errCode != ASI_SUCCESS)
        LOGF_DEBUG("ASIGetNumOfControls error (%d)", errCode);


    createControls(piNumberOfControls);

    if (HasCooler())
    {
        ASI_CONTROL_CAPS pCtrlCaps;
        errCode = ASIGetControlCaps(m_camInfo->CameraID, ASI_TARGET_TEMP, &pCtrlCaps);
        if (errCode == ASI_SUCCESS)
        {
            CoolerN[0].min = pCtrlCaps.MinValue;
            CoolerN[0].max = pCtrlCaps.MaxValue;
            CoolerN[0].value = pCtrlCaps.DefaultValue;
        }
        //defineProperty(&CoolerNP);
        //defineProperty(&CoolerSP);
    }

    // Set minimum ASI_BANDWIDTHOVERLOAD on ARM
#ifdef LOW_USB_BANDWIDTH
    for (int j = 0; j < piNumberOfControls; j++)
    {
        ASI_CONTROL_CAPS pCtrlCaps;
        ASIGetControlCaps(m_camInfo->CameraID, j, &pCtrlCaps);
        if (pCtrlCaps.ControlType == ASI_BANDWIDTHOVERLOAD)
        {
            LOGF_DEBUG("setupParams->set USB %d", pCtrlCaps.MinValue);
            ASISetControlValue(m_camInfo->CameraID, ASI_BANDWIDTHOVERLOAD, pCtrlCaps.MinValue, ASI_FALSE);
            break;
        }
    }
#endif

    // Get Image Format
    int w = 0, h = 0, bin = 0;
    ASI_IMG_TYPE imgType;
    ASIGetROIFormat(m_camInfo->CameraID, &w, &h, &bin, &imgType);

    LOGF_DEBUG("CCD ID: %d Width: %d Height: %d Binning: %dx%d Image Type: %d",
               m_camInfo->CameraID, w, h, bin, bin, imgType);

    // Get video format and bit depth
    int bit_depth = 8;

    switch (imgType)
    {
        case ASI_IMG_RAW16:
            bit_depth = 16;
            break;

        default:
            break;
    }

    VideoFormatSP.nsp = 0;

    int nVideoFormats = 0;

    for (const auto &videoFormat : m_camInfo->SupportedVideoFormat)
    {
        if (videoFormat == ASI_IMG_END)
            break;
        nVideoFormats++;
    }

    try {
        VideoFormatS.resize(nVideoFormats);
    } catch (const std::bad_alloc &e) {
        LOGF_ERROR("Camera ID: %d malloc failed (setup)",  m_camInfo->CameraID);
        return;
    }

    ISwitch *oneVF = VideoFormatS.data();

    for (int i = 0; i < nVideoFormats; i++)
    {
        switch (m_camInfo->SupportedVideoFormat[i])
        {
            case ASI_IMG_RAW8:
                IUFillSwitch(oneVF, "ASI_IMG_RAW8", "Raw 8 bit", (imgType == ASI_IMG_RAW8) ? ISS_ON : ISS_OFF);
                LOG_DEBUG("Supported Video Format: ASI_IMG_RAW8");
                break;

            case ASI_IMG_RGB24:
                IUFillSwitch(oneVF, "ASI_IMG_RGB24", "RGB 24", (imgType == ASI_IMG_RGB24) ? ISS_ON : ISS_OFF);
                LOG_DEBUG("Supported Video Format: ASI_IMG_RGB24");
                break;

            case ASI_IMG_RAW16:
                IUFillSwitch(oneVF, "ASI_IMG_RAW16", "Raw 16 bit", (imgType == ASI_IMG_RAW16) ? ISS_ON : ISS_OFF);
                LOG_DEBUG("Supported Video Format: ASI_IMG_RAW16");
                break;

            case ASI_IMG_Y8:
                IUFillSwitch(oneVF, "ASI_IMG_Y8", "Luma", (imgType == ASI_IMG_Y8) ? ISS_ON : ISS_OFF);
                LOG_DEBUG("Supported Video Format: ASI_IMG_Y8");
                break;

            default:
                LOGF_DEBUG("Unknown video format (%d)", m_camInfo->SupportedVideoFormat[i]);
                continue;
        }

        oneVF->aux = const_cast<ASI_IMG_TYPE*>(&m_camInfo->SupportedVideoFormat[i]);
        oneVF++;
        VideoFormatSP.nsp++;
    }

    // Resize the buffers to free up unused space
    VideoFormatS.resize(VideoFormatSP.nsp);

    VideoFormatSP.sp  = VideoFormatS.data();
    rememberVideoFormat = IUFindOnSwitchIndex(&VideoFormatSP);

    float x_pixel_size, y_pixel_size;

    x_pixel_size = m_camInfo->PixelSize;
    y_pixel_size = m_camInfo->PixelSize;

    uint32_t maxWidth = m_camInfo->MaxWidth;
    uint32_t maxHeight = m_camInfo->MaxHeight;

#if 0
    // JM 2019-04-22
    // We need to restrict width to width % 8 = 0
    // and height to height % 2 = 0;
    // Check this thread for discussion:
    // https://www.indilib.org/forum/ccds-dslrs/4956-indi-asi-driver-bug-causes-false-binned-images.html
    int maxBin = 1;
    for (int i = 0; i < 16; i++)
    {
        if (m_camInfo->SupportedBins[i] != 0)
            maxBin = m_camInfo->SupportedBins[i];
        else
            break;
    }

    maxWidth  = ((maxWidth / maxBin) - ((maxWidth / maxBin) % 8)) * maxBin;
    maxHeight = ((maxHeight / maxBin) - ((maxHeight / maxBin) % 2)) * maxBin;
#endif

    SetCCDParams(maxWidth, maxHeight, bit_depth, x_pixel_size, y_pixel_size);

    // Let's calculate required buffer
    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    //if (HasCooler())
    //{
    long pValue     = 0;
    ASI_BOOL isAuto = ASI_FALSE;

    if ((errCode = ASIGetControlValue(m_camInfo->CameraID, ASI_TEMPERATURE, &pValue, &isAuto)) != ASI_SUCCESS)
        LOGF_DEBUG("ASIGetControlValue temperature error (%d)", errCode);

    TemperatureN[0].value = pValue / 10.0;

    LOGF_INFO("The CCD Temperature is %.3f", TemperatureN[0].value);
    IDSetNumber(&TemperatureNP, nullptr);
    //}

    ASIStopVideoCapture(m_camInfo->CameraID);

    LOGF_DEBUG("setupParams ASISetROIFormat (%dx%d,  bin %d, type %d)", maxWidth, maxHeight, 1, imgType);
    ASISetROIFormat(m_camInfo->CameraID, maxWidth, maxHeight, 1, imgType);

    updateRecorderFormat();
    Streamer->setSize(maxWidth, maxHeight);
}

bool ASICCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, ControlNP.name))
        {
            std::vector<double> oldValues;
            for (const auto &num : ControlN)
                oldValues.push_back(num.value);

            if (IUUpdateNumber(&ControlNP, values, names, n) < 0)
            {
                ControlNP.s = IPS_ALERT;
                IDSetNumber(&ControlNP, nullptr);
                return true;
            }

            for (int i = 0; i < ControlNP.nnp; i++)
            {
                auto numCtrlCap = static_cast<ASI_CONTROL_CAPS *>(ControlN[i].aux0);

                if (fabs(ControlN[i].value - oldValues[i]) < 0.01)
                    continue;

                LOGF_DEBUG("Setting %s --> %.2f", ControlN[i].label, ControlN[i].value);
                errCode = ASISetControlValue(m_camInfo->CameraID, numCtrlCap->ControlType, static_cast<long>(ControlN[i].value), ASI_FALSE);
                if (errCode != ASI_SUCCESS)
                {
                    LOGF_ERROR("ASISetControlValue (%s=%g) error (%d)", ControlN[i].name,ControlN[i].value, errCode);
                    ControlNP.s = IPS_ALERT;
                    for (int i = 0; i < ControlNP.nnp; i++)
                        ControlN[i].value = oldValues[i];
                    IDSetNumber(&ControlNP, nullptr);
                    return false;
                }

                // If it was set to numCtrlCap->IsAutoSupported value to turn it off
                if (numCtrlCap->IsAutoSupported)
                {
                    for (auto &sw : ControlS)
                    {
                        auto swCtrlCap = static_cast<ASI_CONTROL_CAPS *>(sw.aux);

                        if (swCtrlCap->ControlType == numCtrlCap->ControlType)
                        {
                            sw.s = ISS_OFF;
                            break;
                        }
                    }

                    IDSetSwitch(&ControlSP, nullptr);
                }
            }

            ControlNP.s = IPS_OK;
            IDSetNumber(&ControlNP, nullptr);
            return true;
        }

        if (!strcmp(name, BlinkNP.name))
        {
            BlinkNP.s = IUUpdateNumber(&BlinkNP, values, names, n) < 0 ? IPS_ALERT : IPS_OK;
            IDSetNumber(&BlinkNP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool ASICCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, ControlSP.name))
        {
            if (IUUpdateSwitch(&ControlSP, states, names, n) < 0)
            {
                ControlSP.s = IPS_ALERT;
                IDSetSwitch(&ControlSP, nullptr);
                return true;
            }

            for (auto &sw : ControlS)
            {
                auto swCtrlCap  = static_cast<ASI_CONTROL_CAPS *>(sw.aux);
                ASI_BOOL swAuto = (sw.s == ISS_ON) ? ASI_TRUE : ASI_FALSE;

                for (auto &num : ControlN)
                {
                    auto numCtrlCap = static_cast<ASI_CONTROL_CAPS *>(num.aux0);

                    if (swCtrlCap->ControlType != numCtrlCap->ControlType)
                        continue;

                    LOGF_DEBUG("Setting %s --> %.2f", num.label, num.value);

                    ASI_ERROR_CODE errCode = ASISetControlValue(m_camInfo->CameraID, numCtrlCap->ControlType, num.value, swAuto);
                    if (errCode != ASI_SUCCESS)
                    {
                        LOGF_ERROR("ASISetControlValue (%s=%g) error (%d)", num.name, num.value, errCode);
                        ControlNP.s = IPS_ALERT;
                        ControlSP.s = IPS_ALERT;
                        IDSetNumber(&ControlNP, nullptr);
                        IDSetSwitch(&ControlSP, nullptr);
                        return false;
                    }
                    numCtrlCap->IsAutoSupported = swAuto;
                    break;
                }
            }

            ControlSP.s = IPS_OK;
            IDSetSwitch(&ControlSP, nullptr);
            return true;
        }

        /* Cooler */
        if (!strcmp(name, CoolerSP.name))
        {
            if (IUUpdateSwitch(&CoolerSP, states, names, n) < 0)
            {
                CoolerSP.s = IPS_ALERT;
                IDSetSwitch(&CoolerSP, nullptr);
                return true;
            }

            activateCooler(CoolerS[0].s == ISS_ON);

            return true;
        }

        if (!strcmp(name, VideoFormatSP.name))
        {
            if (Streamer->isBusy())
            {
                VideoFormatSP.s = IPS_ALERT;
                LOG_ERROR("Cannot change format while streaming/recording.");
                IDSetSwitch(&VideoFormatSP, nullptr);
                return true;
            }

            const char *targetFormat = IUFindOnSwitchName(states, names, n);
            int targetIndex = -1;
            for (int i = 0; i < VideoFormatSP.nsp; i++)
            {
                if (!strcmp(targetFormat, VideoFormatS[i].name))
                {
                    targetIndex = i;
                    break;
                }
            }

            if (targetIndex == -1)
            {
                VideoFormatSP.s = IPS_ALERT;
                LOGF_ERROR("Unable to locate format %s.", targetFormat);
                IDSetSwitch(&VideoFormatSP, nullptr);
                return true;
            }

            return setVideoFormat(targetIndex);
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool ASICCD::setVideoFormat(uint8_t index)
{
    if (index == IUFindOnSwitchIndex(&VideoFormatSP))
        return true;

    IUResetSwitch(&VideoFormatSP);
    VideoFormatS[index].s = ISS_ON;

    ASI_IMG_TYPE type = getImageType();

    switch (type)
    {
        case ASI_IMG_RAW16:
            PrimaryCCD.setBPP(16);
            break;

        default:
            PrimaryCCD.setBPP(8);
            break;
    }

    // When changing video format, reset frame
    UpdateCCDFrame(0, 0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    updateRecorderFormat();

    VideoFormatSP.s = IPS_OK;
    IDSetSwitch(&VideoFormatSP, nullptr);
    return true;
}

bool ASICCD::StartStreaming()
{
#if 0
    ASI_IMG_TYPE type = getImageType();

    rememberVideoFormat = IUFindOnSwitchIndex(&VideoFormatSP);

    if (type != ASI_IMG_Y8 && type != ASI_IMG_RGB24)
    {
        IUResetSwitch(&VideoFormatSP);
        ISwitch *vf = IUFindSwitch(&VideoFormatSP, "ASI_IMG_Y8");
        if (vf == nullptr)
            vf = IUFindSwitch(&VideoFormatSP, "ASI_IMG_RAW8");

        if (vf)
        {
            vf->s = ISS_ON;
            LOGF_DEBUG("Switching to %s video format.", vf->label);
            PrimaryCCD.setBPP(8);
            UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
            IDSetSwitch(&VideoFormatSP, nullptr);
        }
        else
        {
            LOG_ERROR("No 8 bit video format found, cannot start stream.");
            return false;
        }
    }
#endif
    worker.run(std::bind(&ASICCD::workerStreamVideo, this, std::placeholders::_1));
    return true;
}

bool ASICCD::StopStreaming()
{
    worker.quit();
    worker.wait();

    //if (IUFindOnSwitchIndex(&VideoFormatSP) != rememberVideoFormat)
    //setVideoFormat(rememberVideoFormat);

    return true;
}

int ASICCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    if (activateCooler(true) == false)
    {
        LOG_ERROR("Failed to activate cooler!");
        return -1;
    }

    long tVal;
    if (temperature > 0.5)
        tVal = static_cast<long>(temperature + 0.49);
    else if (temperature  < 0.5)
        tVal = static_cast<long>(temperature - 0.49);
    else
        tVal = 0;
    if (ASISetControlValue(m_camInfo->CameraID, ASI_TARGET_TEMP, tVal, ASI_TRUE) != ASI_SUCCESS)
    {
        LOG_ERROR("Failed to set temperature!");
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);
    return 0;
}

bool ASICCD::activateCooler(bool enable)
{
    bool rc = (ASISetControlValue(m_camInfo->CameraID, ASI_COOLER_ON, enable ? ASI_TRUE : ASI_FALSE, ASI_FALSE) ==
               ASI_SUCCESS);
    if (rc == false)
        CoolerSP.s = IPS_ALERT;
    else
    {
        CoolerS[0].s = enable ? ISS_ON : ISS_OFF;
        CoolerS[1].s = enable ? ISS_OFF : ISS_ON;
        CoolerSP.s   = enable ? IPS_BUSY : IPS_IDLE;
    }
    IDSetSwitch(&CoolerSP, nullptr);

    return rc;
}

bool ASICCD::StartExposure(float duration)
{
    worker.run(std::bind(&ASICCD::workerExposure, this, std::placeholders::_1, duration));
    //updateControls();
    return true;
}

bool ASICCD::AbortExposure()
{
    LOG_DEBUG("Aborting camera exposure...");

    worker.quit();
    worker.wait();

    ASIStopExposure(m_camInfo->CameraID);
    return true;
}

bool ASICCD::UpdateCCDFrame(int x, int y, int w, int h)
{

    uint32_t binX = PrimaryCCD.getBinX();
    uint32_t binY = PrimaryCCD.getBinY();
    uint32_t subX = x / binX;
    uint32_t subY = y / binY;
    uint32_t subW = w / binX;
    uint32_t subH = h / binY;

    if (subW > static_cast<uint32_t>(PrimaryCCD.getXRes() / binX))
    {
        LOGF_INFO("Error: invalid width requested %d", w);
        return false;
    }
    if (subH > static_cast<uint32_t>(PrimaryCCD.getYRes() / binY))
    {
        LOGF_INFO("Error: invalid height request %d", h);
        return false;
    }

    // ZWO rules are this: width%8 = 0, height%2 = 0
    // if this condition is not met, we set it internally to slightly smaller values

    if (warn_roi_width && subW % 8 > 0)
    {
        LOGF_INFO ("Incompatible frame width %dpx. Reducing by %dpx.", subW, subW % 8);
        warn_roi_width = false;
    }
    if (warn_roi_height && subH % 2 > 0)
    {
        LOGF_INFO ("Incompatible frame height %dpx. Reducing by %dpx.", subH, subH % 2);
        warn_roi_height = false;
    }

    subW -= subW % 8;
    subH -= subH % 2;

    LOGF_DEBUG("CCD Frame ROI x:%d y:%d w:%d h:%d", subX, subY, subW, subH);

    int rc = ASI_SUCCESS;

    if ((rc = ASISetROIFormat(m_camInfo->CameraID, subW, subH, binX, getImageType())) !=
            ASI_SUCCESS)
    {
        LOGF_ERROR("ASISetROIFormat error (%d)", rc);
        return false;
    }

    if ((rc = ASISetStartPos(m_camInfo->CameraID, subX, subY)) != ASI_SUCCESS)
    {
        LOGF_ERROR("ASISetStartPos error (%d)", rc);
        return false;
    }

    // Set UNBINNED coords
    //PrimaryCCD.setFrame(x, y, w, h);
    PrimaryCCD.setFrame(subX * binX, subY * binY, subW * binX, subH * binY);

    // Total bytes required for image buffer
    uint32_t nbuf = (subW * subH * static_cast<uint32_t>(PrimaryCCD.getBPP()) / 8) * ((getImageType() == ASI_IMG_RGB24) ? 3 :
                    1);

    LOGF_DEBUG("Setting frame buffer size to %d bytes.", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(subW, subH);

    return true;
}

bool ASICCD::UpdateCCDBin(int binx, int biny)
{
    INDI_UNUSED(biny);

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int ASICCD::grabImage(float duration)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    ASI_IMG_TYPE type = getImageType();

    std::unique_lock<std::mutex> guard(ccdBufferLock);
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    uint8_t *buffer = image;

    uint16_t subW = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    uint16_t subH = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();
    int nChannels = (type == ASI_IMG_RGB24) ? 3 : 1;
    size_t nTotalBytes = subW * subH * nChannels * (PrimaryCCD.getBPP() / 8);

    if (type == ASI_IMG_RGB24)
    {
        buffer = static_cast<uint8_t *>(malloc(nTotalBytes));
        if (buffer == nullptr)
        {
            LOGF_ERROR("%s: %d malloc failed (RGB 24)", getDeviceName());
            return -1;
        }
    }

    if ((errCode = ASIGetDataAfterExp(m_camInfo->CameraID, buffer, nTotalBytes)) != ASI_SUCCESS)
    {
        LOGF_ERROR("ASIGetDataAfterExp (%dx%d #%d channels) error (%d)", subW, subH, nChannels,
                   errCode);
        if (type == ASI_IMG_RGB24)
            free(buffer);
        return -1;
    }

    if (type == ASI_IMG_RGB24)
    {
        uint8_t *subR = image;
        uint8_t *subG = image + subW * subH;
        uint8_t *subB = image + subW * subH * 2;
        uint32_t nPixels = subW * subH * 3 - 3;

        for (uint32_t i = 0; i <= nPixels; i += 3)
        {
            *subB++ = buffer[i];
            *subG++ = buffer[i + 1];
            *subR++ = buffer[i + 2];
        }

        free(buffer);
    }
    guard.unlock();

    if (type == ASI_IMG_RGB24)
        PrimaryCCD.setNAxis(3);
    else
        PrimaryCCD.setNAxis(2);

    // If mono camera or we're sending Luma or RGB, turn off bayering
    if (m_camInfo->IsColorCam == false || type == ASI_IMG_Y8 || type == ASI_IMG_RGB24 || isMonoBinActive())
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    else
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);

    if (duration > VERBOSE_EXPOSURE)
        LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);
    return 0;
}

bool ASICCD::isMonoBinActive()
{
    long monoBin = 0;
    ASI_BOOL isAuto = ASI_FALSE;
    ASI_ERROR_CODE errCode = ASIGetControlValue(m_camInfo->CameraID, ASI_MONO_BIN, &monoBin, &isAuto);
    if (errCode != ASI_SUCCESS)
    {
        if (errCode != ASI_ERROR_INVALID_CONTROL_TYPE)
        {
            LOGF_ERROR("ASIGetControlValue ASI_MONO_BIN error(%d)", errCode);
        }
        return false;
    }

    if (monoBin == 0)
    {
        return false;
    }

    int width = 0, height = 0, bin = 1;
    ASI_IMG_TYPE imgType = ASI_IMG_RAW8;
    errCode = ASIGetROIFormat(m_camInfo->CameraID, &width, &height, &bin, &imgType);
    if (errCode != ASI_SUCCESS)
    {
        LOGF_ERROR("ASIGetROIFormat error(%d)", errCode);
        return false;
    }

    return (imgType == ASI_IMG_RAW8 || imgType == ASI_IMG_RAW16) && bin > 1;
}

/* The timer call back is used for temperature monitoring */
void ASICCD::temperatureTimerTimeout()
{
    long ASIControlValue = 0;
    ASI_BOOL ASIControlAuto = ASI_FALSE;
    double currentTemperature = TemperatureN[0].value;

    ASI_ERROR_CODE errCode = ASIGetControlValue(m_camInfo->CameraID,
                             ASI_TEMPERATURE, &ASIControlValue, &ASIControlAuto);
    if (errCode != ASI_SUCCESS)
    {
        LOGF_ERROR("ASIGetControlValue ASI_TEMPERATURE error (%d)", errCode);
        TemperatureNP.s = IPS_ALERT;
    }
    else
    {
        TemperatureN[0].value = ASIControlValue / 10.0;
    }

    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:
            if (fabs(currentTemperature - TemperatureN[0].value) > TEMP_THRESHOLD / 10.0)
            {
                IDSetNumber(&TemperatureNP, nullptr);
            }
            break;

        case IPS_ALERT:
            break;

        case IPS_BUSY:
            // If we're within threshold, let's make it BUSY ---> OK
            if (fabs(TemperatureRequest - TemperatureN[0].value) <= TEMP_THRESHOLD)
            {
                TemperatureNP.s = IPS_OK;
            }
            IDSetNumber(&TemperatureNP, nullptr);
            break;
    }

    if (HasCooler())
    {
        errCode = ASIGetControlValue(m_camInfo->CameraID,
                                     ASI_COOLER_POWER_PERC, &ASIControlValue, &ASIControlAuto);
        if (errCode != ASI_SUCCESS)
        {
            LOGF_ERROR("ASIGetControlValue ASI_COOLER_POWER_PERC error (%d)", errCode);
            CoolerNP.s = IPS_ALERT;
        }
        else
        {
            CoolerN[0].value = ASIControlValue;
            CoolerNP.s = ASIControlValue > 0 ? IPS_BUSY : IPS_IDLE;
        }
        IDSetNumber(&CoolerNP, nullptr);
    }
}


IPState ASICCD::guidePulse(INDI::Timer &timer, float ms, ASI_GUIDE_DIRECTION dir)
{
    timer.stop();
    ASIPulseGuideOn(m_camInfo->CameraID, dir);

    LOGF_DEBUG("Starting %s guide for %f ms", asiGuideDirectionAsString(dir), ms);

    timer.callOnTimeout([this, dir]{
        LOGF_DEBUG("Stopped %s guide.", asiGuideDirectionAsString(dir));
        ASIPulseGuideOff(m_camInfo->CameraID, dir);

        if (dir == ASI_GUIDE_NORTH || dir == ASI_GUIDE_SOUTH)
            GuideComplete(AXIS_DE);
        else
        if (dir == ASI_GUIDE_EAST || dir == ASI_GUIDE_WEST)
            GuideComplete(AXIS_RA);
    });

    if (ms < 1)
    {
        timer.timeout();
        return IPS_OK;
    }

    timer.start(ms);
    return IPS_BUSY;
}


void ASICCD::stopGuidePulse(INDI::Timer &timer)
{
    if (timer.isActive())
    {
        timer.stop();
        timer.timeout();
    }
}

IPState ASICCD::GuideNorth(uint32_t ms)
{
    return guidePulse(timerNS, ms, ASI_GUIDE_NORTH);
}

IPState ASICCD::GuideSouth(uint32_t ms)
{
    return guidePulse(timerNS, ms, ASI_GUIDE_SOUTH);
}

IPState ASICCD::GuideEast(uint32_t ms)
{
    return guidePulse(timerWE, ms, ASI_GUIDE_EAST);
}

IPState ASICCD::GuideWest(uint32_t ms)
{
    return guidePulse(timerWE, ms, ASI_GUIDE_WEST);
}

void ASICCD::createControls(int piNumberOfControls)
{
    ControlNP.nnp = 0;
    ControlSP.nsp = 0;

    try {
        m_controlCaps.resize(piNumberOfControls);
        ControlN.resize(piNumberOfControls);
        ControlS.resize(piNumberOfControls);
    } catch(const std::bad_alloc& e) {
        IDLog("Failed to allocate memory.");
        return;
    }

    INumber *control_np = ControlN.data();
    ISwitch *auto_sp    = ControlS.data();

    int i = 0;
    for(auto &cap : m_controlCaps)
    {
        ASI_ERROR_CODE errCode = ASIGetControlCaps(m_camInfo->CameraID, i++, &cap);
        if (errCode != ASI_SUCCESS)
        {
            LOGF_ERROR("ASIGetControlCaps error (%d)", errCode);
            return;
        }

        LOGF_DEBUG("Control #%d: name (%s), Descp (%s), Min (%ld), Max (%ld), Default Value (%ld), IsAutoSupported (%s), "
                   "isWritale (%s) ",
                   i, cap.Name, cap.Description, cap.MinValue, cap.MaxValue,
                   cap.DefaultValue, cap.IsAutoSupported ? "True" : "False",
                   cap.IsWritable ? "True" : "False");

        if (cap.IsWritable == ASI_FALSE || cap.ControlType == ASI_TARGET_TEMP || cap.ControlType == ASI_COOLER_ON)
            continue;

        // Update Min/Max exposure as supported by the camera
        if (cap.ControlType == ASI_EXPOSURE)
        {
            double minExp = cap.MinValue / 1000000.0;
            double maxExp = cap.MaxValue / 1000000.0;
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", minExp, maxExp, 1);
            continue;
        }

        if (cap.ControlType == ASI_BANDWIDTHOVERLOAD)
        {
            long value = cap.MinValue;

#ifndef LOW_USB_BANDWIDTH
            if (m_camInfo->IsUSB3Camera && !m_camInfo->IsUSB3Host)
                value = 0.8 * cap.MaxValue;
#endif

            LOGF_DEBUG("createControls->set USB %d", value);
            ASISetControlValue(m_camInfo->CameraID, cap.ControlType, value, ASI_FALSE);
        }

        long pValue     = 0;
        ASI_BOOL isAuto = ASI_FALSE;
        ASIGetControlValue(m_camInfo->CameraID, cap.ControlType, &pValue, &isAuto);

        if (cap.IsWritable)
        {
            LOGF_DEBUG("Adding above control as writable control number %d", ControlNP.nnp);

            // JM 2018-07-04: If Max-Min == 1 then it's boolean value
            // So no need to set a custom step value.
            double step = 1;
            if (cap.MaxValue - cap.MinValue > 1)
                step = (cap.MaxValue - cap.MinValue) / 10.0;

            IUFillNumber(control_np,
                         cap.Name,
                         cap.Name,
                         "%g",
                         cap.MinValue,
                         cap.MaxValue,
                         step,
                         pValue);

            control_np->aux0 = &cap;
            control_np++;
            ControlNP.nnp++;
        }

        if (cap.IsAutoSupported)
        {
            LOGF_DEBUG("Adding above control as auto control number %d", ControlSP.nsp);

            char autoName[MAXINDINAME];
            snprintf(autoName, MAXINDINAME, "AUTO_%s", cap.Name);
            IUFillSwitch(auto_sp, autoName, cap.Name, isAuto == ASI_TRUE ? ISS_ON : ISS_OFF);

            auto_sp->aux = &cap;
            auto_sp++;
            ControlSP.nsp++;
        }
    }

    // Resize the buffers to free up unused space
    ControlN.resize(ControlNP.nnp);
    ControlS.resize(ControlSP.nsp);

    ControlNP.np  = ControlN.data();
    ControlSP.sp  = ControlS.data();
}

ASI_IMG_TYPE ASICCD::getImageType()
{
    ASI_IMG_TYPE type = ASI_IMG_END;

    if (VideoFormatSP.nsp > 0)
    {
        ISwitch *sp = IUFindOnSwitch(&VideoFormatSP);
        if (sp)
            type = *(static_cast<ASI_IMG_TYPE *>(sp->aux));
    }

    return type;
}

void ASICCD::updateControls()
{
    long pValue     = 0;
    ASI_BOOL isAuto = ASI_FALSE;

    for (auto &num : ControlN)
    {
        auto numCtrlCap = static_cast<ASI_CONTROL_CAPS *>(num.aux0);

        ASIGetControlValue(m_camInfo->CameraID, numCtrlCap->ControlType, &pValue, &isAuto);

        num.value = pValue;

        for (auto &sw : ControlS)
        {
            auto swCtrlCap = static_cast<ASI_CONTROL_CAPS *>(sw.aux);

            if (numCtrlCap->ControlType == swCtrlCap->ControlType)
            {
                sw.s = (isAuto == ASI_TRUE) ? ISS_ON : ISS_OFF;
                break;
            }
        }
    }

    IDSetNumber(&ControlNP, nullptr);
    IDSetSwitch(&ControlSP, nullptr);
}

void ASICCD::updateRecorderFormat()
{
    currentVideoFormat = getImageType();

    switch (currentVideoFormat)
    {
        case ASI_IMG_Y8:
            Streamer->setPixelFormat(INDI_MONO);
            break;

        case ASI_IMG_RAW8:
            if (m_camInfo->BayerPattern == ASI_BAYER_RG)
                Streamer->setPixelFormat(INDI_BAYER_RGGB);
            else if (m_camInfo->BayerPattern == ASI_BAYER_BG)
                Streamer->setPixelFormat(INDI_BAYER_BGGR);
            else if (m_camInfo->BayerPattern == ASI_BAYER_GR)
                Streamer->setPixelFormat(INDI_BAYER_GRBG);
            else if (m_camInfo->BayerPattern == ASI_BAYER_GB)
                Streamer->setPixelFormat(INDI_BAYER_GBRG);
            break;

        case ASI_IMG_RAW16:
            if (m_camInfo->IsColorCam == ASI_FALSE)
                Streamer->setPixelFormat(INDI_MONO, 16);
            else if (m_camInfo->BayerPattern == ASI_BAYER_RG)
                Streamer->setPixelFormat(INDI_BAYER_RGGB, 16);
            else if (m_camInfo->BayerPattern == ASI_BAYER_BG)
                Streamer->setPixelFormat(INDI_BAYER_BGGR, 16);
            else if (m_camInfo->BayerPattern == ASI_BAYER_GR)
                Streamer->setPixelFormat(INDI_BAYER_GRBG, 16);
            else if (m_camInfo->BayerPattern == ASI_BAYER_GB)
                Streamer->setPixelFormat(INDI_BAYER_GBRG, 16);
            break;

        case ASI_IMG_RGB24:
            Streamer->setPixelFormat(INDI_RGB);
            break;

        case ASI_IMG_END:
            break;
    }
}

void ASICCD::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    // e-/ADU
    INumber *np = IUFindNumber(&ControlNP, "Gain");
    if (np)
    {
        int status = 0;
        fits_update_key_s(fptr, TDOUBLE, "Gain", &(np->value), "Gain", &status);
    }

    np = IUFindNumber(&ControlNP, "Offset");
    if (np)
    {
        int status = 0;
        fits_update_key_s(fptr, TDOUBLE, "OFFSET", &(np->value), "Offset", &status);
    }
}

bool ASICCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasCooler())
        IUSaveConfigSwitch(fp, &CoolerSP);

    if (ControlNP.nnp > 0)
        IUSaveConfigNumber(fp, &ControlNP);

    if (ControlSP.nsp > 0)
        IUSaveConfigSwitch(fp, &ControlSP);

    if (VideoFormatSP.nsp > 0)
        IUSaveConfigSwitch(fp, &VideoFormatSP);

    IUSaveConfigNumber(fp, &BlinkNP);

    return true;
}
