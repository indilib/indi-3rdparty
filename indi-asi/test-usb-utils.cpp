/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "usb_utils.h"
#include <stdio.h>
#include <string.h>

void printUsage(const char *progName)
{
    printf("Usage: %s <vendorID> <productName> [unbindWait powerSuspend rediscoverWait]\n", progName);
    printf("Example: %s 0x03c3 \"ZWO ASI120MC-S\"\n", progName);
    printf("Optional delays (microseconds):\n");
    printf("  unbindWait: delay after unbinding (default: 1000000)\n");
    printf("  powerSuspend: time in suspend state (default: 2000000)\n");
    printf("  rediscoverWait: time to wait for rediscovery (default: 5000000)\n");
}

void testDevice(uint16_t vendorId, const char *productName, int unbindWait, int powerSuspend, int rediscoverWait)
{
    printf("\nTesting device: VID=0x%04x Product='%s'\n", vendorId, productName);
    printf("Using delays (microseconds):\n");
    printf("  Unbind wait: %d\n", unbindWait);
    printf("  Power suspend: %d\n", powerSuspend);
    printf("  Rediscover wait: %d\n", rediscoverWait);

    std::string path = USBUtils::findDevicePath(vendorId, productName, nullptr);
    if (!path.empty())
    {
        printf("Found device path: %s\n", path.c_str());

        printf("Testing USB reset...\n");
        if (USBUtils::resetDevice(vendorId, productName, nullptr, unbindWait, powerSuspend, rediscoverWait))
        {
            printf("USB reset successful\n");
        }
        else
        {
            printf("USB reset failed\n");
        }
    }
    else
    {
        printf("Device not found\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 6)
    {
        printUsage(argv[0]);
        return 1;
    }

    uint16_t vendorId;
    if (sscanf(argv[1], "0x%hx", &vendorId) != 1)
    {
        printf("Invalid vendor ID format. Use hex format like 0x03c3\n");
        return 1;
    }

    int unbindWait = 1000000;    // Default: 1 second
    int powerSuspend = 2000000;  // Default: 2 seconds
    int rediscoverWait = 5000000; // Default: 5 seconds

    if (argc == 6)
    {
        unbindWait = atoi(argv[3]);
        powerSuspend = atoi(argv[4]);
        rediscoverWait = atoi(argv[5]);

        if (unbindWait <= 0 || powerSuspend <= 0 || rediscoverWait <= 0)
        {
            printf("Invalid delay values. All delays must be positive.\n");
            return 1;
        }
    }

    testDevice(vendorId, argv[2], unbindWait, powerSuspend, rediscoverWait);
    return 0;
}
