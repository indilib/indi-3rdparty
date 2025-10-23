/*
    PlayerOne CCD Hot Plug Handler Class Header File

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
#include "playerone_ccd.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

class PlayerOneCCDHotPlugHandler : public INDI::HotPlugCapableDevice
{
    public:
        PlayerOneCCDHotPlugHandler();
        ~PlayerOneCCDHotPlugHandler() override;

        /**
         * @brief Discovers currently connected PlayerOne cameras.
         * @return A vector of unique string identifiers for connected PlayerOne cameras.
         */
        std::vector<std::string> discoverConnectedDeviceIdentifiers() override;

        /**
         * @brief Factory method to create a new PlayerOneCCD instance.
         * @param identifier The unique string identifier of the PlayerOne camera to create.
         * @return A shared pointer to the newly created PlayerOneCCD instance.
         */
        std::shared_ptr<INDI::DefaultDevice> createDevice(const std::string& identifier) override;

        /**
         * @brief Destroys an PlayerOneCCD instance and performs driver-specific cleanup.
         * @param device A shared pointer to the device to destroy.
         */
        void destroyDevice(std::shared_ptr<INDI::DefaultDevice> device) override;

        /**
         * @brief Provides a unified map view of currently managed PlayerOneCCD devices.
         * @return A const reference to a map of managed PlayerOneCCD devices, keyed by their unique string identifiers.
         */
        const std::map<std::string, std::shared_ptr<INDI::DefaultDevice>>& getManagedDevices() const override;

    private:
        // Internal storage for managed PlayerOne CCD devices
        std::deque<std::shared_ptr<POACCD>> m_internalCameras;
        // A map view for getManagedDevices() to satisfy the interface
        mutable std::map<std::string, std::shared_ptr<INDI::DefaultDevice>> m_managedDevicesView;

        // Helper to get camera info by CameraID (converted to string)
        static bool getCameraInfoByCameraID(const std::string& cameraIDStr, POACameraProperties& cameraInfo);
        // Helper to get serial number from camera ID (if supported)
        static std::string getSerialNumberFromCameraID(int cameraID);
};
