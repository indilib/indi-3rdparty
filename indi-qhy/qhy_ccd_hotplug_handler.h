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

#pragma once

#include <hotplugcapabledevice.h>
#include <memory>
#include <map>
#include <string>
#include <vector>

#include "qhy_ccd.h" // For QHYCCD

class QHYCCDHotPlugHandler : public INDI::HotPlugCapableDevice
{
    public:
        QHYCCDHotPlugHandler();
        ~QHYCCDHotPlugHandler() override;

        std::vector<std::string> discoverConnectedDeviceIdentifiers() override;
        std::shared_ptr<INDI::DefaultDevice> createDevice(const std::string& identifier) override;
        void destroyDevice(std::shared_ptr<INDI::DefaultDevice> device) override;
        const std::map<std::string, std::shared_ptr<INDI::DefaultDevice>>& getManagedDevices() const override;

    private:
        // Internal storage for managed QHYCCD devices
        std::vector<std::shared_ptr<QHYCCD>> m_internalCameras;
        // A mutable map to return for getManagedDevices, constructed on demand
        mutable std::map<std::string, std::shared_ptr<INDI::DefaultDevice>> m_managedDevicesView;
        // Vector to store IDs of currently connected devices
        std::vector<std::string> m_connectedDeviceIDs;
};
