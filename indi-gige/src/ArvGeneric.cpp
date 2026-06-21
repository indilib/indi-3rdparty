/*
 GigE interface wrapper on araviss
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
#include "ArvGeneric.h"

#include <indilogger.h>
#include <string>

using namespace arv;

namespace {
    inline const char *_str_val(const std::string & s)
    {
        return (s != "" ? s.c_str() : "None");
    }
}

// Variadic template error handler function
template<typename Func, typename... Args>
void ArvGeneric::call_log(const char *func_description, Func &&func, Args&&... args) {
    GError * error = nullptr;
    func(args..., &error);
    if (error) {
        if (error->message)
            LOGF_ERROR("%s: %s", func_description, error->message);
        else
            LOGF_ERROR("%s: unknown error!", func_description);
    }
}
#define CALL(func, ...) \
  this->call_log(#func "(" #__VA_ARGS__ ")", func, __VA_ARGS__)

// Variadic template error handler function
template<typename Func, typename... Args>
auto ArvGeneric::call_log_return(const char *func_description, Func &&func, Args&&... args) {
    GError * error = nullptr;
    auto ret = func(args..., &error);
    if (error) {
        if (error->message)
            LOGF_ERROR("%s: %s", func_description, error->message);
        else
            LOGF_ERROR("%s: unknown error!", func_description);
    }
    return ret;
}
#define CALL_RETURN(func, ...) \
  this->call_log_return(#func "(" #__VA_ARGS__ ")", func, __VA_ARGS__)

const char *ArvGeneric::vendor_name()
{
    return _str_val(this->cam.vendor_name);
}
const char *ArvGeneric::model_name()
{
    return _str_val(this->cam.model_name);
}
const char *ArvGeneric::device_id()
{
    return _str_val(this->cam.device_id);
}
min_max_property<int> ArvGeneric::get_bin_x()
{
    return min_max_property<int>(this->cam.bin_x);
}
min_max_property<int> ArvGeneric::get_bin_y()
{
    return min_max_property<int>(this->cam.bin_y);
}
min_max_property<int> ArvGeneric::get_x_offset()
{
    return min_max_property<int>(this->cam.x_offset);
}
min_max_property<int> ArvGeneric::get_y_offset()
{
    return min_max_property<int>(this->cam.y_offset);
}
min_max_property<int> ArvGeneric::get_width()
{
    return min_max_property<int>(this->cam.width);
}
min_max_property<int> ArvGeneric::get_height()
{
    return min_max_property<int>(this->cam.height);
}
min_max_property<int> ArvGeneric::get_bpp()
{
    return min_max_property<int>(16, 16, 16);
}
min_max_property<double> ArvGeneric::get_pixel_pitch()
{
    return min_max_property<double>(this->cam.pixel_pitch);
}
min_max_property<double> ArvGeneric::get_exposure()
{
    return min_max_property<double>(this->cam.exposure);
}
min_max_property<double> ArvGeneric::get_gain()
{
    return min_max_property<double>(this->cam.gain);
}
min_max_property<double> ArvGeneric::get_frame_rate()
{
    return min_max_property<double>(this->cam.frame_rate);
}

bool ArvGeneric::has_feature(const char * feature)
{
    return CALL_RETURN(arv_camera_is_feature_available, this->camera, feature) == TRUE;
}

double ArvGeneric::get_float(const char * feature)
{
    return CALL_RETURN(arv_camera_get_float, this->camera, feature);
}

template <typename T>
bool ArvGeneric::_get_bounds(void (*fn_arv_bounds)(::ArvCamera *, T *min, T *max, GError**), min_max_property<T> *prop)
{
    T min, max;
    CALL(fn_arv_bounds, this->camera, &min, &max);
    prop->update(min, max);
    return true;
}

template <typename T>
bool ArvGeneric::_get_incr(T (*fn_arv_incr)(::ArvCamera *, GError**), min_max_property<T> *prop)
{
    prop->set_increment(CALL_RETURN(fn_arv_incr, this->camera));
    return true;
}

bool ArvGeneric::is_exposing_single()
{
    bool single = this->single_acquisition_active.load();
    bool stream = this->stream_active.load();
    if (single && stream)
        LOG_ERROR("Streaming during single-frame acquisition?!?");
    return single;
}

bool ArvGeneric::is_streaming()
{
    bool single = this->single_acquisition_active.load();
    bool stream = this->stream_active.load();
    if (single && stream)
        LOG_ERROR("Streaming during single-frame acquisition?!?");
    return stream;
}

bool ArvGeneric::is_connected()
{
    return (this->camera ? true : false);
}

ArvGeneric::ArvGeneric(std::string device_id, std::string model_name)
: ArvCamera(device_id, model_name)
{
    this->_init();
    /* device_id and model name must be available to INDI before Connect */
    this->cam.device_id = device_id;
    this->cam.model_name  = model_name;
}

