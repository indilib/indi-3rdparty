/*
    ASI Camera Base

    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)

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

#include "asi_base.h"
#include "asi_helpers.h"

#include "config.h"

#include <stream/streammanager.h>
#include <indielapsedtimer.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <map>
#include <unistd.h>

#define MAX_EXP_RETRIES         3
#define VERBOSE_EXPOSURE        3
#define TEMP_TIMER_MS           1000 /* Temperature polling time (ms) */
#define TEMP_THRESHOLD          .25  /* Differential temperature threshold (C)*/

#define CONTROL_TAB "Controls"

static bool warn_roi_height = true;
static bool warn_roi_width = true;

const char *ASIBase::getBayerString() const
{
    return Helpers::toString(mCameraInfo.BayerPattern);
}

void ASIBase::workerStreamVideo(const std::atomic_bool &isAboutToQuit)
{
    ASI_ERROR_CODE ret;
    double ExposureRequest = 1.0 / Streamer->getTargetFPS();
    long uSecs = static_cast<long>(ExposureRequest * 950000.0);

    ret = ASISetControlValue(mCameraInfo.CameraID, ASI_EXPOSURE, uSecs, ASI_FALSE);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));
    }

    ret = ASIStartVideoCapture(mCameraInfo.CameraID);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to start video capture (%s).", Helpers::toString(ret));
    }

    while (!isAboutToQuit)
    {
        uint8_t *targetFrame = PrimaryCCD.getFrameBuffer();
        uint32_t totalBytes  = PrimaryCCD.getFrameBufferSize();
        int waitMS           = static_cast<int>((ExposureRequest * 2000.0) + 500);

        ret = ASIGetVideoData(mCameraInfo.CameraID, targetFrame, totalBytes, waitMS);
        if (ret != ASI_SUCCESS)
        {
            if (ret != ASI_ERROR_TIMEOUT)
            {
                Streamer->setStream(false);
                LOGF_ERROR("Failed to read video data (%s).", Helpers::toString(ret));
                break;
            }

            usleep(100);
            continue;
        }

        if (mCurrentVideoFormat == ASI_IMG_RGB24)
            for (uint32_t i = 0; i < totalBytes; i += 3)
                std::swap(targetFrame[i], targetFrame[i + 2]);

        Streamer->newFrame(targetFrame, totalBytes);
    }

    ASIStopVideoCapture(mCameraInfo.CameraID);
}

void ASIBase::workerBlinkExposure(const std::atomic_bool &isAboutToQuit, int blinks, float duration)
{
    if (blinks <= 0)
        return;

    ASI_ERROR_CODE ret;
    long usecsDuration = duration * 1000 * 1000;

    LOGF_DEBUG("Blinking %ld time(s) before exposure.", blinks);

    ret = ASISetControlValue(mCameraInfo.CameraID, ASI_EXPOSURE, usecsDuration, ASI_FALSE);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set blink exposure to %ldus (%d).", usecsDuration, Helpers::toString(ret));
        return;
    }

    do
    {
        ret = ASIStartExposure(mCameraInfo.CameraID, ASI_TRUE);
        if (ret != ASI_SUCCESS)
        {
            LOGF_ERROR("Failed to start blink exposure (%s).", Helpers::toString(ret));
            break;
        }

        ASI_EXPOSURE_STATUS status = ASI_EXP_IDLE;
        do
        {
            if (isAboutToQuit)
                return;

            usleep(100 * 1000);
            ret = ASIGetExpStatus(mCameraInfo.CameraID, &status);
        }
        while (ret == ASI_SUCCESS && status == ASI_EXP_WORKING);

        if (ret != ASI_SUCCESS || status != ASI_EXP_SUCCESS)
        {
            LOGF_ERROR("Blink exposure failed, status %d (%s).", status, Helpers::toString(ret));
            break;
        }
    }
    while (--blinks > 0);

    if (blinks > 0)
    {
        LOGF_WARN("%ld blink exposure(s) NOT done.", blinks);
    }
}

void ASIBase::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    ASI_ERROR_CODE ret;

    workerBlinkExposure(
        isAboutToQuit,
        BlinkNP[BLINK_COUNT   ].getValue(),
        BlinkNP[BLINK_DURATION].getValue()
    );

    PrimaryCCD.setExposureDuration(duration);

    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);
    ret = ASISetControlValue(mCameraInfo.CameraID, ASI_EXPOSURE, duration * 1000 * 1000, ASI_FALSE);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));
    }

    // Try exposure for 3 times
    ASI_BOOL isDark = (PrimaryCCD.getFrameType() == INDI::CCDChip::DARK_FRAME) ? ASI_TRUE : ASI_FALSE;

    for (int i = 0; i < 3; i++)
    {
        ret = ASIStartExposure(mCameraInfo.CameraID, isDark);
        if (ret == ASI_SUCCESS)
            break;

        LOGF_ERROR("Failed to start exposure (%d)", Helpers::toString(ret));
        // Wait 100ms before trying again
        usleep(100 * 1000);
    }

    if (ret != ASI_SUCCESS)
    {
        LOG_WARN(
            "ASI firmware might require an update to *compatible mode."
            "Check http://www.indilib.org/devices/ccds/zwo-optics-asi-cameras.html for details."
        );
        return;
    }

    INDI::ElapsedTimer exposureTimer;

    if (duration > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", duration);

    int statRetry = 0;
    ASI_EXPOSURE_STATUS status = ASI_EXP_IDLE;
    do
    {
        float delay = 0.1;
        float timeLeft = std::max(duration - exposureTimer.elapsed() / 1000.0, 0.0);

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
        {
            delay = std::max(timeLeft - std::trunc(timeLeft), 0.005f);
            timeLeft = std::round(timeLeft);
        }

        if (timeLeft > 0)
        {
            PrimaryCCD.setExposureLeft(timeLeft);
        }

        usleep(delay * 1000 * 1000);

        ASI_ERROR_CODE ret = ASIGetExpStatus(mCameraInfo.CameraID, &status);
        // 2021-09-11 <sterne-jaeger@openfuture.de>: Fix for
        // https://www.indilib.org/forum/development/10346-asi-driver-sends-image-after-abort.html
        // Aborting an exposure also returns ASI_SUCCESS here, therefore
        // we need to ensure that the quit flag is not set if we want to continue.
        if (isAboutToQuit)
            return;

        if (ret != ASI_SUCCESS)
        {
            LOGF_DEBUG("Failed to get exposure status (%s)", Helpers::toString(ret));
            if (++statRetry < 10)
            {
                usleep(100);
                continue;
            }

            LOGF_ERROR("Exposure status timed out (%s)", Helpers::toString(ret));
            PrimaryCCD.setExposureFailed();
            return;
        }

        if (status == ASI_EXP_FAILED)
        {
            if (++mExposureRetry < MAX_EXP_RETRIES)
            {
                LOG_DEBUG("ASIGetExpStatus failed. Restarting exposure...");
                ASIStopExposure(mCameraInfo.CameraID);
                workerExposure(isAboutToQuit, duration);
                return;
            }

            LOGF_ERROR("Exposure failed after %d attempts.", mExposureRetry);
            ASIStopExposure(mCameraInfo.CameraID);
            PrimaryCCD.setExposureFailed();
            return;
        }
    }
    while (status != ASI_EXP_SUCCESS);

    // Reset exposure retry
    mExposureRetry = 0;
    PrimaryCCD.setExposureLeft(0.0);
    if (PrimaryCCD.getExposureDuration() > VERBOSE_EXPOSURE)
        LOG_INFO("Exposure done, downloading image...");

    grabImage(duration);
}

