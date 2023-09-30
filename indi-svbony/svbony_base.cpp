/*
    SVBony Camera Driver

    Copyright (C) 2023 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)
    Copyright (C) 2020 Blaise-Florentin Collin (thx8411@yahoo.fr)

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

#include "svbony_base.h"
#include "svbony_helpers.h"

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

const char *SVBONYBase::getBayerString() const
{
    return Helpers::toString(mCameraProperty.BayerPattern);
}

void SVBONYBase::workerStreamVideo(const std::atomic_bool &isAboutToQuit)
{
    SVB_ERROR_CODE ret;
    double ExposureRequest = 1.0 / Streamer->getTargetFPS();
    long uSecs = static_cast<long>(ExposureRequest * 950000.0);

    ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_EXPOSURE, uSecs, SVB_FALSE);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));
    }

    ret = SVBStartVideoCapture(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to start video capture (%s).", Helpers::toString(ret));
    }

    while (!isAboutToQuit)
    {
        uint8_t *targetFrame = PrimaryCCD.getFrameBuffer();
        uint32_t totalBytes  = PrimaryCCD.getFrameBufferSize();
        int waitMS           = static_cast<int>((ExposureRequest * 2000.0) + 500);

        ret = SVBGetVideoData(mCameraInfo.CameraID, targetFrame, totalBytes, waitMS);
        if (ret != SVB_SUCCESS)
        {
            if (ret != SVB_ERROR_TIMEOUT)
            {
                Streamer->setStream(false);
                LOGF_ERROR("Failed to read video data (%s).", Helpers::toString(ret));
                break;
            }

            usleep(100);
            continue;
        }

        if (mCurrentVideoFormat == SVB_IMG_RGB24)
            for (uint32_t i = 0; i < totalBytes; i += 3)
                std::swap(targetFrame[i], targetFrame[i + 2]);

        Streamer->newFrame(targetFrame, totalBytes);
    }

    SVBStopVideoCapture(mCameraInfo.CameraID);
}

void SVBONYBase::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    SVB_ERROR_CODE ret;

    ret = SVBStartVideoCapture(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to start video capture (%s).", Helpers::toString(ret));
        return;
    }

    PrimaryCCD.setExposureDuration(duration);

    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);
    ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_EXPOSURE, duration * 1000 * 1000, SVB_FALSE);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));
    }

    // Try exposure for 3 times
    for (int i = 0; i < 3; i++)
    {
        ret = SVBSendSoftTrigger(mCameraInfo.CameraID);
        if (ret == SVB_SUCCESS)
            break;

        LOGF_ERROR("Failed to start exposure (%d)", Helpers::toString(ret));
        // Wait 100ms before trying again
        usleep(100 * 1000);
    }

    INDI::ElapsedTimer exposureTimer;

    if (duration > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", duration);

    while (1)
    {
        if (isAboutToQuit)
        {
            // Discard from buffer but do not send
            grabImage(0, false);
            return;
        }

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
        else
        {
            grabImage(duration);
            return;
        }

        usleep(delay * 1000 * 1000);
    }
}

///////////////////////////////////////////////////////////////////////
/// Generic constructor
///////////////////////////////////////////////////////////////////////
SVBONYBase::SVBONYBase()
{
    setVersion(SVBONY_VERSION_MAJOR, SVBONY_VERSION_MINOR);
    mTimerWE.setSingleShot(true);
    mTimerNS.setSingleShot(true);
}

SVBONYBase::~SVBONYBase()
{
    if (isConnected())
    {
        Disconnect();
    }
}

const char *SVBONYBase::getDefaultName()
{
    return "SVBONY CCD";
}

void SVBONYBase::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}

bool SVBONYBase::initProperties()
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

    ADCDepthNP[0].fill("BITS", "Bits", "%2.0f", 0, 32, 1, 16);
    ADCDepthNP.fill(getDeviceName(), "ADC_DEPTH", "ADC Depth", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    SDKVersionSP[0].fill("VERSION", "Version", SVBGetSDKVersion());
    SDKVersionSP.fill(getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 60, IPS_IDLE);

    SerialNumberTP[0].fill("SN#", "SN#", mSerialNumber);
    SerialNumberTP.fill(getDeviceName(), "Serial Number", "Serial Number", INFO_TAB, IP_RO, 60, IPS_IDLE);

    NicknameTP[0].fill("nickname", "nickname", mNickname);
    NicknameTP.fill(getDeviceName(), "NICKNAME", "Nickname", INFO_TAB, IP_RW, 60, IPS_IDLE);

    addAuxControls();

    return true;
}

bool SVBONYBase::updateProperties()
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
        //        // Even if there is no cooler, we define temperature property as READ ONLY
        //        else
        //        {
        //            TemperatureNP.p = IP_RO;
        //            defineProperty(&TemperatureNP);
        //        }

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
                    if (mCameraProperty.SupportedVideoFormat[i] == SVB_IMG_RAW16)
                    {
                        setVideoFormat(i);
                        CaptureFormatSP[i].setState(ISS_ON);
                        break;
                    }
                }
                CaptureFormatSP.apply();
            }
        }

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

bool SVBONYBase::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", mCameraName.c_str());

    auto ret = SVBOpenCamera(mCameraInfo.CameraID);

    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error Initializing the CCD (%s).", Helpers::toString(ret));
        return false;
    }

    // Restore settings
    ret = SVBRestoreDefaultParam(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
    {
        LOGF_WARN("Error Initializing the CCD (%s).", Helpers::toString(ret));
    }

    ret = SVBSetAutoSaveParam(mCameraInfo.CameraID, SVB_FALSE);
    if (ret != SVB_SUCCESS)
    {
        LOGF_WARN("Error Initializing the CCD (%s).", Helpers::toString(ret));
    }

    // Get Camera Property
    ret = SVBGetCameraProperty(mCameraInfo.CameraID, &mCameraProperty);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error Initializing the CCD (%s).", Helpers::toString(ret));
        return false;
    }

    ret = SVBGetCameraPropertyEx(mCameraInfo.CameraID, &mCameraPropertyExtended);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error Initializing the CCD (%s).", Helpers::toString(ret));
        return false;
    }

    ADCDepthNP[0].setValue(mCameraProperty.MaxBitDepth);

    int maxBin = 1;

    for (const auto &supportedBin : mCameraProperty.SupportedBins)
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
    LOGF_DEBUG("Camera: %s", mCameraInfo.FriendlyName);
    LOGF_DEBUG("ID: %d", mCameraInfo.CameraID);
    LOGF_DEBUG("MaxWidth: %d MaxHeight: %d", mCameraProperty.MaxWidth, mCameraProperty.MaxHeight);
    LOGF_DEBUG("IsColorCamera: %s", mCameraProperty.IsColorCam ? "True" : "False");
    LOGF_DEBUG("IsCoolerCam: %s", mCameraPropertyExtended.bSupportControlTemp ? "True" : "False");
    LOGF_DEBUG("BitDepth: %d", mCameraProperty.MaxBitDepth);
    LOGF_DEBUG("IsTriggerCam: %s", mCameraProperty.IsTriggerCam ? "True" : "False");

    uint32_t cap = 0;

    if (maxBin > 1)
        cap |= CCD_CAN_BIN;

    if (mCameraPropertyExtended.bSupportControlTemp)
        cap |= CCD_HAS_COOLER;

    if (mCameraPropertyExtended.bSupportPulseGuide)
        cap |= CCD_HAS_ST4_PORT;

    if (mCameraProperty.IsColorCam)
        cap |= CCD_HAS_BAYER;

    cap |= CCD_CAN_ABORT;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_HAS_STREAMING;

#ifdef HAVE_WEBSOCKET
    cap |= CCD_HAS_WEB_SOCKET;
#endif

    SetCCDCapability(cap);

    if (mCameraPropertyExtended.bSupportControlTemp)
    {
        mTimerTemperature.callOnTimeout(std::bind(&SVBONYBase::temperatureTimerTimeout, this));
        mTimerTemperature.start(TEMP_TIMER_MS);
    }

    // fix for SDK gain error issue
    // set exposure time
    SVBSetControlValue(mCameraInfo.CameraID, SVB_EXPOSURE, static_cast<long>(1 * 1000000L), SVB_FALSE);

    /* Success! */
    LOG_INFO("Camera is online. Retrieving configuration.");

    return true;
}

