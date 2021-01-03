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

#include <cstring>
#include <stdexcept>
#include "broadcompipeline.h"
#include "inditest.h"

void BroadcomPipeline::reset()
{
    memset(&header, 0, sizeof header);
    state = State::WANT_BRCMO;
    pos = 0;
}

void BroadcomPipeline::data_received(uint8_t  *data,  uint32_t length)
{
    uint8_t byte;
    for(;length; data++, length--)
    {
        byte = *data;
        switch(state)
        {
        case State::FORWARDING:
            // Fast lane.
            forward(data, length);
            return;

        case State::WANT_BRCMO:
            if (pos >= sizeof header.BRCM) {
                throw std::runtime_error("Did not find BRCMo header");
            }
            header.BRCM[pos++] = byte;
            if (pos >= 8 && strncmp(header.BRCM + pos - 8, "BRCMo", 5) == 0) {
                state = State::WANT_OMX_DATA;
                pos = 0;
            }
            break;

        case State::WANT_OMX_DATA:
            if (pos < (signed)((sizeof header.omx_data))) {
                reinterpret_cast<uint8_t *>(&header.omx_data)[pos] = byte;
            }
            else if (pos >= (32767 - 8)) {
                LOG_TEST("finished broadcom processing");
                state = State::FORWARDING;
            }
            pos++;
            break;
        }
    }
}
