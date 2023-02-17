/*
 Copyright (C) 2018-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "libtoupbase.h"
#include <map>

std::string errorCodes(HRESULT rc)
{
    static std::map<HRESULT, std::string> errCodes = {
        {0x00000000, "Success"},
        {0x00000001, "Yet another success"},
        {0x8000ffff, "Catastrophic failure"},
        {0x80004001, "Not supported or not implemented"},
        {0x80070005, "Permission denied"},
        {0x8007000e, "Out of memory"},
        {0x80070057, "One or more arguments are not valid"},
        {0x80004003, "Pointer that is not valid"},
        {0x80004005, "Generic failure"},
        {0x8001010e, "Call function in the wrong thread"},
        {0x8007001f, "Device not functioning"},
        {0x800700aa, "The requested resource is in use"},
        {0x8000000a, "The data necessary to complete this operation is not yet available"},
        {0x8001011f, "This operation returned because the timeout period expired"}
    };
    
    const std::map<HRESULT, std::string>::iterator it = errCodes.find(rc);
    if (it != errCodes.end())
        return it->second;
    else
    {
        char str[256];
        sprintf(str, "Unknown error: 0x%08x", rc);
        return std::string(str);
    }
}