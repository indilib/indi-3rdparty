/*
    Toupbase CCD Hot Plug Handler Class Header File

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

#include <hotplugcapabledevice.h> // Correct include for HotPlugCapableDevice
#include <memory>                 // For std::shared_ptr
#include <map>                    // For std::map
#include <string>                 // For std::string
#include <vector>                 // For std::vector

#include "indi_toupbase.h"

class ToupbaseCCDHotPlugHandler : public INDI::HotPlugCapableDevice
{
    public:
        ToupbaseCCDHotPlugHandler();
        ~ToupbaseCCDHotPlugHandler() override;

        std::vector<std::string> discoverConnectedDeviceIdentifiers() override;
        std::shared_ptr<INDI::DefaultDevice> createDevice(const std::string& identifier) override;
        void destroyDevice(std::shared_ptr<INDI::DefaultDevice> device) override;
        const std::map<std::string, std::shared_ptr<INDI::DefaultDevice>> &getManagedDevices() const override;

    private:
        // Internal storage for managed ToupbaseCCD devices
        std::vector<std::shared_ptr<ToupBase>> m_internalCameras;
        // A mutable map to return for getManagedDevices, constructed on demand
        mutable std::map<std::string, std::shared_ptr<INDI::DefaultDevice>> m_managedDevicesView;

        // Helper to get camera info by ID
        bool getCameraInfoByCameraID(const std::string& cameraIDStr, XP(DeviceV2)& cameraInfo);

        // Map to store device info with stable addresses (heap-allocated)
        // Key: device ID string, Value: shared pointer to device info
        // This prevents dangling pointers when enumeration updates the device list
        std::map<std::string, std::shared_ptr<XP(DeviceV2)>> m_connectedDevices;
};
