/*
 Pentax CCD Driver for Indi (using Ricoh Camera SDK)
 Copyright (C) 2020 Karl Rees

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

#include "indi_pentax.h"

#define MAX_DEVICES    20   /* Max device cameraCount */

#include "pktriggercord_ccd.h"

#ifndef __aarch64__
#include "pentax_ccd.h"
#include "pentax_event_handler.h"

static std::vector<std::shared_ptr<CameraDevice>> registeredSDKCams;
#endif

static int cameraCount = 0;
static INDI::CCD *cameras[MAX_DEVICES];
// static char logdevicename[14]= "Pentax Driver";

static void cleanup()
{
    for (int i = 0; i < cameraCount; i++)
    {
        delete cameras[i];
    }
}

static bool isInit = false;

//idea here is to allow users to switch between PTP and MSC mode automatically when driver is disconnected
//which requires looking for new cameras.  This is a little problemmatic if more than one camera is connected, but I don't think that'd be a common use case
bool cameraIsConnected() {
    for (int j=0; j<cameraCount; j++) {
        if (cameras[j]->isConnected()) return true;
    }
    return false;
}

void ISInit()
{
    if (!isInit)
    {

#ifndef __aarch64__
        std::vector<std::shared_ptr<CameraDevice>> detectedCameraDevices = CameraDeviceDetector::detect(DeviceInterface::USB);
        size_t detectedCameraCount = detectedCameraDevices.size();
        // int registeredSDKCameraCount = registeredSDKCams.size();

        // look for SDK supported cameras (PTP mode) first
		IDLog("Looking for Pentax camera in  PTP mode.\n");
        if (detectedCameraCount > 0) {
            for (size_t i = 0; (i < detectedCameraCount) && (i < MAX_DEVICES); i++) {
                bool camalreadyregistered = false;
                for (size_t j=0; j<registeredSDKCams.size(); j++) {
                    if (detectedCameraDevices[i] == registeredSDKCams[j]) camalreadyregistered = true;
                }
                if (!camalreadyregistered) {
                    cameras[cameraCount++] = new PentaxCCD(detectedCameraDevices[i]);
                    registeredSDKCams.push_back(detectedCameraDevices[i]);
                }
            }
        }
#endif

        // now look for pktriggercord supported cameras (MSC mode)
        char *model = NULL;
        char *device = NULL;

		IDLog("Looking for Pentax camera in MSC mode.\n");
        pslr_handle_t camhandle = pslr_init(model,device);
        if (camhandle) {
            if (!pslr_connect(camhandle)) {
                // pslr_status status;
                const char *camname = pslr_camera_name(camhandle);
                bool camalreadyregistered = false;
                for (int j=0; j<cameraCount; j++) {
                    if (typeid(*cameras[j])==typeid(PkTriggerCordCCD) && !strncmp(camname,cameras[j]->getDeviceName(),strlen(camname))) camalreadyregistered = true;
                }
                if (!camalreadyregistered) {
                    cameras[cameraCount++] = new PkTriggerCordCCD(camname);
                }
                pslr_disconnect(camhandle);
                pslr_shutdown(camhandle);
            }
        }
        if (cameraCount <= 0)
            IDLog("No supported Pentax cameras were found.  Perhaps the camera is not supported, not powered up, or needs to be in MSC mode?\n");

        atexit(cleanup);
        isInit = true;
    }
}

struct Loader
{
    Loader() { ISInit(); }
} loader;
