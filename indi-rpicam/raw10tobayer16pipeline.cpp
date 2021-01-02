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

#include "raw10tobayer16pipeline.h"
#include "broadcompipeline.h"
#include "chipwrapper.h"

/**
 * Decoding the RAW11 format which is rows of:
 * [ B1h ] [ G1h ] [ B2h ] [ G2h ] [ B1l | G1l | B2l | G2l ] ...
 *
 * h = high 8 bits, l = low 2 bits
 *
 * If subframes are used. The mapping from subframe image start x to first RAW12 x in received buffer is as:
 * x pixel:     0  1  2  3  -  4  5  6  7  -
 *                       |
 *                       V
 * Raw12 byte:  0  1  2  3  4  5  6  7  8  9  10 11
 *              B1 G1 B2 G2 mix B  G  bg B  G  bg
 *
 * To simplify, start all raw lines on bayer group boundry
 * startRawX = (getSubX() / 4) * 5
 */


void Raw10ToBayer16Pipeline::reset()
{
    x = 0;
    y = 0;
    raw_x = 0;
    state = 0;
    startRawX = (ccd->getSubX() / 4) * 5;
    raw_y = 0;
}

void Raw10ToBayer16Pipeline::data_received(uint8_t *data,  uint32_t length)
{
    assert(bcm_pipe->header.omx_data.raw_width == 4128);
    assert(ccd->getXRes() == 3280);
    assert(ccd->getYRes() == 2464);

    int maxX = ccd->getSubW();
    int maxY = ccd->getSubH();

    uint8_t byte;
    for(;length; data++, length--)
    {
        byte = *data;

        if (raw_x >= bcm_pipe->header.omx_data.raw_width) {
            x = 0;
            raw_x = 0;
            state = 0;
	    raw_y++;
            if (raw_y > ccd->getSubY()) {
                y += 1;
            }
        }

        if (raw_x >= startRawX && raw_y >= ccd->getSubY() && x < maxX && y < maxY) {
            uint16_t *cur_row = reinterpret_cast<uint16_t *>(ccd->getFrameBuffer()) + y * ccd->getSubW();

            // RAW according to experiment.
            switch(state)
            {
            case 0:
                // FIXME: Optimize, if at least 5 bytes remaining here, all data can be calculated faster in one step.
		// FIXME: upp the data to upper bits.
                cur_row[x] = static_cast<uint16_t>(byte << 8);
                x++;
                state = 1;
                break;

            case 1:
                cur_row[x] = static_cast<uint16_t>(byte << 8);
                x++;
                state = 2;
                break;

            case 2:
                cur_row[x] = static_cast<uint16_t>(byte << 8);
                x++;
                state = 3;
                break;

            case 3:
                cur_row[x] = static_cast<uint16_t>(byte << 8);
                x++;
                state = 4;
                break;

            case 4:
                cur_row[x-4] |= (byte & 0x03) << 6;
                cur_row[x-3] |= ((byte >> 2) & 0x03) << 6;
                cur_row[x-2] |= ((byte >> 4) & 0x03) << 6;
                cur_row[x-1] |= ((byte >> 6) & 0x03) << 6;
                state = 0;
                break;
            }
        }

        raw_x++;
    }
}