///////////////////////////////////////////////////////////////////////
/// Generic constructor
///////////////////////////////////////////////////////////////////////
ASIBase::ASIBase()
{
    setVersion(ASI_VERSION_MAJOR, ASI_VERSION_MINOR);
    mTimerWE.setSingleShot(true);
    mTimerNS.setSingleShot(true);
}

ASIBase::~ASIBase()
{
    if (isConnected())
    {
        Disconnect();
    }
}

const char *ASIBase::getDefaultName()
{
    return "ZWO CCD";
}

void ASIBase::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}

bool ASIBase::initProperties()
{
    INDI::CCD::initProperties();

    // Add Debug Control.
    addDebugControl();

    CoolerSP[0].fill("COOLER_ON",  "ON",  ISS_OFF);
    CoolerSP[1].fill("COOLER_OFF", "OFF", ISS_ON);
    CoolerSP.fill(getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    CoolerNP[0].fill("CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    CoolerNP.fill(getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ControlNP.fill(getDeviceName(), "CCD_CONTROLS",      "Controls", CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    ControlSP.fill(getDeviceName(), "CCD_CONTROLS_MODE", "Set Auto", CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    FlipSP[FLIP_HORIZONTAL].fill("FLIP_HORIZONTAL", "Horizontal", ISS_OFF);
    FlipSP[FLIP_VERTICAL].fill("FLIP_VERTICAL", "Vertical", ISS_OFF);
    FlipSP.fill(getDeviceName(), "FLIP", "Flip", CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    VideoFormatSP.fill(getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    BlinkNP[BLINK_COUNT   ].fill("BLINK_COUNT",    "Blinks before exposure", "%2.0f", 0, 100, 1.000, 0);
    BlinkNP[BLINK_DURATION].fill("BLINK_DURATION", "Blink duration",         "%2.3f", 0,  60, 0.001, 0);
    BlinkNP.fill(getDeviceName(), "BLINK", "Blink", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUSaveText(&BayerT[2], getBayerString());

    ADCDepthNP[0].fill("BITS", "Bits", "%2.0f", 0, 32, 1, mCameraInfo.BitDepth);
    ADCDepthNP.fill(getDeviceName(), "ADC_DEPTH", "ADC Depth", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    SDKVersionSP[0].fill("VERSION", "Version", ASIGetSDKVersion());
    SDKVersionSP.fill(getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 60, IPS_IDLE);

    SerialNumberTP[0].fill("SN#", "SN#", mSerialNumber);
    SerialNumberTP.fill(getDeviceName(), "Serial Number", "Serial Number", INFO_TAB, IP_RO, 60, IPS_IDLE);

    NicknameTP[0].fill("nickname", "nickname", mNickname);
    NicknameTP.fill(getDeviceName(), "NICKNAME", "Nickname", INFO_TAB, IP_RW, 60, IPS_IDLE);

    int maxBin = 1;

    for (const auto &supportedBin : mCameraInfo.SupportedBins)
    {
        if (supportedBin != 0)
            maxBin = supportedBin;
        else
            break;
    }

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0, 3600, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, maxBin, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, maxBin, 1, false);

    // Log camera capabilities.
    LOGF_DEBUG("Camera: %s", mCameraInfo.Name);
    LOGF_DEBUG("ID: %d", mCameraInfo.CameraID);
    LOGF_DEBUG("MaxWidth: %d MaxHeight: %d", mCameraInfo.MaxWidth, mCameraInfo.MaxHeight);
    LOGF_DEBUG("PixelSize: %.2f", mCameraInfo.PixelSize);
    LOGF_DEBUG("IsColorCamera: %s", mCameraInfo.IsCoolerCam ? "True" : "False");
    LOGF_DEBUG("MechanicalShutter: %s", mCameraInfo.MechanicalShutter ? "True" : "False");
    LOGF_DEBUG("ST4Port: %s", mCameraInfo.ST4Port ? "True" : "False");
    LOGF_DEBUG("IsCoolerCam: %s", mCameraInfo.IsCoolerCam ? "True" : "False");
    LOGF_DEBUG("IsUSB3Camera: %s", mCameraInfo.IsUSB3Camera ? "True" : "False");
    LOGF_DEBUG("ElecPerADU: %.2f", mCameraInfo.ElecPerADU);
    LOGF_DEBUG("BitDepth: %d", mCameraInfo.BitDepth);
    LOGF_DEBUG("IsTriggerCam: %s", mCameraInfo.IsTriggerCam ? "True" : "False");

    uint32_t cap = 0;

    if (maxBin > 1)
        cap |= CCD_CAN_BIN;

    if (mCameraInfo.IsCoolerCam)
        cap |= CCD_HAS_COOLER;

    if (mCameraInfo.MechanicalShutter)
        cap |= CCD_HAS_SHUTTER;

    if (mCameraInfo.ST4Port)
        cap |= CCD_HAS_ST4_PORT;

    if (mCameraInfo.IsColorCam)
        cap |= CCD_HAS_BAYER;

    cap |= CCD_CAN_ABORT;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_HAS_STREAMING;

#ifdef HAVE_WEBSOCKET
    cap |= CCD_HAS_WEB_SOCKET;
#endif

    SetCCDCapability(cap);

    addAuxControls();

    return true;
}

bool ASIBase::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        if (HasCooler())
        {
            defineProperty(CoolerNP);
            loadConfig(true, CoolerNP.getName());
            defineProperty(CoolerSP);
            loadConfig(true, CoolerSP.getName());
        }
        // Even if there is no cooler, we define temperature property as READ ONLY
        else
        {
            TemperatureNP.p = IP_RO;
            defineProperty(&TemperatureNP);
        }

        if (!ControlNP.isEmpty())
        {
            defineProperty(ControlNP);
            loadConfig(true, ControlNP.getName());
        }

        if (!ControlSP.isEmpty())
        {
            defineProperty(ControlSP);
            loadConfig(true, ControlSP.getName());
        }

        if (hasFlipControl())
        {
            defineProperty(FlipSP);
            loadConfig(true, FlipSP.getName());
        }

        if (!VideoFormatSP.isEmpty())
        {
            defineProperty(VideoFormatSP);

            // Try to set 16bit RAW by default.
            // It can get be overwritten by config value.
            // If config fails, we try to set 16 if exists.
            if (loadConfig(true, VideoFormatSP.getName()) == false)
            {
                for (size_t i = 0; i < VideoFormatSP.size(); i++)
                {
                    CaptureFormatSP[i].setState(ISS_OFF);
                    if (mCameraInfo.SupportedVideoFormat[i] == ASI_IMG_RAW16)
                    {
                        setVideoFormat(i);
                        CaptureFormatSP[i].setState(ISS_ON);
                        break;
                    }
                }
                CaptureFormatSP.apply();
            }
        }

        defineProperty(BlinkNP);
        defineProperty(ADCDepthNP);
        defineProperty(SDKVersionSP);
        if (!mSerialNumber.empty())
        {
            defineProperty(SerialNumberTP);
            defineProperty(NicknameTP);
        }
    }
    else
    {
        if (HasCooler())
        {
            deleteProperty(CoolerNP.getName());
            deleteProperty(CoolerSP.getName());
        }
        else
            deleteProperty(TemperatureNP.name);

        if (!ControlNP.isEmpty())
            deleteProperty(ControlNP.getName());

        if (!ControlSP.isEmpty())
            deleteProperty(ControlSP.getName());

        if (hasFlipControl())
        {
            deleteProperty(FlipSP.getName());
        }

        if (!VideoFormatSP.isEmpty())
            deleteProperty(VideoFormatSP.getName());

        deleteProperty(BlinkNP.getName());
        deleteProperty(SDKVersionSP.getName());
        if (!mSerialNumber.empty())
        {
            deleteProperty(SerialNumberTP.getName());
            deleteProperty(NicknameTP.getName());
        }
        deleteProperty(ADCDepthNP.getName());
    }

    return true;
}

bool ASIBase::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", mCameraName.c_str());

    ASI_ERROR_CODE ret = ASI_SUCCESS;

    if (isSimulation() == false)
        ret = ASIOpenCamera(mCameraInfo.CameraID);

    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Error connecting to the CCD (%s).", Helpers::toString(ret));
        return false;
    }

    if (isSimulation() == false)
        ret = ASIInitCamera(mCameraInfo.CameraID);

    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Error Initializing the CCD (%s).", Helpers::toString(ret));
        return false;
    }

    mTimerTemperature.callOnTimeout(std::bind(&ASIBase::temperatureTimerTimeout, this));
    mTimerTemperature.start(TEMP_TIMER_MS);

    LOG_INFO("Setting intital bandwidth to AUTO on connection.");
    if ((ret = ASISetControlValue(mCameraInfo.CameraID, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE)) != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set initial bandwidth (%s).", Helpers::toString(ret));
    }
    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.");

    return true;
}

