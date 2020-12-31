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

#ifndef _MMAL_CAMERA_H
#define _MMAL_CAMERA_H

class MMALCamera;
class MMALEncoder;

#include <vector>
#include <stdexcept>
#include <mmal.h>

#include "mmalcomponent.h"
#include "mmallistener.h"

class MMALCamera : public MMALComponent
{
public:
    MMALCamera(int cameraNum = 0);
    virtual ~MMALCamera();
    static const int MMAL_CAMERA_PREVIEW_PORT {0};
    static const int MMAL_CAMERA_VIDEO_PORT {1};
    static const int MMAL_CAMERA_CAPTURE_PORT {2};

    void set_shutter_speed_us(long shutter_speed)  { this->shutter_speed = shutter_speed; }
    void set_iso(int iso) { this->iso = iso; }
    void set_gain(double gain) { this->gain = gain; }
    int capture();
    void abort();
    uint32_t get_width() { return width; }
    uint32_t get_height() { return height; }
    const char *get_name() { return cameraName; }

private:
    void create_camera_component();
    void create_encoder();
    void set_capture_port_format();
    void get_sensor_info();
    void set_camera_parameters();

    int32_t cameraNum {};
    char cameraName[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN] {};
    uint32_t shutter_speed {100000};
    unsigned int iso {0};
    double gain {1};
    uint32_t width {};
    uint32_t height {};
    MMAL_RATIONAL_T fps_low {}, fps_high {};
};

#endif // _MMAL_CAMERA_H
