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
#include <indiccdchip.h>

#include "raw10tobayer16pipeline.h"
#include "broadcompipeline.h"

void Raw10ToBayer16Pipeline::reset()
{
    x = 0;
    y = 0;
    raw_x = 0;
    pos = 0;
    state = b0;
}

void Raw10ToBayer16Pipeline::acceptByte(uint8_t byte)
{
    if (raw_x >= bcm_pipe->header.omx_data.raw_width) {
        y += 1;
        x = 0;
        raw_x = 0;
    }

    assert(bcm_pipe->header.omx_data.raw_width == 4128);
    assert(ccd->getXRes() == 3280);
    assert(ccd->getYRes() == 2464);

    if ( x < ccd->getXRes() && y < ccd->getYRes()) {
        uint16_t *cur_row = reinterpret_cast<uint16_t *>(ccd->getFrameBuffer()) + y * ccd->getXRes();

        assert((cur_row - reinterpret_cast<uint16_t *>(ccd->getFrameBuffer())) % 3280 == 0);

        // RAW according to experiment.
        switch(state)
        {
        case b0:
            cur_row[x] = static_cast<uint16_t>(byte << 2);
            x++;
            state = b1;
            break;

        case b1:
            cur_row[x] = static_cast<uint16_t>(byte << 2);
            x++;
            state = b2;
            break;

        case b2:
            cur_row[x] = static_cast<uint16_t>(byte << 2);
            x++;
            state = b3;
            break;

        case b3:
            cur_row[x] = static_cast<uint16_t>(byte << 2);
            x++;
            state = b4;
            break;

        case b4:
            cur_row[x-4] |= byte & 0x03;
            cur_row[x-3] |= (byte >> 2) & 0x03;
            cur_row[x-2] |= (byte >> 4) & 0x03;
            cur_row[x-1] |= (byte >> 6) & 0x03;
            state = b0;
            break;
        }
    }

    pos++;
    raw_x++;
}


