/*
    INDI Driver for Kepler sCMOS camera.
    Copyright (C) 2022 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "config.h"
#include "kepler.h"

#include <unistd.h>
#include <memory>
#include <map>
#include <locale>
#include <codecvt>
#include <indielapsedtimer.h>

#define FLI_MAX_SUPPORTED_CAMERAS 4
#define VERBOSE_EXPOSURE          3

/********************************************************************************
*
********************************************************************************/
static class Loader
{
        INDI::Timer hotPlugTimer;
        FPRODEVICEINFO camerasDeviceInfo[FLI_MAX_SUPPORTED_CAMERAS];
        // Serial / Camera Object
        std::map<std::wstring, std::shared_ptr<Kepler>> cameras;
    public:
        Loader()
        {
            load(false);
        }

    public:
        size_t getCountOfConnectedCameras()
        {
            uint32_t detectedCamerasCount = FLI_MAX_SUPPORTED_CAMERAS;
            int32_t result = FPROCam_GetCameraList(camerasDeviceInfo, &detectedCamerasCount);
            return (result >= 0 ? detectedCamerasCount : 0);

        }

    public:
        void load(bool isHotPlug)
        {
            auto usedCameras = std::move(cameras);
            auto detectedCamerasCount = getCountOfConnectedCameras();

            UniqueName uniqueName(usedCameras);

            for(uint32_t i = 0; i < detectedCamerasCount; i++)
            {
                const auto serialID = camerasDeviceInfo[i].cSerialNo;

                // camera already created
                if (usedCameras.find(serialID) != usedCameras.end())
                {
                    std::swap(cameras[serialID], usedCameras[serialID]);
                    continue;
                }

                Kepler *kepler = new Kepler(camerasDeviceInfo[i], uniqueName.make(camerasDeviceInfo[i]));
                cameras[serialID] = std::shared_ptr<Kepler>(kepler);
                if (isHotPlug)
                    kepler->ISGetProperties(nullptr);
            }
        }

    public:
        class UniqueName
        {
                std::map<std::wstring, bool> used;
            public:
                UniqueName() = default;
                explicit UniqueName(const std::map<std::wstring, std::shared_ptr<Kepler>> &usedCameras)
                {

                    for (const auto &camera : usedCameras)
                    {
                        auto name = std::string(camera.second->getDeviceName());
                        auto wname = std::wstring(name.begin(), name.end());
                        used[wname] = true;
                    }
                }

                std::wstring make(const FPRODEVICEINFO &cameraInfo)
                {
                    std::wstring cameraName = std::wstring(L"FLI ") + std::wstring(cameraInfo.cFriendlyName + 4);
                    std::wstring uniqueName = cameraName;

                    for (int index = 0; used[uniqueName] == true; )
                        uniqueName = cameraName + std::wstring(L" ") + std::to_wstring(++index);

                    used[uniqueName] = true;
                    return uniqueName;
                }
        };
} loader;

// Map of device type --> Pixel sizes.
// Pixel sizes 99 means I couldn't find information on them.
std::map<FPRODEVICETYPE, double> Kepler::SensorPixelSize
{
    {FPRO_CAM_DEVICE_TYPE_GSENSE400, 11},
    {FPRO_CAM_DEVICE_TYPE_GSENSE2020, 6.5},
    {FPRO_CAM_DEVICE_TYPE_GSENSE4040, 9},
    {FPRO_CAM_DEVICE_TYPE_GSENSE6060, 10},
    {FPRO_CAM_DEVICE_TYPE_KODAK47051, 99},
    {FPRO_CAM_DEVICE_TYPE_KODAK29050, 99},
    {FPRO_CAM_DEVICE_TYPE_DC230_42, 15},
    {FPRO_CAM_DEVICE_TYPE_DC230_84, 15},
    {FPRO_CAM_DEVICE_TYPE_DC4320, 24},
    {FPRO_CAM_DEVICE_TYPE_SONYIMX183, 2.4},
    {FPRO_CAM_DEVICE_TYPE_FTM, 99}
};

/********************************************************************************
*
********************************************************************************/
void Kepler::workerStreamVideo(const std::atomic_bool &isAboutToQuit)
{

}

