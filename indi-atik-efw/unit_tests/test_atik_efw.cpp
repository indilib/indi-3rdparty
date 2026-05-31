/*
 Atik Electronic Filter Wheel INDI Driver

 Copyright (C) 2025 Eric Dejouhanet
 Based on Python code by duke164.

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

#include "atik_efw.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct MockState
{
    int slotCount {5};
    int currentSlot {1};
    bool statusOk {true};
    bool asciiStatus {false};
    bool malformedStatus {false};
    bool resetOk {true};
    int openCount {0};
    int resetCount {0};
    int detachCount {0};
    int setConfigurationCount {0};
    int claimInterfaceCount {0};
    int controlTransferCount {0};
    enum class LastCommand
    {
        None,
        Status,
        SetPosition
    };
    LastCommand lastCommand {LastCommand::None};
    std::vector<std::vector<uint8_t>> writes;
};

class MockHandle : public AtikEfwUsb::DeviceHandle
{
    public:
        explicit MockHandle(MockState &state) : state_(state) {}

        bool reset() override
        {
            state_.resetCount++;
            return state_.resetOk;
        }

        bool detachKernelDriver(int) override
        {
            state_.detachCount++;
            return true;
        }

        bool setConfiguration(int) override
        {
            state_.setConfigurationCount++;
            return true;
        }

        bool claimInterface(int) override
        {
            state_.claimInterfaceCount++;
            return true;
        }

        int controlTransfer(uint8_t, uint8_t, uint16_t, uint16_t, unsigned char *, uint16_t, unsigned int) override
        {
            state_.controlTransferCount++;
            return 0;
        }

        int write(uint8_t endpoint, const unsigned char *data, int length, unsigned int) override
        {
            if (endpoint != 0x02)
                return -1;

            std::vector<uint8_t> payload(data, data + length);
            state_.writes.push_back(payload);

            if (payload.size() >= 4 && payload[0] == 0x23 && payload[1] == 0x04)
            {
                state_.lastCommand = MockState::LastCommand::Status;
            }
            else if (payload.size() >= 4 && payload[0] == 0x23 && payload[1] == 0x01)
            {
                state_.lastCommand = MockState::LastCommand::SetPosition;
                state_.currentSlot = payload[2];
            }

            return length;
        }

        int read(uint8_t endpoint, unsigned char *data, int length, unsigned int) override
        {
            if (endpoint != 0x81)
                return -1;

            if (!state_.statusOk || state_.lastCommand != MockState::LastCommand::Status)
                return 0;

            if (state_.malformedStatus)
            {
                std::vector<uint8_t> response = {0x60, 0x01, 0x23, 0x04, 0x00, 0x23};
                int toCopy = std::min(length, static_cast<int>(response.size()));
                for (int i = 0; i < toCopy; i++)
                    data[i] = response[i];
                return toCopy;
            }

            uint8_t current = static_cast<uint8_t>(state_.currentSlot);
            uint8_t slots = static_cast<uint8_t>(state_.slotCount);
            if (state_.asciiStatus)
            {
                current = static_cast<uint8_t>('0' + state_.currentSlot);
                slots = static_cast<uint8_t>('0' + state_.slotCount);
            }

            std::vector<uint8_t> response = {0x60, 0x01, 0x23, 0x04,
                                             current, slots, 0x23};

            int toCopy = std::min(length, static_cast<int>(response.size()));
            for (int i = 0; i < toCopy; i++)
                data[i] = response[i];

            return toCopy;
        }

    private:
        MockState &state_;
};

class MockBackend : public AtikEfwUsb::Backend
{
    public:
        void addDevice(AtikEfwUsb::DeviceInfo info, MockState state)
        {
            devices_.push_back(info);
            states_[keyFor(info)] = std::move(state);
        }

        MockState &stateFor(const AtikEfwUsb::DeviceInfo &info)
        {
            return states_.at(keyFor(info));
        }

        std::vector<AtikEfwUsb::DeviceInfo> listDevices(uint16_t vendorId, uint16_t productId) override
        {
            std::vector<AtikEfwUsb::DeviceInfo> matches;
            for (const auto &device : devices_)
            {
                if (device.vendorId == vendorId && device.productId == productId)
                    matches.push_back(device);
            }
            return matches;
        }

        std::unique_ptr<AtikEfwUsb::DeviceHandle> open(const AtikEfwUsb::DeviceInfo &info) override
        {
            openAttempts_++;
            auto it = states_.find(keyFor(info));
            if (it == states_.end())
                return nullptr;
            it->second.openCount++;
            return std::make_unique<MockHandle>(it->second);
        }

        int openAttempts() const
        {
            return openAttempts_;
        }

    private:
        static std::string keyFor(const AtikEfwUsb::DeviceInfo &info)
        {
            return std::to_string(info.bus) + ":" + std::to_string(info.address);
        }

        std::vector<AtikEfwUsb::DeviceInfo> devices_;
        std::map<std::string, MockState> states_;
        int openAttempts_ {0};
};

class TestAtikEFW : public AtikEFW
{
    public:
        using AtikEFW::AtikEFW;
        using AtikEFW::initProperties;
        using AtikEFW::QueryFilter;
        using AtikEFW::SelectFilter;
        using INDI::DefaultDevice::setSimulation;

        int maxSlots() const
        {
            return static_cast<int>(FilterSlotNP[0].getMax());
        }

        int currentFilter() const
        {
            return CurrentFilter;
        }
};

AtikEfwUsb::DeviceInfo makeDevice(uint8_t bus, uint8_t address, std::vector<uint8_t> ports)
{
    AtikEfwUsb::DeviceInfo info;
    info.vendorId = 0x0403;
    info.productId = 0xaf01;
    info.bus = bus;
    info.address = address;
    info.ports = std::move(ports);
    return info;
}

} // namespace

TEST(AtikEFWDriver, EnumerateUsesHandshake)
{
    MockBackend backend;

    MockState okState;
    okState.slotCount = 7;
    okState.currentSlot = 2;
    okState.statusOk = true;

    MockState badState;
    badState.statusOk = false;

    auto deviceOk = makeDevice(1, 5, {3});
    auto deviceBad = makeDevice(1, 6, {4});

    backend.addDevice(deviceOk, okState);
    backend.addDevice(deviceBad, badState);

    auto devices = AtikEFW::Enumerate(backend);
    ASSERT_EQ(devices.size(), 2u);
    EXPECT_EQ(devices[0].slotCount, 7);
    EXPECT_EQ(devices[0].currentSlot, 2);
    EXPECT_TRUE(devices[0].slotCountKnown);
    EXPECT_NE(devices[0].name.find("1-3"), std::string::npos);

    EXPECT_EQ(devices[1].slotCount, 5);
    EXPECT_EQ(devices[1].currentSlot, 1);
    EXPECT_FALSE(devices[1].slotCountKnown);
    EXPECT_NE(devices[1].name.find("1-4"), std::string::npos);
}

TEST(AtikEFWDriver, ConnectSelectsAndQueries)
{
    MockBackend backend;
    MockState state;
    state.slotCount = 8;
    state.currentSlot = 3;
    state.statusOk = true;

    auto device = makeDevice(2, 7, {2, 1});
    backend.addDevice(device, state);

    auto devices = AtikEFW::Enumerate(backend);
    ASSERT_EQ(devices.size(), 1u);

    TestAtikEFW wheel(devices[0], backend);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 8);
    EXPECT_EQ(wheel.currentFilter(), 3);
    EXPECT_EQ(backend.stateFor(device).resetCount, 1);
    EXPECT_EQ(backend.stateFor(device).openCount, 3);
    EXPECT_EQ(backend.stateFor(device).setConfigurationCount, 2);
    EXPECT_EQ(backend.stateFor(device).claimInterfaceCount, 2);
    EXPECT_EQ(backend.stateFor(device).controlTransferCount, 6);

    ASSERT_TRUE(wheel.SelectFilter(5));
    auto &writes = backend.stateFor(device).writes;
    ASSERT_FALSE(writes.empty());
    EXPECT_EQ(writes.back(), (std::vector<uint8_t>{0x23, 0x01, 5, 0x23}));

    EXPECT_EQ(wheel.QueryFilter(), 5);
}

TEST(AtikEFWDriver, SimulationConnectsWithoutUsb)
{
    MockBackend backend;
    AtikEFW::DeviceDescriptor desc;
    desc.name = "Atik EFW";
    desc.slotCount = 6;
    desc.currentSlot = 2;

    TestAtikEFW wheel(desc, backend);
    ASSERT_TRUE(wheel.initProperties());
    wheel.setSimulation(true);

    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(backend.openAttempts(), 0);
    EXPECT_EQ(wheel.maxSlots(), 6);
    EXPECT_EQ(wheel.currentFilter(), 2);

    ASSERT_TRUE(wheel.SelectFilter(4));
    EXPECT_EQ(wheel.QueryFilter(), 4);
    EXPECT_EQ(wheel.currentFilter(), 4);
    EXPECT_TRUE(wheel.Disconnect());
}

TEST(AtikEFWDriver, ParsesAsciiStatusBytes)
{
    MockBackend backend;
    MockState state;
    state.slotCount = 8;
    state.currentSlot = 4;
    state.asciiStatus = true;

    auto device = makeDevice(3, 8, {5});
    backend.addDevice(device, state);

    auto devices = AtikEFW::Enumerate(backend);
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0].slotCount, 8);
    EXPECT_EQ(devices[0].currentSlot, 4);
    EXPECT_TRUE(devices[0].slotCountKnown);

    TestAtikEFW wheel(devices[0], backend);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 8);
    EXPECT_EQ(wheel.currentFilter(), 4);
}

TEST(AtikEFWDriver, PreservesBinarySlotOneWhenStrippingFtdiStatus)
{
    MockBackend backend;
    MockState state;
    state.slotCount = 5;
    state.currentSlot = 1;

    auto device = makeDevice(3, 11, {8});
    backend.addDevice(device, state);

    auto devices = AtikEFW::Enumerate(backend);
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0].slotCount, 5);
    EXPECT_EQ(devices[0].currentSlot, 1);
    EXPECT_TRUE(devices[0].slotCountKnown);
}

TEST(AtikEFWDriver, QueryRejectsMalformedStatus)
{
    MockBackend backend;
    MockState state;
    state.malformedStatus = true;

    auto device = makeDevice(4, 9, {6});
    backend.addDevice(device, state);

    auto devices = AtikEFW::Enumerate(backend);
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_FALSE(devices[0].slotCountKnown);
    EXPECT_EQ(devices[0].slotCount, 5);
    EXPECT_EQ(devices[0].currentSlot, 1);

    TestAtikEFW wheel(devices[0], backend);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.QueryFilter(), -1);
}

TEST(AtikEFWDriver, ConnectFailsWithoutStatusResponse)
{
    MockBackend backend;
    MockState state;
    state.statusOk = false;

    auto device = makeDevice(5, 10, {7});
    backend.addDevice(device, state);

    auto devices = AtikEFW::Enumerate(backend);
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_FALSE(devices[0].slotCountKnown);

    TestAtikEFW wheel(devices[0], backend);
    ASSERT_TRUE(wheel.initProperties());
    EXPECT_FALSE(wheel.Connect());
    EXPECT_EQ(backend.stateFor(device).resetCount, 1);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
