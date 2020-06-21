#ifndef MMALENCODER_H
#define MMALENCODER_H

#include <mmal.h>
#include <vector>

#include "mmalcomponent.h"
#include "mmallistener.h"


class MMALEncoder : public MMALComponent
{
public:
    MMALEncoder();
    virtual ~MMALEncoder();
    void activate();

protected:
    virtual void return_buffer(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) override;

private:
    MMAL_POOL_T *pool {};
};

#endif // MMALENCODER_H
