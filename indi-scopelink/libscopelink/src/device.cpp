/*
    ScopeLink INDI driver - identification, capabilities, status and device operations

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink/device.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <utility>

namespace scopelink
{

Identification Identification::read(Protocol &protocol)
{
    const Frame versionFrame = protocol.transact(command::InterfaceVersion, {}, 8);

    Identification identification = readCommon(protocol);

    identification.hardwareMajor  = byte_order::toByte(versionFrame, 4);
    identification.hardwareMinor  = byte_order::toByte(versionFrame, 5);
    identification.interfaceMajor = byte_order::toByte(versionFrame, 6);
    identification.interfaceMinor = byte_order::toByte(versionFrame, 7);

    return identification;
}

Identification Identification::readCommon(Protocol &protocol)
{
    const Frame hardwareFrame = protocol.transact(command::HardwareIdentification, {}, 16);
    const Frame softwareFrame = protocol.transact(command::SoftwareIdentification, {}, 44);

    Identification identification;

    for (size_t index = 4; index < 16; index++)
    {
        char pair[4];

        snprintf(pair, sizeof(pair), "%02X", byte_order::toByte(hardwareFrame, index));
        identification.hardwareIdentifier += pair;
    }

    std::string software(reinterpret_cast<const char *>(softwareFrame.data()) + 4, 40);

    // The firmware pads the field with nulls or spaces depending on the build, so both are trimmed.
    const size_t end = software.find_last_not_of(std::string("\0 ", 2));

    software = (end == std::string::npos) ? std::string() : software.substr(0, end + 1);

    std::transform(software.begin(), software.end(), software.begin(),
                   [](unsigned char character) { return static_cast<char>(std::toupper(character)); });

    identification.softwareIdentifier = software;

    return identification;
}

std::string Identification::toString() const
{
    return "HW " + std::to_string(hardwareMajor) + "." + std::to_string(hardwareMinor) + ", interface "
           + std::to_string(interfaceMajor) + "." + std::to_string(interfaceMinor) + ", id " + hardwareIdentifier
           + ", firmware " + softwareIdentifier;
}

bool Capabilities::isSupported(int hardwareMajor)
{
    return (hardwareMajor >= MinimumSupportedHardwareMajor) && (hardwareMajor <= MaximumSupportedHardwareMajor);
}

Capabilities Capabilities::of(const Identification &identification, int temperatureSensorFitted)
{
    if (!isSupported(identification.hardwareMajor))
    {
        throw UnsupportedDeviceError(
            "ScopeLink hardware generation " + std::to_string(identification.hardwareMajor)
            + " is not supported by this driver (supported: " + std::to_string(MinimumSupportedHardwareMajor) + " to "
            + std::to_string(MaximumSupportedHardwareMajor) + "). Please install a newer driver version.");
    }

    Capabilities capabilities;
    const int major = identification.hardwareMajor;

    capabilities.hardwareMajor                     = major;
    capabilities.hasUsbHub                         = major > 2;
    capabilities.hasSecondMotor                    = major > 1;
    capabilities.hasThirdMotor                     = major > 3;
    capabilities.hasPowerSwitches                  = major > 1;
    capabilities.hasSmartSwitchDiagnostics         = major > 2;
    capabilities.hasAuxVoltageMonitoring           = major > 2;
    capabilities.hasConfigurableTemperatureLimits  = major > 2;
    capabilities.hasTemperatureSensorConfiguration = major > 2;

    capabilities.motorCount = capabilities.hasThirdMotor ? 3 : (capabilities.hasSecondMotor ? 2 : 1);

    capabilities.usbDownstreamPortCount      = capabilities.hasUsbHub ? ((major > 3) ? 6 : 2) : 0;
    capabilities.hasUsbPowerFailureReporting = capabilities.hasUsbHub && (major < 4);

    // Only generation 3 records whether the sensor is fitted; on earlier generations it always is, and on
    // a generation 3 controller that does not answer the identifier the sensor is assumed to be fitted.
    capabilities.hasTemperatureSensor =
        (!capabilities.hasTemperatureSensorConfiguration || (temperatureSensorFitted < 0)) ?
            true :
            (temperatureSensorFitted != 0);

    // Interface 1.1 onwards. of() is handed the whole identification rather than the generation alone
    // exactly so that a capability can be taken from the firmware's interface version, for the ones the
    // board does not decide: a controller can be reflashed, and the generation cannot change.
    capabilities.hasConfigurableMotorRoles =
        (identification.interfaceMajor > 1)
        || ((identification.interfaceMajor == 1) && (identification.interfaceMinor >= 1));

    // The four digital inputs went out with interface 1.1, so what carries them is a firmware that
    // predates the motor roles rather than a particular board. Generation 1 is the one controller where
    // the status frame and the freeze frame disagree about them, and it is outside this driver's range.
    capabilities.hasDigitalInputs = !capabilities.hasConfigurableMotorRoles;

    // Built up in the order the frame is laid out rather than looked up per generation, because there is
    // no longer a generation whose length can be read off its number - and there is now a length that
    // belongs to no generation at all. The four measured lengths fall out of this: 44, 56, 60 and 65 for
    // generations 1 to 4. So does the fifth, a generation 3 controller on interface 1.1, which loses four
    // bytes of digital inputs and gains the flap state: 57.
    //
    // Reading these off the generation alone is what this replaced, and it was not a near miss. A 1.1
    // controller sends 57 where the old arithmetic expected 60, so Status::parse refused every frame it
    // sent and open() failed on its first sample - the driver could not connect to one at all.
    //
    // Status::parse walks this same list against a running offset, from these same capabilities, so a
    // field is added in one place. The length check on arrival is what catches the two disagreeing.
    capabilities.statusFrameLength = Protocol::HeaderLength
                                     + 26 // the readings every controller sends
                                     + (6 * static_cast<size_t>(capabilities.motorCount))
                                     + (capabilities.hasPowerSwitches ? 2u : 0u)
                                     + (capabilities.hasDigitalInputs ? 4u : 0u)
                                     + 8 // flat box, system load, fault counts, I2C errors
                                     + static_cast<size_t>(capabilities.usbDownstreamPortCount)
                                     + (capabilities.hasUsbPowerFailureReporting ? 2u : 0u)
                                     + (capabilities.hasConfigurableMotorRoles ? 1u : 0u);

    // The same construction for the freeze frame, in the order Fault::buildSnapshotFields names its
    // fields. Two of these lengths are measured: a controller with one stored fault answers the fault
    // store request with a 6 byte response header plus one record of 6 header bytes and the freeze frame,
    //   generation 2 answers 46 bytes  ->  46 - 6 - 6 = 34
    //   generation 3 answers 60 bytes  ->  60 - 6 - 6 = 48
    // The 14 byte difference is the two auxiliary output voltages and the ten smart switch and USB fields
    // that only generation 3 records.
    //
    // Generation 4 comes to 48 as well, and that is arithmetic rather than a shared layout: it spends the
    // four digital inputs and the two USB failure flags on a third motor and four more ports. So a length
    // proves nothing about content, which is why the decoder names its fields from these capabilities and
    // checks its own total against this number - that check, not this number, is what catches a mistake.
    //
    // A generation 3 controller on interface 1.1 loses the digital inputs and nothing else: 44.
    capabilities.dtcSnapshotLength = 4 + 2 + 2 + 2 + 2 // uptime, supply, sensor supply, two fans
                                     + (capabilities.hasAuxVoltageMonitoring ? 4u : 0u)
                                     + 2 + 2 // controller temperature and supply
                                     + 7     // the real time clock
                                     // Before the smart switch monitoring the flat box sat between two
                                     // motors, and it was five bytes however many motors the controller
                                     // actually drove. After it the motors are together and the flat box
                                     // follows them, so the block grows with the count.
                                     + (capabilities.hasSmartSwitchDiagnostics
                                            ? ((2 * static_cast<size_t>(capabilities.motorCount)) + 1)
                                            : 5u)
                                     + (capabilities.hasDigitalInputs ? 4u : 0u)
                                     + 2                                          // the two fan states
                                     + (capabilities.hasSmartSwitchDiagnostics ? 6u : 0u) // aux and errors
                                     + static_cast<size_t>(capabilities.usbDownstreamPortCount)
                                     + (capabilities.hasUsbPowerFailureReporting ? 2u : 0u);

    return capabilities;
}

MotorReading Status::motor(int index) const
{
    if ((index < 0) || (index >= static_cast<int>(motors.size())))
        return {};

    return motors[static_cast<size_t>(index)];
}

bool Status::usbPowerActive(int port) const
{
    if ((port < 0) || (port >= static_cast<int>(usbDownstreamPowerActive.size())))
        return false;

    return usbDownstreamPowerActive[static_cast<size_t>(port)];
}

long Status::ageMs() const
{
    const auto elapsed = std::chrono::steady_clock::now() - timestamp;

    return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

bool Status::isTemperaturePlausible(int raw)
{
    if (raw == SensorNotRespondingRaw)
        return false;

    return (raw >= MinimumPlausibleTemperatureRaw) && (raw <= MaximumPlausibleTemperatureRaw);
}

/** @brief Turns a motor status byte into a direction. Anything above 1 means the motor is not running. */
static MotorDirection directionOf(uint8_t statusByte)
{
    if (statusByte == 0)
        return MotorDirection::Clockwise;

    if (statusByte == 1)
        return MotorDirection::CounterClockwise;

    return MotorDirection::Halted;
}

