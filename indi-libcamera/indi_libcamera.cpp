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

#include "image/image.hpp"
#include "core/still_options.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <map>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <libraw.h>
#include <jpeglib.h>


#define CONTROL_TAB "Controls"

static std::unique_ptr<INDILibCamera> m_Camera(new INDILibCamera());

void INDILibCamera::shutdownVideo()
{
    m_VideoApp->StopCamera();
    m_VideoApp->StopEncoder();
    m_VideoApp->Teardown();
    //m_VideoApp->CloseCamera();
    Streamer->setStream(false);
}

void INDILibCamera::workerStreamVideo(const std::atomic_bool &isAboutToQuit)
{
    //m_VideoApp.reset(new LibcameraEncoder());

    m_VideoApp->SetEncodeOutputReadyCallback(std::bind(&INDILibCamera::outputReady, this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4));

    VideoOptions* options = m_VideoApp->GetOptions();
    options->nopreview = true;
    options->codec = "mjpeg";
    // TODO add denoise property
    options->denoise = "cdn_off";

    try
    {
        //m_VideoApp->OpenCamera();
        m_VideoApp->ConfigureVideo(LibcameraEncoder::FLAG_VIDEO_JPEG_COLOURSPACE);
        m_VideoApp->StartEncoder();
        m_VideoApp->StartCamera();
    }
    catch (std::exception &e)
    {
        LOGF_ERROR("Error opening camera: %s", e.what());
        shutdownVideo();
        return;
    }

    while (!isAboutToQuit)
    {
        LibcameraEncoder::Msg msg = m_VideoApp->Wait();
        if (msg.type == LibcameraEncoder::MsgType::Quit)
        {
            shutdownVideo();
            return;
        }
        else if (msg.type != LibcameraEncoder::MsgType::RequestComplete)
        {
            LOGF_ERROR("Video Streaming failed: %d", msg.type);
            shutdownVideo();
            return;
        }

        auto completed_request = std::get<CompletedRequestPtr>(msg.payload);
        m_VideoApp->EncodeBuffer(completed_request, m_VideoApp->VideoStream());
    }

    m_VideoApp->StopCamera();
    m_VideoApp->StopEncoder();
    m_VideoApp->Teardown();
    m_VideoApp->CloseCamera();
}

void INDILibCamera::outputReady(void *mem, size_t size, int64_t timestamp_us, bool keyframe)
{
    INDI_UNUSED(timestamp_us);
    INDI_UNUSED(keyframe);
    uint8_t * cameraBuffer = PrimaryCCD.getFrameBuffer();
    size_t cameraBufferSize = 0;
    int w = 0, h = 0, naxis = 0;

    // Read jpeg from memory
    std::unique_lock<std::mutex> ccdguard(ccdBufferLock);

    if (m_LiveVideoWidth <= 0)
    {
        auto info = m_VideoApp->GetStreamInfo(m_VideoApp->VideoStream());
        m_LiveVideoWidth = info.width;
        m_LiveVideoHeight = info.height;
        PrimaryCCD.setBin(1, 1);
        PrimaryCCD.setFrame(0, 0, m_LiveVideoWidth, m_LiveVideoHeight);
        Streamer->setSize(m_LiveVideoWidth, m_LiveVideoHeight);
    }

    if (CaptureFormatSP.findOnSwitchIndex() == CAPTURE_JPG)
    {
        // We are done with writing to CCD buffer
        ccdguard.unlock();
        Streamer->newFrame(static_cast<uint8_t*>(mem), size);
        return;
    }

    int rc = processJPEGMemory(reinterpret_cast<uint8_t*>(mem), size, &cameraBuffer, &cameraBufferSize, &naxis, &w, &h);

    if (rc != 0)
    {
        LOG_ERROR("Error getting live video frame.");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }

    PrimaryCCD.setFrameBuffer(cameraBuffer);
    if (naxis != PrimaryCCD.getNAxis())
    {
        if (naxis == 1)
            Streamer->setPixelFormat(INDI_MONO);

        PrimaryCCD.setNAxis(naxis);
    }
    if (PrimaryCCD.getFrameBufferSize() != static_cast<int>(cameraBufferSize))
        PrimaryCCD.setFrameBufferSize(cameraBufferSize, false);

    // We are done with writing to CCD buffer
    ccdguard.unlock();
    Streamer->newFrame(cameraBuffer, cameraBufferSize);
}

