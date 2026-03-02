/*
 ASI Camera Benchmark

 Measures frame capture performance across different binning modes (1x1, 2x2, 4x4).
 No frames are written to disk — this is a pure capture latency / throughput benchmark.

 Usage:
   ./asi_camera_bench [--camera <index>] [--exposure <ms>] [--bin <1|2|4>] [--frames <N>]

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

#include <ASICamera2.h>

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

static BinResult benchmarkBin(int camIndex, int bin, int exposureMs, int numFrames, int maxWidth, int maxHeight)
{
    BinResult result;
    result.bin = bin;

    // Compute image dimensions for this binning (align to even number)
    int roiWidth  = (maxWidth  / bin) & ~1;
    int roiHeight = (maxHeight / bin) & ~1;

    printf("\n--- Binning %dx%d  (%d x %d px) ---\n", bin, bin, roiWidth, roiHeight);

    // Configure ROI and format (RAW16 = 2 bytes/pixel)
    if (ASISetROIFormat(camIndex, roiWidth, roiHeight, bin, ASI_IMG_RAW16) != ASI_SUCCESS)
    {
        fprintf(stderr, "  ERROR: ASISetROIFormat(%d, %d, %d, RAW16) failed\n", roiWidth, roiHeight, bin);
        return result;
    }

    // Read back actual size (camera may round values)
    int actualWidth = 0, actualHeight = 0, actualBin = bin;
    ASI_IMG_TYPE actualFormat = ASI_IMG_RAW16;
    ASIGetROIFormat(camIndex, &actualWidth, &actualHeight, &actualBin, &actualFormat);
    result.width  = actualWidth;
    result.height = actualHeight;

    // Set exposure (microseconds)
    ASISetControlValue(camIndex, ASI_EXPOSURE, static_cast<long>(exposureMs) * 1000L, ASI_FALSE);

    // Allocate frame buffer (RAW16 = 2 bytes per pixel)
    long imgSize = static_cast<long>(actualWidth) * actualHeight * 2;
    std::vector<unsigned char> imgBuf(imgSize);

    // Warm-up frame (not measured)
    printf("  Warm-up frame... ");
    fflush(stdout);
    {
        ASI_EXPOSURE_STATUS status = ASI_EXP_WORKING;
        ASIStartExposure(camIndex, ASI_FALSE);
        while (status == ASI_EXP_WORKING)
            ASIGetExpStatus(camIndex, &status);

        if (status == ASI_EXP_SUCCESS)
            ASIGetDataAfterExp(camIndex, imgBuf.data(), imgSize);
        else
            ASIStopExposure(camIndex);
    }
    printf("done\n");

    // Timed frames
    std::vector<double> timingsMs;
    timingsMs.reserve(numFrames);

    for (int f = 0; f < numFrames; ++f)
    {
        auto t0 = std::chrono::high_resolution_clock::now();

        ASI_EXPOSURE_STATUS status = ASI_EXP_WORKING;
        ASIStartExposure(camIndex, ASI_FALSE);
        while (status == ASI_EXP_WORKING)
            ASIGetExpStatus(camIndex, &status);

        auto t1 = std::chrono::high_resolution_clock::now();

        if (status != ASI_EXP_SUCCESS)
        {
            fprintf(stderr, "  Frame %d: exposure failed (status %d)\n", f + 1, static_cast<int>(status));
            ASIStopExposure(camIndex);
            return result;
        }

        ASI_ERROR_CODE err = ASIGetDataAfterExp(camIndex, imgBuf.data(), imgSize);
        if (err != ASI_SUCCESS)
        {
            fprintf(stderr, "  Frame %d: ASIGetDataAfterExp error %d\n", f + 1, static_cast<int>(err));
            return result;
        }

        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();
        timingsMs.push_back(elapsed);

        printf("  Frame %2d/%d : %8.2f ms  (overhead %+.2f ms)\n",
               f + 1, numFrames, elapsed, elapsed - static_cast<double>(exposureMs));
    }

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
    int numDevices = ASIGetNumOfConnectedCameras();
    if (numDevices <= 0)
    {
        printf("No ASI cameras detected.\n");
        printf("ASI Camera Benchmark failed.\n");
        return -1;
    }

    printf("=== ASI Camera Benchmark ===\n\n");
    printf("Detected cameras:\n");

    ASI_CAMERA_INFO camInfo;
    for (int i = 0; i < numDevices; ++i)
    {
        ASIGetCameraProperty(&camInfo, i);
        printf("  [%d] %s\n", i, camInfo.Name);
    }

    if (camIndex < 0 || camIndex >= numDevices)
    {
        fprintf(stderr, "\nInvalid camera index %d (0..%d available)\n", camIndex, numDevices - 1);
        return -1;
    }

    ASIGetCameraProperty(&camInfo, camIndex);
    printf("\nUsing camera [%d] %s\n", camIndex, camInfo.Name);
    printf("Max resolution: %u x %u\n", camInfo.MaxWidth, camInfo.MaxHeight);

    if (camInfo.IsColorCam)
    {
        const char *bayer[] = {"RG", "BG", "GR", "GB"};
        printf("Color camera  : bayer pattern %s\n", bayer[camInfo.BayerPattern]);
    }
    else
    {
        printf("Mono camera\n");
    }

    printf("Exposure      : %d ms\n", exposureMs);
    printf("Frames/mode   : %d\n", numFrames);

    // --- Open & init camera ---
    if (ASIOpenCamera(camInfo.CameraID) != ASI_SUCCESS)
    {
        fprintf(stderr, "Failed to open camera %d. Are you root / do you have USB permissions?\n", camIndex);
        return -1;
    }
    if (ASIInitCamera(camInfo.CameraID) != ASI_SUCCESS)
    {
        fprintf(stderr, "Failed to initialise camera %d.\n", camIndex);
        ASICloseCamera(camInfo.CameraID);
        return -1;
    }

    // Print sensor temperature
    long ltemp = 0;
    ASI_BOOL bAuto = ASI_FALSE;
    if (ASIGetControlValue(camInfo.CameraID, ASI_TEMPERATURE, &ltemp, &bAuto) == ASI_SUCCESS)
        printf("Sensor temp   : %.1f °C\n", static_cast<float>(ltemp) / 10.0f);

    // Set gain to 0, USB bandwidth to moderate value
    ASISetControlValue(camInfo.CameraID, ASI_GAIN, 0, ASI_FALSE);
    ASISetControlValue(camInfo.CameraID, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE);

    // --- Determine which binning modes to run ---
    std::vector<int> bins;
    if (binMode == 0)
        bins = {1, 2, 4};
    else
        bins = {binMode};

    // Filter out binning values not supported by this camera
    {
        std::vector<int> supportedBins;
        for (int b : bins)
        {
            // ASI_CAMERA_INFO.SupportedBins is a zero-terminated array (max 16 entries)
            for (int k = 0; k < 16 && camInfo.SupportedBins[k] != 0; ++k)
            {
                if (camInfo.SupportedBins[k] == b)
                {
                    supportedBins.push_back(b);
                    break;
                }
            }
        }
        if (supportedBins.empty())
        {
            fprintf(stderr, "No requested binning mode is supported by this camera.\n");
            ASICloseCamera(camInfo.CameraID);
            return -1;
        }
        bins = supportedBins;
    }

    // --- Run benchmarks ---
    std::vector<BinResult> results;
    for (int b : bins)
    {
        BinResult r = benchmarkBin(camInfo.CameraID, b, exposureMs, numFrames,
                                   camInfo.MaxWidth, camInfo.MaxHeight);
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
    printf("Camera   : %s\n", camInfo.Name);
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
    ASICloseCamera(camInfo.CameraID);
    printf("ASI Camera Benchmark completed.\n");
    return 0;
}
