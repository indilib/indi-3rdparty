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

void probe_usb_system()
{
    printf("\n=== USB Subsystem Diagnostics ===\n");

    // Run dmesg to check for recent USB errors
    printf("\nChecking recent USB messages from dmesg:\n");
    FILE *fp = popen("dmesg | grep -i usb | tail -n 10", "r");
    if (fp)
    {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), fp))
            printf("%s", buffer);
        pclose(fp);
    }
    else
        fprintf(stderr, "Failed to run dmesg command\n");

    // Get USB device information
    printf("\nUSB device information:\n");
    fp = popen("lsusb -v 2>/dev/null | grep -A 2 -B 2 \"ZWO\"", "r");
    if (fp)
    {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), fp))
            printf("%s", buffer);
        pclose(fp);
    }
    else
        fprintf(stderr, "Failed to run lsusb command\n");

    // Check USB controller status
    printf("\nUSB Controller Status:\n");
    fp = popen("lspci -v | grep -A 4 USB", "r");
    if (fp)
    {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), fp))
            printf("%s", buffer);
        pclose(fp);
    }
    else
        fprintf(stderr, "Failed to get USB controller status\n");

    // Check USB device kernel driver
    printf("\nUSB Device Kernel Driver:\n");
    DIR *dir;
    struct dirent *ent;
    dir = opendir("/sys/bus/usb/devices");
    if (dir)
    {
        while ((ent = readdir(dir)))
        {
            if (ent->d_name[0] != '.')
            {
                char path[256];
                char buffer[256];

                // Check manufacturer
                snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/manufacturer", ent->d_name);
                FILE *f = fopen(path, "r");
                if (f)
                {
                    if (fgets(buffer, sizeof(buffer), f))
                    {
                        if (strstr(buffer, "ZWO"))
                        {
                            printf("Found ZWO device at: %s\n", ent->d_name);

                            // Check driver
                            snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/driver", ent->d_name);
                            char driver_path[256];
                            ssize_t len = readlink(path, driver_path, sizeof(driver_path) - 1);
                            if (len != -1)
                            {
                                driver_path[len] = '\0';
                                printf("Driver: %s\n", basename(driver_path));
                            }

                            // Check speed
                            snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/speed", ent->d_name);
                            f = fopen(path, "r");
                            if (f)
                            {
                                if (fgets(buffer, sizeof(buffer), f))
                                    printf("Speed: %s", buffer);
                                fclose(f);
                            }

                            // Check power
                            snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/power/", ent->d_name);
                            DIR *power_dir = opendir(path);
                            if (power_dir)
                            {
                                struct dirent *power_ent;
                                while ((power_ent = readdir(power_dir)))
                                {
                                    if (strcmp(power_ent->d_name, "control") == 0 ||
                                            strcmp(power_ent->d_name, "autosuspend") == 0)
                                    {
                                        char power_path[512];
                                        snprintf(power_path, sizeof(power_path), "%s%s", path, power_ent->d_name);
                                        f = fopen(power_path, "r");
                                        if (f)
                                        {
                                            if (fgets(buffer, sizeof(buffer), f))
                                                printf("Power %s: %s", power_ent->d_name, buffer);
                                            fclose(f);
                                        }
                                    }
                                }
                                closedir(power_dir);
                            }
                        }
                    }
                    fclose(f);
                }
            }
        }
        closedir(dir);
    }
    else
        fprintf(stderr, "Failed to access USB device information in sysfs\n");

    printf("\n================================\n");
}

void print_usage()
{
    printf("Usage: asi_camera_test [options]\n");
    printf("Options:\n");
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
    while ((opt = getopt(argc, argv, "c:w:h:b:f:e:n:t:?")) != -1)
    {
        switch (opt)
        {
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

            // Get USB traffic before starting exposure
            long usb_traffic = 0;
            ASI_BOOL is_auto = ASI_FALSE;
            ASIGetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, &usb_traffic, &is_auto);
            printf("Current USB traffic: %ld\n", usb_traffic);

            // Start exposure
            ASI_ERROR_CODE exp_result = ASIStartExposure(CamNum, ASI_FALSE);
            if (exp_result != ASI_SUCCESS)
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
                    case ASI_ERROR_EXPOSURE_IN_PROGRESS:
                        fprintf(stderr, "Exposure already in progress\n");
                        break;
                    default:
                        fprintf(stderr, "Unknown error\n");
                }

                // Probe USB system for diagnostic information
                probe_usb_system();

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

                    // Check USB traffic during exposure
                    ASIGetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, &usb_traffic, &is_auto);
                    printf("USB traffic during exposure: %ld\n", usb_traffic);
                }

                usleep(100000);  // 100ms delay between checks
            }

            if(status == ASI_EXP_SUCCESS)
            {
                ASI_ERROR_CODE data_result = ASIGetDataAfterExp(CamNum, imgBuf, imgSize);
                if (data_result != ASI_SUCCESS)
                {
                    fprintf(stderr, "Failed to get image data (error code: %d)\n", data_result);

                    // Probe USB system for diagnostic information
                    probe_usb_system();

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

                // Probe USB system for diagnostic information
                probe_usb_system();

                retry_count++;
            }

            ASIStopExposure(CamNum);
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
