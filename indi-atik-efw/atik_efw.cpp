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

#include "config.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace
{

constexpr uint16_t kVendorId = 0x0403;
constexpr uint16_t kProductId = 0xaf01;
constexpr uint8_t kInterface = 0;
constexpr uint8_t kEndpointOut = 0x02;
constexpr uint8_t kEndpointIn = 0x81;
constexpr uint8_t kFrameByte = 0x23;
constexpr uint8_t kCmdSetPosition = 0x01;
constexpr uint8_t kCmdStatus = 0x04;
constexpr uint8_t kFtdiRequestType = 0x40;
constexpr uint8_t kFtdiReset = 0x00;
constexpr uint8_t kFtdiSetBaud = 0x03;
constexpr uint8_t kFtdiSetFlow = 0x01;
constexpr uint16_t kFtdiBaudValue = 0x4138;
constexpr uint16_t kFtdiFlowValue = 0x0303;
constexpr unsigned int kControlTimeoutMs = 1000;
constexpr unsigned int kReadTimeoutMs = 1000;
constexpr unsigned int kWriteTimeoutMs = 1000;
constexpr int kMaxStatusReadAttempts = 4;
constexpr useconds_t kUsbResetDelayUs = 1000000;
constexpr useconds_t kFtdiDelayUs = 200000;
constexpr useconds_t kStatusDelayUs = 100000;
constexpr std::chrono::milliseconds kMoveFallbackDelay {1500};
constexpr int kDefaultSlots = 5;
constexpr int kMaxSlots = 16;
constexpr int kMaxResponseBytes = 64;

std::string bytesToHex(const std::vector<uint8_t> &data)
{
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); i++)
    {
        if (i)
            oss << ' ';
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::vector<uint8_t> sanitizeResponse(const std::vector<uint8_t> &raw)
{
    if (raw.size() == 2)
        return {};

    if (raw.size() >= 3 && raw[0] != kFrameByte && raw[2] == kFrameByte)
        return {raw.begin() + 2, raw.end()};

    return raw;
}

int decodeByte(uint8_t value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    return value;
}

bool parseStatusResponse(const std::vector<uint8_t> &data, int *position, int *slots)
{
    std::vector<int> compactValues;
    size_t start = 0;
    while (start < data.size())
    {
        while (start < data.size() && data[start] != kFrameByte)
            start++;
        if (start == data.size())
            break;

        size_t end = start + 1;
        while (end < data.size() && data[end] != kFrameByte)
            end++;
        if (end == data.size())
            break;

        size_t payloadSize = end - start - 1;

        // Some wheel firmware returns one frame containing the status command and values.
        if (payloadSize >= 2 && data[start + 1] == kCmdStatus)
        {
            int decodedPosition = decodeByte(data[start + 2]);
            if (decodedPosition > 0 && position)
                *position = decodedPosition;

            if (payloadSize >= 3)
            {
                int decodedSlots = decodeByte(data[start + 3]);
                if (decodedSlots > 0 && slots)
                    *slots = decodedSlots;
            }

            return (decodedPosition > 0);
        }

        // EFW2 hardware also returns two compact frames: #position##slot-count#.
        if (payloadSize == 1)
        {
            int value = decodeByte(data[start + 1]);
            if (value > 0 && value <= kMaxSlots)
                compactValues.push_back(value);
        }

        start = end + 1;
    }

    if (compactValues.empty())
        return false;

    if (position)
        *position = compactValues[0];
    if (slots && compactValues.size() > 1)
        *slots = compactValues[1];
    return true;
}

bool writeCommand(AtikEfwUsb::DeviceHandle &handle, const std::vector<uint8_t> &command)
{
    int rc = handle.write(kEndpointOut, command.data(), static_cast<int>(command.size()), kWriteTimeoutMs);
    return (rc == static_cast<int>(command.size()));
}

enum class StatusReadResult
{
    NoResponse,
    Unparsed,
    Parsed
};

StatusReadResult readStatusResponse(AtikEfwUsb::DeviceHandle &handle, std::vector<uint8_t> *rawResponse,
                                    int *position, int *slots)
{
    std::vector<uint8_t> raw;
    std::vector<uint8_t> cleaned;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kReadTimeoutMs);

    for (int attempt = 0; attempt < kMaxStatusReadAttempts; attempt++)
    {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                             deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0)
            break;

        unsigned char buffer[kMaxResponseBytes] = {0};
        int rc = handle.read(kEndpointIn, buffer, sizeof(buffer), static_cast<unsigned int>(remaining));
        if (rc <= 0)
            break;

        std::vector<uint8_t> packet(buffer, buffer + rc);
        raw.insert(raw.end(), packet.begin(), packet.end());

        auto payload = sanitizeResponse(packet);
        cleaned.insert(cleaned.end(), payload.begin(), payload.end());

        int parsedPosition = 0;
        int parsedSlots = 0;
        if (parseStatusResponse(cleaned, &parsedPosition, &parsedSlots))
        {
            if (rawResponse)
                *rawResponse = std::move(raw);
            if (position)
                *position = parsedPosition;
            if (slots)
                *slots = parsedSlots;
            return StatusReadResult::Parsed;
        }
    }

    if (rawResponse)
        *rawResponse = raw;
    return raw.empty() ? StatusReadResult::NoResponse : StatusReadResult::Unparsed;
}

