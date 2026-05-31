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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace AtikEfwUsb
{

struct DeviceInfo
{
    uint16_t vendorId {0};
    uint16_t productId {0};
    uint8_t bus {0};
    uint8_t address {0};
    std::vector<uint8_t> ports;
    std::string product;

    DeviceInfo() = default;
    DeviceInfo(const DeviceInfo &);
    DeviceInfo(DeviceInfo &&) = default;
    DeviceInfo &operator=(const DeviceInfo &) = default;
    DeviceInfo &operator=(DeviceInfo &&) = default;
    ~DeviceInfo() = default;
};

class DeviceHandle
{
    public:
        virtual ~DeviceHandle() = default;

        virtual bool reset() = 0;
        virtual bool detachKernelDriver(int iface) = 0;
        virtual bool setConfiguration(int config) = 0;
        virtual bool claimInterface(int iface) = 0;

        virtual int controlTransfer(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index,
                                    unsigned char *data, uint16_t length, unsigned int timeoutMs) = 0;
        virtual int write(uint8_t endpoint, const unsigned char *data, int length, unsigned int timeoutMs) = 0;
        virtual int read(uint8_t endpoint, unsigned char *data, int length, unsigned int timeoutMs) = 0;
};

class Backend
{
    public:
        virtual ~Backend() = default;

        virtual std::vector<DeviceInfo> listDevices(uint16_t vendorId, uint16_t productId) = 0;
        virtual std::unique_ptr<DeviceHandle> open(const DeviceInfo &info) = 0;
};

Backend &defaultBackend();
std::string formatDevicePath(const DeviceInfo &info);

} // namespace AtikEfwUsb
