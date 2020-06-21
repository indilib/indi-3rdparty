#ifndef MMALCOMPONENT_H
#define MMALCOMPONENT_H

#include <vector>
#include <mmal_component.h>
#include <mmal_pool.h>


class MMALListener;
class MMAL_CONNECTION_T;

class MMALComponent
{
public:
    MMALComponent(const char *component_type);
    virtual ~MMALComponent();
    void connect(int src_port, MMALComponent *dst, int dst_port);
    void add_port_listener(MMALListener *l) { port_listeners.push_back(l); }

protected:
    virtual void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    virtual void return_buffer(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    void enable_port_with_callback(MMAL_PORT_T *port);
    MMAL_COMPONENT_T *component {};
    // FIXME: Support multiple connections.
    MMAL_CONNECTION_T *connection {};

private:
    static void c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    std::vector<MMALListener *>port_listeners;
};

#endif // MMALCOMPONENT_H
