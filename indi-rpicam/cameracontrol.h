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

#ifndef CAMERACONTROL_H
#define CAMERACONTROL_H

#include <memory>
#include <bcm_host.h>
#include <chrono>
#include <unordered_set>

#include "mmalcamera.h"
#include "mmalencoder.h"
#include "mmalbufferlistener.h"
#include "capturelistener.h"

class Pipeline;

/**
 * @brief The CameraControl class Initializes all MMAL components and connections.
 * Also sets up and handles callbacks to receivers of the image.
 */
class CameraControl : MMALBufferListener
{
public:
    CameraControl();
    virtual ~CameraControl();
    void startCapture();
    void stopCapture();
    MMALCamera *get_camera() { return camera.get(); }
    void add_pipeline(Pipeline *p) { pipelines.insert(p); }
    void add_capture_listener(CaptureListener *c) { capture_listeners.insert(c); }
    void setGain(double gain) { this->gain = gain; }
    void setShutterSpeed(uint32_t shutter_speed)  { this->shutter_speed = shutter_speed; }

protected: 
    std::unique_ptr<MMALCamera> camera {};
    std::unique_ptr<MMALEncoder> encoder {};
    virtual void buffer_received(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) override;

private:
    std::unordered_set<Pipeline *> pipelines;
    std::unordered_set<CaptureListener *> capture_listeners;
    void signal_complete();
    void signal_data_received(uint8_t *data, uint32_t length);
    std::chrono::steady_clock::time_point start_time;
    bool print_first {true};
    double gain {1};
    uint32_t shutter_speed {100000};
    bool is_capturing {false};

#ifndef NDEBUG
    std::chrono::duration <double> buffer_processing_time {};
#endif
};

#endif // CAMERACONTROL_H
