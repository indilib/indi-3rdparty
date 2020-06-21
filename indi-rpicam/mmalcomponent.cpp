#define MMAL_COMPONENT_USERDATA_T MMALComponent // Not quite C++ but thats how its supposed to be used I think.

#include <stdio.h>
#include <mmal_types.h>
#include <mmal_default_components.h>
#include <mmal_pool.h>
#include <mmal_component.h>
#include <mmal_connection.h>

#include "mmalcomponent.h"
#include "mmalexception.h"
#include "mmallistener.h"

MMALComponent::MMALComponent(const char *component_type)
{
    MMAL_STATUS_T status;

    /* Create the component */
    status = mmal_component_create(component_type, &component);
    MMALException::throw_if(status, "Failed to create component");
    component->userdata = this; // c_callback needs this to find this object.
}

MMALComponent::~MMALComponent()
{
    // FIXME: Disable all inputs and outputs before closing the component.
    if (component) {
        mmal_component_disable(component);
        mmal_component_destroy(component);
        component = nullptr;
        fprintf(stderr, "component disabled and destroyed\n");
    }
}

void MMALComponent::enable_port_with_callback(MMAL_PORT_T *port)
{
    MMAL_STATUS_T status = mmal_port_enable(port, c_callback);
    MMALException::throw_if(status, "Failed to enable port");
}

void MMALComponent::c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    MMALComponent *p = dynamic_cast<MMALComponent *>(port->component->userdata);
    p->callback(port, buffer);
}

/**
 *  Fan out port callbacks.
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
void MMALComponent::callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    mmal_buffer_header_mem_lock(buffer);

    try {
        for(auto *l : port_listeners)
        {
            l->buffer_received(port, buffer);
        }
    }
    catch (std::exception &e) {
        mmal_buffer_header_mem_unlock(buffer);
        throw e;
    }

    mmal_buffer_header_mem_unlock(buffer);

    return_buffer(port, buffer);
}

void MMALComponent::return_buffer(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    (void)port;
    (void)buffer;
    throw MMALException("MMALComponent::return_buffer: No one there to recyle buffers, please override this method.");
}

void MMALComponent::connect(int src_port, MMALComponent *dst, int dst_port)
{
   MMAL_STATUS_T status;

   MMALException::throw_if(connection, "Only one connection supported");

   status =  mmal_connection_create(&connection, component->output[src_port], dst->component->input[dst_port], MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
   MMALException::throw_if(status, "Failed to connect components");
   if (status == MMAL_SUCCESS)
   {
      status =  mmal_connection_enable(connection);
      if (status != MMAL_SUCCESS) {
         mmal_connection_destroy(connection);
         connection = nullptr;
         throw MMALException("Failed to enable connection");
      }
   }
}

