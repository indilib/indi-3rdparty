/*
 Raspberry Pi High Quality Camera CCD Driver for Indi.
 Copyright (C) 2020 Lars Berntzon (lars.berntzon@cecilia-data.se).
 All rights reserved.

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

#include <stdio.h>
#include <mmal_logging.h>
#include <mmal_default_components.h>
#include <util/mmal_util.h>
#include <util/mmal_util_params.h>
#include <bcm_host.h>

#include "mmalcamera.h"
#include "mmalexception.h"
#include "mmalencoder.h"
#include "inditest.h"

MMALCamera::MMALCamera(int n) : MMALComponent(MMAL_COMPONENT_DEFAULT_CAMERA), cameraNum(n)
{
    LOG_TEST("entered");

    selectCameraNumber(cameraNum);

    getSensorInfo();

    selectSensorConfig(0 /* What ever 0 means */);

    configureCamera();

    getFPSRange();

    // FIXME: moved from #001 to after sensor Enable the controlport so calls below works.
    enablePort(component->control, false);

    LOGF_TEST("fps_low=%d/%d, fps_high=%d/%d", fps_low.num, fps_low.den, fps_high.num, fps_high.den);
}

MMALCamera::~MMALCamera()
{
    if(component->control->is_enabled) {
        MMALException::throw_if(mmal_port_disable(component->control), "Failed to disable control port");
    }
}

