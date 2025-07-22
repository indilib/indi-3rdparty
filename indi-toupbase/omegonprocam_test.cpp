#include <iostream>
#include <vector>
#include <string>
#include <cstdio> // For sprintf

#ifdef _WIN32
#include <windows.h> // For Sleep
#else
#include <unistd.h>  // For usleep
#endif

#define OMEGONPROCAM_HRESULT_ERRORCODE_NEEDED // Define this to get HRESULT constants
#include <libomegonprocam/omegonprocam.h> // Omegonprocam SDK header

// Function to convert HRESULT to a human-readable string
std::string omegonprocamErrorCodes(HRESULT rc) {
    switch (rc) {
        case S_OK: return "Success";
        case S_FALSE: return "Yet another success";
        case E_UNEXPECTED: return "Catastrophic failure";
        case E_NOTIMPL: return "Not supported or not implemented";
        case E_ACCESSDENIED: return "Permission denied";
        case E_OUTOFMEMORY: return "Out of memory";
        case E_INVALIDARG: return "One or more arguments are not valid";
        case E_POINTER: return "Pointer that is not valid";
        case E_FAIL: return "Generic failure";
        case E_WRONG_THREAD: return "Call function in the wrong thread";
        case E_GEN_FAILURE: return "Device not functioning";
        case E_BUSY: return "The requested resource is in use";
        case E_PENDING: return "The data necessary to complete this operation is not yet available";
        case E_TIMEOUT: return "This operation returned because the timeout period expired";
        case E_UNREACH: return "Network is unreachable";
        default:
            char str[256];
            sprintf(str, "Unknown error: 0x%08x", rc);
            return std::string(str);
    }
}

// Global flag to signal image readiness
bool g_imageReady = false;
// Global camera handle
HOmegonprocam g_hCam = nullptr;
// Global flag to indicate if it's a still image event
bool g_isStillImage = false;

// Callback function for events
void onEvent(unsigned int Event, void* pCallbackCtx) {
    (void)pCallbackCtx; // Suppress unused parameter warning
    switch (Event) {
        case OMEGONPROCAM_EVENT_IMAGE:
            std::cout << "Event: Live image ready!" << std::endl;
            g_imageReady = true;
            g_isStillImage = false;
            break;
        case OMEGONPROCAM_EVENT_STILLIMAGE:
            std::cout << "Event: Still image ready!" << std::endl;
            g_imageReady = true;
            g_isStillImage = true;
            break;
        case OMEGONPROCAM_EVENT_EXPOSURE:
            std::cout << "Event: Exposure completed (hardware event)!" << std::endl;
            break;
        case OMEGONPROCAM_EVENT_DISCONNECTED:
            std::cout << "Event: Camera disconnected!" << std::endl;
            // Handle disconnection, e.g., close the application or try to reconnect
            break;
        case OMEGONPROCAM_EVENT_TRIGGERFAIL:
            std::cout << "Event: Trigger failed!" << std::endl;
            break;
        default:
            std::cout << "Event: Unknown event 0x" << std::hex << Event << std::dec << std::endl;
            break;
    }
}

