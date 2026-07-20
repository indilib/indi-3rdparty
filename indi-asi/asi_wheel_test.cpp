/*
 ASI Filter Wheel Test

 Based on ASI Camera Test

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

#include <EFW_filter.h>
#include "stdio.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <libusb-1.0/libusb.h>
#include <vector>
#include <string>

#define EFW_IS_MOVING -1

struct ASIUSBDevice
{
    uint16_t product_id;
    std::string manufacturer;
    std::string product;
    std::string serial;
    int bus_number;
    int port_number;
    uint16_t usb_version;
    uint8_t device_class;
    int num_interfaces;
};

std::vector<ASIUSBDevice> probe_usb_system(bool verbose = true);

void reset_usb_device(uint16_t vendor_id, uint16_t product_id)
{
    char cmd[512];
    char path[256] = {0};
    printf("Finding USB port for device %04x:%04x...\n", vendor_id, product_id);

    // Find the device's USB port path
    snprintf(cmd, sizeof(cmd),
             "for dev in /sys/bus/usb/devices/*; do "
             "  if [ -f \"$dev/idVendor\" ] && [ -f \"$dev/idProduct\" ]; then "
             "    if [ \"$(cat $dev/idVendor)\" = \"%04x\" ] && [ \"$(cat $dev/idProduct)\" = \"%04x\" ]; then "
             "      echo \"$dev\"; "
             "      exit 0; "
             "    fi; "
             "  fi; "
             "done",
             vendor_id, product_id);

    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        fprintf(stderr, "Failed to execute device search\n");
        return;
    }

    if (fgets(path, sizeof(path), fp) == NULL)
    {
        fprintf(stderr, "Failed to find device path\n");
        pclose(fp);
        return;
    }
    pclose(fp);

    // Remove newline if present
    char *newline = strchr(path, '\n');
    if (newline)
        *newline = '\0';

    printf("Found device at: %s\n", path);

    // First try to unbind the device
    printf("Unbinding USB device...\n");
    char unbind_path[512];
    snprintf(unbind_path, sizeof(unbind_path), "%s/driver/unbind", path);

    // Get the device name (last part of path)
    char *device_name = strrchr(path, '/');
    if (device_name)
        device_name++; // Skip the '/'
    else
        device_name = path;

    FILE *unbind_fp = fopen(unbind_path, "w");
    if (!unbind_fp)
    {
        fprintf(stderr, "Failed to open unbind path: %s\n", strerror(errno));
        return;
    }
    fprintf(unbind_fp, "%s\n", device_name);
    fclose(unbind_fp);
    printf("Device unbound\n");
    usleep(1000000); // 1 second

    // Try to reset the parent hub port
    char parent_path[512];
    snprintf(parent_path, sizeof(parent_path), "%s/..", path);
    char real_parent[512];
    if (realpath(parent_path, real_parent))
    {
        printf("Found parent hub: %s\n", real_parent);

        // Try port power control
        char port_power[512];
        snprintf(port_power, sizeof(port_power), "%s/power/level", real_parent);
        printf("Attempting to access power control at: %s\n", port_power);
        if (access(port_power, W_OK) == 0)
        {
            printf("Cycling parent hub port power...\n");
            FILE *power_fp = fopen(port_power, "w");
            if (!power_fp)
            {
                fprintf(stderr, "Failed to open power control: %s\n", strerror(errno));
            }
            else
            {
                fprintf(power_fp, "suspend\n");
                fclose(power_fp);
                usleep(2000000); // 2 seconds

                power_fp = fopen(port_power, "w");
                if (!power_fp)
                {
                    fprintf(stderr, "Failed to reopen power control: %s\n", strerror(errno));
                }
                else
                {
                    fprintf(power_fp, "on\n");
                    fclose(power_fp);
                }
            }
        }
        else
        {
            fprintf(stderr, "No write access to power control\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to resolve parent hub path: %s\n", strerror(errno));
    }

    // Now rebind the device
    printf("Rebinding USB device...\n");
    char bind_path[512];
    snprintf(bind_path, sizeof(bind_path), "%s/../bind", unbind_path);
    FILE *bind_fp = fopen(bind_path, "w");
    if (!bind_fp)
    {
        // If direct bind fails, try the generic USB driver path
        snprintf(bind_path, sizeof(bind_path), "/sys/bus/usb/drivers/usb/bind");
        bind_fp = fopen(bind_path, "w");
        if (!bind_fp)
        {
            fprintf(stderr, "Failed to open bind path: %s\n", strerror(errno));
            // Continue anyway as the device might rebind automatically
        }
    }

    if (bind_fp)
    {
        fprintf(bind_fp, "%s\n", device_name);
        fclose(bind_fp);
        printf("Device rebound\n");
    }

    // Wait for device to be rediscovered
    printf("Waiting for device to be rediscovered...\n");
    usleep(5000000); // 5 seconds

    printf("USB port power cycle complete\n");
}

std::vector<ASIUSBDevice> probe_usb_system(bool verbose)
{
    std::vector<ASIUSBDevice> devices;
    if (verbose)
        printf("\n=== USB Subsystem Diagnostics ===\n");

    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    ssize_t count;
    int ret = libusb_init(&ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to initialize libusb: %s\n", libusb_error_name(ret));
        return std::vector<ASIUSBDevice>();
    }

    count = libusb_get_device_list(ctx, &list);
    if (count < 0)
    {
        fprintf(stderr, "Failed to get device list: %s\n", libusb_error_name(count));
        libusb_exit(ctx);
        return std::vector<ASIUSBDevice>();
    }

    if (verbose)
        printf("\nScanning USB devices:\n");
    for (ssize_t i = 0; i < count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;
        ret = libusb_get_device_descriptor(device, &desc);
        if (ret < 0)
        {
            fprintf(stderr, "Failed to get device descriptor: %s\n", libusb_error_name(ret));
            continue;
        }

        // Check if this is a ZWO device
        if (desc.idVendor == 0x03c3)
        {
            ASIUSBDevice asi_device;
            asi_device.product_id = desc.idProduct;
            asi_device.bus_number = libusb_get_bus_number(device);
            asi_device.port_number = libusb_get_port_number(device);
            asi_device.usb_version = desc.bcdUSB;
            asi_device.device_class = desc.bDeviceClass;

            libusb_device_handle *handle;
            ret = libusb_open(device, &handle);
            if (ret >= 0)
            {
                unsigned char string[256];
                if (desc.iManufacturer > 0)
                {
                    ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
                    if (ret > 0)
                    {
                        asi_device.manufacturer = std::string((char*)string);
                        if (verbose)
                            printf("Manufacturer: %s\n", asi_device.manufacturer.c_str());
                    }
                }

                if (desc.iProduct > 0)
                {
                    ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
                    if (ret > 0)
                    {
                        asi_device.product = std::string((char*)string);
                        if (verbose)
                            printf("Product: %s\n", asi_device.product.c_str());
                    }
                }

                if (desc.iSerialNumber > 0)
                {
                    ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string, sizeof(string));
                    if (ret > 0)
                    {
                        asi_device.serial = std::string((char*)string);
                        if (verbose)
                            printf("Serial Number: %s\n", asi_device.serial.c_str());
                    }
                }

                struct libusb_config_descriptor *config;
                ret = libusb_get_active_config_descriptor(device, &config);
                if (ret == 0)
                {
                    asi_device.num_interfaces = config->bNumInterfaces;
                    if (verbose)
                        printf("Number of interfaces: %d\n", asi_device.num_interfaces);
                    libusb_free_config_descriptor(config);
                }

                libusb_close(handle);
            }

            if (verbose)
            {
                printf("Bus: %d, Port: %d\n", asi_device.bus_number, asi_device.port_number);
                printf("VID:PID: %04x:%04x\n", desc.idVendor, asi_device.product_id);
                printf("USB Version: %04x\n", asi_device.usb_version);
                printf("Device Class: %d\n", asi_device.device_class);
            }

            devices.push_back(asi_device);
        }
    }

    if (verbose)
        printf("\n================================\n");
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return devices;
}

void print_usage()
{
    printf("Usage: asi_wheel_test [options]\n");
    printf("Options:\n");
    printf("  -p                 Probe USB system and exit\n");
    printf("  -r                 Probe and reset USB device and exit\n");
    printf("  -c <wheel_id>      Filter wheel ID to use (default: 0)\n");
    printf("  -l                 List all connected filter wheels with properties\n");
    printf("  -g                 Get current filter wheel position\n");
    printf("  -s <slot>          Set filter wheel to target slot (1-based)\n");
    printf("  -d <0|1>           Set unidirectional mode (0=disable, 1=enable)\n");
    printf("  -C                 Calibrate the filter wheel\n");
    printf("  -?                 Show this help message\n");
}

int main(int argc, char *argv[])
{
    int wheel_id = 0;
    int list_only = 0;
    int get_pos = 0;
    int slot = -1;
    int set_direction = -1;
    int calibrate = 0;

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "prc:lgs:d:C?")) != -1)
    {
        switch (opt)
        {
            case 'p':
                probe_usb_system(true);
                return 0;
            case 'r':
            {
                printf("Probing USB system to find ZWO device...\n");
                auto devices = probe_usb_system(true);
                if (devices.empty())
                {
                    fprintf(stderr, "No ZWO device found in USB system\n");
                    return -1;
                }

                // Use the first found device
                printf("Found ZWO device with product ID: 0x%04x\n", devices[0].product_id);
                reset_usb_device(0x03c3, devices[0].product_id);
                return 0;
            }
            case 'c':
                wheel_id = atoi(optarg);
                break;
            case 'l':
                list_only = 1;
                break;
            case 'g':
                get_pos = 1;
                break;
            case 's':
                slot = atoi(optarg);
                if (slot < 1)
                {
                    fprintf(stderr, "Slot must be >= 1.\n");
                    return -1;
                }
                break;
            case 'd':
                set_direction = atoi(optarg);
                if (set_direction != 0 && set_direction != 1)
                {
                    fprintf(stderr, "Direction must be 0 (disable) or 1 (enable).\n");
                    return -1;
                }
                break;
            case 'C':
                calibrate = 1;
                break;
            case '?':
                print_usage();
                return 0;
            default:
                fprintf(stderr, "Unknown option: %c\n", opt);
                print_usage();
                return -1;
        }
    }

    int numDevices = EFWGetNum();
    if (numDevices <= 0)
    {
        printf("No filter wheel detected.\n");
        printf("ASI Filter Wheel Test failed.\n");
        return -1;
    }

    printf("attached filter wheels: %d\n", numDevices);

    // List all EFW devices and their properties
    for (int i = 0; i < numDevices; i++)
    {
        int id;
        EFWGetID(i, &id);
        EFW_INFO info;
        EFW_ERROR_CODE result = EFWGetProperty(id, &info);
        if (result == EFW_SUCCESS)
        {
            printf("Index %d: ID=%d, Name=%s, Slots=%d\n", i, id, info.Name, info.slotNum);
        }
        else
        {
            printf("Index %d: ID=%d, Failed to get property (error: %d)\n", i, id, result);
        }
    }

    // Get the ID for the specified index
    int target_id;
    EFWGetID(wheel_id, &target_id);
    if (target_id < 0)
    {
        fprintf(stderr, "Invalid filter wheel index: %d (available: 0-%d)\n", wheel_id, numDevices - 1);
        return -1;
    }

    EFW_INFO info;
    EFW_ERROR_CODE result = EFWGetProperty(target_id, &info);
    if (result != EFW_SUCCESS)
    {
        fprintf(stderr, "Failed to get filter wheel property for ID %d (error: %d)\n", target_id, result);
        return -1;
    }

    printf("\nUsing filter wheel: ID=%d, Name=%s, Slots=%d\n", target_id, info.Name, info.slotNum);

    // If only listing, we're done
    if (list_only)
    {
        return 0;
    }

    // Open the filter wheel
    result = EFWOpen(target_id);
    if (result != EFW_SUCCESS)
    {
        fprintf(stderr, "Failed to open filter wheel ID %d (error: %d). Are you root?\n", target_id, result);
        return -1;
    }

    printf("Filter wheel opened successfully.\n");

    // Get current position
    int current;
    result = EFWGetPosition(target_id, &current);
    if (result != EFW_SUCCESS)
    {
        fprintf(stderr, "Failed to get current position (error: %d)\n", result);
        EFWClose(target_id);
        return -1;
    }

    if (current == EFW_IS_MOVING)
        printf("Current position: moving...\n");
    else
        printf("Current position: %d\n", current + 1);

    // Get unidirectional mode status
    bool isUniDirection = false;
    result = EFWGetDirection(target_id, &isUniDirection);
    if (result == EFW_SUCCESS)
    {
        printf("Unidirectional mode: %s\n", isUniDirection ? "Enabled" : "Disabled");
    }

    // Get position only
    if (get_pos)
    {
        EFWClose(target_id);
        return 0;
    }

    // Set unidirectional mode
    if (set_direction >= 0)
    {
        bool enable = (set_direction == 1);
        result = EFWSetDirection(target_id, enable);
        if (result == EFW_SUCCESS)
        {
            printf("Unidirectional mode set to: %s\n", enable ? "Enabled" : "Disabled");
        }
        else
        {
            fprintf(stderr, "Failed to set unidirectional mode (error: %d)\n", result);
            EFWClose(target_id);
            return -1;
        }
    }

    // Calibrate
    if (calibrate)
    {
        printf("Calibrating filter wheel...\n");
        result = EFWCalibrate(target_id);
        if (result != EFW_SUCCESS)
        {
            fprintf(stderr, "Failed to calibrate filter wheel (error: %d)\n", result);
            EFWClose(target_id);
            return -1;
        }

        printf("Calibration started. Waiting for completion...\n");
        // Poll until calibration is complete
        int pos;
        int timeout = 120; // 2 minute timeout
        while (timeout-- > 0)
        {
            usleep(500000); // 500ms
            result = EFWGetPosition(target_id, &pos);
            if (result == EFW_SUCCESS && pos != EFW_IS_MOVING)
            {
                printf("Calibration complete. Position: %d\n", pos + 1);
                break;
            }
        }
        if (timeout <= 0)
        {
            fprintf(stderr, "Calibration timed out.\n");
            EFWClose(target_id);
            return -1;
        }
    }

    // Set position
    if (slot > 0)
    {
        if (slot < 1 || slot > info.slotNum)
        {
            fprintf(stderr, "Invalid slot %d. Must be between 1 and %d.\n", slot, info.slotNum);
            EFWClose(target_id);
            return -1;
        }

        if (current != EFW_IS_MOVING && (current + 1) == slot)
        {
            printf("Already at slot %d.\n", slot);
            EFWClose(target_id);
            return 0;
        }

        printf("Moving filter wheel from slot %d to slot %d...\n", current + 1, slot);
        result = EFWSetPosition(target_id, slot - 1);
        if (result != EFW_SUCCESS)
        {
            fprintf(stderr, "Failed to set position to slot %d (error: %d)\n", slot, result);
            EFWClose(target_id);
            return -1;
        }

        // Poll until movement is complete
        int pos;
        int timeout = 120; // 2 minute timeout
        while (timeout-- > 0)
        {
            usleep(250000); // 250ms - matches the driver's default polling period
            result = EFWGetPosition(target_id, &pos);
            if (result == EFW_SUCCESS)
            {
                if (pos == EFW_IS_MOVING)
                {
                    printf("Moving...\n");
                }
                else if (pos + 1 == slot)
                {
                    printf("Movement complete. Position: %d\n", pos + 1);
                    break;
                }
                else
                {
                    printf("Intermediate position: %d (target: %d)\n", pos + 1, slot);
                }
            }
        }
        if (timeout <= 0)
        {
            fprintf(stderr, "Movement timed out.\n");
            EFWClose(target_id);
            return -1;
        }
    }

    // If no action was specified, just show info
    if (!get_pos && slot < 0 && set_direction < 0 && !calibrate)
    {
        printf("\nNo action specified. Use options to control the filter wheel.\n");
        printf("Run with -? for usage information.\n");
    }

    EFWClose(target_id);
    printf("ASI Filter Wheel Test completed successfully.\n");
    return 0;
}