bool ASIBase::Disconnect()
{
    // Save all config before shutdown
    saveConfig(true);

    LOGF_DEBUG("Closing %s...", mCameraName.c_str());

    stopGuidePulse(mTimerNS);
    stopGuidePulse(mTimerWE);
    mTimerTemperature.stop();

    mWorker.quit();
    Streamer->setStream(false);

    if (isSimulation() == false)
    {
        ASIStopVideoCapture(mCameraInfo.CameraID);
        ASIStopExposure(mCameraInfo.CameraID);
        ASICloseCamera(mCameraInfo.CameraID);
    }

    LOG_INFO("Camera is offline.");


    setConnected(false, IPS_IDLE);
    return true;
}

void ASIBase::setupParams()
{
    int piNumberOfControls = 0;
    ASI_ERROR_CODE ret;

    ret = ASIGetNumOfControls(mCameraInfo.CameraID, &piNumberOfControls);

    if (ret != ASI_SUCCESS)
        LOGF_ERROR("Failed to get number of controls (%s).", Helpers::toString(ret));

    createControls(piNumberOfControls);

    if (HasCooler())
    {
        ASI_CONTROL_CAPS pCtrlCaps;
        ret = ASIGetControlCaps(mCameraInfo.CameraID, ASI_TARGET_TEMP, &pCtrlCaps);
        if (ret == ASI_SUCCESS)
        {
            CoolerNP[0].setMinMax(pCtrlCaps.MinValue, pCtrlCaps.MaxValue);
            CoolerNP[0].setValue(pCtrlCaps.DefaultValue);
        }
    }

    // Set minimum ASI_BANDWIDTHOVERLOAD on ARM
#ifdef LOW_USB_BANDWIDTH
    for (int j = 0; j < piNumberOfControls; j++)
    {
        ASI_CONTROL_CAPS pCtrlCaps;
        ASIGetControlCaps(mCameraInfo.CameraID, j, &pCtrlCaps);
        if (pCtrlCaps.ControlType == ASI_BANDWIDTHOVERLOAD)
        {
            LOGF_DEBUG("setupParams->set USB %d", pCtrlCaps.MinValue);
            ASISetControlValue(mCameraInfo.CameraID, ASI_BANDWIDTHOVERLOAD, pCtrlCaps.MinValue, ASI_FALSE);
            break;
        }
    }
#endif

    // Get Image Format
    int w = 0, h = 0, bin = 0;
    ASI_IMG_TYPE imgType;

    ret = ASIGetROIFormat(mCameraInfo.CameraID, &w, &h, &bin, &imgType);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to get ROI format (%s).", Helpers::toString(ret));
    }

    LOGF_DEBUG("CCD ID: %d Width: %d Height: %d Binning: %dx%d Image Type: %d",
               mCameraInfo.CameraID, w, h, bin, bin, imgType);

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

    VideoFormatSP.resize(0);
    for (const auto &videoFormat : mCameraInfo.SupportedVideoFormat)
    {
        if (videoFormat == ASI_IMG_END)
            break;

        INDI::WidgetSwitch node;
        node.fill(
            Helpers::toString(videoFormat),
            Helpers::toPrettyString(videoFormat),
            videoFormat == imgType ? ISS_ON : ISS_OFF
        );

        node.setAux(const_cast<ASI_IMG_TYPE*>(&videoFormat));
        VideoFormatSP.push(std::move(node));
        CaptureFormat format = {Helpers::toString(videoFormat),
                                Helpers::toPrettyString(videoFormat),
                                static_cast<uint8_t>((videoFormat == ASI_IMG_RAW16) ? 16 : 8),
                                videoFormat == imgType
                               };
        addCaptureFormat(format);
    }

    float x_pixel_size = mCameraInfo.PixelSize;
    float y_pixel_size = mCameraInfo.PixelSize;

    uint32_t maxWidth = mCameraInfo.MaxWidth;
    uint32_t maxHeight = mCameraInfo.MaxHeight;

