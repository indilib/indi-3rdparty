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