/********************************************************************************
*
********************************************************************************/
void Kepler::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    int32_t result = FPROCtrl_SetExposure(m_CameraHandle, duration * 1e9, 0, false);
    if (result != 0)
    {
        LOGF_ERROR("%s: Failed to start exposure: %d", __PRETTY_FUNCTION__, result);
        return;
    }

    PrimaryCCD.setExposureDuration(duration);
    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);

    // Try exposure for 3 times
    for (int i = 0; i < 3; i++)
    {
        result = FPROFrame_CaptureStart(m_CameraHandle, 1);
        if (result == 0)
            break;

        // Wait 100ms before trying again
        usleep(100 * 1000);
    }

    if (result != 0)
    {
        LOGF_ERROR("Failed to start exposure: %d", result);
        return;
    }

    INDI::ElapsedTimer exposureTimer;

    if (duration > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %.2f seconds frame...", duration);

    // Countdown if we have a multi-second exposure.
    // For expsures less than a second, we skip this entirely.
    float timeLeft = std::max(duration - exposureTimer.elapsed() / 1000.0, 0.0);
    while (timeLeft >= 1)
    {
        auto delay = std::max(timeLeft - std::trunc(timeLeft), 0.005f);
        timeLeft = std::round(timeLeft);
        PrimaryCCD.setExposureLeft(timeLeft);
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(delay * 1e6)));
    }

    uint32_t grabSize = PrimaryCCD.getFrameBufferSize();

    // This is blocking?
    std::unique_lock<std::mutex> guard(ccdBufferLock);
    result = FPROFrame_GetVideoFrame(m_CameraHandle, PrimaryCCD.getFrameBuffer(), &grabSize, timeLeft * 1000);

    if (result >= 0)
    {
        PrimaryCCD.setExposureLeft(0.0);
        if (PrimaryCCD.getExposureDuration() > VERBOSE_EXPOSURE)
            LOG_INFO("Exposure done, downloading image...");

        ExposureComplete(&PrimaryCCD);
    }
    else
    {
        PrimaryCCD.setExposureFailed();
        LOGF_ERROR("Failed to grab frame: %d", result);
    }
}

Kepler::Kepler(const FPRODEVICEINFO &info, std::wstring name) : m_CameraInfo(info)
{
    setVersion(FLI_CCD_VERSION_MAJOR, FLI_CCD_VERSION_MINOR);

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
    auto byteName = convert.to_bytes(name);
    setDeviceName(byteName.c_str());
}

Kepler::~Kepler()
{

}

const char *Kepler::getDefaultName()
{
    return "FLI Kepler";
}

bool Kepler::initProperties()
{
    // Initialize parent camera properties.
    INDI::CCD::initProperties();

    // Set Camera capabilities
    SetCCDCapability(CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER);

    // Add capture format
    CaptureFormat mono = {"INDI_MONO", "Mono", 16, true};
    addCaptureFormat(mono);

    // Set exposure range
    // TODO double check this is the supported range.
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

    /*****************************************************************************************************
    // Properties
    ******************************************************************************************************/

    // Communication Method
    CommunicationMethodSP[FPRO_CONNECTION_USB].fill("FPRO_CONNECTION_USB", "USB", ISS_ON);
    CommunicationMethodSP[FPRO_CONNECTION_FIBRE].fill("FPRO_CONNECTION_FIBRE", "Fiber", ISS_OFF);
    CommunicationMethodSP.fill(getDefaultName(), "COMMUNICATION_METHOD", "Connect Via", OPTIONS_TAB, IP_RO, ISR_1OFMANY, 60, IPS_IDLE);

    addAuxControls();

    return true;
}