Status Status::parse(const Frame &frame, const Capabilities &capabilities)
{
    if (frame.size() != capabilities.statusFrameLength)
    {
        throw ProtocolError("The controller returned a " + std::to_string(frame.size())
                            + " byte status frame, but hardware generation "
                            + std::to_string(capabilities.hardwareMajor) + " uses "
                            + std::to_string(capabilities.statusFrameLength) + " bytes.");
    }

    Status status;

    status.timestamp     = std::chrono::steady_clock::now();
    status.hardwareMajor = capabilities.hardwareMajor;

    // Fields common to every hardware generation
    status.supplyVoltage           = static_cast<int>(byte_order::toUInt(frame, 4, 2));
    status.sensorSupplyVoltage     = static_cast<int>(byte_order::toUInt(frame, 6, 2));
    status.fanAVoltage             = static_cast<int>(byte_order::toUInt(frame, 8, 2));
    status.fanBVoltage             = static_cast<int>(byte_order::toUInt(frame, 10, 2));
    status.controllerTemperature   = byte_order::toInt(frame, 12, 2);
    status.controllerSupplyVoltage = static_cast<int>(byte_order::toUInt(frame, 14, 2));

    status.fanAManualOverrideEnabled = byte_order::toBool(frame, 18);
    status.fanBManualOverrideEnabled = byte_order::toBool(frame, 19);
    status.fanAManualOverrideState   = byte_order::toBool(frame, 20);
    status.fanBManualOverrideState   = byte_order::toBool(frame, 21);
    status.fanATargetDT              = static_cast<int>(byte_order::toUInt(frame, 22, 2));
    status.fanBTargetDT              = static_cast<int>(byte_order::toUInt(frame, 24, 2));

    // Temperatures are unsigned 1/50 Kelvin, so a set high bit is not a negative reading.
    status.ambientTemperatureRaw = static_cast<int>(byte_order::toUInt(frame, 26, 2));
    status.mirrorTemperatureRaw  = static_cast<int>(byte_order::toUInt(frame, 28, 2));

    status.ambientTemperatureValid = isTemperaturePlausible(status.ambientTemperatureRaw);
    status.mirrorTemperatureValid  = isTemperaturePlausible(status.mirrorTemperatureRaw);

    // The motors sit in a run of six bytes each starting at 30, and there are as many of them as this
    // controller has. Read as a run rather than as a named pair because generation 4 has a third: two
    // more named fields would have been two more places for a caller to have to ask which motor it meant.
    //
    // Everything behind them is at a different offset on every generation, which is why the tail is read
    // against a running offset rather than against numbers written out per field. The third motor pushes
    // the whole of it six bytes along, and interface 1.1 took four bytes out of the middle of it and added
    // one to the end, so there is no set of constants that describes more than one controller.
    status.motors.reserve(static_cast<size_t>(capabilities.motorCount));

    for (int index = 0; index < capabilities.motorCount; index++)
    {
        const size_t base        = 30 + (6 * static_cast<size_t>(index));
        const uint8_t statusByte = byte_order::toByte(frame, base);

        MotorReading reading;

        reading.moving    = statusByte < 2;
        reading.direction = directionOf(statusByte);
        reading.load      = byte_order::toUInt(frame, base + 1, 1);
        reading.position  = byte_order::toInt(frame, base + 2, 4);

        status.motors.push_back(reading);
    }

    size_t at = 30 + (6 * static_cast<size_t>(capabilities.motorCount));

    if (capabilities.hasPowerSwitches)
    {
        status.powerSwitch1State = byte_order::toBool(frame, at);
        status.powerSwitch2State = byte_order::toBool(frame, at + 1);

        at += 2;
    }

    // Four digital inputs nothing was ever wired to. The firmware sent them as zeros and interface 1.1
    // dropped them; they are stepped over rather than read.
    if (capabilities.hasDigitalInputs)
        at += 4;

    status.flatboxDuty      = byte_order::toByte(frame, at);
    status.cpuLoad          = byte_order::toByte(frame, at + 1);
    status.peakCpuLoad      = byte_order::toByte(frame, at + 2);
    status.stackUsage       = byte_order::toByte(frame, at + 3);
    status.storedFaultCount = byte_order::toByte(frame, at + 4);
    status.activeFaultCount = byte_order::toByte(frame, at + 5);
    status.i2cErrorCounter  = static_cast<int>(byte_order::toUInt(frame, at + 6, 2));

    at += 8;

    // As many ports as the hub has: none, two on generation 3, six on generation 4. Read as a run for the
    // same reason the motors are - the count is what the capability set says, and half reading a hub with
    // more ports than the reader expected is how the flap state behind them ends up at the wrong offset.
    status.usbDownstreamPowerActive.reserve(static_cast<size_t>(capabilities.usbDownstreamPortCount));

    for (int port = 0; port < capabilities.usbDownstreamPortCount; port++)
        status.usbDownstreamPowerActive.push_back(byte_order::toBool(frame, at + static_cast<size_t>(port)));

    at += static_cast<size_t>(capabilities.usbDownstreamPortCount);

    if (capabilities.hasUsbPowerFailureReporting)
    {
        status.usb1PowerFailure = byte_order::toBool(frame, at);
        status.usb2PowerFailure = byte_order::toBool(frame, at + 1);

        at += 2;
    }

    // What the flap is doing as a whole, which the motor readings above cannot be asked: a part standing
    // still is opening while it waits out its delay, and a flap of two parts is neither open nor shut
    // until both have arrived. Earlier interfaces have one flap motor and no sequence, so this stays
    // Unknown and the flap is read off that motor.
    if (capabilities.hasConfigurableMotorRoles)
        status.flapState = static_cast<FlapState>(byte_order::toByte(frame, at));

    return status;
}

