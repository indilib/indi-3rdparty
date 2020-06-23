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
#include "mmalcamera.h"
#include "mmalencoder.h"
#include "mmallistener.h"
#include <vector>

class PixelListener;

class CameraControl : MMALListener
{
public:
    CameraControl();
    virtual ~CameraControl();
    void start_capture();
    void stop_capture();
    MMALCamera *get_camera() { return camera.get(); }
    void add_pixel_listener(PixelListener *l) { pixel_listeners.push_back(l); }

private:
    virtual void buffer_received(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) override;
    std::unique_ptr<MMALCamera> camera {};
    std::unique_ptr<MMALEncoder> encoder {};
    std::vector<PixelListener *> pixel_listeners;
};

#endif // CAMERACONTROL_H
