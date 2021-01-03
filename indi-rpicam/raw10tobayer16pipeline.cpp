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
#include <algorithm>

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
    frame_buffer = reinterpret_cast<uint16_t *>(ccd->getFrameBuffer());
    x = 0;
    y = 0;
    raw_x = 0;
    state = 0;
    startRawX = (ccd->getSubX() / 4) * 5;
    startRawY = ccd->getSubY();
    raw_y = 0;
    bytes_consumed = 0;
}

void Raw10ToBayer16Pipeline::next_line(uint32_t maxX)
{
    y += 1;
    raw_y += 1;
    cur_row = frame_buffer + (y * maxX);
    raw_x = 0;
    x = 0;
    state = 0;
}


void Raw10ToBayer16Pipeline::data_received(uint8_t *data,  uint32_t length)
{
    //auto buffer_length = length;
    //frame_buffer = reinterpret_cast<uint16_t *>(ccd->getFrameBuffer()); //This should be in reset() but ccd seems to be not always available there
    raw_width = bcm_pipe->header.omx_data.raw_width;
    assert(raw_width == 4128 || raw_width == 3264);
    xRes = ccd->getXRes();
    yRes = ccd->getYRes();
    assert(xRes == 3280 || xRes == 2592);
    assert(yRes == 2464 || yRes == 1944);

    uint32_t maxX = ccd->getSubW();
    uint32_t maxY = ccd->getSubH()-1;

    //Skip ahead to start of subframe
    auto offset_start = startRawY * raw_width + startRawX;
    auto offset_end = (startRawY + maxY) * raw_width + startRawX + ((maxX * 5) / 4);
    if(bytes_consumed < offset_start)
    {
        auto diff = std::min(length, offset_start - bytes_consumed);
        length -= diff;
        data += diff;
        bytes_consumed += diff;
        raw_x = bytes_consumed % raw_width;
        raw_y = bytes_consumed / raw_width;
        x = 0;
    }
    //Skip ahead to end of frame if we are beyond the sub frame limits
    else if (bytes_consumed > offset_end)
    {
        bytes_consumed += length;
        return;
    }

    uint8_t byte;
    const uint32_t u32Magic = 0x4001; // bit expansion multiplier (2 pairs of bits at a time)
    const uint32_t u32Mask = 0x30003; // mask to preserve lower 2 bits of expanded values
    uint32_t u32Temp, u32_01, u32_23, *pu32;
    cur_row = frame_buffer + y * maxX;
    
    //At this point we are for sure at y > startRawY
    while(length > 0 && bytes_consumed < offset_end)
    {  
        //If we are aligned to the 4 pixel stride (state 0), try to do some bulk conversion 
        if(state == 0)
        {
            //Use 32bit aligned pixel chunks to convert 4 pixels  at the same time
            while(length >= 5 && x < maxX && raw_x >= startRawX)
            {
                assert(x % 4 == 0);
                pu32 = (uint32_t *)(&cur_row[x]);
                u32_01 = (*data++ << 18);
                u32_01 |= (*data++ << 2); // each 32-bit value will hold 2 source bytes spread out to 16-bits
                u32_23 = (*data++ << 18); // and shifted over 2 to make room for lower 2 bits
                u32_23 |= (*data++ << 2);
                u32Temp = *data++ * u32Magic; // 5th byte contains 4 pairs of bits (0/1) for the 4 pixels
                u32_01 |= (u32Temp & u32Mask); // combine lower 2 bits to bytes 0 and 1
                u32Temp >>= 4; // shift down to access bits for bytes 2/3
                u32_23 |= (u32Temp & u32Mask);
                *pu32++ = u32_01; // store 4 16-bit pixels (10 significant bits)
                *pu32++ = u32_23;
          	    length -= 5;
            	x += 4;
                raw_x += 5;
                bytes_consumed += 5;
            }
            if(length == 0)
            {
                return;
            }
        }
        
        // Skip over bytes outside of sub frame
        if(raw_x < startRawX || x >= maxX)
        {
            uint32_t diff;
            if(raw_x < startRawX)
                diff = std::min(length, startRawX - raw_x);
            else
                diff = std::min(length, raw_width - raw_x);
            raw_x += diff;
            length -= diff;
            data += diff;
            bytes_consumed += diff;

            //Start a new line if necessary
            if(raw_x >= raw_width)
                next_line(maxX);
            continue;
        }
        
        //IF we are not in state 0, we are not aligned and do at least one cycle of byte 
        //wise processing until we are in state 0 or buffer ends
        byte = *data;

        //At this point we are for sure within the raw y coordinates of the subframe and only need to check x range
        if (raw_x >= startRawX && x < maxX) {
            // RAW according to experiment.
            switch(state)
            {
            case 0:
                // FIXME: Optimize, if at least 5 bytes remaining here, all data can be calculated faster in one step.
	        	// FIXME: upp the data to upper bits.
                cur_row[x] = static_cast<uint16_t>(byte << 2);
                x++;
                state = 1;
                break;

            case 1:
                cur_row[x] = static_cast<uint16_t>(byte << 2);
                x++;
                state = 2;
                break;

            case 2:
                cur_row[x] = static_cast<uint16_t>(byte << 2);
                x++;
                state = 3;
                break;

            case 3:
                cur_row[x] = static_cast<uint16_t>(byte << 2);
                x++;
                state = 4;
                break;

            case 4:
                cur_row[x-1] |= byte & 0x03;
                cur_row[x-2] |= (byte >> 2) & 0x03;
                cur_row[x-3] |= (byte >> 4) & 0x03;
                cur_row[x-4] |= (byte >> 6) & 0x03;
                state = 0;
                break;
            }
        }
        length--;
        raw_x++;
        data++;
        bytes_consumed++;
    }
    return;
}

