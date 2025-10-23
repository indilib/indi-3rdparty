/*
    SVBony CCD Hot Plug Handler Class Header File

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
#include "svbony_ccd.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace INDI
{

class SVBONYCCDHotPlugHandler : public HotPlugCapableDevice
{
    public:
        SVBONYCCDHotPlugHandler();
        ~SVBONYCCDHotPlugHandler() override;

        /**
         * @brief Discovers currently connected SVBony cameras.
         * @return A vector of unique string identifiers for connected SVBony cameras.
         */
        std::vector<std::string> discoverConnectedDeviceIdentifiers() override;

        /**
         * @brief Factory method to create a new SVBONYCCD instance.
         * @param identifier The unique string identifier of the SVBony camera to create.
         * @return A shared pointer to the newly created SVBONYCCD instance.
         */
        std::shared_ptr<DefaultDevice> createDevice(const std::string& identifier) override;

        /**
         * @brief Destroys an SVBONYCCD instance and performs driver-specific cleanup.
         * @param device A shared pointer to the device to destroy.
         */
        void destroyDevice(std::shared_ptr<DefaultDevice> device) override;

        /**
         * @brief Provides a unified map view of currently managed SVBONYCCD devices.
         * @return A const reference to a map of managed SVBONYCCD devices, keyed by their unique string identifiers.
         */
        const std::map<std::string, std::shared_ptr<DefaultDevice>>& getManagedDevices() const override;

    private:
        // Internal storage for managed SVBony CCD devices
        std::deque<std::shared_ptr<SVBONYCCD>> m_internalCameras;
        // A map view for getManagedDevices() to satisfy the interface
        mutable std::map<std::string, std::shared_ptr<DefaultDevice>> m_managedDevicesView;

        // Helper to get camera info by CameraID (converted to string)
        static bool getCameraInfoByCameraID(const std::string& cameraIDStr, SVB_CAMERA_INFO& cameraInfo);
        // Helper to get serial number from camera ID (if supported)
        static std::string getSerialNumberFromCameraID(int cameraID);
};

} // namespace INDI