#if 0
    // JM 2019-04-22
    // We need to restrict width to width % 8 = 0
    // and height to height % 2 = 0;
    // Check this thread for discussion:
    // https://www.indilib.org/forum/ccds-dslrs/4956-indi-asi-driver-bug-causes-false-binned-images.html
    int maxBin = 1;
    for (int i = 0; i < 16; i++)
    {
        if (mCameraInfo.SupportedBins[i] != 0)
            maxBin = mCameraInfo.SupportedBins[i];
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

    long value      = 0;
    ASI_BOOL isAuto = ASI_FALSE;

    ret = ASIGetControlValue(mCameraInfo.CameraID, ASI_TEMPERATURE, &value, &isAuto);
    if (ret != ASI_SUCCESS)
        LOGF_DEBUG("Failed to get temperature (%s).", Helpers::toString(ret));

    TemperatureN[0].value = value / 10.0;
    IDSetNumber(&TemperatureNP, nullptr);
    LOGF_INFO("The CCD Temperature is %.3f.", TemperatureN[0].value);

    ret = ASIStopVideoCapture(mCameraInfo.CameraID);
    if (ret != ASI_SUCCESS)
        LOGF_ERROR("Failed to stop video capture (%s).", Helpers::toString(ret));

    LOGF_DEBUG("setupParams ASISetROIFormat (%dx%d,  bin %d, type %d)", maxWidth, maxHeight, 1, imgType);
    ASISetROIFormat(mCameraInfo.CameraID, maxWidth, maxHeight, 1, imgType);

    updateRecorderFormat();
    Streamer->setSize(maxWidth, maxHeight);
}

