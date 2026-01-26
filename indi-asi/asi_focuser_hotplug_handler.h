/*
    ASI EAF Hot Plug Handler Class Header File

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

#pragma once

#include <hotplugcapabledevice.h>
#include "asi_focuser.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace INDI
{

class ASIEAFHotPlugHandler : public HotPlugCapableDevice
{
    public:
        ASIEAFHotPlugHandler();
        ~ASIEAFHotPlugHandler() override;

        /**
         * @brief Discovers currently connected ASI EAF focusers.
         * @return A vector of unique string identifiers for connected ASI EAF focusers.
         */
        std::vector<std::string> discoverConnectedDeviceIdentifiers() override;

        /**
         * @brief Factory method to create a new ASIEAF instance.
         * @param identifier The unique string identifier of the ASI EAF focuser to create.
         * @return A shared pointer to the newly created ASIEAF instance.
         */
        std::shared_ptr<DefaultDevice> createDevice(const std::string& identifier) override;

        /**
         * @brief Destroys an ASIEAF instance and performs driver-specific cleanup.
         * @param device A shared pointer to the device to destroy.
         */
        void destroyDevice(std::shared_ptr<DefaultDevice> device) override;

        /**
         * @brief Provides a unified map view of currently managed ASIEAF devices.
         * @return A const reference to a map of managed ASIEAF devices, keyed by their unique string identifiers.
         */
        const std::map<std::string, std::shared_ptr<DefaultDevice>> &getManagedDevices() const override;

    private:
        // Internal storage for managed ASI EAF devices
        std::deque<std::shared_ptr<ASIEAF>> m_internalFocusers;
        // A map view for getManagedDevices() to satisfy the interface
        mutable std::map<std::string, std::shared_ptr<DefaultDevice>> m_managedDevicesView;

        // Helper to get focuser info by ID (converted to string)
        static bool getEAFInfoByID(const std::string& idStr, EAF_INFO& eafInfo);
        // Helper to get serial number from focuser ID (if supported)
        static std::string getSerialNumberFromID(int id);
};

} // namespace INDI
