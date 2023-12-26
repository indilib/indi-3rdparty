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
class ChipWrapper;

/**
 * @brief The Raw10ToBayer16Pipeline class
 * Accepts bytes in raw10 format and writes 16 bits bayer image.
 * Format of first line is: | B | G | B | G |  {lower 2 bits for the earlier 4 bytes} |
 * Second line is G R ...
 */
class Raw10ToBayer16Pipeline : public Pipeline
{
public:
    Raw10ToBayer16Pipeline(const BroadcomPipeline *bcm_pipe, ChipWrapper *ccd) : Pipeline(), bcm_pipe(bcm_pipe), ccd(ccd) {}

    virtual void data_received(uint8_t *data,  uint32_t length) override;
    virtual void reset() override;

private:
    void next_line(uint32_t maxX);
    const BroadcomPipeline *bcm_pipe;
    ChipWrapper *ccd;
    uint32_t x {0};
    uint32_t y {0};
    uint32_t raw_x {0};  // Position in the raw-data comming in.
    uint32_t raw_y {0};  //! Position in the raw-data comming in.
    uint32_t startRawX {0};
    uint32_t startRawY {0};
    uint8_t state = 0;
    uint16_t* frame_buffer;
    uint32_t raw_width{0};
    uint32_t xRes, yRes = 0;
    uint16_t* cur_row {0};
    uint32_t bytes_consumed = 0;
};

#endif // RAW10TOBAYER16PIPELINE_H
