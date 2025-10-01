/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "usb_utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <libusb-1.0/libusb.h>
#include <unistd.h>

void printUsage(const char *progName)
{
    printf("Usage: sudo %s <vendorID> <productID> [unbindWait powerSuspend rediscoverWait]\n", progName);
    printf("Example: sudo %s 0x03c3 0x120a\n", progName);
    printf("Note: Root privileges (sudo) are required for USB device reset operations\n");
    printf("Optional delays (microseconds):\n");
    printf("  unbindWait: delay after unbinding (default: 1000000)\n");
    printf("  powerSuspend: time in suspend state (default: 2000000)\n");
    printf("  rediscoverWait: time to wait for rediscovery (default: 5000000)\n");
}

// Function to get product name from VID/PID
std::string getProductName(uint16_t vendorId, uint16_t productId)
{
    libusb_context *ctx = nullptr;
    libusb_device **list = nullptr;
    std::string result;

    int ret = libusb_init(&ctx);
    if (ret < 0)
        return result;

    ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0)
    {
        libusb_exit(ctx);
        return result;
    }

    for (ssize_t i = 0; i < count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;
        ret = libusb_get_device_descriptor(device, &desc);
        if (ret < 0)
            continue;

        if (desc.idVendor == vendorId && desc.idProduct == productId)
        {
            libusb_device_handle *handle;
            ret = libusb_open(device, &handle);
            if (ret < 0)
                continue;

            unsigned char string[256];
            if (desc.iProduct > 0)
            {
                ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
                if (ret > 0)
                {
                    result = (char *)string;
                }
            }
            libusb_close(handle);
            if (!result.empty())
                break;
        }
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return result;
}

int main(int argc, char *argv[])
{
    // Check if running as root
    if (geteuid() != 0)
    {
        printf("Error: This program requires root privileges to reset USB devices.\n");
        printf("Please run with sudo:\n");
        printUsage(argv[0]);
        return 1;
    }

    if (argc != 3 && argc != 6)
    {
        printUsage(argv[0]);
        return 1;
    }

    uint16_t vendorId, productId;
    if (sscanf(argv[1], "0x%hx", &vendorId) != 1)
    {
        printf("Invalid vendor ID format. Use hex format like 0x03c3\n");
        return 1;
    }

    if (sscanf(argv[2], "0x%hx", &productId) != 1)
    {
        printf("Invalid product ID format. Use hex format like 0x120a\n");
        return 1;
    }

    int unbindWait = 1000000;     // Default: 1 second
    int powerSuspend = 2000000;   // Default: 2 seconds
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

    // Get product name from VID/PID
    std::string productName = getProductName(vendorId, productId);
    if (productName.empty())
    {
        printf("No USB device found with VID=0x%04x PID=0x%04x\n", vendorId, productId);
        return 1;
    }

    printf("Found device: VID=0x%04x PID=0x%04x Product='%s'\n", vendorId, productId, productName.c_str());
    printf("Using delays (microseconds):\n");
    printf("  Unbind wait: %d\n", unbindWait);
    printf("  Power suspend: %d\n", powerSuspend);
    printf("  Rediscover wait: %d\n", rediscoverWait);

    printf("Resetting USB device...\n");
    if (USBUtils::resetDevice(vendorId, productName.c_str(), nullptr, unbindWait, powerSuspend, rediscoverWait))
    {
        printf("USB reset successful\n");
        return 0;
    }
    else
    {
        printf("USB reset failed\n");
        return 1;
    }
}
