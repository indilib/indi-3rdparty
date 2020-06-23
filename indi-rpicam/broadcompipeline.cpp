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

void BroadcomPipeline::reset()
{
    pos = -1;
    memset(&header, 0, sizeof header);
}

void BroadcomPipeline::acceptByte(uint8_t byte)
{
    pos++;

    if (pos < 9) {
        header.BRCM[pos] = byte;
        if (pos == 8) {
            if (strcmp(header.BRCM, "@BRCMo") != 0) {
                throw std::runtime_error("Did not find BRCM header");
            }
        }
    }
    else if (pos < (signed)((9 + sizeof header.omx_data))) {
        reinterpret_cast<uint8_t *>(&header.omx_data)[pos - 9] = byte;
    }
    else if (pos >= 32768) {
        forward(byte);
    }
}

