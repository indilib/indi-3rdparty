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

#include <indifilterwheel.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AtikEFW : public INDI::FilterWheel
{
    public:
        class Transport
        {
            public:
                virtual ~Transport() = default;

                virtual int write(const uint8_t *data, size_t length, unsigned int timeoutMs) = 0;
                virtual int read(uint8_t *data, size_t length, unsigned int timeoutMs) = 0;
                virtual void flush() = 0;
        };

        struct DeviceDescriptor
        {
            std::string name;
            int slotCount {0};
            int currentSlot {0};
        };

        AtikEFW();
        explicit AtikEFW(const DeviceDescriptor &desc, std::shared_ptr<Transport> transport = nullptr);
        ~AtikEFW() override;

        const char *getDefaultName() override;

        bool Connect() override;
        bool Disconnect() override;

    protected:
        bool initProperties() override;
        bool updateProperties() override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool saveConfigItems(FILE *fp) override;
        bool Handshake() override;

        int QueryFilter() override;
        bool SelectFilter(int targetFilter) override;
        void TimerHit() override;

    private:
        bool connectSimulation();
        bool connectTransport();
        bool sendStatus(bool requireParse, int *slotCount, int *currentSlot);
        bool sendCommand(const std::vector<uint8_t> &command);
        void applySlotCount(int slots, bool updateProperty);

        std::shared_ptr<Transport> injectedTransport_;
        std::shared_ptr<Transport> activeTransport_;
        int slotCountHint_ {0};
        int currentSlotHint_ {0};
        bool movementPending_ {false};
        std::chrono::steady_clock::time_point movementStartedAt_;

        INDI::PropertyNumber SlotCountNP {1};
};
