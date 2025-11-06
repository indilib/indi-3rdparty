/*
 QHY INDI Driver

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

#include "qhy_ccd_hotplug_handler.h"
#include "qhy_ccd.h"

#include <indilogger.h>

#include <algorithm>       // For std::remove_if, std::find_if
#include <string>          // For std::string, std::to_string
#include <vector>          // For std::vector
#include <map>             // For std::map
#include <memory>          // For std::shared_ptr
#include <set>             // For std::set

QHYCCDHotPlugHandler::QHYCCDHotPlugHandler()
{
    LOG_DEBUG("QHYCCDHotPlugHandler initialized.");
    // Initialize QHYCCD SDK resources if not already done
    // This should ideally be handled globally or checked for success
    InitQHYCCDResource();
}

QHYCCDHotPlugHandler::~QHYCCDHotPlugHandler()
{
    // Clean up any remaining devices if necessary
    for (const auto& device : m_internalCameras)
    {
        device->deleteProperty(nullptr); // Delete all properties for this device
    }
    m_internalCameras.clear();
    m_managedDevicesView.clear();
    m_connectedDeviceIDs.clear();
    LOG_DEBUG("QHYCCDHotPlugHandler shut down.");
    // Release QHYCCD SDK resources if this is the last handler
    // This should ideally be handled globally or reference counted
    ReleaseQHYCCDResource();
}

std::vector<std::string> QHYCCDHotPlugHandler::discoverConnectedDeviceIdentifiers()
{
    std::vector<std::string> currentIdentifiers;
    char id[64]; // QHYCCD ID buffer

    uint32_t numCameras = ScanQHYCCD();
    if (numCameras == 0)
    {
        LOG_DEBUG("No QHYCCD cameras found.");
        m_connectedDeviceIDs.clear();
        return currentIdentifiers;
    }


    std::set<std::string> newlyDetectedIDs;
    for (uint32_t i = 0; i < numCameras; ++i)
    {
        if (GetQHYCCDId(i, id) == QHYCCD_SUCCESS)
        {
            std::string cameraID(id);
            currentIdentifiers.push_back(cameraID);
            newlyDetectedIDs.insert(cameraID);

            // Check if this is a newly connected device
            bool found = false;
            for (const auto& existingID : m_connectedDeviceIDs)
            {
                if (existingID == cameraID)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                char model[64];
                GetQHYCCDModel(id, model);
                LOGF_DEBUG("QHYCCD camera newly connected: ID: %s, Model: %s", cameraID.c_str(), model);
            }
        }
    }

    // Identify disconnected devices
    for (const auto& existingID : m_connectedDeviceIDs)
    {
        if (newlyDetectedIDs.find(existingID) == newlyDetectedIDs.end())
        {
            LOGF_DEBUG("QHYCCD camera disconnected: %s", existingID.c_str());
        }
    }

    m_connectedDeviceIDs = currentIdentifiers;
    return currentIdentifiers;
}

std::shared_ptr<INDI::DefaultDevice> QHYCCDHotPlugHandler::createDevice(const std::string & identifier)
{
    // Check if a device with this CameraID is already managed
    for (const auto& device : m_internalCameras)
    {
        if (device->getCameraID() == identifier)
        {
            LOGF_DEBUG("Device with identifier %s already managed, not creating new.", identifier.c_str());
            return device;
        }
    }

    // Generate a unique name for the new device
    char model[64];
    if (GetQHYCCDModel(const_cast<char * >(identifier.c_str()), model) != QHYCCD_SUCCESS)
    {
        LOGF_ERROR("Could not get model name for QHYCCD camera with ID: %s", identifier.c_str());
        return nullptr;
    }
    std::string baseName = model;
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

    QHYCCD *qhyCcd = new QHYCCD(uniqueName.c_str(), identifier.c_str());
    std::shared_ptr<QHYCCD> newDevice = std::shared_ptr<QHYCCD>(qhyCcd);
    m_internalCameras.push_back(newDevice);
    LOGF_INFO("Created new QHYCCD device: %s (ID: %s)", uniqueName.c_str(), identifier.c_str());
    return newDevice;
}

void QHYCCDHotPlugHandler::destroyDevice(std::shared_ptr<INDI::DefaultDevice> device)
{
    std::shared_ptr<QHYCCD> qhyCcd = std::dynamic_pointer_cast<QHYCCD>(device);
    if (!qhyCcd)
    {
        LOG_ERROR("Attempted to destroy a non-QHYCCD device with QHYCCDHotPlugHandler.");
        return;
    }

    // Disconnect the device if it's connected
    if (qhyCcd->isConnected())
    {
        qhyCcd->Disconnect();
    }

    // Delete properties from the INDI server
    qhyCcd->deleteProperty(nullptr);

    // Remove from internal storage
    auto it = std::remove_if(m_internalCameras.begin(), m_internalCameras.end(),
                             [&](const std::shared_ptr<QHYCCD> &d)
    {
        return d == qhyCcd;
    });

    if (it != m_internalCameras.end())
    {
        std::string deviceID = qhyCcd->getCameraID();
        m_internalCameras.erase(it, m_internalCameras.end());
        LOGF_INFO("Destroyed QHYCCD device: %s (ID: %s)", qhyCcd->getDeviceName(), deviceID.c_str());
    }
    else
    {
        LOGF_WARN("Attempted to destroy QHYCCD device %s not found in managed list.",
                  qhyCcd->getDeviceName());
    }
}

const std::map<std::string, std::shared_ptr<INDI::DefaultDevice>>& QHYCCDHotPlugHandler::getManagedDevices() const
{
    // Dynamically construct the map view from m_internalCameras
    m_managedDevicesView.clear();
    for (const auto& device : m_internalCameras)
    {
        m_managedDevicesView[device->getCameraID()] = std::static_pointer_cast<INDI::DefaultDevice>(device);
    }
    return m_managedDevicesView;
}
