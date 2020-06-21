#include <stdio.h>
#include <bcm_host.h>
#include <stdexcept>

#include <mmal_logging.h>
#include "cameracontrol.h"
#include "mmalexception.h"
#include "pixellistener.h"

CameraControl::CameraControl()
{
    camera.reset(new MMALCamera(0));
    encoder.reset(new MMALEncoder());
    encoder->add_port_listener(this);

    camera->connect(2, encoder.get(), 0); // Connected the capture port to the encoder.

    encoder->activate();
}

CameraControl::~CameraControl()
{
    fprintf(stderr, "CameraControl deleted\n");
}

/**
 * @brief CameraControl::buffer_received Buffer received to what ever we are listened to.
 * This method is responsible to feed image data to the indi-driver code.
 * @param port
 * @param buffer
 */
void CameraControl::buffer_received(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if (port->type == MMAL_PORT_TYPE_OUTPUT)
    {
        assert(buffer->type->video.planes == 1);

        if (buffer->length) {
            for(auto l : pixel_listeners) {
                l->pixels_received(buffer->data + buffer->offset, buffer->length);
            }
        }

        // Now flag if we have completed
        if (buffer->flags & (MMAL_BUFFER_HEADER_FLAG_EOS | MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED)) {
            for(auto l : pixel_listeners) {
                l->capture_complete();
            }
        }
    }
    else  {
        fprintf(stderr, "%s(%s): unsupported port type: %d\n", __FILE__, __func__, port->type);
    }
}

/**
 * @brief CameraControl::capture Start thread to perform the actual capture.
 */
void CameraControl::start_capture()
{
    camera->capture();
}

/**
 * @brief CameraControl::stop_capture Tell camera object to stop capturing.
 */
void CameraControl::stop_capture()
{
    camera->abort();
}
