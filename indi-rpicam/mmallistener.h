#ifndef MMALLISTENER_H
#define MMALLISTENER_H

#include <mmal_port.h>
#include <mmal_buffer.h>

class MMALListener
{
public:
    virtual void buffer_received(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) = 0;
};

#endif // MMALLISTENER_H
