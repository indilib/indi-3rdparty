/*
    ScopeLink INDI driver - configuration data identifiers

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#pragma once

#include "scopelink/device.h"
#include "scopelink/types.h"

#include <string>
#include <vector>

namespace scopelink
{

/** @brief Storage type of a configuration identifier, which decides its length and its range. */
enum class DidType
{
    UInt8,
    UInt16,
    UInt32,
    SInt8,
    SInt16,
    SInt32
};

/** @brief Which part of the controller a configuration identifier belongs to. */
enum class DidGroup
{
    FocuserMotor,
    FlapMotor,
    Fans,
    Temperature,
    SmartSwitch,
    Miscellaneous
};

/**
 * @brief One configuration data identifier held by the controller.
 *
 * Values are carried in a 32 bit signed integer throughout, so a UINT32 identifier is capped at
 * 2147483647 rather than 4294967295. That is not a limitation in practice - the only UINT32 identifiers
 * are motor positions, and the calibrator itself uses 0x7FFFFFFF as the uncalibrated travel limit - but
 * it means a value typed above that is rejected rather than silently wrapped to a negative position.
 */
class Did
{
    public:
        Did(uint32_t id, DidType type, DidGroup group, std::string description);

        uint32_t id() const { return m_id; }
        DidType type() const { return m_type; }
        DidGroup group() const { return m_group; }
        const std::string &description() const { return m_description; }

        /** @brief Element name this identifier is published under, for example DID_010F. */
        std::string elementName() const;

        int value() const { return m_value; }
        void setValue(int value) { m_value = value; }

        /** @brief True once a read has succeeded, which is also what says the controller holds it. */
        bool isAvailable() const { return m_available; }

        /** @brief Why the last read or write failed, empty after a successful one. */
        const std::string &lastError() const { return m_lastError; }

        int minimum() const { return minimumOf(m_type); }
        int maximum() const { return maximumOf(m_type); }
        bool isInRange(int value) const { return (value >= minimum()) && (value <= maximum()); }

        /** @brief Number of value bytes this identifier's type occupies on the wire. */
        size_t length() const;

        /**
         * @brief Reads the value from the controller.
         * @return True when the value was read; a failure leaves the previous value alone.
         *
         * Reported as a bool rather than thrown so that one unreadable identifier does not abandon a whole
         * list. The reason is kept in lastError() so that the caller can still tell the user what happened
         * - discarding it turns a protocol mismatch and an unplugged controller into the same silent nothing.
         */
        bool read(Device &device);

        /** @brief Writes the current value to the controller. */
        bool write(Device &device);

        static int minimumOf(DidType type);
        static int maximumOf(DidType type);

    private:
        void checkOperationAndDid(const Frame &response, uint8_t expectedOperation) const;

        uint32_t m_id;
        DidType m_type;
        DidGroup m_group;
        std::string m_description;

        int m_value{ 0 };
        bool m_available{ false };
        std::string m_lastError;
};

/**
 * @brief The configuration identifiers a given ScopeLink controller holds.
 *
 * Built from the capability set rather than hard coded, because a controller that does not hold an
 * identifier answers nothing at all for it: offering it anyway shows the user a value that can never be
 * read and, worse, accepts a write that goes nowhere.
 */
std::vector<Did> buildDidCatalogue(const Capabilities &capabilities);

/** @brief Human readable name of a parameter group, used as the property label. */
const char *didGroupName(DidGroup group);

/**
 * @brief The configuration file format the Windows parameter editor exports and imports.
 *
 * Deliberately byte compatible with it, so that a set of parameters saved on one platform loads on the
 * other. That matters most for support: the customer runs the Linux driver, the file arrives by email,
 * and it has to open in the tool the vendor already has.
 */
namespace did_file
{
/**
 * @brief Writes the current value of every identifier that has been read.
 * @param fileName File to write
 * @param deviceDescription Controller the values came from, recorded in the file
 * @param identifiers Identifiers to write
 * @throws std::runtime_error The file could not be written.
 *
 * Written to a temporary file and moved into place, so that a failure part way through does not leave a
 * truncated file where a good export used to be.
 */
void write(const std::string &fileName, const std::string &deviceDescription, const std::vector<Did> &identifiers);

/**
 * @brief Reads values back out of an exported file into a catalogue.
 * @return Number of identifiers whose value was taken from the file
 *
 * Identifiers the file does not mention are left alone, and identifiers the file mentions but this
 * controller does not hold are ignored: a file exported from a generation 3 unit is still useful on a
 * generation 2 one, for everything the two have in common.
 */
size_t read(const std::string &fileName, std::vector<Did> &identifiers);
} // namespace did_file

} // namespace scopelink