int main() {
    std::cout << "Omegonprocam Test Application (Single Exposure Trigger Mode)" << std::endl;

    // 1. Enumerate devices
    OmegonprocamDeviceV2 pDevs[OMEGONPROCAM_MAX];
    int iNumDev = Omegonprocam_EnumV2(pDevs);

    if (iNumDev == 0) {
        std::cout << "No Omegonprocam devices found." << std::endl;
        return 0;
    }

    std::cout << "Found " << iNumDev << " Omegonprocam device(s):" << std::endl;
    for (int i = 0; i < iNumDev; ++i) {
        std::cout << "  Device " << i << ": " << pDevs[i].displayname << " (ID: " << pDevs[i].id << ")" << std::endl;
    }

    // For simplicity, try to open the first device
    if (iNumDev > 0) {
        g_hCam = Omegonprocam_Open(pDevs[0].id);
        if (g_hCam == nullptr) {
            std::cout << "Failed to open device." << std::endl; 
            return 1;
        }
        std::cout << "Successfully opened device: " << pDevs[0].displayname << std::endl;

        // 2. Set resolution and start pull mode with event callback
        HRESULT hr = Omegonprocam_put_eSize(g_hCam, 0); // Set to first resolution for simplicity
        if (FAILED(hr)) {
            std::cout << "Failed to set resolution: " << omegonprocamErrorCodes(hr) << std::endl;
            Omegonprocam_Close(g_hCam);
            return 1;
        }

        // Enable software trigger mode
        hr = Omegonprocam_put_Option(g_hCam, OMEGONPROCAM_OPTION_TRIGGER, 1); // 1 = software or simulated trigger mode
        if (FAILED(hr)) {
            std::cout << "Failed to set trigger mode: " << omegonprocamErrorCodes(hr) << std::endl;
            Omegonprocam_Close(g_hCam);
            return 1;
        }
        std::cout << "Camera set to software trigger mode." << std::endl;

        // Start pull mode with event callback
        hr = Omegonprocam_StartPullModeWithCallback(g_hCam, (POMEGONPROCAM_EVENT_CALLBACK)onEvent, nullptr); // Cast to correct type
        if (FAILED(hr)) {
            std::cout << "Failed to start pull mode with callback: " << omegonprocamErrorCodes(hr) << std::endl;
            Omegonprocam_Close(g_hCam);
            return 1;
        }
        std::cout << "Pull mode with event callback started." << std::endl;

        // 3. Set exposure time
        unsigned int nExposure = 100000; // 100ms in microseconds
        hr = Omegonprocam_put_ExpoTime(g_hCam, nExposure);
        if (FAILED(hr)) {
            std::cout << "Failed to set exposure time: " << omegonprocamErrorCodes(hr) << std::endl;
            Omegonprocam_Stop(g_hCam);
            Omegonprocam_Close(g_hCam);
            return 1;
        }
        std::cout << "Exposure set to " << nExposure << " microseconds." << std::endl;

        // 4. Trigger a single exposure
        // Use Omegonprocam_Trigger for software trigger
        hr = Omegonprocam_Trigger(g_hCam, 1); // Trigger 1 image
        if (FAILED(hr)) {
            std::cout << "Failed to trigger: " << omegonprocamErrorCodes(hr) << std::endl;
            Omegonprocam_Stop(g_hCam);
            Omegonprocam_Close(g_hCam);
            return 1;
        }
        std::cout << "Triggered 1 image. Waiting for image ready event..." << std::endl;

        // Wait for the image ready event
        int timeout_ms = 10000; // 10 seconds timeout
        int elapsed_ms = 0;
        while (!g_imageReady && elapsed_ms < timeout_ms) {
            #ifdef _WIN32
                Sleep(100); // milliseconds
            #else
                usleep(100 * 1000); // microseconds
            #endif
            elapsed_ms += 100;
        }

        if (g_imageReady) {
            // 5. Pull image data
            unsigned int nWidth = 0, nHeight = 0;
            // Omegonprocam_get_Resolution takes resolution index, not 0 for current.
            // We need to get the current resolution index first.
            unsigned int currentResolutionIndex = 0;
            Omegonprocam_get_eSize(g_hCam, &currentResolutionIndex);
            Omegonprocam_get_Resolution(g_hCam, currentResolutionIndex, (int*)&nWidth, (int*)&nHeight); // Get current resolution
            
            // Allocate buffer for image data
            // Assuming 24-bit RGB for simplicity, adjust if camera outputs different format
            unsigned int bufferSize = nWidth * nHeight * 3; 
            std::vector<unsigned char> imageData(bufferSize);

            if (g_isStillImage) {
                hr = Omegonprocam_PullStillImage(g_hCam, imageData.data(), 24, &nWidth, &nHeight); // Request RGB24
            } else {
                hr = Omegonprocam_PullImage(g_hCam, imageData.data(), 24, &nWidth, &nHeight); // Request RGB24
            }
            
            if (FAILED(hr)) {
                std::cout << "Failed to pull image: " << omegonprocamErrorCodes(hr) << std::endl;
            } else {
                std::cout << "Successfully pulled image! Width: " << nWidth << ", Height: " << nHeight << std::endl;
                // Here you would typically save the image or process it.
            }
        } else {
            std::cout << "Timeout waiting for image ready event." << std::endl;
        }

        // 6. Stop and close device
        hr = Omegonprocam_Stop(g_hCam);
        if (FAILED(hr)) {
            std::cout << "Failed to stop camera: " << omegonprocamErrorCodes(hr) << std::endl;
        } else {
            std::cout << "Camera stopped." << std::endl;
        }

        Omegonprocam_Close(g_hCam);
        std::cout << "Device closed." << std::endl;
    }

    return 0;
}
