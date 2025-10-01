#include <libflipro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fitsio.h>

#define FLI_MAX_SUPPORTED_CAMERAS 4

static void save_merged_fits_file(const char *fileName, void *buffer, int width, int height, int bpp)
{
    fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
    int status = 0;       /* CFITSIO status value MUST be initialized to zero! */
    long naxes[2];
    int bitpix;
    long fpixel = 1;
    long nelements;

    // Determine BITPIX based on bits per pixel
    switch (bpp)
    {
        case 8:
            bitpix = BYTE_IMG;
            break;
        case 16:
            bitpix = USHORT_IMG;
            break;
        case 32:
            bitpix = ULONG_IMG;
            break;
        default:
            fprintf(stderr, "Error: Unsupported bits per pixel: %d\n", bpp);
            return;
    }

    naxes[0] = width;
    naxes[1] = height;
    nelements = naxes[0] * naxes[1];

    /* create new FITS file */
    fits_create_file(&fptr, fileName, &status);
    if (status)
    {
        fits_report_error(stderr, status);
        return;
    }

    /* create the primary array image (16-bit short integer pixels) */
    fits_create_img(fptr, bitpix, 2, naxes, &status);
    if (status)
    {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return;
    }

    /* Write a minimal FITS header */
    fits_update_key(fptr, TSTRING, "COMMENT", (char*)"Created by indi-fli test_kepler_merged.cpp", "Minimal FITS header",
                    &status);
    fits_update_key(fptr, TSTRING, "EXTEND", (char*)"T", "FITS file may contain extensions", &status);

    /* Write the image data */
    fits_write_img(fptr, bitpix, fpixel, nelements, buffer, &status);
    if (status)
    {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return;
    }

    /* close the FITS file */
    fits_close_file(fptr, &status);
    if (status)
    {
        fits_report_error(stderr, status);
        return;
    }

    printf("Saved merged FITS image to %s\n", fileName);
}

#include <getopt.h> // Required for command-line argument parsing

