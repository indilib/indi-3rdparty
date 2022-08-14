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

int  main()
{
    int width, height;
    const char *formats[] = {"RAW 8-bit", "RGB 24-bit", "RAW 16-bit", "Luma 8-bit" };
    int CamNum = 0;

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

    printf("\nselect one to preview\n");
    if (scanf("%d", &CamNum) != 1)
    {
        fprintf(stderr, "Error no input. Assuming camera 0");
        CamNum = 0;
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

    printf("\nImage Formats:\n\n");
    for (int i = 0; i < 8; i++)
    {
        if (ASICameraInfo.SupportedVideoFormat[i] == ASI_IMG_END)
            break;

        printf("Format #%d : %s\n", i, formats[i]);
    }

    int bin = 1, imageFormat = 0;

    do
    {
        printf("\nPlease input the <width height bin format> with one space, ie. 640 480 2 0. Leave w/h to zero to use maximum.\n");

        if (scanf("%d %d %d %d", &width, &height, &bin, &imageFormat) == 4)
        {
            if(width == 0 || height == 0)
            {
                width = iMaxWidth;
                height = iMaxHeight;
            }
        }
    }
    while(ASI_SUCCESS != ASISetROIFormat(CamNum, width, height, bin, static_cast<ASI_IMG_TYPE>(imageFormat)));

    printf("\nset image format %d %d %d %d success, Will capture now a 100ms image.\n", width, height, bin, imageFormat);

    ASIGetROIFormat(CamNum, &width, &height, &bin, (ASI_IMG_TYPE*)&imageFormat);
    long imgSize = width * height * (1 + (imageFormat == ASI_IMG_RAW16));
    unsigned char* imgBuf = new unsigned char[imgSize];

    ASISetControlValue(CamNum, ASI_GAIN, 0, ASI_FALSE);

    int exp_ms = 100;
    ASISetControlValue(CamNum, ASI_EXPOSURE, exp_ms * 1000, ASI_FALSE);
    ASISetControlValue(CamNum, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE);

    ASI_EXPOSURE_STATUS status;


    ASIStartExposure(CamNum, ASI_FALSE);
    //10ms
    usleep(10000);
    status = ASI_EXP_WORKING;
    while(status == ASI_EXP_WORKING)
    {
        ASIGetExpStatus(CamNum, &status);
    }
    if(status == ASI_EXP_SUCCESS)
    {
        ASIGetDataAfterExp(CamNum, imgBuf, imgSize);

        printf("Image successfully captured. Writing image to image.raw file...\n");

        FILE *output = fopen("image.raw", "wb");
        if (output)
        {
            int n = 0 ;
            for (int i = 0; i < imgSize; i += n)
                n = fwrite(imgBuf + i, 1, imgSize - i, output);
            fclose(output);
        }
        else
        {
            fprintf(stderr, "Failed to open a file for writing!");
            printf("ASI Camera Test failed.\n");
            return -1;
        }
    }
    else
        fprintf(stderr, "Failed to capture an image: %d\n", status);

    ASIStopExposure(CamNum);
    ASICloseCamera(CamNum);
    if(imgBuf)
        delete[] imgBuf;
    printf("ASI Camera Test completed successfully\n");
    return 0;
}
