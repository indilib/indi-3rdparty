/*
    ScopeLink INDI driver - shared types, errors and wire format helpers

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink/types.h"

#include <cstdio>

namespace scopelink
{
namespace byte_order
{

/**
 * @brief Validates that the requested field lies inside the frame.
 * @param data Frame to read from
 * @param offset Offset of the first byte
 * @param size Number of bytes
 */
static void checkBounds(const Frame &data, size_t offset, size_t size)
{
    if ((size < 1) || (size > 4))
        throw ProtocolError("Unsupported field size " + std::to_string(size) + ", only 1 to 4 bytes are supported.");

    if ((offset + size) > data.size())
    {
        throw ProtocolError("Field at offset " + std::to_string(offset) + " (length " + std::to_string(size)
                            + ") lies outside the " + std::to_string(data.size()) + " byte response frame.");
    }
}

uint32_t toUInt(const Frame &data, size_t offset, size_t size)
{
    checkBounds(data, offset, size);

    uint32_t result = 0;

    for (size_t index = offset; index < (offset + size); index++)
    {
        result <<= 8;
        result |= data[index];
    }

    return result;
}

int32_t toInt(const Frame &data, size_t offset, size_t size)
{
    uint32_t value = toUInt(data, offset, size);

    // Sign extension is done on the unsigned value and the result cast, rather than by shifting a signed
    // seed the way the C# implementation does. The two agree for every input; this one has no
    // implementation defined shifting of negative values in it.
    if (size < 4)
    {
        const uint32_t signBit = 1u << ((size * 8) - 1);

        if ((value & signBit) != 0)
            value |= ~((1u << (size * 8)) - 1u);
    }

    return static_cast<int32_t>(value);
}

uint8_t toByte(const Frame &data, size_t offset)
{
    checkBounds(data, offset, 1);
    return data[offset];
}

bool toBool(const Frame &data, size_t offset)
{
    return toByte(data, offset) != 0;
}

} // namespace byte_order

std::string toHex(const Frame &data, size_t maximumBytes)
{
    std::string text;
    const size_t shown = (data.size() < maximumBytes) ? data.size() : maximumBytes;

    text.reserve((shown * 3) + 8);

    for (size_t index = 0; index < shown; index++)
    {
        char pair[4];

        snprintf(pair, sizeof(pair), "%02X", data[index]);

        if (index > 0)
            text += ' ';

        text += pair;
    }

    if (shown < data.size())
        text += " ...";

    return text;
}

} // namespace scopelink
