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
#ifndef CPP_ARV_GENERIC_H
#define CPP_ARV_GENERIC_H

#include <string>
#include <atomic>

//#extern "C" {
#include <stddef.h>
#include <stdio.h>
#include <arv.h>
//}

#include "ArvInterface.h"

using namespace arv;

class ArvGeneric : public arv::ArvCamera
{
  public:
    using arv::ArvCamera::HandleImgCB;

    ArvGeneric(std::string device_id, std::string model_name);
    virtual ~ArvGeneric();

    virtual bool is_connected() override;
    virtual bool is_exposing_single() override;
    virtual bool is_streaming() override;
    virtual bool connect() override;
    virtual bool disconnect() override;
    virtual const char *vendor_name() override;
    virtual const char *model_name() override;
    virtual const char *device_id() override;
    virtual int get_frame_byte_size() override;
    virtual min_max_property<int> get_bin_x() override;
    virtual min_max_property<int> get_bin_y() override;
    virtual min_max_property<int> get_x_offset() override;
    virtual min_max_property<int> get_y_offset() override;
    virtual min_max_property<int> get_width() override;
    virtual min_max_property<int> get_height() override;
    virtual min_max_property<int> get_bpp() override;
    virtual min_max_property<double> get_pixel_pitch() override;
    virtual min_max_property<double> get_exposure() override;
    virtual min_max_property<double> get_gain() override;
    virtual min_max_property<double> get_frame_rate() override;
    virtual bool has_feature(const char * feature) override;
    virtual double get_float(const char * feature) override;

    virtual void set_bin(int const bin_x, int const bin_y) override;
    virtual void set_geometry(int const x, int const y, int const w, int const h) override;
    virtual void update_geometry(void) override;
    virtual void set_exposure_time(double const val) override;
    virtual void set_gain(double const val) override;

    virtual void exposure_start(void) override;
    virtual void exposure_abort(void) override;
    virtual ARV_EXPOSURE_STATUS exposure_poll(HandleImgCB fn_image_callback) override;
    virtual ARV_EXPOSURE_STATUS next_streaming_image(HandleImgCB fn_image_callback) override;
    virtual void stop_streaming(void)  override;

  protected:
    virtual void start_streaming_impl(unsigned int n_buffers) override;
    void _init(void);
    virtual bool _configure(void);

  private:
    template <typename T>
    bool _get_bounds(void (*fn_arv_bounds)(::ArvCamera *, T *min, T *max, GError**), min_max_property<T> *prop);

    /** Set a given property where it should not be changed during a
     * single-frame acquisition and attemping to do so should cause the
     * acquisition to be aborted.
     * This function does *not* stop acquisition if a change is made during a
     * streaming operation.
     * If the 'get' function is also passed, the value of the parameter will be
     * queried from the camera after the 'set' function has been called.  This
     * can be useful since the camera can often demote/promote settings to some
     * gridded pattern internal to the camera.
     */
    template <typename T>
    void set_cam_exposure_property(void (*arv_set)(::ArvCamera *, T, GError**),
                                   min_max_property<T> *prop, T const new_val,
                                   T (*arv_get)(::ArvCamera *, GError**) = nullptr);

    const char * getDeviceName() { return this->device_id(); }
    // Variadic template error handler function with no return value
    template<typename Func, typename... Args>
    void call_log(const char *func_description, Func &&func, Args&&... args);
    // Variadic template error handler function with no return value
    template<typename Func, typename... Args>
    auto call_log_return(const char *func_description, Func &&func, Args&&... args);

  protected:
    virtual bool _get_initial_config();
    virtual bool _set_initial_config();
    void _get_image(HandleImgCB fn_image_callback, ArvBuffer * buf);

    /* aravis library state variables */
    ::ArvCamera *camera;
    ::ArvDevice *dev;
    ::ArvStream *stream; /// Hold all buffers in queues until freed

    /* streaming, capturing functions */
    /** Creates the current stream and pushes buffers to its input queue. */
    void create_stream(unsigned int n_buffers = 1);
    void start_acquisition(unsigned int n_buffers, ArvAcquisitionMode mode);
    /** Stops all acquisition. */
    void stop_acquisition();

    std::atomic<bool> single_acquisition_active;
    std::atomic<bool> stream_active;

    /* Camera properties */
    struct
    {
        min_max_property<gint> bin_x;
        min_max_property<gint> bin_y;
        min_max_property<gint> x_offset;
        min_max_property<gint> y_offset;
        min_max_property<gint> width;
        min_max_property<gint> height;

        min_max_property<double> pixel_pitch;

        min_max_property<double> exposure;
        min_max_property<double> gain;
        min_max_property<double> frame_rate;

        std::string vendor_name;
        std::string model_name;
        std::string device_id;
    } cam;
};

#endif /*CPP_ARV_CAMERA_H*/