bool SVBONYBase::Disconnect()
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
        SVBStopVideoCapture(mCameraInfo.CameraID);
        SVBCloseCamera(mCameraInfo.CameraID);
    }

    LOG_INFO("Camera is offline.");


    setConnected(false, IPS_IDLE);
    return true;
}

void SVBONYBase::setupParams()
{
    int piNumberOfControls = 0;
    SVB_ERROR_CODE ret;

    ret = SVBGetNumOfControls(mCameraInfo.CameraID, &piNumberOfControls);

    if (ret != SVB_SUCCESS)
        LOGF_ERROR("Failed to get number of controls (%s).", Helpers::toString(ret));

    createControls(piNumberOfControls);

    if (HasCooler())
    {
        SVB_CONTROL_CAPS pCtrlCaps;
        ret = SVBGetControlCaps(mCameraInfo.CameraID, SVB_TARGET_TEMPERATURE, &pCtrlCaps);
        if (ret == SVB_SUCCESS)
        {
            CoolerNP[0].setMinMax(pCtrlCaps.MinValue, pCtrlCaps.MaxValue);
            CoolerNP[0].setValue(pCtrlCaps.DefaultValue);
        }
    }

    // Get Image Format
    int x, y, w = 0, h = 0, bin = 0;
    ret = SVBGetROIFormat(mCameraInfo.CameraID, &x, &y, &w, &h, &bin);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to get ROI format (%s).", Helpers::toString(ret));
    }

    ret = SVBGetOutputImageType(mCameraInfo.CameraID, &mCurrentVideoFormat);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to get output image type (%s).", Helpers::toString(ret));
    }

    LOGF_DEBUG("CCD ID: %d Width: %d Height: %d Binning: %dx%d Image Type: %d", mCameraInfo.CameraID, w, h, bin, bin,
               mCurrentVideoFormat);

    // Get video format and bit depth
    int bit_depth = 8;
    switch (mCurrentVideoFormat)
    {
        case SVB_IMG_RAW16:
            bit_depth = 16;
            break;

        default:
            break;
    }

    VideoFormatSP.resize(0);
    for (const auto &videoFormat : mCameraProperty.SupportedVideoFormat)
    {
        if (videoFormat == SVB_IMG_END)
            break;

        INDI::WidgetSwitch node;
        node.fill(
            Helpers::toString(videoFormat),
            Helpers::toPrettyString(videoFormat),
            videoFormat == mCurrentVideoFormat ? ISS_ON : ISS_OFF
        );

        node.setAux(const_cast<SVB_IMG_TYPE*>(&videoFormat));
        VideoFormatSP.push(std::move(node));
        CaptureFormat format = {Helpers::toString(videoFormat),
                                Helpers::toPrettyString(videoFormat),
                                static_cast<uint8_t>((videoFormat == SVB_IMG_RAW16) ? 16 : 8),
                                videoFormat == mCurrentVideoFormat
                               };
        addCaptureFormat(format);
    }

    float pixelSize = 2.75;
    SVBGetSensorPixelSize(mCameraInfo.CameraID, &pixelSize);

    uint32_t maxWidth = mCameraProperty.MaxWidth;
    uint32_t maxHeight = mCameraProperty.MaxHeight;

    SetCCDParams(maxWidth, maxHeight, bit_depth, pixelSize, pixelSize);

    // Let's calculate required buffer
    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    long value      = 0;
    SVB_BOOL isAuto = SVB_FALSE;

    ret = SVBGetControlValue(mCameraInfo.CameraID, SVB_CURRENT_TEMPERATURE, &value, &isAuto);
    if (ret != SVB_SUCCESS)
        LOGF_DEBUG("Failed to get temperature (%s).", Helpers::toString(ret));
    else
    {
        TemperatureN[0].value = value / 10.0;
        IDSetNumber(&TemperatureNP, nullptr);
        LOGF_INFO("The CCD Temperature is %.3f.", TemperatureN[0].value);
    }

    ret = SVBStopVideoCapture(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
        LOGF_ERROR("Failed to stop video capture (%s).", Helpers::toString(ret));

    LOGF_DEBUG("setupParams SVBSetROIFormat (%dx%d,  bin %d, type %d)", maxWidth, maxHeight, 1, mCurrentVideoFormat);
    SVBSetROIFormat(mCameraInfo.CameraID, 0, 0, maxWidth, maxHeight, 1);

    updateRecorderFormat();
    Streamer->setSize(maxWidth, maxHeight);
}