ArvGeneric::~ArvGeneric()
{
    this->disconnect();
}

bool ArvGeneric::connect()
{
    printf("%s\n", __PRETTY_FUNCTION__);
    /* (Re-)connect by means of the device-id */
    if (!this->camera)
    {
        this->camera = CALL_RETURN(::arv_camera_new,this->cam.device_id.c_str());
        if (!this->camera)
            return false;

        this->dev             = arv_camera_get_device(this->camera);
        this->cam.model_name  = CALL_RETURN(arv_camera_get_model_name, this->camera);
        this->cam.vendor_name = CALL_RETURN(arv_camera_get_vendor_name,this->camera);
        // do not change device_id since arv_camera_get_device_id seems to just
        // return the serial number instead of the less useful
        // "{vendor} {model}-{serial}" than arv_get_device_id that is used in
        // camera enumeration.
        //this->cam.device_id   = CALL_RETURN(arv_camera_get_device_id,this->camera);
    }
    this->_configure();
    return true;
}

bool ArvGeneric::_configure(void)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    this->_set_initial_config();
    return this->_get_initial_config();
}

void ArvGeneric::_init()
{
    this->camera        = nullptr;
    this->stream        = nullptr;
    this->single_acquisition_active = false;
    this->stream_active = false;

    /* Don't clear device_id, its needed to re-attach with connect() */
}

bool ArvGeneric::disconnect()
{
    if (this->is_connected())
    {
        this->exposure_abort();
        g_clear_object(&this->stream);
        g_clear_object(&this->camera);
    }
    this->_init();
    return true;
}

bool ArvGeneric::_set_initial_config()
{
    /* Configure "manual" mode
     *      (1) disable auto exposure
     *      (2) disable auto framerate (to enable maximum possible exposure time)
     *      (3) set binning to 1x1 */
    CALL(arv_camera_set_binning, camera, 1, 1);
    CALL(arv_camera_set_gain_auto, camera, ARV_AUTO_OFF);
    CALL(arv_camera_set_exposure_time_auto, camera, ARV_AUTO_OFF);
    return true;
}

