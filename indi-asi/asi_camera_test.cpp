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
    printf("  -?                 Show this help message\n");
}

int main(int argc, char *argv[])
{
    int width = 0, height = 0;
    int CamNum = 0, bin = 1, imageFormat = 0;
    int exp_ms = 100;
    int capture_count = 1;

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "c:w:h:b:f:e:n:?")) != -1)
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
    ASISetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE);

    for(int capture = 1; capture <= capture_count; capture++)
    {
        ASI_EXPOSURE_STATUS status;

        printf("Capturing image %d of %d...\n", capture, capture_count);

        ASIStartExposure(CamNum, ASI_FALSE);
        usleep(10000);
        status = ASI_EXP_WORKING;
        while(status == ASI_EXP_WORKING)
        {
            ASIGetExpStatus(CamNum, &status);
        }

        if(status == ASI_EXP_SUCCESS)
        {
            ASIGetDataAfterExp(CamNum, imgBuf, imgSize);

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
            }
            else
            {
                fprintf(stderr, "Failed to open %s for writing!\n", filename);
                ASIStopExposure(CamNum);
                ASICloseCamera(CamNum);
                if(imgBuf)
                    delete[] imgBuf;
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "Failed to capture image %d: %d\n", capture, status);
            ASIStopExposure(CamNum);
            ASICloseCamera(CamNum);
            if(imgBuf)
                delete[] imgBuf;
            return -1;
        }

        ASIStopExposure(CamNum);

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
