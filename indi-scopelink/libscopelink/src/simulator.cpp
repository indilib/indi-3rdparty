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
#include "scopelink/faults.h"
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

SimulatedController::SimulatedController(int hardwareMajor, int interfaceMinor)
    : m_hardwareMajor(hardwareMajor), m_interfaceMinor(interfaceMinor)
{
    // Seeded from the same catalogue the driver reads, so that the simulator holds exactly the
    // identifiers a controller of this generation holds - no more, and no fewer. Seeding a hand-picked
    // few instead left the rest unanswered, which does not model a controller that lacks them: it models
    // one that has them and has stopped replying, and it cost the driver a full retry sequence for each
    // one on every connect.
    Identification identification;

    identification.hardwareMajor  = hardwareMajor;
    identification.interfaceMajor = 1;
    identification.interfaceMinor = interfaceMinor;

    const Capabilities capabilities = Capabilities::of(identification, 1);

    m_capabilities = capabilities;

    for (const Did &identifier : buildDidCatalogue(capabilities))
    {
        // The motor assignment is seeded from the catalogue rather than made up here, and the catalogue is
        // the narrowed one: on a two motor controller "not used" is 2, and a plausible looking 3 written
        // here would be an assignment the firmware refuses to hold. Everything else is a value this file
        // decides, because being answerable and self consistent is all a simulator owes.
        const bool isAssignment = (identifier.id() >= 0x0400) && (identifier.id() <= 0x0442);

        m_dids[identifier.id()]      = isAssignment ? identifier.descriptor().fallback : defaultValue(identifier.id());
        m_didLength[identifier.id()] = identifier.length();
    }

    // As many motors as this controller drives, each starting where its own learnt position identifier
    // says. value(), not m_dids[], so that asking about a motor this generation does not have does not
    // create the identifier and start answering for it.
    m_motors.resize(static_cast<size_t>(capabilities.motorCount));
    m_motorStatus.assign(static_cast<size_t>(capabilities.motorCount), 2);

    for (int index = 0; index < capabilities.motorCount; index++)
    {
        Motor &motor = m_motors[static_cast<size_t>(index)];

        motor.position = motor.target = value(motor::lastPositionDid(index));
    }

    // The number this generation sends for that fault, rather than the number written here. Generation 4
    // inserted the third motor's failures and four more USB ports into the middle of the list, so a
    // simulator that served a literal would be serving a different fault on that controller.
    for (StoredFault &fault : m_faults)
    {
        const int wire = Fault::wireCodeOf(static_cast<FaultCode>(fault.code), hardwareMajor);

        fault.code = static_cast<uint16_t>((wire < 0) ? 0 : wire);
    }

    // Last, because it is taken from the identifiers seeded above: a controller starts by reading its
    // configuration, and what it reads is what it drives from until it is restarted.
    m_running = storedRoles();

    m_lastTick = now();
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
            return frame(commandId,
                         Frame{ static_cast<uint8_t>(m_hardwareMajor), 0, 1, static_cast<uint8_t>(m_interfaceMinor) });

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

        // Interface 1.1 onwards, and refused before it by falling through to the default below - which is
        // what a controller that does not know the command does, and what the driver's choice between the
        // two command sets is held to.
        case command::Function:
            if (m_capabilities.hasConfigurableMotorRoles)
                return functionCommand(payload);
            break;

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
            // The one thing a restart does that is worth modelling: the configuration is read again, so
            // an assignment written since this controller started is finally the one it drives from.
            // Nothing else is reset - the motors keep their positions, which is close enough to a real
            // controller that reads its learnt positions back out of the same memory.
            m_running = storedRoles();
            return frame(commandId, Frame{ 1 });

        case command::EepromStatistics:
            return frame(commandId, Frame{ 0, 12, 0, 5, 0, 3, 0, 2, 0, 1, 0, 1, 0, 0, 0x2f, 0x1a });

        case command::ReadDtc:
            return frame(commandId, faultPayload());

        case command::ClearDtc:
            m_faults.clear();
            return frame(commandId, Frame{ 1, 0, 0 });

        default:
            break;
    }

    // An unknown command is answered with a frame for a different command, which is what a real
    // desynchronised link looks like and exercises the driver's validation.
    return frame(0xff, Frame{ 0 });
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

    for (size_t index = 0; index < m_motors.size(); index++)
        m_motorStatus[index] = m_motors[index].advance(elapsed);
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

    // Six bytes each from offset 30, as many as this controller drives. A third motor pushes everything
    // below six bytes further along, which is the whole of generation 4's difference here.
    for (size_t index = 0; index < m_motors.size(); index++)
    {
        payload.push_back(m_motorStatus[index]);                         // 30, 36, 42
        payload.push_back(static_cast<uint8_t>(m_motors[index].load));   // 31, 37, 43
        put32(payload, static_cast<unsigned>(m_motors[index].position)); // 32, 38, 44
    }

    payload.push_back(m_power[0] ? 1 : 0);
    payload.push_back(m_power[1] ? 1 : 0);

    // The four digital inputs, which interface 1.1 dropped. Everything below moves four bytes forward on
    // a controller that does not send them, which is the whole of the layout difference between the two
    // interface versions apart from the flap state at the end.
    if (m_capabilities.hasDigitalInputs)
        put32(payload, 0); // reserved

    payload.push_back(static_cast<uint8_t>(m_flatboxDuty));
    payload.push_back(11);
    payload.push_back(23);
    payload.push_back(31);
    payload.push_back(static_cast<uint8_t>(m_faults.size()));
    payload.push_back(activeFaultCount());
    put16(payload, 0); // I2C errors

    for (int port = 0; port < m_capabilities.usbDownstreamPortCount; port++)
        payload.push_back(1); // powered

    if (m_capabilities.hasUsbPowerFailureReporting)
    {
        payload.push_back(0); // DS1 fault
        payload.push_back(0); // DS2 fault
    }

    if (m_capabilities.hasConfigurableMotorRoles)
        payload.push_back(flapState());

    return payload;
}

