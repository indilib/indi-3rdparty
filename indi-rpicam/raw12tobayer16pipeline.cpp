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

/**
 * Decoding the RAW12 format (not the official one, the Broadcom one) which is rows of:
 * [ Bh ] [ Gh ] [ Bl | Gl ] ...
 * [ Gh ] [ Rh ] [ Gl | Rl ] ...
 *
 * h = high 8 bits, l = low 4 bits
 *
 * If subframes are used. The mapping from subframe image start x to first RAW12 x in received buffer is as:
 * x pixel:     0  1  -  2  3  -  4  5  -  6  7  -
 *                       |
 *                       V
 * Raw12 byte:  0  1  2  3  4  5  6  7  8  9  10 11
 *              B  G  bg B  G  bg B  G  bg B  G  bg
 *
 * To simplify, start all raw lines on bayer group boundry
 * startRawX = (getSubX() / 2) * 3
 */

void Raw12ToBayer16Pipeline::reset()
{
    x = 0;
    y = 0;
    state = 0;
    startRawX = (ccd->getSubX() / 2) * 3;
    raw_x = 0;
    raw_y = 0;
}

void Raw12ToBayer16Pipeline::data_received(uint8_t *data,  uint32_t length)
{
    uint8_t byte;
    uint8_t byte2;
    uint8_t byte3;

    assert(bcm_pipe->header.omx_data.raw_width == 6112);
    assert(ccd->getXRes() == 4056);
    assert(ccd->getYRes() == 3040);

    int maxX = ccd->getSubW();
    int maxY = ccd->getSubH();

    for(; length; data++, length--)
    {
        byte = *data;

        if (raw_x >= bcm_pipe->header.omx_data.raw_width)
        {
            x = 0;
            raw_x = 0;
            state = 0;

            raw_y++;
            if (raw_y > ccd->getSubY())
            {
                y += 1;
            }
        }

        if (raw_x >= startRawX && raw_y >= ccd->getSubY() && x < maxX && y < maxY)
        {
            uint16_t *cur_row = reinterpret_cast<uint16_t *>(ccd->getFrameBuffer()) + y * ccd->getSubW();

            // Is it safe to optimize?
            if (state == 0 && length > 3 && raw_x <= bcm_pipe->header.omx_data.raw_width - 3)
            {
                byte2 = *data++;
                byte3 = *data++;
                raw_x += 2;
                length -= 2;
                cur_row[x + 0] = byte  << 8;
                cur_row[x + 1] = byte2 << 8;
                cur_row[x + 0] |= static_cast<uint16_t>((byte3 & 0x0F) << 4);
                cur_row[x + 1] |= static_cast<uint16_t>((byte3 & 0xF0) << 0);
                x += 2;

            }
            else
            {

                // RAW according to experiment.
                switch(state)
                {
                    case 0:
                        cur_row[x] = byte << 8;
                        state = 1;
                        break;

                    case 1:
                        cur_row[x + 1] = byte << 8;
                        state = 2;
                        break;

                    case 2:
                        cur_row[x + 0] |= static_cast<uint16_t>((byte & 0x0F) << 4);
                        cur_row[x + 1] |= static_cast<uint16_t>((byte & 0xF0) << 0);
                        x += 2;
                        state = 0;
                        break;
                }
            }
        }

        raw_x++;
    }
}