Device::Device(Protocol &protocol) : m_protocol(protocol) {}

void Device::setLogger(Logger logger)
{
    m_logger = std::move(logger);
}

void Device::open()
{
    reset();

    m_identification = Identification::read(m_protocol);

    // The capability set decides how every later frame is parsed, so the sensor identifier is read before
    // anything is derived from it. A generation that does not hold it is not asked.
    int temperatureSensorFitted = -1;

    if (m_identification.hardwareMajor > 2)
        temperatureSensorFitted = tryReadConfigurationByte(Capabilities::TemperatureSensorFittedDid);

    m_capabilities = Capabilities::of(m_identification, temperatureSensorFitted);
    m_identified   = true;

    if (!m_capabilities.hasTemperatureSensor)
        log("connect",
            "The controller reports no temperature sensor fitted, so the temperature readings are not offered");

    sendRtcTimestamp();

    // Which motor drives what. It decides which devices the caller is able to offer, so it is read here,
    // where a controller that will not answer for it still fails the connection cleanly rather than
    // producing a focuser that turns out to be the flap.
    //
    // Behind the clock check rather than in front of it, unlike the Windows driver, and for a reason that
    // is specific to this side: a port that has something other than a ScopeLink on it is rejected by that
    // check in one transaction, where six unanswered identifiers ahead of it would each be retried three
    // times first. A controller that predates the assignment sends nothing at all here.
    m_roles = MotorRoles::of(m_capabilities,
                             [this](uint32_t did, int length) { return tryReadConfigurationValue(did, length); });

    if (m_roles.isConfigurable() && !m_roles.isValid())
        log("connect", "The controller's motor assignment is unusable (" + m_roles.problem()
                           + "), so it will drive nothing until it is corrected");

    // Take the first sample synchronously: it proves that the frame layout derived from the identification
    // block actually matches what the controller sends, while the connection can still be refused cleanly.
    refreshStatus();

    log("connect",
        "Connected on " + m_protocol.healthSummary() + " - " + m_identification.toString() + ", " + m_roles.toString());
}

