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

static void cleanup()
{
    for (int i = 0; i < cameraCount; i++)
    {
        delete cameras[i];
    }
}

static bool isInit = false;

void ISInit()
{

    if (!isInit)
    {
        std::vector<std::shared_ptr<CameraDevice>> detectedCameraDevices = CameraDeviceDetector::detect(DeviceInterface::USB);
        cameraCount = detectedCameraDevices.size();

        // look for SDK supported cameras first
        if (cameraCount > 0) {
            DEBUGDEVICE(logdevicename,INDI::Logger::DBG_SESSION, "Pentax Camera driver using Ricoh Camera SDK");
            DEBUGDEVICE(logdevicename,INDI::Logger::DBG_SESSION, "Please be sure the camera is on, connected, and in PTP mode!!!");
            DEBUGFDEVICE(logdevicename,INDI::Logger::DBG_SESSION, "%d Pentax camera(s) have been detected.",cameraCount);
            for (int i = 0; (i < cameraCount) && (i < MAX_DEVICES); i++) {
                std::shared_ptr<CameraDevice> camera = detectedCameraDevices[i];
                cameras[i] = new PentaxCCD(camera);
            }
        } else {
            char *model = NULL;
            char *device = NULL;
            pslr_handle_t camhandle = pslr_init(model,device);
            // now look for pktriggercord supported cameras
            if (camhandle) {
                    cameras[cameraCount] = new PkTriggerCordCCD(camhandle);
                    cameraCount++;
            }
            if (cameraCount <= 0)
                DEBUGDEVICE(logdevicename,INDI::Logger::DBG_ERROR, "No supported Pentax cameras were found.  Perhaps the camera is not supported, not powered up, or needs to be in MSC mode?");
        }

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    if (cameraCount == 0) isInit = false;
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        INDI::CCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->getDeviceName()))
        {
            camera->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        INDI::CCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->getDeviceName()))
        {
            camera->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        INDI::CCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->getDeviceName()))
        {
            camera->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        INDI::CCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->getDeviceName()))
        {
            camera->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    ISInit();

    for (int i = 0; i < cameraCount; i++)
    {
        INDI::CCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}