bool ASIBase::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ASI_ERROR_CODE ret = ASI_SUCCESS;

    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (ControlNP.isNameMatch(name))
        {
            std::vector<double> oldValues;
            for (const auto &num : ControlNP)
                oldValues.push_back(num.getValue());

            if (ControlNP.update(values, names, n) == false)
            {
                ControlNP.setState(IPS_ALERT);
                ControlNP.apply();
                return true;
            }

            for (size_t i = 0; i < ControlNP.size(); i++)
            {
                auto numCtrlCap = static_cast<ASI_CONTROL_CAPS *>(ControlNP[i].getAux());

                if (std::abs(ControlNP[i].getValue() - oldValues[i]) < 0.01)
                    continue;

                LOGF_DEBUG("Setting %s=%.2f...", ControlNP[i].getLabel(), ControlNP[i].getValue());
                ret = ASISetControlValue(mCameraInfo.CameraID, numCtrlCap->ControlType, static_cast<long>(ControlNP[i].getValue()),
                                         ASI_FALSE);
                if (ret != ASI_SUCCESS)
                {
                    LOGF_ERROR("Failed to set %s=%g (%s).", ControlNP[i].getName(), ControlNP[i].getValue(), Helpers::toString(ret));
                    for (size_t i = 0; i < ControlNP.size(); i++)
                        ControlNP[i].setValue(oldValues[i]);
                    ControlNP.setState(IPS_ALERT);
                    ControlNP.apply();
                    return false;
                }

                // If it was set to numCtrlCap->IsAutoSupported value to turn it off
                if (numCtrlCap->IsAutoSupported)
                {
                    auto sw = ControlSP.find_if([&numCtrlCap](const INDI::WidgetSwitch & it) -> bool
                    {
                        return static_cast<const ASI_CONTROL_CAPS *>(it.getAux())->ControlType == numCtrlCap->ControlType;
                    });

                    if (sw != ControlSP.end())
                        sw->setState(ISS_OFF);

                    ControlSP.apply();
                }
            }

            ControlNP.setState(IPS_OK);
            ControlNP.apply();
            return true;
        }

        if (BlinkNP.isNameMatch(name))
        {
            BlinkNP.setState(BlinkNP.update(values, names, n) ? IPS_OK : IPS_ALERT);
            BlinkNP.apply();
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool ASIBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (ControlSP.isNameMatch(name))
        {
            if (ControlSP.update(states, names, n) == false)
            {
                ControlSP.setState(IPS_ALERT);
                ControlSP.apply();
                return true;
            }

            for (auto &sw : ControlSP)
            {
                auto swCtrlCap  = static_cast<ASI_CONTROL_CAPS *>(sw.getAux());
                ASI_BOOL swAuto = (sw.getState() == ISS_ON) ? ASI_TRUE : ASI_FALSE;

                for (auto &num : ControlNP)
                {
                    auto numCtrlCap = static_cast<ASI_CONTROL_CAPS *>(num.aux0);

                    if (swCtrlCap->ControlType != numCtrlCap->ControlType)
                        continue;

                    LOGF_DEBUG("Setting %s=%.2f...", num.label, num.value);

                    ASI_ERROR_CODE ret = ASISetControlValue(mCameraInfo.CameraID, numCtrlCap->ControlType, num.value, swAuto);
                    if (ret != ASI_SUCCESS)
                    {
                        LOGF_ERROR("Failed to set %s=%g (%s).", num.name, num.value, Helpers::toString(ret));
                        ControlNP.setState(IPS_ALERT);
                        ControlSP.setState(IPS_ALERT);
                        ControlNP.apply();
                        ControlSP.apply();
                        return false;
                    }
                    numCtrlCap->IsAutoSupported = swAuto;
                    break;
                }
            }

            ControlSP.setState(IPS_OK);
            ControlSP.apply();
            return true;
        }

        if (FlipSP.isNameMatch(name))
        {
            if (FlipSP.update(states, names, n) == false)
            {
                FlipSP.setState(IPS_ALERT);
                FlipSP.apply();
                return true;
            }

            int flip = 0;
            if (FlipSP[FLIP_HORIZONTAL].getState() == ISS_ON)
                flip |= ASI_FLIP_HORIZ;
            if (FlipSP[FLIP_VERTICAL].getState() == ISS_ON)
                flip |= ASI_FLIP_VERT;

            ASI_ERROR_CODE ret = ASISetControlValue(mCameraInfo.CameraID, ASI_FLIP, flip, ASI_FALSE);
            if (ret != ASI_SUCCESS)
            {
                LOGF_ERROR("Failed to set ASI_FLIP=%d (%s).", flip, Helpers::toString(ret));
                FlipSP.setState(IPS_ALERT);
                FlipSP.apply();
                return false;
            }

            FlipSP.setState(IPS_OK);
            FlipSP.apply();
            return true;
        }

        /* Cooler */
        if (CoolerSP.isNameMatch(name))
        {
            if (!CoolerSP.update(states, names, n))
            {
                CoolerSP.setState(IPS_ALERT);
                CoolerSP.apply();
                return true;
            }

            activateCooler(CoolerSP[0].getState() == ISS_ON);

            return true;
        }

        if (VideoFormatSP.isNameMatch(name))
        {
            if (Streamer->isBusy())
            {
                LOG_ERROR("Cannot change format while streaming/recording.");
                VideoFormatSP.setState(IPS_ALERT);
                VideoFormatSP.apply();
                return true;
            }

            const char *targetFormat = IUFindOnSwitchName(states, names, n);
            int targetIndex = VideoFormatSP.findWidgetIndexByName(targetFormat);

            if (targetIndex == -1)
            {
                LOGF_ERROR("Unable to locate format %s.", targetFormat);
                VideoFormatSP.setState(IPS_ALERT);
                VideoFormatSP.apply();
                return true;
            }

            auto result = setVideoFormat(targetIndex);
            if (result)
            {
                VideoFormatSP.reset();
                VideoFormatSP[targetIndex].setState(ISS_ON);
                VideoFormatSP.setState(IPS_OK);
                VideoFormatSP.apply();
            }
            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool ASIBase::setVideoFormat(uint8_t index)
{
    auto currentFormat = getImageType();
    // If requested type is 16bit but we are already on 8bit and camera is 120, then ignore request
    if (currentFormat != ASI_IMG_RAW16 && index == ASI_IMG_RAW16 && (strstr(getDeviceName(), "ASI120")
            || (strstr(getDeviceName(), "ASI130"))))
    {
        VideoFormatSP.reset();
        VideoFormatSP[currentFormat].setState(ISS_ON);
        VideoFormatSP.setState(IPS_OK);
        VideoFormatSP.apply();
        return false;
    }

    if (index == VideoFormatSP.findOnSwitchIndex())
        return true;

    VideoFormatSP.reset();

    // JM 2022-11-30 Always set ASI120 to 8bit if target was 16bit since 16bit is not supported.
    if (index == ASI_IMG_RAW16 && (strstr(getDeviceName(), "ASI120") || (strstr(getDeviceName(), "ASI130"))))
        VideoFormatSP[ASI_IMG_RAW8].setState(ISS_ON);
    else
        VideoFormatSP[index].setState(ISS_ON);

    switch (getImageType())
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

    VideoFormatSP.setState(IPS_OK);
    VideoFormatSP.apply();
    return true;
}

int ASIBase::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    // #PS: how will it warm up?
    if (std::abs(temperature - mCurrentTemperature) < TEMP_THRESHOLD)
        return 1;

    if (activateCooler(true) == false)
    {
        LOG_ERROR("Failed to activate cooler.");
        return -1;
    }

    ASI_ERROR_CODE ret;

    ret = ASISetControlValue(mCameraInfo.CameraID, ASI_TARGET_TEMP, std::round(temperature), ASI_TRUE);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set temperature (%s).", Helpers::toString(ret));
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    mTargetTemperature = temperature;
    LOGF_INFO("Setting temperature to %.2f C.", temperature);
    return 0;
}

bool ASIBase::activateCooler(bool enable)
{
    ASI_ERROR_CODE ret = ASISetControlValue(mCameraInfo.CameraID, ASI_COOLER_ON, enable ? ASI_TRUE : ASI_FALSE, ASI_FALSE);
    if (ret != ASI_SUCCESS)
    {
        CoolerSP.setState(IPS_ALERT);
        LOGF_ERROR("Failed to activate cooler (%s).", Helpers::toString(ret));
    }
    else
    {
        CoolerSP[0].setState(enable ? ISS_ON  : ISS_OFF);
        CoolerSP[1].setState(enable ? ISS_OFF : ISS_ON);
        CoolerSP.setState(enable ? IPS_BUSY : IPS_IDLE);
    }
    CoolerSP.apply();

    return (ret == ASI_SUCCESS);
}

bool ASIBase::StartExposure(float duration)
{
    mExposureRetry = 0;
    mWorker.start(std::bind(&ASIBase::workerExposure, this, std::placeholders::_1, duration));
    return true;
}

bool ASIBase::AbortExposure()
{
    LOG_DEBUG("Aborting exposure...");

    mWorker.quit();

    ASIStopExposure(mCameraInfo.CameraID);
    return true;
}

bool ASIBase::StartStreaming()
{
#if 0
    ASI_IMG_TYPE type = getImageType();

    if (type != ASI_IMG_Y8 && type != ASI_IMG_RGB24)
    {
        VideoFormatSP.reset();
        auto vf = VideoFormatSP.findWidgetByName("ASI_IMG_Y8");
        if (vf == nullptr)
            vf = VideoFormatSP.findWidgetByName("ASI_IMG_RAW8");

        if (vf != nullptr)
        {
            LOGF_DEBUG("Switching to %s video format", vf->getLabel());
            PrimaryCCD.setBPP(8);
            UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
            vf->setState(ISS_ON);
            VideoFormatSP.apply();
        }
        else
        {
            LOG_ERROR("No 8 bit video format found, cannot start stream");
            return false;
        }
    }
#endif
    mWorker.start(std::bind(&ASIBase::workerStreamVideo, this, std::placeholders::_1));
    return true;
}

bool ASIBase::StopStreaming()
{
    mWorker.quit();
    return true;
}

bool ASIBase::UpdateCCDFrame(int x, int y, int w, int h)
{
    uint32_t binX = PrimaryCCD.getBinX();
    uint32_t binY = PrimaryCCD.getBinY();
    uint32_t subX = x / binX;
    uint32_t subY = y / binY;
    uint32_t subW = w / binX;
    uint32_t subH = h / binY;

    if (subW > static_cast<uint32_t>(PrimaryCCD.getXRes() / binX))
    {
        LOGF_INFO("Invalid width request %d", w);
        return false;
    }
    if (subH > static_cast<uint32_t>(PrimaryCCD.getYRes() / binY))
    {
        LOGF_INFO("Invalid height request %d", h);
        return false;
    }

    // ZWO rules are this: width%8 = 0, height%2 = 0
    // if this condition is not met, we set it internally to slightly smaller values

    if (warn_roi_width && subW % 8 > 0)
    {
        LOGF_INFO("Incompatible frame width %dpx. Reducing by %dpx.", subW, subW % 8);
        warn_roi_width = false;
    }
    if (warn_roi_height && subH % 2 > 0)
    {
        LOGF_INFO("Incompatible frame height %dpx. Reducing by %dpx.", subH, subH % 2);
        warn_roi_height = false;
    }

    subW -= subW % 8;
    subH -= subH % 2;

    LOGF_DEBUG("Frame ROI x:%d y:%d w:%d h:%d", subX, subY, subW, subH);

    ASI_ERROR_CODE ret;

    ret = ASISetROIFormat(mCameraInfo.CameraID, subW, subH, binX, getImageType());
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set ROI (%s).", Helpers::toString(ret));
        return false;
    }

    ret = ASISetStartPos(mCameraInfo.CameraID, subX, subY);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set start position (%s).", Helpers::toString(ret));
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

bool ASIBase::UpdateCCDBin(int binx, int biny)
{
    INDI_UNUSED(biny);

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int ASIBase::grabImage(float duration)
{
    ASI_ERROR_CODE ret = ASI_SUCCESS;

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
            LOGF_ERROR("%s: %d malloc failed (RGB 24).", getDeviceName());
            return -1;
        }
    }

    ret = ASIGetDataAfterExp(mCameraInfo.CameraID, buffer, nTotalBytes);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR(
            "Failed to get data after exposure (%dx%d #%d channels) (%s).",
            subW, subH, nChannels, Helpers::toString(ret)
        );
        if (type == ASI_IMG_RGB24)
            free(buffer);
        return -1;
    }

    if (type == ASI_IMG_RGB24)
    {
        uint8_t *dstR = image;
        uint8_t *dstG = image + subW * subH;
        uint8_t *dstB = image + subW * subH * 2;

        const uint8_t *src = buffer;
        const uint8_t *end = buffer + subW * subH * 3;

        while (src != end)
        {
            *dstB++ = *src++;
            *dstG++ = *src++;
            *dstR++ = *src++;
        }

        free(buffer);
    }
    guard.unlock();

    PrimaryCCD.setNAxis(type == ASI_IMG_RGB24 ? 3 : 2);

    // If mono camera or we're sending Luma or RGB, turn off bayering
    if (mCameraInfo.IsColorCam == false || type == ASI_IMG_Y8 || type == ASI_IMG_RGB24 || isMonoBinActive())
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    else
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);

    if (duration > VERBOSE_EXPOSURE)
        LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);
    return 0;
}

