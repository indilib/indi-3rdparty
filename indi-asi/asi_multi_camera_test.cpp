/*
 ASI Camera Test

 Based on ZWO ASI Snap Demo

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

#include <ASICamera2.h>
#include "stdio.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "pthread.h"
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <libusb-1.0/libusb.h>
#include <vector>
#include <string>
#include <algorithm>

struct CameraThreadData
{
    int camera_id;
    long exposure_ms;
    int bin;
    int image_type;
    int usb_traffic;
    const char *name;
};

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
    printf("Usage: asi_multi_camera_test [options]\n");
    printf("Options:\n");
    printf("  -p                 Probe USB system and exit\n");
    printf("  -r                 Probe and reset USB device and exit\n");
    printf("  -e1 <exposure>     Primary camera exposure time in milliseconds (default: 30000)\n");
    printf("  -e2 <exposure>     Guide camera exposure time in milliseconds (default: 1000)\n");
    printf("  -t <traffic>       USB traffic value (0-100, default: 40)\n");
    printf("  -?                 Show this help message\n");
}

void *camera_thread_function(void *arg)
{
    CameraThreadData *data = (CameraThreadData *)arg;
    int CamNum = data->camera_id;
    long exp_ms = data->exposure_ms;
    int bin = data->bin;
    int imageFormat = data->image_type;
    int usb_traffic = data->usb_traffic;
    const char *cameraName = data->name;
    const int capture_count = 100;

    printf("[%s] Thread started for camera ID %d\n", cameraName, CamNum);


    if (ASIOpenCamera(CamNum) != ASI_SUCCESS)
    {
        fprintf(stderr, "[%s] Failed to open camera\n", cameraName);
        return nullptr;
    }

    ASIInitCamera(CamNum);

    ASI_CAMERA_INFO ASICameraInfo;
    ASIGetCameraProperty(&ASICameraInfo, CamNum);

    int width = ASICameraInfo.MaxWidth;
    int height = ASICameraInfo.MaxHeight;

    if (ASI_SUCCESS != ASISetROIFormat(CamNum, width, height, bin, static_cast<ASI_IMG_TYPE>(imageFormat)))
    {
        fprintf(stderr, "[%s] Failed to set ROI format\n", cameraName);
        ASICloseCamera(CamNum);
        return nullptr;
    }

    long imgSize = width * height * (1 + (imageFormat == ASI_IMG_RAW16));
    unsigned char *imgBuf = new unsigned char[imgSize];

    ASISetControlValue(CamNum, ASI_GAIN, 0, ASI_FALSE);
    ASISetControlValue(CamNum, ASI_EXPOSURE, exp_ms * 1000, ASI_FALSE);
    ASISetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, usb_traffic, ASI_FALSE);

    for (int capture = 1; capture <= capture_count; capture++)
    {
        printf("[%s] Starting exposure %d...\n", cameraName, capture);
        ASIStartExposure(CamNum, ASI_FALSE);

        ASI_EXPOSURE_STATUS status;
        do
        {
            ASIGetExpStatus(CamNum, &status);
            usleep(100000);
        }
        while (status == ASI_EXP_WORKING);

        if (status == ASI_EXP_SUCCESS)
        {
            if (ASIGetDataAfterExp(CamNum, imgBuf, imgSize) == ASI_SUCCESS)
            {
                printf("[%s] Exposure %d successful\n", cameraName, capture);
            }
            else
            {
                fprintf(stderr, "[%s] Failed to get data after exposure %d\n", cameraName, capture);
            }
        }
        else
        {
            fprintf(stderr, "[%s] Exposure %d failed with status %d\n", cameraName, capture, status);
        }
    }

    delete[] imgBuf;
    ASICloseCamera(CamNum);
    printf("[%s] Thread finished for camera ID %d\n", cameraName, CamNum);
    return nullptr;
}

int main(int argc, char *argv[])
{
    long primary_exp_ms = 30000;
    long guide_exp_ms = 1000;
    int usb_traffic = 40;  // Default USB traffic value
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "pre:t:?")) != -1)
    {
        switch (opt)
        {
            case 'p':
                probe_usb_system(true);
                return 0;
            case 'r':
            {
                printf("Probing USB system to find ZWO camera...\n");
                auto devices = probe_usb_system(true);
                if (devices.empty())
                {
                    fprintf(stderr, "No ZWO camera found in USB system\n");
                    return -1;
                }

                // Use the first found device
                printf("Found ZWO camera with product ID: 0x%04x\n", devices[0].product_id);
                reset_usb_device(0x03c3, devices[0].product_id);
                return 0;
            }
            case 'e':
                if (strcmp(optarg, "1") == 0)
                    primary_exp_ms = atol(argv[optind]);
                else if (strcmp(optarg, "2") == 0)
                    guide_exp_ms = atol(argv[optind]);
                optind++;
                break;
            case 't':
                usb_traffic = atoi(optarg);
                if (usb_traffic < 0 || usb_traffic > 100)
                {
                    fprintf(stderr, "USB traffic value must be between 0 and 100.\n");
                    return -1;
                }
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

    int numDevices = ASIGetNumOfConnectedCameras();
    if (numDevices < 2)
    {
        printf("This test requires at least two cameras.\n");
        return -1;
    }

    std::vector<ASI_CAMERA_INFO> camera_infos(numDevices);
    for (int i = 0; i < numDevices; i++)
    {
        ASIGetCameraProperty(&camera_infos[i], i);
    }

    // Identify primary and guide cameras
    int primary_cam_idx = -1, guide_cam_idx = -1;
    long max_res = 0, min_res = -1;

    for (int i = 0; i < numDevices; i++)
    {
        long current_res = camera_infos[i].MaxWidth * camera_infos[i].MaxHeight;
        if (current_res > max_res)
        {
            max_res = current_res;
            primary_cam_idx = i;
        }
        if (min_res == -1 || current_res < min_res)
        {
            min_res = current_res;
            guide_cam_idx = i;
        }
    }

    if (primary_cam_idx == guide_cam_idx)
    {
        // If only one resolution is found, assign them based on index
        primary_cam_idx = 0;
        guide_cam_idx = 1;
    }

    printf("Primary Camera: %s (ID: %d)\n", camera_infos[primary_cam_idx].Name, primary_cam_idx);
    printf("Guide Camera: %s (ID: %d)\n", camera_infos[guide_cam_idx].Name, guide_cam_idx);

    pthread_t primary_thread, guide_thread;
    CameraThreadData primary_data = {primary_cam_idx, primary_exp_ms, 1, ASI_IMG_RAW16, usb_traffic, "Primary"};
    primary_data.image_type = ASI_IMG_RAW16;
    CameraThreadData guide_data = {guide_cam_idx, guide_exp_ms, 2, ASI_IMG_RAW8, usb_traffic, "Guide"};
    guide_data.image_type = ASI_IMG_RAW8;


    pthread_create(&primary_thread, NULL, camera_thread_function, &primary_data);
    pthread_create(&guide_thread, NULL, camera_thread_function, &guide_data);

    pthread_join(primary_thread, NULL);
    pthread_join(guide_thread, NULL);

    printf("Multi-camera test finished.\n");

    return 0;
}
