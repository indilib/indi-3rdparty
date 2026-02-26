/*
 GigE interface for INDI based on aravis
 Copyright (C) 2016 Hendrik Beijeman (hbeyeman@gmail.com)

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

#include <time.h>
#include <list>
#include <sys/time.h>
#include <deque>
#include <memory>
#include <mutex>

#include "indidevapi.h"
#include "eventloop.h"

#include "indi_gige.h"

#define TIME_VAL_INIT(x)  \
    ({                    \
        (x)->tv_sec  = 0; \
        (x)->tv_usec = 0; \
    })
#define TIME_VAL_ISSET(x) (((x)->tv_sec != 0) && ((x)->tv_usec != 0))
#define TIME_VAL_US(x)    (((x)->tv_sec) * 1000000 + ((x)->tv_usec))
#define TIME_VAL_GET(x)   (gettimeofday(x, nullptr))

#define TIMER_TRANSFER_TIMEOUT_US (5000000UL) /* Allow for relatively large link-layer delays */
#define TIMER_EXPOSURE_TIMEOUT_US (200000UL)  /* GigE cameras are very precise, so set 100ms time-out */

#define TIMER_US_TO_MS (1000)
#define TIMER_US_TO_S  (1000000)
#define CAPS           (CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_STREAMING)

static class Loader
{
    std::deque<std::unique_ptr<GigECCD>> cameras;
public:
    Loader()
    {
        arv::ArvCamera *camera = arv::ArvFactory::find_first_available();
        cameras.push_back(std::unique_ptr<GigECCD>(new GigECCD(camera)));
        IDLog("Found Camera: %s\n", camera->device_id());
    }
} loader;

const char *GigECCD::getDefaultName()
{
    return "GigE CCD";
}

GigECCD::GigECCD(arv::ArvCamera *camera)
{
    this->camera = camera;
    setDeviceName(this->camera->device_id());
}

GigECCD::~GigECCD()
{
}

bool GigECCD::initProperties()
{
    INDI::CCD::initProperties();
    this->SetCCDCapability((CAPS));
    this->addConfigurationControl();
    this->addDebugControl();
    return true;
}

bool GigECCD::_update_geometry(void)
{
    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    /* Get actual values */
    this->camera->update_geometry();

    /* Sync these with INDI */
    PrimaryCCD.setBin(this->camera->get_bin_x().val(), this->camera->get_bin_y().val());
    PrimaryCCD.setFrame(this->camera->get_x_offset().val(), this->camera->get_y_offset().val(),
                        this->camera->get_width().val(), this->camera->get_height().val());

    /* Sanity checks, reserve buffers */
    [[maybe_unused]] int const width           = this->camera->get_width().val();
    [[maybe_unused]] int const height          = this->camera->get_height().val();
    int const frame_byte_size = this->camera->get_frame_byte_size();
    int const indi_bufsize    = PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * PrimaryCCD.getBPP() / 8;

    if (indi_bufsize != frame_byte_size)
    {
        LOGF_ERROR("Unexpected INDI image buffer size, has %i bytes, camera has %i", indi_bufsize,
               frame_byte_size);
        PrimaryCCD.setFrameBufferSize(0);
    }
    else
    {
        LOGF_INFO("Reserving INDI image buffer size %i bytes", indi_bufsize);
        PrimaryCCD.setFrameBufferSize(frame_byte_size);
    }

    this->Streamer->setPixelFormat(INDI_MONO, PrimaryCCD.getBPP());
    this->Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
    return true;
}

