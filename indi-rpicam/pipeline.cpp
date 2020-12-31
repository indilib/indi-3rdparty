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

#include <stdexcept>
#include "pipeline.h"

Pipeline::Pipeline()
{

}

Pipeline::~Pipeline()
{
    if (nextPipeline) {
        delete nextPipeline;
    }
}

void Pipeline::daisyChain(Pipeline *p)
{
    Pipeline *last = this;
    while(last->nextPipeline != nullptr) {
        last = last->nextPipeline;
    }
    last->nextPipeline = p;
}

void Pipeline::forward(uint8_t byte)
{
    if (nextPipeline == nullptr) {
        throw std::runtime_error("No next pipeline to forward bytes to.");
    }

    nextPipeline->acceptByte(byte);
}

void Pipeline::reset_pipe()
{
    Pipeline *pipe = this;
    while(pipe != nullptr) {
        pipe->reset();
        pipe = pipe->nextPipeline;
    }
}