void INDILibCamera::shutdownExposure()
{
    // m_StillApp->StopCamera();
    m_StillApp->Teardown();
    m_StillApp->CloseCamera();
    PrimaryCCD.setExposureFailed();
}

void INDILibCamera::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    //m_StillApp.reset(new LibcameraApp(std::make_unique<StillOptions>()));
    auto options = static_cast<StillOptions *>(m_StillApp->GetOptions());
    //options->Parse(0, nullptr);
    options->nopreview = true;
    options->immediate = true;
    options->encoding = "yuv420";
    options->shutter = duration * 1e6;
    options->denoise = "cdn_off";

    unsigned int still_flags = LibcameraApp::FLAG_STILL_RAW;

    try
    {
        //m_StillApp->OpenCamera();
        m_StillApp->ConfigureStill(still_flags);
        m_StillApp->StartCamera();
    }
    catch (std::exception &e)
    {
        LOGF_ERROR("Error opening camera: %s", e.what());
        shutdownExposure();
        return;
    }

    //auto start_time = std::chrono::high_resolution_clock::now();

    LibcameraApp::Msg msg = m_StillApp->Wait();
    if (msg.type != LibcameraApp::MsgType::RequestComplete)
    {
        PrimaryCCD.setExposureFailed();
        LOGF_ERROR("Exposure failed: %d", msg.type);
        return;
    }
    else if (isAboutToQuit)
        return;

    //auto now = std::chrono::high_resolution_clock::now();

    auto stream = m_StillApp->StillStream();
    auto payload = std::get<CompletedRequestPtr>(msg.payload);
    StreamInfo info = m_StillApp->GetStreamInfo(stream);
    const std::vector<libcamera::Span<uint8_t>> mem = m_StillApp->Mmap(payload->buffers[stream]);

    try
    {
        char filename[MAXINDIFORMAT] {0};

        if (IUFindOnSwitchIndex(&CaptureFormatSP) == CAPTURE_DNG)
        {
            strncpy(filename, "/tmp/output.dng", MAXINDIFORMAT);
            dng_save(mem, info, payload->metadata, filename, m_StillApp->CameraId(), options);
        }
        else
        {
            strncpy(filename, "/tmp/output.jpg", MAXINDIFORMAT);
            jpeg_save(mem, info, payload->metadata, filename, m_StillApp->CameraId(), options);
        }

        char bayer_pattern[8] = {};
        uint8_t * memptr = PrimaryCCD.getFrameBuffer();
        size_t memsize = 0;
        int naxis = 2, w = 0, h = 0, bpp = 8;

        if (EncodeFormatSP[FORMAT_FITS].getState() == ISS_ON)
        {
            if (IUFindOnSwitchIndex(&CaptureFormatSP) == CAPTURE_DNG)
            {
                if (!processRAW(filename, &memptr, &memsize, &naxis, &w, &h, &bpp, bayer_pattern))
                {
                    LOG_ERROR("Exposure failed to parse raw image.");
                    shutdownExposure();
                    unlink(filename);
                    return;
                }

                SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
                IUSaveText(&BayerT[2], bayer_pattern);
                IDSetText(&BayerTP, nullptr);
            }
            else
            {
                if (!processJPEG(filename, &memptr, &memsize, &naxis, &w, &h))
                {
                    LOG_ERROR("Exposure failed to parse jpeg.");
                    shutdownExposure();
                    unlink(filename);
                    return;
                }

                LOGF_DEBUG("read_jpeg: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d)", memsize, naxis, w, h, bpp);

                SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
            }

            PrimaryCCD.setImageExtension("fits");

            uint16_t subW = PrimaryCCD.getSubW();
            uint16_t subH = PrimaryCCD.getSubH();

            // If subframing is requested
            // If either axis is less than the image resolution
            // then we subframe, given the OTHER axis is within range as well.
            if ( (subW > 0 && subH > 0) && ((subW < w && subH <= h) || (subH < h && subW <= w)))
            {

                uint16_t subX = PrimaryCCD.getSubX();
                uint16_t subY = PrimaryCCD.getSubY();

                int subFrameSize     = subW * subH * bpp / 8 * ((naxis == 3) ? 3 : 1);
                int oneFrameSize     = subW * subH * bpp / 8;

                int lineW  = subW * bpp / 8;

                LOGF_DEBUG("Subframing... subFrameSize: %d - oneFrameSize: %d - subX: %d - subY: %d - subW: %d - subH: %d",
                           subFrameSize, oneFrameSize,
                           subX, subY, subW, subH);

                if (naxis == 2)
                {
                    // JM 2020-08-29: Using memmove since regions are overlaping
                    // as proposed by Camiel Severijns on INDI forums.
                    for (int i = subY; i < subY + subH; i++)
                        memmove(memptr + (i - subY) * lineW, memptr + (i * w + subX) * bpp / 8, lineW);
                }
                else
                {
                    uint8_t * subR = memptr;
                    uint8_t * subG = memptr + oneFrameSize;
                    uint8_t * subB = memptr + oneFrameSize * 2;

                    uint8_t * startR = memptr;
                    uint8_t * startG = memptr + (w * h * bpp / 8);
                    uint8_t * startB = memptr + (w * h * bpp / 8 * 2);

                    for (int i = subY; i < subY + subH; i++)
                    {
                        memcpy(subR + (i - subY) * lineW, startR + (i * w + subX) * bpp / 8, lineW);
                        memcpy(subG + (i - subY) * lineW, startG + (i * w + subX) * bpp / 8, lineW);
                        memcpy(subB + (i - subY) * lineW, startB + (i * w + subX) * bpp / 8, lineW);
                    }
                }

                PrimaryCCD.setFrameBuffer(memptr);
                PrimaryCCD.setFrameBufferSize(memsize, false);
                PrimaryCCD.setResolution(w, h);
                PrimaryCCD.setFrame(subX, subY, subW, subH);
                PrimaryCCD.setNAxis(naxis);
                PrimaryCCD.setBPP(bpp);

                // binning if needed
                if(PrimaryCCD.getBinX() > 1)
                    PrimaryCCD.binBayerFrame();
            }
            else
            {
                if (PrimaryCCD.getSubW() != 0 && (w > PrimaryCCD.getSubW() || h > PrimaryCCD.getSubH()))
                    LOGF_WARN("Camera image size (%dx%d) is less than requested size (%d,%d). Purge configuration and update frame size to match camera size.",
                              w, h, PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

                PrimaryCCD.setFrameBuffer(memptr);
                PrimaryCCD.setFrameBufferSize(memsize, false);
                PrimaryCCD.setResolution(w, h);
                PrimaryCCD.setFrame(0, 0, w, h);
                PrimaryCCD.setNAxis(naxis);
                PrimaryCCD.setBPP(bpp);

                // binning if needed
                if(PrimaryCCD.getBinX() > 1)
                    PrimaryCCD.binBayerFrame();
            }
        }
        else
        {
            int fd = open(filename, O_RDONLY);
            struct stat sb;

            // Get file size
            if (fstat(fd, &sb) == -1)
            {
                LOGF_ERROR("Error opening file %s: %s", filename, strerror(errno));
                shutdownExposure();
                close(fd);
                return;
            }

            // Copy file to memory using mmap
            memsize = sb.st_size;
            // Guard CCD Buffer content until we finish copying mmap buffer to it
            std::unique_lock<std::mutex> guard(ccdBufferLock);
            // If CCD Buffer size is different, allocate memory to file size
            if (PrimaryCCD.getFrameBufferSize() != static_cast<int>(memsize))
            {
                PrimaryCCD.setFrameBufferSize(memsize);
                memptr = PrimaryCCD.getFrameBuffer();
            }

            void *mmap_mem = mmap(nullptr, memsize, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mmap_mem == nullptr)
            {
                LOGF_ERROR("Error reading file %s: %s", filename, strerror(errno));
                shutdownExposure();
                close(fd);
                return;
            }

            // Copy mmap buffer to ccd buffer
            memcpy(memptr, mmap_mem, memsize);

            // Release mmap memory
            munmap(mmap_mem, memsize);
            // Close file
            close(fd);
            // Set extension (eg. cr2..etc)
            PrimaryCCD.setImageExtension(strchr(filename, '.') + 1);
            // We are ready to unlock
            guard.unlock();
        }

        ExposureComplete(&PrimaryCCD);

        m_StillApp->StopCamera();
        m_StillApp->Teardown();
        m_StillApp->CloseCamera();
    }
    catch (std::exception &e)
    {
        LOGF_ERROR("Error saving image: %s", e.what());
        shutdownExposure();
    }
}