/**
 * @brief Main exposure method.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
void MMALCamera::startCapture()
{
    // Start capturing.
    LOGF_TEST("starting capture with speed %d", getShutterSpeed());
    MMALException::throw_if(mmal_port_parameter_set_boolean(component->output[CAPTURE_PORT_NO], MMAL_PARAMETER_CAPTURE, 1), "Failed to start capture");
}

void MMALCamera::stopCapture()
{
    MMALException::throw_if(mmal_port_parameter_set_boolean(component->output[CAPTURE_PORT_NO], MMAL_PARAMETER_CAPTURE, 0), "Failed to stop capture");

    LOG_TEST("capture stopped");
}

void MMALCamera::setExposureParameters(double gain, uint32_t shutter_speed)
{
    MMAL_PARAMETER_AWBMODE_T awb = {{MMAL_PARAMETER_AWB_MODE,sizeof awb}, MMAL_PARAM_AWBMODE_AUTO};
    MMALException::throw_if(mmal_port_parameter_set(component->control, &awb.hdr), "Failed to set AWB mode");

    MMALException::throw_if(mmal_port_parameter_set_rational(component->control, MMAL_PARAMETER_SATURATION, MMAL_RATIONAL_T {10, 0}), "Failed to set saturation");

    MMALException::throw_if(mmal_port_parameter_set_rational(component->control, MMAL_PARAMETER_DIGITAL_GAIN, MMAL_RATIONAL_T {1, 1}), "Failed to set digital gain");

#ifdef USE_ISO
    MMALException::throw_if(mmal_port_parameter_set_uint32(component->control, MMAL_PARAMETER_ISO, iso), "Failed to set ISO");
    LOGF_TEST("ISO set to %d", iso);
#endif

    MMALException::throw_if(mmal_port_parameter_set_rational(component->control, MMAL_PARAMETER_BRIGHTNESS, MMAL_RATIONAL_T{50, 100}), "Failed to set brightness");

    MMAL_PARAMETER_EXPOSUREMODE_T exposure = {{MMAL_PARAMETER_EXPOSURE_MODE, sizeof exposure}, MMAL_PARAM_EXPOSUREMODE_OFF};
    MMALException::throw_if(mmal_port_parameter_set(component->control, &exposure.hdr), "Failed to set exposure mode");

    MMAL_PARAMETER_INPUT_CROP_T crop_param = {{MMAL_PARAMETER_INPUT_CROP, sizeof crop_param}, crop};
    MMALException::throw_if(mmal_port_parameter_set(component->control, &crop_param.hdr), "Failed to set ROI");
    MMALException::throw_if(mmal_port_parameter_get(component->control, &crop_param.hdr), "Failed to get ROI");
    LOGF_TEST("Camera crop set to %d,%d,%d,%d\n", crop_param.rect.x, crop_param.rect.y, crop_param.rect.width, crop_param.rect.height);

    component->port[CAPTURE_PORT_NO]->buffer_size = component->port[CAPTURE_PORT_NO]->buffer_size_recommended;

    MMALException::throw_if(mmal_port_parameter_set_boolean(component->output[VIDEO_PORT_NO], MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE),
                            "Failed to turn on zero-copy for video port");

    MMALException::throw_if(mmal_port_parameter_set_boolean(component->output[CAPTURE_PORT_NO], MMAL_PARAMETER_ENABLE_RAW_CAPTURE, 1), "Failed to set raw capture");

    MMALException::throw_if(mmal_port_parameter_set_uint32(component->control, MMAL_PARAMETER_CAPTURE_STATS_PASS, MMAL_TRUE), "Failed to set CAPTURE_STATS_PASS");

    // Exposure ranges
    MMAL_RATIONAL_T low, high;
    if(shutter_speed > 6000000) {
        low = {5, 1000};
        high = {166, 1000};
    }
    else if(shutter_speed > 1000000) {
        low = {167, 1000};
        high = {999, 1000};
    }
    else {
        low = fps_low;
        high = fps_high;
    }
    LOGF_TEST("setting fps range %d/%d -> %d/%d", low.num, low.den, high.num, high.den);
    MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)}, low, high};
    MMALException::throw_if(mmal_port_parameter_set(component->output[CAPTURE_PORT_NO], &fps_range.hdr), "Failed to set FPS range");
    MMALException::throw_if(mmal_port_parameter_get(component->output[CAPTURE_PORT_NO], &fps_range.hdr), "Failed to get FPS range");
    if (fps_range.fps_low.num != low.num || fps_range.fps_low.den != low.den || 
        fps_range.fps_high.num != high.num || fps_range.fps_high.den != high.den) {
        LOGF_TEST("failed to set fps ranges: low range is %d/%d, high range is %d/%d",
                fps_range.fps_low.num, fps_range.fps_low.den, fps_range.fps_high.num, fps_range.fps_high.den);
    }

    // Exposure time.
    MMALException::throw_if(mmal_port_parameter_set_uint32(component->control, MMAL_PARAMETER_SHUTTER_SPEED, shutter_speed), "Failed to set shutter speed");
    uint32_t actual_shutter_speed;
    actual_shutter_speed = getShutterSpeed();
    if (actual_shutter_speed < shutter_speed - 100000 || actual_shutter_speed > shutter_speed + 100000) {
        LOGF_TEST("Failed to set shutter speed, requested %d but actual value is %d", shutter_speed, actual_shutter_speed); 
    }
    LOGF_TEST("shutter speed set to %d", actual_shutter_speed); 

    // Gain settings
    MMALException::throw_if(mmal_port_parameter_set_rational(component->control, MMAL_PARAMETER_ANALOG_GAIN, MMAL_RATIONAL_T {static_cast<int32_t>(gain * 65536), 65536}),
                            "Failed to set analog gain");
    MMAL_RATIONAL_T actual_gain;
    MMALException::throw_if(mmal_port_parameter_get_rational(component->control, MMAL_PARAMETER_ANALOG_GAIN, &actual_gain), "failed to get gain");
    LOGF_TEST("gain set to %d/%d", actual_gain.num, actual_gain.den);
}

uint32_t MMALCamera::getShutterSpeed()
{
    uint32_t actual_shutter_speed;
    MMALException::throw_if(mmal_port_parameter_get_uint32(component->control, MMAL_PARAMETER_SHUTTER_SPEED, &actual_shutter_speed), "Failed to get shutter speed");
    return actual_shutter_speed;
}

/**
 * @brief Set format for the output capture port.
 */
void MMALCamera::setCapturePortFormat()
{
    LOG_TEST("entered");
    assert(component->is_enabled == 0);
    assert(component->output[CAPTURE_PORT_NO]->is_enabled == 0);

    // Set our stills format on the stills (for encoder) port
    MMAL_ES_FORMAT_T *format {component->output[CAPTURE_PORT_NO]->format};

    // Special case for raw format.
    format->encoding = MMAL_ENCODING_OPAQUE; format->encoding_variant = 0;

    if (!mmal_util_rgb_order_fixed(component->output[CAPTURE_PORT_NO]))
    {
       if (format->encoding == MMAL_ENCODING_RGB24)
          format->encoding = MMAL_ENCODING_BGR24;
       else if (format->encoding == MMAL_ENCODING_BGR24)
          format->encoding = MMAL_ENCODING_RGB24;
    }

    format->encoding_variant = 0;
    format->es->video.width = width;
    format->es->video.height = height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = static_cast<int32_t>(width);
    format->es->video.crop.height = static_cast<int32_t>(height);
    format->es->video.frame_rate.num = 0;
    format->es->video.frame_rate.den = 1;
    format->es->video.par.num = 1;
    format->es->video.par.den = 1;

    MMALException::throw_if(mmal_port_format_commit(component->output[CAPTURE_PORT_NO]), "camera capture port format couldn't be set");
}

