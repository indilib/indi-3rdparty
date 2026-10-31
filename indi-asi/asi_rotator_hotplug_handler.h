/*
    ZWO CAA Rotator Hot Plug Handler Class Header File

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
#include "asi_rotator.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace INDI
{

class ASICAAHotPlugHandler : public HotPlugCapableDevice
{
    public:
        ASICAAHotPlugHandler();
        ~ASICAAHotPlugHandler() override;

        /**
         * @brief Discovers currently connected ZWO CAA rotators.
         * @return A vector of unique string identifiers for connected ZWO CAA rotators.
         */
        std::vector<std::string> discoverConnectedDeviceIdentifiers() override;

        /**
         * @brief Factory method to create a new ASICAA instance.
         * @param identifier The unique string identifier of the ZWO CAA rotator to create.
         * @return A shared pointer to the newly created ASICAA instance.
         */
        std::shared_ptr<DefaultDevice> createDevice(const std::string& identifier) override;

        /**
         * @brief Destroys an ASICAA instance and performs driver-specific cleanup.
         * @param device A shared pointer to the device to destroy.
         */
        void destroyDevice(std::shared_ptr<DefaultDevice> device) override;

        /**
         * @brief Provides a unified map view of currently managed ASICAA devices.
         * @return A const reference to a map of managed ASICAA devices, keyed by their unique string identifiers.
         */
        const std::map<std::string, std::shared_ptr<DefaultDevice>> &getManagedDevices() const override;

    private:
        // Internal storage for managed ZWO CAA devices
        std::deque<std::shared_ptr<ASICAA>> m_internalRotators;
        // A map view for getManagedDevices() to satisfy the interface
        mutable std::map<std::string, std::shared_ptr<DefaultDevice>> m_managedDevicesView;

        // Helper to read the serial number for a rotator ID (best effort, may be empty)
        static std::string getSerialNumberFromID(int id);
};

} // namespace INDI