void Device::reset()
{
    m_identified = false;
    m_hasStatus  = false;

    m_identification = Identification();
    m_capabilities   = Capabilities();
    m_roles          = MotorRoles();
    m_status         = Status();
}

const MotorRoles &Device::refreshMotorRoles()
{
    if (!m_identified)
        throw CommunicationError("The ScopeLink controller has not been identified.");

    m_roles = MotorRoles::of(m_capabilities,
                             [this](uint32_t did, int length) { return tryReadConfigurationValue(did, length); });

    log("roles", "The motor assignment is now " + m_roles.toString());

    return m_roles;
}

bool Device::hasFreshStatus() const
{
    return m_hasStatus && !m_status.isStale(StatusMaxAgeMs);
}

const Status &Device::requireStatus() const
{
    if (!m_hasStatus)
        throw CommunicationError("No status data has been received from the ScopeLink controller.");

    if (m_status.isStale(StatusMaxAgeMs))
    {
        throw CommunicationError(
            "The ScopeLink status data is " + std::to_string(m_status.ageMs()) + " ms old, which exceeds the "
            + std::to_string(StatusMaxAgeMs) + " ms limit. The controller has stopped responding ("
            + std::to_string(m_protocol.consecutiveFailures()) + " consecutive failed transactions).");
    }

    return m_status;
}