/**
 * @breif Sets the size of subframe.
 */
void MMALCamera::set_crop(int x, int y, int w, int h)
{
    crop.x = x;
    crop.y = y;
    crop.width = w;
    crop.height = h;
}

/**
 * @brief Gets default size for camrea.
 */
void MMALCamera::getSensorInfo()
{
    MMAL_COMPONENT_T *camera_info;
    MMAL_STATUS_T status;

    // Try to get the camera name and maximum supported resolution
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);

    // Default to the OV5647 setup
    strncpy(cameraModel, "OV5647", sizeof cameraModel);

    MMAL_PARAMETER_CAMERA_INFO_T param;
    param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
    param.hdr.size = sizeof(param) - 4;  // Deliberately undersize to check firmware version
    status = mmal_port_parameter_get(component->control, &param.hdr);

    if (status != MMAL_SUCCESS)
    {
        // Running on newer firmware
        param.hdr.size = sizeof(param);
        status = mmal_port_parameter_get(camera_info->control, &param.hdr);
        MMALException::throw_if(status, "Failed to get camera parameters.");
        MMALException::throw_if(param.num_cameras <= static_cast<uint32_t>(cameraNum), "Camera number not found.");
        // Take the parameters from the first camera listed.
        width = param.cameras[cameraNum].max_width;
        height = param.cameras[cameraNum].max_height;
        strncpy(cameraModel, param.cameras[cameraNum].camera_name, sizeof cameraModel);
        cameraModel[sizeof cameraModel - 1] = 0;
    }
    else {
        // default to OV5647 if nothing detected..
        width = 2592;
        height = 1944;
    }

    mmal_component_destroy(camera_info);

    /** Workaround for faulty model name. */
    if (!strcmp(cameraModel, "testc")) {
        strncpy(cameraModel, "imx477", sizeof cameraModel);
    }
   
    if (!strcmp(cameraModel, "imx477")) {
        xPixelSize = yPixelSize = 1.55F;
    }
    else if (!strcmp(cameraModel, "ov5647")) {
        xPixelSize = yPixelSize = 1.4F;
    }
    else if (!strcmp(cameraModel, "imx219")) {
        xPixelSize = yPixelSize = 1.12F;
    }
    else {
        throw MMALException("Unsupported camera");
    }

    LOGF_TEST("width=%d, height=%d", width, height);
}

/*
 *  Helper functions.
 */
void MMALCamera::selectCameraNumber(uint32_t n)
{
    MMALException::throw_if(mmal_port_parameter_set_uint32(component->control, MMAL_PARAMETER_CAMERA_NUM, n), "Could not select camera number");
}

void MMALCamera::selectSensorConfig(uint32_t config)
{
    MMALException::throw_if(mmal_port_parameter_set_uint32(component->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, config), "Could not set sensor mode");
}

void MMALCamera::configureCamera()
{
    //  set up the camera configuration
    MMAL_PARAMETER_CAMERA_CONFIG_T cam_config;

    cam_config.hdr.id = MMAL_PARAMETER_CAMERA_CONFIG;
    cam_config.hdr.size = sizeof cam_config;
    cam_config.max_stills_w = width;
    cam_config.max_stills_h = height;
    cam_config.stills_yuv422 = 0;
    cam_config.one_shot_stills = 1;
    cam_config.max_preview_video_w = 1024;  // Must really be set, even though we are not interested in a preview.
    cam_config.max_preview_video_h = 768;   // -''-
    cam_config.num_preview_video_frames = 1;
    cam_config.stills_capture_circular_buffer_height = 0;
    cam_config.fast_preview_resume = 0;
    cam_config.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;

    MMALException::throw_if(mmal_port_parameter_set(component->control, &cam_config.hdr), "Failed to set camera config");
}

void MMALCamera::getFPSRange()
{
    // Save cameras default FPS range.
    MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)}, {0, 0}, {0, 0}};
    MMALException::throw_if(mmal_port_parameter_get(component->output[CAPTURE_PORT_NO], &fps_range.hdr), "Failed to get FPS range");

    fps_low = fps_range.fps_low;
    fps_high = fps_range.fps_high;
}

void MMALCamera::enableOutput()
{
    assert(component);
    assert(component->output[0]);
    enablePort(component->output[CAPTURE_PORT_NO], false);
}

void MMALCamera::disableOutput()
{
    assert(component);
    assert(component->output[0]);
    disablePort(component->output[CAPTURE_PORT_NO]);
}
