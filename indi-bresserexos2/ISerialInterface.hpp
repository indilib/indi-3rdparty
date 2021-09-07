/*
 * ISerialInterface.hpp
 *
 * Copyright 2020 Kevin Krüger <kkevin@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#ifndef _ISERIALINTERFACE_H_INCLUDED_
#define _ISERIALINTERFACE_H_INCLUDED_

#include <cstdint>
#include "config.h"

namespace SerialDeviceControl
{
//Abstraction of the serial interface to allow reuse of code for several implementations.
class ISerialInterface
{
    public:
        //Opens the serial device, the acutal implementation has to deal with the handles!
        virtual bool Open() = 0;

        //Closes the serial device, the actual implementation has to deal with the handles!
        virtual bool Close() = 0;

        //Returns true if the serial port is open and ready to receive or transmit data.
        virtual bool IsOpen() = 0;

        //Returns the number of bytes to read available in the serial receiver queue.
        virtual size_t BytesToRead() = 0;

        //Reads a byte from the serial device. Can safely cast to uint8_t unless -1 is returned, corresponding to "stream end reached".
        virtual int16_t ReadByte() = 0;

        //writes the buffer to the serial interface.
        //this function should handle all the quirks of various serial interfaces.
        virtual bool Write(uint8_t* buffer, size_t offset, size_t length) = 0;

        //flush the buffer.
        virtual bool Flush() = 0;
};
}
#endif
