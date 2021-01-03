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

void Raw10ToBayer16Pipeline::reset()
{
    x = 0;
    y = 0;
    raw_x = 0;
    state = b0;
}

void Raw10ToBayer16Pipeline::next_line()
{
    y += 1;
    cur_row = frame_buffer + (y * xRes);
    raw_x = 0;
    x = 0;
    state = b0;
}


void Raw10ToBayer16Pipeline::data_received(uint8_t *data,  uint32_t length)
{
    frame_buffer = reinterpret_cast<uint16_t *>(ccd->getFrameBuffer());
    raw_width = bcm_pipe->header.omx_data.raw_width;
    assert(raw_width == 4128 || raw_width == 3264);
    xRes = ccd->getXRes();
    yRes = ccd->getYRes();
    assert(xRes == 3280 || xRes == 2592);
    assert(yRes == 2464 || yRes == 1944);

    uint8_t byte;
    cur_row = frame_buffer + y * xRes;
    
    const uint32_t u32Magic = 0x4001; // bit expansion multiplier (2 pairs of bits at a time)
    const uint32_t u32Mask = 0x30003; // mask to preserve lower 2 bits of expanded values
    uint32_t u32Temp, u32_01, u32_23, *pu32;
    while(length > 0)
    {  
        //If we are aligned to the 4 pixel stride (state b0), try to do some bulk conversion 
        if(state == b0)
        {
            //Use 32bit aligned pixel chunks to convert 4 pixels  at the same time
            while(length >= 5 && x < xRes)
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
            }
            if(length == 0)
            {
                return;
            }
        }
        
        if (x >= xRes) {
            int diff = std::min(length, raw_width - raw_x);
            raw_x += diff;
            length -= diff;
            data += diff;
            if(raw_x >= raw_width)
                next_line();
            continue;
        }
        
        //IF we are not in b0 state, we are not aligned and do at least one cycle of byte 
        //wise processing until we are in b0 or buffer ends
        byte = *data;


        if ( x < xRes && y < yRes) {


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
                cur_row[x-1] |= byte & 0x03;
                cur_row[x-2] |= (byte >> 2) & 0x03;
                cur_row[x-3] |= (byte >> 4) & 0x03;
                cur_row[x-4] |= (byte >> 6) & 0x03;
                state = b0;
                break;
            }
        }
        length--;
        raw_x++;
        data++;
    }
    return;
}

