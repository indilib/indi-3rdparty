/*
    PlayerOne CCD Driver (Based on ASI CCD Driver)

    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)
    Copyright (C) 2021 Hiroshi Saito (hiro3110g@gmail.com)

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

#include "playerone_base.h"
#include "playerone_helpers.h"

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

const char *POABase::getBayerString() const
{
    return Helpers::toString(mCameraInfo.bayerPattern);
}

void POABase::workerStreamVideo(const std::atomic_bool &isAbortToQuit)
{
    POAErrors ret;
    double ExposureRequest = 1.0 / Streamer->getTargetFPS();
    POAConfigValue confVal;
    confVal.intValue = static_cast<long>(ExposureRequest * 950000.0);

    ret = POASetConfig(mCameraInfo.cameraID, POA_EXPOSURE, confVal, POA_FALSE);
    if (ret != POA_OK)
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));

    // start video exposure
    ret = POAStartExposure(mCameraInfo.cameraID, POA_FALSE);
    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to start video capture (%s).", Helpers::toString(ret));
    }

    uint8_t *targetFrame = PrimaryCCD.getFrameBuffer();
    uint32_t totalBytes  = PrimaryCCD.getFrameBufferSize();
    int waitMS           = static_cast<int>((ExposureRequest * 1000.0) + 500);

    while (!isAbortToQuit)
    {
        POABool pIsReady = POA_FALSE;
		while (pIsReady == POA_FALSE)
	    {
		    //if (isAbortToQuit) //Triggered by external conditions
			//    break;

			//usleep(ExposureRequest / 10);
			POAImageReady(mCameraInfo.cameraID, &pIsReady);
		}
		
        ret = POAGetImageData(mCameraInfo.cameraID, targetFrame, totalBytes, waitMS);
        if (ret != POA_OK)
        {
            if (ret != POA_ERROR_TIMEOUT)
            {
                Streamer->setStream(false);
                LOGF_ERROR("Failed to read video data (%s).", Helpers::toString(ret));
                break;
            }
            
            usleep(100);
            continue;
        }

        if (mCurrentVideoFormat == POA_RGB24)
            for (uint32_t i = 0; i < totalBytes; i += 3)
                std::swap(targetFrame[i], targetFrame[i + 2]);

        Streamer->newFrame(targetFrame, totalBytes);
    }

    // stop video capture
    POAStopExposure(mCameraInfo.cameraID);
}

void POABase::workerBlinkExposure(const std::atomic_bool &isAbortToQuit, int blinks, float duration)
{
    if (blinks <= 0)
        return;

    POAErrors ret;
    POAConfigValue confVal;
    confVal.intValue = duration * 1000 * 1000;

    LOGF_DEBUG("Blinking %ld time(s) before exposure.", blinks);

    ret = POASetConfig(mCameraInfo.cameraID, POA_EXPOSURE, confVal, POA_FALSE);
    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to set blink exposure to %ldus (%s).", confVal.intValue, Helpers::toString(ret));
        return;
    }

    do
    {
        // start single shot exposure
        ret = POAStartExposure(mCameraInfo.cameraID, POA_TRUE);
        if (ret != POA_OK)
        {
            LOGF_ERROR("Failed to start blink exposure (%s).", Helpers::toString(ret));
            break;
        }

        POACameraState status;
        do
        {
            if (isAbortToQuit)
                return;

            usleep(100 * 1000);
            ret = POAGetCameraState(mCameraInfo.cameraID, &status);
        }
        while (ret == POA_OK && status == STATE_EXPOSING);

        POABool pIsReady;
        POAImageReady(mCameraInfo.cameraID, &pIsReady);

        if (!pIsReady)
        {
            LOGF_ERROR("Blink exposure failed, status %d (%s).", status, Helpers::toString(ret));
            LOGF_ERROR("Blink exposure failed (%s).", Helpers::toString(ret));
            break;
        }
    }
    while (--blinks > 0);

    if (blinks > 0)
    {
        LOGF_WARN("%ld blink exposure(s) NOT done.", blinks);
    }
}

void POABase::workerExposure(const std::atomic_bool &isAbortToQuit, float duration)
{
    POAErrors ret;
    POAConfigValue confVal;

    workerBlinkExposure(
        isAbortToQuit,
        BlinkNP[BLINK_COUNT   ].getValue(),
        BlinkNP[BLINK_DURATION].getValue()
    );

    PrimaryCCD.setExposureDuration(duration);

    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);

    confVal.intValue = (long)(duration * 1000 * 1000);
    ret = POASetConfig(mCameraInfo.cameraID, POA_EXPOSURE, confVal, POA_FALSE);
    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));
    }
    
    // Try exposure for 3 times
    // isDark is for mechanical shutter control
    // However, PlayerOne Cameras doesn't have mechanical shutter
    //POABool isDark = (PrimaryCCD.getFrameType() == INDI::CCDChip::DARK_FRAME) ? POA_TRUE : POA_FALSE;
    
    for (int i = 0; i < 3; i++)
    {
        //ret = POAStartExposure(mCameraInfo.cameraID, isDark);
        ret = POAStartExposure(mCameraInfo.cameraID, POA_TRUE);
        if (ret == POA_OK)
            break;

        LOGF_ERROR("Failed to start exposure (%s)", Helpers::toString(ret));
        // Wait 100ms before trying again
        usleep(100 * 1000);
    }

    if (ret != POA_OK)
    {
        LOG_WARN("PlayerOne firmware might require an update to *compatible mode.");
        return;
    }

    INDI::ElapsedTimer exposureTimer;

    if (duration > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", duration);

    int statRetry = 0;
    POACameraState status;
    POABool pIsReady;
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

        POAErrors ret = POAGetCameraState(mCameraInfo.cameraID, &status);

        if (isAbortToQuit)
            return;
        
        if (ret != POA_OK)
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

        if (ret == POA_ERROR_EXPOSURE_FAILED)
        {
            if (++mExposureRetry < MAX_EXP_RETRIES)
            {
                LOG_DEBUG("POA_ERROR_EXPOSURE_FAILED. Restarting exposure...");
                POAStopExposure(mCameraInfo.cameraID);
                workerExposure(isAbortToQuit, duration);
                return;
            }

            LOGF_ERROR("Exposure failed after %d attempts.", mExposureRetry);
            POAStopExposure(mCameraInfo.cameraID);
            PrimaryCCD.setExposureFailed();
            return;
        }
        
        POAImageReady(mCameraInfo.cameraID, &pIsReady);
    }
    while (!pIsReady);

    // Reset exposure retry
    mExposureRetry = 0;
    PrimaryCCD.setExposureLeft(0.0);
    if (PrimaryCCD.getExposureDuration() > 3)
        LOG_INFO("Exposure done, downloading image...");

    grabImage(duration);
}

///////////////////////////////////////////////////////////////////////
/// Generic constructor
///////////////////////////////////////////////////////////////////////
POABase::POABase()
{
    setVersion(PLAYERONE_VERSION_MAJOR, PLAYERONE_VERSION_MINOR);
    mTimerWE.setSingleShot(true);
    mTimerNS.setSingleShot(true);
}

POABase::~POABase()
{
    if (isConnected())
    {
        Disconnect();
        // updateProperties();
    }
}

const char *POABase::getDefaultName()
{
    return "PlayerOne CCD";
}

void POABase::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}

bool POABase::initProperties()
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

    ADCDepthNP[0].fill("BITS", "Bits", "%2.0f", 0, 32, 1, mCameraInfo.bitDepth);
    ADCDepthNP.fill(getDeviceName(), "ADC_DEPTH", "ADC Depth", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    SDKVersionSP[0].fill("VERSION", "Version", POAGetSDKVersion());
    SDKVersionSP.fill(getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 60, IPS_IDLE);

    SerialNumberTP[0].fill("SN#", "SN#", mSerialNumber);
    SerialNumberTP.fill(getDeviceName(), "Serial Number", "Serial Number", INFO_TAB, IP_RO, 60, IPS_IDLE);

    NicknameTP[0].fill("nickname", "nickname", mNickname);
    NicknameTP.fill(getDeviceName(), "NICKNAME", "Nickname", INFO_TAB, IP_RW, 60, IPS_IDLE);
    
    int maxBin = 1;

    for (const auto &supportedBin : mCameraInfo.bins)
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

    if (maxBin > 1)
        cap |= CCD_CAN_BIN;

    if (mCameraInfo.isHasCooler)
        cap |= CCD_HAS_COOLER;

    //if (mCameraInfo.MechanicalShutter)
    //    cap |= CCD_HAS_SHUTTER;

    if (mCameraInfo.isHasST4Port)
        cap |= CCD_HAS_ST4_PORT;

    if (mCameraInfo.isColorCamera)
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

bool POABase::updateProperties()
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
                    if (mCameraInfo.imgFormats[i] == POA_RAW16)
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

bool POABase::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", mCameraName.c_str());

    POAErrors ret = POA_OK;
    POAConfigValue confVal;

    if (isSimulation() == false)
        ret = POAOpenCamera(mCameraInfo.cameraID);

    if (ret != POA_OK)
    {
        LOGF_ERROR("Error connecting to the CCD (%s).", Helpers::toString(ret));
        return false;
    }

    if (isSimulation() == false)
        ret = POAInitCamera(mCameraInfo.cameraID);

    if (ret != POA_OK)
    {
        LOGF_ERROR("Error Initializing the CCD (%s).", Helpers::toString(ret));
        return false;
    }

    mTimerTemperature.callOnTimeout(std::bind(&POABase::temperatureTimerTimeout, this));
    mTimerTemperature.start(TEMP_TIMER_MS);

    LOG_INFO("Setting intital bandwidth to AUTO on connection.");
    confVal.intValue = 40;
    if ((ret = POASetConfig(mCameraInfo.cameraID, POA_USB_BANDWIDTH_LIMIT, confVal, POA_FALSE)) != POA_OK)
    {
        LOGF_ERROR("Failed to set initial bandwidth (%s).", Helpers::toString(ret));
    }
    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.");

    return true;
}

bool POABase::Disconnect()
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
        POAStopExposure(mCameraInfo.cameraID);
        POACloseCamera(mCameraInfo.cameraID);
    }

    LOG_INFO("Camera is offline.");
    
    
    setConnected(false, IPS_IDLE);
    return true;
}

void POABase::setupParams()
{
    int piNumberOfControls = 0;
    POAErrors ret = POAGetConfigsCount(mCameraInfo.cameraID, &piNumberOfControls);

    if (ret != POA_OK)
        LOGF_ERROR("Failed to get number of controls (%s).", Helpers::toString(ret));

    createControls(piNumberOfControls);

    if (HasCooler())
    {
        POAConfigAttributes pCtrlCaps;
        ret = POAGetConfigAttributes(mCameraInfo.cameraID, POA_TARGET_TEMP, &pCtrlCaps);
        if (ret == POA_OK)
        {
            CoolerNP[0].setMinMax(pCtrlCaps.minValue.intValue, pCtrlCaps.maxValue.intValue);
            CoolerNP[0].setValue(pCtrlCaps.defaultValue.intValue);
        }
    }

    // Set minimum POA_USB_BANDWIDTH_LIMIT on ARM
#ifdef LOW_USB_BANDWIDTH
    for (int j = 0; j < piNumberOfControls; j++)
    {
        POAConfigAttributes pCtrlCaps;
        POAGetConfigAttributes(mCameraInfo.cameraID, j, &pCtrlCaps);

        if (pCtrlCaps.configID == POA_USB_BANDWIDTH_LIMIT)
        {
            LOGF_DEBUG("setupParams->set USB %d", pCtrlCaps.minValue.intValue);

            POAConfigValue confVal;
            confVal.intValue = pCtrlCaps.minValue.intValue;
            POASetConfig(mCameraInfo.cameraID, POA_USB_BANDWIDTH_LIMIT, confVal, POA_FALSE);
            break;
        }
    }
#endif

    // Get Image Format
    int w = 0, h = 0, bin = 0;
    POAImgFormat imgType;

    ret = POAGetROIFormat(mCameraInfo.cameraID, &w, &h, &bin, &imgType);
    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to get ROI format (%s).", Helpers::toString(ret));
    }

    LOGF_DEBUG("CCD ID: %d Width: %d Height: %d Binning: %dx%d Image Type: %d",
               mCameraInfo.cameraID, w, h, bin, bin, imgType);

    // Get video format and bit depth
    int bit_depth = 8;

    switch (imgType)
    {
        case POA_RAW16:
            bit_depth = 16;
            break;

        default:
            break;
    }

    VideoFormatSP.resize(0);
    for (const auto &videoFormat : mCameraInfo.imgFormats)
    {
        if (videoFormat == POA_END)
            break;

        INDI::WidgetSwitch node;
        node.fill(
            Helpers::toString(videoFormat),
            Helpers::toPrettyString(videoFormat),
            videoFormat == imgType ? ISS_ON : ISS_OFF
        );

        node.setAux(const_cast<POAImgFormat*>(&videoFormat));
        VideoFormatSP.push(std::move(node));
        CaptureFormat format = {Helpers::toString(videoFormat),
                                Helpers::toPrettyString(videoFormat),
                                static_cast<uint8_t>((videoFormat == POA_RAW16) ? 16 : 8),
                                videoFormat == imgType
                               };
        addCaptureFormat(format);
    }

    auto x_pixel_size = mCameraInfo.pixelSize;
    auto y_pixel_size = mCameraInfo.pixelSize;

    auto maxWidth = mCameraInfo.maxWidth;
    auto maxHeight = mCameraInfo.maxHeight;

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

    POAConfigValue confVal;
    confVal.floatValue = 0.0;
    POABool isAuto = POA_FALSE;

    ret = POAGetConfig(mCameraInfo.cameraID, POA_TEMPERATURE, &confVal, &isAuto);
    if (ret != POA_OK)
        LOGF_DEBUG("Failed to get temperature (%s).", Helpers::toString(ret));

    TemperatureN[0].value = confVal.floatValue;
    IDSetNumber(&TemperatureNP, nullptr);
    LOGF_INFO("The CCD Temperature is %.3f.", TemperatureN[0].value);

    // stop video capture
    ret = POAStopExposure(mCameraInfo.cameraID);
    if (ret != POA_OK)
        LOGF_ERROR("Failed to stop video capture (%s).", Helpers::toString(ret));

    LOGF_DEBUG("setupParams POASetROIFormat (%dx%d,  bin %d, type %d)", maxWidth, maxHeight, 1, imgType);
    POASetROIFormat(mCameraInfo.cameraID, maxWidth, maxHeight, 1, imgType);

    updateRecorderFormat();
    Streamer->setSize(maxWidth, maxHeight);
}

bool POABase::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    POAErrors ret = POA_OK;

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
                auto numCtrlCap = static_cast<POAConfigAttributes *>(ControlNP[i].getAux());

                if (std::abs(ControlNP[i].getValue() - oldValues[i]) < 0.01)
                    continue;

                LOGF_DEBUG("Setting %s=%.2f...", ControlNP[i].getLabel(), ControlNP[i].getValue());

                POAConfigValue confVal;
                confVal.intValue = static_cast<long>(ControlNP[i].getValue());
                ret = POASetConfig(mCameraInfo.cameraID, numCtrlCap->configID, confVal, POA_FALSE);

                if (ret != POA_OK)
                {
                    LOGF_ERROR("Failed to set %s=%g (%s).", ControlNP[i].getName(), ControlNP[i].getValue(), Helpers::toString(ret));
                    for (size_t i = 0; i < ControlNP.size(); i++)
                        ControlNP[i].setValue(oldValues[i]);
                    ControlNP.setState(IPS_ALERT);
                    ControlNP.apply();
                    return false;
                }

                // If it was set to numCtrlCap->isSupportAuto value to turn it off
                if (numCtrlCap->isSupportAuto)
                {
                    auto sw = ControlSP.find_if([&numCtrlCap](const INDI::WidgetSwitch & it) -> bool
                    {
                        return static_cast<const POAConfigAttributes *>(it.getAux())->configID == numCtrlCap->configID;
                    });

                    if (sw != ControlSP.end())
                        sw->setState(ISS_OFF);

                    ControlSP.apply();
                }
            }

            ControlNP.setState(IPS_OK);
            ControlNP.apply();
            saveConfig(true, ControlNP.getName());
            return true;
        }

        if (BlinkNP.isNameMatch(name))
        {
            BlinkNP.setState(BlinkNP.update(values, names, n) ? IPS_OK : IPS_ALERT);
            BlinkNP.apply();
            saveConfig(true, BlinkNP.getName());
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool POABase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
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
                auto swCtrlCap  = static_cast<POAConfigAttributes *>(sw.getAux());
                POABool swAuto = (sw.getState() == ISS_ON) ? POA_TRUE : POA_FALSE;

                for (auto &num : ControlNP)
                {
                    auto numCtrlCap = static_cast<POAConfigAttributes *>(num.aux0);

                    if (swCtrlCap->configID != numCtrlCap->configID)
                        continue;

                    LOGF_DEBUG("Setting %s=%.2f...", num.label, num.value);

                    POAConfigValue confVal;
                    confVal.floatValue = num.value;
                    POAErrors ret = POASetConfig(mCameraInfo.cameraID, numCtrlCap->configID, confVal, swAuto);
                    if (ret != POA_OK)
                    {
                        LOGF_ERROR("Failed to set %s=%g (%s).", num.name, num.value, Helpers::toString(ret));
                        ControlNP.setState(IPS_ALERT);
                        ControlSP.setState(IPS_ALERT);
                        ControlNP.apply();
                        ControlSP.apply();
                        return false;
                    }
                    numCtrlCap->isSupportAuto = swAuto;
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

            POAConfig flip = POA_FLIP_NONE;
            if (FlipSP[FLIP_HORIZONTAL].getState() == ISS_ON)
            {
                if (FlipSP[FLIP_VERTICAL].getState() == ISS_ON)
                {
                    flip = POA_FLIP_BOTH;                  
                }
                else
                {
                    flip = POA_FLIP_HORI;                 
                }
            }
            else
            {
                if (FlipSP[FLIP_VERTICAL].getState() == ISS_ON)
                {
                    flip = POA_FLIP_VERT;
                }
            }
            
            POAConfigValue confVal; // confValue will be ignored by POASetConfig()
            POAErrors ret = POASetConfig(mCameraInfo.cameraID, flip, confVal, POA_FALSE);
            if (ret != POA_OK)
            {
                LOGF_ERROR("Failed to set POA_FLIP=%d (%s).", flip, Helpers::toString(ret));
                FlipSP.setState(IPS_ALERT);
                FlipSP.apply();
                return false;
            }

            // Compensate bayer pattern (effective for RAW data format)
            char bayer[5];
            POABayerCompensationByFlip(flip, bayer); 
            IUSaveText(&BayerT[2], bayer);

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

            auto  result = setVideoFormat(targetIndex);
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

bool POABase::setVideoFormat(uint8_t index)
{
    if (index == VideoFormatSP.findOnSwitchIndex())
        return true;

    VideoFormatSP.reset();
    VideoFormatSP[index].setState(ISS_ON);

    switch (getImageType())
    {
        case POA_RAW16:
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

int POABase::SetTemperature(double temperature)
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

    POAErrors ret;
    POAConfigValue confVal;

    confVal.intValue = std::round(temperature);
    ret = POASetConfig(mCameraInfo.cameraID, POA_TARGET_TEMP, confVal, POA_TRUE);
    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to set temperature (%s).", Helpers::toString(ret));
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    mTargetTemperature = temperature;
    LOGF_INFO("Setting temperature to %.2f C.", temperature);
    return 0;
}

bool POABase::activateCooler(bool enable)
{
    POAConfigValue confVal;

    confVal.boolValue = enable ? POA_TRUE : POA_FALSE;
    POAErrors ret = POASetConfig(mCameraInfo.cameraID, POA_COOLER, confVal, POA_FALSE);
    if (ret != POA_OK)
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

    return (ret == POA_OK);
}

bool POABase::StartExposure(float duration)
{
    mExposureRetry = 0;
    mWorker.start(std::bind(&POABase::workerExposure, this, std::placeholders::_1, duration));
    return true;
}

bool POABase::AbortExposure()
{
    LOG_DEBUG("Aborting exposure...");

    mWorker.quit();

    POAStopExposure(mCameraInfo.cameraID);
    return true;
}

bool POABase::StartStreaming()
{
#if 0
    POAImgFormat type = getImageType();

    if (type != POA_MONO8 && type != POA_RGB24)
    {
        VideoFormatSP.reset();
        auto vf = VideoFormatSP.findWidgetByName("POA_MONO8");
        if (vf == nullptr)
            vf = VideoFormatSP.findWidgetByName("POA_RAW8");

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
    mWorker.start(std::bind(&POABase::workerStreamVideo, this, std::placeholders::_1));
    return true;
}

bool POABase::StopStreaming()
{
    mWorker.quit();
    return true;
}

bool POABase::UpdateCCDFrame(int x, int y, int w, int h)
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

    // PlayerOne rules are this: width%4 = 0, height%2 = 0
    // if this condition is not met, we set it internally to slightly smaller values

    if (warn_roi_width && subW % 4 > 0)
    {
        LOGF_INFO("Incompatible frame width %dpx. Reducing by %dpx.", subW, subW % 4);
        warn_roi_width = false;
    }
    if (warn_roi_height && subH % 2 > 0)
    {
        LOGF_INFO("Incompatible frame height %dpx. Reducing by %dpx.", subH, subH % 2);
        warn_roi_height = false;
    }

    subW -= subW % 4;
    subH -= subH % 2;

    LOGF_DEBUG("Frame ROI x:%d y:%d w:%d h:%d", subX, subY, subW, subH);

    POAErrors ret;

    ret = POASetROIFormat(mCameraInfo.cameraID, subW, subH, binX, getImageType());
    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to set ROI (%s).", Helpers::toString(ret));
        return false;
    }

    ret = POASetImageStartPos(mCameraInfo.cameraID, subX, subY);
    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to set start position (%s).", Helpers::toString(ret));
        return false;
    }

    // Set UNBINNED coords
    //PrimaryCCD.setFrame(x, y, w, h);
    PrimaryCCD.setFrame(subX * binX, subY * binY, subW * binX, subH * binY);

    // Total bytes required for image buffer
    uint32_t nbuf = (subW * subH * static_cast<uint32_t>(PrimaryCCD.getBPP()) / 8) * ((getImageType() == POA_RGB24) ? 3 :
                    1);

    LOGF_DEBUG("Setting frame buffer size to %d bytes.", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(subW, subH);

    return true;
}

bool POABase::UpdateCCDBin(int binx, int biny)
{
    INDI_UNUSED(biny);

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int POABase::grabImage(float duration)
{
    POAErrors ret = POA_OK;

    POAImgFormat type = getImageType();

    std::unique_lock<std::mutex> guard(ccdBufferLock);
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    uint8_t *buffer = image;

    uint16_t subW = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    uint16_t subH = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();
    int nChannels = (type == POA_RGB24) ? 3 : 1;
    size_t nTotalBytes = subW * subH * nChannels * (PrimaryCCD.getBPP() / 8);

    if (type == POA_RGB24)
    {
        buffer = static_cast<uint8_t *>(malloc(nTotalBytes));
        if (buffer == nullptr)
        {
            LOGF_ERROR("%s: %d malloc failed (RGB 24).", getDeviceName());
            return -1;
        }
    }

    ret = POAGetImageData(mCameraInfo.cameraID, buffer, nTotalBytes, -1);
    if (ret != POA_OK)
    {
        LOGF_ERROR(
            "Failed to get data after exposure (%dx%d #%d channels) (%s).",
            subW, subH, nChannels, Helpers::toString(ret)
        );
        if (type == POA_RGB24)
            free(buffer);
        return -1;
    }

    if (type == POA_RGB24)
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

    PrimaryCCD.setNAxis(type == POA_RGB24 ? 3 : 2);

    // If mono camera or we're sending Luma or RGB, turn off bayering
    if (mCameraInfo.isColorCamera == POA_FALSE || type == POA_MONO8 || type == POA_RGB24 || isMonoBinActive())
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    else
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);

    if (duration > VERBOSE_EXPOSURE)
        LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);
    return 0;
}

bool POABase::isMonoBinActive()
{
    long monoBin = 0;

#if 1   // MONO_BIN has supported since SDK v3.4.0
    POABool isAuto = POA_FALSE;
    POAConfigValue confVal;
    POAErrors ret = POAGetConfig(mCameraInfo.cameraID, POA_MONO_BIN, &confVal, &isAuto);
    if (ret != POA_OK)
    {
        if (ret != POA_ERROR_INVALID_CONFIG)
        {
            LOGF_ERROR("Failed to get mono bin information (%s).", Helpers::toString(ret));
        }
        return false;
    }
    monoBin = confVal.boolValue;
#endif

    if (monoBin == 0)
    {
        return false;
    }

    int width = 0, height = 0, bin = 1;
    POAImgFormat imgType = POA_RAW8;
    ret = POAGetROIFormat(mCameraInfo.cameraID, &width, &height, &bin, &imgType);
    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to get ROI format (%s).", Helpers::toString(ret));
        return false;
    }

    return (imgType == POA_RAW8 || imgType == POA_RAW16) && bin > 1;
}

bool POABase::hasFlipControl()
{
    if (find_if(begin(mControlCaps), end(mControlCaps), [](POAConfigAttributes cap)
{
    return cap.configID == POA_FLIP_BOTH;
}) == end(mControlCaps))
    return false;
    else
        return true;
}

/* The timer call back is used for temperature monitoring */
void POABase::temperatureTimerTimeout()
{
    POAErrors ret;
    POABool isAuto = POA_FALSE;
    POAConfigValue confVal;

    double value = 0.0;
    IPState newState = TemperatureNP.s;

    ret = POAGetConfig(mCameraInfo.cameraID, POA_TEMPERATURE, &confVal, &isAuto);
    value = confVal.floatValue;

    if (ret != POA_OK)
    {
        LOGF_ERROR("Failed to get temperature (%s).", Helpers::toString(ret));
        newState = IPS_ALERT;
    }
    else
    {
        mCurrentTemperature = value;
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
        ret = POAGetConfig(mCameraInfo.cameraID, POA_COOLER_POWER, &confVal, &isAuto);
        value = confVal.intValue;
        if (ret != POA_OK)
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

IPState POABase::guidePulse(INDI::Timer &timer, float ms, POAConfig dir)
{
    timer.stop();

    POAPulseGuideOn(mCameraInfo.cameraID, dir);
    LOGF_DEBUG("Starting %s guide for %f ms.", Helpers::toString(dir), ms);

    timer.callOnTimeout([this, dir]
    {
        POAPulseGuideOff(mCameraInfo.cameraID, dir);
        LOGF_DEBUG("Stopped %s guide.", Helpers::toString(dir));

        if (dir == POA_GUIDE_NORTH || dir == POA_GUIDE_SOUTH)
            GuideComplete(AXIS_DE);
        else if (dir == POA_GUIDE_EAST || dir == POA_GUIDE_WEST)
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


void POABase::stopGuidePulse(INDI::Timer &timer)
{
    if (timer.isActive())
    {
        timer.stop();
        timer.timeout();
    }
}

IPState POABase::GuideNorth(uint32_t ms)
{
    return guidePulse(mTimerNS, ms, POA_GUIDE_NORTH);
}

IPState POABase::GuideSouth(uint32_t ms)
{
    return guidePulse(mTimerNS, ms, POA_GUIDE_SOUTH);
}

IPState POABase::GuideEast(uint32_t ms)
{
    return guidePulse(mTimerWE, ms, POA_GUIDE_EAST);
}

IPState POABase::GuideWest(uint32_t ms)
{
    return guidePulse(mTimerWE, ms, POA_GUIDE_WEST);
}

void POABase::createControls(int piNumberOfControls)
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
        POAErrors ret = POAGetConfigAttributes(mCameraInfo.cameraID, i++, &cap);
        if (ret != POA_OK)
        {
            LOGF_ERROR("Failed to get control information (%s).", Helpers::toString(ret));
            return;
        }

        LOGF_DEBUG("Control #%d: name (%s), Descp (%s), Min (%ld), Max (%ld), Default Value (%ld), isSupportAuto (%s), "
                   "isWritale (%s) ",
                   i, cap.szConfName, cap.szDescription, cap.minValue.intValue, cap.maxValue.intValue,
                   cap.defaultValue.intValue, cap.isSupportAuto ? "True" : "False",
                   cap.isWritable ? "True" : "False");

        if (cap.isWritable == POA_FALSE || cap.configID == POA_TARGET_TEMP || cap.configID == POA_COOLER ||
                cap.configID == POA_GUIDE_NORTH || cap.configID == POA_GUIDE_SOUTH ||
                cap.configID == POA_GUIDE_EAST || cap.configID == POA_GUIDE_WEST ||
                cap.configID == POA_FLIP_NONE || cap.configID == POA_FLIP_HORI ||
                cap.configID == POA_FLIP_VERT || cap.configID == POA_FLIP_BOTH)
            continue;

        // Update Min/Max exposure as supported by the camera
        if (cap.configID == POA_EXPOSURE)
        {
            double minExp = cap.minValue.intValue / 1000000.0;
            double maxExp = cap.maxValue.intValue / 1000000.0;
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", minExp, maxExp, 1);
            continue;
        }

        if (cap.configID == POA_USB_BANDWIDTH_LIMIT)
        {
            long value = cap.minValue.intValue;

#ifndef LOW_USB_BANDWIDTH
            if (!mCameraInfo.isUSB3Speed)
                value = 0.8 * cap.maxValue.intValue;
#endif

            LOGF_DEBUG("createControls->set USB %d", value);
            POAConfigValue confVal;
            confVal.intValue = value;
            POASetConfig(mCameraInfo.cameraID, cap.configID, confVal, POA_FALSE);
        }

        POABool isAuto = POA_FALSE;
        POAConfigValue confVal;

        POAGetConfig(mCameraInfo.cameraID, cap.configID, &confVal, &isAuto);

        if (cap.isWritable)
        {
            LOGF_DEBUG("Adding above control as writable control number %d.", ControlNP.size());

            // JM 2018-07-04: If Max-Min == 1 then it's boolean value
            // So no need to set a custom step value.
            double step = 1;
            if (cap.maxValue.intValue - cap.minValue.intValue > 1)
                step = (cap.maxValue.intValue - cap.minValue.intValue) / 100.0;

            INDI::WidgetNumber node;
            node.fill(cap.szConfName, cap.szConfName, "%g", cap.minValue.intValue, cap.maxValue.intValue, step, confVal.intValue);
            node.setAux(&cap);
            ControlNP.push(std::move(node));
        }

        if (cap.isSupportAuto)
        {
            LOGF_DEBUG("Adding above control as auto control number %d.", ControlSP.size());

            char autoName[MAXINDINAME];
            snprintf(autoName, MAXINDINAME, "AUTO_%s", cap.szConfName);

            INDI::WidgetSwitch node;
            node.fill(autoName, cap.szConfName, isAuto == POA_TRUE ? ISS_ON : ISS_OFF);
            node.setAux(&cap);
            ControlSP.push(std::move(node));
        }
    }

    // Resize the buffers to free up unused space
    ControlNP.shrink_to_fit();
    ControlSP.shrink_to_fit();
}

POAImgFormat POABase::getImageType() const
{
    auto sp = VideoFormatSP.findOnSwitch();
    return sp != nullptr ? *static_cast<POAImgFormat *>(sp->getAux()) : POA_END;
}

void POABase::updateControls()
{
    for (auto &num : ControlNP)
    {
        auto numCtrlCap = static_cast<POAConfigAttributes *>(num.getAux());
        long value      = 0;
        POABool isAuto = POA_FALSE;
        POAConfigValue confVal;
        POAGetConfig(mCameraInfo.cameraID, numCtrlCap->configID, &confVal, &isAuto);
        value = confVal.intValue;

        num.setValue(value);

        auto sw = ControlSP.find_if([&numCtrlCap](const INDI::WidgetSwitch & it) -> bool
        {
            return static_cast<const POAConfigAttributes *>(it.getAux())->configID == numCtrlCap->configID;
        });

        if (sw != ControlSP.end())
            sw->setState(isAuto == POA_TRUE ? ISS_ON : ISS_OFF);
    }

    ControlNP.apply();
    ControlSP.apply();
}

void POABase::updateRecorderFormat()
{
    mCurrentVideoFormat = getImageType();
    if (mCurrentVideoFormat == POA_END)
        return;

    Streamer->setPixelFormat(
        Helpers::pixelFormat(
            mCurrentVideoFormat,
            mCameraInfo.bayerPattern,
            mCameraInfo.isColorCamera
        ),
        mCurrentVideoFormat == POA_RAW16 ? 16 : 8
    );
}

void POABase::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
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

bool POABase::saveConfigItems(FILE *fp)
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

bool POABase::SetCaptureFormat(uint8_t index)
{
    return setVideoFormat(index);
}

POAErrors POABase::POASetROIFormat(int cameraID, int width, int height, int bin, POAImgFormat imgType)
{
    POAErrors ret;

    // ImageBin should be set first
    if ((ret = POASetImageBin(cameraID, bin)) != POA_OK)
        return ret;
    // then, ImageSize can be set
    if ((ret = POASetImageSize(cameraID, width, height)) != POA_OK)
        return ret;
    if ((ret = POASetImageFormat(cameraID, imgType)) != POA_OK)
        return ret;

    return POA_OK;
}

POAErrors POABase::POAGetROIFormat(int cameraID, int *width, int *height, int *bin, POAImgFormat *imgType)
{
    POAErrors ret;

    if ((ret = POAGetImageSize(cameraID, width, height)) != POA_OK)
        return ret;
    if ((ret = POAGetImageBin(cameraID, bin)) != POA_OK)
        return ret;
    if ((ret = POAGetImageFormat(cameraID, imgType)) != POA_OK)
        return ret;

    return POA_OK;
}

POAErrors POABase::POAPulseGuideOn(int cameraID, POAConfig dir)
{
    POAConfigValue confVal;
    confVal.boolValue = POA_TRUE;
    return POASetConfig(cameraID, dir, confVal, POA_FALSE); // start to guide
}

POAErrors POABase::POAPulseGuideOff(int cameraID, POAConfig dir)
{
    POAConfigValue confVal;
    confVal.boolValue = POA_FALSE;
    return POASetConfig(cameraID, dir, confVal, POA_FALSE); // stop to guide
}

// Compensate bayer pttern by flip (effective for RAW data format)
POAErrors POABase::POABayerCompensationByFlip(POAConfig flip, char *dest)
{
    const char *src = getBayerString();
    
    switch (flip)
    {
        case POA_FLIP_NONE:
            sprintf(dest, "%c%c%c%c", src[0], src[1], src[2], src[3]);
            break;
        case POA_FLIP_HORI:
            sprintf(dest, "%c%c%c%c", src[1], src[0], src[3], src[2]);
            break;
        case POA_FLIP_VERT:
            sprintf(dest, "%c%c%c%c", src[2], src[3], src[0], src[1]);
            break;
        case POA_FLIP_BOTH:
            sprintf(dest, "%c%c%c%c", src[3], src[2], src[1], src[0]);
            break;
        default:
            return POA_ERROR_INVALID_ARGU;
    }
    
    return POA_OK;
}
