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

#include <iostream>
#include <cassert>

#include "raw12tobayer16pipeline.h"
#include "broadcompipeline.h"
#include "chipwrapper.h"

#include <fstream>

void Raw12ToBayer16Pipeline::reset()
{
    x = 0;
    y = 0;
    raw_x = 0;
    state = b0;
}

void Raw12ToBayer16Pipeline::data_received(uint8_t *data,  uint32_t length)
{
    uint8_t byte;

    assert(bcm_pipe->header.omx_data.raw_width == 6112);
    assert(ccd->getXRes() == 4056);
    assert(ccd->getYRes() == 3040);

    for(;length; data++, length--)
    {
        byte = *data;

        if (raw_x >= bcm_pipe->header.omx_data.raw_width) {
            y += 1;
            x = 0;
            raw_x = 0;
        }

        if ( x < ccd->getXRes() && y < ccd->getYRes()) {
            uint16_t *cur_row = reinterpret_cast<uint16_t *>(ccd->getFrameBuffer()) + y * ccd->getXRes();

            // RAW according to experiment.
            switch(state)
            {
            case b0:
                // FIXME: Optimize, if at least 3 bytes remaining here, all data can be calculated faster in one step.
                cur_row[x] = byte << 8;
                state = b1;
                break;

            case b1:
                cur_row[x+1] = byte << 8;
                state = b2;
                break;

            case b2:
                cur_row[x+0] |= static_cast<uint16_t>((byte & 0x0F) << 4);
                cur_row[x+1] |= static_cast<uint16_t>((byte & 0xF0) << 0);
                x += 2;
                state = b0;
                break;
            }
        }

        raw_x++;
    }
}
