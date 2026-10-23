/*
    ZWO CAA Rotator Hot Plug Handler Class Source File

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

#include "asi_rotator_hotplug_handler.h"
#include "indilogger.h"   // For LOG_INFO, LOG_DEBUG, etc.
#include <algorithm>      // For std::remove_if, std::find_if
#include <CAA_API.h>      // For CAA SDK functions
#include <cstdio>         // For snprintf
#include <string>         // For std::string, std::to_string
#include <vector>         // For std::vector
#include <map>            // For std::map
#include <memory>         // For std::shared_ptr
#include <stdexcept>      // For std::invalid_argument, std::out_of_range

namespace INDI
{

ASICAAHotPlugHandler::ASICAAHotPlugHandler()
{
    LOG_DEBUG("HotPlugManager: ASICAAHotPlugHandler initialized.");
}

ASICAAHotPlugHandler::~ASICAAHotPlugHandler()
{
    // Clean up any remaining devices, deleting their properties from the server.
    for (const auto& device : m_internalRotators)
        device->deleteProperty(nullptr); // Delete all properties for this device

    m_internalRotators.clear();
    m_managedDevicesView.clear();
    LOG_DEBUG("HotPlugManager: ASICAAHotPlugHandler shut down.");
}

std::vector<std::string> ASICAAHotPlugHandler::discoverConnectedDeviceIdentifiers()
{
    std::vector<std::string> identifiers;

    // CAAGetNum() refreshes the SDK's device list. We deliberately do NOT call
    // CAAOpen() here: opening claims the USB interface, and doing so during
    // discovery would collide with the already-connected driver instance
    // (LIBUSB_ERROR_BUSY). CAAGetID() only needs the enumeration.
    int numRotators = CAAGetNum();
    if (numRotators < 0)
    {
        LOG_ERROR("HotPlugManager: CAAGetNum returned an error.");
        return identifiers;
    }

    for (int i = 0; i < numRotators; ++i)
    {
        int id = -1;
        if (CAAGetID(i, &id) == CAA_SUCCESS)
        {
            identifiers.push_back(std::to_string(id));
            LOGF_DEBUG("HotPlugManager: Discovered ZWO CAA with ID: %d", id);
        }
        else
        {
            LOGF_WARN("HotPlugManager: Failed to get rotator ID for index %d.", i);
        }
    }
    return identifiers;
}

std::shared_ptr<DefaultDevice> ASICAAHotPlugHandler::createDevice(const std::string& identifier)
{
    int rotatorID;
    try
    {
        rotatorID = std::stoi(identifier);
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("HotPlugManager: Invalid identifier format for rotator ID: %s. Error: %s",
                   identifier.c_str(), e.what());
        return nullptr;
    }

    // Resolve the CAA_INFO for this ID from the current enumeration. As in
    // discovery, no CAAOpen() is used here so we never claim the interface
    // before the device instance connects.
    CAA_INFO info;
    bool foundRotator = false;
    int numRotators = CAAGetNum();
    for (int i = 0; i < numRotators; ++i)
    {
        int id = -1;
        if (CAAGetID(i, &id) == CAA_SUCCESS && id == rotatorID)
        {
            if (CAAGetProperty(id, &info) == CAA_SUCCESS)
                foundRotator = true;
            break;
        }
    }

    if (!foundRotator)
    {
        LOGF_ERROR("HotPlugManager: Failed to get rotator info for ID: %d", rotatorID);
        return nullptr;
    }

    // If a device with this ID is already managed, return it (do not duplicate).
    for (const auto& device : m_internalRotators)
    {
        if (device->getCAAInfo().ID == rotatorID)
        {
            LOGF_DEBUG("HotPlugManager: Device with rotator ID %d already managed, not creating new.", rotatorID);
            return device;
        }
    }

    // Generate a unique device name. Preserve the historical "ZWO CAA <Name>"
    // scheme (e.g. "ZWO CAA CAA") so existing saved configurations keep working,
    // appending a counter only when more than one identical device is present.
    std::string baseName = std::string("ZWO CAA ") + info.Name;
    std::string uniqueName = baseName;
    int index = 0;
    bool nameExists = true;
    while (nameExists)
    {
        nameExists = false;
        for (const auto& device : m_internalRotators)
        {
            if (uniqueName == device->getDeviceName())
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

    std::string serialNumber = getSerialNumberFromID(rotatorID);

    std::shared_ptr<ASICAA> newDevice = std::make_shared<ASICAA>(info, uniqueName.c_str(), serialNumber);
    m_internalRotators.push_back(newDevice);
    LOGF_INFO("HotPlugManager: Created new ASICAA device: %s (ID: %d)", uniqueName.c_str(), rotatorID);
    return newDevice;
}

void ASICAAHotPlugHandler::destroyDevice(std::shared_ptr<DefaultDevice> device)
{
    std::shared_ptr<ASICAA> asiCaa = std::dynamic_pointer_cast<ASICAA>(device);
    if (!asiCaa)
    {
        LOG_ERROR("HotPlugManager: Attempted to destroy a non-ASICAA device with ASICAAHotPlugHandler.");
        return;
    }

    // Delete properties from the INDI server (this also handles disconnection).
    asiCaa->deleteProperty(nullptr);

    // Remove from internal storage.
    auto it = std::remove_if(m_internalRotators.begin(), m_internalRotators.end(),
                             [&](const std::shared_ptr<ASICAA> &d)
    {
        return d == asiCaa;
    });

    if (it != m_internalRotators.end())
    {
        LOGF_INFO("HotPlugManager: Destroyed ASICAA device: %s (ID: %d)",
                  asiCaa->getDeviceName(), asiCaa->getCAAInfo().ID);
        m_internalRotators.erase(it, m_internalRotators.end());
    }
    else
    {
        LOGF_WARN("HotPlugManager: Attempted to destroy ASICAA device %s not found in managed list.",
                  asiCaa->getDeviceName());
    }
}

const std::map<std::string, std::shared_ptr<DefaultDevice>> &ASICAAHotPlugHandler::getManagedDevices() const
{
    // Dynamically construct the map view from m_internalRotators.
    m_managedDevicesView.clear();
    for (const auto& device : m_internalRotators)
        m_managedDevicesView[std::to_string(device->getCAAInfo().ID)] = device;
    return m_managedDevicesView;
}

std::string ASICAAHotPlugHandler::getSerialNumberFromID(int rotatorID)
{
    // Best effort: the serial number may require the device to be open, so a
    // failure here is non-fatal. The connected device also reads the serial in
    // updateProperties(); an empty value simply falls back to the unique name.
    CAA_SN serialNumber;
    if (CAAGetSerialNumber(rotatorID, &serialNumber) == CAA_SUCCESS)
    {
        char snChars[17];
        snprintf(snChars, sizeof(snChars), "%02x%02x%02x%02x%02x%02x%02x%02x",
                 serialNumber.id[0], serialNumber.id[1], serialNumber.id[2], serialNumber.id[3],
                 serialNumber.id[4], serialNumber.id[5], serialNumber.id[6], serialNumber.id[7]);
        snChars[16] = 0;
        return std::string(snChars);
    }
    return "";
}

} // namespace INDI
