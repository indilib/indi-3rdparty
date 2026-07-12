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
#include <chrono>
#include <memory>
#include <string>
#include <thread>
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
    bool compactStatus {false};
    bool malformedStatus {false};
    bool statusOnlyResponse {false};
    int statusOnlyPackets {0};
    int flushCount {0};
    enum class LastCommand
    {
        None,
        Status,
        SetPosition
    };
    LastCommand lastCommand {LastCommand::None};
    std::vector<std::vector<uint8_t>> writes;
    std::vector<uint8_t> readQueue;
};

class MockTransport : public AtikEFW::Transport
{
    public:
        explicit MockTransport(MockState state) : state_(std::move(state)) {}

        MockState &state()
        {
            return state_;
        }

        int write(const uint8_t *data, size_t length, unsigned int) override
        {
            std::vector<uint8_t> payload(data, data + length);
            state_.writes.push_back(payload);

            if (payload.size() >= 4 && payload[0] == 0x23 && payload[1] == 0x04)
            {
                state_.lastCommand = MockState::LastCommand::Status;
                prepareStatusResponse();
            }
            else if (payload.size() >= 4 && payload[0] == 0x23 && payload[1] == 0x01)
            {
                state_.lastCommand = MockState::LastCommand::SetPosition;
                state_.currentSlot = payload[2];
                state_.readQueue.clear();
            }

            return static_cast<int>(length);
        }

        int read(uint8_t *data, size_t length, unsigned int) override
        {
            if (data == nullptr || length == 0 || state_.readQueue.empty())
                return 0;

            size_t toCopy = std::min(length, state_.readQueue.size());
            std::copy_n(state_.readQueue.begin(), toCopy, data);
            state_.readQueue.erase(state_.readQueue.begin(), state_.readQueue.begin() + static_cast<std::ptrdiff_t>(toCopy));
            return static_cast<int>(toCopy);
        }

        void flush() override
        {
            state_.flushCount++;
            state_.readQueue.clear();
        }

    private:
        void prepareStatusResponse()
        {
            state_.readQueue.clear();
            if (!state_.statusOk)
                return;

            for (int i = 0; i < state_.statusOnlyPackets; i++)
            {
                state_.readQueue.push_back(0x01);
                state_.readQueue.push_back(0x60);
            }

            if (state_.statusOnlyResponse)
                return;

            if (state_.malformedStatus)
            {
                state_.readQueue.insert(state_.readQueue.end(), {0x01, 0x60, 0x23, 0x04, 0x00, 0x23});
                return;
            }

            uint8_t current = static_cast<uint8_t>(state_.currentSlot);
            uint8_t slots = static_cast<uint8_t>(state_.slotCount);
            if (state_.asciiStatus)
            {
                current = static_cast<uint8_t>('0' + state_.currentSlot);
                slots = static_cast<uint8_t>('0' + state_.slotCount);
            }

            if (state_.compactStatus)
                state_.readQueue.insert(state_.readQueue.end(), {0x01, 0x60, 0x23, current, 0x23, 0x23, slots, 0x23});
            else
                state_.readQueue.insert(state_.readQueue.end(), {0x01, 0x60, 0x23, 0x04, current, slots, 0x23});
        }

        MockState state_;
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

AtikEFW::DeviceDescriptor makeDescriptor(int slots = 5, int currentSlot = 1)
{
    AtikEFW::DeviceDescriptor desc;
    desc.name = "Atik EFW";
    desc.slotCount = slots;
    desc.currentSlot = currentSlot;
    return desc;
}

} // namespace

TEST(AtikEFWDriver, ConnectSelectsAndQueries)
{
    MockState state;
    state.slotCount = 8;
    state.currentSlot = 3;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());

    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 8);
    EXPECT_EQ(wheel.currentFilter(), 3);
    EXPECT_EQ(transport->state().flushCount, 1);

    ASSERT_TRUE(wheel.SelectFilter(5));
    auto &writes = transport->state().writes;
    ASSERT_FALSE(writes.empty());
    EXPECT_EQ(writes.back(), (std::vector<uint8_t> {0x23, 0x01, 5, 0x23}));

    EXPECT_EQ(wheel.QueryFilter(), 5);
}

TEST(AtikEFWDriver, SimulationConnectsWithoutTransport)
{
    TestAtikEFW wheel(makeDescriptor(6, 2));
    ASSERT_TRUE(wheel.initProperties());
    wheel.setSimulation(true);

    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 6);
    EXPECT_EQ(wheel.currentFilter(), 2);

    ASSERT_TRUE(wheel.SelectFilter(4));
    EXPECT_EQ(wheel.QueryFilter(), 4);
    EXPECT_EQ(wheel.currentFilter(), 4);
    EXPECT_TRUE(wheel.Disconnect());
}

TEST(AtikEFWDriver, ParsesAsciiStatusBytes)
{
    MockState state;
    state.slotCount = 8;
    state.currentSlot = 4;
    state.asciiStatus = true;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 8);
    EXPECT_EQ(wheel.currentFilter(), 4);
}

TEST(AtikEFWDriver, PreservesBinarySlotOne)
{
    MockState state;
    state.slotCount = 5;
    state.currentSlot = 1;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 5);
    EXPECT_EQ(wheel.currentFilter(), 1);
}

TEST(AtikEFWDriver, ParsesCapturedCompactStatusResponse)
{
    MockState state;
    state.slotCount = 5;
    state.currentSlot = 1;
    state.compactStatus = true;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 5);
    EXPECT_EQ(wheel.currentFilter(), 1);
}

TEST(AtikEFWDriver, SkipsFtdiStatusOnlyPackets)
{
    MockState state;
    state.slotCount = 7;
    state.currentSlot = 3;
    state.compactStatus = true;
    state.statusOnlyPackets = 2;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 7);
    EXPECT_EQ(wheel.currentFilter(), 3);
}

TEST(AtikEFWDriver, QueryRejectsMalformedStatus)
{
    MockState state;
    state.malformedStatus = true;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.QueryFilter(), -1);
}

TEST(AtikEFWDriver, ConnectUsesHintsWithoutStatusResponse)
{
    MockState state;
    state.statusOk = false;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());
    EXPECT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 5);
    EXPECT_EQ(wheel.currentFilter(), 1);
}

TEST(AtikEFWDriver, ConnectUsesHintsForFtdiStatusOnlyResponse)
{
    MockState state;
    state.statusOnlyPackets = 16;
    state.statusOnlyResponse = true;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());
    EXPECT_TRUE(wheel.Connect());
    EXPECT_EQ(wheel.maxSlots(), 5);
    EXPECT_EQ(wheel.currentFilter(), 1);
}

TEST(AtikEFWDriver, CompletesMoveWhenStatusRemainsUnavailable)
{
    MockState state;
    state.slotCount = 5;
    state.currentSlot = 1;

    auto transport = std::make_shared<MockTransport>(state);
    TestAtikEFW wheel(makeDescriptor(), transport);
    ASSERT_TRUE(wheel.initProperties());
    ASSERT_TRUE(wheel.Connect());
    ASSERT_TRUE(wheel.SelectFilter(4));

    transport->state().statusOk = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1600));

    EXPECT_EQ(wheel.QueryFilter(), 4);
    EXPECT_EQ(wheel.currentFilter(), 4);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