const Status &Device::refreshStatus()
{
    if (!m_identified)
        throw CommunicationError("The ScopeLink controller has not been identified.");

    const Frame frame = m_protocol.transact(command::Status, {}, m_capabilities.statusFrameLength);

    m_status    = Status::parse(frame, m_capabilities);
    m_hasStatus = true;

    return m_status;
}

/** @brief Builds the payload of a positioning motor command. */
static Frame motorPayload(int motorId, uint8_t subCommand, int position)
{
    const uint32_t value = static_cast<uint32_t>(position);

    return Frame{ static_cast<uint8_t>(motorId),
                  subCommand,
                  static_cast<uint8_t>((value >> 24) & 0xff),
                  static_cast<uint8_t>((value >> 16) & 0xff),
                  static_cast<uint8_t>((value >> 8) & 0xff),
                  static_cast<uint8_t>(value & 0xff) };
}

void Device::moveMotor(int motorId, int position)
{
    m_protocol.transact(command::Motor, motorPayload(motorId, motor::Move, position), 5);
}

void Device::syncMotor(int motorId, int position)
{
    m_protocol.transact(command::Motor, motorPayload(motorId, motor::Sync, position), 5);
}

void Device::haltMotor(int motorId)
{
    m_protocol.transact(command::Motor, Frame{ static_cast<uint8_t>(motorId), motor::Halt }, 5);
}

FunctionResponse Device::sendFunction(uint8_t functionId, uint8_t subFunction)
{
    const Frame response = m_protocol.transact(command::Function, Frame{ functionId, subFunction }, 5);

    return static_cast<FunctionResponse>(byte_order::toByte(response, 4));
}