bool SVBONYBase::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    SVB_ERROR_CODE ret = SVB_SUCCESS;

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
                auto numCtrlCap = static_cast<SVB_CONTROL_CAPS *>(ControlNP[i].getAux());

                if (std::abs(ControlNP[i].getValue() - oldValues[i]) < 0.01)
                    continue;

                LOGF_DEBUG("Setting %s=%.2f...", ControlNP[i].getLabel(), ControlNP[i].getValue());
                ret = SVBSetControlValue(mCameraInfo.CameraID, numCtrlCap->ControlType, static_cast<long>(ControlNP[i].getValue()),
                                         SVB_FALSE);
                if (ret != SVB_SUCCESS)
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
                        return static_cast<const SVB_CONTROL_CAPS *>(it.getAux())->ControlType == numCtrlCap->ControlType;
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
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool SVBONYBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
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
                auto swCtrlCap  = static_cast<SVB_CONTROL_CAPS *>(sw.getAux());
                SVB_BOOL swAuto = (sw.getState() == ISS_ON) ? SVB_TRUE : SVB_FALSE;

                for (auto &num : ControlNP)
                {
                    auto numCtrlCap = static_cast<SVB_CONTROL_CAPS *>(num.aux0);

                    if (swCtrlCap->ControlType != numCtrlCap->ControlType)
                        continue;

                    LOGF_DEBUG("Setting %s=%.2f...", num.label, num.value);

                    SVB_ERROR_CODE ret = SVBSetControlValue(mCameraInfo.CameraID, numCtrlCap->ControlType, num.value, swAuto);
                    if (ret != SVB_SUCCESS)
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
                flip |= SVB_FLIP_HORIZ;
            if (FlipSP[FLIP_VERTICAL].getState() == ISS_ON)
                flip |= SVB_FLIP_VERT;

            SVB_ERROR_CODE ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_FLIP, flip, SVB_FALSE);
            if (ret != SVB_SUCCESS)
            {
                LOGF_ERROR("Failed to set SVB_FLIP=%d (%s).", flip, Helpers::toString(ret));
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

bool SVBONYBase::setVideoFormat(uint8_t index)
{
    if (index == VideoFormatSP.findOnSwitchIndex())
        return true;

    VideoFormatSP.reset();
    VideoFormatSP[index].setState(ISS_ON);

    // When changing video format, reset frame
    UpdateCCDFrame(0, 0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    updateRecorderFormat();

    VideoFormatSP.setState(IPS_OK);
    VideoFormatSP.apply();
    return true;
}

int SVBONYBase::SetTemperature(double temperature)
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

    SVB_ERROR_CODE ret;

    ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_TARGET_TEMPERATURE, std::round(temperature), SVB_TRUE);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to set temperature (%s).", Helpers::toString(ret));
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    mTargetTemperature = temperature;
    LOGF_INFO("Setting temperature to %.2f C.", temperature);
    return 0;
}

