/*
    ScopeLink INDI driver - simulated controller

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink/simulator.h"

#include "scopelink/device.h"
#include "scopelink/parameters.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>

namespace scopelink
{

namespace
{

/** Steps the simulated motors advance per millisecond of elapsed time. */
constexpr double StepsPerMs = 2.0;

} // namespace

// ---------------------------------------------------------------------------------------------------
// Motor
// ---------------------------------------------------------------------------------------------------

uint8_t SimulatedController::Motor::advance(long elapsedMs)
{
    if (position == target)
    {
        load = 0;
        return 2; // halted
    }

    const int step = static_cast<int>(elapsedMs * StepsPerMs);
    const bool up  = target > position;

    if (step >= std::abs(target - position))
        position = target;
    else
        position += up ? step : -step;

    load = 35;

    return up ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------
// SimulatedController
// ---------------------------------------------------------------------------------------------------

SimulatedController::SimulatedController(int hardwareMajor) : m_hardwareMajor(hardwareMajor)
{
    // Seeded from the same catalogue the driver reads, so that the simulator holds exactly the
    // identifiers a controller of this generation holds - no more, and no fewer. Seeding a hand-picked
    // few instead left the rest unanswered, which does not model a controller that lacks them: it models
    // one that has them and has stopped replying, and it cost the driver a full retry sequence for each
    // one on every connect.
    Identification identification;

    identification.hardwareMajor = hardwareMajor;

    const Capabilities capabilities = Capabilities::of(identification, 1);

    for (const Did &identifier : buildDidCatalogue(capabilities))
    {
        m_dids[identifier.id()]      = defaultValue(identifier.id());
        m_didLength[identifier.id()] = identifier.length();
    }

    // value(), not m_dids[], so that asking about the flap on a generation that has none does not create
    // the identifier and start answering for it.
    m_focuser.position = m_focuser.target = value(0x000e);
    m_flap.position = m_flap.target = value(0x010e);
    m_lastTick                      = now();
}

Frame SimulatedController::handle(const Frame &request)
{
    if (request.size() < 4)
        return {};

    const uint8_t commandId = request[2];
    const Frame payload(request.begin() + 4, request.end());

    tick();

    switch (commandId)
    {
        case command::Status:
            return frame(commandId, statusPayload());

        case command::InterfaceVersion:
            return frame(commandId, Frame{ static_cast<uint8_t>(m_hardwareMajor), 0, 1, 0 });

        case command::HardwareIdentification:
            return frame(commandId, Frame{ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42 });

        case command::SoftwareIdentification:
            return frame(commandId, softwarePayload());

        case command::SetRtc:
            return frame(commandId, Frame{ 1 });

        case command::DataIdentifier:
            return dataIdentifier(payload);

        case command::Motor:
            return motorCommand(payload);

        case command::Fan:
            return fanCommand(payload);

        case command::FlatboxDuty:
            m_flatboxDuty = (payload.size() > 1) ? payload[1] : 0;
            return frame(commandId, Frame{ 1 });

        case command::PowerSwitch:
            if (payload.size() > 1)
                m_power[payload[0] & 1] = payload[1] != 0;
            return frame(commandId, Frame{ 1 });

        case command::Reset:
            return frame(commandId, Frame{ 1 });

        case command::EepromStatistics:
            return frame(commandId, Frame{ 0, 12, 0, 5, 0, 3, 0, 2, 0, 1, 0, 1, 0, 0, 0x2f, 0x1a });

        case command::ReadDtc:
            return frame(commandId, faultPayload());

        case command::ClearDtc:
            m_faults.clear();
            return frame(commandId, Frame{ 1, 0, 0 });

        default:
            // An unknown command is answered with a frame for a different command, which is what a real
            // desynchronised link looks like and exercises the driver's validation.
            return frame(0xff, Frame{ 0 });
    }
}

long SimulatedController::now()
{
    return static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void SimulatedController::tick()
{
    const long moment  = now();
    const long elapsed = moment - m_lastTick;

    m_lastTick = moment;

    m_focuserStatus = m_focuser.advance(elapsed);
    m_flapStatus    = m_flap.advance(elapsed);
}

Frame SimulatedController::frame(uint8_t command, const Frame &payload)
{
    Frame reply{ 0xaa, 0x55, command, static_cast<uint8_t>(payload.size()) };

    reply.insert(reply.end(), payload.begin(), payload.end());

    return reply;
}

void SimulatedController::put16(Frame &frame, unsigned value)
{
    frame.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    frame.push_back(static_cast<uint8_t>(value & 0xff));
}

void SimulatedController::put32(Frame &frame, unsigned value)
{
    frame.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
    frame.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    frame.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    frame.push_back(static_cast<uint8_t>(value & 0xff));
}

Frame SimulatedController::softwarePayload() const
{
    const std::string text = "SCOPELINK SIMULATOR " + std::to_string(m_hardwareMajor) + ".0";

    Frame payload(40, 0);

    std::memcpy(payload.data(), text.c_str(), std::min<size_t>(text.size(), payload.size()));

    return payload;
}

/**
 * @brief Builds the cyclic status payload for the configured generation.
 *
 * The offsets here are the ones the driver's parser expects, written out in the same order, so a
 * disagreement between the two shows up as a failing test rather than as a puzzling reading.
 */
Frame SimulatedController::statusPayload() const
{
    Frame payload;

    put16(payload, 12100);                    //  4 supply voltage
    put16(payload, 4980);                     //  6 IR sensor supply
    put16(payload, m_fanOn[0] ? 11800u : 0u); //  8 fan A
    put16(payload, m_fanOn[1] ? 11750u : 0u); // 10 fan B
    put16(payload, 42);                       // 12 controller temperature
    put16(payload, 3290);                     // 14 controller supply
    put16(payload, 0);                        // 16 reserved

    payload.push_back(m_fanOverride[0] ? 1 : 0); // 18
    payload.push_back(m_fanOverride[1] ? 1 : 0); // 19
    payload.push_back(m_fanOn[0] ? 1 : 0);       // 20
    payload.push_back(m_fanOn[1] ? 1 : 0);       // 21

    put16(payload, static_cast<unsigned>(value(0x0202))); // 22 fan A target dT
    put16(payload, static_cast<unsigned>(value(0x0212))); // 24 fan B target dT
    put16(payload, 14650);                                // 26 ambient, 20.0 C in 1/50 K
    put16(payload, 14400);                                // 28 mirror, 15.0 C in 1/50 K

    payload.push_back(m_focuserStatus);                        // 30
    payload.push_back(static_cast<uint8_t>(m_focuser.load));   // 31
    put32(payload, static_cast<unsigned>(m_focuser.position)); // 32

    payload.push_back(m_flapStatus);                        // 36
    payload.push_back(static_cast<uint8_t>(m_flap.load));   // 37
    put32(payload, static_cast<unsigned>(m_flap.position)); // 38

    payload.push_back(m_power[0] ? 1 : 0); // 42
    payload.push_back(m_power[1] ? 1 : 0); // 43

    put32(payload, 0); // 44 reserved

    payload.push_back(static_cast<uint8_t>(m_flatboxDuty));   // 48
    payload.push_back(11);                                    // 49
    payload.push_back(23);                                    // 50
    payload.push_back(31);                                    // 51
    payload.push_back(static_cast<uint8_t>(m_faults.size())); // 52
    payload.push_back(activeFaultCount());                    // 53
    put16(payload, 0);                                        // 54

    if (m_hardwareMajor > 2)
    {
        payload.push_back(1); // 56 USB DS1 powered
        payload.push_back(1); // 57 USB DS2 powered
        payload.push_back(0); // 58 DS1 fault
        payload.push_back(0); // 59 DS2 fault
    }

    return payload;
}

int SimulatedController::value(uint32_t did) const
{
    const auto found = m_dids.find(did);

    return (found == m_dids.end()) ? 0 : found->second;
}

/**
 * @brief Number of value bytes an identifier occupies, taken from the catalogue's storage type.
 *
 * Not inferred from the identifier's number: the catalogue is what the driver decodes the reply with, so
 * anything the simulator worked out for itself would only ever agree with it by luck.
 */
size_t SimulatedController::didLength(uint32_t did) const
{
    const auto found = m_didLength.find(did);

    return (found == m_didLength.end()) ? 1 : found->second;
}

/**
 * @brief What one identifier holds on a freshly configured controller.
 *
 * Plausible rather than measured - the simulator's job is to be answerable and self consistent, not to
 * reproduce a particular unit's tuning. The motor pages share a layout, so the low byte decides the value
 * for both the focuser at 0x00xx and the flap at 0x01xx.
 */
int SimulatedController::defaultValue(uint32_t did)
{
    const bool isMotorPage = (did < 0x0200);

    if (isMotorPage)
    {
        const bool isFlap = (did >= 0x0100);

        switch (did & 0xff)
        {
            case 0x00:
                return 16; // global current scaler
            case 0x01:
                return 8; // hold current
            case 0x02:
                return 24; // move current
            case 0x03:
                return 1000; // A max
            case 0x04:
                return 500; // A start
            case 0x05:
                return 1000; // D max
            case 0x06:
                return 500; // D stop
            case 0x07:
                return 4000; // V max
            case 0x08:
                return 100; // V start
            case 0x09:
                return 200; // V stop
            case 0x0a:
                return 1500; // V tran
            case 0x0b:
                return 800; // V stealth chop max
            case 0x0c:
                return 0; // invert direction
            case 0x0d:
                return 4; // stall detection sensitivity
            case 0x0e:
                return isFlap ? 0 : 20000; // position
            case 0x0f:
                return isFlap ? 12000 : 40000; // maximum position
            default:
                return 0;
        }
    }

    switch (did)
    {
        case 0x0200: // fan override defaults, off on both fans
        case 0x0201:
        case 0x0210:
        case 0x0211:
            return 0;

        case 0x0202: // target dT, in hundredths of a kelvin
        case 0x0212:
            return 25;

        case 0x0203: // target hysteresis
        case 0x0213:
            return 5;

        case 0x0204: // startup blow time
        case 0x0214:
            return 30;

        case 0x0300: // temperature filter coefficients
        case 0x0301:
            return 8;

        case 0x0302: // temperature sensor fitted
            return 1;

        case 0x0500: // max voltage drop on the IR sensor supply
            return 500;

        case 0x0600: // short to Vcc max voltage difference
            return 800;

        case 0x0601: // overcurrent max voltage difference
            return 600;

        case 0x0602: // enable open load detection
            return 1;

        case 0x0700: // controller temperature warning level
            return 70;

        case 0x0701: // controller temperature error level
            return 85;

        default:
            return 0;
    }
}

Frame SimulatedController::dataIdentifier(const Frame &payload)
{
    if (payload.size() < 3)
        return frame(command::DataIdentifier, Frame{ 0xff, 0, 0 });

    const uint32_t did  = static_cast<uint32_t>((payload[1] << 8) | payload[2]);
    const size_t length = didLength(did);

    if (payload[0] == 0)
    {
        // An identifier this generation does not hold is not answered at all, which is what makes the
        // driver's capability discovery fall back to its defaults.
        if (m_dids.find(did) == m_dids.end())
            return {};

        Frame reply{ 0, payload[1], payload[2] };
        const unsigned stored = static_cast<unsigned>(m_dids[did]);

        if (length == 1)
            reply.push_back(static_cast<uint8_t>(stored & 0xff));
        else if (length == 2)
            put16(reply, stored);
        else
            put32(reply, stored);

        return frame(command::DataIdentifier, reply);
    }

    unsigned stored = 0;

    for (size_t index = 0; index < length; index++)
        stored = (stored << 8) | ((payload.size() > (3 + index)) ? payload[3 + index] : 0);

    m_dids[did] = static_cast<int>(stored);

    if (did == 0x000e)
        m_focuser.position = m_focuser.target = m_dids[did];

    return frame(command::DataIdentifier, Frame{ 1, payload[1], payload[2] });
}

Frame SimulatedController::motorCommand(const Frame &payload)
{
    if (payload.size() >= 2)
    {
        Motor &affected = (payload[0] == 1) ? m_flap : m_focuser;

        if ((payload[1] == motor::Halt))
        {
            affected.target = affected.position;
        }
        else if (payload.size() >= 6)
        {
            const int position =
                static_cast<int>((payload[2] << 24) | (payload[3] << 16) | (payload[4] << 8) | payload[5]);

            if (payload[1] == motor::Sync)
                affected.position = affected.target = position;
            else
                affected.target = position;
        }
    }

    return frame(command::Motor, Frame{ 1 });
}

Frame SimulatedController::fanCommand(const Frame &payload)
{
    if (payload.size() >= 3)
    {
        const int index = payload[1] & 1;

        if (payload[0] == 0)
            m_fanOverride[index] = payload[2] != 0;
        else if (payload[0] == 1)
            m_fanOn[index] = payload[2] != 0;
        else if ((payload[0] == 2) && (payload.size() >= 4))
            m_dids[(index == 0) ? 0x0202 : 0x0212] = (payload[2] << 8) | payload[3];
    }

    return frame(command::Fan, Frame{ 1 });
}

Frame SimulatedController::faultPayload() const
{
    const size_t snapshotLength = (m_hardwareMajor > 2) ? 48 : 34;

    Frame payload{ 0x07, static_cast<uint8_t>(m_faults.size()) };

    for (const StoredFault &fault : m_faults)
    {
        put16(payload, fault.code);
        put16(payload, 0);
        payload.push_back(fault.occurrences);
        payload.push_back(fault.active ? 1 : 0);

        Frame snapshot(snapshotLength, 0);

        // Uptime and a plausible timestamp, so that the driver's freeze frame decoding has something
        // recognisable to show rather than a block of zeros.
        snapshot[3]                               = 0x64;
        snapshot[4]                               = 0x2f;
        snapshot[5]                               = 0x44;
        snapshot[(m_hardwareMajor > 2) ? 20 : 16] = 0x07;
        snapshot[(m_hardwareMajor > 2) ? 21 : 17] = 0xea;
        snapshot[(m_hardwareMajor > 2) ? 22 : 18] = 8;
        snapshot[(m_hardwareMajor > 2) ? 23 : 19] = 11;

        payload.insert(payload.end(), snapshot.begin(), snapshot.end());
    }

    return payload;
}

uint8_t SimulatedController::activeFaultCount() const
{
    uint8_t count = 0;

    for (const StoredFault &fault : m_faults)
        count = static_cast<uint8_t>(count + (fault.active ? 1 : 0));

    return count;
}

// ---------------------------------------------------------------------------------------------------
// SimulatedTransport
// ---------------------------------------------------------------------------------------------------

SimulatedTransport::SimulatedTransport(int hardwareMajor) : m_controller(hardwareMajor) {}

bool SimulatedTransport::isOpen() const
{
    return m_open;
}

void SimulatedTransport::close()
{
    m_open = false;
    discardBuffers();
}

void SimulatedTransport::discardBuffers()
{
    m_request.clear();
    m_reply.clear();
}

void SimulatedTransport::write(const Frame &data)
{
    if (!m_open)
        throw CommunicationError("Cannot write to the simulated controller because it is closed.");

    m_request.insert(m_request.end(), data.begin(), data.end());

    // Requests are framed exactly like responses, so the same length byte drives reassembly here as on a
    // real link.
    while (m_request.size() >= 4)
    {
        if ((m_request[0] != 0xaa) || (m_request[1] != 0x55))
        {
            m_request.erase(m_request.begin());
            continue;
        }

        const size_t length = 4 + m_request[3];

        if (m_request.size() < length)
            break;

        const Frame request(m_request.begin(), m_request.begin() + static_cast<long>(length));

        m_request.erase(m_request.begin(), m_request.begin() + static_cast<long>(length));

        const Frame reply = m_controller.handle(request);

        m_reply.insert(m_reply.end(), reply.begin(), reply.end());
    }
}

Frame SimulatedTransport::read(size_t count)
{
    if (!m_open)
        throw CommunicationError("Cannot read from the simulated controller because it is closed.");

    // A command the simulated generation does not answer leaves nothing here, and the caller sees the same
    // timeout a silent controller would produce rather than a short read it has to interpret.
    if (m_reply.size() < count)
        throw TimeoutError("The simulated controller did not answer.");

    Frame data(m_reply.begin(), m_reply.begin() + static_cast<long>(count));

    m_reply.erase(m_reply.begin(), m_reply.begin() + static_cast<long>(count));

    return data;
}

bool SimulatedTransport::reopen()
{
    m_open = true;
    discardBuffers();

    return true;
}

std::string SimulatedTransport::name() const
{
    return "simulator";
}

} // namespace scopelink