bool initializeWheel(AtikEfwUsb::DeviceHandle &handle, std::string *error)
{
    handle.detachKernelDriver(kInterface);

    if (!handle.setConfiguration(1))
    {
        if (error)
            *error = "Failed to set USB configuration";
        return false;
    }

    if (!handle.claimInterface(kInterface))
    {
        if (error)
            *error = "Failed to claim USB interface";
        return false;
    }

    if (handle.controlTransfer(kFtdiRequestType, kFtdiReset, 0, 0, nullptr, 0, kControlTimeoutMs) < 0)
    {
        if (error)
            *error = "Failed to reset FTDI interface";
        return false;
    }

    if (handle.controlTransfer(kFtdiRequestType, kFtdiSetBaud, kFtdiBaudValue, 0, nullptr, 0, kControlTimeoutMs) < 0)
    {
        if (error)
            *error = "Failed to set FTDI baud rate";
        return false;
    }

    if (handle.controlTransfer(kFtdiRequestType, kFtdiSetFlow, kFtdiFlowValue, 0, nullptr, 0, kControlTimeoutMs) < 0)
    {
        if (error)
            *error = "Failed to set FTDI flow control";
        return false;
    }

    usleep(kFtdiDelayUs);

    unsigned char flush[kMaxResponseBytes] = {0};
    handle.read(kEndpointIn, flush, sizeof(flush), 100);
    return true;
}

bool probeStatus(AtikEfwUsb::DeviceHandle &handle, bool requireParse, int *slotCount, int *currentSlot,
                 std::vector<uint8_t> *raw)
{
    const std::vector<uint8_t> statusCommand {kFrameByte, kCmdStatus, 0x00, kFrameByte};

    if (!writeCommand(handle, statusCommand))
        return false;

    usleep(kStatusDelayUs);

    int slots = 0;
    int position = 0;
    auto result = readStatusResponse(handle, raw, &position, &slots);
    if (result == StatusReadResult::Parsed)
    {
        if (slotCount)
            *slotCount = slots;
        if (currentSlot)
            *currentSlot = position;
    }

    return result == StatusReadResult::Parsed || (!requireParse && result == StatusReadResult::Unparsed);
}

std::string buildDeviceName(const AtikEfwUsb::DeviceInfo &info, size_t index, size_t count)
{
    std::string name = "Atik EFW";
    const char *envDev = getenv("INDIDEV");
    if (envDev && envDev[0] && count == 1)
        name = envDev;

    std::string path = AtikEfwUsb::formatDevicePath(info);
    if (!path.empty())
        name += " " + path;
    else if (count > 1)
        name += " " + std::to_string(index + 1);

    return name;
}

} // namespace

