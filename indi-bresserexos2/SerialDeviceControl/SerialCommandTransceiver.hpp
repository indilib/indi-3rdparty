/*
 * SerialCommandTransceiver.hpp
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

#ifndef _SERIALCOMMANDTRANSCEIVER_H_INCLUDED_
#define _SERIALCOMMANDTRANSCEIVER_H_INCLUDED_

#include <cstdint>
#include <iostream>
#include <vector>
#include <deque>
#include <queue>
#include <thread>

#include <algorithm>
#include "Config.hpp"
#include "INotifyPointingCoordinatesReceived.hpp"
#include "ISerialInterface.hpp"
#include "CriticalData.hpp"
#include "SerialCommand.hpp"
#include "CircularBuffer.hpp"

namespace SerialDeviceControl
{
//These types have to inherit/implement:
//-The ISerialInterface.hpp as Interface type.
//-The INotifyPointingCoordinatesReceived.hpp as callback type
template<class InterfaceType, class CallbackType>
class SerialCommandTransceiver
{
    public:
        //Create the serial transceiver according to provided template parameters. Requires references of the data received interface implementation and the serial interface implementation.
        SerialCommandTransceiver(InterfaceType &interfaceImplementation, CallbackType &dataReceivedCallback) :
            mInterfaceImplementation(interfaceImplementation),
            mDataReceivedCallback(dataReceivedCallback),
            mThreadRunning(false),
            mSerialReceiverBuffer(0x00),
            mSerialReaderThread()
        {
            SerialCommand::PushHeader(mMessageHeader);
        }

        //Destroys this transceiver, and stops the thread pulling the serial data from the mount.
        virtual ~SerialCommandTransceiver()
        {
            bool threadRunning = mThreadRunning.Get();

            if(threadRunning)
            {
                Stop();
            }
        }
        //Start the serial command dispatching.
        virtual bool Start()
        {
            mSerialReaderThread = std::thread(&SerialCommandTransceiver::SerialReaderThreadFunction, this);

            return true;
        }

        //Stop the serial command dispatching.
        bool Stop()
        {
            bool running = mThreadRunning.Get();

            if(running)
            {
                mThreadRunning.Set(false);

                mSerialReaderThread.join();
            }

            return true;
        }

    protected:
        //Send a message using the provided serial interface implementation.
        bool SendMessageBuffer(uint8_t* buffer, size_t offset, size_t length)
        {
            return mInterfaceImplementation.Write(buffer, offset, length);
        }

    private:
        //Reference to the serial implementation.
        InterfaceType &mInterfaceImplementation;

        //Reference to the data received interface.
        CallbackType &mDataReceivedCallback;

        //mutex locked running state variable, if set to false the serial receiver thread is terminated.
        CriticalData<bool> mThreadRunning;

        //A cicular buffer implementation to receive serial message from the mount.
        CircularBuffer<uint8_t, 256> mSerialReceiverBuffer;

        //Contains a message header for convinience.
        std::vector<uint8_t> mMessageHeader;

        //movable thread object to control.
        std::thread mSerialReaderThread;

        //buffer used when serial messages are parsed.
        std::vector<uint8_t> mParseBuffer;

        //When messages are received, try parsing them.
        //It may happen that messages are received in fragments, this function tries to piece together these fragments to valid messages.
        //skip any previous junk if message was found, drop anything until the end of the parsed message, to clean up the buffer.
        void TryParseMessagesFromBuffer()
        {
            mParseBuffer.clear();

            if(mSerialReceiverBuffer.Size() > 0)
            {
                mSerialReceiverBuffer.CopyToVector(mParseBuffer);

                std::vector<uint8_t>::iterator startPosition = std::search(mParseBuffer.begin(), mParseBuffer.end(), mMessageHeader.begin(),
                        mMessageHeader.end());

                std::vector<uint8_t>::iterator endPosition = startPosition + MESSAGE_FRAME_SIZE;

                if(startPosition != mParseBuffer.end() && endPosition != mParseBuffer.end())
                {
                    //std::cout << "found sequence!" << std::endl;

                    FloatByteConverter ra_bytes;
                    FloatByteConverter dec_bytes;

                    ra_bytes.bytes[0] = *(startPosition + 5);
                    ra_bytes.bytes[1] = *(startPosition + 6);
                    ra_bytes.bytes[2] = *(startPosition + 7);
                    ra_bytes.bytes[3] = *(startPosition + 8);

                    dec_bytes.bytes[0] = *(startPosition + 9);
                    dec_bytes.bytes[1] = *(startPosition + 10);
                    dec_bytes.bytes[2] = *(startPosition + 11);
                    dec_bytes.bytes[3] = *(startPosition + 12);

                    uint8_t cid = *(startPosition + 4);
                    float ra = ra_bytes.decimal_number;
                    float dec = dec_bytes.decimal_number;

                    //handle specific response.
                    switch(cid)
                    {
                        case SerialCommandID::TELESCOPE_SITE_LOCATION_REPORT_COMMAND_ID:
                            //std::cout << "new location received!" << std::endl;
                            mDataReceivedCallback.OnSiteLocationCoordinatesReceived(ra, dec);
                            break;

                        case SerialCommandID::TELESCOPE_POSITION_REPORT_COMMAND_ID:
                            mDataReceivedCallback.OnPointingCoordinatesReceived(ra, dec);
                            break;

                        default:
                            break;
                    }

                    size_t dropCount = endPosition - mParseBuffer.begin();

                    mSerialReceiverBuffer.DiscardFront(dropCount);

                    //std::cout << "Receive size after :" << mSerialReceiverBuffer.Size() << " dropped " << dropCount << std::endl;
                }
            }
        }
        //Endless loop function of the thread used to receive the serial messages of the mount.
        void SerialReaderThreadFunction()
        {
            std::cerr << "Serial Reader Thread started!" << std::endl;
            bool running = mThreadRunning.Get();

            mInterfaceImplementation.Open();

            if(running == false)
            {
                mThreadRunning.Set(true);

                do
                {
                    //controller sends status messages about every second so wait a bit
                    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(500));

                    size_t bufferContent = mInterfaceImplementation.BytesToRead();
                    int16_t data = -1;

                    bool addSucceed = false;

                    if(bufferContent > 0)
                    {
                        while((data = mInterfaceImplementation.ReadByte()) > -1)
                        {
                            addSucceed = mSerialReceiverBuffer.PushBack((uint8_t)data);
                        }

                        if(addSucceed)
                        {
                            TryParseMessagesFromBuffer();
                        }
                    }
                    running = mThreadRunning.Get();
                }
                while(running == true);
            }
            std::cerr << "Serial Reader Thread stopped!" << std::endl;
            mInterfaceImplementation.Flush();
            mInterfaceImplementation.Close();
        }
};
}
#endif
