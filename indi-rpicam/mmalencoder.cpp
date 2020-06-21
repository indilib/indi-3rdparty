
#include <bcm_host.h>
#include <mmal_default_components.h>
#include <mmal_types.h>
#include <mmal_parameters.h>
#include <mmal_pool.h>
#include <mmal_port.h>
#include <mmal_util.h>
#include <mmal_util_params.h>
#include <mmal_logging.h>

#include "mmalencoder.h"
#include "mmallistener.h"
#include "mmalexception.h"

MMALEncoder::MMALEncoder() : MMALComponent(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER)
{
    MMAL_STATUS_T status;

    MMAL_PORT_T *output = component->output[0];
    mmal_format_copy(output->format, component->input[0]->format);

    if (output->buffer_size < output->buffer_size_min) {
        output->buffer_size = output->buffer_size_min;
    }

    output->buffer_num = output->buffer_num_recommended;
    if (output->buffer_num < output->buffer_num_min) {
        output->buffer_num = output->buffer_num_min;
    }

    /* Seems its only the JPEG encoding that actually returns the true raw-format from the HQ-camera */
    output->format->encoding = MMAL_ENCODING_JPEG; output->format->encoding_variant = 0;

    status = mmal_port_format_commit(output);
    MMALException::throw_if(status, "Failed to commit encoder output");

    status = mmal_port_parameter_set_uint32(output, MMAL_PARAMETER_JPEG_Q_FACTOR, 85);
    MMALException::throw_if(status, "Failed to set JPEG quality");

    status = mmal_port_parameter_set_uint32(output, MMAL_PARAMETER_JPEG_RESTART_INTERVAL, 0);
    MMALException::throw_if(status, "Failed to set JPEG restart interval");

    status = mmal_component_enable(component);
    MMALException::throw_if(status, "Failed enable encoder");

    pool = mmal_port_pool_create(output, output->buffer_num, output->buffer_size);
    if (pool== nullptr) {
        MMALException::throw_if(status, "To create pool");
    }
}

/**
 * @brief MMALEncoder::activate Enables the output and activates the encoder.
 */
void MMALEncoder::activate()
{
    MMAL_STATUS_T status;

    enable_port_with_callback(component->output[0]);

    unsigned int num = mmal_queue_length(pool->queue);
    for (unsigned int q = 0; q < num; q++)
    {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
        MMALException::throw_if(buffer == nullptr, "Failed to get pool buffer");
        status = mmal_port_send_buffer(component->output[0], buffer);
        MMALException::throw_if(status, "Failed to send buffer to port");
    }
}

MMALEncoder::~MMALEncoder()
{
    if (component->output[0]->is_enabled) {
        mmal_port_disable(component->output[0]);
    }
    if (pool) {
        mmal_port_pool_destroy(component->output[0], pool);
        pool = nullptr;
    }
}

/**
 * @brief MMALEncoder::return_buffer Returns the buffer to the pool.
 * Only the object that caused the callback can know which pool it belongs to since that info
 * is not available with the port object.
 * @param port
 * @param buffer
 */
void MMALEncoder::return_buffer(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled)
    {
        MMAL_STATUS_T status = MMAL_SUCCESS;
        MMAL_BUFFER_HEADER_T *new_buffer;

        new_buffer = mmal_queue_get(pool->queue);

        if (new_buffer)
        {
            status = mmal_port_send_buffer(port, new_buffer);
        }
        if (!new_buffer || status != MMAL_SUCCESS) {
            vcos_log_error("Unable to return a buffer to the camera port");
        }
    }
}
