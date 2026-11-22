/*
 PlayerOne filter wheel hot-plug handler.
 Distributed under the GNU General Public License, version 2 or later.
 */
#include "playerone_wheel_hotplug_handler.h"
#include "playerone_wheel.h"

#include <indilogger.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

PlayerOneWheelHotPlugHandler::PlayerOneWheelHotPlugHandler()
{
    mRetryTimer.setInterval(2000);
    mRetryTimer.callOnTimeout([this]()
    {
        retryDiscovery();
    });
}

PlayerOneWheelHotPlugHandler::~PlayerOneWheelHotPlugHandler()
{
    mRetryTimer.stop();
    for (auto &entry : mDevices)
    {
        auto wheel = std::static_pointer_cast<POAWHEEL>(entry.second);
        wheel->unplug();
        wheel->deleteProperty(nullptr);
    }
}

std::vector<std::string> PlayerOneWheelHotPlugHandler::discoverConnectedDeviceIdentifiers()
{
    std::vector<std::string> identifiers;
    const int count = POAGetPWCount();
    bool incomplete = count < 0;
    if (incomplete)
        LOGF_ERROR("POAGetPWCount failed: %d", count);

    for (int i = 0; i < count; ++i)
    {
        PWProperties info {};
        const PWErrors result = POAGetPWProperties(i, &info);
        if (result == PW_OK)
            identifiers.push_back(std::to_string(info.Handle));
        else
        {
            LOGF_WARN("POAGetPWProperties(%d) failed: %d", i, result);
            incomplete = true;
        }
    }

    // A partial enumeration must not be interpreted as physical removal.
    if (incomplete)
    {
        mRetryTimer.start();
        for (const auto &entry : mDevices)
            if (std::find(identifiers.begin(), identifiers.end(), entry.first) == identifiers.end())
                identifiers.push_back(entry.first);
    }

    mIncomplete = incomplete;

    return identifiers;
}

std::shared_ptr<INDI::DefaultDevice> PlayerOneWheelHotPlugHandler::createDevice(const std::string &identifier)
{
    const auto existing = mDevices.find(identifier);
    if (existing != mDevices.end())
        return existing->second;

    const int count = POAGetPWCount();
    for (int i = 0; i < count; ++i)
    {
        PWProperties info {};
        const PWErrors result = POAGetPWProperties(i, &info);
        if (result != PW_OK)
        {
            LOGF_WARN("POAGetPWProperties(%d) failed: %d", i, result);
            continue;
        }
        if (std::to_string(info.Handle) != identifier)
            continue;

        info.Name[sizeof(info.Name) - 1] = '\0';
        info.SN[sizeof(info.SN) - 1] = '\0';
        if (strcasestr(info.Name, "error") != nullptr || info.Name[0] == '\0')
        {
            LOGF_WARN("Invalid PlayerOne wheel name '%s' for handle %d", info.Name, info.Handle);
            mRetryTimer.start();
            return nullptr;
        }

        std::string baseName = "PlayerOne " + std::string(info.Name);
        const char *envDev = std::getenv("INDIDEV");
        if (count == 1 && mDevices.empty() && envDev && *envDev)
            baseName = envDev;
        else if (count > 1)
            baseName += " " + std::to_string(i);

        std::string name = baseName;
        unsigned int suffix = 1;
        auto nameInUse = [&](const std::string & candidate)
        {
            return std::any_of(mDevices.begin(), mDevices.end(), [&](const auto & entry)
            {
                return candidate == entry.second->getDeviceName();
            });
        };
        while (nameInUse(name))
            name = baseName + " " + std::to_string(suffix++);

        auto wheel = std::make_shared<POAWHEEL>(info, name.c_str());
        if (nameInUse(wheel->getDeviceName()))
        {
            LOGF_ERROR("PlayerOne wheel name '%s' is already in use.", wheel->getDeviceName());
            mRetryTimer.start();
            return nullptr;
        }
        mDevices.emplace(identifier, wheel);
        LOGF_INFO("PlayerOne wheel %s discovered (handle %d).", wheel->getDeviceName(), info.Handle);
        return wheel;
    }
    LOGF_ERROR("PlayerOne wheel %s was not found during creation.", identifier.c_str());
    mRetryTimer.start();
    return nullptr;
}

void PlayerOneWheelHotPlugHandler::retryDiscovery()
{
    const auto identifiers = discoverConnectedDeviceIdentifiers();
    const auto devices = mDevices;
    for (const auto &entry : devices)
        if (std::find(identifiers.begin(), identifiers.end(), entry.first) == identifiers.end())
            destroyDevice(entry.second);

    bool retry = mIncomplete;
    for (const auto &identifier : identifiers)
    {
        if (mDevices.count(identifier) == 0)
        {
            auto device = createDevice(identifier);
            if (device)
                device->ISGetProperties(nullptr);
            else
                retry = true;
        }
    }
    if (!retry)
        mRetryTimer.stop();
}

void PlayerOneWheelHotPlugHandler::destroyDevice(std::shared_ptr<INDI::DefaultDevice> device)
{
    auto wheel = std::dynamic_pointer_cast<POAWHEEL>(device);
    if (!wheel)
    {
        LOG_ERROR("Cannot remove a non-PlayerOne wheel.");
        return;
    }
    wheel->unplug();
    wheel->deleteProperty(nullptr);
    mDevices.erase(std::to_string(wheel->getHandle()));
}

const std::map<std::string, std::shared_ptr<INDI::DefaultDevice>> &
PlayerOneWheelHotPlugHandler::getManagedDevices() const
{
    return mDevices;
}