bool SVBONYBase::activateCooler(bool enable)
{
    SVB_ERROR_CODE ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_COOLER_ENABLE, enable ? SVB_TRUE : SVB_FALSE, SVB_FALSE);
    if (ret != SVB_SUCCESS)
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

    return (ret == SVB_SUCCESS);
}

bool SVBONYBase::StartExposure(float duration)
{
    mExposureRetry = 0;
    mWorker.start(std::bind(&SVBONYBase::workerExposure, this, std::placeholders::_1, duration));
    return true;
}

bool SVBONYBase::AbortExposure()
{
    LOG_DEBUG("Aborting exposure...");

    mWorker.quit();

    SVBStopVideoCapture(mCameraInfo.CameraID);
    return true;
}

bool SVBONYBase::StartStreaming()
{
    mWorker.start(std::bind(&SVBONYBase::workerStreamVideo, this, std::placeholders::_1));
    return true;
}

bool SVBONYBase::StopStreaming()
{
    mWorker.quit();
    return true;
}

bool SVBONYBase::UpdateCCDFrame(int x, int y, int w, int h)
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

    SVB_ERROR_CODE ret;

    ret = SVBSetROIFormat(mCameraInfo.CameraID, subX, subY, subW, subH, binX);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to set ROI (%s).", Helpers::toString(ret));
        return false;
    }

    mCurrentVideoFormat = getImageType();
    switch (mCurrentVideoFormat)
    {
        case SVB_IMG_RAW16:
            PrimaryCCD.setBPP(16);
            break;

        default:
            PrimaryCCD.setBPP(8);
            break;
    }

    SVBSetOutputImageType(mCameraInfo.CameraID, mCurrentVideoFormat);

    // Set UNBINNED coords
    PrimaryCCD.setFrame(subX * binX, subY * binY, subW * binX, subH * binY);

    // Total bytes required for image buffer
    auto nbuf = (subW * subH * static_cast<uint32_t>(PrimaryCCD.getBPP()) / 8) * ((getImageType() == SVB_IMG_RGB24) ? 3 : 1);

    LOGF_DEBUG("Setting frame buffer size to %d bytes.", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(subW, subH);

    return true;
}

