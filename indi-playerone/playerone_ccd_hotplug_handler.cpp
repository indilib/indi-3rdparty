/*
    PlayerOne CCD Hot Plug Handler Class Source File

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

#include "playerone_ccd_hotplug_handler.h"
#include <indilogger.h>   // For LOG_INFO, LOG_DEBUG, etc.
#include <algorithm>      // For std::remove_if, std::find_if
#include <string>         // For std::string, std::to_string
#include <vector>         // For std::vector
#include <map>            // For std::map
#include <memory>         // For std::shared_ptr
#include <stdexcept>      // For std::invalid_argument, std::out_of_range

PlayerOneCCDHotPlugHandler::PlayerOneCCDHotPlugHandler()
{
    LOG_DEBUG("PlayerOneCCDHotPlugHandler initialized.");
}

PlayerOneCCDHotPlugHandler::~PlayerOneCCDHotPlugHandler()
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
    LOG_DEBUG("PlayerOneCCDHotPlugHandler shut down.");
}

std::vector<std::string> PlayerOneCCDHotPlugHandler::discoverConnectedDeviceIdentifiers()
{
    std::vector<std::string> identifiers;
    int numCameras = POAGetCameraCount();
    if (numCameras < 0)
    {
        LOG_ERROR("POAGetCameraCount returned an error.");
        return identifiers;
    }

    for (int i = 0; i < numCameras; ++i)
    {
        POACameraProperties cameraInfo;
        if (POAGetCameraProperties(i, &cameraInfo) == POA_OK)
        {
            identifiers.push_back(std::to_string(cameraInfo.cameraID));
            LOGF_DEBUG("Discovered PlayerOne camera with CameraID: %d", cameraInfo.cameraID);
        }
        else
        {
            LOGF_WARN("Failed to get camera property for index %d.", i);
        }
    }
    return identifiers;
}

std::shared_ptr<INDI::DefaultDevice> PlayerOneCCDHotPlugHandler::createDevice(const std::string& identifier)
{
    int cameraID;
    try
    {
        cameraID = std::stoi(identifier);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("PlayerOneCCDHotPlugHandler", "Invalid identifier format for CameraID: %s. Error: %s", identifier.c_str(),
                   e.what());
        return nullptr;
    }

    POACameraProperties cameraInfo;
    bool foundCamera = false;
    int numCameras = POAGetCameraCount();
    if (numCameras >= 0)
    {
        for (int i = 0; i < numCameras; ++i)
        {
            POACameraProperties currentCameraInfo;
            if (POAGetCameraProperties(i, &currentCameraInfo) == POA_OK && currentCameraInfo.cameraID == cameraID)
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
        if (device->getCameraInfo().cameraID == cameraID)
        {
            LOGF_DEBUG("Device with CameraID %d already managed, not creating new.", cameraID);
            return device;
        }
    }

    // Generate a unique name for the new device
    std::string baseName = "PlayerOne CCD " + std::string(
                               cameraInfo.cameraModelName); // Using cameraModelName as FriendlyName is not available
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

    // Retrieve serial number for the PlayerOneCCD constructor
    std::string serialNumber = getSerialNumberFromCameraID(cameraID);

    POACCD *playeroneCcd = new POACCD(cameraInfo, uniqueName, serialNumber);
    std::shared_ptr<POACCD> newDevice = std::shared_ptr<POACCD>(playeroneCcd);
    m_internalCameras.push_back(newDevice);
    LOGF_INFO("Created new PlayerOneCCD device: %s (CameraID: %d)", uniqueName.c_str(), cameraID);
    return newDevice;
}

void PlayerOneCCDHotPlugHandler::destroyDevice(std::shared_ptr<INDI::DefaultDevice> device)
{
    std::shared_ptr<POACCD> playeroneCcd = std::dynamic_pointer_cast<POACCD>(device);
    if (!playeroneCcd)
    {
        LOG_ERROR("Attempted to destroy a non-PlayerOneCCD device with PlayerOneCCDHotPlugHandler.");
        return;
    }

    // Disconnect the device if it's connected
    if (playeroneCcd->isConnected())
    {
        playeroneCcd->Disconnect();
    }

    // Delete properties from the INDI server
    playeroneCcd->deleteProperty(nullptr);

    // Remove from internal storage
    auto it = std::remove_if(m_internalCameras.begin(), m_internalCameras.end(),
                             [&](const std::shared_ptr<POACCD> &d)
    {
        return d == playeroneCcd;
    });

    if (it != m_internalCameras.end())
    {
        m_internalCameras.erase(it, m_internalCameras.end());
        LOGF_INFO("Destroyed PlayerOneCCD device: %s (CameraID: %d)", playeroneCcd->getDeviceName(),
                  playeroneCcd->getCameraInfo().cameraID);
    }
    else
    {
        LOGF_WARN("Attempted to destroy PlayerOneCCD device %s not found in managed list.",
                  playeroneCcd->getDeviceName());
    }
}

const std::map<std::string, std::shared_ptr<INDI::DefaultDevice>>& PlayerOneCCDHotPlugHandler::getManagedDevices() const
{
    // Dynamically construct the map view from m_internalCameras
    m_managedDevicesView.clear();
    for (const auto& device : m_internalCameras)
    {
        m_managedDevicesView[std::to_string(device->getCameraInfo().cameraID)] = device;
    }
    return m_managedDevicesView;
}

bool PlayerOneCCDHotPlugHandler::getCameraInfoByCameraID(const std::string& cameraIDStr, POACameraProperties& cameraInfo)
{
    int cameraID;
    try
    {
        cameraID = std::stoi(cameraIDStr);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("PlayerOneCCDHotPlugHandler", "Invalid CameraID format: %s. Error: %s", cameraIDStr.c_str(), e.what());
        return false;
    }

    int numCameras = POAGetCameraCount();
    if (numCameras < 0)
    {
        return false;
    }

    for (int i = 0; i < numCameras; ++i)
    {
        POACameraProperties currentCameraInfo;
        if (POAGetCameraProperties(i, &currentCameraInfo) == POA_OK)
        {
            if (currentCameraInfo.cameraID == cameraID)
            {
                cameraInfo = currentCameraInfo;
                return true;
            }
        }
    }
    return false;
}

std::string PlayerOneCCDHotPlugHandler::getSerialNumberFromCameraID(int cameraID)
{
    POACameraProperties cameraInfo;
    if (POAOpenCamera(cameraID) == POA_OK)
    {
        if (POAGetCameraPropertiesByID(cameraID, &cameraInfo) == POA_OK)
        {
            POACloseCamera(cameraID);
            return std::string(cameraInfo.SN);
        }
        POACloseCamera(cameraID); // Ensure camera is closed even if serial number retrieval fails
    }
    return "";
}
