/*
 PlayerOne Camera Benchmark

 Measures frame capture performance across different binning modes (1x1, 2x2, 4x4).
 No frames are written to disk — this is a pure capture latency / throughput benchmark.

 Usage:
   ./playerone_camera_bench [--camera <index>] [--exposure <ms>] [--bin <1|2|4>] [--frames <N>]

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

#include <PlayerOneCamera.h>

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
// Benchmark a single binning mode
// Returns false on any camera error.
// ---------------------------------------------------------------------------
struct BinResult
{
    int    bin       {0};
    int    width     {0};
    int    height    {0};
    double meanMs    {0.0};
    double minMs     {0.0};
    double maxMs     {0.0};
    double stddevMs  {0.0};
    double fps       {0.0};
    double overheadMs{0.0}; // mean total - exposure time
    bool   ok        {false};
};

static BinResult benchmarkBin(int camIndex, int bin, int exposureMs, int numFrames, int maxWidth, int maxHeight)
{
    BinResult result;
    result.bin = bin;

    // Compute image dimensions for this binning
    int roiWidth  = (maxWidth  / bin);
    int roiHeight = (maxHeight / bin);
    result.width  = roiWidth;
    result.height = roiHeight;

    printf("\n--- Binning %dx%d  (%d x %d px) ---\n", bin, bin, roiWidth, roiHeight);

    // Configure camera
    if (POASetImageBin(camIndex, bin) != POA_OK)
    {
        fprintf(stderr, "  ERROR: POASetImageBin(%d) failed\n", bin);
        return result;
    }
    if (POASetImageSize(camIndex, roiWidth, roiHeight) != POA_OK)
    {
        fprintf(stderr, "  ERROR: POASetImageSize(%d, %d) failed\n", roiWidth, roiHeight);
        return result;
    }
    if (POASetImageFormat(camIndex, POA_RAW16) != POA_OK)
    {
        fprintf(stderr, "  ERROR: POASetImageFormat(POA_RAW16) failed\n");
        return result;
    }

    // Read back actual size (camera may round)
    int actualWidth = 0, actualHeight = 0;
    POAGetImageSize(camIndex, &actualWidth, &actualHeight);
    result.width  = actualWidth;
    result.height = actualHeight;

    // Allocate frame buffer (RAW16 = 2 bytes per pixel)
    long imgSize = static_cast<long>(actualWidth) * actualHeight * 2;
    std::vector<unsigned char> imgBuf(imgSize);

    // Set exposure
    POAConfigValue confVal;
    confVal.intValue = exposureMs * 1000; // microseconds
    POASetConfig(camIndex, POA_EXPOSURE, confVal, POA_FALSE);

    // Warm-up frame (not measured)
    printf("  Warm-up frame... ");
    fflush(stdout);
    {
        POABool pIsReady = POA_FALSE;
        POAStartExposure(camIndex, POA_FALSE);
        while (pIsReady == POA_FALSE)
            POAImageReady(camIndex, &pIsReady);
        POAGetImageData(camIndex, imgBuf.data(), imgSize, exposureMs + 5000);
        POAStopExposure(camIndex);
    }
    printf("done\n");

    // Timed frames
    std::vector<double> timingsMs;
    timingsMs.reserve(numFrames);

    for (int f = 0; f < numFrames; ++f)
    {
        auto t0 = std::chrono::high_resolution_clock::now();

        POABool pIsReady = POA_FALSE;
        POAStartExposure(camIndex, POA_FALSE);
        while (pIsReady == POA_FALSE)
            POAImageReady(camIndex, &pIsReady);

        POAErrors err = POAGetImageData(camIndex, imgBuf.data(), imgSize, exposureMs + 5000);
        POAStopExposure(camIndex);

        auto t1 = std::chrono::high_resolution_clock::now();

        if (err != POA_OK)
        {
            fprintf(stderr, "  Frame %d: POAGetImageData error %d\n", f + 1, static_cast<int>(err));
            return result;
        }

        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();
        timingsMs.push_back(elapsed);

        printf("  Frame %2d/%d : %8.2f ms  (overhead %+.2f ms)\n",
               f + 1, numFrames, elapsed, elapsed - static_cast<double>(exposureMs));
    }

    // Statistics
    result.meanMs    = mean(timingsMs);
    result.minMs     = *std::min_element(timingsMs.begin(), timingsMs.end());
    result.maxMs     = *std::max_element(timingsMs.begin(), timingsMs.end());
    result.stddevMs  = stddev(timingsMs);
    result.fps       = 1000.0 / result.meanMs;
    result.overheadMs = result.meanMs - static_cast<double>(exposureMs);
    result.ok        = true;

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
    int binMode    = 0;   // 0 = run all
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
    int numDevices = POAGetCameraCount();
    if (numDevices <= 0)
    {
        printf("No PlayerOne cameras detected.\n");
        printf("PlayerOne Camera Benchmark failed.\n");
        return -1;
    }

    printf("=== PlayerOne Camera Benchmark ===\n\n");
    printf("Detected cameras:\n");

    POACameraProperties camInfo;
    for (int i = 0; i < numDevices; ++i)
    {
        POAGetCameraProperties(i, &camInfo);
        printf("  [%d] %s\n", i, camInfo.cameraModelName);
    }

    if (camIndex < 0 || camIndex >= numDevices)
    {
        fprintf(stderr, "\nInvalid camera index %d (0..%d available)\n", camIndex, numDevices - 1);
        return -1;
    }

    POAGetCameraProperties(camIndex, &camInfo);
    printf("\nUsing camera [%d] %s\n", camIndex, camInfo.cameraModelName);
    printf("Max resolution: %d x %d\n", camInfo.maxWidth, camInfo.maxHeight);
    printf("Exposure: %d ms\n", exposureMs);
    printf("Frames per mode: %d\n", numFrames);

    // --- Open & init camera ---
    if (POAOpenCamera(camIndex) != POA_OK)
    {
        fprintf(stderr, "Failed to open camera %d. Are you root / do you have USB permissions?\n", camIndex);
        return -1;
    }
    if (POAInitCamera(camIndex) != POA_OK)
    {
        fprintf(stderr, "Failed to initialise camera %d.\n", camIndex);
        POACloseCamera(camIndex);
        return -1;
    }

    // Set gain to 0, USB bandwidth to moderate
    POAConfigValue confVal;
    confVal.intValue = 0;
    POASetConfig(camIndex, POA_GAIN, confVal, POA_FALSE);
    confVal.intValue = 40;
    POASetConfig(camIndex, POA_USB_BANDWIDTH_LIMIT, confVal, POA_FALSE);

    // --- Determine which binning modes to run ---
    std::vector<int> bins;
    if (binMode == 0)
        bins = {1, 2, 4};
    else
        bins = {binMode};

    // --- Run benchmarks ---
    std::vector<BinResult> results;
    for (int b : bins)
    {
        BinResult r = benchmarkBin(camIndex, b, exposureMs, numFrames, camInfo.maxWidth, camInfo.maxHeight);
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
    printf("Camera   : %s\n", camInfo.cameraModelName);
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
    POACloseCamera(camIndex);
    printf("PlayerOne Camera Benchmark completed.\n");
    return 0;
}
