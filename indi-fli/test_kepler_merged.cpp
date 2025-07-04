#include <libflipro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FLI_MAX_SUPPORTED_CAMERAS 4

static void save_merged_file(const char *fileName, void *buffer, size_t size)
{
    FILE *fp = fopen(fileName, "wb");
    if (fp)
    {
        fwrite(buffer, 1, size, fp);
        fclose(fp);
        printf("Saved merged image to %s\n", fileName);
    }
    else
    {
        perror("Error saving file");
    }
}

int main()
{
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
    fproUnpacked.bLowImageRequest = true;
    fproUnpacked.bHighImageRequest = true;
    fproUnpacked.bMergedImageRequest = true;
    fproUnpacked.eMergeFormat = FPRO_IMAGE_FORMAT::IFORMAT_FITS;

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
        if (fproUnpacked.pMergedImage)
        {
            save_merged_file("merged_image.fits", fproUnpacked.pMergedImage, fproUnpacked.uiMergedBufferSize);
        }
        else
        {
            printf("Merged image buffer is null.\n");
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