bool SVBONYBase::UpdateCCDBin(int binx, int biny)
{
    INDI_UNUSED(biny);

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
bool SVBONYBase::grabImage(float duration, bool send)
{
    SVB_ERROR_CODE ret = SVB_SUCCESS;

    SVB_IMG_TYPE type = getImageType();

    std::unique_lock<std::mutex> guard(ccdBufferLock);
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    uint8_t *buffer = image;

    uint16_t subW = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    uint16_t subH = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();
    int nChannels = (type == SVB_IMG_RGB24) ? 3 : 1;
    size_t nTotalBytes = subW * subH * nChannels * (PrimaryCCD.getBPP() / 8);

    if (type == SVB_IMG_RGB24)
    {
        buffer = static_cast<uint8_t *>(malloc(nTotalBytes));
        if (buffer == nullptr)
        {
            LOGF_ERROR("%s: %d malloc failed (RGB 24).", getDeviceName());
            return -1;
        }
    }

    for (int i = 0; i < 5; i++)
    {
        ret = SVBGetVideoData(mCameraInfo.CameraID, buffer, nTotalBytes, 1000);
        // Sleep for 100 ms and try up to five times
        if (ret == SVB_ERROR_TIMEOUT)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        else if (ret != SVB_SUCCESS)
        {
            LOGF_ERROR("Failed to get data after exposure (%dx%d #%d channels) (%s).", subW, subH, nChannels, Helpers::toString(ret));
            PrimaryCCD.setExposureLeft(0);
            PrimaryCCD.setExposureFailed();
            if (type == SVB_IMG_RGB24)
                free(buffer);
            return false;
        }
        else
            break;
    }

    PrimaryCCD.setExposureLeft(0);

    // Duration 0 means aborted so we just need to discard the frame.
    if (duration == 0)
    {
        PrimaryCCD.setExposureFailed();
        if (type == SVB_IMG_RGB24)
            free(buffer);
        return true;
    }
    else if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to get data after exposure (%dx%d #%d channels) (%s).", subW, subH, nChannels, Helpers::toString(ret));
        PrimaryCCD.setExposureFailed();
        if (type == SVB_IMG_RGB24)
            free(buffer);
        return false;
    }
    else
    {
        if (PrimaryCCD.getExposureDuration() > VERBOSE_EXPOSURE)
            LOG_INFO("Exposure done, downloading image...");
    }

    if (type == SVB_IMG_RGB24)
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

    if (send)
        sendImage(type, duration);
    return true;
}

void SVBONYBase::sendImage(SVB_IMG_TYPE type, float duration)
{
    PrimaryCCD.setNAxis(type == SVB_IMG_RGB24 ? 3 : 2);

    // If mono camera or we're sending Luma or RGB, turn off bayering
    if (mCameraProperty.IsColorCam == false || type >= SVB_IMG_Y8)
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    else
    {
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
        auto bayerString = getBayerString();
        // Send if different
        if (strcmp(bayerString, BayerT[2].text))
        {
            IUSaveText(&BayerT[2], bayerString);
            IDSetText(&BayerTP, nullptr);
        }
    }

    if (duration > VERBOSE_EXPOSURE)
        LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);
}

