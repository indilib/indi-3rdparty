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

#include "atik_efw_usb.h"

#include <libusb.h>

#include <algorithm>
#include <cstring>

namespace AtikEfwUsb
{

// Out-of-line definition prevents GCC from inlining through the copy constructor
// and producing a false-positive -Warray-bounds diagnostic (GCC 14+).
DeviceInfo::DeviceInfo(const DeviceInfo &) = default;


namespace
{

std::vector<uint8_t> getPortNumbers(libusb_device *device)
{
    uint8_t ports[8];
    int portCount = libusb_get_port_numbers(device, ports, sizeof(ports));
    if (portCount <= 0)
        return {};
    return {ports, ports + portCount};
}

bool matchesDevice(libusb_device *device, const libusb_device_descriptor &desc, const DeviceInfo &info)
{
    if (desc.idVendor != info.vendorId || desc.idProduct != info.productId)
        return false;

    uint8_t bus = libusb_get_bus_number(device);
    if (!info.ports.empty())
        return bus == info.bus && getPortNumbers(device) == info.ports;

    return bus == info.bus && libusb_get_device_address(device) == info.address;
}

class LibusbDeviceHandle : public DeviceHandle
{
    public:
        explicit LibusbDeviceHandle(libusb_device_handle *handle) : handle_(handle) {}

        ~LibusbDeviceHandle() override
        {
            if (!handle_)
                return;

            if (claimedInterface_ >= 0)
                libusb_release_interface(handle_, claimedInterface_);
            libusb_close(handle_);
        }

        bool reset() override
        {
            if (!handle_)
                return false;
            return (libusb_reset_device(handle_) == 0);
        }

        bool detachKernelDriver(int iface) override
        {
            if (!handle_)
                return false;

            int rc = libusb_kernel_driver_active(handle_, iface);
            if (rc == 1)
                return (libusb_detach_kernel_driver(handle_, iface) == 0);
            if (rc == 0 || rc == LIBUSB_ERROR_NOT_SUPPORTED)
                return true;
            return false;
        }

        bool setConfiguration(int config) override
        {
            if (!handle_)
                return false;

            int rc = libusb_set_configuration(handle_, config);
            if (rc == LIBUSB_ERROR_BUSY)
                return true;
            return (rc == 0);
        }

        bool claimInterface(int iface) override
        {
            if (!handle_)
                return false;

            int rc = libusb_claim_interface(handle_, iface);
            if (rc == 0)
                claimedInterface_ = iface;
            return (rc == 0);
        }

        int controlTransfer(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index,
                            unsigned char *data, uint16_t length, unsigned int timeoutMs) override
        {
            if (!handle_)
                return LIBUSB_ERROR_NO_DEVICE;

            return libusb_control_transfer(handle_, requestType, request, value, index, data, length, timeoutMs);
        }

        int write(uint8_t endpoint, const unsigned char *data, int length, unsigned int timeoutMs) override
        {
            if (!handle_)
                return LIBUSB_ERROR_NO_DEVICE;

            int transferred = 0;
            int rc = libusb_bulk_transfer(handle_, endpoint, const_cast<unsigned char *>(data), length, &transferred, timeoutMs);
            if (rc < 0)
                return rc;
            return transferred;
        }

        int read(uint8_t endpoint, unsigned char *data, int length, unsigned int timeoutMs) override
        {
            if (!handle_)
                return LIBUSB_ERROR_NO_DEVICE;

            int transferred = 0;
            int rc = libusb_bulk_transfer(handle_, endpoint, data, length, &transferred, timeoutMs);
            if (rc < 0)
                return rc;
            return transferred;
        }

    private:
        libusb_device_handle *handle_ {nullptr};
        int claimedInterface_ {-1};
};

class LibusbBackend : public Backend
{
    public:
        LibusbBackend()
        {
            if (libusb_init(&context_) == 0)
                initialized_ = true;
        }

        ~LibusbBackend() override
        {
            if (context_)
                libusb_exit(context_);
        }

        std::vector<DeviceInfo> listDevices(uint16_t vendorId, uint16_t productId) override
        {
            std::vector<DeviceInfo> devices;
            if (!initialized_)
                return devices;

            libusb_device **list = nullptr;
            ssize_t count = libusb_get_device_list(context_, &list);
            if (count < 0)
                return devices;

            for (ssize_t i = 0; i < count; i++)
            {
                libusb_device *device = list[i];
                libusb_device_descriptor desc;
                if (libusb_get_device_descriptor(device, &desc) != 0)
                    continue;

                if (desc.idVendor != vendorId || desc.idProduct != productId)
                    continue;

                DeviceInfo info;
                info.vendorId = desc.idVendor;
                info.productId = desc.idProduct;
                info.bus = libusb_get_bus_number(device);
                info.address = libusb_get_device_address(device);

                info.ports = getPortNumbers(device);

                if (desc.iProduct > 0)
                {
                    libusb_device_handle *handle = nullptr;
                    if (libusb_open(device, &handle) == 0)
                    {
                        unsigned char product[256];
                        int rc = libusb_get_string_descriptor_ascii(handle, desc.iProduct, product, sizeof(product));
                        if (rc > 0)
                            info.product.assign(reinterpret_cast<char *>(product), rc);
                        libusb_close(handle);
                    }
                }

                devices.push_back(std::move(info));
            }

            libusb_free_device_list(list, 1);
            return devices;
        }

        std::unique_ptr<DeviceHandle> open(const DeviceInfo &info) override
        {
            if (!initialized_)
                return nullptr;

            libusb_device **list = nullptr;
            ssize_t count = libusb_get_device_list(context_, &list);
            if (count < 0)
                return nullptr;

            std::unique_ptr<DeviceHandle> result;
            for (ssize_t i = 0; i < count; i++)
            {
                libusb_device *device = list[i];
                libusb_device_descriptor desc;
                if (libusb_get_device_descriptor(device, &desc) != 0)
                    continue;

                if (!matchesDevice(device, desc, info))
                    continue;

                libusb_device_handle *handle = nullptr;
                if (libusb_open(device, &handle) == 0 && handle)
                {
                    result = std::make_unique<LibusbDeviceHandle>(handle);
                    break;
                }
            }

            libusb_free_device_list(list, 1);
            return result;
        }

    private:
        libusb_context *context_ {nullptr};
        bool initialized_ {false};
};

} // namespace

Backend &defaultBackend()
{
    static LibusbBackend backend;
    return backend;
}

std::string formatDevicePath(const DeviceInfo &info)
{
    std::string path;
    if (!info.ports.empty())
    {
        path = std::to_string(info.bus) + "-" + std::to_string(info.ports[0]);
        for (size_t i = 1; i < info.ports.size(); i++)
            path += "." + std::to_string(info.ports[i]);
        return path;
    }

    if (info.bus > 0 || info.address > 0)
        path = std::to_string(info.bus) + "-" + std::to_string(info.address);
    return path;
}

} // namespace AtikEfwUsb
