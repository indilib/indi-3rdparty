/*
 * TestDataReceivedCallback.hpp
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

#ifndef _TESTDATARECEIVEDCALLBACK_H_INCLUDED_
#define _TESTDATARECEIVEDCALLBACK_H_INCLUDED_

#include <cstdint>
#include <vector>
#include <iostream>

#include "Config.hpp"

namespace Testing
{
//Simple interface for coordinate report receiving.
class TestDataReceivedCallback
{
    public:
        TestDataReceivedCallback();

        virtual ~TestDataReceivedCallback();
        //Called each time a pair of coordinates was received from the serial interface.
        virtual void OnPointingCoordinatesReceived(float right_ascension, float declination);
};
}
#endif
