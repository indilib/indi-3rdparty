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

#ifndef PIPELINE_H
#define PIPELINE_H

#include <cstdint>

class Pipeline
{
public:
    Pipeline();
    virtual ~Pipeline();

    /**
     * Add pipeline after this.
     *
     * This also takes ownership of the object and will call its delete.
     */
    void daisyChain(Pipeline *p);

    /**
     * Cascading reset of whole pipeline.
     */
    void reset_pipe();

    virtual void data_received(uint8_t  *data,  uint32_t length) = 0;

    /**
     * Reset this pipeline object to its initial state. 
     * 
     * Nothing about the image to be received can be changed after calling this and 
     * until all buffers has been received.
     */
    virtual void reset() = 0;

protected:
    void forward(uint8_t *data,  uint32_t length);

private:
    Pipeline *nextPipeline {};
};

#endif // PIPELINE_H
