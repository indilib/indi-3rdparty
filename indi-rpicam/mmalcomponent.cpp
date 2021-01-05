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

#define MMAL_COMPONENT_USERDATA_T MMALComponent // Not quite C++ but thats how its supposed to be used I think.

#include <stdio.h>
#include <mmal_types.h>
#include <mmal_default_components.h>
#include <mmal_pool.h>
#include <mmal_component.h>
#include <mmal_connection.h>

#include "inditest.h"
#include "mmalcomponent.h"
#include "mmalexception.h"
#include "mmalbufferlistener.h"

MMALComponent::MMALComponent(const char *component_type)
{
    MMAL_STATUS_T status;

    /* Create the component */
    status = mmal_component_create(component_type, &component);
    MMALException::throw_if(status, "Failed to create component");
    component->userdata = this; // c_port_callback needs this to find this object.
}

MMALComponent::~MMALComponent()
{
    // FIXME: Disable all inputs and outputs before closing the component.
    if (component) {
        mmal_component_disable(component);
        mmal_component_destroy(component);
        component = nullptr;
    }
}

void MMALComponent::enablePort(MMAL_PORT_T *port, bool use_callback)
{
    if (use_callback) {
        MMALException::throw_if(mmal_port_enable(port, c_port_callback), "Failed to enable port on component %s", component->name);
    }
    else {
        MMALException::throw_if(mmal_port_enable(port, nullptr), "Failed to enable port on component %s", component->name);
    }
}

void MMALComponent::c_port_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    MMALComponent *p = dynamic_cast<MMALComponent *>(port->component->userdata);
    p->port_callback(port, buffer);
}

/**
 *  Fan out port callbacks to all listeners of this component.
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
void MMALComponent::port_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    mmal_buffer_header_mem_lock(buffer);

    for(auto *l : buffer_listeners)
    {
        l->buffer_received(port, buffer);
    }

    mmal_buffer_header_mem_unlock(buffer);

    return_buffer(port, buffer);
}

void MMALComponent::connect(int src_port, MMALComponent *dst, int dst_port)
{
    MMAL_STATUS_T status;

    MMALException::throw_if(connection, "Only one connection supported");
    assert(dst);

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

void MMALComponent::disconnect()
{
    if (!connection) {
        throw MMALException("%s: no connection found", __FUNCTION__);
    }

   MMALException::throw_if(mmal_connection_destroy(connection), "Failed to release connection");
   connection = nullptr;
}

void MMALComponent::add_buffer_listener(MMALBufferListener *l)
{
    if (l == nullptr) {
        throw MMALException("Null pointer passed to MMALComponent::add_buffer_listener");
    }
    buffer_listeners.push_back(l);
}

void MMALComponent::enableComponent()
{
    LOGF_TEST("enabling %s", component->name);
    MMALException::throw_if(mmal_component_enable(component), "Failed enable component %s", component->name);
}

void MMALComponent::disableComponent()
{
    LOGF_TEST("disabeling %s", component->name);
    MMALException::throw_if(mmal_component_disable(component), "Failed enable component %s", component->name);
}
