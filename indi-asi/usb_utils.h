/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <string>
#include <cstdint>

namespace USBUtils
{

// Find USB device path by vendor ID and product name
// Returns empty string if device not found
std::string findDevicePath(uint16_t vendorId, const char *productName, const char *deviceName = nullptr);

// Reset USB device by unbinding and rebinding
// Returns true if successful
// Delays are in microseconds:
// - unbindWaitTime: delay after unbinding (default: 1 second)
// - powerSuspendTime: time to stay in suspend state (default: 2 seconds)
// - rediscoverWaitTime: time to wait for device rediscovery (default: 5 seconds)
bool resetDevice(uint16_t vendorId, const char *productName, const char *deviceName = nullptr,
                 int unbindWaitTime = 1000000, int powerSuspendTime = 2000000, int rediscoverWaitTime = 5000000);

// Internal logging function
void log(const char *deviceName, const char *format, ...);
}
