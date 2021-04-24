/*
 * IndiSerialWrapper.hpp
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

#ifndef _INDISERIALWRAPPER_H_INCLUDED_
#define _INDISERIALWRAPPER_H_INCLUDED_

#include <cstdint>
#include <cmath>
#include <memory>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <mutex>

#include <libindi/indicom.h>
#include <libindi/inditelescope.h>
#include <libindi/indilogger.h>

#include "SerialDeviceControl/ISerialInterface.hpp"
#include "Config.hpp"

namespace GoToDriver
{
//Implementation of the serial interface abstraction dedicated for the indi interface.
class IndiSerialWrapper : public SerialDeviceControl::ISerialInterface
{
    private:
        //Handle of the serial port provided by the indi interface.
        int mTtyFd;

        static constexpr const uint8_t DRIVER_TIMEOUT {3};

        //mutex to protect writing queue, from being concurrently modified, causing corrupt messages.
        std::mutex mMutex;

    public:
        //Default implementation constructor.
        IndiSerialWrapper();

        //Default Destructor.
        virtual ~IndiSerialWrapper();

        //Get the current device handle.
        int GetFD();

        //Set the device handle.
        void SetFD(int fd);

        //Opens the serial device, the acutal implementation has to deal with the handles!
        virtual bool Open();

        //Closes the serial device, the actual implementation has to deal with the handles!
        virtual bool Close();

        //Returns true if the serial port is open and ready to receive or transmit data.
        virtual bool IsOpen();

        //Returns the number of bytes to read available in the serial receiver queue.
        virtual size_t BytesToRead();

        //Reads a byte from the serial device. Can safely cast to uint8_t unless -1 is returned, corresponding to "stream end reached".
        virtual int16_t ReadByte();

        //writes the buffer to the serial interface.
        //this function should handle all the quirks of various serial interfaces.
        virtual bool Write(uint8_t* buffer, size_t offset, size_t length);

        //flush the buffer.
        virtual bool Flush();
};
}

#endif