FunctionResponse Device::sendFunction(uint8_t functionId, uint8_t subFunction, int position)
{
    const uint32_t value = static_cast<uint32_t>(position);

    const Frame payload = { functionId,
                            subFunction,
                            static_cast<uint8_t>((value >> 24) & 0xff),
                            static_cast<uint8_t>((value >> 16) & 0xff),
                            static_cast<uint8_t>((value >> 8) & 0xff),
                            static_cast<uint8_t>(value & 0xff) };

    const Frame response = m_protocol.transact(command::Function, payload, 5);

    return static_cast<FunctionResponse>(byte_order::toByte(response, 4));
}

void Device::requireFunction(const char *name, FunctionResponse response)
{
    if (response == FunctionResponse::Ok)
        return;

    const std::string what(name);
    std::string reason;

    switch (response)
    {
        case FunctionResponse::NotConfigured:
            reason = "this controller has no " + what
                     + ": no motor is assigned to it. If one was assigned since the controller last "
                       "started, restart the controller - it reads its configuration when it starts";
            break;

        case FunctionResponse::NotCalibrated:
            reason = "the " + what + " has not been calibrated, so it has no travel to move within";
            break;

        case FunctionResponse::InvalidParameter:
            reason = "the position is outside the " + what + "'s travel";
            break;

        case FunctionResponse::Busy:
            reason = "the " + what + " is already moving and will not take a new target until it stops";
            break;

        case FunctionResponse::MotorRefused:
            reason = "the motor driving the " + what + " refused the command";
            break;

        default:
            reason = "the controller answered the " + what + " request with an unknown code "
                     + std::to_string(static_cast<int>(response));
            break;
    }

    throw CommunicationError("The ScopeLink controller refused the request: " + reason + ".");
}

void Device::moveFocuser(int position)
{
    requireFunction("focuser", sendFunction(function::Focuser, function::Move, position));
}

void Device::syncFocuser(int position)
{
    requireFunction("focuser", sendFunction(function::Focuser, function::Sync, position));
}

void Device::haltFocuser()
{
    requireFunction("focuser", sendFunction(function::Focuser, function::Halt));
}

void Device::moveRotator(int position)
{
    requireFunction("rotator", sendFunction(function::Rotator, function::Move, position));
}

void Device::haltRotator()
{
    requireFunction("rotator", sendFunction(function::Rotator, function::Halt));
}

void Device::openFlap()
{
    requireFunction("front flap", sendFunction(function::Flap, function::Open));
}

void Device::closeFlap()
{
    requireFunction("front flap", sendFunction(function::Flap, function::Close));
}

void Device::haltFlap()
{
    requireFunction("front flap", sendFunction(function::Flap, function::Halt));
}

void Device::setFanOverrideEnabled(int fanIndex, bool enabled)
{
    // Sub-function 0 selects the override enable flag, sub-function 1 the commanded state.
    m_protocol.transact(command::Fan,
                        Frame{ 0x00, static_cast<uint8_t>(fanIndex), static_cast<uint8_t>(enabled ? 1 : 0) }, 5);
}

void Device::setFanOverrideState(int fanIndex, bool on)
{
    m_protocol.transact(command::Fan, Frame{ 0x01, static_cast<uint8_t>(fanIndex), static_cast<uint8_t>(on ? 1 : 0) },
                        5);
}

void Device::setFanTarget(int fanIndex, double targetKelvin)
{
    // The controller holds fan targets in the same 1/50 Kelvin units as the temperatures.
    const unsigned raw = static_cast<unsigned>((targetKelvin * 50.0) + 0.5);

    m_protocol.transact(command::Fan,
                        Frame{ 0x02, static_cast<uint8_t>(fanIndex), static_cast<uint8_t>((raw >> 8) & 0xff),
                               static_cast<uint8_t>(raw & 0xff) },
                        5);
}

void Device::setFlatboxDuty(int percent)
{
    if ((percent < 0) || (percent > 100))
        throw CommunicationError("A flat box duty cycle of " + std::to_string(percent)
                                 + " percent is outside 0 to 100.");

    m_protocol.transact(command::FlatboxDuty, Frame{ 0x00, static_cast<uint8_t>(percent) }, 5);
}