#define _GET_BOUNDS(T, feature, prop) \
  this->_get_bounds<T>(arv_camera_get_ ## feature ## _bounds, &this->cam.prop)
#define _GET_BOUNDS_INCR(T, feature, prop) \
  _GET_BOUNDS(T, feature, prop); \
  this->_get_incr<T>(arv_camera_get_ ## feature ## _increment, &this->cam.prop)


bool ArvGeneric::_get_initial_config()
{
    _GET_BOUNDS_INCR(gint, x_binning, bin_x);
    _GET_BOUNDS_INCR(gint, y_binning, bin_y);
    _GET_BOUNDS_INCR(gint, x_offset, x_offset);
    _GET_BOUNDS_INCR(gint, y_offset, y_offset);
    _GET_BOUNDS_INCR(gint, width, width);
    _GET_BOUNDS_INCR(gint, height, height);
    _GET_BOUNDS(double, frame_rate, frame_rate);
    _GET_BOUNDS(double, exposure_time, exposure);
    _GET_BOUNDS(double, gain, gain);

    /* No GVCP call for this..., specializations of this class could make this
     * read-only by setting min=max=val (see BlackFly implementation). */
    this->cam.pixel_pitch.update(1.0, 40.0);
    this->cam.pixel_pitch.set(1.0);

    return true;
}

int ArvGeneric::get_frame_byte_size()
{
    return CALL_RETURN(arv_camera_get_payload, this->camera);
}

void ArvGeneric::set_geometry(int const x, int const y, int const w, int const h)
{
    this->cam.x_offset.set(x);
    this->cam.y_offset.set(y);
    this->cam.width.set(w);
    this->cam.height.set(h);

    CALL(arv_camera_set_region, this->camera, this->cam.x_offset.val(),
                                this->cam.y_offset.val(), this->cam.width.val(),
                                this->cam.height.val());
}

std::tuple<int,int,int,int> ArvGeneric::update_geometry(void)
{
    gint x, y, w, h;

    CALL(arv_camera_get_region, this->camera, &x, &y, &w, &h);

    this->cam.x_offset.set(x);
    this->cam.y_offset.set(y);
    this->cam.width.set(w);
    this->cam.height.set(h);
    return std::tuple<int,int,int,int>(x, y, w, h);
}

void ArvGeneric::set_bin(int const bin_x, int const bin_y)
{
    this->cam.bin_x.set(bin_x);
    this->cam.bin_y.set(bin_y);

    CALL(arv_camera_set_binning, this->camera, this->cam.bin_x.val(), this->cam.bin_y.val());
}

std::pair<int,int> ArvGeneric::update_bin()
{
    /* read this back to ensure we know what binning was actually set.  For at
     * least some cameras, a binning of 3 gets promoted to a binning of 4. */
    gint binx, biny;
    CALL(arv_camera_get_binning, this->camera, &binx, &biny);
    this->cam.bin_x.set(binx);
    this->cam.bin_y.set(biny);
    return std::pair<int,int>(binx, biny);
}

template <typename T>
void ArvGeneric::set_cam_exposure_property(
  void (*arv_set)(::ArvCamera *, T, GError**),
  min_max_property<T> *prop, T const new_val,
  T (*arv_get)(::ArvCamera *, GError**) )
{
    if (this->is_exposing_single())
        this->exposure_abort();
    prop->set(new_val);
    CALL(arv_set, this->camera, prop->val());

    if (arv_get != nullptr)
      prop->set(CALL_RETURN(arv_get, this->camera));
}

void ArvGeneric::set_gain(double const val)
{
    this->set_cam_exposure_property(arv_camera_set_gain, &this->cam.gain, val,
                                    arv_camera_get_gain);
}

void ArvGeneric::set_exposure_time(double const val)
{
    this->set_cam_exposure_property(arv_camera_set_exposure_time,
                                    &this->cam.exposure, val,
                                    arv_camera_get_exposure_time);
}

void ArvGeneric::create_stream(unsigned int n_buffers)
{
    this->stream = CALL_RETURN(arv_camera_create_stream, this->camera, nullptr, nullptr);
    if (this->stream == nullptr) {
        LOG_ERROR("Could not allocate a stream object!");
        return;
    }

    gint const payload = CALL_RETURN(arv_camera_get_payload, this->camera);
    for (; n_buffers > 0; --n_buffers) {
        auto buffer = arv_buffer_new(payload, nullptr);
        if (this->stream == nullptr) {
            LOG_ERROR("Could not allocate stream buffer!");
            break;
        }
        arv_stream_push_buffer(this->stream, buffer);
    }
}

void ArvGeneric::start_acquisition(unsigned int n_buffers,
                                   ArvAcquisitionMode mode)
{
    this->exposure_abort();

    // 1. make stream
    this->create_stream(n_buffers);

    // 2. Disable triggers; just acquire as soon as possible.
    CALL(arv_camera_clear_triggers, this->camera);

    // 3. start the acquisition stream
    CALL(arv_camera_set_acquisition_mode, this->camera, mode);
    CALL(arv_camera_start_acquisition, this->camera);
}

void ArvGeneric::start_streaming_impl(unsigned int n_buffers)
{
    this->start_acquisition(n_buffers, ARV_ACQUISITION_MODE_CONTINUOUS);
    this->stream_active = true;
}

void ArvGeneric::stop_streaming()
{
    this->stop_acquisition();
}

void ArvGeneric::stop_acquisition()
{
    /* stop the acquisition stream */
    CALL(arv_camera_stop_acquisition, this->camera);
    /* Free stream resources. */
    g_clear_object(&this->stream);

    this->stream_active = false;
    this->single_acquisition_active = false;
}

void ArvGeneric::exposure_start(void)
{
    this->start_acquisition(1, ARV_ACQUISITION_MODE_SINGLE_FRAME);
    this->single_acquisition_active = true;
}

void ArvGeneric::exposure_abort(void)
{
    if (this->is_acquiring())
    {
        CALL(arv_camera_abort_acquisition, this->camera);
        this->stop_acquisition();
    }
}

void ArvGeneric::_get_image(ArvGeneric::HandleImgCB fn_image_callback,
                            ArvBuffer * buf)
{
    if (fn_image_callback != nullptr)
    {
        size_t size;
        uint8_t const * data = (uint8_t const *)arv_buffer_get_data(buf, &size);
        fn_image_callback(data, size);
    }
}

ARV_EXPOSURE_STATUS ArvGeneric::exposure_poll(ArvGeneric::HandleImgCB fn_image_callback)
{
    if (!this->is_exposing_single())
        return ARV_EXPOSURE_UNKNOWN;

    {
        /* There is no point in examining the buffer status until it in the
         * output queue of the stream. */
        gint n_inputs = 0, n_outputs = 0;
        arv_stream_get_n_buffers(this->stream, &n_inputs, &n_outputs);
        if (n_outputs < 1) {
            if (n_inputs == 0) {
                LOG_ERROR("Waiting for image data without input buffer!");
            }
            return ARV_EXPOSURE_BUSY;
        }
    }

    ArvBuffer * buf = arv_stream_timeout_pop_buffer(
      this->stream, static_cast<guint64>(this->get_exposure().val()));
    if (buf == nullptr)
        return ARV_EXPOSURE_BUSY;
    ARV_EXPOSURE_STATUS retval;

    ::ArvBufferStatus const status = arv_buffer_get_status(buf);
    switch (status)
    {
        case ARV_BUFFER_STATUS_CLEARED:
            retval = ARV_EXPOSURE_BUSY;
            break;
        case ARV_BUFFER_STATUS_FILLING:
            retval = ARV_EXPOSURE_FILLING;
            break;
        case ARV_BUFFER_STATUS_UNKNOWN:
            retval = ARV_EXPOSURE_UNKNOWN;
            break;
        case ARV_BUFFER_STATUS_SUCCESS:
            this->_get_image(fn_image_callback, buf);
            this->stop_acquisition();
            retval = ARV_EXPOSURE_FINISHED;
            break;
        case ARV_BUFFER_STATUS_TIMEOUT:
        case ARV_BUFFER_STATUS_MISSING_PACKETS:
        case ARV_BUFFER_STATUS_WRONG_PACKET_ID:
        case ARV_BUFFER_STATUS_SIZE_MISMATCH:
        case ARV_BUFFER_STATUS_ABORTED:
            this->stop_acquisition();
            retval = ARV_EXPOSURE_FAILED;
            break;
        default:
            retval = ARV_EXPOSURE_UNKNOWN;
            break;
    }

    if (this->is_acquiring())
      /* give buffer back to stream and let the stream own buffer memory.
       * This case seems unlikely.
       */
      arv_stream_push_buffer(this->stream, buf);
    else
      // free orphaned buffer
      g_clear_object(&buf);
    return retval;
}

ARV_EXPOSURE_STATUS ArvGeneric::next_streaming_image(ArvGeneric::HandleImgCB fn_image_callback)
{
    auto get_n_inputs = [this]() {
        /* There is no point in examining the buffer status until it in the
         * output queue of the stream. */
        gint n_inputs = 0, n_outputs = 0;
        arv_stream_get_n_buffers(this->stream, &n_inputs, &n_outputs);
        return static_cast<int>(n_inputs);
    };

    if (!this->is_streaming())
        return ARV_EXPOSURE_UNKNOWN;

    auto pre_inputs = get_n_inputs();
    ArvBuffer *const buf = arv_stream_timeout_pop_buffer(
      this->stream, static_cast<guint64>(this->get_exposure().val()));
    if (buf == nullptr) {
        //LOG_DEBUG("Timed out getting next streaming image");
        auto post_inputs = get_n_inputs();
        if (pre_inputs != post_inputs)
          LOGF_ERROR("Leaked %d buffers when buffer pop returned NULL!!!",
                     (pre_inputs - post_inputs));
        return ARV_EXPOSURE_BUSY;
    }

    ::ArvBufferStatus const status = arv_buffer_get_status(buf);
    ARV_EXPOSURE_STATUS retval = ARV_EXPOSURE_UNKNOWN;
    switch (status)
    {
        case ARV_BUFFER_STATUS_TIMEOUT:
        case ARV_BUFFER_STATUS_CLEARED:
            retval = ARV_EXPOSURE_BUSY;
            break;
        case ARV_BUFFER_STATUS_FILLING:
            retval = ARV_EXPOSURE_FILLING;
            break;
        case ARV_BUFFER_STATUS_UNKNOWN:
            retval = ARV_EXPOSURE_UNKNOWN;
            break;
        case ARV_BUFFER_STATUS_SUCCESS:
            this->_get_image(fn_image_callback, buf);
            retval = ARV_EXPOSURE_FINISHED;
            break;
        case ARV_BUFFER_STATUS_MISSING_PACKETS:
        case ARV_BUFFER_STATUS_WRONG_PACKET_ID:
        case ARV_BUFFER_STATUS_SIZE_MISMATCH:
        case ARV_BUFFER_STATUS_ABORTED:
            LOG_ERROR("Exposure failed, stopping acquisition");
            this->stop_acquisition();
            retval = ARV_EXPOSURE_FAILED;
            break;
        default:
            retval = ARV_EXPOSURE_UNKNOWN;
            break;
    }

    // give buffer back to stream and let the stream own buffer memory.
    //LOG_DEBUG("Pushing buffer back to stream");
    arv_stream_push_buffer(this->stream, buf);
    return retval;
}
