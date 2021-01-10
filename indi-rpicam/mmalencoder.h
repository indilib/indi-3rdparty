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

#ifndef MMALENCODER_H
#define MMALENCODER_H

#include <mmal.h>
#include <vector>

#include "mmalcomponent.h"
#include "mmalbufferlistener.h"

/**
 * @brief The MMALEncoder class is a C++ wrapper around the pure MMAL encoder component.
 */
class MMALEncoder : public MMALComponent
{
public:
    MMALEncoder();
    virtual ~MMALEncoder();
    void enableOutput();
    void disableOutput();

private:
    virtual void return_buffer(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) override;
    MMAL_POOL_T *pool {};
};

#endif // MMALENCODER_H
