/*
 * CriticalData.hpp
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

#ifndef _CRITICALDATA_H_INCLUDED_
#define _CRITICALDATA_H_INCLUDED_

#include <cstdint>
#include <mutex>
#include "Config.hpp"

namespace SerialDeviceControl
{
//simple mutex container, protecting its content from concurrently access and its side effects.
//intended for simple data types.
template<typename T>
class CriticalData
{
    public:
        //default constructure, leaves the contained object uninitialized.
        CriticalData()
        {

        }

        //constructor setting the contained object to the initial value.
        CriticalData(T initialValue) :
            mData(initialValue)
        {

        }

        virtual ~CriticalData()
        {

        }

        //return the value of the contained data object.
        T Get()
        {
            T val;

            {
                std::lock_guard<std::mutex> guard(mMutex);
                val = mData;
            }

            return val;

        }

        //set the value of the contained data object.
        void Set(T value)
        {
            {
                std::lock_guard<std::mutex> guard(mMutex);
                mData = value;
            }
        }

    private:
        //instance of the data type.
        T mData;
        //mutex protecting the data object.
        std::mutex mMutex;
};
}

#endif