/**
 * @brief What the front flap is doing as a whole, for a controller whose interface reports it.
 *
 * Worked out from whichever motors the assignment names, so a flap made of two parts is neither open nor
 * shut until both of them have arrived. What is not modelled is the delay between the parts - a real
 * controller starts them in order and reports Opening while a part waits out its delay, and here they
 * all start at once. That makes this the honest floor rather than the full behaviour: enough for the
 * driver to have a state to follow and to be held to.
 */
uint8_t SimulatedController::flapState() const
{
    const std::vector<int> parts = m_running.flapMotors();

    if (parts.empty())
        return 5; // NotConfigured

    bool open   = true;
    bool closed = true;

    for (int part : parts)
    {
        // The part's calibrated travel, which is what "fully open" means for it. Seeded from the
        // catalogue like every other identifier, so a simulator starts out calibrated - and a client that
        // writes a zero travel into it gets the state a real uncalibrated flap reports, which is worth
        // being able to reach.
        const int travel = travelOf(part);

        if (travel <= 0)
            return 6; // NotCalibrated

        const Motor &motor = m_motors[static_cast<size_t>(part)];

        if (m_motorStatus[static_cast<size_t>(part)] < 2)
            return (motor.target > motor.position) ? 1u : 3u; // Opening / Closing

        open   = open && (motor.position >= travel);
        closed = closed && (motor.position <= 0);
    }

    if (closed)
        return 0; // Closed

    return open ? 2u : 4u; // Open / Partial
}

MotorRoles SimulatedController::storedRoles() const
{
    if (!m_capabilities.hasConfigurableMotorRoles)
        return MotorRoles::legacy(m_capabilities);

    std::vector<int> flapParts;

    for (int part = 0; part < MotorRoles::flapPartsOn(m_capabilities); part++)
        flapParts.push_back(value(MotorRoles::FlapPartMotorDids[part]));

    return MotorRoles::build(m_capabilities, value(MotorRoles::FocuserMotorDid), value(MotorRoles::RotatorMotorDid),
                             flapParts, value(MotorRoles::FocuserStepMultiplierDid),
                             value(MotorRoles::RotatorStepsPerRevolutionDid));
}

