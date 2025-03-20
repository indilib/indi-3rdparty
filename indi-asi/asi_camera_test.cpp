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
    printf("Usage: asi_camera_test [options]\n");
    printf("Options:\n");
    printf("  -p                 Probe USB system and exit\n");
    printf("  -r                 Probe and reset USB device and exit\n");
    printf("  -c <camera_id>     Camera ID to use (default: 0)\n");
    printf("  -w <width>         Image width (0 for max)\n");
    printf("  -h <height>        Image height (0 for max)\n");
    printf("  -b <bin>           Binning value (default: 1)\n");
    printf("  -f <format>        Image format (default: 0)\n");
    printf("                     0: RAW 8-bit\n");
    printf("                     1: RGB 24-bit\n");
    printf("                     2: RAW 16-bit\n");
    printf("                     3: Luma 8-bit\n");
    printf("  -e <exposure>      Exposure time in milliseconds (default: 100)\n");
    printf("  -n <count>         Number of images to capture (minimum: 1, default: 1)\n");
    printf("  -t <traffic>       USB traffic value (0-100, default: 40)\n");
    printf("  -?                 Show this help message\n");
}

int main(int argc, char *argv[])
{
    int width = 0, height = 0;
    int CamNum = 0, bin = 1, imageFormat = 0;
    int exp_ms = 100;
    int capture_count = 1;
    int usb_traffic = 40;  // Default USB traffic value
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "prc:w:h:b:f:e:n:t:?")) != -1)
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
            case 'c':
                CamNum = atoi(optarg);
                break;
            case 'w':
                width = atoi(optarg);
                break;
            case 'h':
                height = atoi(optarg);
                break;
            case 'b':
                bin = atoi(optarg);
                break;
            case 'f':
                imageFormat = atoi(optarg);
                if (imageFormat < 0 || imageFormat > 3)
                {
                    fprintf(stderr, "Invalid format. Must be between 0 and 3.\n");
                    return -1;
                }
                break;
            case 'e':
                exp_ms = atoi(optarg);
                if (exp_ms <= 0)
                {
                    fprintf(stderr, "Exposure time must be positive.\n");
                    return -1;
                }
                break;
            case 'n':
                capture_count = atoi(optarg);
                if (capture_count < 1)
                {
                    fprintf(stderr, "Capture count must be at least 1.\n");
                    return -1;
                }
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
    if(numDevices <= 0)
    {
        printf("No camera detected.\n");
        printf("ASI Camera Test failed.\n");
        return -1;
    }
    else
        printf("attached cameras:\n");

    ASI_CAMERA_INFO ASICameraInfo;

    for(int i = 0; i < numDevices; i++)
    {
        ASIGetCameraProperty(&ASICameraInfo, i);
        printf("%d %s\n", i, ASICameraInfo.Name);

        if(ASIOpenCamera(ASICameraInfo.CameraID) != ASI_SUCCESS)
        {
            printf("failed to open camera id %d\n", ASICameraInfo.CameraID);
        }

        ASI_SN asi_sn;
        if (ASIGetSerialNumber(i, &asi_sn) == ASI_SUCCESS)
        {
            printf("serial number for %d: ", i);
            for (int i = 0; i < 8; ++i)
                printf("%02x", asi_sn.id[i] & 0xff);
            printf("\n");
        }
        else
        {
            printf("Serial number for %d is unavailable\n", i);
        }

        if(ASICloseCamera(ASICameraInfo.CameraID) != ASI_SUCCESS)
        {
            printf("failed to close camera id %d\n", ASICameraInfo.CameraID);
        }

    }

    if(ASIOpenCamera(CamNum) != ASI_SUCCESS)
    {
        printf("OpenCamera error,are you root?\n");
        printf("ASI Camera Test failed.\n");
        return -1;
    }
    ASIInitCamera(CamNum);

    ASIGetCameraProperty(&ASICameraInfo, CamNum);

    printf("%s information\n", ASICameraInfo.Name);
    int iMaxWidth, iMaxHeight;
    iMaxWidth = ASICameraInfo.MaxWidth;
    iMaxHeight =  ASICameraInfo.MaxHeight;
    printf("resolution:%dX%d\n", iMaxWidth, iMaxHeight);
    if(ASICameraInfo.IsColorCam)
    {
        const char* bayer[] = {"RG", "BG", "GR", "GB"};
        printf("Color Camera: bayer pattern:%s\n", bayer[ASICameraInfo.BayerPattern]);
    }
    else
        printf("Mono camera\n");

    ASI_CONTROL_CAPS ControlCaps;
    int iNumOfCtrl = 0;
    ASIGetNumOfControls(CamNum, &iNumOfCtrl);
    for(int i = 0; i < iNumOfCtrl; i++)
    {
        ASIGetControlCaps(CamNum, i, &ControlCaps);
        printf("%s\n", ControlCaps.Name);
    }

    long ltemp = 0;
    ASI_BOOL bAuto = ASI_FALSE;
    ASIGetControlValue(CamNum, ASI_TEMPERATURE, &ltemp, &bAuto);
    printf("sensor temperature:%02f\n", (float)ltemp / 10.0);

    // If width or height is 0, use maximum values
    if(width == 0 || height == 0)
    {
        width = iMaxWidth;
        height = iMaxHeight;
    }

    if(ASI_SUCCESS != ASISetROIFormat(CamNum, width, height, bin, static_cast<ASI_IMG_TYPE>(imageFormat)))
    {
        fprintf(stderr, "Failed to set ROI format\n");
        return -1;
    }

    printf("Set image format %d %d %d %d success\n", width, height, bin, imageFormat);

    ASIGetROIFormat(CamNum, &width, &height, &bin, (ASI_IMG_TYPE*)&imageFormat);
    long imgSize = width * height * (1 + (imageFormat == ASI_IMG_RAW16));
    unsigned char* imgBuf = new unsigned char[imgSize];

    ASISetControlValue(CamNum, ASI_GAIN, 0, ASI_FALSE);

    ASISetControlValue(CamNum, ASI_EXPOSURE, exp_ms * 1000, ASI_FALSE);
    ASISetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, usb_traffic, ASI_FALSE);
    printf("USB traffic set to: %d\n", usb_traffic);

    for(int capture = 1; capture <= capture_count; capture++)
    {
        ASI_EXPOSURE_STATUS status;
        bool capture_success = false;
        int retry_count = 0;
        const int max_retries = 3;

        printf("Capturing image %d of %d...\n", capture, capture_count);

        while (!capture_success && retry_count < max_retries)
        {
            if (retry_count > 0)
            {
                printf("Retrying capture (attempt %d of %d)...\n", retry_count + 1, max_retries);
                // Add a longer delay between retries
                usleep(500000);  // 500ms delay
            }

            // Start exposure
            ASI_ERROR_CODE exp_result = ASIStartExposure(CamNum, ASI_FALSE);
            if (exp_result == ASI_ERROR_EXPOSURE_IN_PROGRESS)
            {
                fprintf(stderr, "Exposure already in progress, stopping it first\n");
                ASIStopExposure(CamNum);
                retry_count++;
                continue;
            }
            else if (exp_result != ASI_SUCCESS)
            {
                fprintf(stderr, "Failed to start exposure (error code: %d). ", exp_result);
                switch(exp_result)
                {
                    case ASI_ERROR_INVALID_ID:
                        fprintf(stderr, "Invalid camera ID\n");
                        break;
                    case ASI_ERROR_CAMERA_CLOSED:
                        fprintf(stderr, "Camera is closed\n");
                        break;
                    case ASI_ERROR_CAMERA_REMOVED:
                        fprintf(stderr, "Camera was removed\n");
                        break;
                    case ASI_ERROR_INVALID_MODE:
                        fprintf(stderr, "Invalid mode\n");
                        break;
                    default:
                        fprintf(stderr, "Unknown error\n");
                }
                retry_count++;
                continue;
            }

            // Initial delay
            usleep(10000);
            status = ASI_EXP_WORKING;

            // Monitor exposure progress
            time_t start_time = time(NULL);
            int progress_counter = 0;
            while(status == ASI_EXP_WORKING)
            {
                ASIGetExpStatus(CamNum, &status);

                // Print progress every second
                if (time(NULL) - start_time > progress_counter)
                {
                    progress_counter++;
                    printf("Exposure in progress... %d seconds elapsed\n", progress_counter);
                }

                usleep(100000);  // 100ms delay between checks
            }

            if (status == ASI_EXP_SUCCESS)
            {
                ASI_ERROR_CODE data_result = ASIGetDataAfterExp(CamNum, imgBuf, imgSize);
                if (data_result != ASI_SUCCESS)
                {
                    fprintf(stderr, "Failed to get image data (error code: %d)\n", data_result);

                    // Probe USB system for diagnostic information
                    auto devices = probe_usb_system(true);
                    if (!devices.empty())
                    {
                        printf("\nAttempting recovery sequence...\n");
                        reset_usb_device(0x03c3, devices[0].product_id);  // Reset using detected camera ID
                    }

                    retry_count++;
                    ASIStopExposure(CamNum);
                    continue;
                }

                char filename[32];
                snprintf(filename, sizeof(filename), "image_%03d.raw", capture);
                printf("Image successfully captured. Writing to %s...\n", filename);

                FILE *output = fopen(filename, "wb");
                if (output)
                {
                    int n = 0;
                    for (int i = 0; i < imgSize; i += n)
                        n = fwrite(imgBuf + i, 1, imgSize - i, output);
                    fclose(output);
                    capture_success = true;
                }
                else
                {
                    fprintf(stderr, "Failed to open %s for writing!\n", filename);
                    retry_count++;
                }
            }
            else
            {
                fprintf(stderr, "Exposure failed (status: %d). ", status);
                switch(status)
                {
                    case ASI_EXP_FAILED:
                        fprintf(stderr, "General exposure failure\n");
                        break;
                    case ASI_EXP_IDLE:
                        fprintf(stderr, "Exposure was not started\n");
                        break;
                    default:
                        fprintf(stderr, "Unknown status\n");
                }

                // Stop exposure and try USB reset
                ASIStopExposure(CamNum);
                ASICloseCamera(CamNum);

                // Get USB device info
                auto devices = probe_usb_system(true);
                if (!devices.empty())
                {
                    printf("\nAttempting recovery sequence...\n");
                    reset_usb_device(0x03c3, devices[0].product_id);  // Reset using detected camera ID
                }

                printf("Waiting for device to settle...\n");
                usleep(3000000);  // Wait 3 seconds

                // Reopen camera after reset
                printf("Reopening camera...\n");
                if(ASIOpenCamera(CamNum) != ASI_SUCCESS)
                {
                    fprintf(stderr, "Failed to reopen camera after USB reset\n");
                    return -1;
                }

                printf("Reinitializing camera...\n");
                ASIInitCamera(CamNum);

                // Restore previous settings
                ASISetROIFormat(CamNum, width, height, bin, static_cast<ASI_IMG_TYPE>(imageFormat));
                ASISetControlValue(CamNum, ASI_GAIN, 0, ASI_FALSE);
                ASISetControlValue(CamNum, ASI_EXPOSURE, exp_ms * 1000, ASI_FALSE);
                ASISetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, usb_traffic, ASI_FALSE);

                retry_count++;
                continue;
            }
        }

        if (!capture_success)
        {
            fprintf(stderr, "Failed to capture image %d after %d attempts\n", capture, max_retries);
            ASICloseCamera(CamNum);
            if(imgBuf)
                delete[] imgBuf;
            return -1;
        }

        // If we have more images to capture, add a small delay
        if(capture < capture_count)
            usleep(100000);  // 100ms delay between captures
    }
    ASICloseCamera(CamNum);
    if(imgBuf)
        delete[] imgBuf;
    printf("ASI Camera Test completed successfully\n");
    return 0;
}
