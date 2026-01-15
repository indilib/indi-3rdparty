/*
    ASI EFW Hot Plug Handler Class Source File

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

#include "asi_wheel_hotplug_handler.h"
#include "indilogger.h"   // For LOG_INFO, LOG_DEBUG, etc.
#include <algorithm>      // For std::remove_if, std::find_if
#include <EFW_filter.h>   // For EFW SDK functions
#include <string>         // For std::string, std::to_string
#include <vector>         // For std::vector
#include <map>            // For std::map
#include <memory>         // For std::shared_ptr
#include <stdexcept>      // For std::invalid_argument, std::out_of_range

namespace INDI
{

ASIWHEELHotPlugHandler::ASIWHEELHotPlugHandler()
{
    LOG_DEBUG("ASIWHEELHotPlugHandler initialized.");
}

ASIWHEELHotPlugHandler::~ASIWHEELHotPlugHandler()
{
    // Clean up any remaining devices if necessary
    for (const auto& device : m_internalWheels)
    {
        // Perform any necessary INDI cleanup for the device
        // For example, delete properties if they are still defined
        device->deleteProperty(nullptr); // Delete all properties for this device
    }
    m_internalWheels.clear();
    m_managedDevicesView.clear();
    LOG_DEBUG("ASIWHEELHotPlugHandler shut down.");
}

std::vector<std::string> ASIWHEELHotPlugHandler::discoverConnectedDeviceIdentifiers()
{
    std::vector<std::string> identifiers;
    int numWheels = EFWGetNum();
    if (numWheels < 0)
    {
        LOG_ERROR("EFWGetNum returned an error.");
        return identifiers;
    }

    for (int i = 0; i < numWheels; ++i)
    {
        int id;
        EFW_ERROR_CODE result = EFWGetID(i, &id);
        if (result == EFW_SUCCESS)
        {
            identifiers.push_back(std::to_string(id));
            LOGF_DEBUG("Discovered ASI EFW with ID: %d", id);
        }
        else
        {
            LOGF_WARN("Failed to get filter wheel ID for index %d.", i);
        }
    }
    return identifiers;
}

std::shared_ptr<DefaultDevice> ASIWHEELHotPlugHandler::createDevice(const std::string& identifier)
{
    int wheelID;
    try
    {
        wheelID = std::stoi(identifier);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("ASIWHEELHotPlugHandler", "Invalid identifier format for filter wheel ID: %s. Error: %s", identifier.c_str(),
                   e.what());
        return nullptr;
    }

    EFW_INFO efwInfo;
    bool foundWheel = false;
    int numWheels = EFWGetNum();
    if (numWheels >= 0)
    {
        for (int i = 0; i < numWheels; ++i)
        {
            int id;
            if (EFWGetID(i, &id) == EFW_SUCCESS && id == wheelID)
            {
                // Get properties (note: may return ERROR_CLOSED, which is acceptable)
                EFW_ERROR_CODE result = EFWGetProperty(id, &efwInfo);
                if (result == EFW_SUCCESS || result == EFW_ERROR_CLOSED)
                {
                    foundWheel = true;
                    break;
                }
            }
        }
    }

    if (!foundWheel)
    {
        LOGF_ERROR("Failed to get filter wheel info for ID: %d", wheelID);
        return nullptr;
    }

    // Check if a device with this ID is already managed
    for (const auto& device : m_internalWheels)
    {
        if (device->getEFWInfo().ID == wheelID)
        {
            LOGF_DEBUG("Device with filter wheel ID %d already managed, not creating new.", wheelID);
            return device;
        }
    }

    // Generate a unique name for the new device
    std::string baseName = "ZWO " + std::string(efwInfo.Name);
    std::string uniqueName = baseName;
    int index = 0;
    bool nameExists = true;
    while (nameExists)
    {
        nameExists = false;
        for (const auto& device : m_internalWheels)
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

    ASIWHEEL *asiWheel = new ASIWHEEL(efwInfo, uniqueName.c_str());
    std::shared_ptr<ASIWHEEL> newDevice = std::shared_ptr<ASIWHEEL>(asiWheel);
    m_internalWheels.push_back(newDevice);
    LOGF_INFO("Created new ASIWHEEL device: %s (ID: %d)", uniqueName.c_str(), wheelID);
    return newDevice;
}

void ASIWHEELHotPlugHandler::destroyDevice(std::shared_ptr<DefaultDevice> device)
{
    std::shared_ptr<ASIWHEEL> asiWheel = std::dynamic_pointer_cast<ASIWHEEL>(device);
    if (!asiWheel)
    {
        LOG_ERROR("Attempted to destroy a non-ASIWHEEL device with ASIWHEELHotPlugHandler.");
        return;
    }

    // Delete properties from the INDI server (this will handle disconnection)
    asiWheel->deleteProperty(nullptr);

    // Remove from internal storage
    auto it = std::remove_if(m_internalWheels.begin(), m_internalWheels.end(),
                             [&](const std::shared_ptr<ASIWHEEL> &d)
    {
        return d == asiWheel;
    });

    if (it != m_internalWheels.end())
    {
        m_internalWheels.erase(it, m_internalWheels.end());
        LOGF_INFO("Destroyed ASIWHEEL device: %s (ID: %d)", asiWheel->getDeviceName(), asiWheel->getEFWInfo().ID);
    }
    else
    {
        LOGF_WARN("Attempted to destroy ASIWHEEL device %s not found in managed list.",
                  asiWheel->getDeviceName());
    }
}

const std::map<std::string, std::shared_ptr<DefaultDevice>> &ASIWHEELHotPlugHandler::getManagedDevices() const
{
    // Dynamically construct the map view from m_internalWheels
    m_managedDevicesView.clear();
    for (const auto& device : m_internalWheels)
    {
        m_managedDevicesView[std::to_string(device->getEFWInfo().ID)] = device;
    }
    return m_managedDevicesView;
}

bool ASIWHEELHotPlugHandler::getEFWInfoByID(const std::string& idStr, EFW_INFO& efwInfo)
{
    int wheelID;
    try
    {
        wheelID = std::stoi(idStr);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("ASIWHEELHotPlugHandler", "Invalid filter wheel ID format: %s. Error: %s", idStr.c_str(), e.what());
        return false;
    }

    int numWheels = EFWGetNum();
    if (numWheels < 0)
    {
        return false;
    }

    for (int i = 0; i < numWheels; ++i)
    {
        int id;
        if (EFWGetID(i, &id) == EFW_SUCCESS && id == wheelID)
        {
            EFW_ERROR_CODE result = EFWGetProperty(id, &efwInfo);
            if (result == EFW_SUCCESS || result == EFW_ERROR_CLOSED)
            {
                return true;
            }
        }
    }
    return false;
}

} // namespace INDI
