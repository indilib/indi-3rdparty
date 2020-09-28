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

#ifndef RAW10TOBAYER16PIPELINE_H
#define RAW10TOBAYER16PIPELINE_H

#include <cstddef>
#include "pipeline.h"

struct BroadcomPipeline;
namespace INDI {
class CCDChip;
};

/**
 * @brief The Raw10ToBayer16Pipeline class
 * Accepts bytes in raw10 format and writes 16 bits bayer image.
 * Format of first line is: | B | G | B | G |  {lower 2 bits for the earlier 4 bytes} |
 * Second line is G R ...
 */
class Raw10ToBayer16Pipeline : public Pipeline
{
public:
    Raw10ToBayer16Pipeline(const BroadcomPipeline *bcm_pipe, INDI::CCDChip *ccd) : Pipeline(), bcm_pipe(bcm_pipe), ccd(ccd) {}

    virtual void acceptByte(uint8_t byte) override;
    virtual void reset() override;

private:
    const BroadcomPipeline *bcm_pipe;
    INDI::CCDChip *ccd;
    int x {0};
    int y {0};
    int pos {0};
    int raw_x {0};  // Position in the raw-data comming in.
    enum {b0, b1, b2, b3, b4} state = b0; // Which byte in the RAW10 format.
};

#endif // RAW10TOBAYER16PIPELINE_H
