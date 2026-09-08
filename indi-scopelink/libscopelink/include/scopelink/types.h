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
 * Only the commands this driver uses are listed, so that a stray transaction cannot reach one that is
 * merely defined. Two of them take the controller off the USB bus - @ref JumpToBootloader and @ref Reset
 * - and both exist for the firmware update alone; nothing else in the driver sends either.
 */
namespace command
{
constexpr uint8_t Status                 = 0x01; /**< Read the cyclic status frame. */
constexpr uint8_t ClearDtc               = 0x03; /**< Clear a stored diagnostic trouble code. */
constexpr uint8_t ReadDtc                = 0x05; /**< Read all stored codes with their freeze frames. */
constexpr uint8_t HardwareIdentification = 0x07; /**< Read the hardware identification block. */
constexpr uint8_t DataIdentifier         = 0x08; /**< Read or write a configuration identifier. */
constexpr uint8_t SoftwareIdentification = 0x09; /**< Read the firmware identification string. */
constexpr uint8_t JumpToBootloader       = 0x0d; /**< Restart into the boot loader, for a firmware update. */
constexpr uint8_t Motor                  = 0x0e; /**< Motor control - move, sync, halt. */
constexpr uint8_t Fan                    = 0x0f; /**< Fan override and target control. */
constexpr uint8_t Reset                  = 0x10; /**< Reset the controller. */
constexpr uint8_t FlatboxDuty            = 0x11; /**< Set the flat box duty cycle. */
constexpr uint8_t InterfaceVersion       = 0x12; /**< Read the hardware and interface version numbers. */
constexpr uint8_t PowerSwitch            = 0x16; /**< Switch an auxiliary power output. */
constexpr uint8_t SetRtc                 = 0x17; /**< Set the controller's real time clock. */
constexpr uint8_t EepromStatistics       = 0x18; /**< Read the EEPROM wear statistics. */

/**
 * Command what a motor drives rather than a motor: move the focuser, open the flap.
 *
 * Interface 1.1 onwards. Which motors a request turns out to involve is the controller's business: this
 * driver reads the assignment to decide which devices to offer and does not carry it any further than
 * that. The front flap is the reason it works this way - a flap split into parts has halves that would
 * collide if they moved together, and the delays that keep them apart have to be run by something that
 * cannot be unplugged half way through.
 */
constexpr uint8_t Function = 0x1a;
} // namespace command

/** @brief What a @ref command::Function request can be addressed to, in byte 0 of its payload. */
namespace function
{
constexpr uint8_t Focuser = 0x00;
constexpr uint8_t Rotator = 0x01;

/** The telescope front flap, however many parts it is made of. */
constexpr uint8_t Flap = 0x02;

/**
 * @brief Sub-function selectors, in byte 1 of the payload.
 *
 * The first three share their numbering with @ref motor, which the firmware did deliberately: they mean
 * the same thing in both commands, and one set of numbers is one thing to get wrong instead of two.
 *
 * The firmware also offers 0x09, which reads a function's state back. It is not listed because nothing
 * sends it: the flap state arrives in every status frame, and a position is read off the motor the
 * assignment names - so a listed 0x09 would only be a sub-function a stray transaction could reach.
 */
constexpr uint8_t Halt  = 0x04; /**< Stop the function immediately. */
constexpr uint8_t Move  = 0x05; /**< Move to an absolute position, in motor steps. */
constexpr uint8_t Sync  = 0x06; /**< Adopt the supplied position without moving. */
constexpr uint8_t Open  = 0x07; /**< Open the front flap. */
constexpr uint8_t Close = 0x08; /**< Close the front flap. */
} // namespace function

/**
 * @brief What a @ref command::Function request is answered with, in byte 0 of the response payload.
 *
 * Every one of the refusals is a condition a user can act on, which is why they are decoded rather than
 * counted as an acknowledgement. The motor command answers with a byte too and nothing reads it, so a
 * move that generation of controller would not make was reported to the client as a move that had
 * started.
 */
enum class FunctionResponse
{
    /** No motor is assigned to the requested function on this unit. */
    NotConfigured = 0,

    /** A motor is assigned, but its travel has never been calibrated. */
    NotCalibrated = 1,

    /** The request named something the controller could not use. */
    InvalidParameter = 2,

    /** The motor is already moving and will not take a new target. */
    Busy = 3,

    /** The motor driver refused the request. */
    MotorRefused = 4,

    /** The request was accepted. */
    Ok = 5
};

/** @brief Sub-function selectors for @ref command::Motor. */
namespace motor
{
constexpr uint8_t Halt = 0x04; /**< Stop the motor immediately. */
constexpr uint8_t Move = 0x05; /**< Move the motor to an absolute position. */
constexpr uint8_t Sync = 0x06; /**< Adopt the supplied value as the current position without moving. */

/** How many motors any ScopeLink has parameters for, which is what @ref pageBase can be asked about. */
constexpr int KnownMotors = 3;

/**
 * @brief Where one motor's block of configuration identifiers starts.
 * @param index Motor index, as the protocol numbers them
 * @return The first identifier of its block, or zero for a motor there are no parameters for
 *
 * The three blocks are 0x0000, 0x0100 and 0x0800, which is not an arithmetic series: the third motor's
 * block was placed where there was room for it rather than where a pattern would have put it, so the
 * mapping is written out.
 *
 * Asked by index rather than by job on purpose. Which motor focuses is a setting from interface 1.1, so
 * the identifier holding the focuser's end of travel is 0x000F on one controller and 0x080F on another -
 * and there is no longer a constant that names it. Ask MotorRoles which motor, then ask this one which
 * identifiers.
 */
uint32_t pageBase(int index);

/** @brief The identifier holding a motor's calibrated end of travel. */
uint32_t maximumPositionDid(int index);

/**
 * @brief The identifier holding the position a motor was last seen at.
 *
 * Written on the way down and read on the way up, so that a controller knows where its mechanism is
 * standing after a power interruption.
 */
uint32_t lastPositionDid(int index);

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