#ifndef ATIK_EFW_DISABLE_LOADER
static class Loader
{
        std::deque<std::unique_ptr<AtikEFW>> wheels;
    public:
        Loader()
        {
            auto &backend = AtikEfwUsb::defaultBackend();
            auto devices = AtikEFW::Enumerate(backend);
            if (devices.empty())
            {
                AtikEFW::DeviceDescriptor desc;
                desc.name = buildDeviceName(desc.info, 0, 1);
                desc.slotCount = kDefaultSlots;
                desc.currentSlot = 1;
                devices.push_back(std::move(desc));
                IDLog("Atik EFW: no USB wheel detected; exposing simulation-capable device.\n");
            }

            for (const auto &device : devices)
                wheels.push_back(std::make_unique<AtikEFW>(device, backend));
        }
} loader;
#endif

AtikEFW::AtikEFW(const DeviceDescriptor &desc, AtikEfwUsb::Backend &backend)
    : backend_(backend)
    , deviceInfo_(desc.info)
    , slotCountHint_(desc.slotCount > 0 ? desc.slotCount : kDefaultSlots)
    , currentSlotHint_(desc.currentSlot > 0 ? desc.currentSlot : 1)
{
    setVersion(ATIK_EFW_VERSION_MAJOR, ATIK_EFW_VERSION_MINOR);
    setDeviceName(desc.name.c_str());
}

AtikEFW::~AtikEFW() = default;

const char *AtikEFW::getDefaultName()
{
    return "Atik EFW";
}

bool AtikEFW::initProperties()
{
    INDI::FilterWheel::initProperties();

    SlotCountNP[0].fill("SLOTS", "Slots", "%.0f", 1, kMaxSlots, 1, slotCountHint_);
    SlotCountNP.fill(getDeviceName(), "FILTER_SLOTS", "Slots", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    SlotCountNP.load();
    slotCountHint_ = static_cast<int>(SlotCountNP[0].getValue());

    addDebugControl();
    addSimulationControl();
    setDefaultPollingPeriod(250);

    return true;
}

bool AtikEFW::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
        defineProperty(SlotCountNP);
    else
        deleteProperty(SlotCountNP);

    return true;
}

bool AtikEFW::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (SlotCountNP.isNameMatch(name))
        {
            SlotCountNP.update(values, names, n);
            int slots = static_cast<int>(SlotCountNP[0].getValue());
            if (slots > 0)
            {
                applySlotCount(slots, false);
                SlotCountNP.setState(IPS_OK);
            }
            else
            {
                SlotCountNP.setState(IPS_ALERT);
            }
            SlotCountNP.apply();
            return true;
        }
    }

    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

bool AtikEFW::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);
    SlotCountNP.save(fp);
    return true;
}

bool AtikEFW::openHandle()
{
    handle_ = backend_.open(deviceInfo_);
    if (!handle_)
    {
        LOGF_ERROR("Failed to open USB device for %s", getDeviceName());
        return false;
    }

    return true;
}

bool AtikEFW::resetAndReopenHandle()
{
    if (!handle_)
        return false;

    if (!handle_->reset())
        LOGF_WARN("%s: USB device reset failed, continuing with reopen", getDeviceName());

    handle_.reset();
    usleep(kUsbResetDelayUs);

    return openHandle();
}

bool AtikEFW::configureDevice()
{
    if (!handle_)
        return false;

    std::string error;
    if (!initializeWheel(*handle_, &error))
    {
        LOGF_ERROR("%s: %s", getDeviceName(), error.c_str());
        return false;
    }

    return true;
}

bool AtikEFW::sendCommand(const std::vector<uint8_t> &command)
{
    if (!handle_)
        return false;

    int rc = handle_->write(kEndpointOut, command.data(), static_cast<int>(command.size()), kWriteTimeoutMs);
    if (rc != static_cast<int>(command.size()))
    {
        LOGF_ERROR("%s: failed to write command (%d)", getDeviceName(), rc);
        return false;
    }

    LOGF_DEBUG("%s: sent command %s", getDeviceName(), bytesToHex(command).c_str());
    return true;
}

