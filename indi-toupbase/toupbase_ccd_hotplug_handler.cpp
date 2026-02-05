/*
    Toupbase CCD Hot Plug Handler Class Source File

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

#include "toupbase_ccd_hotplug_handler.h"
#include "indilogger.h"    // For LOG_INFO, LOG_DEBUG, etc.
#include "indi_toupbase.h" // For ToupBase
#include "libtoupbase.h"   // For Toupcam SDK functions

#include <algorithm>       // For std::remove_if, std::find_if
#include <string>          // For std::string, std::to_string
#include <vector>          // For std::vector
#include <map>             // For std::map
#include <memory>          // For std::shared_ptr
#include <stdexcept>       // For std::invalid_argument, std::out_of_range
#include <set>             // For std::set

ToupbaseCCDHotPlugHandler::ToupbaseCCDHotPlugHandler()
{
    LOG_DEBUG("ToupbaseCCDHotPlugHandler initialized.");
}

ToupbaseCCDHotPlugHandler::~ToupbaseCCDHotPlugHandler()
{
    // Clean up any remaining devices if necessary
    for (const auto& device : m_internalCameras)
    {
        // Perform any necessary INDI cleanup for the device
        device->deleteProperty(nullptr); // Delete all properties for this device
    }
    m_internalCameras.clear();
    m_managedDevicesView.clear();
    LOG_DEBUG("ToupbaseCCDHotPlugHandler shut down.");
}

std::vector<std::string> ToupbaseCCDHotPlugHandler::discoverConnectedDeviceIdentifiers()
{
    std::vector<std::string> identifiers;

    XP(DeviceV2) pDevs[CP(MAX)];
    int numCameras = FP(EnumV2(pDevs));

    // Create a set of IDs from the currently enumerated devices for quick lookup,
    // ignoring FILTERWHEEL and FOCUSER devices.
    std::set<std::string> currentEnumeratedDeviceIDs;
    for (int i = 0; i < numCameras; ++i)
    {
        if (pDevs[i].model->flag & (CP(FLAG_AUTOFOCUSER) | CP(FLAG_FILTERWHEEL)))
        {
            currentEnumeratedDeviceIDs.insert(std::string(pDevs[i].id));
        }
        else
        {
            LOGF_DEBUG("Ignoring enumerated Toupbase device with display name: %s (ID: %s)", pDevs[i].displayname, pDevs[i].id);
        }
    }

    // Remove disconnected devices from the map
    // Also ensure that FILTERWHEEL and FOCUSER devices are not in the managed list.
    std::vector<std::string> devicesToRemove;
    for (const auto& [deviceID, deviceInfo] : m_connectedDevices)
    {
        if (deviceInfo->model->flag & (CP(FLAG_AUTOFOCUSER) | CP(FLAG_FILTERWHEEL)))
        {
            LOGF_DEBUG("Removing previously connected Toupbase device with display name: %s (ID: %s)",
                       deviceInfo->displayname, deviceInfo->id);
            devicesToRemove.push_back(deviceID);
            continue;
        }

        if (!currentEnumeratedDeviceIDs.count(deviceID))
        {
            LOGF_DEBUG("Toupbase camera disconnected: %s", deviceID.c_str());
            devicesToRemove.push_back(deviceID);
        }
    }

    // Remove devices that are no longer connected
    for (const auto& deviceID : devicesToRemove)
    {
        m_connectedDevices.erase(deviceID);
    }

    // Add newly connected devices, ignoring FILTERWHEEL and FOCUSER devices
    // Use heap allocation with shared_ptr for stable addresses
    for (int i = 0; i < numCameras; ++i)
    {
        if (pDevs[i].model->flag & (CP(FLAG_AUTOFOCUSER) | CP(FLAG_FILTERWHEEL)))
        {
            LOGF_DEBUG("Ignoring newly connected Toupbase device with display name: %s (ID: %s)",
                       pDevs[i].displayname, pDevs[i].id);
            continue; // Skip this device
        }

        std::string deviceID = std::string(pDevs[i].id);
        if (m_connectedDevices.find(deviceID) == m_connectedDevices.end())
        {
            // Allocate device info on heap for stable address
            auto deviceInfo = std::make_shared<XP(DeviceV2)>(pDevs[i]);
            m_connectedDevices[deviceID] = deviceInfo;
            LOGF_DEBUG("Toupbase camera newly connected: %s, Model: %s", pDevs[i].id, pDevs[i].displayname);
        }
    }

    // Update the m_Instance pointers for all managed ToupBase devices
    // These pointers now point to heap-allocated memory with stable addresses
    for (const auto& managedDevice : m_internalCameras)
    {
        std::string managedDeviceID = managedDevice->getCameraID();
        auto it = m_connectedDevices.find(managedDeviceID);
        if (it != m_connectedDevices.end())
        {
            managedDevice->updateDeviceInfo(it->second.get());
        }
    }

    if (m_connectedDevices.empty())
    {
        LOG_DEBUG("No Toupbase cameras found after update.");
        return identifiers;
    }

    for (const auto& [deviceID, deviceInfo] : m_connectedDevices)
    {
        identifiers.push_back(deviceID);
        LOGF_DEBUG("Managed Toupbase camera with ID: %s, Model: %s", deviceID.c_str(), deviceInfo->displayname);
    }
    return identifiers;
}

std::shared_ptr<INDI::DefaultDevice> ToupbaseCCDHotPlugHandler::createDevice(const std::string& identifier)
{
    // Find the camera info in our persistent storage
    auto it = m_connectedDevices.find(identifier);
    if (it == m_connectedDevices.end())
    {
        LOGF_ERROR("No Toupbase camera found with identifier: %s in managed list.", identifier.c_str());
        return nullptr;
    }

    // Get pointer to the heap-allocated device info (stable address)
    XP(DeviceV2)* cameraInfoPtr = it->second.get();
    std::string baseName = cameraInfoPtr->displayname;

    // Check if a device with this CameraID is already managed
    for (const auto& device : m_internalCameras)
    {
        if (device->getCameraID() == identifier) // Use string ID
        {
            LOGF_DEBUG("Device with identifier %s already managed, not creating new.", identifier.c_str());
            return device;
        }
    }

    // Generate a unique name for the new device
    std::string uniqueName = baseName;
    int nameIndex = 0;
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
            nameIndex++;
            uniqueName = baseName + " " + std::to_string(nameIndex);
        }
    }

    // Pass a pointer to the heap-allocated cameraInfo (stable address that won't be invalidated)
    ToupBase *toupbaseCcd = new ToupBase(cameraInfoPtr, uniqueName);
    std::shared_ptr<ToupBase> newDevice = std::shared_ptr<ToupBase>(toupbaseCcd);
    m_internalCameras.push_back(newDevice);
    LOGF_INFO("Created new ToupBase device: %s (ID: %s)", uniqueName.c_str(), identifier.c_str());
    return newDevice;
}

void ToupbaseCCDHotPlugHandler::destroyDevice(std::shared_ptr<INDI::DefaultDevice> device)
{
    std::shared_ptr<ToupBase> toupbaseCcd = std::dynamic_pointer_cast<ToupBase>(device);
    if (!toupbaseCcd)
    {
        LOG_ERROR("Attempted to destroy a non-INDI::ToupBase device with ToupbaseCCDHotPlugHandler.");
        return;
    }

    // Disconnect the device if it's connected
    if (toupbaseCcd->isConnected())
    {
        toupbaseCcd->Disconnect();
    }

    // Delete properties from the INDI server
    toupbaseCcd->deleteProperty(nullptr);

    // Remove from internal storage
    auto it = std::remove_if(m_internalCameras.begin(), m_internalCameras.end(),
                             [&](const std::shared_ptr<ToupBase> &d)
    {
        return d == toupbaseCcd;
    });

    if (it != m_internalCameras.end())
    {
        std::string deviceID = toupbaseCcd->getCameraID();
        m_internalCameras.erase(it, m_internalCameras.end());
        LOGF_INFO("Destroyed INDI::ToupBase device: %s (ID: %s)", toupbaseCcd->getDeviceName(), deviceID.c_str());

        // No m_deviceInfoCache to remove from
    }
    else
    {
        LOGF_WARN("Attempted to destroy INDI::ToupBase device %s not found in managed list.",
                  toupbaseCcd->getDeviceName());
    }
}

const std::map<std::string, std::shared_ptr<INDI::DefaultDevice>> &ToupbaseCCDHotPlugHandler::getManagedDevices() const
{
    // Dynamically construct the map view from m_internalCameras
    m_managedDevicesView.clear();
    for (const auto& device : m_internalCameras)
    {
        m_managedDevicesView[device->getCameraID()] = std::static_pointer_cast<INDI::DefaultDevice>
            (device); // Use string ID and static_pointer_cast
    }
    return m_managedDevicesView;
}

bool ToupbaseCCDHotPlugHandler::getCameraInfoByCameraID(const std::string & cameraIDStr, XP(DeviceV2)& cameraInfo)
{
    // Search our persistent storage for the camera info
    auto it = m_connectedDevices.find(cameraIDStr);
    if (it != m_connectedDevices.end())
    {
        cameraInfo = *(it->second);
        return true;
    }
    return false;
}
