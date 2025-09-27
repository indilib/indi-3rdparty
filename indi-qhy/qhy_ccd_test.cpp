/*
 QHY Test CCD

 Copyright (C) 2017 Jan Soldan
 Copyright (C) 2026 Jasem Mutlaq

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <qhyccd.h>

#define VERSION 1.10

void usage(const char *progName)
{
    printf("Usage: %s [OPTIONS]\n", progName);
    printf("Options:\n");
    printf("  -x, --roi-start-x <value>    ROI Start X (default: 0)\n");
    printf("  -y, --roi-start-y <value>    ROI Start Y (default: 0)\n");
    printf("  -w, --roi-width <value>      ROI Width (default: maxImageSizeX)\n");
    printf("  -h, --roi-height <value>     ROI Height (default: maxImageSizeY)\n");
    printf("  -e, --exposure <value>       Exposure time in seconds (default: 1)\n");
    printf("  -r, --readout-mode <value>   Readout mode index (default: 0)\n");
    printf("  -g, --gain <value>           Gain (default: 10)\n");
    printf("  -o, --offset <value>         Offset (default: 140)\n");
    printf("  -b, --binning <value>        Binning mode (e.g., 1 for 1x1, 2 for 2x2) (default: 1)\n");
    printf("  -t, --usb-traffic <value>    USB Traffic (default: 10)\n");
    printf("  --help                       Display this help message\n");
}

int main(int argc, char *argv[])
{
    int USB_TRAFFIC = 10;
    int CHIP_GAIN = 10;
    int CHIP_OFFSET = 140;
    int EXPOSURE_TIME = 1; // in seconds
    int camBinX = 1;
    int camBinY = 1;
    int readoutMode = 0; // Default readout mode

    unsigned int roiStartX = 0;
    unsigned int roiStartY = 0;
    unsigned int roiSizeX = 0; // Will be set to maxImageSizeX later
    unsigned int roiSizeY = 0; // Will be set to maxImageSizeY later

    bool gainSet = false;
    bool offsetSet = false;
    bool usbTrafficSet = false;

    double chipWidthMM;
    double chipHeightMM;
    double pixelWidthUM;
    double pixelHeightUM;

    unsigned int overscanStartX;
    unsigned int overscanStartY;
    unsigned int overscanSizeX;
    unsigned int overscanSizeY;

    unsigned int effectiveStartX;
    unsigned int effectiveStartY;
    unsigned int effectiveSizeX;
    unsigned int effectiveSizeY;

    unsigned int maxImageSizeX;
    unsigned int maxImageSizeY;
    unsigned int bpp;
    unsigned int channels;

    unsigned char *pImgData = 0;

    static struct option long_options[] =
    {
        {"roi-start-x", required_argument, 0, 'x'},
        {"roi-start-y", required_argument, 0, 'y'},
        {"roi-width", required_argument, 0, 'w'},
        {"roi-height", required_argument, 0, 'h'},
        {"exposure", required_argument, 0, 'e'},
        {"readout-mode", required_argument, 0, 'r'},
        {"gain", required_argument, 0, 'g'},
        {"offset", required_argument, 0, 'o'},
        {"binning", required_argument, 0, 'b'},
        {"usb-traffic", required_argument, 0, 't'},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int c;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "x:y:w:h:e:r:g:o:b:t:", long_options, &option_index)) != -1)
    {
        switch (c)
        {
            case 'x':
                roiStartX = (unsigned int)atoi(optarg);
                break;
            case 'y':
                roiStartY = (unsigned int)atoi(optarg);
                break;
            case 'w':
                roiSizeX = (unsigned int)atoi(optarg);
                break;
            case 'h':
                roiSizeY = (unsigned int)atoi(optarg);
                break;
            case 'e':
                EXPOSURE_TIME = atoi(optarg);
                break;
            case 'r':
                readoutMode = atoi(optarg);
                break;
            case 'g':
                CHIP_GAIN = atoi(optarg);
                gainSet = true;
                break;
            case 'o':
                CHIP_OFFSET = atoi(optarg);
                offsetSet = true;
                break;
            case 'b':
                camBinX = atoi(optarg);
                camBinY = atoi(optarg);
                break;
            case 't':
                USB_TRAFFIC = atoi(optarg);
                usbTrafficSet = true;
                break;
            case '?':
                // getopt_long already printed an error message.
                usage(argv[0]);
                return 1;
            case 0:
                // Long option for --help
                if (strcmp("help", long_options[option_index].name) == 0)
                {
                    usage(argv[0]);
                    return 0;
                }
                break;
            default:
                abort();
        }
    }

    printf("QHY Test CCD using SingleFrameMode, Version: %.2f\n", VERSION);

    // init SDK
    int retVal = InitQHYCCDResource();
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("SDK resources initialized.\n");
    }
    else
    {
        printf("Cannot initialize SDK resources, error: %d\n", retVal);
        return 1;
    }

    // scan cameras
    int camCount = ScanQHYCCD();
    if (camCount > 0)
    {
        printf("Number of QHYCCD cameras found: %d \n", camCount);
    }
    else
    {
        printf("No QHYCCD camera found, please check USB or power.\n");
        return 1;
    }

    // iterate over all attached cameras
    bool camFound = false;
    char camId[32];

    for (int i = 0; i < camCount; i++)
    {
        retVal = GetQHYCCDId(i, camId);
        if (QHYCCD_SUCCESS == retVal)
        {
            printf("Application connected to the following camera from the list: Index: %d,  cameraID = %s\n", (i + 1), camId);
            camFound = true;
            break;
        }
    }

    if (!camFound)
    {
        printf("The detected camera is not QHYCCD or other error.\n");
        // release sdk resources
        retVal = ReleaseQHYCCDResource();
        if (QHYCCD_SUCCESS == retVal)
        {
            printf("SDK resources released.\n");
        }
        else
        {
            printf("Cannot release SDK resources, error %d.\n", retVal);
        }
        return 1;
    }

    // open camera
    qhyccd_handle *pCamHandle = OpenQHYCCD(camId);
    if (pCamHandle != nullptr)
    {
        printf("Open QHYCCD success.\n");
    }
    else
    {
        printf("Open QHYCCD failure.\n");
        return 1;
    }

    // set single frame mode
    int mode = 0;
    retVal = SetQHYCCDStreamMode(pCamHandle, mode);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("SetQHYCCDStreamMode set to: %d, success.\n", mode);
    }
    else
    {
        printf("SetQHYCCDStreamMode: %d failure, error: %d\n", mode, retVal);
        return 1;
    }

    // Set readout mode
    retVal = SetQHYCCDReadMode(pCamHandle, readoutMode);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("SetQHYCCDReadMode set to: %d, success.\n", readoutMode);
    }
    else
    {
        printf("SetQHYCCDReadMode: %d failure, error: %d\n", readoutMode, retVal);
        return 1;
    }

    // initialize camera
    retVal = InitQHYCCD(pCamHandle);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("InitQHYCCD success.\n");
    }
    else
    {
        printf("InitQHYCCD faililure, error: %d\n", retVal);
        return 1;
    }

    // Check if filter wheel is connected
    retVal = IsQHYCCDCFWPlugged(pCamHandle);
    if (retVal == QHYCCD_SUCCESS)
    {
        printf("Filter wheel is connected.\n");
        int filterCount = GetQHYCCDParam(pCamHandle, CONTROL_CFWSLOTSNUM);
        if (filterCount > 0 && filterCount <= 16)
        {
            printf("Filter wheel has %d positions.\n", filterCount);

            char currentPos[64] = {0};
            if (GetQHYCCDCFWStatus(pCamHandle, currentPos) == QHYCCD_SUCCESS)
            {
                int position = strtol(currentPos, nullptr, 16) + 1;
                printf("Current filter position: %d\n", position);
            }
        }
        else
        {
            printf("Filter wheel reports invalid number of positions: %d\n", filterCount);
        }
    }
    else
    {
        printf("No filter wheel detected.\n");
    }

    // get overscan area
    retVal = GetQHYCCDOverScanArea(pCamHandle, &overscanStartX, &overscanStartY, &overscanSizeX, &overscanSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("GetQHYCCDOverScanArea:\n");
        printf("Overscan Area startX x startY : %d x %d\n", overscanStartX, overscanStartY);
        printf("Overscan Area sizeX  x sizeY  : %d x %d\n", overscanSizeX, overscanSizeY);
    }
    else
    {
        printf("GetQHYCCDOverScanArea failure, error: %d\n", retVal);
        return 1;
    }

    // get effective area
    retVal = GetQHYCCDOverScanArea(pCamHandle, &effectiveStartX, &effectiveStartY, &effectiveSizeX, &effectiveSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("GetQHYCCDEffectiveArea:\n");
        printf("Effective Area startX x startY: %d x %d\n", effectiveStartX, effectiveStartY);
        printf("Effective Area sizeX  x sizeY : %d x %d\n", effectiveSizeX, effectiveSizeY);
    }
    else
    {
        printf("GetQHYCCDOverScanArea failure, error: %d\n", retVal);
        return 1;
    }

    // get chip info
    retVal = GetQHYCCDChipInfo(pCamHandle, &chipWidthMM, &chipHeightMM, &maxImageSizeX, &maxImageSizeY, &pixelWidthUM,
                               &pixelHeightUM, &bpp);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("GetQHYCCDChipInfo:\n");
        printf("Effective Area startX x startY: %d x %d\n", effectiveStartX, effectiveStartY);
        printf("Chip  size width x height     : %.3f x %.3f [mm]\n", chipWidthMM, chipHeightMM);
        printf("Pixel size width x height     : %.3f x %.3f [um]\n", pixelWidthUM, pixelHeightUM);
        printf("Image size width x height     : %d x %d\n", maxImageSizeX, maxImageSizeY);
    }
    else
    {
        printf("GetQHYCCDChipInfo failure, error: %d\n", retVal);
        return 1;
    }

    // Set default ROI if not provided by command line
    if (roiSizeX == 0)
    {
        roiSizeX = maxImageSizeX;
    }
    if (roiSizeY == 0)
    {
        roiSizeY = maxImageSizeY;
    }

    // check color camera
    retVal = IsQHYCCDControlAvailable(pCamHandle, CAM_COLOR);
    if (retVal == BAYER_GB || retVal == BAYER_GR || retVal == BAYER_BG || retVal == BAYER_RG)
    {
        printf("This is a color camera.\n");
        SetQHYCCDDebayerOnOff(pCamHandle, true);
        SetQHYCCDParam(pCamHandle, CONTROL_WBR, 20);
        SetQHYCCDParam(pCamHandle, CONTROL_WBG, 20);
        SetQHYCCDParam(pCamHandle, CONTROL_WBB, 20);
    }
    else
    {
        printf("This is a mono camera.\n");
    }

    // check traffic
    if (usbTrafficSet)
    {
        retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_USBTRAFFIC);
        if (QHYCCD_SUCCESS == retVal)
        {
            retVal = SetQHYCCDParam(pCamHandle, CONTROL_USBTRAFFIC, USB_TRAFFIC);
            if (QHYCCD_SUCCESS == retVal)
            {
                printf("SetQHYCCDParam CONTROL_USBTRAFFIC set to: %d, success.\n", USB_TRAFFIC);
            }
            else
            {
                printf("SetQHYCCDParam CONTROL_USBTRAFFIC failure, error: %d\n", retVal);
                getchar();
                return 1;
            }
        }
    }

    // check gain
    if (gainSet)
    {
        retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_GAIN);
        if (QHYCCD_SUCCESS == retVal)
        {
            retVal = SetQHYCCDParam(pCamHandle, CONTROL_GAIN, CHIP_GAIN);
            if (retVal == QHYCCD_SUCCESS)
            {
                printf("SetQHYCCDParam CONTROL_GAIN set to: %d, success\n", CHIP_GAIN);
            }
            else
            {
                printf("SetQHYCCDParam CONTROL_GAIN failure, error: %d\n", retVal);
                getchar();
                return 1;
            }
        }
    }

    // check offset
    if (offsetSet)
    {
        retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_OFFSET);
        if (QHYCCD_SUCCESS == retVal)
        {
            retVal = SetQHYCCDParam(pCamHandle, CONTROL_OFFSET, CHIP_OFFSET);
            if (QHYCCD_SUCCESS == retVal)
            {
                printf("SetQHYCCDParam CONTROL_OFFSET set to: %d, success.\n", CHIP_OFFSET);
            }
            else
            {
                printf("SetQHYCCDParam CONTROL_OFFSET failed.\n");
                getchar();
                return 1;
            }
        }
    }

    // check read mode in QHY42
    char *modeName = (char *)malloc((200) * sizeof(char));
    if (modeName == NULL)
    {
        printf("Failed to allocate memory for modeName.\n");
        return 1;
    }

    retVal = GetQHYCCDReadModeName(pCamHandle, readoutMode, modeName);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("Selected read mode: %d, name: %s \n", readoutMode, modeName);
    }
    else
    {
        printf("Error reading name for selected mode %d \n", readoutMode);
        free(modeName);
        return 1;
    }

    // Set read modes and read resolution for each one (for informational purposes)
    uint32_t numberOfReadModes = 0;
    uint32_t imageRMw, imageRMh;
    retVal = GetQHYCCDNumberOfReadModes(pCamHandle, &numberOfReadModes);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("Available Read Modes:\n");
        for(uint32_t i = 0; i < numberOfReadModes; i++)
        {
            retVal = GetQHYCCDReadModeName(pCamHandle, i, modeName);
            if (QHYCCD_SUCCESS == retVal)
            {
                retVal = GetQHYCCDReadModeResolution(pCamHandle, i, &imageRMw, &imageRMh);
                printf("  Mode %d: %s, Resolution: %d x %d\n", i, modeName, imageRMw, imageRMh);
            }
        }
    }
    else
    {
        printf("Error getting number of read modes.\n");
        free(modeName);
        return 1;
    }
    free(modeName);


    // set exposure time
    retVal = SetQHYCCDParam(pCamHandle, CONTROL_EXPOSURE, EXPOSURE_TIME);
    printf("SetQHYCCDParam CONTROL_EXPOSURE set to: %d, success.\n", EXPOSURE_TIME);
    if (QHYCCD_SUCCESS == retVal)
    {
    }
    else
    {
        printf("SetQHYCCDParam CONTROL_EXPOSURE failure, error: %d\n", retVal);
        getchar();
        return 1;
    }

    // set image resolution
    retVal = SetQHYCCDResolution(pCamHandle, roiStartX, roiStartY, roiSizeX, roiSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("SetQHYCCDResolution roiStartX x roiStartY: %d x %d\n", roiStartX, roiStartY);
        printf("SetQHYCCDResolution roiSizeX  x roiSizeY : %d x %d\n", roiSizeX, roiSizeY);
    }
    else
    {
        printf("SetQHYCCDResolution failure, error: %d\n", retVal);
        return 1;
    }

    // set binning mode
    retVal = SetQHYCCDBinMode(pCamHandle, camBinX, camBinY);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("SetQHYCCDBinMode set to: binX: %d, binY: %d, success.\n", camBinX, camBinY);
    }
    else
    {
        printf("SetQHYCCDBinMode failure, error: %d\n", retVal);
        return 1;
    }

    // set bit resolution
    retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_TRANSFERBIT);
    if (QHYCCD_SUCCESS == retVal)
    {
        retVal = SetQHYCCDBitsMode(pCamHandle, 16);
        if (QHYCCD_SUCCESS == retVal)
        {
            printf("SetQHYCCDParam CONTROL_GAIN set to: %d, success.\n", CONTROL_TRANSFERBIT);
        }
        else
        {
            printf("SetQHYCCDParam CONTROL_GAIN failure, error: %d\n", retVal);
            getchar();
            return 1;
        }
    }

    // single frame
    printf("ExpQHYCCDSingleFrame(pCamHandle) - start...\n");
    retVal = ExpQHYCCDSingleFrame(pCamHandle);
    printf("ExpQHYCCDSingleFrame(pCamHandle) - end...\n");
    if (QHYCCD_ERROR != (uint32_t) retVal)
    {
        printf("ExpQHYCCDSingleFrame success (%d).\n", retVal);
        if (QHYCCD_READ_DIRECTLY != retVal)
        {
            sleep(1);
        }
    }
    else
    {
        printf("ExpQHYCCDSingleFrame failure, error: %d\n", retVal);
        return 1;
    }

    // get requested memory lenght
    uint32_t length = GetQHYCCDMemLength(pCamHandle);

    if (length > 0)
    {
        pImgData = new unsigned char[length];
        memset(pImgData, 0, length);
        printf("Allocated memory for frame: %d [uchar].\n", length);
    }
    else
    {
        printf("Cannot allocate memory for frame.\n");
        return 1;
    }

    // get single frame
    retVal = GetQHYCCDSingleFrame(pCamHandle, &roiSizeX, &roiSizeY, &bpp, &channels, pImgData);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("GetQHYCCDSingleFrame: %d x %d, bpp: %d, channels: %d, success.\n", roiSizeX, roiSizeY, bpp, channels);
        //process image here
    }
    else
    {
        printf("GetQHYCCDSingleFrame failure, error: %d\n", retVal);
    }

    delete [] pImgData;

    retVal = CancelQHYCCDExposingAndReadout(pCamHandle);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("CancelQHYCCDExposingAndReadout success.\n");
    }
    else
    {
        printf("CancelQHYCCDExposingAndReadout failure, error: %d\n", retVal);
        return 1;
    }

    // close camera handle
    retVal = CloseQHYCCD(pCamHandle);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("Close QHYCCD success.\n");
    }
    else
    {
        printf("Close QHYCCD failure, error: %d\n", retVal);
    }

    // release sdk resources
    retVal = ReleaseQHYCCDResource();
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("SDK resources released.\n");
    }
    else
    {
        printf("Cannot release SDK resources, error %d.\n", retVal);
        return 1;
    }

    return 0;
}