bool ASIBase::isMonoBinActive()
{
    long monoBin = 0;
    ASI_BOOL isAuto = ASI_FALSE;
    ASI_ERROR_CODE ret = ASIGetControlValue(mCameraInfo.CameraID, ASI_MONO_BIN, &monoBin, &isAuto);
    if (ret != ASI_SUCCESS)
    {
        if (ret != ASI_ERROR_INVALID_CONTROL_TYPE)
        {
            LOGF_ERROR("Failed to get mono bin information (%s).", Helpers::toString(ret));
        }
        return false;
    }

    if (monoBin == 0)
    {
        return false;
    }

    int width = 0, height = 0, bin = 1;
    ASI_IMG_TYPE imgType = ASI_IMG_RAW8;
    ret = ASIGetROIFormat(mCameraInfo.CameraID, &width, &height, &bin, &imgType);
    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to get ROI format (%s).", Helpers::toString(ret));
        return false;
    }

    return (imgType == ASI_IMG_RAW8 || imgType == ASI_IMG_RAW16) && bin > 1;
}

bool ASIBase::hasFlipControl()
{
    if (find_if(begin(mControlCaps), end(mControlCaps), [](ASI_CONTROL_CAPS cap)
{
    return cap.ControlType == ASI_FLIP;
}) == end(mControlCaps))
    return false;
    else
        return true;
}

