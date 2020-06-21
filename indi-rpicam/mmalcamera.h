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
