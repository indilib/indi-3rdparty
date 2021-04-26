/*
 * CircularBuffer.hpp
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

#ifndef _CIRCULARBUFFER_H_INCLUDED_
#define _CIRCULARBUFFER_H_INCLUDED_

#include <cstdint>
#include <cstring>
#include <vector>
#include <iostream>
#include "Config.hpp"

namespace SerialDeviceControl
{

template<typename T, size_t max_size>
class CircularBuffer
{
    public:
        CircularBuffer(T zeroElement) :
            mStart(0),
            mEnd(0),
            mSize(0),
            mZeroElement(zeroElement)
        {
            std::memset(mBuffer, zeroElement, max_size * sizeof(T));
        }

        virtual ~CircularBuffer()
        {

        }

        bool PushFront(T value)
        {
            if(!IsFull())
            {
                Decrement(mStart);
                mBuffer[mStart] = value;
                mSize++;
                return true;
            }

            return false;
        }

        bool PushBack(T value)
        {
            //std::cout << "value " << value << " size " << mSize << " start " << mStart << " end " << mEnd  << std::endl;
            if(!IsFull())
            {
                mBuffer[mEnd] = value;
                Increment(mEnd);
                mSize++;
                return true;
            }

            return false;
        }

        bool PopFront()
        {
            if(!IsEmpty())
            {
                mBuffer[mStart] = mZeroElement;
                Increment(mStart);
                mSize--;
                return true;
            }

            return false;
        }

        bool PopBack()
        {
            if(!IsEmpty())
            {
                Decrement(mEnd);
                mBuffer[mEnd] = mZeroElement;
                mSize--;
                return true;
            }

            return false;
        }

        bool Front(T &returnValue)
        {
            if(!IsEmpty())
            {
                returnValue = mBuffer[mStart];
                return true;
            }

            returnValue = mZeroElement;
            return false;
        }

        bool Back(T &returnValue)
        {
            if(!IsEmpty())
            {
                size_t elementIndex;

                if(mEnd != 0)
                {
                    elementIndex = mEnd - 1;
                }
                else
                {
                    elementIndex = max_size - 1;
                }

                returnValue = mBuffer[elementIndex];
                return true;
            }

            returnValue = mZeroElement;
            return false;
        }

        size_t Size()
        {
            return mSize;
        }

        bool IsEmpty()
        {
            return mSize == 0;
        }

        bool IsFull()
        {
            return mSize == max_size;
        }

        void CopyToVector(std::vector<T> &targetVector)
        {
            for(size_t logicalIndex = 0; logicalIndex < mSize; logicalIndex++)
            {
                size_t actIndex = ActualIndex(logicalIndex);

                targetVector.push_back(mBuffer[actIndex]);
            }
        }

        bool DiscardFront(size_t count)
        {
            bool returnval = false;

            for(size_t i = 0; i < count; i++)
            {
                returnval = PopFront();
            }

            return returnval;
        }

    private:
        size_t mStart;
        size_t mEnd;
        size_t mSize;
        
        T mZeroElement;
        T mBuffer[max_size];

        size_t ActualIndex(size_t logicalIndex)
        {
            if(logicalIndex < (max_size - mStart))
            {
                return mStart + logicalIndex;
            }
            else
            {
                return mStart + (logicalIndex - max_size);
            }
        }

        void Increment(size_t &value)
        {
            if(++value == max_size)
            {
                value = 0;
            }
        }

        void Decrement(size_t &value)
        {
            if(value == 0)
            {
                value = max_size;
            }
            value++;
        }
};
}

#endif
