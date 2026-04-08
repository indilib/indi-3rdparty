/*
 QHY Camera Benchmark

 Measures frame capture performance across different binning modes (1x1, 2x2, 4x4).
 No frames are written to disk — this is a pure capture latency / throughput benchmark.

 Usage:
   ./qhy_camera_bench [--camera <index>] [--exposure <ms>] [--bin <1|2|4>] [--frames <N>]

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

#include <qhyccd.h>

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
// Capture a single frame and return total elapsed ms, or -1 on error.
// ---------------------------------------------------------------------------
static double captureFrame(qhyccd_handle *cam)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    int retVal = ExpQHYCCDSingleFrame(cam);
    if (retVal == (int)QHYCCD_ERROR)
    {
        fprintf(stderr, "  ExpQHYCCDSingleFrame failed (error %d)\n", retVal);
        return -1.0;
    }

    // Allocate read buffer based on current camera settings
    uint32_t memLen = GetQHYCCDMemLength(cam);
    std::vector<unsigned char> imgBuf(memLen);

    uint32_t w = 0, h = 0, bpp = 0, channels = 0;
    retVal = GetQHYCCDSingleFrame(cam, &w, &h, &bpp, &channels, imgBuf.data());

    auto t1 = std::chrono::high_resolution_clock::now();

    if (retVal != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "  GetQHYCCDSingleFrame failed (error %d)\n", retVal);
        CancelQHYCCDExposingAndReadout(cam);
        return -1.0;
    }

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

static BinResult benchmarkBin(qhyccd_handle *cam, int bin, int exposureMs, int numFrames,
                              unsigned int maxWidth, unsigned int maxHeight)
{
    BinResult result;
    result.bin = bin;

    printf("\n--- Binning %dx%d ---\n", bin, bin);

    // Set full-sensor ROI (binning is applied on top by the SDK)
    int retVal = SetQHYCCDResolution(cam, 0, 0, maxWidth, maxHeight);
    if (retVal != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "  ERROR: SetQHYCCDResolution(%u, %u) failed (%d)\n", maxWidth, maxHeight, retVal);
        return result;
    }

    // Apply binning mode
    retVal = SetQHYCCDBinMode(cam, bin, bin);
    if (retVal != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "  ERROR: SetQHYCCDBinMode(%d) failed (%d)\n", bin, retVal);
        return result;
    }

    // Set 16-bit depth
    retVal = IsQHYCCDControlAvailable(cam, CONTROL_TRANSFERBIT);
    if (retVal == QHYCCD_SUCCESS)
    {
        SetQHYCCDBitsMode(cam, 16);
    }

    // Set exposure time (microseconds)
    retVal = SetQHYCCDParam(cam, CONTROL_EXPOSURE, static_cast<double>(exposureMs) * 1000.0);
    if (retVal != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "  ERROR: SetQHYCCDParam CONTROL_EXPOSURE failed (%d)\n", retVal);
        return result;
    }

    // Determine effective image dimensions via a dry-run buffer query
    uint32_t imgW = maxWidth  / static_cast<unsigned int>(bin);
    uint32_t imgH = maxHeight / static_cast<unsigned int>(bin);
    result.width  = static_cast<int>(imgW);
    result.height = static_cast<int>(imgH);

    printf("  Expected output: %u x %u px\n", imgW, imgH);

    // Warm-up frame (not measured)
    printf("  Warm-up frame... ");
    fflush(stdout);
    {
        int r = ExpQHYCCDSingleFrame(cam);
        if (r != (int)QHYCCD_ERROR)
        {
            uint32_t memLen = GetQHYCCDMemLength(cam);
            std::vector<unsigned char> buf(memLen);
            uint32_t w = 0, h = 0, bpp = 0, ch = 0;
            int gr = GetQHYCCDSingleFrame(cam, &w, &h, &bpp, &ch, buf.data());
            if (gr != QHYCCD_SUCCESS)
                CancelQHYCCDExposingAndReadout(cam);
            else
            {
                // Update actual dimensions from the first real readback
                result.width  = static_cast<int>(w);
                result.height = static_cast<int>(h);
            }
        }
    }
    printf("done  (%d x %d px)\n", result.width, result.height);

    // Timed frames
    std::vector<double> timingsMs;
    timingsMs.reserve(numFrames);

    for (int f = 0; f < numFrames; ++f)
    {
        double elapsed = captureFrame(cam);
        if (elapsed < 0.0)
            return result; // error already printed

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

    // --- Init SDK ---
    int retVal = InitQHYCCDResource();
    if (retVal != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "InitQHYCCDResource failed (%d)\n", retVal);
        return -1;
    }

    // Print SDK version
    uint32_t sdkYear = 0, sdkMonth = 0, sdkDay = 0, sdkSubday = 0;
    if (GetQHYCCDSDKVersion(&sdkYear, &sdkMonth, &sdkDay, &sdkSubday) == QHYCCD_SUCCESS)
        printf("QHYCCD SDK version: %d.%d.%d.%d\n", sdkYear, sdkMonth, sdkDay, sdkSubday);

    // --- Enumerate cameras ---
    int numDevices = ScanQHYCCD();
    if (numDevices <= 0)
    {
        printf("No QHY cameras detected.\n");
        printf("QHY Camera Benchmark failed.\n");
        ReleaseQHYCCDResource();
        return -1;
    }

    printf("\n=== QHY Camera Benchmark ===\n\n");
    printf("Detected cameras:\n");

    std::vector<std::string> camIds(numDevices);
    for (int i = 0; i < numDevices; ++i)
    {
        char idBuf[64] = {0};
        if (GetQHYCCDId(i, idBuf) == QHYCCD_SUCCESS)
        {
            camIds[i] = idBuf;
            printf("  [%d] %s\n", i, idBuf);
        }
        else
        {
            camIds[i] = "(unknown)";
            printf("  [%d] (failed to get ID)\n", i);
        }
    }

    if (camIndex < 0 || camIndex >= numDevices)
    {
        fprintf(stderr, "\nInvalid camera index %d (0..%d available)\n", camIndex, numDevices - 1);
        ReleaseQHYCCDResource();
        return -1;
    }

    const std::string &camId = camIds[camIndex];
    printf("\nUsing camera [%d] %s\n", camIndex, camId.c_str());

    // --- Open camera ---
    qhyccd_handle *cam = OpenQHYCCD(const_cast<char *>(camId.c_str()));
    if (cam == nullptr)
    {
        fprintf(stderr, "OpenQHYCCD failed. Are you root / do you have USB permissions?\n");
        ReleaseQHYCCDResource();
        return -1;
    }

    // Single frame mode
    retVal = SetQHYCCDStreamMode(cam, 0);
    if (retVal != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "SetQHYCCDStreamMode(0) failed (%d)\n", retVal);
        CloseQHYCCD(cam);
        ReleaseQHYCCDResource();
        return -1;
    }

    // Use readout mode 0 (default)
    SetQHYCCDReadMode(cam, 0);

    // Init camera
    retVal = InitQHYCCD(cam);
    if (retVal != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "InitQHYCCD failed (%d)\n", retVal);
        CloseQHYCCD(cam);
        ReleaseQHYCCDResource();
        return -1;
    }

    // --- Get chip info ---
    double chipWidthMM = 0, chipHeightMM = 0, pixelWidthUM = 0, pixelHeightUM = 0;
    unsigned int maxWidth = 0, maxHeight = 0, bpp = 0;
    retVal = GetQHYCCDChipInfo(cam, &chipWidthMM, &chipHeightMM,
                               &maxWidth, &maxHeight,
                               &pixelWidthUM, &pixelHeightUM, &bpp);
    if (retVal != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "GetQHYCCDChipInfo failed (%d)\n", retVal);
        CloseQHYCCD(cam);
        ReleaseQHYCCDResource();
        return -1;
    }

    printf("Max resolution: %u x %u\n", maxWidth, maxHeight);
    printf("Chip size     : %.3f x %.3f mm\n", chipWidthMM, chipHeightMM);
    printf("Pixel size    : %.3f x %.3f µm\n", pixelWidthUM, pixelHeightUM);

    // Color / mono detection
    retVal = IsQHYCCDControlAvailable(cam, CAM_COLOR);
    if (retVal == BAYER_GB || retVal == BAYER_GR || retVal == BAYER_BG || retVal == BAYER_RG)
    {
        const char *bayerNames[] = {"GB", "GR", "BG", "RG"};
        int bi = (retVal == BAYER_GB) ? 0 : (retVal == BAYER_GR) ? 1 : (retVal == BAYER_BG) ? 2 : 3;
        printf("Color camera  : bayer pattern %s\n", bayerNames[bi]);
    }
    else
    {
        printf("Mono camera\n");
    }

    // Print temperature if available
    if (IsQHYCCDControlAvailable(cam, CONTROL_CURTEMP) == QHYCCD_SUCCESS)
    {
        double temp = GetQHYCCDParam(cam, CONTROL_CURTEMP);
        printf("Sensor temp   : %.1f °C\n", temp);
    }

    printf("Exposure      : %d ms\n", exposureMs);
    printf("Frames/mode   : %d\n", numFrames);

    // --- Apply base settings: gain=0, USB traffic=40 ---
    if (IsQHYCCDControlAvailable(cam, CONTROL_GAIN) == QHYCCD_SUCCESS)
        SetQHYCCDParam(cam, CONTROL_GAIN, 0);
    if (IsQHYCCDControlAvailable(cam, CONTROL_USBTRAFFIC) == QHYCCD_SUCCESS)
        SetQHYCCDParam(cam, CONTROL_USBTRAFFIC, 40);

    // --- Determine which binning modes to run ---
    std::vector<int> bins;
    if (binMode == 0)
        bins = {1, 2, 4};
    else
        bins = {binMode};

    // Filter against supported binning modes
    {
        std::vector<int> supportedBins;
        for (int b : bins)
        {
            // QHY SDK: IsQHYCCDControlAvailable with CAM_BIN<n>X<n>MODE
            CONTROL_ID binCtrl = CONTROL_ID(-1);
            if      (b == 1) binCtrl = CAM_BIN1X1MODE;
            else if (b == 2) binCtrl = CAM_BIN2X2MODE;
            else if (b == 4) binCtrl = CAM_BIN4X4MODE;

            if (binCtrl != CONTROL_ID(-1) && IsQHYCCDControlAvailable(cam, binCtrl) == QHYCCD_SUCCESS)
                supportedBins.push_back(b);
            else
                printf("  Binning %dx%d not supported by this camera — skipping.\n", b, b);
        }
        if (supportedBins.empty())
        {
            fprintf(stderr, "No requested binning mode is supported by this camera.\n");
            CloseQHYCCD(cam);
            ReleaseQHYCCDResource();
            return -1;
        }
        bins = supportedBins;
    }

    // --- Run benchmarks ---
    std::vector<BinResult> results;
    for (int b : bins)
    {
        BinResult r = benchmarkBin(cam, b, exposureMs, numFrames, maxWidth, maxHeight);
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
    printf("Camera   : %s\n", camId.c_str());
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
    CancelQHYCCDExposingAndReadout(cam);
    CloseQHYCCD(cam);
    ReleaseQHYCCDResource();
    printf("QHY Camera Benchmark completed.\n");
    return 0;
}