/* The timer call back is used for temperature monitoring */
void ASIBase::temperatureTimerTimeout()
{
    ASI_ERROR_CODE ret;
    ASI_BOOL isAuto = ASI_FALSE;
    long value = 0;
    IPState newState = TemperatureNP.s;

    ret = ASIGetControlValue(mCameraInfo.CameraID, ASI_TEMPERATURE, &value, &isAuto);

    if (ret != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to get temperature (%s).", Helpers::toString(ret));
        newState = IPS_ALERT;
    }
    else
    {
        mCurrentTemperature = value / 10.0;
        // If cooling is active, show goal status
        //        if (CoolerSP[0].getState() == ISS_ON)
        //        {
        //            newState = std::abs(mCurrentTemperature - mTargetTemperature) <= TEMP_THRESHOLD
        //                       ? IPS_OK
        //                       : IPS_BUSY;
        //        }
    }

    // Update if there is a change
    if (
        std::abs(mCurrentTemperature - TemperatureN[0].value) > 0.05 ||
        TemperatureNP.s != newState
    )
    {
        TemperatureNP.s = newState;
        TemperatureN[0].value = mCurrentTemperature;
        IDSetNumber(&TemperatureNP, nullptr);
    }

    if (HasCooler())
    {
        ret = ASIGetControlValue(mCameraInfo.CameraID, ASI_COOLER_POWER_PERC, &value, &isAuto);
        if (ret != ASI_SUCCESS)
        {
            LOGF_ERROR("Failed to get perc power information (%s).", Helpers::toString(ret));
            CoolerNP.setState(IPS_ALERT);
        }
        else
        {
            CoolerNP[0].setValue(value);
            CoolerNP.setState(value > 0 ? IPS_BUSY : IPS_IDLE);
        }
        CoolerNP.apply();
    }
}

IPState ASIBase::guidePulse(INDI::Timer &timer, float ms, ASI_GUIDE_DIRECTION dir)
{
    timer.stop();
    ASIPulseGuideOn(mCameraInfo.CameraID, dir);

    LOGF_DEBUG("Starting %s guide for %f ms.", Helpers::toString(dir), ms);

    timer.callOnTimeout([this, dir]
    {
        ASIPulseGuideOff(mCameraInfo.CameraID, dir);
        LOGF_DEBUG("Stopped %s guide.", Helpers::toString(dir));

        if (dir == ASI_GUIDE_NORTH || dir == ASI_GUIDE_SOUTH)
            GuideComplete(AXIS_DE);
        else if (dir == ASI_GUIDE_EAST || dir == ASI_GUIDE_WEST)
            GuideComplete(AXIS_RA);
    });

    if (ms < 1)
    {
        usleep(ms * 1000);
        timer.timeout();
        return IPS_OK;
    }

    timer.start(ms);
    return IPS_BUSY;
}


void ASIBase::stopGuidePulse(INDI::Timer &timer)
{
    if (timer.isActive())
    {
        timer.stop();
        timer.timeout();
    }
}

IPState ASIBase::GuideNorth(uint32_t ms)
{
    return guidePulse(mTimerNS, ms, ASI_GUIDE_NORTH);
}

IPState ASIBase::GuideSouth(uint32_t ms)
{
    return guidePulse(mTimerNS, ms, ASI_GUIDE_SOUTH);
}

IPState ASIBase::GuideEast(uint32_t ms)
{
    return guidePulse(mTimerWE, ms, ASI_GUIDE_EAST);
}

