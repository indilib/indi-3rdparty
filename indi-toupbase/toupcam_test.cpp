#include <iostream>
#include <vector>
#include <string>
#include <cstdio> // For sprintf

#ifdef _WIN32
#include <windows.h> // For Sleep
#else
#include <unistd.h>  // For usleep
#endif

#define TOUPCAM_HRESULT_ERRORCODE_NEEDED // Define this to get HRESULT constants
#include <toupcam.h> // Toupcam SDK header

// Function to convert HRESULT to a human-readable string
std::string toupcamErrorCodes(HRESULT rc)
{
    switch (rc)
    {
        case S_OK:
            return "Success";
        case S_FALSE:
            return "Yet another success";
        case E_UNEXPECTED:
            return "Catastrophic failure";
        case E_NOTIMPL:
            return "Not supported or not implemented";
        case E_ACCESSDENIED:
            return "Permission denied";
        case E_OUTOFMEMORY:
            return "Out of memory";
        case E_INVALIDARG:
            return "One or more arguments are not valid";
        case E_POINTER:
            return "Pointer that is not valid";
        case E_FAIL:
            return "Generic failure";
        case E_WRONG_THREAD:
            return "Call function in the wrong thread";
        case E_GEN_FAILURE:
            return "Device not functioning";
        case E_BUSY:
            return "The requested resource is in use";
        case E_PENDING:
            return "The data necessary to complete this operation is not yet available";
        case E_TIMEOUT:
            return "This operation returned because the timeout period expired";
        case E_UNREACH:
            return "Network is unreachable";
        default:
            char str[256];
            sprintf(str, "Unknown error: 0x%08x", rc);
            return std::string(str);
    }
}

// Global flag to signal image readiness
bool g_imageReady = false;
// Global camera handle
HToupcam g_hCam = nullptr;
// Global flag to indicate if it's a still image event
bool g_isStillImage = false;

// Callback function for events
void onEvent(unsigned int Event, void* pCallbackCtx)
{
    (void)pCallbackCtx; // Suppress unused parameter warning
    switch (Event)
    {
        case TOUPCAM_EVENT_IMAGE:
            std::cout << "Event: Live image ready!" << std::endl;
            g_imageReady = true;
            g_isStillImage = false;
            break;
        case TOUPCAM_EVENT_STILLIMAGE:
            std::cout << "Event: Still image ready!" << std::endl;
            g_imageReady = true;
            g_isStillImage = true;
            break;
        case TOUPCAM_EVENT_EXPOSURE:
            std::cout << "Event: Exposure completed (hardware event)!" << std::endl;
            break;
        case TOUPCAM_EVENT_DISCONNECTED:
            std::cout << "Event: Camera disconnected!" << std::endl;
            // Handle disconnection, e.g., close the application or try to reconnect
            break;
        case TOUPCAM_EVENT_TRIGGERFAIL:
            std::cout << "Event: Trigger failed!" << std::endl;
            break;
        default:
            std::cout << "Event: Unknown event 0x" << std::hex << Event << std::dec << std::endl;
            break;
    }
}

