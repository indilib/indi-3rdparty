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
#include <sharedblob.h>

#include "image/image.hpp"
#include "core/still_options.hpp"
#include "core/rpicam_encoder.hpp"
#include "output/output.hpp"

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

static class Loader
{
        std::map<int, std::shared_ptr<INDILibCamera>> cameras;
    public:
        Loader()
        {
            load();
        }

    public:
    public:
        void load()
        {
            RPiCamINDIApp app;
            int argc = 0;
            char *argv[] = {};
            auto options = app.GetOptions();
            if (options->Parse(argc, argv))
            {
                auto new_cameras = app.GetCameras();

                if (new_cameras.size() == 0)
                {
                    IDLog("No cameras detected.");
                    return;
                }

                for (size_t i = 0; i < new_cameras.size(); i++)
                {
                    auto newCamera = new INDILibCamera(i, new_cameras[i]->properties());
                    cameras[i] = std::shared_ptr<INDILibCamera>(newCamera);
                }
            }
        }

} loader;

///////////////////////////////////////////////////////////////////////
/// Generic constructor
///////////////////////////////////////////////////////////////////////
INDILibCamera::INDILibCamera(uint8_t index, const libcamera::ControlList &list) : m_CameraIndex(index), m_ControlList(list)
{
    setVersion(LIBCAMERA_VERSION_MAJOR, LIBCAMERA_VERSION_MINOR);
    signal(SIGBUS, default_signal_handler);
    auto fullName = std::string("LibCamera ") + list.get(properties::Model).value() + "-" + std::to_string(index);
    setDeviceName(fullName.c_str());
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
void INDILibCamera::shutdownVideo()
{
    Streamer->setStream(false);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int INDILibCamera::getColorspaceFlags(std::string const &codec)
{
    if (codec == "mjpeg" || codec == "yuv420")
        return RPiCamEncoder::FLAG_VIDEO_JPEG_COLOURSPACE;
    else
        return RPiCamEncoder::FLAG_VIDEO_NONE;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::workerStreamVideo(const std::atomic_bool &isAboutToQuit, double framerate)
{
    RPiCamEncoder app;
    auto options = app.GetOptions();
    configureVideoOptions(options, framerate);
    std::unique_ptr<Output> output = std::unique_ptr<Output>(Output::Create(options));
    app.SetEncodeOutputReadyCallback(std::bind(&INDILibCamera::outputReady, this, std::placeholders::_1, std::placeholders::_2,
                                     std::placeholders::_3, std::placeholders::_4));
    app.SetMetadataReadyCallback(std::bind(&INDILibCamera::metadataReady, this, std::placeholders::_1));

    try
    {
        app.OpenCamera();
        app.ConfigureVideo(getColorspaceFlags(options->codec));
        app.StartEncoder();
        app.StartCamera();
    }
    catch (std::exception &e)
    {
        LOGF_ERROR("Error opening camera: %s", e.what());
        shutdownVideo();
        return;
    }

    if (m_LiveVideoWidth <= 0)
    {
        m_LiveVideoWidth = PrimaryCCD.getSubW();
        m_LiveVideoHeight = PrimaryCCD.getSubH();
        PrimaryCCD.setBin(1, 1);
        PrimaryCCD.setFrame(0, 0, m_LiveVideoWidth, m_LiveVideoHeight);
        Streamer->setPixelFormat(INDI_JPG);
        Streamer->setSize(m_LiveVideoWidth, m_LiveVideoHeight);
    }

    while (!isAboutToQuit)
    {
        RPiCamEncoder::Msg msg = app.Wait();

        if (msg.type == RPiCamApp::MsgType::Timeout)
        {
            LOG_WARN("Device timeout detected, attempting a restart!");
            app.StopCamera();
            app.StartCamera();
            continue;
        }
        else if (msg.type == RPiCamEncoder::MsgType::Quit)
        {
            return;
        }
        else if (msg.type != RPiCamEncoder::MsgType::RequestComplete)
        {
            LOGF_ERROR("Video Streaming failed: %d", msg.type);
            shutdownVideo();
            return;
        }

        CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);
        //auto completed_request = std::get<CompletedRequestPtr>(msg.payload);
        app.EncodeBuffer(completed_request, app.VideoStream());
    }

    app.StopCamera();
    app.StopEncoder();
    app.Teardown();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::outputReady(void *mem, size_t size, int64_t timestamp_us, bool keyframe)
{
    INDI_UNUSED(timestamp_us);

    if (!keyframe)
        return;

    // Read buffer from memory
    std::unique_lock<std::mutex> ccdguard(ccdBufferLock);

    Streamer->newFrame(static_cast<uint8_t*>(mem), size);

    // We are done with writing to CCD buffer
    ccdguard.unlock();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::metadataReady(libcamera::ControlList &metadata)
{
    // TODO could this metadata be useful?
    INDI_UNUSED(metadata);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    RPiCamINDIApp app;
    auto options = app.GetOptions();
    configureStillOptions(options, duration);
    unsigned int still_flags = RPiCamApp::FLAG_STILL_RAW;

    try
    {
        app.OpenCamera();
        app.ConfigureStill(still_flags);
        app.StartCamera();
    }
    catch (std::exception &e)
    {
        LOGF_ERROR("Error opening camera: %s", e.what());
        PrimaryCCD.setExposureFailed();
        app.StopCamera();
        app.Teardown();
        app.CloseCamera();
    }

    RPiCamApp::Msg msg = app.Wait();
    if (msg.type != RPiCamApp::MsgType::RequestComplete)
    {
        PrimaryCCD.setExposureFailed();
        app.StopCamera();
        app.Teardown();
        app.CloseCamera();
        LOGF_ERROR("Exposure failed: %d", msg.type);
        return;
    }
    else if (isAboutToQuit)
    {
        app.StopCamera();
        app.Teardown();
        app.CloseCamera();
        return;
    }

    bool raw = CaptureFormatSP.findOnSwitchIndex() == CAPTURE_DNG;
    auto stream = raw ? app.RawStream() : app.StillStream();
    auto payload = std::get<CompletedRequestPtr>(msg.payload);
    StreamInfo info = app.GetStreamInfo(stream);
    BufferReadSync r(&app, payload->buffers[stream]);
    const std::vector<libcamera::Span<uint8_t>> mem = r.Get();

    try
    {
        char filename[MAXINDIFORMAT] {0};

        if (raw)
        {
            strncpy(filename, "/tmp/output.dng", MAXINDIFORMAT);
            dng_save(mem, info, payload->metadata, filename, app.CameraId(), options);
        }
        else
        {
            strncpy(filename, "/tmp/output.jpg", MAXINDIFORMAT);
            jpeg_save(mem, info, payload->metadata, filename, app.CameraId(), options);
        }

        char bayer_pattern[8] = {};
        uint8_t * memptr = PrimaryCCD.getFrameBuffer();
        size_t memsize = 0;
        int naxis = 2, w = 0, h = 0, bpp = 8;

        if (EncodeFormatSP[FORMAT_FITS].getState() == ISS_ON)
        {
            if (CaptureFormatSP.findOnSwitchIndex() == CAPTURE_DNG)
            {
                if (!processRAW(filename, &memptr, &memsize, &naxis, &w, &h, &bpp, bayer_pattern))
                {
                    LOG_ERROR("Exposure failed to parse raw image.");
                    PrimaryCCD.setExposureFailed();
                    app.StopCamera();
                    app.Teardown();
                    app.CloseCamera();
                    unlink(filename);
                    return;
                }

                SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
                BayerTP[2].setText(bayer_pattern);
                BayerTP.apply();
            }
            else
            {
                if (!processJPEG(filename, &memptr, &memsize, &naxis, &w, &h))
                {
                    LOG_ERROR("Exposure failed to parse jpeg.");
                    PrimaryCCD.setExposureFailed();
                    app.StopCamera();
                    app.Teardown();
                    app.CloseCamera();
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
                PrimaryCCD.setExposureFailed();
                app.StopCamera();
                app.Teardown();
                app.CloseCamera();
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
            // mmap crashes randomly for some reason
            if(0)
            {

                void *mmap_mem = mmap(nullptr, memsize, PROT_READ, MAP_PRIVATE, fd, 0);
                if (mmap_mem == nullptr)
                {
                    LOGF_ERROR("Error reading file %s: %s", filename, strerror(errno));
                    PrimaryCCD.setExposureFailed();
                    app.StopCamera();
                    app.Teardown();
                    app.CloseCamera();
                    close(fd);
                    return;
                }

                // Copy mmap buffer to ccd buffer
                memcpy(memptr, mmap_mem, memsize);

                // Release mmap memory
                munmap(mmap_mem, memsize);
            }
            else
            {
                read(fd, memptr, memsize);
            }
            // Close file
            close(fd);
            // Set extension (eg. cr2..etc)
            PrimaryCCD.setImageExtension(strchr(filename, '.') + 1);
            // We are ready to unlock
            guard.unlock();
        }

        ExposureComplete(&PrimaryCCD);
    }
    catch (std::exception &e)
    {
        LOGF_ERROR("Error saving image: %s", e.what());
        PrimaryCCD.setExposureFailed();
    }

    app.StopCamera();
    app.Teardown();
    app.CloseCamera();
}

/*
Adjustments:
Brightness : [-1.000000..1.000000]
Contrast : [0.000000..32.000000]
Saturation : [0.000000..32.000000]
ColourGains : [0.000000..32.000000]
AnalogueGain : [1.000000..31.622776]
Sharpness : [0.000000..16.000000]

Exposure:
ExposureTime : [14..7229147]
FrameDurationLimits : [16666..7230033]
ExposureValue : [-8.000000..8.000000]

ScalerCrop : [(0, 0)/64x64..(0, 0)/1920x1080]

NoiseReductionMode : [0..4]
AwbMode : [0..7]
AwbEnable : [false..true]
ColourCorrectionMatrix : [-16.000000..16.000000]

Auto Exposure:
AeEnable : [false..true]
AeMeteringMode : [0..3]
AeExposureMode : [0..3]
AeConstraintMode : [0..3]
*/

/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::initSwitch(INDI::PropertySwitch &switchSP, int n, const char **names)
{

    switchSP.resize(n);
    for (int i = 0; i < n; i++)
    {
        switchSP[i].fill(names[i], names[i], ISS_OFF);
    }

    int onIndex = -1;
    if (IUGetConfigOnSwitchIndex(getDeviceName(), "CAMERAS", &onIndex) == 0)
        switchSP[onIndex].setState(ISS_ON);
    else
        switchSP[0].setState(ISS_ON);

}

bool INDILibCamera::initProperties()
{
    INDI::CCD::initProperties();

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0, 3600, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, 4, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, 4, 1, false);

    const char *IMAGE_CONTROLS_TAB = MAIN_CONTROL_TAB;
    AdjustExposureModeSP.fill(getDeviceName(), "ExposureMode", "Exposure Mode", IMAGE_CONTROLS_TAB, IP_RW, ISR_1OFMANY, 60,
                              IPS_IDLE);
    const char *exposureModes[] = {"normal", "sport", "short", "long", "custom"};
    initSwitch(AdjustExposureModeSP, 5, exposureModes);

    AdjustAwbModeSP.fill(getDeviceName(), "AwbMode", "Awb Mode", IMAGE_CONTROLS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    const char *awbModes[] = { "auto", "normal", "incandescent", "tungsten", "fluorescent", "indoor", "daylight", "cloudy", "custom"};
    initSwitch(AdjustAwbModeSP, 9, awbModes);

    AdjustMeteringModeSP.fill(getDeviceName(), "MeteringMode", "Metering Mode", IMAGE_CONTROLS_TAB, IP_RW, ISR_1OFMANY, 60,
                              IPS_IDLE);
    const char *meteringModes[] = { "centre", "spot", "average", "matrix", "custom"};
    initSwitch(AdjustMeteringModeSP, 5, meteringModes);

    AdjustDenoiseModeSP.fill(getDeviceName(), "DenoiseMode", "Denoise Mode", IMAGE_CONTROLS_TAB, IP_RW, ISR_1OFMANY, 60,
                             IPS_IDLE);
    const char *denoiseModes[] = { "off", "cdn_off", "cdn_fast", "cdn_hq"};
    initSwitch(AdjustDenoiseModeSP, 4, denoiseModes);

    AdjustmentNP[AdjustBrightness].fill("Brightness", "Brightness", "%.2f", -1.00, 1.00, 0.1, 0.00);
    AdjustmentNP[AdjustContrast].fill("Contrast", "Contrast", "%.2f", 0.00, 2.00, 0.1, 1.00);
    AdjustmentNP[AdjustSaturation].fill("Saturation", "Saturation", "%.2f", 0.00, 1.00, 0.1, 1.00);
    AdjustmentNP[AdjustSharpness].fill("Sharpness", "Sharpness", "%.2f", 0.00, 16.00, 1.00, 1.00);
    AdjustmentNP[AdjustQuality].fill("Quality", "Quality", "%.2f", 0.00, 100.00, 1.00, 100.00);
    AdjustmentNP[AdjustExposureValue].fill("ExposureValue", "Exposure Value", "%.2f", -8.00, 8.00, .25, 0.00);
    AdjustmentNP[AdjustAwbRed].fill("AwbRed", "AWB Red", "%.2f", 0.00, 2.00, .1, 0.00);
    AdjustmentNP[AdjustAwbBlue].fill("AwbBlue", "AWB Blue", "%.2f", 0.00, 2.00, .1, 0.00);
    AdjustmentNP.fill(getDeviceName(), "Adjustments", "Adjustments", IMAGE_CONTROLS_TAB, IP_RW, 60, IPS_IDLE);

    GainNP[0].fill("GAIN", "Gain", "%.2f", 0.00, 100.00, 1.00, 0.00);
    GainNP.fill(getDeviceName(), "CCD_GAIN", "Gain", IMAGE_CONTROLS_TAB, IP_RW, 60, IPS_IDLE);

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
bool INDILibCamera::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Setup camera
        setup();

        defineProperty(AdjustmentNP);
        defineProperty(GainNP);
        defineProperty(AdjustExposureModeSP);
        defineProperty(AdjustAwbModeSP);
        defineProperty(AdjustMeteringModeSP);
        defineProperty(AdjustDenoiseModeSP);
    }
    else
    {
        deleteProperty(AdjustmentNP);
        deleteProperty(GainNP);
        deleteProperty(AdjustExposureModeSP);
        deleteProperty(AdjustAwbModeSP);
        deleteProperty(AdjustMeteringModeSP);
        deleteProperty(AdjustDenoiseModeSP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::configureStillOptions(StillOptions *options, double duration)
{
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(duration * 1000000));
    TimeVal<std::chrono::microseconds> tv;
    tv.set(std::to_string(duration) + "s");

    int argc = 0;
    char *argv[] = {};
    options->Parse(argc, argv);

    options->camera = m_CameraIndex;
    options->nopreview = true;
    options->immediate = true;
    options->quality = 100;
    options->restart = true;
    options->thumb_quality = 0;
    options->shutter = tv;

    options->brightness = AdjustmentNP[AdjustBrightness].getValue();
    options->contrast = AdjustmentNP[AdjustContrast].getValue();
    options->saturation = AdjustmentNP[AdjustSaturation].getValue();
    options->sharpness = AdjustmentNP[AdjustSharpness].getValue();
    options->quality = AdjustmentNP[AdjustQuality].getValue();
    options->ev = AdjustmentNP[AdjustExposureValue].getValue();
    options->awb_gain_r = AdjustmentNP[AdjustAwbRed].getValue();
    options->awb_gain_b = AdjustmentNP[AdjustAwbBlue].getValue();

    options->gain = GainNP[0].getValue();

    options->exposure_index = AdjustExposureModeSP.findOnSwitchIndex();
    options->awb_index = AdjustAwbModeSP.findOnSwitchIndex();
    options->metering_index = AdjustMeteringModeSP.findOnSwitchIndex();
    options->denoise = AdjustDenoiseModeSP.findOnSwitch()->getName();

    options->width = PrimaryCCD.getSubW();
    options->height = PrimaryCCD.getSubH();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::configureVideoOptions(VideoOptions *options, double framerate)
{
    int argc = 0;
    INDI_UNUSED(framerate);
    char *argv[] = {};
    options->Parse(argc, argv);

    options->camera = m_CameraIndex;
    options->nopreview = true;

    options->codec = "mjpeg";
    options->brightness = AdjustmentNP[AdjustBrightness].getValue();
    options->contrast = AdjustmentNP[AdjustContrast].getValue();
    options->saturation = AdjustmentNP[AdjustSaturation].getValue();
    options->sharpness = AdjustmentNP[AdjustSharpness].getValue();
    options->quality = AdjustmentNP[AdjustQuality].getValue();
    options->ev = AdjustmentNP[AdjustExposureValue].getValue();
    options->awb_gain_r = AdjustmentNP[AdjustAwbRed].getValue();
    options->awb_gain_b = AdjustmentNP[AdjustAwbBlue].getValue();

    options->gain = GainNP[0].getValue();

    options->exposure_index = AdjustExposureModeSP.findOnSwitchIndex();
    options->awb_index = AdjustAwbModeSP.findOnSwitchIndex();
    options->metering_index = AdjustMeteringModeSP.findOnSwitchIndex();
    options->denoise = AdjustDenoiseModeSP.findOnSwitch()->getName();
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

    auto pas = m_ControlList.get(properties::PixelArraySize);
    // no idea why the IMX290 returns an uneven number of pixels, so just round down
    auto width = 2.0 * (pas->width / 2);
    auto height = pas->height;

    PrimaryCCD.setResolution(width, height);
    UpdateCCDFrame(0, 0, width, height);

    auto ucs = m_ControlList.get(properties::UnitCellSize);
    auto ucsWidth = ucs->width / 1000.0;
    auto ucsHeight = ucs->height / 1000.0;
    PrimaryCCD.setPixelSize(ucsWidth, ucsHeight);
    PrimaryCCD.setBPP(8);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::Disconnect()
{
    m_Worker.quit();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void INDILibCamera::setup()
{
    /* TODO: use these to fill out the controls
        ControlList controls_ = m_CameraApp.get().GetControls();
        if (!controls_.get(controls::ExposureTime) && options_->shutter)
                controls_.set(controls::ExposureTime, options_->shutter);
        if (!controls_.get(controls::AnalogueGain) && options_->gain)
                controls_.set(controls::AnalogueGain, options_->gain);
        if (!controls_.get(controls::AeMeteringMode))
                controls_.set(controls::AeMeteringMode, options_->metering_index);
        if (!controls_.get(controls::AeExposureMode))
                controls_.set(controls::AeExposureMode, options_->exposure_index);
        if (!controls_.get(controls::ExposureValue))
                controls_.set(controls::ExposureValue, options_->ev);
        if (!controls_.get(controls::AwbMode))
                controls_.set(controls::AwbMode, options_->awb_index);
        if (!controls_.get(controls::ColourGains) && options_->awb_gain_r && options_->awb_gain_b)
                controls_.set(controls::ColourGains,  libcamera::Span<const float, 2>({ options_->awb_gain_r, options_->awb_gain_b }));
        if (!controls_.get(controls::Brightness))
                controls_.set(controls::Brightness, options_->brightness);
        if (!controls_.get(controls::Contrast))
                controls_.set(controls::Contrast, options_->contrast);
        if (!controls_.get(controls::Saturation))
                controls_.set(controls::Saturation, options_->saturation);
        if (!controls_.get(controls::Sharpness))
                controls_.set(controls::Sharpness, options_->sharpness);
    */
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (AdjustmentNP.isNameMatch(name))
        {
            AdjustmentNP.update(values, names, n);
            AdjustmentNP.setState(IPS_OK);
            AdjustmentNP.apply();
            saveConfig(AdjustmentNP);



            return true;
        }
        if (GainNP.isNameMatch(name))
        {
            GainNP.update(values, names, n);
            GainNP.setState(IPS_OK);
            GainNP.apply();
            saveConfig(GainNP);
            return true;
        }
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
        // Adjust Exposure Mode
        if (AdjustExposureModeSP.isNameMatch(name))
        {
            AdjustExposureModeSP.update(states, names, n);
            AdjustExposureModeSP.setState(IPS_OK);
            AdjustExposureModeSP.apply();
            saveConfig(AdjustExposureModeSP);
            return true;
        }

        // Adjust AWB Mode
        if (AdjustAwbModeSP.isNameMatch(name))
        {
            AdjustAwbModeSP.update(states, names, n);
            AdjustAwbModeSP.setState(IPS_OK);
            AdjustAwbModeSP.apply();
            saveConfig(AdjustAwbModeSP);
            return true;
        }

        // Adjust Metering MOde
        if (AdjustMeteringModeSP.isNameMatch(name))
        {
            AdjustMeteringModeSP.update(states, names, n);
            AdjustMeteringModeSP.setState(IPS_OK);
            AdjustMeteringModeSP.apply();
            saveConfig(AdjustMeteringModeSP);
            return true;
        }

        // Denoise
        if (AdjustDenoiseModeSP.isNameMatch(name))
        {
            AdjustDenoiseModeSP.update(states, names, n);
            AdjustDenoiseModeSP.setState(IPS_OK);
            AdjustDenoiseModeSP.apply();
            saveConfig(AdjustDenoiseModeSP);
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
    // do something dynamic here
    double framerate = Streamer.get()->getTargetFPS();
    m_Worker.start(std::bind(&INDILibCamera::workerStreamVideo, this, std::placeholders::_1, framerate));
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

    LOGF_INFO("Frame ROI x:%d y:%d w:%d h:%d", subX, subY, subW, subH);


    // Set UNBINNED coords
    //PrimaryCCD.setFrame(x, y, w, h);
    PrimaryCCD.setFrame(subX * binX, subY * binY, subW * binX, subH * binY);

    // Total bytes required for image buffer
    uint32_t nbuf = (subW * subH * static_cast<uint32_t>(PrimaryCCD.getBPP()) / 8);

    LOGF_INFO("Setting frame buffer size to %d bytes.", nbuf);
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
void INDILibCamera::addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);
    // TODO Add Gain
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool INDILibCamera::saveConfigItems(FILE * fp)
{
    INDI::CCD::saveConfigItems(fp);


    AdjustmentNP.save(fp);
    GainNP.save(fp);
    AdjustExposureModeSP.save(fp);
    AdjustAwbModeSP.save(fp);
    AdjustMeteringModeSP.save(fp);
    AdjustDenoiseModeSP.save(fp);

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
bool INDILibCamera::processRAWMemory(unsigned char *inBuffer, unsigned long inSize, uint8_t **memptr, size_t *memsize,
                                     int *n_axis, int *w, int *h,
                                     int *bitsperpixel, char *bayer_pattern)
{
    int ret = 0;
    // Creation of image processing object
    LibRaw RawProcessor;

    // Let us open the file
    if ((ret = RawProcessor.open_buffer(inBuffer, inSize)) != LIBRAW_SUCCESS)
    {
        LOGF_ERROR("Cannot open buffer %s", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    // Let us unpack the image
    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
        LOGF_ERROR( "Cannot unpack buffer %s", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    // Covert to image
    if ((ret = RawProcessor.raw2image()) != LIBRAW_SUCCESS)
    {
        LOGF_ERROR("Cannot convert %s", libraw_strerror(ret));
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

    if (m_LiveVideoWidth <= 0)
    {
        m_LiveVideoWidth = *w;
        m_LiveVideoHeight = *h;
        PrimaryCCD.setBin(1, 1);
        PrimaryCCD.setFrame(0, 0, m_LiveVideoWidth, m_LiveVideoHeight);
        Streamer->setSize(m_LiveVideoWidth, m_LiveVideoHeight);
    }

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

    if (m_LiveVideoWidth <= 0)
    {
        m_LiveVideoWidth = cinfo.output_width;
        m_LiveVideoHeight = cinfo.output_height;
        PrimaryCCD.setBin(1, 1);
        PrimaryCCD.setFrame(0, 0, m_LiveVideoWidth, m_LiveVideoHeight);
        Streamer->setSize(m_LiveVideoWidth, m_LiveVideoHeight);
    }

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

INDI_PIXEL_FORMAT INDILibCamera::bayerToPixelFormat(const char *bayer)
{
    if (!strcmp(bayer, "RGGB"))
        return INDI_BAYER_RGGB;
    else if (!strcmp(bayer, "GRBG"))
        return INDI_BAYER_GRBG;
    else if (!strcmp(bayer, "GBRG"))
        return INDI_BAYER_GBRG;
    else if (!strcmp(bayer, "BGGR"))
        return INDI_BAYER_BGGR;

    return INDI_MONO;
}