int SimulatedController::travelOf(int index) const
{
    const int travel = value(motor::maximumPositionDid(index));

    // The sentinel the calibrator writes to mean "no end of travel is known" is not a distance, so it is
    // reported as no travel at all rather than as a flap that opens two billion steps from here.
    return (travel == motor::UncalibratedTravel) ? 0 : travel;
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
 * reproduce a particular unit's tuning. The three motor pages share a layout, so the low byte decides the
 * value for all of them.
 *
 * The motor assignment is not among them: it is seeded from the catalogue in the constructor, because
 * plausible is not good enough there. Zero for every one of them puts the rotator on the focuser's motor,
 * which the firmware refuses to hold, so a simulator that made one up would be modelling a controller
 * sitting on MotorConfigurationInvalid rather than a working one.
 */
int SimulatedController::defaultValue(uint32_t did)
{
    // The three motor pages: 0x00xx, 0x01xx and, from generation 4, 0x08xx. They share a layout, so the
    // low byte decides the value and only the learnt position differs between them.
    const bool isMotorPage = (did < 0x0200) || ((did >= 0x0800) && (did < 0x0900));

    if (isMotorPage)
    {
        const bool isFlap = (did >= 0x0100) && (did < 0x0200);

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

    // A learnt position written into a motor's page moves that motor, which is what a sync does. Asked by
    // page rather than by a single identifier, so that the second and third motors behave as the first.
    for (size_t index = 0; index < m_motors.size(); index++)
    {
        if (did != motor::lastPositionDid(static_cast<int>(index)))
            continue;

        m_motors[index].position = m_motors[index].target = m_dids[did];
    }

    return frame(command::DataIdentifier, Frame{ 1, payload[1], payload[2] });
}

Frame SimulatedController::motorCommand(const Frame &payload)
{
    // Addressed by the index the protocol numbers them with. A command for a motor this controller does
    // not have is refused rather than applied to a neighbouring one, which is what a driver that has
    // resolved a function to the wrong motor deserves to be told.
    if ((payload.size() < 2) || (static_cast<size_t>(payload[0]) >= m_motors.size()))
        return frame(command::Motor, Frame{ 0 });

    {
        Motor &affected = m_motors[payload[0]];

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

FunctionResponse SimulatedController::driveMotor(int index, uint8_t subFunction, int position)
{
    Motor &affected = m_motors[static_cast<size_t>(index)];

    if (subFunction == function::Halt)
    {
        affected.target = affected.position;
        return FunctionResponse::Ok;
    }

    if (subFunction == function::Sync)
    {
        affected.position = affected.target = position;
        return FunctionResponse::Ok;
    }

    const int travel = travelOf(index);

    if (travel <= 0)
        return FunctionResponse::NotCalibrated;

    if ((position < 0) || (position > travel))
        return FunctionResponse::InvalidParameter;

    // A motor that is running will not take a new target. It is the refusal a driver is most likely to
    // meet in normal use, which is exactly why it is worth being able to meet it here.
    if (m_motorStatus[static_cast<size_t>(index)] < 2)
        return FunctionResponse::Busy;

    affected.target = position;

    return FunctionResponse::Ok;
}

/**
 * @brief Commands what a motor drives rather than a motor, resolving the assignment the way the firmware
 *        does.
 *
 * The whole point of the command is that the caller does not say which motor, so everything here goes
 * through the assignment: a request for a function nothing is assigned to is refused rather than applied
 * to whichever motor a fixed mapping would have named.
 *
 * The running assignment, not the stored one. They are the same on a controller nobody has reconfigured
 * since it started, and on one that has been they are how the firmware behaves: the new assignment reads
 * back, and every command still goes where the old one sent it.
 */
Frame SimulatedController::functionCommand(const Frame &payload)
{
    const auto answer = [](FunctionResponse response)
    { return frame(command::Function, Frame{ static_cast<uint8_t>(response) }); };

    if (payload.size() < 2)
        return answer(FunctionResponse::InvalidParameter);

    const MotorRoles assignment = m_running;

    if (!assignment.isValid())
        return answer(FunctionResponse::NotConfigured);

    const uint8_t subFunction = payload[1];
    int position              = 0;

    if (payload.size() >= 6)
        position = static_cast<int>((payload[2] << 24) | (payload[3] << 16) | (payload[4] << 8) | payload[5]);

    if (payload[0] == function::Focuser)
    {
        if (!assignment.hasFocuser())
            return answer(FunctionResponse::NotConfigured);

        return answer(driveMotor(assignment.focuserMotor().value(), subFunction, position));
    }

    if (payload[0] == function::Rotator)
    {
        if (!assignment.hasRotator())
            return answer(FunctionResponse::NotConfigured);

        return answer(driveMotor(assignment.rotatorMotor().value(), subFunction, position));
    }

    if (payload[0] != function::Flap)
        return answer(FunctionResponse::InvalidParameter);

    if (!assignment.hasFlap())
        return answer(FunctionResponse::NotConfigured);

    // Every part is commanded, and the first refusal is the answer: a sequence that cannot be run in full
    // is not one to start half of. A real controller runs the parts in order with the configured delays
    // between them, which is the one thing this does not model.
    FunctionResponse result = FunctionResponse::Ok;

    for (int part : assignment.flapMotors())
    {
        const int travel = travelOf(part);
        FunctionResponse answered;

        if (subFunction == function::Open)
            answered = driveMotor(part, function::Move, travel);
        else if (subFunction == function::Close)
            answered = driveMotor(part, function::Move, 0);
        else if (subFunction == function::Halt)
            answered = driveMotor(part, function::Halt, 0);
        else
            answered = FunctionResponse::InvalidParameter;

        if ((result == FunctionResponse::Ok) && (answered != FunctionResponse::Ok))
            result = answered;
    }

    return answer(result);
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
    // Taken from the capability set rather than written out per generation, so that a freeze frame served
    // here is the length the driver's field list consumes by construction and not by agreement.
    const size_t snapshotLength = m_capabilities.dtcSnapshotLength;

    // Uptime, the two supplies, the two fans, then the auxiliary pair where there is one, then the
    // controller's own temperature and supply - and the clock sits behind all of it.
    const size_t yearOffset = 12 + (m_capabilities.hasAuxVoltageMonitoring ? 4u : 0u) + 4;

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
        snapshot[3]              = 0x64;
        snapshot[4]              = 0x2f;
        snapshot[5]              = 0x44;
        snapshot[yearOffset]     = 0x07;
        snapshot[yearOffset + 1] = 0xea;
        snapshot[yearOffset + 2] = 8;
        snapshot[yearOffset + 3] = 11;

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

SimulatedTransport::SimulatedTransport(int hardwareMajor, int interfaceMinor)
    : m_controller(hardwareMajor, interfaceMinor)
{
}

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
