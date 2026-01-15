/*
    ASI EAF Hot Plug Handler Class Source File

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

#include "asi_focuser_hotplug_handler.h"
#include "indilogger.h"   // For LOG_INFO, LOG_DEBUG, etc.
#include <algorithm>      // For std::remove_if, std::find_if
#include <EAF_focuser.h>  // For EAF SDK functions
#include <string>         // For std::string, std::to_string
#include <vector>         // For std::vector
#include <map>            // For std::map
#include <memory>         // For std::shared_ptr
#include <stdexcept>      // For std::invalid_argument, std::out_of_range

namespace INDI
{

ASIEAFHotPlugHandler::ASIEAFHotPlugHandler()
{
    LOG_DEBUG("ASIEAFHotPlugHandler initialized.");
}

ASIEAFHotPlugHandler::~ASIEAFHotPlugHandler()
{
    // Clean up any remaining devices if necessary
    for (const auto& device : m_internalFocusers)
    {
        // Perform any necessary INDI cleanup for the device
        // For example, delete properties if they are still defined
        device->deleteProperty(nullptr); // Delete all properties for this device
    }
    m_internalFocusers.clear();
    m_managedDevicesView.clear();
    LOG_DEBUG("ASIEAFHotPlugHandler shut down.");
}

std::vector<std::string> ASIEAFHotPlugHandler::discoverConnectedDeviceIdentifiers()
{
    std::vector<std::string> identifiers;
    int numFocusers = EAFGetNum();
    if (numFocusers < 0)
    {
        LOG_ERROR("EAFGetNum returned an error.");
        return identifiers;
    }

    for (int i = 0; i < numFocusers; ++i)
    {
        int id;
        EAF_ERROR_CODE result = EAFGetID(i, &id);
        if (result == EAF_SUCCESS)
        {
            identifiers.push_back(std::to_string(id));
            LOGF_DEBUG("Discovered ASI EAF with ID: %d", id);
        }
        else
        {
            LOGF_WARN("Failed to get focuser ID for index %d.", i);
        }
    }
    return identifiers;
}

std::shared_ptr<DefaultDevice> ASIEAFHotPlugHandler::createDevice(const std::string& identifier)
{
    int focuserID;
    try
    {
        focuserID = std::stoi(identifier);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("ASIEAFHotPlugHandler", "Invalid identifier format for focuser ID: %s. Error: %s", identifier.c_str(), e.what());
        return nullptr;
    }

    EAF_INFO eafInfo;
    bool foundFocuser = false;
    int numFocusers = EAFGetNum();
    if (numFocusers >= 0)
    {
        for (int i = 0; i < numFocusers; ++i)
        {
            int id;
            if (EAFGetID(i, &id) == EAF_SUCCESS && id == focuserID)
            {
                // Open device to get properties
                if (EAFOpen(id) == EAF_SUCCESS)
                {
                    if (EAFGetProperty(id, &eafInfo) == EAF_SUCCESS)
                    {
                        foundFocuser = true;
                        EAFClose(id);
                        break;
                    }
                    EAFClose(id);
                }
            }
        }
    }

    if (!foundFocuser)
    {
        LOGF_ERROR("Failed to get focuser info for ID: %d", focuserID);
        return nullptr;
    }

    // Check if a device with this ID is already managed
    for (const auto& device : m_internalFocusers)
    {
        if (device->getEAFInfo().ID == focuserID)
        {
            LOGF_DEBUG("Device with focuser ID %d already managed, not creating new.", focuserID);
            return device;
        }
    }

    // Generate a unique name for the new device
    std::string baseName = "ZWO EAF";
    std::string uniqueName = baseName;
    int index = 0;
    bool nameExists = true;
    while (nameExists)
    {
        nameExists = false;
        for (const auto& device : m_internalFocusers)
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

    // Retrieve serial number for the ASIEAF constructor
    std::string serialNumber = getSerialNumberFromID(focuserID);

    ASIEAF *asiEaf = new ASIEAF(eafInfo, uniqueName.c_str(), serialNumber);
    std::shared_ptr<ASIEAF> newDevice = std::shared_ptr<ASIEAF>(asiEaf);
    m_internalFocusers.push_back(newDevice);
    LOGF_INFO("Created new ASIEAF device: %s (ID: %d)", uniqueName.c_str(), focuserID);
    return newDevice;
}

void ASIEAFHotPlugHandler::destroyDevice(std::shared_ptr<DefaultDevice> device)
{
    std::shared_ptr<ASIEAF> asiEaf = std::dynamic_pointer_cast<ASIEAF>(device);
    if (!asiEaf)
    {
        LOG_ERROR("Attempted to destroy a non-ASIEAF device with ASIEAFHotPlugHandler.");
        return;
    }

    // Delete properties from the INDI server (this will handle disconnection)
    asiEaf->deleteProperty(nullptr);

    // Remove from internal storage
    auto it = std::remove_if(m_internalFocusers.begin(), m_internalFocusers.end(),
                             [&](const std::shared_ptr<ASIEAF> &d)
    {
        return d == asiEaf;
    });

    if (it != m_internalFocusers.end())
    {
        m_internalFocusers.erase(it, m_internalFocusers.end());
        LOGF_INFO("Destroyed ASIEAF device: %s (ID: %d)", asiEaf->getDeviceName(), asiEaf->getEAFInfo().ID);
    }
    else
    {
        LOGF_WARN("Attempted to destroy ASIEAF device %s not found in managed list.",
                  asiEaf->getDeviceName());
    }
}

const std::map<std::string, std::shared_ptr<DefaultDevice>> &ASIEAFHotPlugHandler::getManagedDevices() const
{
    // Dynamically construct the map view from m_internalFocusers
    m_managedDevicesView.clear();
    for (const auto& device : m_internalFocusers)
    {
        m_managedDevicesView[std::to_string(device->getEAFInfo().ID)] = device;
    }
    return m_managedDevicesView;
}

bool ASIEAFHotPlugHandler::getEAFInfoByID(const std::string& idStr, EAF_INFO& eafInfo)
{
    int focuserID;
    try
    {
        focuserID = std::stoi(idStr);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("ASIEAFHotPlugHandler", "Invalid focuser ID format: %s. Error: %s", idStr.c_str(), e.what());
        return false;
    }

    int numFocusers = EAFGetNum();
    if (numFocusers < 0)
    {
        return false;
    }

    for (int i = 0; i < numFocusers; ++i)
    {
        int id;
        if (EAFGetID(i, &id) == EAF_SUCCESS && id == focuserID)
        {
            if (EAFOpen(id) == EAF_SUCCESS)
            {
                if (EAFGetProperty(id, &eafInfo) == EAF_SUCCESS)
                {
                    EAFClose(id);
                    return true;
                }
                EAFClose(id);
            }
        }
    }
    return false;
}

std::string ASIEAFHotPlugHandler::getSerialNumberFromID(int focuserID)
{
    EAF_SN serialNumber;
    if (EAFOpen(focuserID) == EAF_SUCCESS)
    {
        if (EAFGetSerialNumber(focuserID, &serialNumber) == EAF_SUCCESS)
        {
            EAFClose(focuserID);
            char snChars[17];
            sprintf(snChars, "%02X%02X%02X%02X%02X%02X%02X%02X",
                    serialNumber.id[0], serialNumber.id[1],
                    serialNumber.id[2], serialNumber.id[3],
                    serialNumber.id[4], serialNumber.id[5],
                    serialNumber.id[6], serialNumber.id[7]);
            snChars[16] = 0;
            return std::string(snChars);
        }
        EAFClose(focuserID); // Ensure focuser is closed even if serial number retrieval fails
    }
    return "";
}

} // namespace INDI
