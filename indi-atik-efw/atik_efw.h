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

#pragma once

#include "atik_efw_usb.h"

#include <indifilterwheel.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

class AtikEFW : public INDI::FilterWheel
{
    public:
        struct DeviceDescriptor
        {
            AtikEfwUsb::DeviceInfo info;
            std::string name;
            int slotCount {0};
            int currentSlot {0};
            bool slotCountKnown {false};
        };

        AtikEFW(const DeviceDescriptor &desc, AtikEfwUsb::Backend &backend);
        ~AtikEFW() override;

        const char *getDefaultName() override;

        bool Connect() override;
        bool Disconnect() override;

    protected:
        bool initProperties() override;
        bool updateProperties() override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool saveConfigItems(FILE *fp) override;

        int QueryFilter() override;
        bool SelectFilter(int targetFilter) override;
        void TimerHit() override;

    public:
        static std::vector<DeviceDescriptor> Enumerate(AtikEfwUsb::Backend &backend);

    private:
        bool openHandle();
        bool resetAndReopenHandle();
        bool configureDevice();
        bool sendStatus(bool requireParse, int *slotCount, int *currentSlot);
        bool sendCommand(const std::vector<uint8_t> &command);
        void applySlotCount(int slots, bool updateProperty);

        AtikEfwUsb::Backend &backend_;
        AtikEfwUsb::DeviceInfo deviceInfo_;
        std::unique_ptr<AtikEfwUsb::DeviceHandle> handle_;
        int slotCountHint_ {0};
        int currentSlotHint_ {0};
        bool movementPending_ {false};
        std::chrono::steady_clock::time_point movementStartedAt_;

        INDI::PropertyNumber SlotCountNP {1};
};
