/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "usb_utils.h"
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cstdarg>
#include <indilogger.h>

namespace USBUtils
{

void log(const char *deviceName, const char *format, ...)
{
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (deviceName)
        DEBUGFDEVICE(deviceName, INDI::Logger::DBG_DEBUG, "%s", message);
    else
        printf("%s\n", message);
}

std::string findDevicePath(uint16_t vendorId, const char *productName, const char *deviceName)
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

        if (desc.idVendor == vendorId)
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
                    // Check if libusb product string is included in the supplied product name
                    // This handles cases where supplied name might have manufacturer prefix
                    if (strstr(productName, (char *)string) != nullptr)
                    {
                        uint8_t bus = libusb_get_bus_number(device);
                        uint8_t ports[7];
                        int port_count = libusb_get_port_numbers(device, ports, sizeof(ports));

                        if (port_count > 0)
                        {
                            char path[128];
                            if (port_count == 1)
                            {
                                snprintf(path, sizeof(path), "%d-%d", bus, ports[0]);
                            }
                            else
                            {
                                snprintf(path, sizeof(path), "%d-%d", bus, ports[0]);
                                for (int p = 1; p < port_count; p++)
                                {
                                    char temp[8];
                                    snprintf(temp, sizeof(temp), ".%d", ports[p]);
                                    strcat(path, temp);
                                }
                            }
                            result = path;
                            log(deviceName, "Found device at path: %s", path);
                        }
                    }
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

bool resetDevice(uint16_t vendorId, const char *productName, const char *deviceName,
                 int unbindWaitTime, int powerSuspendTime, int rediscoverWaitTime)
{
    std::string devicePath = findDevicePath(vendorId, productName, deviceName);
    if (devicePath.empty())
    {
        log(deviceName, "Failed to find device path for VID=0x%04x Product='%s'", vendorId, productName);
        return false;
    }

    // Check bind/unbind permissions
    char unbind_path[512] = "/sys/bus/usb/drivers/usb/unbind";
    char bind_path[512] = "/sys/bus/usb/drivers/usb/bind";

    log(deviceName, "Checking USB reset permissions:");
    if (access(unbind_path, W_OK) == 0)
        log(deviceName, "Unbind path (%s): Writable", unbind_path);
    else
        log(deviceName, "Unbind path (%s): Not writable (error: %s)", unbind_path, strerror(errno));

    if (access(bind_path, W_OK) == 0)
        log(deviceName, "Bind path (%s): Writable", bind_path);
    else
        log(deviceName, "Bind path (%s): Not writable (error: %s)", bind_path, strerror(errno));

    // First try to unbind the device
    FILE *unbind_fp = fopen(unbind_path, "w");
    if (!unbind_fp)
    {
        log(deviceName, "Failed to open unbind file: %s", strerror(errno));
        return false;
    }

    log(deviceName, "Unbinding device %s (wait: %d us)", devicePath.c_str(), unbindWaitTime);
    fprintf(unbind_fp, "%s\n", devicePath.c_str());
    fclose(unbind_fp);
    usleep(unbindWaitTime);

    // Try to reset the parent hub port
    char parent_path[512];
    snprintf(parent_path, sizeof(parent_path), "/sys/bus/usb/devices/%s/..", devicePath.c_str());
    char real_parent[512];
    if (realpath(parent_path, real_parent))
    {
        // Try port power control
        char port_power[512];
        snprintf(port_power, sizeof(port_power), "%s/power/level", real_parent);
        if (access(port_power, W_OK) == 0)
        {
            log(deviceName, "Cycling port power on %s (suspend: %d us)", real_parent, powerSuspendTime);
            FILE *power_fp = fopen(port_power, "w");
            if (power_fp)
            {
                fprintf(power_fp, "suspend\n");
                fclose(power_fp);
                usleep(powerSuspendTime);

                power_fp = fopen(port_power, "w");
                if (power_fp)
                {
                    fprintf(power_fp, "on\n");
                    fclose(power_fp);
                }
            }
        }
        else
        {
            log(deviceName, "No write access to power control: %s", port_power);
        }
    }

    // Now rebind the device
    snprintf(bind_path, sizeof(bind_path), "/sys/bus/usb/drivers/usb/bind");
    FILE *bind_fp = fopen(bind_path, "w");
    if (!bind_fp)
    {
        log(deviceName, "Failed to open bind file: %s", strerror(errno));
        return false;
    }

    log(deviceName, "Rebinding device %s", devicePath.c_str());
    fprintf(bind_fp, "%s\n", devicePath.c_str());
    fclose(bind_fp);

    // Wait for device to be rediscovered
    log(deviceName, "Waiting for device to be rediscovered (wait: %d us)...", rediscoverWaitTime);
    usleep(rediscoverWaitTime);

    return true;
}

}
