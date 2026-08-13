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
    const Frame versionFrame  = protocol.transact(command::InterfaceVersion, {}, 8);
    const Frame hardwareFrame = protocol.transact(command::HardwareIdentification, {}, 16);
    const Frame softwareFrame = protocol.transact(command::SoftwareIdentification, {}, 44);

    Identification identification;

    identification.hardwareMajor  = byte_order::toByte(versionFrame, 4);
    identification.hardwareMinor  = byte_order::toByte(versionFrame, 5);
    identification.interfaceMajor = byte_order::toByte(versionFrame, 6);
    identification.interfaceMinor = byte_order::toByte(versionFrame, 7);

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
    capabilities.hasSmartSwitchDiagnostics         = major > 2;
    capabilities.hasAuxVoltageMonitoring           = major > 2;
    capabilities.hasConfigurableTemperatureLimits  = major > 2;
    capabilities.hasTemperatureSensorConfiguration = major > 2;

    // Only generation 3 records whether the sensor is fitted; on earlier generations it always is, and on
    // a generation 3 controller that does not answer the identifier the sensor is assumed to be fitted.
    capabilities.hasTemperatureSensor =
        (!capabilities.hasTemperatureSensorConfiguration || (temperatureSensorFitted < 0)) ?
            true :
            (temperatureSensorFitted != 0);

    capabilities.statusFrameLength = (major == 2) ? 56 : 60;

    // Both lengths are measured. A controller with one stored fault answers the fault store request with a
    // 6 byte response header plus one record of 6 header bytes and the freeze frame:
    //   generation 2 answers 46 bytes  ->  46 - 6 - 6 = 34
    //   generation 3 answers 60 bytes  ->  60 - 6 - 6 = 48
    // The 14 byte difference is the two auxiliary output voltages and the ten smart switch and USB fields
    // that only generation 3 records. The freeze frame decoder walks its field list in order rather than
    // using fixed offsets, so these two numbers and the capability flags above fully describe the layout.
    capabilities.dtcSnapshotLength = capabilities.hasSmartSwitchDiagnostics ? 48 : 34;

    return capabilities;
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

    const uint8_t motor1Status = byte_order::toByte(frame, 30);

    status.motor1Moving    = motor1Status < 2;
    status.motor1Direction = directionOf(motor1Status);
    status.motor1Load      = byte_order::toUInt(frame, 31, 1);
    status.motor1Position  = byte_order::toInt(frame, 32, 4);

    const uint8_t motor2Status = byte_order::toByte(frame, 36);

    status.motor2Moving    = motor2Status < 2;
    status.motor2Direction = directionOf(motor2Status);
    status.motor2Load      = byte_order::toUInt(frame, 37, 1);
    status.motor2Position  = byte_order::toInt(frame, 38, 4);

    status.powerSwitch1State = byte_order::toBool(frame, 42);
    status.powerSwitch2State = byte_order::toBool(frame, 43);

    status.flatboxDuty      = byte_order::toByte(frame, 48);
    status.cpuLoad          = byte_order::toByte(frame, 49);
    status.peakCpuLoad      = byte_order::toByte(frame, 50);
    status.stackUsage       = byte_order::toByte(frame, 51);
    status.storedFaultCount = byte_order::toByte(frame, 52);
    status.activeFaultCount = byte_order::toByte(frame, 53);
    status.i2cErrorCounter  = static_cast<int>(byte_order::toUInt(frame, 54, 2));

    if (capabilities.hasUsbHub)
    {
        status.usb1PowerActive  = byte_order::toBool(frame, 56);
        status.usb2PowerActive  = byte_order::toBool(frame, 57);
        status.usb1PowerFailure = byte_order::toBool(frame, 58);
        status.usb2PowerFailure = byte_order::toBool(frame, 59);
    }

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

    // Take the first sample synchronously: it proves that the frame layout derived from the identification
    // block actually matches what the controller sends, while the connection can still be refused cleanly.
    refreshStatus();

    log("connect", "Connected on " + m_protocol.healthSummary() + " - " + m_identification.toString());
}

void Device::reset()
{
    m_identified = false;
    m_hasStatus  = false;

    m_identification = Identification();
    m_capabilities   = Capabilities();
    m_status         = Status();
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

int Device::tryReadConfigurationByte(uint32_t did)
{
    try
    {
        const Frame request  = { 0x00, static_cast<uint8_t>(did >> 8), static_cast<uint8_t>(did & 0xff) };
        const Frame response = m_protocol.transact(command::DataIdentifier, request, 8);

        const uint8_t operation = byte_order::toByte(response, 4);
        const uint32_t answered = byte_order::toUInt(response, 5, 2);

        if ((operation != 0) || (answered != did))
        {
            log("capabilities",
                "DID " + std::to_string(did) + " was answered for a different identifier, ignoring the answer");
            return -1;
        }

        return static_cast<int>(byte_order::toUInt(response, 7, 1));
    }
    catch (const std::exception &error)
    {
        log("capabilities",
            std::string("A configuration identifier could not be read, assuming the default: ") + error.what());
        return -1;
    }
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