int main(int argc, char *argv[])
{
    bool captureLowGain = false;
    bool captureHighGain = false;
    bool captureMerged = true; // Default to merged

    int opt;
    static struct option long_options[] =
    {
        {"low", no_argument, 0, 'l'},
        {"high", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "lh", long_options, NULL)) != -1)
    {
        switch (opt)
        {
            case 'l':
                captureLowGain = true;
                captureHighGain = false;
                captureMerged = false;
                break;
            case 'h':
                captureHighGain = true;
                captureLowGain = false;
                captureMerged = false;
                break;
            default:
                fprintf(stderr, "Usage: %s [--low] [--high]\n", argv[0]);
                return 1;
        }
    }

    FPRODEVICEINFO camerasDeviceInfo[FLI_MAX_SUPPORTED_CAMERAS];
    uint32_t detectedCamerasCount = FLI_MAX_SUPPORTED_CAMERAS;
    int32_t result = FPROCam_GetCameraList(camerasDeviceInfo, &detectedCamerasCount);

    if (result < 0 || detectedCamerasCount == 0)
    {
        printf("No FLI cameras found.\n");
        return -1;
    }

    printf("Found %u FLI cameras.\n", detectedCamerasCount);

    int32_t cameraHandle = -1;
    result = FPROCam_Open(&camerasDeviceInfo[0], &cameraHandle);
    if (result < 0)
    {
        printf("Failed to open camera.\n");
        return -1;
    }

    printf("Camera opened successfully.\n");

    uint32_t caps[(uint32_t)FPROCAPS::FPROCAP_NUM];
    uint32_t numCaps = (uint32_t)FPROCAPS::FPROCAP_NUM;
    result = FPROSensor_GetCapabilityList(cameraHandle, caps, &numCaps);
    if (result < 0)
    {
        printf("Failed to get capability list.\n");
        FPROCam_Close(cameraHandle);
        return -1;
    }

    printf("Camera Capabilities:\n");
    printf("  Max Width: %u\n", caps[(uint32_t)FPROCAPS::FPROCAP_MAX_PIXEL_WIDTH]);
    printf("  Max Height: %u\n", caps[(uint32_t)FPROCAPS::FPROCAP_MAX_PIXEL_HEIGHT]);
    printf("  Low Gain Table Size: %u\n", caps[(uint32_t)FPROCAPS::FPROCAP_LOW_GAIN_TABLE_SIZE]);
    printf("  High Gain Table Size: %u\n", caps[(uint32_t)FPROCAPS::FPROCAP_HIGH_GAIN_TABLE_SIZE]);

    FPRO_REFFRAMES refFrames;
    memset(&refFrames, 0, sizeof(refFrames));
    refFrames.uiWidth = caps[(uint32_t)FPROCAPS::FPROCAP_MAX_PIXEL_WIDTH];
    refFrames.uiHeight = caps[(uint32_t)FPROCAPS::FPROCAP_MAX_PIXEL_HEIGHT];
    result = FPROAlgo_SetHardwareMergeReferenceFrames(cameraHandle, &refFrames);
    if (result < 0)
    {
        printf("Failed to set hardware merge reference frames.\n");
        FPROCam_Close(cameraHandle);
        return -1;
    }

    result = FPROFrame_SetImageArea(cameraHandle, 0, 0, caps[(uint32_t)FPROCAPS::FPROCAP_MAX_PIXEL_WIDTH],
                                    caps[(uint32_t)FPROCAPS::FPROCAP_MAX_PIXEL_HEIGHT]);
    if (result < 0)
    {
        printf("Failed to set image area.\n");
        FPROCam_Close(cameraHandle);
        return -1;
    }

    // Set 1 second exposure
    result = FPROCtrl_SetExposure(cameraHandle, 1e9, 0, false);
    if (result < 0)
    {
        printf("Failed to set exposure.\n");
        FPROCam_Close(cameraHandle);
        return -1;
    }

    printf("Exposure set to 1 second.\n");

    uint32_t frameSize = FPROFrame_ComputeFrameSize(cameraHandle);
    uint8_t *frameBuffer = (uint8_t *)malloc(frameSize);
    if (!frameBuffer)
    {
        printf("Failed to allocate frame buffer.\n");
        FPROCam_Close(cameraHandle);
        return -1;
    }

    FPROUNPACKEDIMAGES fproUnpacked;
    memset(&fproUnpacked, 0, sizeof(fproUnpacked));
    fproUnpacked.bMetaDataRequest = true;
    fproUnpacked.eMergeFormat = FPRO_IMAGE_FORMAT::IFORMAT_FITS;

    if (captureLowGain)
    {
        fproUnpacked.bLowImageRequest = true;
        fproUnpacked.bHighImageRequest = false;
        fproUnpacked.bMergedImageRequest = false;
    }
    else if (captureHighGain)
    {
        fproUnpacked.bLowImageRequest = false;
        fproUnpacked.bHighImageRequest = true;
        fproUnpacked.bMergedImageRequest = false;
    }
    else // captureMerged is true by default
    {
        fproUnpacked.bLowImageRequest = false;
        fproUnpacked.bHighImageRequest = false;
        fproUnpacked.bMergedImageRequest = true;
    }

    result = FPROFrame_CaptureStart(cameraHandle, 1);
    if (result < 0)
    {
        printf("Failed to start capture.\n");
        free(frameBuffer);
        FPROCam_Close(cameraHandle);
        return -1;
    }

    printf("Capture started, waiting for image...\n");

    result = FPROFrame_GetVideoFrameUnpacked(cameraHandle, frameBuffer, &frameSize, 2000, &fproUnpacked, nullptr);
    FPROFrame_CaptureAbort(cameraHandle);

    if (result >= 0)
    {
        printf("Image received.\n");
        uint32_t imageWidth = caps[(uint32_t)FPROCAPS::FPROCAP_MAX_PIXEL_WIDTH];
        uint32_t imageHeight = caps[(uint32_t)FPROCAPS::FPROCAP_MAX_PIXEL_HEIGHT];
        uint32_t bitsPerPixel = 16;

        if (captureLowGain && fproUnpacked.pLowImage)
        {
            save_merged_fits_file("low_gain_image.fits", fproUnpacked.pLowImage, imageWidth, imageHeight, bitsPerPixel);
        }
        else if (captureHighGain && fproUnpacked.pHighImage)
        {
            save_merged_fits_file("high_gain_image.fits", fproUnpacked.pHighImage, imageWidth, imageHeight, bitsPerPixel);
        }
        else if (captureMerged && fproUnpacked.pMergedImage)
        {
            save_merged_fits_file("merged_image.fits", fproUnpacked.pMergedImage, imageWidth, imageHeight, bitsPerPixel);
        }
        else
        {
            printf("Requested image buffer is null.\n");
        }
    }
    else
    {
        printf("Failed to get frame: %d\n", result);
    }

    FPROFrame_FreeUnpackedBuffers(&fproUnpacked);
    free(frameBuffer);
    FPROCam_Close(cameraHandle);

    printf("Camera closed.\n");

    return 0;
}
