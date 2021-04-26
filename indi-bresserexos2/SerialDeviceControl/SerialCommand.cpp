/*
 * SerialCommand.cpp
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

#include "SerialCommand.hpp"

using SerialDeviceControl::SerialCommand;

//TODO: change logging,
#ifdef USE_CERR_LOGGING
#include <iostream>
#endif

#define ERROR_NULL_BUFFER ("error: buffer is null pointer.")
#define ERROR_INVALID_RA_RANGE ("error: invalid range for right ascension.")
#define ERROR_INVALID_DEC_RANGE ("error: invalid range for declination.")
#define ERROR_INVALID_LAT_RANGE ("error: invalid range for latitude.")
#define ERROR_INVALID_LON_RANGE ("error: invalid range for longitude.")
#define ERROR_INVALID_YEAR_RANGE ("error: invalid range for year.")
#define ERROR_INVALID_MONTH_RANGE ("error: invalid range for month.")
#define ERROR_INVALID_DAY_RANGE ("error: invalid range for day.")
#define ERROR_INVALID_HOUR_RANGE ("error: invalid range for hour.")
#define ERROR_INVALID_MINUTE_RANGE ("error: invalid range for minute.")
#define ERROR_INVALID_SECOND_RANGE ("error: invalid range for second.")
#define ERROR_INVALID_RANGE_FEBURARY ("error: the february can only have 29 days at maximum!")
#define ERROR_INVALID_RANGE_THIRTYONE ("error: the provided month can only have 31 days!")
#define ERROR_INVALID_RANGE_THIRTY ("error: the provided month can only have 30 days!")
#define ERROR_INVALID_RANGE_NO_LEAP_YEAR ("error: the provided date is invalid, februry can only have 29 days in leap years!")
#define ERROR_INVALID_DIRECTION ("error: the direction provided is invalid!")


uint8_t SerialCommand::MessageHeader[4] = {0x55, 0xaa, 0x01, 0x09};

void SerialCommand::PushHeader(std::vector<uint8_t> &buffer)
{
    buffer.push_back(SerialCommand::MessageHeader[0]);
    buffer.push_back(SerialCommand::MessageHeader[1]);
    buffer.push_back(SerialCommand::MessageHeader[2]);
    buffer.push_back(SerialCommand::MessageHeader[3]);
}

void SerialCommand::push_bytes(std::vector<uint8_t> &buffer, uint8_t byte, size_t count)
{
    if(count < 1)
    {
        return;
    }

    for(size_t i = 0; i < count; i++)
    {
        buffer.push_back(byte);
    }
}

void SerialCommand::push_float_bytes(std::vector<uint8_t> &buffer, FloatByteConverter &values)
{
    buffer.push_back(values.bytes[0]);
    buffer.push_back(values.bytes[1]);
    buffer.push_back(values.bytes[2]);
    buffer.push_back(values.bytes[3]);
}

//The following two commands are the simplest. They only consist of the header and the command id, padding the remaining bytes with zeros.
//Gracefully disconnect from the GoTo Controller.
bool SerialCommand::GetDisconnectCommandMessage(std::vector<uint8_t> &buffer)
{
    PushHeader(buffer);

    buffer.push_back(SerialCommandID::DISCONNET_COMMAND_ID);

    push_bytes(buffer, 0x00, 8);

    return true;
}

//The following two commands are the simplest. They only consist of the header and the command id, padding the remaining bytes with zeros.
//This command stops the telescope if, it is moving, or tracking.
bool SerialCommand::GetStopMotionCommandMessage(std::vector<uint8_t> &buffer)
{
    PushHeader(buffer);

    buffer.push_back(SerialCommandID::STOP_MOTION_COMMAND_ID);

    push_bytes(buffer, 0x00, 8);

    return true;
}

//This slews the telescope back to the initial position or home position.
bool SerialCommand::GetParkCommandMessage(std::vector<uint8_t> &buffer)
{
    PushHeader(buffer);

    buffer.push_back(SerialCommandID::PARK_COMMAND_ID);

    push_bytes(buffer, 0x00, 8);

    return true;
}

bool SerialCommand::GetGetSiteLocationCommandMessage(std::vector<uint8_t> &buffer)
{
    PushHeader(buffer);

    buffer.push_back(SerialCommandID::GET_SITE_LOCATION_COMMAND_ID);

    push_bytes(buffer, 0x00, 8);

    return true;
}

//This command slews the telescope to the coordinates provided. It is autonomous, and the change of slewing speeds are not allowed.
bool SerialCommand::GetGotoCommandMessage(std::vector<uint8_t> &buffer, float decimal_right_ascension,
        float decimal_declination)
{
    if(decimal_right_ascension < 0 || decimal_right_ascension > 24)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_RA_RANGE << std::endl;
#endif
        return false;
    }

    if(decimal_declination < -90 || decimal_declination > 90)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_DEC_RANGE << std::endl;
#endif
        return false;
    }

    PushHeader(buffer);

    buffer.push_back(SerialCommandID::GOTO_COMMAND_ID);

    FloatByteConverter ra_bytes;
    FloatByteConverter dec_bytes;

    ra_bytes.decimal_number = decimal_right_ascension;
    dec_bytes.decimal_number = decimal_declination;

    push_float_bytes(buffer, ra_bytes);
    push_float_bytes(buffer, dec_bytes);

    return true;
}

//This command syncs the telescope to the coordinates provided. It should be useful when doing plate solvings.
bool SerialCommand::GetSyncCommandMessage(std::vector<uint8_t> &buffer, float decimal_right_ascension,
        float decimal_declination)
{
    if(decimal_right_ascension < 0 || decimal_right_ascension > 24)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_RA_RANGE << std::endl;
#endif
        return false;
    }

    if(decimal_declination < -90 || decimal_declination > 90)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_DEC_RANGE << std::endl;
#endif
        return false;
    }

    PushHeader(buffer);

    buffer.push_back(SerialCommandID::SYNC_COMMAND_ID);

    FloatByteConverter ra_bytes;
    FloatByteConverter dec_bytes;

    ra_bytes.decimal_number = decimal_right_ascension;
    dec_bytes.decimal_number = decimal_declination;

    push_float_bytes(buffer, ra_bytes);
    push_float_bytes(buffer, dec_bytes);

    return true;
}

//This sets the site location of the mount, it just supports longitude and latitude, but no elevation.
bool SerialCommand::GetSetSiteLocationCommandMessage(std::vector<uint8_t> &buffer, float decimal_latitude,
        float decimal_longitude)
{
    if(decimal_latitude < -180 || decimal_latitude > 180)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_LAT_RANGE << std::endl;
#endif
        return false;
    }

    if(decimal_longitude < -90 || decimal_longitude > 90)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_LON_RANGE << std::endl;
#endif
        return false;
    }

    PushHeader(buffer);

    buffer.push_back(SerialCommandID::SET_SITE_LOCATION_COMMAND_ID);

    FloatByteConverter lat_bytes;
    FloatByteConverter lon_bytes;

    lat_bytes.decimal_number = decimal_latitude;
    lon_bytes.decimal_number = decimal_longitude;

    push_float_bytes(buffer, lon_bytes);
    push_float_bytes(buffer, lat_bytes);

    return true;
}

//This message sets the dates and time of the telescope mount, the values are simple binary coded decimals (BCD).
//Also the controller accepts any value, even if it is incorrect eg. 99:99:99 as a time is possible, so checking for validity is encouraged.
//further a check for leap years is advisable.
bool SerialCommand::GetSetDateTimeCommandMessage(std::vector<uint8_t> &buffer, uint16_t year, uint8_t month, uint8_t day,
        uint8_t hour, uint8_t minute, uint8_t second)
{
    if(year > 9999)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_YEAR_RANGE << std::endl;
#endif
        return false;
    }

    if(month < 1 || month > 12)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_MONTH_RANGE << " " << month << std::endl;
#endif
        return false;
    }

    if(day < 1 || day > 31)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_DAY_RANGE << std::endl;
#endif
        return false;
    }

    if(hour > 24)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_HOUR_RANGE << std::endl;
#endif
        return false;
    }

    if(minute > 59)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_MINUTE_RANGE << std::endl;
#endif
        return false;
    }

    if(second > 59)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_SECOND_RANGE << std::endl;
#endif
        return false;
    }

    //check if the february is in its bounds
    if(month == DateMonths::February && day > 29)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_RANGE_FEBURARY << std::endl;
#endif
        return false;
    }

    switch(month)
    {
        case DateMonths::January:
        case DateMonths::March:
        case DateMonths::May:
        case DateMonths::July:
        case DateMonths::August:
        case DateMonths::October:
        case DateMonths::December:
        {
            if(day > 31)
            {
#ifdef USE_CERR_LOGGING
                std::cerr << ERROR_INVALID_RANGE_THIRTYONE << std::endl;
#endif
                return false;
            }
        }
        break;

        case DateMonths::April:
        case DateMonths::June:
        case DateMonths::September:
        case DateMonths::November:
            if(day > 30)
            {
#ifdef USE_CERR_LOGGING
                std::cerr << ERROR_INVALID_RANGE_THIRTY << std::endl;
#endif
                return false;
            }
            break;
    }

    if(year > 0 && (year % 4) != 0)
    {
        //common year.
        if(day > 28)
        {
#ifdef USE_CERR_LOGGING
            std::cerr << ERROR_INVALID_RANGE_NO_LEAP_YEAR << std::endl;
#endif
            return false;
        }
    }
    else if(year > 0 && (year % 100) == 0)
    {
        //leap year.
    }
    else if(year > 0 && (year % 400) != 0)
    {
        //common year.
        if(day > 28)
        {
#ifdef USE_CERR_LOGGING
            std::cerr << ERROR_INVALID_RANGE_NO_LEAP_YEAR << std::endl;
#endif
            return false;
        }
    }
    else
    {
        //leap year.
    }

    PushHeader(buffer);

    buffer.push_back(SerialCommandID::SET_DATE_TIME_COMMAND_ID);

    uint8_t hiYear = (uint8_t)(year / 100);
    uint8_t loYear = (uint8_t)(year % 100);

    buffer.push_back(hiYear);
    buffer.push_back(loYear);
    buffer.push_back(month);
    buffer.push_back(day);

    buffer.push_back(hour);
    buffer.push_back(minute);
    buffer.push_back(second);
    buffer.push_back(0x00); // a whole unused byte ... what a waste...

    return true;
}

//move the telescope in a certain direction. Use the first 4 command IDs for a particular direction.
bool SerialCommand::GetMoveWhileTrackingCommandMessage(std::vector<uint8_t> &buffer, SerialCommandID direction)
{
    if(direction < SerialCommandID::MOVE_EAST_COMMAND_ID || direction > SerialCommandID::MOVE_SOUTH_COMMAND_ID)
    {
#ifdef USE_CERR_LOGGING
        std::cerr << ERROR_INVALID_DIRECTION << std::endl;
#endif
        return false;
    }

    PushHeader(buffer);
    buffer.push_back(direction);

    buffer.push_back(0xC8);
    push_bytes(buffer, 0x00, 3);

    buffer.push_back(0xC8);
    push_bytes(buffer, 0x00, 3);

    return true;
}