IPState ASIBase::GuideWest(uint32_t ms)
{
    return guidePulse(mTimerWE, ms, ASI_GUIDE_WEST);
}

void ASIBase::createControls(int piNumberOfControls)
{
    ControlNP.resize(0);
    ControlSP.resize(0);

    try
    {
        mControlCaps.resize(piNumberOfControls);
        ControlNP.reserve(piNumberOfControls);
        ControlSP.reserve(piNumberOfControls);
    }
    catch(const std::bad_alloc &)
    {
        IDLog("Failed to allocate memory.");
        return;
    }

    int i = 0;
    for(auto &cap : mControlCaps)
    {
        ASI_ERROR_CODE ret = ASIGetControlCaps(mCameraInfo.CameraID, i++, &cap);
        if (ret != ASI_SUCCESS)
        {
            LOGF_ERROR("Failed to get control information (%s).", Helpers::toString(ret));
            return;
        }

        LOGF_DEBUG("Control #%d: name (%s), Descp (%s), Min (%ld), Max (%ld), Default Value (%ld), IsAutoSupported (%s), "
                   "isWritale (%s) ",
                   i, cap.Name, cap.Description, cap.MinValue, cap.MaxValue,
                   cap.DefaultValue, cap.IsAutoSupported ? "True" : "False",
                   cap.IsWritable ? "True" : "False");

        if (cap.IsWritable == ASI_FALSE || cap.ControlType == ASI_TARGET_TEMP || cap.ControlType == ASI_COOLER_ON
                || cap.ControlType == ASI_FLIP)
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
            if (mCameraInfo.IsUSB3Camera && !mCameraInfo.IsUSB3Host)
                value = 0.8 * cap.MaxValue;
#endif

            LOGF_DEBUG("createControls->set USB %d", value);
            ASISetControlValue(mCameraInfo.CameraID, cap.ControlType, value, ASI_FALSE);
        }

        long value     = 0;
        ASI_BOOL isAuto = ASI_FALSE;
        ASIGetControlValue(mCameraInfo.CameraID, cap.ControlType, &value, &isAuto);

        if (cap.IsWritable)
        {
            LOGF_DEBUG("Adding above control as writable control number %d.", ControlNP.size());

            // JM 2018-07-04: If Max-Min == 1 then it's boolean value
            // So no need to set a custom step value.
            double step = 1;
            if (cap.MaxValue - cap.MinValue > 1)
                step = (cap.MaxValue - cap.MinValue) / 10.0;

            INDI::WidgetNumber node;
            node.fill(cap.Name, cap.Name, "%g", cap.MinValue, cap.MaxValue, step, value);
            node.setAux(&cap);
            ControlNP.push(std::move(node));
        }

        if (cap.IsAutoSupported)
        {
            LOGF_DEBUG("Adding above control as auto control number %d.", ControlSP.size());

            char autoName[MAXINDINAME];
            snprintf(autoName, MAXINDINAME, "AUTO_%s", cap.Name);

            INDI::WidgetSwitch node;
            node.fill(autoName, cap.Name, isAuto == ASI_TRUE ? ISS_ON : ISS_OFF);
            node.setAux(&cap);
            ControlSP.push(std::move(node));
        }
    }

    // Resize the buffers to free up unused space
    ControlNP.shrink_to_fit();
    ControlSP.shrink_to_fit();
}

ASI_IMG_TYPE ASIBase::getImageType() const
{
    auto sp = VideoFormatSP.findOnSwitch();
    return sp != nullptr ? *static_cast<ASI_IMG_TYPE *>(sp->getAux()) : ASI_IMG_END;
}

void ASIBase::updateControls()
{
    for (auto &num : ControlNP)
    {
        auto numCtrlCap = static_cast<ASI_CONTROL_CAPS *>(num.getAux());
        long value      = 0;
        ASI_BOOL isAuto = ASI_FALSE;
        ASIGetControlValue(mCameraInfo.CameraID, numCtrlCap->ControlType, &value, &isAuto);

        num.setValue(value);

        auto sw = ControlSP.find_if([&numCtrlCap](const INDI::WidgetSwitch & it) -> bool
        {
            return static_cast<const ASI_CONTROL_CAPS *>(it.getAux())->ControlType == numCtrlCap->ControlType;
        });

        if (sw != ControlSP.end())
            sw->setState(isAuto == ASI_TRUE ? ISS_ON : ISS_OFF);
    }

    ControlNP.apply();
    ControlSP.apply();
}

void ASIBase::updateRecorderFormat()
{
    mCurrentVideoFormat = getImageType();
    if (mCurrentVideoFormat == ASI_IMG_END)
        return;

    Streamer->setPixelFormat(
        Helpers::pixelFormat(
            mCurrentVideoFormat,
            mCameraInfo.BayerPattern,
            mCameraInfo.IsColorCam
        ),
        mCurrentVideoFormat == ASI_IMG_RAW16 ? 16 : 8
    );
}

void ASIBase::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    // e-/ADU
    auto np = ControlNP.findWidgetByName("Gain");
    if (np)
    {
        fitsKeywords.push_back({"GAIN", np->value, 3, "Gain"});
    }

    np = ControlNP.findWidgetByName("Offset");
    if (np)
    {
        fitsKeywords.push_back({"OFFSET", np->value, 3, "Offset"});
    }
}

bool ASIBase::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasCooler())
        CoolerSP.save(fp);

    if (!ControlNP.isEmpty())
        ControlNP.save(fp);

    if (!ControlSP.isEmpty())
        ControlSP.save(fp);

    if (hasFlipControl())
        FlipSP.save(fp);

    if (!VideoFormatSP.isEmpty())
        VideoFormatSP.save(fp);

    BlinkNP.save(fp);

    return true;
}

bool ASIBase::SetCaptureFormat(uint8_t index)
{
    return setVideoFormat(index);
}
