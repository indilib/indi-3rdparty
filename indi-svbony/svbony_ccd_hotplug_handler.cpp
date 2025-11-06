/*
    SVBony CCD Hot Plug Handler Class Source File

    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "svbony_ccd_hotplug_handler.h"
#include <indilogger.h>   // For LOG_INFO, LOG_DEBUG, etc.
#include <algorithm>      // For std::remove_if, std::find_if
#include <string>         // For std::string, std::to_string
#include <vector>         // For std::vector
#include <map>            // For std::map
#include <memory>         // For std::shared_ptr
#include <stdexcept>      // For std::invalid_argument, std::out_of_range

namespace INDI
{

SVBONYCCDHotPlugHandler::SVBONYCCDHotPlugHandler()
{
    LOG_DEBUG("SVBONYCCDHotPlugHandler initialized.");
}

SVBONYCCDHotPlugHandler::~SVBONYCCDHotPlugHandler()
{
    // Clean up any remaining devices if necessary
    for (const auto& device : m_internalCameras)
    {
        // Perform any necessary INDI cleanup for the device
        // For example, delete properties if they are still defined
        device->deleteProperty(nullptr); // Delete all properties for this device
    }
    m_internalCameras.clear();
    m_managedDevicesView.clear();
    LOG_DEBUG("SVBONYCCDHotPlugHandler shut down.");
}

std::vector<std::string> SVBONYCCDHotPlugHandler::discoverConnectedDeviceIdentifiers()
{
    std::vector<std::string> identifiers;
    int numCameras = SVBGetNumOfConnectedCameras();
    if (numCameras < 0)
    {
        LOG_ERROR("SVBGetNumOfConnectedCameras returned an error.");
        return identifiers;
    }

    for (int i = 0; i < numCameras; ++i)
    {
        SVB_CAMERA_INFO cameraInfo;
        if (SVBGetCameraInfo(&cameraInfo, i) == SVB_SUCCESS)
        {
            identifiers.push_back(std::to_string(cameraInfo.CameraID));
            LOGF_DEBUG("Discovered SVBony camera with CameraID: %d", cameraInfo.CameraID);
        }
        else
        {
            LOGF_WARN("Failed to get camera property for index %d.", i);
        }
    }
    return identifiers;
}

std::shared_ptr<DefaultDevice> SVBONYCCDHotPlugHandler::createDevice(const std::string& identifier)
{
    int cameraID;
    try
    {
        cameraID = std::stoi(identifier);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("SVBONYCCDHotPlugHandler", "Invalid identifier format for CameraID: %s. Error: %s", identifier.c_str(),
                   e.what());
        return nullptr;
    }

    SVB_CAMERA_INFO cameraInfo;
    bool foundCamera = false;
    int numCameras = SVBGetNumOfConnectedCameras();
    if (numCameras >= 0)
    {
        for (int i = 0; i < numCameras; ++i)
        {
            SVB_CAMERA_INFO currentCameraInfo;
            if (SVBGetCameraInfo(&currentCameraInfo, i) == SVB_SUCCESS && currentCameraInfo.CameraID == cameraID)
            {
                cameraInfo = currentCameraInfo;
                foundCamera = true;
                break;
            }
        }
    }

    if (!foundCamera)
    {
        LOGF_ERROR("Failed to get camera info for CameraID: %d", cameraID);
        return nullptr;
    }

    // Check if a device with this CameraID is already managed
    for (const auto& device : m_internalCameras)
    {
        if (device->getCameraInfo().CameraID == cameraID)
        {
            LOGF_DEBUG("Device with CameraID %d already managed, not creating new.", cameraID);
            return device;
        }
    }

    // Generate a unique name for the new device
    std::string baseName = "SVBONY CCD " + std::string(cameraInfo.FriendlyName +
                           7); // Assuming FriendlyName starts with "SVBONY "
    std::string uniqueName = baseName;
    int index = 0;
    bool nameExists = true;
    while (nameExists)
    {
        nameExists = false;
        for (const auto& device : m_internalCameras)
        {
            if (device->getDeviceName() == uniqueName)
            {
                nameExists = true;
                break;
            }
        }
        if (nameExists)
        {
            index++;
            uniqueName = baseName + " " + std::to_string(index);
        }
    }

    // Retrieve serial number for the SVBONYCCD constructor
    std::string serialNumber = getSerialNumberFromCameraID(cameraID);

    SVBONYCCD *svbonyCcd = new SVBONYCCD(cameraInfo, uniqueName, serialNumber);
    std::shared_ptr<SVBONYCCD> newDevice = std::shared_ptr<SVBONYCCD>(svbonyCcd);
    m_internalCameras.push_back(newDevice);
    LOGF_INFO("Created new SVBONYCCD device: %s (CameraID: %d)", uniqueName.c_str(), cameraID);
    return newDevice;
}

void SVBONYCCDHotPlugHandler::destroyDevice(std::shared_ptr<DefaultDevice> device)
{
    std::shared_ptr<SVBONYCCD> svbonyCcd = std::dynamic_pointer_cast<SVBONYCCD>(device);
    if (!svbonyCcd)
    {
        LOG_ERROR("Attempted to destroy a non-SVBONYCCD device with SVBONYCCDHotPlugHandler.");
        return;
    }

    // Disconnect the device if it's connected
    if (svbonyCcd->isConnected())
    {
        svbonyCcd->Disconnect();
    }

    // Delete properties from the INDI server
    svbonyCcd->deleteProperty(nullptr);

    // Remove from internal storage
    auto it = std::remove_if(m_internalCameras.begin(), m_internalCameras.end(),
                             [&](const std::shared_ptr<SVBONYCCD> &d)
    {
        return d == svbonyCcd;
    });

    if (it != m_internalCameras.end())
    {
        m_internalCameras.erase(it, m_internalCameras.end());
        LOGF_INFO("Destroyed SVBONYCCD device: %s (CameraID: %d)", svbonyCcd->getDeviceName(), svbonyCcd->getCameraInfo().CameraID);
    }
    else
    {
        LOGF_WARN("Attempted to destroy SVBONYCCD device %s not found in managed list.",
                  svbonyCcd->getDeviceName());
    }
}

const std::map<std::string, std::shared_ptr<DefaultDevice>>& SVBONYCCDHotPlugHandler::getManagedDevices() const
{
    // Dynamically construct the map view from m_internalCameras
    m_managedDevicesView.clear();
    for (const auto& device : m_internalCameras)
    {
        m_managedDevicesView[std::to_string(device->getCameraInfo().CameraID)] = device;
    }
    return m_managedDevicesView;
}

bool SVBONYCCDHotPlugHandler::getCameraInfoByCameraID(const std::string& cameraIDStr, SVB_CAMERA_INFO& cameraInfo)
{
    int cameraID;
    try
    {
        cameraID = std::stoi(cameraIDStr);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("SVBONYCCDHotPlugHandler", "Invalid CameraID format: %s. Error: %s", cameraIDStr.c_str(), e.what());
        return false;
    }

    int numCameras = SVBGetNumOfConnectedCameras();
    if (numCameras < 0)
    {
        return false;
    }

    for (int i = 0; i < numCameras; ++i)
    {
        SVB_CAMERA_INFO currentCameraInfo;
        if (SVBGetCameraInfo(&currentCameraInfo, i) == SVB_SUCCESS)
        {
            if (currentCameraInfo.CameraID == cameraID)
            {
                cameraInfo = currentCameraInfo;
                return true;
            }
        }
    }
    return false;
}

std::string SVBONYCCDHotPlugHandler::getSerialNumberFromCameraID(int cameraID)
{
    SVB_SN serialNumber;
    if (SVBOpenCamera(cameraID) == SVB_SUCCESS)
    {
        if (SVBGetSerialNumber(cameraID, &serialNumber) == SVB_SUCCESS)
        {
            SVBCloseCamera(cameraID);
            char snChars[100];
            sprintf(snChars, "%02x%02x%02x%02x%02x%02x%02x%02x", serialNumber.id[0], serialNumber.id[1],
                    serialNumber.id[2], serialNumber.id[3], serialNumber.id[4], serialNumber.id[5],
                    serialNumber.id[6], serialNumber.id[7]);
            snChars[16] = 0;
            return std::string(snChars);
        }
        SVBCloseCamera(cameraID); // Ensure camera is closed even if serial number retrieval fails
    }
    return "";
}

} // namespace INDI
