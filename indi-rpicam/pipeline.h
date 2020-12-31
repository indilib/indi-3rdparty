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

    // Add pipeline after this, also takes ownership of the object.
    void daisyChain(Pipeline *p);
    void reset_pipe();

    virtual void acceptByte(uint8_t byte) = 0;
    virtual void reset() = 0;

protected:
    void forward(uint8_t);

private:
    Pipeline *nextPipeline {};
};

#endif // PIPELINE_H
