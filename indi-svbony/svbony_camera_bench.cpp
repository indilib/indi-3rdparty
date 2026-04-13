/*
 SVBony Camera Benchmark

 Measures frame capture performance across different binning modes (1x1, 2x2, 4x4).
 No frames are written to disk — this is a pure capture latency / throughput benchmark.

 Usage:
   ./svbony_camera_bench [--camera <index>] [--exposure <ms>] [--bin <1|2|4>] [--frames <N>]

 Options:
   --camera   <index>   Camera index to use (default: 0)
   --exposure <ms>      Exposure time in milliseconds (default: 1000)
   --bin      <n>       Binning mode: 1, 2, or 4. Omit (or 0) to benchmark all three (default: 0)
   --frames   <N>       Number of frames to capture per binning mode (default: 5)

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

#include <SVBCameraSDK.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void printUsage(const char *prog)
{
    printf("Usage: %s [--camera <index>] [--exposure <ms>] [--bin <1|2|4>] [--frames <N>]\n\n", prog);
    printf("  --camera   <index>  Camera index (default: 0)\n");
    printf("  --exposure <ms>     Exposure time in milliseconds (default: 1000)\n");
    printf("  --bin      <n>      Binning: 1, 2, or 4. Use 0 to run all three (default: 0)\n");
    printf("  --frames   <N>      Frames per binning mode (default: 5)\n");
}

static double mean(const std::vector<double> &v)
{
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

static double stddev(const std::vector<double> &v)
{
    double m = mean(v);
    double sq = 0.0;
    for (double x : v)
        sq += (x - m) * (x - m);
    return std::sqrt(sq / static_cast<double>(v.size()));
}

// ---------------------------------------------------------------------------
// Capture one soft-triggered frame; returns elapsed ms or -1.0 on error.
// Call after SVBStartVideoCapture + SVBSetControlValue(SVB_EXPOSURE) are set.
// ---------------------------------------------------------------------------
static double captureFrame(int camID, unsigned char *buf, long bufSize, int exposureMs)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    // Trigger the exposure
    SVB_ERROR_CODE ret = SVBSendSoftTrigger(camID);
    if (ret != SVB_SUCCESS)
    {
        fprintf(stderr, "  SVBSendSoftTrigger failed (%d)\n", static_cast<int>(ret));
        return -1.0;
    }

    // Poll for image data; use a generous timeout per attempt
    const int waitPerCallMs = 1000;
    const int maxRetries    = 60; // up to 60 s for very long exposures
    int retry = maxRetries;
    while (retry--)
    {
        ret = SVBGetVideoData(camID, buf, bufSize, waitPerCallMs);
        if (ret == SVB_SUCCESS)
            break;
        if (ret != SVB_ERROR_TIMEOUT)
        {
            fprintf(stderr, "  SVBGetVideoData error (%d)\n", static_cast<int>(ret));
            return -1.0;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    if (ret != SVB_SUCCESS)
    {
        fprintf(stderr, "  SVBGetVideoData timed out after %d s\n", maxRetries);
        return -1.0;
    }

    (void)exposureMs; // only used externally for overhead calculation
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// ---------------------------------------------------------------------------
// Benchmark a single binning mode.
// Returns a result struct; result.ok == false on any camera error.
// ---------------------------------------------------------------------------
struct BinResult
{
    int    bin        {0};
    int    width      {0};
    int    height     {0};
    double meanMs     {0.0};
    double minMs      {0.0};
    double maxMs      {0.0};
    double stddevMs   {0.0};
    double fps        {0.0};
    double overheadMs {0.0}; // mean total - exposure time
    bool   ok         {false};
};

static BinResult benchmarkBin(int camID, int bin, int exposureMs, int numFrames,
                              int maxWidth, int maxHeight)
{
    BinResult result;
    result.bin = bin;

    // SVBony ROI rules: width % 8 == 0, height % 2 == 0
    int roiWidth  = (maxWidth  / bin) & ~7; // round down to multiple of 8
    int roiHeight = (maxHeight / bin) & ~1; // round down to multiple of 2

    printf("\n--- Binning %dx%d  (%d x %d px) ---\n", bin, bin, roiWidth, roiHeight);

    // Set ROI
    SVB_ERROR_CODE ret = SVBSetROIFormat(camID, 0, 0, roiWidth, roiHeight, bin);
    if (ret != SVB_SUCCESS)
    {
        fprintf(stderr, "  ERROR: SVBSetROIFormat(%d, %d, bin=%d) failed (%d)\n",
                roiWidth, roiHeight, bin, static_cast<int>(ret));
        return result;
    }

    // Set 16-bit output format
    ret = SVBSetOutputImageType(camID, SVB_IMG_RAW16);
    if (ret != SVB_SUCCESS)
    {
        fprintf(stderr, "  ERROR: SVBSetOutputImageType(RAW16) failed (%d)\n", static_cast<int>(ret));
        return result;
    }

    // Read back actual ROI dimensions
    int actualX = 0, actualY = 0, actualW = 0, actualH = 0, actualBin = bin;
    SVBGetROIFormat(camID, &actualX, &actualY, &actualW, &actualH, &actualBin);
    result.width  = actualW;
    result.height = actualH;

    // Set exposure (microseconds)
    ret = SVBSetControlValue(camID, SVB_EXPOSURE, static_cast<long>(exposureMs) * 1000L, SVB_FALSE);
    if (ret != SVB_SUCCESS)
    {
        fprintf(stderr, "  ERROR: SVBSetControlValue(SVB_EXPOSURE) failed (%d)\n", static_cast<int>(ret));
        return result;
    }

    // Enter soft-trigger mode and start video capture for this binning run
    SVBSetCameraMode(camID, SVB_MODE_TRIG_SOFT);
    ret = SVBStartVideoCapture(camID);
    if (ret != SVB_SUCCESS)
    {
        fprintf(stderr, "  ERROR: SVBStartVideoCapture failed (%d)\n", static_cast<int>(ret));
        return result;
    }

    // Allocate frame buffer (RAW16 = 2 bytes per pixel)
    long imgSize = static_cast<long>(result.width) * result.height * 2;
    std::vector<unsigned char> imgBuf(imgSize);

    // Warm-up frame (not measured)
    printf("  Warm-up frame... ");
    fflush(stdout);
    captureFrame(camID, imgBuf.data(), imgSize, exposureMs); // ignore error/time
    printf("done\n");

    // Timed frames
    std::vector<double> timingsMs;
    timingsMs.reserve(numFrames);

    for (int f = 0; f < numFrames; ++f)
    {
        double elapsed = captureFrame(camID, imgBuf.data(), imgSize, exposureMs);
        if (elapsed < 0.0)
        {
            SVBStopVideoCapture(camID);
            return result; // error already printed
        }

        timingsMs.push_back(elapsed);

        printf("  Frame %2d/%d : %8.2f ms  (overhead %+.2f ms)\n",
               f + 1, numFrames, elapsed, elapsed - static_cast<double>(exposureMs));
    }

    SVBStopVideoCapture(camID);

    // Statistics
    result.meanMs     = mean(timingsMs);
    result.minMs      = *std::min_element(timingsMs.begin(), timingsMs.end());
    result.maxMs      = *std::max_element(timingsMs.begin(), timingsMs.end());
    result.stddevMs   = stddev(timingsMs);
    result.fps        = 1000.0 / result.meanMs;
    result.overheadMs = result.meanMs - static_cast<double>(exposureMs);
    result.ok         = true;

    printf("  → mean=%.2f ms  min=%.2f ms  max=%.2f ms  stddev=%.2f ms  FPS=%.4f  overhead=%.2f ms\n",
           result.meanMs, result.minMs, result.maxMs, result.stddevMs, result.fps, result.overheadMs);

    return result;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // --- Parse arguments ---
    int camIndex   = 0;
    int exposureMs = 1000;
    int binMode    = 0;  // 0 = run all
    int numFrames  = 5;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
        else if (std::strcmp(argv[i], "--camera") == 0 && i + 1 < argc)
            camIndex = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--exposure") == 0 && i + 1 < argc)
            exposureMs = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--bin") == 0 && i + 1 < argc)
            binMode = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            numFrames = std::atoi(argv[++i]);
        else
        {
            fprintf(stderr, "Unknown argument: %s\n\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    if (exposureMs <= 0)
    {
        fprintf(stderr, "Exposure must be > 0 ms\n");
        return 1;
    }
    if (numFrames <= 0)
    {
        fprintf(stderr, "Frame count must be > 0\n");
        return 1;
    }
    if (binMode != 0 && binMode != 1 && binMode != 2 && binMode != 4)
    {
        fprintf(stderr, "Binning must be 0 (all), 1, 2, or 4\n");
        return 1;
    }

    // --- Enumerate cameras ---
    int numDevices = SVBGetNumOfConnectedCameras();
    if (numDevices <= 0)
    {
        printf("No SVBony cameras detected.\n");
        printf("SVBony Camera Benchmark failed.\n");
        return -1;
    }

    printf("=== SVBony Camera Benchmark ===\n\n");
    printf("SDK version   : %s\n", SVBGetSDKVersion());
    printf("\nDetected cameras:\n");

    SVB_CAMERA_INFO camInfo;
    for (int i = 0; i < numDevices; ++i)
    {
        SVBGetCameraInfo(&camInfo, i);
        printf("  [%d] %s (ID=%d)\n", i, camInfo.FriendlyName, camInfo.CameraID);
    }

    if (camIndex < 0 || camIndex >= numDevices)
    {
        fprintf(stderr, "\nInvalid camera index %d (0..%d available)\n", camIndex, numDevices - 1);
        return -1;
    }

    SVBGetCameraInfo(&camInfo, camIndex);
    int camID = camInfo.CameraID;
    printf("\nUsing camera [%d] %s (ID=%d)\n", camIndex, camInfo.FriendlyName, camID);

    // --- Open & init camera ---
    SVB_ERROR_CODE ret = SVBOpenCamera(camID);
    if (ret != SVB_SUCCESS)
    {
        fprintf(stderr, "SVBOpenCamera failed (%d). Are you root / do you have USB permissions?\n",
                static_cast<int>(ret));
        return -1;
    }

    SVBRestoreDefaultParam(camID);
    SVBSetAutoSaveParam(camID, SVB_FALSE);

    // Get camera properties
    SVB_CAMERA_PROPERTY camProp;
    ret = SVBGetCameraProperty(camID, &camProp);
    if (ret != SVB_SUCCESS)
    {
        fprintf(stderr, "SVBGetCameraProperty failed (%d)\n", static_cast<int>(ret));
        SVBCloseCamera(camID);
        return -1;
    }

    printf("Max resolution: %ld x %ld\n", camProp.MaxWidth, camProp.MaxHeight);
    printf("Max bit depth : %d\n", camProp.MaxBitDepth);

    // Pixel size
    float pixelSize = 0.0f;
    SVBGetSensorPixelSize(camID, &pixelSize);
    if (pixelSize > 0.0f)
        printf("Pixel size    : %.2f µm\n", pixelSize);

    if (camProp.IsColorCam)
    {
        const char *bayer[] = {"RG", "BG", "GR", "GB"};
        int bi = static_cast<int>(camProp.BayerPattern);
        printf("Color camera  : bayer pattern %s\n", (bi >= 0 && bi < 4) ? bayer[bi] : "?");
    }
    else
    {
        printf("Mono camera\n");
    }

    // Temperature
    {
        long temp = 0;
        SVB_BOOL isAuto = SVB_FALSE;
        if (SVBGetControlValue(camID, SVB_CURRENT_TEMPERATURE, &temp, &isAuto) == SVB_SUCCESS)
            printf("Sensor temp   : %.1f °C\n", temp / 10.0);
    }

    printf("Exposure      : %d ms\n", exposureMs);
    printf("Frames/mode   : %d\n", numFrames);

    // --- Base settings: gain = 0, low frame speed ---
    SVBSetControlValue(camID, SVB_GAIN, 0, SVB_FALSE);
    // SVB_FRAME_SPEED_MODE: 0=low, 1=medium, 2=high
    SVBSetControlValue(camID, SVB_FRAME_SPEED_MODE, 0, SVB_FALSE);

    // --- Determine which binning modes to run ---
    std::vector<int> bins;
    if (binMode == 0)
        bins = {1, 2, 4};
    else
        bins = {binMode};

    // Filter against supported binning modes (zero-terminated array)
    {
        std::vector<int> supportedBins;
        for (int b : bins)
        {
            bool found = false;
            for (int k = 0; k < 16 && camProp.SupportedBins[k] != 0; ++k)
            {
                if (camProp.SupportedBins[k] == b)
                {
                    found = true;
                    break;
                }
            }
            if (found)
                supportedBins.push_back(b);
            else
                printf("  Binning %dx%d not supported by this camera — skipping.\n", b, b);
        }
        if (supportedBins.empty())
        {
            fprintf(stderr, "No requested binning mode is supported by this camera.\n");
            SVBCloseCamera(camID);
            return -1;
        }
        bins = supportedBins;
    }

    // --- Run benchmarks ---
    std::vector<BinResult> results;
    for (int b : bins)
    {
        BinResult r = benchmarkBin(camID, b, exposureMs, numFrames,
                                   camProp.MaxWidth, camProp.MaxHeight);
        results.push_back(r);
        if (!r.ok)
        {
            fprintf(stderr, "\nBenchmark aborted due to error on binning %dx%d.\n", b, b);
            break;
        }
    }

    // --- Summary table ---
    printf("\n");
    printf("=== Summary ===\n");
    printf("Camera   : %s\n", camInfo.FriendlyName);
    printf("Exposure : %d ms\n", exposureMs);
    printf("Frames   : %d per mode\n\n", numFrames);

    printf("%-6s  %-15s  %10s  %10s  %10s  %10s  %8s  %12s\n",
           "Bin", "Resolution", "Mean(ms)", "Min(ms)", "Max(ms)", "Stddev(ms)", "FPS", "Overhead(ms)");
    printf("%-6s  %-15s  %10s  %10s  %10s  %10s  %8s  %12s\n",
           "------", "---------------", "----------", "----------", "----------", "----------", "--------", "------------");

    for (const BinResult &r : results)
    {
        if (!r.ok)
        {
            printf("%-6s  %-15s  %s\n",
                   (std::to_string(r.bin) + "x" + std::to_string(r.bin)).c_str(),
                   "N/A", "FAILED");
            continue;
        }
        char binLabel[16];
        snprintf(binLabel, sizeof(binLabel), "%dx%d", r.bin, r.bin);
        char resolution[32];
        snprintf(resolution, sizeof(resolution), "%d x %d", r.width, r.height);

        printf("%-6s  %-15s  %10.2f  %10.2f  %10.2f  %10.2f  %8.4f  %12.2f\n",
               binLabel, resolution,
               r.meanMs, r.minMs, r.maxMs, r.stddevMs,
               r.fps, r.overheadMs);
    }
    printf("\n");

    // --- Clean up ---
    SVBCloseCamera(camID);
    printf("SVBony Camera Benchmark completed.\n");
    return 0;
}
