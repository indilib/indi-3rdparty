/*
 * INotifyPointingCoordinatesReceived.hpp
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

#ifndef _INOTIFYPOINTINGCOORDINATESRECEIVED_H_INCLUDED_
#define _INOTIFYPOINTINGCOORDINATESRECEIVED_H_INCLUDED_

#include <cstdint>
#include <vector>
#include "Config.hpp"

namespace SerialDeviceControl
{
//Simple interface for coordinate report receiving.
class INotifyPointingCoordinatesReceived
{
    public:
        //Called each time a pair of coordinates was received from the serial interface.
        virtual void OnPointingCoordinatesReceived(float right_ascension, float declination) = 0;

        //Called each time a pair of geo coordinates was received from the serial inferface.
        //This occurs only by active request (GET_SITE_LOCATION_COMMAND_ID)
        virtual void OnSiteLocationCoordinatesReceived(float latitude, float longitude) = 0;
};
}
#endif
