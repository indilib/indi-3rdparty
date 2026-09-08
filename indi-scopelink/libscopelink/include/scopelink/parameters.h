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

#include <cstddef>
#include <string>
#include <vector>

namespace scopelink
{

/** @brief Storage type of a configuration identifier, which decides its length on the wire. */
enum class DidType
{
    UInt8,
    UInt16,
    UInt32,
    SInt8,
    SInt16,
    SInt32
};

/** @brief One selectable setting of an enumerated parameter. */
struct DidOption
{
        /** Value as it goes over the wire. */
        int value;

        /** Element name the setting is published under, for example MOTOR_DIR_INVERTED. */
        const char *name;

        /** What the user picks it by. */
        const char *label;
};

/** @brief A subsystem the parameters are shown under, published as one property. */
struct DidGroupInfo
{
        /** Referred to by DidDescriptor::group. */
        const char *id;

        /** Heading shown for the group. */
        const char *label;

        /** Name of the number property the group's numeric parameters are published under. */
        const char *property;

        /** Ascending, the order the groups are published in. */
        int order;
};

/**
 * @brief Everything the firmware says about one configuration parameter.
 *
 * Filled in by the generated table in parameters_generated.cpp, which the firmware's own params.json is
 * turned into. Nothing here is the driver's opinion: the limits, the units, the display scale, the
 * settings an enumerated parameter offers and the help text are all the firmware's, which is the point
 * of generating the table rather than keeping a second copy of it by hand.
 *
 * @c scale is a DISPLAY divisor and is never applied to what goes over the wire. A read and a write both
 * carry the raw value; scaling one of them would leave two conventions on one interface. The driver
 * divides by it to publish a value and multiplies back before writing one, so a velocity the controller
 * holds as 1500 is published as 1.500 rev/s and nothing below the property layer ever sees 1.5.
 *
 * An aggregate on purpose: the generated table is a file scope array of these, built at load time with no
 * constructor to run.
 */
struct DidDescriptor
{
        /** Data identifier the parameter is read and written by. */
        uint32_t id;

        /** Stable dotted key, the same one params.json is keyed on. */
        const char *name;

        /** Short name shown next to the value. */
        const char *label;

        /** Identifier of the group this belongs to. */
        const char *group;

        /** Unit of the displayed value, empty when it has none. */
        const char *unit;

        /** How the value is carried on the wire. */
        DidType type;

        /** True when the controller will not accept a write. */
        bool readOnly;

        /** Smallest raw value the controller accepts. */
        int minimum;

        /** Largest raw value the controller accepts. */
        int maximum;

        /** Raw value the firmware starts from. */
        int fallback;

        /** Display divisor, 1 when the raw value is shown as it is. */
        int scale;

        /** True when the limits were declared, false when they are what the storage type can hold. */
        bool ranged;

        /** True when the parameter is kept only for older clients. */
        bool deprecated;

        /** Settings this parameter offers, null when it is a number rather than a choice. */
        const DidOption *options;

        /** How many settings @c options holds. */
        size_t optionCount;

        /** Name of the switch property an enumerated parameter is published under, null otherwise. */
        const char *switchProperty;

        /** What the parameter does, in the firmware's own words. */
        const char *description;

        /** @brief True when this is one of a fixed set of settings rather than a number. */
        bool isEnumerated() const { return optionCount > 0; }

        /** @brief Element name the parameter is published under, for example DID_010F. */
        std::string elementName() const;

        /**
     * @brief How many decimal places a displayed value needs to show every raw step.
     *
     * A scale of 1000 needs three; a scale of 50 needs two, because a raw step of one is 0.02. Anything
     * whose steps do not land on a decimal at all is shown to six places, which is past the point where
     * the rounding would be visible.
     */
        int decimals() const;

        /** @brief The printf format a number property shows this parameter with. */
        std::string numberFormat() const;

        /** @brief Smallest step the displayed value can move by, which is one raw step. */
        double step() const { return 1.0 / scale; }

        /** @brief A raw value in display units, for a number property. */
        double toDisplay(int raw) const { return static_cast<double>(raw) / scale; }

        /**
     * @brief A displayed value back as the raw value the controller is sent.
     *
     * Rounded rather than truncated: a parameter that steps in 0.02 K has no way to hold 2.035, and the
     * nearer step is what somebody who typed that meant. Exactly half way between two steps lands on
     * whichever of them the double happens to be nearer, which is as meaningful an answer as there is.
     * Saturated at the int the value travels in, so
     * that a number arriving from a client far outside the range is refused by the range check rather
     * than wrapping into it.
     */
        int fromDisplay(double display) const;

