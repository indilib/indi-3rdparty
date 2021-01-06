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

#ifndef _JPEGPIPELINE_H
#define _JPEGPIPELINE_H

#include "pipeline.h"

/**
 * @brief The RawStreamReceiver class
 * Repsonsible for receiving a raw image from the MMAL subsystem. In this mode
 * the image consist of a normal JPEG-image, followed by a 32K broadcom header and then
 * the true raw data. This class accepts one byte at a time and spools past the JPEG header,
 * picks up the row pitch and then spools past the @BRCMo data.
 */
class JpegPipeline : public Pipeline
{
public:
    class Exception : public std::runtime_error
    {
    public: Exception(const char *text) : std::runtime_error(text) {}
    };

    enum class State {
        WANT_FF,
        WANT_TYPE,
        WANT_S1,
        WANT_S2,
        SKIP_BYTES,
        WANT_ENTROPY_DATA,
        ENTROPY_GOT_FF,
        END_OF_JPEG,
        INVALID} state {State::WANT_FF};

    JpegPipeline() {}

    virtual void data_received(uint8_t *data,  uint32_t length) override;
    virtual void reset() override;

    State getState() { return state; }

private:
    int s1 {0}; // Length field, first byte MSB.
    int s2 {0}; // Length field, second byte LSB.
    uint32_t skip_bytes {0}; //Counter for skipping bytes.
    bool entropy_data_follows {false};
    int current_type {}; // For debugging only.
};

#endif // _JPEGPIPELINE_H
