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

#ifndef RAW12TOBAYER16PIPELINE_H
#define RAW12TOBAYER16PIPELINE_H

#include <cstddef>
#include "pipeline.h"

struct BroadcomPipeline;
class ChipWrapper;

/**
 * @brief The Raw12ToBayer16Pipeline class
 * Accepts bytes in raw12 format and writes 16 bits bayer image.
 * RAW12 format is like | {R03,R02,R01,R00,G11,G10,G09,G08} | {R11,R10,R09,R08,R07,R06,R05,R04} | {G07,G06,G05,G04,G03,G02,G01} |
 *                                   b1                                      b2                               b3
 * Odd lines are swapped R->G, G-B
 */
class Raw12ToBayer16Pipeline : public Pipeline
{
public:
    Raw12ToBayer16Pipeline(const BroadcomPipeline *bcm_pipe, ChipWrapper *ccd) : Pipeline(), bcm_pipe(bcm_pipe), ccd(ccd) {}

    virtual void data_received(uint8_t *data,  uint32_t length) override;
    virtual void reset() override;

private:
    const BroadcomPipeline *bcm_pipe;
    ChipWrapper *ccd;
    int x {0};
    int y {0};
    int raw_x {0}; //! Position in the raw-data comming in.
    int raw_y {0}; //! Position in the raw-data comming in.
    int startRawX {0};
    uint8_t state = 0; //! Which byte in the RAW12 format (see above).
};

#endif // RAW12TOBAYER16PIPELINE_H
