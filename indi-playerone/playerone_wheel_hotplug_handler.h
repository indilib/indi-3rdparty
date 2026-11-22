/*
 PlayerOne filter wheel hot-plug handler.
 Distributed under the GNU General Public License, version 2 or later.
 */
#pragma once

#include <hotplugcapabledevice.h>
#include <inditimer.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

class POAWHEEL;

class PlayerOneWheelHotPlugHandler : public INDI::HotPlugCapableDevice
{
    public:
        PlayerOneWheelHotPlugHandler();
        ~PlayerOneWheelHotPlugHandler() override;
        std::vector<std::string> discoverConnectedDeviceIdentifiers() override;
        std::shared_ptr<INDI::DefaultDevice> createDevice(const std::string &identifier) override;
        void destroyDevice(std::shared_ptr<INDI::DefaultDevice> device) override;
        const std::map<std::string, std::shared_ptr<INDI::DefaultDevice>> &getManagedDevices() const override;

    private:
        void retryDiscovery();
        std::map<std::string, std::shared_ptr<INDI::DefaultDevice>> mDevices;
        INDI::Timer mRetryTimer;
        bool mIncomplete {false};
};