/* The timer call back is used for temperature monitoring */
void SVBONYBase::temperatureTimerTimeout()
{
    SVB_ERROR_CODE ret;
    SVB_BOOL isAuto = SVB_FALSE;
    long value = 0;
    IPState newState = TemperatureNP.s;

    ret = SVBGetControlValue(mCameraInfo.CameraID, SVB_CURRENT_TEMPERATURE, &value, &isAuto);

    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to get temperature (%s).", Helpers::toString(ret));
        newState = IPS_ALERT;
    }
    else
    {
        mCurrentTemperature = value / 10.0;
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
        ret = SVBGetControlValue(mCameraInfo.CameraID, SVB_COOLER_POWER, &value, &isAuto);
        if (ret != SVB_SUCCESS)
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

IPState SVBONYBase::guidePulse(INDI::Timer &timer, float ms, SVB_GUIDE_DIRECTION dir)
{
    timer.stop();
    SVBPulseGuide(mCameraInfo.CameraID, dir, ms);

    LOGF_DEBUG("Starting %s guide for %f ms.", Helpers::toString(dir), ms);

    timer.callOnTimeout([this, dir]
    {
        LOGF_DEBUG("Stopped %s guide.", Helpers::toString(dir));

        if (dir == SVB_GUIDE_NORTH || dir == SVB_GUIDE_SOUTH)
            GuideComplete(AXIS_DE);
        else if (dir == SVB_GUIDE_EAST || dir == SVB_GUIDE_WEST)
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


void SVBONYBase::stopGuidePulse(INDI::Timer &timer)
{
    if (timer.isActive())
    {
        timer.stop();
        timer.timeout();
    }
}

IPState SVBONYBase::GuideNorth(uint32_t ms)
{
    return guidePulse(mTimerNS, ms, SVB_GUIDE_NORTH);
}

IPState SVBONYBase::GuideSouth(uint32_t ms)
{
    return guidePulse(mTimerNS, ms, SVB_GUIDE_SOUTH);
}

IPState SVBONYBase::GuideEast(uint32_t ms)
{
    return guidePulse(mTimerWE, ms, SVB_GUIDE_EAST);
}

IPState SVBONYBase::GuideWest(uint32_t ms)
{
    return guidePulse(mTimerWE, ms, SVB_GUIDE_WEST);
}

void SVBONYBase::createControls(int piNumberOfControls)
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
        SVB_ERROR_CODE ret = SVBGetControlCaps(mCameraInfo.CameraID, i++, &cap);
        if (ret != SVB_SUCCESS)
        {
            LOGF_ERROR("Failed to get control information (%s).", Helpers::toString(ret));
            return;
        }

        LOGF_DEBUG("Control #%d: name (%s), Descp (%s), Min (%ld), Max (%ld), Default Value (%ld), IsAutoSupported (%s), "
                   "isWritale (%s) ",
                   i, cap.Name, cap.Description, cap.MinValue, cap.MaxValue,
                   cap.DefaultValue, cap.IsAutoSupported ? "True" : "False",
                   cap.IsWritable ? "True" : "False");

        if (cap.IsWritable == SVB_FALSE || cap.ControlType == SVB_TARGET_TEMPERATURE || cap.ControlType == SVB_COOLER_ENABLE
                || cap.ControlType == SVB_FLIP)
            continue;

        // Update Min/Max exposure as supported by the camera
        if (cap.ControlType == SVB_EXPOSURE)
        {
            double minExp = cap.MinValue / 1000000.0;
            double maxExp = cap.MaxValue / 1000000.0;
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", minExp, maxExp, 1);
            continue;
        }

        long value     = 0;
        SVB_BOOL isAuto = SVB_FALSE;
        SVBGetControlValue(mCameraInfo.CameraID, cap.ControlType, &value, &isAuto);

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
            node.fill(autoName, cap.Name, isAuto == SVB_TRUE ? ISS_ON : ISS_OFF);
            node.setAux(&cap);
            ControlSP.push(std::move(node));
        }
    }

    // Resize the buffers to free up unused space
    ControlNP.shrink_to_fit();
    ControlSP.shrink_to_fit();
}

SVB_IMG_TYPE SVBONYBase::getImageType() const
{
    auto sp = VideoFormatSP.findOnSwitch();
    return sp != nullptr ? *static_cast<SVB_IMG_TYPE *>(sp->getAux()) : SVB_IMG_END;
}

bool SVBONYBase::hasFlipControl()
{
    if (find_if(begin(mControlCaps), end(mControlCaps), [](SVB_CONTROL_CAPS cap)
{
    return cap.ControlType == SVB_FLIP;
}) == end(mControlCaps))
    return false;
    else
        return true;
}

void SVBONYBase::updateControls()
{
    for (auto &num : ControlNP)
    {
        auto numCtrlCap = static_cast<SVB_CONTROL_CAPS *>(num.getAux());
        long value      = 0;
        SVB_BOOL isAuto = SVB_FALSE;
        SVBGetControlValue(mCameraInfo.CameraID, numCtrlCap->ControlType, &value, &isAuto);

        num.setValue(value);

        auto sw = ControlSP.find_if([&numCtrlCap](const INDI::WidgetSwitch & it) -> bool
        {
            return static_cast<const SVB_CONTROL_CAPS *>(it.getAux())->ControlType == numCtrlCap->ControlType;
        });

        if (sw != ControlSP.end())
            sw->setState(isAuto == SVB_TRUE ? ISS_ON : ISS_OFF);
    }

    ControlNP.apply();
    ControlSP.apply();
}

void SVBONYBase::updateRecorderFormat()
{
    mCurrentVideoFormat = getImageType();
    if (mCurrentVideoFormat == SVB_IMG_END)
        return;

    Streamer->setPixelFormat(
        Helpers::pixelFormat(
            mCurrentVideoFormat,
            mCameraProperty.BayerPattern,
            mCameraProperty.IsColorCam
        ),
        mCurrentVideoFormat == SVB_IMG_RAW16 ? 16 : 8
    );
}

void SVBONYBase::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
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

bool SVBONYBase::saveConfigItems(FILE *fp)
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

    return true;
}

bool SVBONYBase::SetCaptureFormat(uint8_t index)
{
    return setVideoFormat(index);
}