/********************************************************************************
*
********************************************************************************/
void Kepler::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
    defineProperty(&CommunicationMethodSP);
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {

    }
    else
    {

    }

    return true;
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {

    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {

    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::Connect()
{
    int32_t result = FPROCam_Open(&m_CameraInfo, &m_CameraHandle);
    if ((result >= 0) && (m_CameraHandle >= 0))
    {
        // Different camera models support a different set of capabilities.
        // The API allows you to retrieve the capabilities so that you can obtain
        // images properly and configure your applications accordingly.  In all cases,
        // you need to know the size of the Meta Data supplied by the camera that is
        // prepended to every image.  This size is contained in the capabilities structure.
        m_CameraCapabilitiesSize = sizeof(FPROCAP);
        result = FPROSensor_GetCapabilities(m_CameraHandle, &m_CameraCapabilities, &m_CameraCapabilitiesSize);

        CommunicationMethodSP[FPRO_CONNECTION_USB].setState(m_CameraInfo.eConnType == FPRO_CONNECTION_USB ? ISS_ON : ISS_OFF);
        CommunicationMethodSP[FPRO_CONNECTION_FIBRE].setState(m_CameraInfo.eConnType == FPRO_CONNECTION_FIBRE ? ISS_ON : ISS_OFF);
        CommunicationMethodSP.setState(IPS_OK);
        CommunicationMethodSP.apply();

        LOGF_INFO("Established connection to camera via %s", m_CameraInfo.eConnType == FPRO_CONNECTION_USB ? "USB" : "Fiber");

        return (result == 0);
    }

    LOGF_ERROR("Failed to established connection with the camera: %d", result);
    return false;
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::Disconnect()
{
    return true;
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::establishConnection()
{
    return true;
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::setup()
{
    // We need image data
    FPROFrame_SetImageDataEnable(m_CameraHandle, true);

    uint32_t pixelDepth = 16, pixelLSB = 1;
    int32_t result = FPROFrame_GetPixelConfig(m_CameraHandle, &pixelDepth, &pixelLSB);
    if (result != 0)
    {
        LOGF_ERROR("%s: Failed to query camera pixel depth: %d", __PRETTY_FUNCTION__, result);
        return false;
    }

    auto pixelSize = SensorPixelSize[static_cast<FPRODEVICETYPE>(m_CameraCapabilities.uiDeviceType)];

    if (pixelSize > 90)
        LOG_WARN("Pixel size is unkown for this camera model! Contact INDI to supply correct pixel information.");

    SetCCDParams(m_CameraCapabilities.uiMaxPixelImageWidth, m_CameraCapabilities.uiMaxPixelImageHeight, pixelDepth, pixelSize, pixelSize);

    // Get required frame buffer size including all the metadata and extra bits added by the SDK.
    // We need to only
    uint32_t totalFrameBufferSize = FPROFrame_ComputeFrameSize(m_CameraHandle);
    // This would allocate memory
    PrimaryCCD.setFrameBufferSize(totalFrameBufferSize);
    // This is actual image data size
    uint32_t rawFrameSize = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    // We set it again, but without allocating memory.
    PrimaryCCD.setFrameBufferSize(rawFrameSize, false);

    return true;
}

/********************************************************************************
*
********************************************************************************/
int Kepler::SetTemperature(double temperature)
{
    return 0;
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::StartExposure(float duration)
{
    m_Worker.start(std::bind(&Kepler::workerExposure, this, std::placeholders::_1, duration));
    return true;
}

bool Kepler::AbortExposure()
{
    InExposure = false;
    return true;
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType)
{
    return true;
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::UpdateCCDFrame(int x, int y, int w, int h)
{

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    int nbuf = (w / PrimaryCCD.getBinX()) * (h / PrimaryCCD.getBinY()) * (PrimaryCCD.getBPP() / 8);
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::UpdateCCDBin(int binx, int biny)
{
    PrimaryCCD.setBin(binx, biny);
    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/********************************************************************************
*
********************************************************************************/
void Kepler::TimerHit()
{
    if (isConnected() == false)
        return;

    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:
            break;

        case IPS_BUSY:
            break;

        case IPS_ALERT:
            break;
    }
}

/********************************************************************************
*
********************************************************************************/
bool Kepler::saveConfigItems(FILE * fp)
{
    INDI::CCD::saveConfigItems(fp);
    return true;
}

/********************************************************************************
*
********************************************************************************/
void Kepler::debugTriggered(bool enable)
{
    FPRODebug_EnableLevel(true, enable ? FPRO_DEBUG_DEBUG : FPRO_DEBUG_NONE);
}