void Device::setPowerSwitch(int index, bool on)
{
    m_protocol.transact(command::PowerSwitch, Frame{ static_cast<uint8_t>(index), static_cast<uint8_t>(on ? 1 : 0) },
                        5);
}

/** @brief Sends a request whose whole answer is one byte saying whether the controller accepted it. */
static bool requestRestart(Protocol &protocol, uint8_t commandId)
{
    const Frame response = protocol.transact(commandId, {}, 5);

    return byte_order::toByte(response, 4) == 1;
}

bool Device::requestJumpToBootloader()
{
    try
    {
        const bool accepted = requestRestart(m_protocol, command::JumpToBootloader);

        log("firmware", accepted ? "The controller accepted the request to restart into its boot loader"
                                 : "The controller declined the request to restart into its boot loader");

        return accepted;
    }
    catch (const std::exception &error)
    {
        log("firmware", std::string("No answer to the boot loader request: ") + error.what());

        return false;
    }
}

bool Device::requestReset()
{
    try
    {
        const bool accepted = requestRestart(m_protocol, command::Reset);

        log("firmware", accepted ? "The controller accepted the restart request"
                                 : "The controller declined the restart request");

        return accepted;
    }
    catch (const std::exception &error)
    {
        log("firmware", std::string("No answer to the restart request: ") + error.what());

        return false;
    }
}

std::optional<uint32_t> Device::tryReadConfigurationValue(uint32_t did, int length)
{
    // Four header bytes, the operation and the two identifier bytes, then the value itself.
    const size_t expected = 7 + static_cast<size_t>(length);

    try
    {
        const Frame request  = { 0x00, static_cast<uint8_t>(did >> 8), static_cast<uint8_t>(did & 0xff) };
        const Frame response = m_protocol.transact(command::DataIdentifier, request, expected);

        const uint8_t operation = byte_order::toByte(response, 4);
        const uint32_t answered = byte_order::toUInt(response, 5, 2);

        if ((operation != 0) || (answered != did))
        {
            log("capabilities",
                "DID " + std::to_string(did) + " was answered for a different identifier, ignoring the answer");
            return {};
        }

        return byte_order::toUInt(response, 7, static_cast<size_t>(length));
    }
    catch (const std::exception &error)
    {
        log("capabilities", std::string("A configuration identifier could not be read: ") + error.what());
        return {};
    }
}

int Device::tryReadConfigurationByte(uint32_t did)
{
    const std::optional<uint32_t> value = tryReadConfigurationValue(did, 1);

    return value.has_value() ? static_cast<int>(value.value()) : -1;
}

Frame Device::transact(uint8_t commandId, const Frame &payload, size_t expectedResponseLength)
{
    return m_protocol.transact(commandId, payload, expectedResponseLength);
}

Frame Device::transactVariable(uint8_t commandId, const Frame &payload, size_t minimumResponseLength)
{
    return m_protocol.transactVariable(commandId, payload, minimumResponseLength);
}

std::string Device::healthSummary() const
{
    return m_protocol.healthSummary()
           + ", status age: " + (m_hasStatus ? (std::to_string(m_status.ageMs()) + " ms") : std::string("no data"));
}

void Device::sendRtcTimestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};

    localtime_r(&now, &local);

    const int year = local.tm_year + 1900;

    const Frame payload = { static_cast<uint8_t>((year >> 8) & 0xff), static_cast<uint8_t>(year & 0xff),
                            static_cast<uint8_t>(local.tm_mon + 1),   static_cast<uint8_t>(local.tm_mday),
                            static_cast<uint8_t>(local.tm_hour),      static_cast<uint8_t>(local.tm_min),
                            static_cast<uint8_t>(local.tm_sec) };

    const Frame response = m_protocol.transact(command::SetRtc, payload, 5);

    if ((response[3] != 1) || (response[4] != 1))
        throw ProtocolError("The controller rejected the real time clock update, so this is probably not a ScopeLink.");
}

void Device::log(const char *scope, const std::string &message) const
{
    if (m_logger)
        m_logger(scope, message);
}

} // namespace scopelink