void GigECCD::_update_indi_properties(void)
{
    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    LOG_INFO("update_indi_properties()");

    // Gain
    GainNP[0].fill("GAIN", "value", "%.f",
                   (double)this->camera->get_gain().min(),
                   (double)this->camera->get_gain().max(), 1.,
                   (double)this->camera->get_gain().val());
    GainNP.fill(getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    auto pitch = this->camera->get_pixel_pitch();
    PixelSizeNP[0].fill("PIXEL_SIZE", "Size [μm]", "%.2f",
                        pitch.min(), pitch.max(), .1, pitch.val());
    PixelSizeNP.fill(getDeviceName(), "CCD_PIXEL_SIZE", "Pixel",
                     IMAGE_SETTINGS_TAB,
                     pitch.max() == pitch.min() ? IP_RO : IP_RW, 60, IPS_IDLE);

    IUFillText(&indiprop_info[0], "Vendor Name", "", this->camera->vendor_name());
    IUFillText(&indiprop_info[1], "Model Name", "", this->camera->model_name());
    IUFillText(&indiprop_info[2], "Device ID", "", this->camera->device_id());
    IUFillTextVector(&indiprop_info_prop, indiprop_info, 3, getDeviceName(), "Camera Info", "", MAIN_CONTROL_TAB, IP_RO,
                     0, IPS_IDLE);

    defineProperty(&indiprop_info_prop);
    defineProperty(this->GainNP);
    defineProperty(this->PixelSizeNP);
    if (this->camera->has_feature("DeviceTemperature")) {
      this->TemperatureNP.setPermission(IP_RO);
      defineProperty(this->TemperatureNP);
    }
}

void GigECCD::_delete_indi_properties(void)
{
    this->deleteProperty(this->GainNP);
    this->deleteProperty(this->PixelSizeNP);
    this->deleteProperty(this->indiprop_info_prop.name);
    if (TemperatureNP.getPermission() == IP_RO) {
      this->TemperatureNP.setPermission(IP_RW);
      this->deleteProperty(this->TemperatureNP);
    }
}

//Initial call
bool GigECCD::updateProperties()
{
    INDI::CCD::updateProperties();

    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    if (this->camera->is_connected())
    {
        this->_update_indi_properties();
        this->SetCCDParams(this->camera->get_width().max(), this->camera->get_height().max(),
                           this->camera->get_bpp().val(), this->camera->get_pixel_pitch().val(),
                           this->camera->get_pixel_pitch().val());

        (void)this->_update_geometry();
        this->timer_id = this->SetTimer(this->getCurrentPollingPeriod());
    }
    else
    {
        rmTimer(this->timer_id);
        this->_delete_indi_properties();
    }

    return true;
}

bool GigECCD::Connect()
{
    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    IDLog("Connect to Camera: %s\n", camera->model_name());
    this->start_streaming_thread();
    return camera->connect();
}

bool GigECCD::Disconnect()
{
    LOGF_INFO("%s", __PRETTY_FUNCTION__);
    this->stop_streaming_thread();
    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    return camera->disconnect();
}

void GigECCD::start_streaming_thread()
{
    auto receive_image = [this](uint8_t const *const data, size_t size)
    {
        /* locked as per indiccd.h guidance. */
        std::lock_guard<std::mutex> lock(this->ccdBufferLock);
        this->Streamer->newFrame(data, size);
        //LOG_DEBUG("injected image into stream manager");
    };

    auto streaming_worker = [this, receive_image]() {
        while (!this->streaming_thread_stop_requested.load()) {
            std::unique_lock<std::recursive_mutex> lock(this->camera_mutex);
            if (!this->streaming_thread_active.load()) {
                if (this->camera->is_streaming())
                    // Have been streaming; have now been requested to stop
                    this->camera->stop_streaming();

                this->streaming_thread_condition.wait(lock);

                if (!this->streaming_thread_active.load()) {
                    LOG_INFO("Quit probably requested for streaming thread");
                    // probably just requested to quit so go to while loop test
                    continue;
                }

                /* While we *weren't* streaming, we are now requested to start
                 * streaming.
                 */
                LOG_INFO("Starting streaming");
                this->camera->start_streaming();
            }

            // camera_mutex should be locked at this point
            auto status = camera->next_streaming_image(receive_image);
            switch (status) {
                case arv::ARV_EXPOSURE_UNKNOWN:
                case arv::ARV_EXPOSURE_FAILED: {
                    LOG_ERROR("Streaming acquisition had unknown failure");
                    this->camera->exposure_abort();
                    break;
                }
                default:
                    break;
            }
        }

        LOG_INFO("Shutting down streaming thread");
    };

    this->streaming_thread = std::thread(streaming_worker);
}

void GigECCD::stop_streaming_thread()
{
    LOG_INFO("Requesting streaming stop");
    {
        std::lock_guard<std::mutex> lock(this->ccdBufferLock);
        this->streaming_thread_active = false;
        this->streaming_thread_stop_requested = true;
    }
    this->streaming_thread_condition.notify_all();
    this->streaming_thread.join();
}

bool GigECCD::StartExposure(float duration)
{
    LOGF_INFO("%s exposure_time=%.4f", __PRETTY_FUNCTION__, duration);
    /* Driver will clamp to lowest possible exposure */
    if (PrimaryCCD.getFrameType() == INDI::CCDChip::BIAS_FRAME)
        duration = 0;

    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    camera->set_exposure_time((double)(duration)*1000000.0);
    PrimaryCCD.setExposureDuration(duration); // to ensure FITS correct header

    TIME_VAL_INIT(&this->exposure_transfer_time);
    TIME_VAL_GET(&this->exposure_start_time);

    camera->exposure_start();
    return camera->is_exposing_single();
}

bool GigECCD::AbortExposure()
{
    LOGF_INFO("%s", __PRETTY_FUNCTION__);
    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    camera->exposure_abort();
    return true;
}

bool GigECCD::StartStreaming()
{
    {
        std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
        if (this->camera->is_exposing_single()) {
            LOG_ERROR("Invalid streaming request during single-frame acquisition");
            return false;
        }

        this->camera->set_exposure_time(Streamer->getTargetExposure()*1000000.0);
        LOG_INFO("Requesting streaming start");
        this->streaming_thread_active = true;
    }
    this->streaming_thread_condition.notify_all();
    return true;
}

bool GigECCD::StopStreaming()
{
    LOG_INFO("Requesting streaming stop");
    this->streaming_thread_active = false;
    this->streaming_thread_condition.notify_all();
    return true;
}

void GigECCD::_update_image(uint8_t const *const data, size_t size)
{
    LOGF_INFO("Received %i bytes image", size);

    size_t const frame_buf_size = PrimaryCCD.getFrameBufferSize();

    if ((size == frame_buf_size) && (data != nullptr))
    {
        { /* locked as per indiccd.h guidance. */
            std::lock_guard<std::mutex> lock(this->ccdBufferLock);
            uint8_t *const image = PrimaryCCD.getFrameBuffer();
            memcpy(image, (void const*)data, frame_buf_size);
        }
        if (TemperatureNP.getPermission() == IP_RO) {
            std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
            this->TemperatureNP[0].setValue(this->camera->get_float("DeviceTemperature"));
            this->TemperatureNP.setState(IPS_OK);
            this->TemperatureNP.apply();
        }
        this->ExposureComplete(&PrimaryCCD);
    }
    else
    {
        LOGF_ERROR("Unexpected failure during image download. Framebuf has %i bytes, got %i",
               frame_buf_size, size);
        this->_handle_failed();
    }
}

void GigECCD::_handle_failed(void)
{
    LOG_ERROR("Failure occurred, filling image with black");

    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    camera->exposure_abort();

    PrimaryCCD.setExposureLeft(0);

    { /* locked as per indiccd.h guidance. */
        std::lock_guard<std::mutex> lock(this->ccdBufferLock);
        /* Fill with black */
        uint8_t *const image = PrimaryCCD.getFrameBuffer();
        memset(image, 0, PrimaryCCD.getFrameBufferSize());
    }

    this->ExposureComplete(&PrimaryCCD);
}

void GigECCD::_handle_timeout(struct timeval *const tv, uint32_t timeout_us)
{
    if (!TIME_VAL_ISSET(tv))
        TIME_VAL_GET(tv);

    struct timeval now;
    TIME_VAL_GET(&now);

    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    uint32_t const elapsed       = ((TIME_VAL_US(&now)) - (TIME_VAL_US(tv)));
    uint32_t const exposure_time = (uint32_t)this->camera->get_exposure().val();
    uint32_t const time_left     = exposure_time - elapsed;

    if (elapsed >= exposure_time)
        PrimaryCCD.setExposureLeft(0);
    else
        PrimaryCCD.setExposureLeft((float)time_left / (float)TIMER_US_TO_S);

    if (elapsed > timeout_us) {
        LOGF_ERROR("Image acquisition timed out (>%d μs)", timeout_us);
        this->_handle_failed();
    }
}

void GigECCD::TimerHit()
{
    this->timer_id = this->SetTimer(this->getCurrentPollingPeriod());
    if (!this->camera->is_connected() || !this->camera->is_exposing_single())
        return;

    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    auto receive_image = [this](uint8_t const *const data, size_t size)
    {
        this->_update_image(data, size);
    };


    auto status = camera->exposure_poll(receive_image);
    switch (status)
    {
        case arv::ARV_EXPOSURE_FINISHED:
            // Nothing to do, ArvCamera automatically unsets is_exposing_single
            break;
        case arv::ARV_EXPOSURE_UNKNOWN:
        case arv::ARV_EXPOSURE_FAILED: {
            LOG_ERROR("Image acquisition had unknown failure");
            this->_handle_failed();
            break;
        }
        case arv::ARV_EXPOSURE_FILLING:
            this->_handle_timeout(&this->exposure_transfer_time, TIMER_TRANSFER_TIMEOUT_US);
            break;
        case arv::ARV_EXPOSURE_BUSY:
            this->_handle_timeout(&this->exposure_start_time,
                                  ((uint32_t)this->camera->get_exposure().val() + TIMER_EXPOSURE_TIMEOUT_US));
            break;
    }
}

bool GigECCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(dev, this->getDeviceName()))
    {
        if (GainNP.isNameMatch(name))
        {
            GainNP.update(values, names, n);
            GainNP.setState(IPS_OK);

            std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
            this->camera->set_gain(this->GainNP[0].getValue());
            /* Get-back from camera system */
            this->GainNP[0].setValue(this->camera->get_gain().val());

            GainNP.apply();
            saveConfig(PixelSizeNP);
            return true;
        }

        if (PixelSizeNP.isNameMatch(name))
        {
            PixelSizeNP.update(values, names, n);
            PixelSizeNP.setState(IPS_OK);
            PixelSizeNP.apply();

            auto dx = PixelSizeNP[0].getValue();
            PrimaryCCD.setPixelSize(dx, dx);
            saveConfig(PixelSizeNP);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool GigECCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    LOGF_INFO("%s x=%i y=%i w=%i h=%i", __PRETTY_FUNCTION__, x, y, w, h);

    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    this->camera->set_geometry(x, y, w, h);
    return this->_update_geometry();
}

bool GigECCD::UpdateCCDBin(int binx, int biny)
{
    LOGF_INFO("%s binx=%i biny=%i", __PRETTY_FUNCTION__, binx, biny);
    std::lock_guard<std::recursive_mutex> lock(this->camera_mutex);
    camera->set_bin(binx, biny);
    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

bool GigECCD::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType)
{
    LOGF_INFO("%s", __PRETTY_FUNCTION__);
    PrimaryCCD.setFrameType(fType);
    return true;
}

void GigECCD::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeyword)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeyword);

    fitsKeyword.push_back({"GAIN", GainNP[0].getValue(), 3, "Gain"});
}

bool GigECCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);
    GainNP.save(fp);
    PixelSizeNP.save(fp);
    return true;
}