int main(int argc, char* argv[])
{
    std::cout << "Toupcam Test Application" << std::endl;

    int numImages = 1; // Default to 1 image
    double exposureTimeSec = 0.1; // Default to 0.1 seconds
    unsigned int exposureTimeUs = 100000; // Will be calculated from exposureTimeSec
    int binning = 1; // Default to 1x1 binning

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-n" && i + 1 < argc)
        {
            numImages = std::stoi(argv[++i]);
        }
        else if (arg == "-e" && i + 1 < argc)
        {
            exposureTimeSec = std::stod(argv[++i]);
        }
        else if (arg == "-b" && i + 1 < argc)
        {
            binning = std::stoi(argv[++i]);
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0] << " [-n <num_images>] [-e <exposure_sec>] [-b <binning>]" << std::endl;
            std::cout << "  -n <num_images> : Number of images to capture (default: 1)" << std::endl;
            std::cout << "  -e <exposure_sec>: Exposure time in seconds (default: 0.1)" << std::endl;
            std::cout << "  -b <binning>    : Binning value (e.g., 1 for 1x1, 2 for 2x2, default: 1)" << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << std::endl;
            std::cout << "Usage: " << argv[0] << " [-n <num_images>] [-e <exposure_sec>] [-b <binning>]" << std::endl;
            return 1;
        }
    }

    exposureTimeUs = static_cast<unsigned int>(exposureTimeSec * 1000000);

    std::cout << "Capturing " << numImages << " image(s) with exposure " << exposureTimeSec << " seconds (" << exposureTimeUs <<
                 " us) and binning " << binning << "x"
              << binning << "." << std::endl;

    // 1. Enumerate devices
    ToupcamDeviceV2 pDevs[TOUPCAM_MAX];
    int iNumDev = Toupcam_EnumV2(pDevs);

    if (iNumDev == 0)
    {
        std::cout << "No Toupcam devices found." << std::endl;
        return 0;
    }

    std::cout << "Found " << iNumDev << " Toupcam device(s):" << std::endl;
    for (int i = 0; i < iNumDev; ++i)
    {
        std::cout << "  Device " << i << ": " << pDevs[i].displayname << " (ID: " << pDevs[i].id << ")" << std::endl;
    }

    // For simplicity, try to open the first device
    if (iNumDev > 0)
    {
        g_hCam = Toupcam_Open(pDevs[0].id);
        if (g_hCam == nullptr)
        {
            // Toupcam_get_LastError is not in toupcam.h, so we can't get specific error here.
            std::cout << "Failed to open device." << std::endl;
            return 1;
        }
        std::cout << "Successfully opened device: " << pDevs[0].displayname << std::endl;

        // 2. Set resolution and start pull mode with event callback
        HRESULT hr = Toupcam_put_eSize(g_hCam, 0); // Set to first resolution for simplicity
        if (FAILED(hr))
        {
            std::cout << "Failed to set resolution: " << toupcamErrorCodes(hr) << std::endl;
            Toupcam_Close(g_hCam);
            return 1;
        }

        // Enable software trigger mode
        hr = Toupcam_put_Option(g_hCam, TOUPCAM_OPTION_TRIGGER, 1); // 1 = software or simulated trigger mode
        if (FAILED(hr))
        {
            std::cout << "Failed to set trigger mode: " << toupcamErrorCodes(hr) << std::endl;
            Toupcam_Close(g_hCam);
            return 1;
        }
        std::cout << "Camera set to software trigger mode." << std::endl;

        // Start pull mode with event callback
        hr = Toupcam_StartPullModeWithCallback(g_hCam, (PTOUPCAM_EVENT_CALLBACK)onEvent, nullptr); // Cast to correct type
        if (FAILED(hr))
        {
            std::cout << "Failed to start pull mode with callback: " << toupcamErrorCodes(hr) << std::endl;
            Toupcam_Close(g_hCam);
            return 1;
        }
        std::cout << "Pull mode with event callback started." << std::endl;

        // 3. Set exposure time
        hr = Toupcam_put_ExpoTime(g_hCam, exposureTimeUs);
        if (FAILED(hr))
        {
            std::cout << "Failed to set exposure time: " << toupcamErrorCodes(hr) << std::endl;
            Toupcam_Stop(g_hCam);
            Toupcam_Close(g_hCam);
            return 1;
        }
        std::cout << "Exposure set to " << exposureTimeUs << " microseconds." << std::endl;

        // Set binning mode
        hr = Toupcam_put_Option(g_hCam, TOUPCAM_OPTION_BINNING, binning);
        if (FAILED(hr))
        {
            std::cout << "Failed to set binning mode: " << toupcamErrorCodes(hr) << std::endl;
            Toupcam_Stop(g_hCam);
            Toupcam_Close(g_hCam);
            return 1;
        }
        std::cout << "Binning set to " << binning << "x" << binning << "." << std::endl;

        // 4. Trigger and capture images in a loop
        for (int i = 0; i < numImages; ++i)
        {
            std::cout << "Capturing image " << i + 1 << " of " << numImages << "..." << std::endl;
            g_imageReady = false; // Reset flag for each capture

            hr = Toupcam_Trigger(g_hCam, 1); // Trigger 1 image
            if (FAILED(hr))
            {
                std::cout << "Failed to trigger image " << i + 1 << ": " << toupcamErrorCodes(hr) << std::endl;
                break; // Exit loop on trigger failure
            }
            std::cout << "Triggered image " << i + 1 << ". Waiting for image ready event..." << std::endl;

            // Wait for the image ready event
            int timeout_ms = 10000; // 10 seconds timeout
            int elapsed_ms = 0;
            while (!g_imageReady && elapsed_ms < timeout_ms)
            {
#ifdef _WIN32
                Sleep(100); // milliseconds
#else
                usleep(100 * 1000); // microseconds
#endif
                elapsed_ms += 100;
            }

            if (g_imageReady)
            {
                // 5. Pull image data
                unsigned int nWidth = 0, nHeight = 0;
                Toupcam_get_Resolution(g_hCam, 0, (int*)&nWidth, (int*)&nHeight); // Get current resolution

                // Allocate buffer for image data
                unsigned int bufferSize = nWidth * nHeight * 3; // Assuming 24-bit RGB
                std::vector<unsigned char> imageData(bufferSize);

                if (g_isStillImage)
                {
                    hr = Toupcam_PullStillImage(g_hCam, imageData.data(), 24, &nWidth, &nHeight); // Request RGB24
                }
                else
                {
                    hr = Toupcam_PullImage(g_hCam, imageData.data(), 24, &nWidth, &nHeight); // Request RGB24
                }

                if (FAILED(hr))
                {
                    std::cout << "Failed to pull image " << i + 1 << ": " << toupcamErrorCodes(hr) << std::endl;
                }
                else
                {
                    std::cout << "Successfully pulled image " << i + 1 << "! Width: " << nWidth << ", Height: " << nHeight << std::endl;
                    // Here you would typically save the image or process it.
                    // For example, saving to a file:
                    // char filename[256];
                    // sprintf(filename, "image_%03d.bmp", i + 1);
                    // Toupcam_SaveImage(filename, imageData.data(), nWidth, nHeight, 24);
                    // std::cout << "Saved image to " << filename << std::endl;
                }
            }
            else
            {
                std::cout << "Timeout waiting for image ready event for image " << i + 1 << "." << std::endl;
            }
        }

        // 6. Stop and close device
        hr = Toupcam_Stop(g_hCam);
        if (FAILED(hr))
        {
            std::cout << "Failed to stop camera: " << toupcamErrorCodes(hr) << std::endl;
        }
        else
        {
            std::cout << "Camera stopped." << std::endl;
        }

        Toupcam_Close(g_hCam);
        std::cout << "Device closed." << std::endl;
    }

    return 0;
}
