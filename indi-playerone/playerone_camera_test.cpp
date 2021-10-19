/*
 PlayerOne Camera Test

 Based on PlayerOne Camera Snap Demo

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

#include <PlayerOneCamera.h>
#include "stdio.h"
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "pthread.h"

int  main()
{
    int width, height;
    const char *formats[] = {"RAW 8-bit", "RAW 16-bit", "RGB 24-bit", "Luma 8-bit" };
    int CamNum = 0;

    int numDevices = POAGetCameraCount();
    if(numDevices <= 0)
    {
        printf("No camera detected.\n");
        printf("PlayerOne Camera Test failed.\n");
        return -1;
    }
    else
        printf("attached cameras:\n");

    POACameraProperties POACameraInfo;

    for(int i = 0; i < numDevices; i++)
    {
        POAGetCameraProperties(i, &POACameraInfo);
        printf("%d %s\n", i, POACameraInfo.cameraModelName);
    }

    printf("\nselect one to privew\n");
    if (scanf("%d", &CamNum) != 1)
    {
        fprintf(stderr, "Error no input. Assuming camera 0");
        CamNum = 0;
    }

    if(POAOpenCamera(CamNum) != POA_OK)
    {
        printf("OpenCamera error,are you root?\n");
        printf("PlayerOne Camera Test failed.\n");
        return -1;
    }
    POAInitCamera(CamNum);

    printf("%s information\n", POACameraInfo.cameraModelName);
    int iMaxWidth, iMaxHeight;
    iMaxWidth = POACameraInfo.maxWidth;
    iMaxHeight =  POACameraInfo.maxHeight;
    printf("resolution:%dX%d\n", iMaxWidth, iMaxHeight);
    if(POACameraInfo.isColorCamera)
    {
        const char* bayer[] = {"RG", "BG", "GR", "GB"};
        printf("Color Camera: bayer pattern:%s\n", bayer[POACameraInfo.bayerPattern]);
    }
    else
        printf("Mono camera\n");

    POAConfigAttributes ControlCaps;
    int iNumOfCtrl = 0;
    POAGetConfigsCount(CamNum, &iNumOfCtrl);
    for(int i = 0; i < iNumOfCtrl; i++)
    {
        POAGetConfigAttributes(CamNum, i, &ControlCaps);
        printf("%s\n", ControlCaps.szConfName);
    }

    POAConfigValue ltemp;
    POABool bAuto = POA_FALSE;
    POAGetConfig(CamNum, POA_TEMPERATURE, &ltemp, &bAuto);
    printf("sensor temperature:%02f\n", (float)ltemp.floatValue);

    printf("\nImage Formats:\n\n");
    for (int i = 0; i < 8; i++)
    {
        if (POACameraInfo.imgFormats[i] == POA_END)
            break;

        printf("Format #%d : %s\n", i, formats[i]);
    }

    int bin = 1, imageFormat = 0;

    POAErrors ret1, ret2, ret3;

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
        ret1 = POASetImageSize(CamNum, width, height);
        ret2 = POASetImageBin(CamNum, bin);
        ret3 = POASetImageFormat(CamNum, static_cast<POAImgFormat>(imageFormat));
    }
    while(ret1 != POA_OK || ret2 != POA_OK || ret3 != POA_OK);

    printf("\nset image format %d %d %d %d success, Will capture now a 100ms image.\n", width, height, bin, imageFormat);

    POAGetImageSize(CamNum, &width, &height);
    POAGetImageBin(CamNum, &bin);
    POAGetImageFormat(CamNum, (POAImgFormat*)&imageFormat);

    long imgSize = width * height;
    if (imageFormat == POA_RAW16) imgSize *= 2;
    else if (imageFormat == POA_RGB24) imgSize *= 3;

    unsigned char* imgBuf = new unsigned char[imgSize];

    POAConfigValue confVal;
    confVal.intValue = 0;
    POASetConfig(CamNum, POA_GAIN, confVal, POA_FALSE);

    int exp_ms = 100;
    confVal.intValue = exp_ms * 1000;
    POASetConfig(CamNum, POA_EXPOSURE, confVal, POA_FALSE);
    confVal.intValue = 40;
    POASetConfig(CamNum, POA_USB_BANDWIDTH_LIMIT, confVal, POA_FALSE);

    POACameraState status;


    POAStartExposure(CamNum, POA_FALSE);
    //10ms
    usleep(10000);
    POABool pIsReady = POA_FALSE;
    while(pIsReady == POA_FALSE)
    {
        POAImageReady(CamNum, &pIsReady);
    }

    if(pIsReady == POA_TRUE)
    {
        POAGetImageData(CamNum, imgBuf, imgSize, 500);

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
            printf("PlayerOne Camera Test failed.\n");
            return -1;
        }
    }
    else {
        POAGetCameraState(CamNum, &status);
        fprintf(stderr, "Failed to capture an image: %d\n", status);
    }

    POAStopExposure(CamNum);
    POACloseCamera(CamNum);
    if(imgBuf)
        delete[] imgBuf;
    printf("PlayerOne Camera Test completed successfully\n");
    return 0;
}
