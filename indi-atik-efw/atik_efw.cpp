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

#include <connectionplugins/connectionserial.h>
#include <indicom.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

namespace
{

constexpr uint8_t kFrameByte = 0x23;
constexpr uint8_t kCmdSetPosition = 0x01;
constexpr uint8_t kCmdStatus = 0x04;
constexpr unsigned int kReadTimeoutMs = 1000;
constexpr unsigned int kWriteTimeoutMs = 1000;
constexpr useconds_t kSerialSettleDelayUs = 200000;
constexpr useconds_t kStatusDelayUs = 100000;
constexpr std::chrono::milliseconds kMoveFallbackDelay {1500};
constexpr int kDefaultSlots = 5;
constexpr int kMaxSlots = 16;
constexpr int kMaxResponseBytes = 64;

class TtyTransport : public AtikEFW::Transport
{
    public:
        explicit TtyTransport(int fd) : fd_(fd) {}

        int write(const uint8_t *data, size_t length, unsigned int) override
        {
            if (fd_ < 0 || data == nullptr || length == 0)
                return -1;

            int written = 0;
            int rc = tty_write(fd_, reinterpret_cast<const char *>(data), static_cast<int>(length), &written);
            if (rc != TTY_OK)
                return -rc;

            return written;
        }

        int read(uint8_t *data, size_t length, unsigned int timeoutMs) override
        {
            if (fd_ < 0 || data == nullptr || length == 0)
                return -1;

            int bytesRead = 0;
            long timeoutSeconds = static_cast<long>(timeoutMs / 1000);
            long timeoutMicroseconds = static_cast<long>((timeoutMs % 1000) * 1000);
            int rc = tty_read_expanded(fd_, reinterpret_cast<char *>(data), static_cast<int>(length),
                                       timeoutSeconds, timeoutMicroseconds, &bytesRead);
            if (rc == TTY_TIME_OUT)
                return 0;
            if (rc != TTY_OK)
                return -rc;

            return bytesRead;
        }

        void flush() override
        {
            if (fd_ >= 0)
                tcflush(fd_, TCIOFLUSH);
        }

    private:
        int fd_ {-1};
};

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

        // Captured hardware also returns two compact frames: #position##slot-count#.
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

enum class StatusReadResult
{
    NoResponse,
    Unparsed,
    Parsed
};

StatusReadResult readStatusResponse(AtikEFW::Transport &transport, std::vector<uint8_t> *rawResponse,
                                    int *position, int *slots)
{
    std::vector<uint8_t> raw;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kReadTimeoutMs);

    while (raw.size() < static_cast<size_t>(kMaxResponseBytes))
    {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                             deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0)
            break;

        uint8_t byte = 0;
        int rc = transport.read(&byte, 1, static_cast<unsigned int>(remaining));
        if (rc <= 0)
            break;

        raw.push_back(byte);

        int parsedPosition = 0;
        int parsedSlots = 0;
        if (parseStatusResponse(raw, &parsedPosition, &parsedSlots))
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

} // namespace

#ifndef ATIK_EFW_DISABLE_LOADER
static class Loader
{
        std::unique_ptr<AtikEFW> wheel;
    public:
        Loader()
        {
            AtikEFW::DeviceDescriptor desc;
            const char *envDev = getenv("INDIDEV");
            if (envDev && envDev[0])
                desc.name = envDev;
            desc.slotCount = kDefaultSlots;
            desc.currentSlot = 1;
            wheel = std::make_unique<AtikEFW>(desc);
        }
} loader;
#endif

AtikEFW::AtikEFW() : AtikEFW(DeviceDescriptor {})
{
}

AtikEFW::AtikEFW(const DeviceDescriptor &desc, std::shared_ptr<Transport> transport)
    : injectedTransport_(std::move(transport))
    , slotCountHint_(desc.slotCount > 0 ? desc.slotCount : kDefaultSlots)
    , currentSlotHint_(desc.currentSlot > 0 ? desc.currentSlot : 1)
{
    setFilterConnection(CONNECTION_SERIAL);
    setVersion(ATIK_EFW_VERSION_MAJOR, ATIK_EFW_VERSION_MINOR);
    if (!desc.name.empty())
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

    if (serialConnection)
    {
        serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
        serialConnection->setPortMatchPattern("0403.*af01|af01|Atik.*EFW|EFW1");
    }

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

bool AtikEFW::Connect()
{
    movementPending_ = false;

    if (isSimulation())
        return connectSimulation();

    if (injectedTransport_)
    {
        activeTransport_ = injectedTransport_;
        activeTransport_->flush();
        return connectTransport();
    }

    return INDI::DefaultDevice::Connect();
}

bool AtikEFW::Disconnect()
{
    movementPending_ = false;
    activeTransport_.reset();

    if (isSimulation() || injectedTransport_)
        return true;

    return INDI::DefaultDevice::Disconnect();
}

bool AtikEFW::Handshake()
{
    movementPending_ = false;

    if (isSimulation())
        return connectSimulation();

    if (injectedTransport_)
    {
        activeTransport_ = injectedTransport_;
    }
    else
    {
        if (PortFD < 0)
        {
            LOG_ERROR("Serial connection has no valid file descriptor.");
            return false;
        }

        activeTransport_ = std::make_shared<TtyTransport>(PortFD);
    }

    activeTransport_->flush();
    usleep(kSerialSettleDelayUs);

    // The previous libusb transport configured FTDI RTS/CTS flow control directly.
    // INDI serial uses raw 9600 8N1 without hardware flow control; see README.md.
    LOG_DEBUG("Using INDI serial transport at 9600 8N1 without hardware flow control.");

    return connectTransport();
}

bool AtikEFW::connectSimulation()
{
    int slots = slotCountHint_ > 0 ? slotCountHint_ : kDefaultSlots;
    CurrentFilter = currentSlotHint_ > 0 ? currentSlotHint_ : 1;
    applySlotCount(slots, true);
    TargetFilter = CurrentFilter;
    LOGF_INFO("%s simulation connected (slots=%d, current=%d)", getDeviceName(), slots, CurrentFilter);
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool AtikEFW::connectTransport()
{
    int detectedSlots = 0;
    int detectedPosition = 0;
    if (!sendStatus(false, &detectedSlots, &detectedPosition))
    {
        LOGF_ERROR("%s: failed to send status command", getDeviceName());
        activeTransport_.reset();
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

bool AtikEFW::sendCommand(const std::vector<uint8_t> &command)
{
    if (!activeTransport_)
        return false;

    int rc = activeTransport_->write(command.data(), command.size(), kWriteTimeoutMs);
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
    auto result = readStatusResponse(*activeTransport_, &response, &position, &slots);

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