        /** @brief The setting a raw value stands for, null when there is none. */
        const DidOption *findOption(int raw) const;

        /** @brief The label a group is shown under, empty when the group is not declared. */
        const char *groupLabel() const;

        /** @brief The parameter named so that it reads unambiguously on its own. */
        std::string qualifiedLabel() const;

        /** @brief How a raw value is shown: the setting's label, or the number with its unit. */
        std::string formatValue(int raw) const;

        /** @brief What the parameter accepts, as the tail of a sentence. */
        std::string limitsText() const;
};

/** @brief The whole firmware description, as generated from params.json. */
namespace catalogue
{
extern const char * const Product;
extern const int InterfaceMajor;
extern const int InterfaceMinor;

extern const DidGroupInfo Groups[];
extern const size_t GroupCount;

extern const DidDescriptor Parameters[];
extern const size_t ParameterCount;
} // namespace catalogue

/** @brief Looks a group up by its identifier, null when there is no such group. */
const DidGroupInfo *findDidGroup(const char *id);

/** @brief Looks a parameter up by its data identifier, null when the firmware has no such parameter. */
const DidDescriptor *findDidDescriptor(uint32_t id);

/**
 * @brief One configuration data identifier held by the controller.
 *
 * Values are carried in a 32 bit signed integer throughout, so a UINT32 identifier is capped at
 * 2147483647 rather than 4294967295. That is not a limitation in practice - the only UINT32 identifiers
 * are motor positions, and the calibrator itself uses 0x7FFFFFFF as the uncalibrated travel limit - but
 * it means a value above that is rejected rather than silently wrapped to a negative position.
 */
class Did
{
    public:
        /**
         * @param descriptor What the firmware says the parameter is
         *
         * The description is copied rather than pointed at, because it is not always the generated one:
         * a motor selection is narrowed to the motors the connected controller has - see
         * buildDidCatalogue - and the narrowed description has to live as long as the identifier does.
         * Everything a copy carries a pointer to is static, so a copy is as valid as the original.
         */
        explicit Did(const DidDescriptor &descriptor);

        const DidDescriptor &descriptor() const { return m_descriptor; }

        uint32_t id() const { return m_descriptor.id; }
        DidType type() const { return m_descriptor.type; }

        /** @brief Identifier of the group this is published under. */
        const char *group() const { return m_descriptor.group; }

        /** @brief The parameter named so that it reads unambiguously on its own, for logs and files. */
        const std::string &description() const { return m_description; }

        /** @brief Element name this identifier is published under, for example DID_010F. */
        std::string elementName() const;

        int value() const { return m_value; }
        void setValue(int value) { m_value = value; }

        /** @brief True once a read has succeeded, which is also what says the controller holds it. */
        bool isAvailable() const { return m_available; }

        /** @brief Why the last read or write failed, empty after a successful one. */
        const std::string &lastError() const { return m_lastError; }

        /** @brief Smallest and largest raw value the FIRMWARE accepts, which is usually narrower than the type. */
        int minimum() const { return m_descriptor.minimum; }
        int maximum() const { return m_descriptor.maximum; }

        bool isInRange(int value) const { return (value >= minimum()) && (value <= maximum()); }

        /**
         * @brief True when a value fits the storage type, whatever the firmware makes of it.
         *
         * The weaker of the two checks, and the one an imported file is held to. A file that disagrees
         * with the storage type is more likely to be from a different product than to be worth guessing
         * at; a value the current firmware happens to refuse is still recognisably this product's, and
         * refusing the write with a reason tells the user more than dropping the line silently would.
         */
        bool fitsStorage(int value) const;

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

        DidDescriptor m_descriptor;
        std::string m_description;

        int m_value{ 0 };
        bool m_available{ false };
        std::string m_lastError;
};

/**
 * @brief The configuration identifiers a given ScopeLink controller holds.
 *
 * A filter over the generated table rather than a list of its own, because the firmware description says
 * nothing about hardware generations - and a controller that does not hold an identifier answers nothing
 * at all for it: offering it anyway shows the user a value that can never be read and, worse, accepts a
 * write that goes nowhere.
 *
 * The motor selections are also narrowed to the motors this controller has, which is a filter over one
 * identifier's settings rather than over the list. See the note in the implementation for why the
 * generated description cannot be shown to a two motor controller as it stands.
 */
std::vector<Did> buildDidCatalogue(const Capabilities &capabilities);

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
