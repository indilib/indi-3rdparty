/*
    ScopeLink INDI driver - shared types, errors and wire format helpers

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace scopelink
{

/** One whole frame, header included. */
using Frame = std::vector<uint8_t>;

/**
 * @brief Where a log line came from and what it said.
 *
 * libscopelink writes no output of its own so that it can be linked into an INDI driver, a command line
 * tool or a test binary without any of them fighting over stdout. Whoever owns the library installs one
 * of these and decides where the text goes.
 */
using Logger = std::function<void(const char *scope, const std::string &message)>;

/** @brief The controller could not be reached, or did not answer usefully. */
class CommunicationError : public std::runtime_error
{
    public:
        explicit CommunicationError(const std::string &what) : std::runtime_error(what) {}
};

/** @brief The controller answered, but not with something this driver can parse. */
class ProtocolError : public CommunicationError
{
    public:
        explicit ProtocolError(const std::string &what) : CommunicationError(what) {}
};

/**
 * @brief The bytes did not arrive in time.
 *
 * Distinguished from the other communication failures because it is the one that says nothing about the
 * port: a single lost frame times out exactly like a healthy link with a slow controller on it, so the
 * protocol layer retries a timeout on the open port instead of recycling the port underneath it.
 */
class TimeoutError : public CommunicationError
{
    public:
        explicit TimeoutError(const std::string &what) : CommunicationError(what) {}
};

/** @brief Whatever is on the other end of the port is not a ScopeLink this driver supports. */
class UnsupportedDeviceError : public CommunicationError
{
    public:
        explicit UnsupportedDeviceError(const std::string &what) : CommunicationError(what) {}
};

/**
 * @brief ScopeLink command identifiers, carried in byte 2 of every request and echoed in byte 2 of every
 *        response.
 *
 * Only the commands this driver uses are listed. Anything that would take the controller off the USB bus
 * is deliberately left out rather than defined and unused, so a stray transaction cannot reach one.
 */
namespace command
{
constexpr uint8_t Status                 = 0x01; /**< Read the cyclic status frame. */
constexpr uint8_t ClearDtc               = 0x03; /**< Clear a stored diagnostic trouble code. */
constexpr uint8_t ReadDtc                = 0x05; /**< Read all stored codes with their freeze frames. */
constexpr uint8_t HardwareIdentification = 0x07; /**< Read the hardware identification block. */
constexpr uint8_t DataIdentifier         = 0x08; /**< Read or write a configuration identifier. */
constexpr uint8_t SoftwareIdentification = 0x09; /**< Read the firmware identification string. */
constexpr uint8_t Motor                  = 0x0e; /**< Motor control - move, sync, halt. */
constexpr uint8_t Fan                    = 0x0f; /**< Fan override and target control. */
constexpr uint8_t Reset                  = 0x10; /**< Reset the controller. */
constexpr uint8_t FlatboxDuty            = 0x11; /**< Set the flat box duty cycle. */
constexpr uint8_t InterfaceVersion       = 0x12; /**< Read the hardware and interface version numbers. */
constexpr uint8_t PowerSwitch            = 0x16; /**< Switch an auxiliary power output. */
constexpr uint8_t SetRtc                 = 0x17; /**< Set the controller's real time clock. */
constexpr uint8_t EepromStatistics       = 0x18; /**< Read the EEPROM wear statistics. */
} // namespace command

/** @brief Sub-function selectors for @ref command::Motor. */
namespace motor
{
constexpr uint8_t Halt = 0x04; /**< Stop the motor immediately. */
constexpr uint8_t Move = 0x05; /**< Move the motor to an absolute position. */
constexpr uint8_t Sync = 0x06; /**< Adopt the supplied value as the current position without moving. */

constexpr int Focuser = 0; /**< Motor identifier of the focuser drive. */
constexpr int Flap    = 1; /**< Motor identifier of the front flap drive. */

/**
 * @brief Travel limit a motor reports when it has never been calibrated.
 *
 * The calibrator writes this into the maximum position identifier to mean "no end of travel is known",
 * so it is a sentinel rather than a distance. Taken as a distance it would send the motor towards a
 * position two billion steps away, which the mechanism answers by running into its end stop.
 */
constexpr int UncalibratedTravel = 0x7fffffff;
} // namespace motor

/**
 * @brief Big endian accessors for the ScopeLink wire format.
 *
 * Every accessor validates its bounds and reports a ProtocolError that names the offending offset, so a
 * frame that is shorter than expected produces a diagnosable error rather than reading past the end of
 * the buffer.
 */
namespace byte_order
{
/** @brief Reads an unsigned big endian integer of 1 to 4 bytes. */
uint32_t toUInt(const Frame &data, size_t offset, size_t size);

/** @brief Reads a two's complement big endian integer of 1 to 4 bytes, sign extended to 32 bits. */
int32_t toInt(const Frame &data, size_t offset, size_t size);

/** @brief Reads a single byte. */
uint8_t toByte(const Frame &data, size_t offset);

/** @brief Reads a single byte as a flag, true when it is non-zero. */
bool toBool(const Frame &data, size_t offset);
} // namespace byte_order

/** @brief Formats a byte buffer as space separated upper case hex, for logs and error text. */
std::string toHex(const Frame &data, size_t maximumBytes = 64);

} // namespace scopelink
