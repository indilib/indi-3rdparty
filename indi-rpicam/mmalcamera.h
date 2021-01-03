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
#include "mmalbufferlistener.h"
#include "config.h"

/**
 * @brief The MMALCamera class is a C++ wrapper around the pure MMAL camera component.
 */

class MMALCamera : public MMALComponent
{
public:
    MMALCamera(int cameraNum = 0);
    virtual ~MMALCamera();
    static const int PREVIEW_PORT_NO {0};
    static const int VIDEO_PORT_NO {1};
    static const int CAPTURE_PORT_NO {2};

    /** Return the current active shutter speed for camera.h */
    uint32_t getShutterSpeed();

#ifdef USE_ISO
    void setISO(int iso) { this->iso = iso; }
#endif
    void startCapture();
    void stopCapture();
    uint32_t get_width() { return width; }
    uint32_t get_height() { return height; }
    const char *getModel() { return cameraModel; }
    void set_crop(int x, int y, int w, int h);
    void setCapturePortFormat();
    void setExposureParameters(double gain, uint32_t shutter_speed);
    void getSensorInfo();
    void selectCameraNumber(uint32_t n);
    void selectSensorConfig(uint32_t config);
    void configureCamera();
    void getFPSRange();
    void setFPSRange(MMAL_RATIONAL_T low, MMAL_RATIONAL_T high);

    float xPixelSize {}, yPixelSize {};

private:
    virtual void return_buffer(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) override { (void)port; (void)buffer; }
    int32_t cameraNum {};
    char cameraModel[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN] {""};
    unsigned int iso {0};
    uint32_t width {};
    uint32_t height {};
    MMAL_RATIONAL_T fps_low {}, fps_high {};
    MMAL_RECT_T crop {0, 0, 0x1000, 0x1000};
};

#endif // _MMAL_CAMERA_H