bool AtikEFW::sendStatus(bool requireParse, int *slotCount, int *currentSlot)
{
    const std::vector<uint8_t> statusCommand {kFrameByte, kCmdStatus, 0x00, kFrameByte};

    if (!sendCommand(statusCommand))
        return false;

    usleep(kStatusDelayUs);

    std::vector<uint8_t> response;
    int slots = 0;
    int position = 0;
    auto result = readStatusResponse(*handle_, &response, &position, &slots);

    if (!response.empty())
        LOGF_DEBUG("%s: raw response %s", getDeviceName(), bytesToHex(response).c_str());

    if (result != StatusReadResult::Parsed)
    {
        if (result == StatusReadResult::NoResponse)
            LOGF_WARN("%s: no status response", getDeviceName());
        else
            LOGF_WARN("%s: unparsed status response %s", getDeviceName(), bytesToHex(response).c_str());
        return !requireParse;
    }

    if (slotCount)
        *slotCount = slots;
    if (currentSlot)
        *currentSlot = position;

    LOGF_DEBUG("%s: status position=%d slots=%d", getDeviceName(), position, slots);
    return true;
}

void AtikEFW::applySlotCount(int slots, bool updateProperty)
{
    if (slots <= 0)
        return;

    if (slots > kMaxSlots)
        slots = kMaxSlots;

    slotCountHint_ = slots;

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(slots);
    FilterSlotNP.updateMinMax();

    if (CurrentFilter < 1)
        CurrentFilter = 1;
    if (CurrentFilter > slots)
        CurrentFilter = slots;

    FilterSlotNP[0].setValue(CurrentFilter);
    FilterSlotNP.apply();
    SetFilterNames();

    if (updateProperty)
    {
        SlotCountNP[0].setValue(slots);
        SlotCountNP.setState(IPS_OK);
        SlotCountNP.apply();
    }
}

