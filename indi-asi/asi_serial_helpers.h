/*
    ASI Serial Number Helpers

    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com),
                       Wolfgang Reissenberger (sterne-jaeger@openfuture.de)

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
#pragma once

#include <cstddef>
#include <string>

namespace Helpers
{

// Formats a raw byte buffer (e.g. the 8-byte hardware serial/ID structs used by the
// EFW/EAF/CAA/ASI camera SDKs) as an uppercase hex string. Used to build a stable
// per-device identifier for INDI::DefaultDevice's nickname support (setDeviceNicknameFromId/
// saveNicknameId), independent of ASI SDK enumeration order.
inline std::string toHexString(const unsigned char *bytes, size_t length)
{
    static const char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(length * 2);
    for (size_t i = 0; i < length; ++i)
    {
        result += digits[(bytes[i] >> 4) & 0x0F];
        result += digits[bytes[i] & 0x0F];
    }
    return result;
}

}
