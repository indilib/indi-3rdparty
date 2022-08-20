/*
    INDI LibCamera Base

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

#include "indi_libcamera.h"

#include "config.h"

#include <stream/streammanager.h>
#include <indielapsedtimer.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <map>
#include <unistd.h>

#define CONTROL_TAB "Controls"

static std::unique_ptr<INDILibCamera> m_Camera(new INDILibCamera());

void INDILibCamera::workerStreamVideo(const std::atomic_bool &isAboutToQuit)
{
}

void INDILibCamera::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    StillOptions const *options = GetOptions();
    unsigned int still_flags = LibcameraApp::FLAG_STILL_NONE;
    if (options->encoding == "rgb" || options->encoding == "png")
        still_flags |= LibcameraApp::FLAG_STILL_BGR;
    else if (options->encoding == "bmp")
        still_flags |= LibcameraApp::FLAG_STILL_RGB;
    if (options->raw)
        still_flags |= LibcameraApp::FLAG_STILL_RAW;



}

///////////////////////////////////////////////////////////////////////
/// Generic constructor
///////////////////////////////////////////////////////////////////////
INDILibCamera::INDILibCamera(): LibcameraApp(std::make_unique<StillOptions>())
{
    setVersion(LIBCAMERA_VERSION_MAJOR, LIBCAMERA_VERSION_MINOR);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
INDILibCamera::~INDILibCamera()
{
    if (isConnected())
    {
        Disconnect();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *INDILibCamera::getDefaultName()
{
    return "LibCamera";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::initProperties()
{
    INDI::CCD::initProperties();

    VideoFormatSP.fill(getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0, 3600, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, 4, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, 4, 1, false);

    uint32_t cap = 0;
    cap |= CCD_HAS_BAYER;
    cap |= CCD_CAN_ABORT;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_HAS_STREAMING;

    SetCCDCapability(cap);

    // Add Debug Control.
    addDebugControl();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Setup camera
        setup();
    }
    else
    {

    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::Connect()
{
    try
    {
        OpenCamera();
        return true;
    }
    catch (std::exception &e)
    {
        LOGF_ERROR("Error opening camera: %s", e.what());
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::Disconnect()
{
    try
    {
        CloseCamera();
        return true;
    }
    catch (std::exception &e)
    {
        LOGF_ERROR("Error opening camera: %s", e.what());
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::setup()
{
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::setVideoFormat(uint8_t index)
{
    if (index == VideoFormatSP.findOnSwitchIndex())
        return true;

    VideoFormatSP.reset();
    VideoFormatSP[index].setState(ISS_ON);

    //    switch (getImageType())
    //    {
    //        case ASI_IMG_RAW16:
    //            PrimaryCCD.setBPP(16);
    //            break;

    //        default:
    //            PrimaryCCD.setBPP(8);
    //            break;
    //    }

    // When changing video format, reset frame
    UpdateCCDFrame(0, 0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    updateRecorderFormat();

    VideoFormatSP.setState(IPS_OK);
    VideoFormatSP.apply();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::StartExposure(float duration)
{
    m_Worker.start(std::bind(&INDILibCamera::workerExposure, this, std::placeholders::_1, duration));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::AbortExposure()
{
    LOG_DEBUG("Aborting exposure...");
    m_Worker.quit();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::StartStreaming()
{
    m_Worker.start(std::bind(&INDILibCamera::workerStreamVideo, this, std::placeholders::_1));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::StopStreaming()
{
    m_Worker.quit();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::UpdateCCDFrame(int x, int y, int w, int h)
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

    LOGF_DEBUG("Frame ROI x:%d y:%d w:%d h:%d", subX, subY, subW, subH);


    // Set UNBINNED coords
    //PrimaryCCD.setFrame(x, y, w, h);
    PrimaryCCD.setFrame(subX * binX, subY * binY, subW * binX, subH * binY);

    // Total bytes required for image buffer
    uint32_t nbuf = (subW * subH * static_cast<uint32_t>(PrimaryCCD.getBPP()) / 8);

    LOGF_DEBUG("Setting frame buffer size to %d bytes.", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(subW, subH);

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::UpdateCCDBin(int binx, int biny)
{
    INDI_UNUSED(biny);

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int INDILibCamera::grabImage(float duration)
{
    ExposureComplete(&PrimaryCCD);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::addFITSKeywords(INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(targetChip);

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);


    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::SetCaptureFormat(uint8_t index)
{
    return setVideoFormat(index);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::updateRecorderFormat()
{
    //    mCurrentVideoFormat = getImageType();
    //    if (mCurrentVideoFormat == ASI_IMG_END)
    //        return;

    //    Streamer->setPixelFormat(
    //        Helpers::pixelFormat(
    //            mCurrentVideoFormat,
    //            mCameraInfo.BayerPattern,
    //            mCameraInfo.IsColorCam
    //        ),
    //        mCurrentVideoFormat == ASI_IMG_RAW16 ? 16 : 8
    //    );
}