bool AtikEFW::Connect()
{
    movementPending_ = false;

    if (isSimulation())
    {
        int slots = slotCountHint_ > 0 ? slotCountHint_ : kDefaultSlots;
        CurrentFilter = currentSlotHint_ > 0 ? currentSlotHint_ : 1;
        applySlotCount(slots, true);
        TargetFilter = CurrentFilter;
        LOGF_INFO("%s simulation connected (slots=%d, current=%d)", getDeviceName(), slots, CurrentFilter);
        SetTimer(getCurrentPollingPeriod());
        return true;
    }

    if (!openHandle())
        return false;

    if (!resetAndReopenHandle())
    {
        handle_.reset();
        return false;
    }

    if (!configureDevice())
    {
        handle_.reset();
        return false;
    }

    int detectedSlots = 0;
    int detectedPosition = 0;
    if (!sendStatus(false, &detectedSlots, &detectedPosition))
    {
        LOGF_ERROR("%s: failed to send status command", getDeviceName());
        handle_.reset();
        return false;
    }

    int slots = detectedSlots > 0 ? detectedSlots : slotCountHint_;
    if (slots <= 0)
        slots = kDefaultSlots;
    if (slots > kMaxSlots)
        slots = kMaxSlots;

    CurrentFilter = (detectedPosition > 0) ? detectedPosition : currentSlotHint_;
    if (CurrentFilter <= 0)
        CurrentFilter = 1;
    currentSlotHint_ = CurrentFilter;

    applySlotCount(slots, true);

    FilterSlotNP[0].setValue(CurrentFilter);
    FilterSlotNP.apply();

    TargetFilter = CurrentFilter;

    LOGF_INFO("%s connected (slots=%d, current=%d)", getDeviceName(), slots, CurrentFilter);

    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool AtikEFW::Disconnect()
{
    movementPending_ = false;
    handle_.reset();
    return true;
}

void AtikEFW::TimerHit()
{
    if (!isConnected())
        return;

    if (FilterSlotNP.getState() == IPS_BUSY)
    {
        int position = QueryFilter();
        if (position > 0 && TargetFilter == CurrentFilter)
            SelectFilterDone(CurrentFilter);
    }

    SetTimer(getCurrentPollingPeriod());
}

bool AtikEFW::SelectFilter(int targetFilter)
{
    TargetFilter = targetFilter;

    if (isSimulation())
    {
        CurrentFilter = TargetFilter;
        FilterSlotNP[0].setValue(CurrentFilter);
        SelectFilterDone(CurrentFilter);
        return true;
    }

    const std::vector<uint8_t> command {kFrameByte, kCmdSetPosition, static_cast<uint8_t>(targetFilter), kFrameByte};

    if (!sendCommand(command))
    {
        LOGF_ERROR("%s: failed to set filter %d", getDeviceName(), targetFilter);
        return false;
    }

    movementPending_ = true;
    movementStartedAt_ = std::chrono::steady_clock::now();
    return true;
}

int AtikEFW::QueryFilter()
{
    if (isSimulation())
    {
        FilterSlotNP[0].setValue(CurrentFilter);
        FilterSlotNP.apply();
        return CurrentFilter;
    }

    int slots = 0;
    int position = 0;
    if (!sendStatus(true, &slots, &position))
    {
        if (movementPending_ && std::chrono::steady_clock::now() - movementStartedAt_ >= kMoveFallbackDelay)
        {
            LOGF_WARN("%s: status unavailable after filter change; assuming target slot %d",
                      getDeviceName(), TargetFilter);
            CurrentFilter = TargetFilter;
            currentSlotHint_ = CurrentFilter;
            movementPending_ = false;
            FilterSlotNP[0].setValue(CurrentFilter);
            FilterSlotNP.apply();
            return CurrentFilter;
        }

        if (movementPending_)
            return -1;

        FilterSlotNP.setState(IPS_ALERT);
        FilterSlotNP.apply();
        return -1;
    }

    if (slots > 0 && slots != static_cast<int>(FilterSlotNP[0].getMax()))
        applySlotCount(slots, true);

    if (position > 0)
    {
        CurrentFilter = position;
        currentSlotHint_ = CurrentFilter;
        if (CurrentFilter == TargetFilter)
            movementPending_ = false;
        FilterSlotNP[0].setValue(CurrentFilter);
        FilterSlotNP.apply();
        return CurrentFilter;
    }

    FilterSlotNP.setState(IPS_ALERT);
    FilterSlotNP.apply();
    return -1;
}

std::vector<AtikEFW::DeviceDescriptor> AtikEFW::Enumerate(AtikEfwUsb::Backend &backend)
{
    std::vector<DeviceDescriptor> detected;
    auto devices = backend.listDevices(kVendorId, kProductId);

    if (devices.empty())
    {
        IDLog("No Atik EFW detected.\n");
        return detected;
    }

    for (size_t i = 0; i < devices.size(); i++)
    {
        DeviceDescriptor desc;
        desc.info = devices[i];
        desc.name = buildDeviceName(devices[i], i, devices.size());
        desc.slotCount = kDefaultSlots;
        desc.currentSlot = 1;
        desc.slotCountKnown = false;

        auto handle = backend.open(devices[i]);
        if (!handle)
        {
            IDLog("Atik EFW: failed to open USB device on bus %u address %u.\n",
                  devices[i].bus, devices[i].address);
            detected.push_back(std::move(desc));
            continue;
        }

        std::string error;
        if (!initializeWheel(*handle, &error))
        {
            IDLog("Atik EFW: init failed on bus %u address %u (%s).\n",
                  devices[i].bus, devices[i].address, error.c_str());
            detected.push_back(std::move(desc));
            continue;
        }

        int slots = 0;
        int position = 0;
        std::vector<uint8_t> raw;
        if (!probeStatus(*handle, false, &slots, &position, &raw))
        {
            IDLog("Atik EFW: status probe failed on bus %u address %u.\n",
                  devices[i].bus, devices[i].address);
        }
        else
        {
            desc.slotCount = slots > 0 ? slots : kDefaultSlots;
            desc.currentSlot = position > 0 ? position : 1;
            desc.slotCountKnown = (slots > 0);
        }

        detected.push_back(std::move(desc));
    }

    return detected;
}
