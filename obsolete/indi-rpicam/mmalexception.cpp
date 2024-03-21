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

#include <iostream>
#include <cstdarg>
#include "mmalexception.h"

MMALException::MMALException(const char *text, ...) : std::runtime_error(msg)
{
    va_list args;
    va_start(args, text);
    vsnprintf(msg, sizeof msg, text, args);
    va_end(args);
}

void  MMALException::throw_if(bool status, const char *text, ...)
{
    if(status) {
        MMALException e;

        va_list args;
        va_start(args, text);
        vsprintf(e.msg, text, args);
        va_end(args);
        
        throw MMALException(e);
    }
}
