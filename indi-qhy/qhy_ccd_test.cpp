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
#include <chrono>
#include <iomanip>
#include <sstream>

#define VERSION 1.10

// Function to get current timestamp in ISO8601 format
std::string getISO8601Timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    localtime_r(&now_time_t, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    oss << std::put_time(&tm_buf, "%z");

    return oss.str();
}

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

    printf("[%s] QHY Test CCD using SingleFrameMode, Version: %.2f\n", getISO8601Timestamp().c_str(), VERSION);

    // Get and display SDK version
    uint32_t sdkYear = 0, sdkMonth = 0, sdkDay = 0, sdkSubday = 0;
    int retVal = GetQHYCCDSDKVersion(&sdkYear, &sdkMonth, &sdkDay, &sdkSubday);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] QHYCCD SDK Version: %d.%d.%d.%d\n", getISO8601Timestamp().c_str(), sdkYear, sdkMonth, sdkDay, sdkSubday);
    }
    else
    {
        printf("[%s] Failed to get QHYCCD SDK version\n", getISO8601Timestamp().c_str());
    }

    // init SDK
    retVal = InitQHYCCDResource();
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] SDK resources initialized.\n", getISO8601Timestamp().c_str());
    }
    else
    {
        printf("[%s] Cannot initialize SDK resources, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    // scan cameras
    int camCount = ScanQHYCCD();
    if (camCount > 0)
    {
        printf("[%s] Number of QHYCCD cameras found: %d \n", getISO8601Timestamp().c_str(), camCount);
    }
    else
    {
        printf("[%s] No QHYCCD camera found, please check USB or power.\n", getISO8601Timestamp().c_str());
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
            printf("[%s] Application connected to the following camera from the list: Index: %d,  cameraID = %s\n",
                   getISO8601Timestamp().c_str(), (i + 1), camId);
            camFound = true;
            break;
        }
    }

    if (!camFound)
    {
        printf("[%s] The detected camera is not QHYCCD or other error.\n", getISO8601Timestamp().c_str());
        // release sdk resources
        retVal = ReleaseQHYCCDResource();
        if (QHYCCD_SUCCESS == retVal)
        {
            printf("[%s] SDK resources released.\n", getISO8601Timestamp().c_str());
        }
        else
        {
            printf("[%s] Cannot release SDK resources, error %d.\n", getISO8601Timestamp().c_str(), retVal);
        }
        return 1;
    }

    // open camera
    qhyccd_handle *pCamHandle = OpenQHYCCD(camId);
    if (pCamHandle != nullptr)
    {
        printf("[%s] Open QHYCCD success.\n", getISO8601Timestamp().c_str());
    }
    else
    {
        printf("[%s] Open QHYCCD failure.\n", getISO8601Timestamp().c_str());
        return 1;
    }

    // set single frame mode
    int mode = 0;
    retVal = SetQHYCCDStreamMode(pCamHandle, mode);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] SetQHYCCDStreamMode set to: %d, success.\n", getISO8601Timestamp().c_str(), mode);
    }
    else
    {
        printf("[%s] SetQHYCCDStreamMode: %d failure, error: %d\n", getISO8601Timestamp().c_str(), mode, retVal);
        return 1;
    }

    // Set readout mode
    retVal = SetQHYCCDReadMode(pCamHandle, readoutMode);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] SetQHYCCDReadMode set to: %d, success.\n", getISO8601Timestamp().c_str(), readoutMode);
    }
    else
    {
        printf("[%s] SetQHYCCDReadMode: %d failure, error: %d\n", getISO8601Timestamp().c_str(), readoutMode, retVal);
        return 1;
    }

    // initialize camera
    retVal = InitQHYCCD(pCamHandle);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] InitQHYCCD success.\n", getISO8601Timestamp().c_str());
    }
    else
    {
        printf("[%s] InitQHYCCD faililure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    // Check if filter wheel is connected
    retVal = IsQHYCCDCFWPlugged(pCamHandle);
    if (retVal == QHYCCD_SUCCESS)
    {
        printf("[%s] Filter wheel is connected.\n", getISO8601Timestamp().c_str());
        int filterCount = GetQHYCCDParam(pCamHandle, CONTROL_CFWSLOTSNUM);
        if (filterCount > 0 && filterCount <= 16)
        {
            printf("[%s] Filter wheel has %d positions.\n", getISO8601Timestamp().c_str(), filterCount);

            char currentPos[64] = {0};
            if (GetQHYCCDCFWStatus(pCamHandle, currentPos) == QHYCCD_SUCCESS)
            {
                int position = strtol(currentPos, nullptr, 16) + 1;
                printf("[%s] Current filter position: %d\n", getISO8601Timestamp().c_str(), position);
            }
        }
        else
        {
            printf("[%s] Filter wheel reports invalid number of positions: %d\n", getISO8601Timestamp().c_str(), filterCount);
        }
    }
    else
    {
        printf("[%s] No filter wheel detected.\n", getISO8601Timestamp().c_str());
    }

    // get overscan area
    retVal = GetQHYCCDOverScanArea(pCamHandle, &overscanStartX, &overscanStartY, &overscanSizeX, &overscanSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] GetQHYCCDOverScanArea:\n", getISO8601Timestamp().c_str());
        printf("[%s] Overscan Area startX x startY : %d x %d\n", getISO8601Timestamp().c_str(), overscanStartX, overscanStartY);
        printf("[%s] Overscan Area sizeX  x sizeY  : %d x %d\n", getISO8601Timestamp().c_str(), overscanSizeX, overscanSizeY);
    }
    else
    {
        printf("[%s] GetQHYCCDOverScanArea failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    // get effective area
    retVal = GetQHYCCDOverScanArea(pCamHandle, &effectiveStartX, &effectiveStartY, &effectiveSizeX, &effectiveSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] GetQHYCCDEffectiveArea:\n", getISO8601Timestamp().c_str());
        printf("[%s] Effective Area startX x startY: %d x %d\n", getISO8601Timestamp().c_str(), effectiveStartX, effectiveStartY);
        printf("[%s] Effective Area sizeX  x sizeY : %d x %d\n", getISO8601Timestamp().c_str(), effectiveSizeX, effectiveSizeY);
    }
    else
    {
        printf("[%s] GetQHYCCDOverScanArea failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    // get chip info
    retVal = GetQHYCCDChipInfo(pCamHandle, &chipWidthMM, &chipHeightMM, &maxImageSizeX, &maxImageSizeY, &pixelWidthUM,
                               &pixelHeightUM, &bpp);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] GetQHYCCDChipInfo:\n", getISO8601Timestamp().c_str());
        printf("[%s] Effective Area startX x startY: %d x %d\n", getISO8601Timestamp().c_str(), effectiveStartX, effectiveStartY);
        printf("[%s] Chip  size width x height     : %.3f x %.3f [mm]\n", getISO8601Timestamp().c_str(), chipWidthMM, chipHeightMM);
        printf("[%s] Pixel size width x height     : %.3f x %.3f [um]\n", getISO8601Timestamp().c_str(), pixelWidthUM,
               pixelHeightUM);
        printf("[%s] Image size width x height     : %d x %d\n", getISO8601Timestamp().c_str(), maxImageSizeX, maxImageSizeY);
    }
    else
    {
        printf("[%s] GetQHYCCDChipInfo failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
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
        printf("[%s] This is a color camera.\n", getISO8601Timestamp().c_str());
        SetQHYCCDDebayerOnOff(pCamHandle, true);
        SetQHYCCDParam(pCamHandle, CONTROL_WBR, 20);
        SetQHYCCDParam(pCamHandle, CONTROL_WBG, 20);
        SetQHYCCDParam(pCamHandle, CONTROL_WBB, 20);
    }
    else
    {
        printf("[%s] This is a mono camera.\n", getISO8601Timestamp().c_str());
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
                printf("[%s] SetQHYCCDParam CONTROL_USBTRAFFIC set to: %d, success.\n", getISO8601Timestamp().c_str(), USB_TRAFFIC);
            }
            else
            {
                printf("[%s] SetQHYCCDParam CONTROL_USBTRAFFIC failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
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
                printf("[%s] SetQHYCCDParam CONTROL_GAIN set to: %d, success\n", getISO8601Timestamp().c_str(), CHIP_GAIN);
            }
            else
            {
                printf("[%s] SetQHYCCDParam CONTROL_GAIN failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
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
                printf("[%s] SetQHYCCDParam CONTROL_OFFSET set to: %d, success.\n", getISO8601Timestamp().c_str(), CHIP_OFFSET);
            }
            else
            {
                printf("[%s] SetQHYCCDParam CONTROL_OFFSET failed.\n", getISO8601Timestamp().c_str());
                getchar();
                return 1;
            }
        }
    }

    // check read mode in QHY42
    char *modeName = (char *)malloc((200) * sizeof(char));
    if (modeName == NULL)
    {
        printf("[%s] Failed to allocate memory for modeName.\n", getISO8601Timestamp().c_str());
        return 1;
    }

    retVal = GetQHYCCDReadModeName(pCamHandle, readoutMode, modeName);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] Selected read mode: %d, name: %s \n", getISO8601Timestamp().c_str(), readoutMode, modeName);
    }
    else
    {
        printf("[%s] Error reading name for selected mode %d \n", getISO8601Timestamp().c_str(), readoutMode);
        free(modeName);
        return 1;
    }

    // Set read modes and read resolution for each one (for informational purposes)
    uint32_t numberOfReadModes = 0;
    uint32_t imageRMw, imageRMh;
    retVal = GetQHYCCDNumberOfReadModes(pCamHandle, &numberOfReadModes);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] Available Read Modes:\n", getISO8601Timestamp().c_str());
        for(uint32_t i = 0; i < numberOfReadModes; i++)
        {
            retVal = GetQHYCCDReadModeName(pCamHandle, i, modeName);
            if (QHYCCD_SUCCESS == retVal)
            {
                retVal = GetQHYCCDReadModeResolution(pCamHandle, i, &imageRMw, &imageRMh);
                printf("[%s]   Mode %d: %s, Resolution: %d x %d\n", getISO8601Timestamp().c_str(), i, modeName, imageRMw, imageRMh);
            }
        }
    }
    else
    {
        printf("[%s] Error getting number of read modes.\n", getISO8601Timestamp().c_str());
        free(modeName);
        return 1;
    }
    free(modeName);


    // set exposure time
    retVal = SetQHYCCDParam(pCamHandle, CONTROL_EXPOSURE, EXPOSURE_TIME);
    printf("[%s] SetQHYCCDParam CONTROL_EXPOSURE set to: %d, success.\n", getISO8601Timestamp().c_str(), EXPOSURE_TIME);
    if (QHYCCD_SUCCESS == retVal)
    {
    }
    else
    {
        printf("[%s] SetQHYCCDParam CONTROL_EXPOSURE failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        getchar();
        return 1;
    }

    // set image resolution
    retVal = SetQHYCCDResolution(pCamHandle, roiStartX, roiStartY, roiSizeX, roiSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] SetQHYCCDResolution roiStartX x roiStartY: %d x %d\n", getISO8601Timestamp().c_str(), roiStartX, roiStartY);
        printf("[%s] SetQHYCCDResolution roiSizeX  x roiSizeY : %d x %d\n", getISO8601Timestamp().c_str(), roiSizeX, roiSizeY);
    }
    else
    {
        printf("[%s] SetQHYCCDResolution failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    // set binning mode
    retVal = SetQHYCCDBinMode(pCamHandle, camBinX, camBinY);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] SetQHYCCDBinMode set to: binX: %d, binY: %d, success.\n", getISO8601Timestamp().c_str(), camBinX, camBinY);
    }
    else
    {
        printf("[%s] SetQHYCCDBinMode failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    // set bit resolution
    retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_TRANSFERBIT);
    if (QHYCCD_SUCCESS == retVal)
    {
        retVal = SetQHYCCDBitsMode(pCamHandle, 16);
        if (QHYCCD_SUCCESS == retVal)
        {
            printf("[%s] SetQHYCCDParam CONTROL_GAIN set to: %d, success.\n", getISO8601Timestamp().c_str(), CONTROL_TRANSFERBIT);
        }
        else
        {
            printf("[%s] SetQHYCCDParam CONTROL_GAIN failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
            getchar();
            return 1;
        }
    }

    // single frame
    printf("[%s] ExpQHYCCDSingleFrame(pCamHandle) - start...\n", getISO8601Timestamp().c_str());
    retVal = ExpQHYCCDSingleFrame(pCamHandle);
    printf("[%s] ExpQHYCCDSingleFrame(pCamHandle) - end...\n", getISO8601Timestamp().c_str());
    if (QHYCCD_ERROR != (uint32_t) retVal)
    {
        printf("[%s] ExpQHYCCDSingleFrame success (%d).\n", getISO8601Timestamp().c_str(), retVal);
        if (QHYCCD_READ_DIRECTLY != retVal)
        {
            sleep(1);
        }
    }
    else
    {
        printf("[%s] ExpQHYCCDSingleFrame failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    // get requested memory lenght
    uint32_t length = GetQHYCCDMemLength(pCamHandle);

    // For color cameras with debayering enabled, multiply by 3 for RGB channels
    retVal = IsQHYCCDControlAvailable(pCamHandle, CAM_COLOR);
    if (retVal == BAYER_GB || retVal == BAYER_GR || retVal == BAYER_BG || retVal == BAYER_RG)
    {
        length *= 3;  // RGB needs 3× the space
        printf("[%s] Color camera detected, allocating 3x buffer for RGB output.\n", getISO8601Timestamp().c_str());
    }

    if (length > 0)
    {
        pImgData = new unsigned char[length];
        memset(pImgData, 0, length);
        printf("[%s] Allocated memory for frame: %d [uchar].\n", getISO8601Timestamp().c_str(), length);
    }
    else
    {
        printf("[%s] Cannot allocate memory for frame.\n", getISO8601Timestamp().c_str());
        return 1;
    }

    // get single frame
    retVal = GetQHYCCDSingleFrame(pCamHandle, &roiSizeX, &roiSizeY, &bpp, &channels, pImgData);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] GetQHYCCDSingleFrame: %d x %d, bpp: %d, channels: %d, success.\n", getISO8601Timestamp().c_str(), roiSizeX,
               roiSizeY, bpp, channels);
        //process image here
    }
    else
    {
        printf("[%s] GetQHYCCDSingleFrame failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
    }

    delete [] pImgData;

    retVal = CancelQHYCCDExposingAndReadout(pCamHandle);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] CancelQHYCCDExposingAndReadout success.\n", getISO8601Timestamp().c_str());
    }
    else
    {
        printf("[%s] CancelQHYCCDExposingAndReadout failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    // close camera handle
    retVal = CloseQHYCCD(pCamHandle);
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] Close QHYCCD success.\n", getISO8601Timestamp().c_str());
    }
    else
    {
        printf("[%s] Close QHYCCD failure, error: %d\n", getISO8601Timestamp().c_str(), retVal);
    }

    // release sdk resources
    retVal = ReleaseQHYCCDResource();
    if (QHYCCD_SUCCESS == retVal)
    {
        printf("[%s] SDK resources released.\n", getISO8601Timestamp().c_str());
    }
    else
    {
        printf("[%s] Cannot release SDK resources, error %d.\n", getISO8601Timestamp().c_str(), retVal);
        return 1;
    }

    return 0;
}