///////////////////////////////////////////////////////////////////////
/// Generic constructor
///////////////////////////////////////////////////////////////////////
INDILibCamera::INDILibCamera()
{
    setVersion(LIBCAMERA_VERSION_MAJOR, LIBCAMERA_VERSION_MINOR);
    signal(SIGBUS, default_signal_handler);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
INDILibCamera::~INDILibCamera()
{
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

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0, 3600, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, 4, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, 4, 1, false);

    // Cameras
    CameraSP.fill(getDeviceName(), "CAMERAS", "Cameras", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    detectCameras();

    uint32_t cap = 0;
    cap |= CCD_HAS_BAYER;
    cap |= CCD_HAS_STREAMING;
    cap |= CCD_CAN_ABORT;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_CAN_BIN;

    SetCCDCapability(cap);

    // Add Debug Control.
    addDebugControl();

    CaptureFormat dng = {"DNG", "DNG", 8, true};
    CaptureFormat jpg = {"JPG", "JPG", 8, false};

    addCaptureFormat(dng);
    addCaptureFormat(jpg);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
    defineProperty(CameraSP);
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
void INDILibCamera::detectCameras()
{
    std::unique_ptr<LibcameraApp::CameraManager> cameraManager(new LibcameraApp::CameraManager());
    cameraManager->start();
    auto cameras = cameraManager->cameras();
    // Do not show USB webcams as these are not supported in libcamera-apps!
    auto rem = std::remove_if(cameras.begin(), cameras.end(),
                              [](auto & cam)
    {
        return cam->id().find("/usb") != std::string::npos;
    });
    cameras.erase(rem, cameras.end());

    if (cameras.size() == 0)
    {
        LOG_ERROR("No cameras detected.");
        return;
    }

    CameraSP.resize(cameras.size());
    for (size_t i = 0; i < cameras.size(); i++)
        CameraSP[i].fill(cameras[i]->id().c_str(), cameras[i]->id().c_str(), ISS_OFF);

    int onIndex = -1;
    if (IUGetConfigOnSwitchIndex(getDeviceName(), "CAMERAS", &onIndex) == 0)
        CameraSP[onIndex].setState(ISS_ON);
    else
        CameraSP[0].setState(ISS_ON);

    cameraManager->stop();
    cameraManager.reset();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::default_signal_handler(int signal_number)
{
    INDI_UNUSED(signal_number);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::Connect()
{
    try
    {
        m_StillApp.reset(new LibcameraApp(std::make_unique<StillOptions>()));
        m_VideoApp.reset(new LibcameraEncoder());

        auto stillOptions = static_cast<StillOptions *>(m_StillApp->GetOptions());
        stillOptions->Parse(0, nullptr);
        stillOptions->camera = CameraSP.findOnSwitchIndex();

        VideoOptions* videoOptions = m_VideoApp->GetOptions();
        videoOptions->camera = CameraSP.findOnSwitchIndex();

        m_StillApp->OpenCamera();
        m_VideoApp->OpenCamera();

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
    return true;
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
bool INDILibCamera::ISNewSwitch(const char *dev, const char *name, ISState * states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (CameraSP.isNameMatch(name))
        {
            CameraSP.update(states, names, n);
            CameraSP.setState(IPS_OK);
            CameraSP.apply();
            saveConfig(CameraSP);
            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}


/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::StartExposure(float duration)
{
    Streamer->setPixelFormat(CaptureFormatSP.findOnSwitchIndex() == CAPTURE_JPG ? INDI_JPG : INDI_RGB);
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
void INDILibCamera::addFITSKeywords(INDI::CCDChip * targetChip)
{
    INDI::CCD::addFITSKeywords(targetChip);

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::saveConfigItems(FILE * fp)
{
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &CameraSP);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::SetCaptureFormat(uint8_t index)
{
    if (index == CAPTURE_DNG)
    {
        Streamer->setPixelFormat(INDI_RGB);
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
    }
    else
    {
        Streamer->setPixelFormat(INDI_JPG);
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::processRAW(const char *filename, uint8_t **memptr, size_t *memsize, int *n_axis, int *w, int *h,
                               int *bitsperpixel,
                               char *bayer_pattern)
{
    int ret = 0;
    // Creation of image processing object
    LibRaw RawProcessor;

    // Let us open the file
    if ((ret = RawProcessor.open_file(filename)) != LIBRAW_SUCCESS)
    {
        LOGF_ERROR("Cannot open %s: %s", filename, libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    // Let us unpack the image
    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
        LOGF_ERROR( "Cannot unpack %s: %s", filename, libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    // Covert to image
    if ((ret = RawProcessor.raw2image()) != LIBRAW_SUCCESS)
    {
        LOGF_ERROR("Cannot convert %s : %s", filename, libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    *n_axis       = 2;
    *w            = RawProcessor.imgdata.rawdata.sizes.width;
    *h            = RawProcessor.imgdata.rawdata.sizes.height;
    *bitsperpixel = 16;
    // cdesc contains counter-clock wise e.g. RGBG CFA pattern while we want it sequential as RGGB
    bayer_pattern[0] = RawProcessor.imgdata.idata.cdesc[RawProcessor.COLOR(0, 0)];
    bayer_pattern[1] = RawProcessor.imgdata.idata.cdesc[RawProcessor.COLOR(0, 1)];
    bayer_pattern[2] = RawProcessor.imgdata.idata.cdesc[RawProcessor.COLOR(1, 0)];
    bayer_pattern[3] = RawProcessor.imgdata.idata.cdesc[RawProcessor.COLOR(1, 1)];
    bayer_pattern[4] = '\0';

    int first_visible_pixel = RawProcessor.imgdata.rawdata.sizes.raw_width * RawProcessor.imgdata.sizes.top_margin +
                              RawProcessor.imgdata.sizes.left_margin;

    LOGF_DEBUG("read_libraw: raw_width: %d top_margin %d left_margin %d first_visible_pixel %d",
               RawProcessor.imgdata.rawdata.sizes.raw_width, RawProcessor.imgdata.sizes.top_margin,
               RawProcessor.imgdata.sizes.left_margin, first_visible_pixel);

    *memsize = RawProcessor.imgdata.rawdata.sizes.width * RawProcessor.imgdata.rawdata.sizes.height * sizeof(uint16_t);
    *memptr  = static_cast<uint8_t *>(IDSharedBlobRealloc(*memptr, *memsize));
    if (*memptr == nullptr)
        *memptr = static_cast<uint8_t *>(IDSharedBlobAlloc(*memsize));
    if (*memptr == nullptr)
    {
        LOGF_ERROR("%s: Failed to allocate %d bytes of memory!", __PRETTY_FUNCTION__, *memsize);
        return false;
    }

    LOGF_DEBUG("read_libraw: rawdata.sizes.width: %d rawdata.sizes.height %d memsize %d bayer_pattern %s",
               RawProcessor.imgdata.rawdata.sizes.width, RawProcessor.imgdata.rawdata.sizes.height, *memsize,
               bayer_pattern);

    uint16_t *image = reinterpret_cast<uint16_t *>(*memptr);
    uint16_t *src   = RawProcessor.imgdata.rawdata.raw_image + first_visible_pixel;

    for (int i = 0; i < RawProcessor.imgdata.rawdata.sizes.height; i++)
    {
        memcpy(image, src, RawProcessor.imgdata.rawdata.sizes.width * 2);
        image += RawProcessor.imgdata.rawdata.sizes.width;
        src += RawProcessor.imgdata.rawdata.sizes.raw_width;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::processJPEG(const char *filename, uint8_t **memptr, size_t *memsize, int *naxis, int *w, int *h)
{
    unsigned char *r_data = nullptr, *g_data = nullptr, *b_data = nullptr;

    /* these are standard libjpeg structures for reading(decompression) */
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    /* libjpeg data structure for storing one row, that is, scanline of an image */
    JSAMPROW row_pointer[1] = { nullptr };

    FILE *infile = fopen(filename, "rb");

    if (!infile)
    {
        LOGF_DEBUG("Error opening jpeg file %s!", filename);
        return false;
    }
    /* here we set up the standard libjpeg error handler */
    cinfo.err = jpeg_std_error(&jerr);
    /* setup decompression process and source, then read JPEG header */
    jpeg_create_decompress(&cinfo);
    /* this makes the library read from infile */
    jpeg_stdio_src(&cinfo, infile);
    /* reading the image header which contains image information */
    jpeg_read_header(&cinfo, (boolean)TRUE);

    /* Start decompression jpeg here */
    jpeg_start_decompress(&cinfo);

    *memsize = cinfo.output_width * cinfo.output_height * cinfo.num_components;
    *memptr  = static_cast<uint8_t *>(IDSharedBlobRealloc(*memptr, *memsize));
    if (*memptr == nullptr)
        *memptr = static_cast<uint8_t *>(IDSharedBlobAlloc(*memsize));
    if (*memptr == nullptr)
    {
        LOGF_ERROR("%s: Failed to allocate %d bytes of memory!", __PRETTY_FUNCTION__, *memsize);
        return false;
    }
    // if you do some ugly pointer math, remember to restore the original pointer or some random crashes will happen. This is why I do not like pointers!!
    uint8_t *oldmem = *memptr;
    *naxis = cinfo.num_components;
    *w     = cinfo.output_width;
    *h     = cinfo.output_height;

    /* now actually read the jpeg into the raw buffer */
    row_pointer[0] = (unsigned char *)malloc(cinfo.output_width * cinfo.num_components);
    if (cinfo.num_components)
    {
        r_data = (unsigned char *)*memptr;
        g_data = r_data + cinfo.output_width * cinfo.output_height;
        b_data = r_data + 2 * cinfo.output_width * cinfo.output_height;
    }
    /* read one scan line at a time */
    for (unsigned int row = 0; row < cinfo.image_height; row++)
    {
        unsigned char *ppm8 = row_pointer[0];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);

        if (cinfo.num_components == 3)
        {
            for (unsigned int i = 0; i < cinfo.output_width; i++)
            {
                *r_data++ = *ppm8++;
                *g_data++ = *ppm8++;
                *b_data++ = *ppm8++;
            }
        }
        else
        {
            memcpy(*memptr, ppm8, cinfo.output_width);
            *memptr += cinfo.output_width;
        }
    }

    /* wrap up decompression, destroy objects, free pointers and close open files */
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    if (row_pointer[0])
        free(row_pointer[0]);
    if (infile)
        fclose(infile);

    *memptr = oldmem;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int INDILibCamera::processJPEGMemory(unsigned char *inBuffer, unsigned long inSize, uint8_t **memptr, size_t *memsize,
                                     int *naxis, int *w,
                                     int *h)
{
    /* these are standard libjpeg structures for reading(decompression) */
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    /* libjpeg data structure for storing one row, that is, scanline of an image */
    JSAMPROW row_pointer[1] = { nullptr };

    /* here we set up the standard libjpeg error handler */
    cinfo.err = jpeg_std_error(&jerr);
    /* setup decompression process and source, then read JPEG header */
    jpeg_create_decompress(&cinfo);
    /* this makes the library read from infile */
    jpeg_mem_src(&cinfo, inBuffer, inSize);

    /* reading the image header which contains image information */
    jpeg_read_header(&cinfo, (boolean)TRUE);

    /* Start decompression jpeg here */
    jpeg_start_decompress(&cinfo);

    *memsize = cinfo.output_width * cinfo.output_height * cinfo.num_components;
    *memptr  = static_cast<uint8_t *>(IDSharedBlobRealloc(*memptr, *memsize));
    if (*memptr == nullptr)
        *memptr = static_cast<uint8_t *>(IDSharedBlobAlloc(*memsize));
    if (*memptr == nullptr)
    {
        LOGF_ERROR("%s: Failed to allocate %d bytes of memory!", __PRETTY_FUNCTION__, *memsize);
        return -1;
    }

    uint8_t *destmem = *memptr;

    *naxis = cinfo.num_components;
    *w     = cinfo.output_width;
    *h     = cinfo.output_height;

    /* now actually read the jpeg into the raw buffer */
    row_pointer[0] = (unsigned char *)malloc(cinfo.output_width * cinfo.num_components);

    /* read one scan line at a time */
    for (unsigned int row = 0; row < cinfo.image_height; row++)
    {
        unsigned char *ppm8 = row_pointer[0];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        memcpy(destmem, ppm8, cinfo.output_width * cinfo.num_components);
        destmem += cinfo.output_width * cinfo.num_components;
    }

    /* wrap up decompression, destroy objects, free pointers and close open files */
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    if (row_pointer[0])
        free(row_pointer[0]);

    return 0;
}
