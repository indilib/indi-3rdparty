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

#include "scopelink/parameters.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace scopelink
{

/** Number of response bytes read after a write. */
static constexpr size_t WriteAcknowledgementLength = 7;

/** Number of bytes that precede the value in a read response. */
static constexpr size_t ReadValueOffset = 7;

// ---------------------------------------------------------------------------------------------------
// What the firmware says about a parameter
// ---------------------------------------------------------------------------------------------------

std::string DidDescriptor::elementName() const
{
    char name[16];

    snprintf(name, sizeof(name), "DID_%04X", id);

    return name;
}

int DidDescriptor::decimals() const
{
    long power = 1;

    for (int places = 0; places <= 6; places++)
    {
        if ((power % scale) == 0)
            return places;

        power *= 10;
    }

    return 6;
}

std::string DidDescriptor::numberFormat() const
{
    char format[8];

    snprintf(format, sizeof(format), "%%.%df", decimals());

    return format;
}

int DidDescriptor::fromDisplay(double display) const
{
    const double raw = std::round(display * scale);

    // Saturated rather than cast straight through: a cast that does not fit is undefined, and a client
    // is free to send any double at all. What comes back out of the ends is refused by the range check.
    if (raw <= static_cast<double>(std::numeric_limits<int>::min()))
        return std::numeric_limits<int>::min();

    if (raw >= static_cast<double>(std::numeric_limits<int>::max()))
        return std::numeric_limits<int>::max();

    return static_cast<int>(raw);
}

const DidOption *DidDescriptor::findOption(int raw) const
{
    for (size_t index = 0; index < optionCount; index++)
    {
        if (options[index].value == raw)
            return &options[index];
    }

    return nullptr;
}

const char *DidDescriptor::groupLabel() const
{
    const DidGroupInfo *info = findDidGroup(group);

    return (info == nullptr) ? "" : info->label;
}

std::string DidDescriptor::qualifiedLabel() const
{
    const std::string heading = groupLabel();

    return heading.empty() ? std::string(label) : (heading + " - " + label);
}

std::string DidDescriptor::formatValue(int raw) const
{
    if (isEnumerated())
    {
        const DidOption *option = findOption(raw);

        // A value no setting stands for is shown as the number it is. It means the controller holds
        // something this build does not know about, and showing the nearest setting would hide that.
        if (option != nullptr)
            return option->label;

        return std::to_string(raw) + " (not a known setting)";
    }

    char text[48];

    snprintf(text, sizeof(text), numberFormat().c_str(), toDisplay(raw));

    const std::string number(text);

    return (unit[0] == 0) ? number : (number + " " + unit);
}

std::string DidDescriptor::limitsText() const
{
    if (isEnumerated())
        return std::to_string(optionCount) + " settings";

    return formatValue(minimum) + " to " + formatValue(maximum);
}

const DidGroupInfo *findDidGroup(const char *id)
{
    if (id == nullptr)
        return nullptr;

    for (size_t index = 0; index < catalogue::GroupCount; index++)
    {
        if (strcmp(catalogue::Groups[index].id, id) == 0)
            return &catalogue::Groups[index];
    }

    return nullptr;
}

const DidDescriptor *findDidDescriptor(uint32_t id)
{
    for (size_t index = 0; index < catalogue::ParameterCount; index++)
    {
        if (catalogue::Parameters[index].id == id)
            return &catalogue::Parameters[index];
    }

    return nullptr;
}

// ---------------------------------------------------------------------------------------------------
// One identifier on one controller
// ---------------------------------------------------------------------------------------------------

Did::Did(const DidDescriptor &descriptor)
    : m_descriptor(descriptor), m_description(descriptor.qualifiedLabel()), m_value(descriptor.fallback)
{
}

std::string Did::elementName() const
{
    return m_descriptor.elementName();
}

bool Did::fitsStorage(int value) const
{
    return (value >= minimumOf(m_descriptor.type)) && (value <= maximumOf(m_descriptor.type));
}

size_t Did::length() const
{
    switch (m_descriptor.type)
    {
        case DidType::UInt8:
        case DidType::SInt8:
            return 1;

        case DidType::UInt16:
        case DidType::SInt16:
            return 2;

        default:
            return 4;
    }
}

int Did::minimumOf(DidType type)
{
    switch (type)
    {
        case DidType::SInt8:
            return -128;

        case DidType::SInt16:
            return -32768;

        case DidType::SInt32:
            return std::numeric_limits<int>::min();

        default:
            return 0;
    }
}

int Did::maximumOf(DidType type)
{
    switch (type)
    {
        case DidType::UInt8:
            return 255;

        case DidType::SInt8:
            return 127;

        case DidType::UInt16:
            return 65535;

        case DidType::SInt16:
            return 32767;

        default:
            return std::numeric_limits<int>::max();
    }
}

bool Did::read(Device &device)
{
    try
    {
        const Frame request  = { 0x00, static_cast<uint8_t>(m_descriptor.id >> 8),
                                 static_cast<uint8_t>(m_descriptor.id & 0xff) };
        const Frame response = device.transact(command::DataIdentifier, request, length() + ReadValueOffset);

        checkOperationAndDid(response, 0);

        switch (m_descriptor.type)
        {
            case DidType::UInt8:
                m_value = static_cast<int>(byte_order::toUInt(response, ReadValueOffset, 1));
                break;

            case DidType::SInt8:
                m_value = byte_order::toInt(response, ReadValueOffset, 1);
                break;

            case DidType::UInt16:
                m_value = static_cast<int>(byte_order::toUInt(response, ReadValueOffset, 2));
                break;

            case DidType::SInt16:
                m_value = byte_order::toInt(response, ReadValueOffset, 2);
                break;

            case DidType::UInt32:
            case DidType::SInt32:
                m_value = byte_order::toInt(response, ReadValueOffset, 4);
                break;
        }

        m_available = true;
        m_lastError.clear();

        return true;
    }
    catch (const std::exception &error)
    {
        m_lastError = error.what();
        return false;
    }
}

bool Did::write(Device &device)
{
    try
    {
        if (!isInRange(m_value))
        {
            throw ProtocolError(m_description + " cannot hold " + m_descriptor.formatValue(m_value) + "; it accepts "
                                + m_descriptor.limitsText() + ".");
        }

        Frame payload        = { 0x01, static_cast<uint8_t>(m_descriptor.id >> 8),
                                 static_cast<uint8_t>(m_descriptor.id & 0xff) };
        const uint32_t value = static_cast<uint32_t>(m_value);

        switch (length())
        {
            case 1:
                payload.push_back(static_cast<uint8_t>(value & 0xff));
                break;

            case 2:
                payload.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
                payload.push_back(static_cast<uint8_t>(value & 0xff));
                break;

            default:
                payload.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
                payload.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
                payload.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
                payload.push_back(static_cast<uint8_t>(value & 0xff));
                break;
        }

        // Measured against hardware (HW 3.0, interface 1.0): a write to DID 0x0202 is acknowledged with
        // AA 55 08 03 01 02 02 - preamble, command echo, payload length 3, operation 1 and the echoed
        // identifier, with no data echo.
        checkOperationAndDid(device.transact(command::DataIdentifier, payload, WriteAcknowledgementLength), 1);

        m_available = true;
        m_lastError.clear();

        return true;
    }
    catch (const std::exception &error)
    {
        m_lastError = error.what();
        return false;
    }
}

void Did::checkOperationAndDid(const Frame &response, uint8_t expectedOperation) const
{
    const uint8_t operation = byte_order::toByte(response, 4);

    if (operation != expectedOperation)
    {
        throw ProtocolError(m_description + " returned operation code " + std::to_string(operation) + ", expected "
                            + std::to_string(expectedOperation) + ".");
    }

    const uint32_t answered = byte_order::toUInt(response, 5, 2);

    if (answered != m_descriptor.id)
    {
        char text[80];

        snprintf(text, sizeof(text), "Requested DID 0x%04X but the controller answered for DID 0x%04X.",
                 m_descriptor.id, answered);
        throw ProtocolError(text);
    }
}

/**
 * @brief True when this controller holds the identifier at all.
 *
 * Decided here rather than taken from the firmware description, because that description says nothing
 * about hardware generations: it describes one firmware, and every parameter in it is one this firmware
 * has. What differs between the units in the field is which of them the controller answers for, and the
 * only thing that knows is the capability set.
 *
 * Two axes, and they answer different questions. The hardware generation says what the board was built
 * with - how many motors, whether there is a USB hub - and cannot change. The interface version says what
 * the firmware can be asked, and does change, because a controller can be reflashed. Reading them as one
 * is the mistake to avoid: the generation 3 board runs both interface 1.0 and interface 1.1 firmware.
 *
 * By identifier rather than by group on purpose. The learnt position of each motor sits in a group of its
 * own but is gated with its motor, the flap's third part sits with the other two but needs a motor none of
 * them has, and the temperature sensor declaration sits with the temperature filters but is the one
 * identifier of that group a generation 2 unit does not answer.
 *
 * params.json has begun to record part of this itself - the 0x0400 block carries "since_if": [1, 1] and a
 * capability name - but only the interface half of it, and only for that block, and the generated table
 * does not carry either field across. Until it does, this stays the one thing about the parameter set
 * written out by hand, and it is kept saying the same thing as the Windows driver's DIDCatalog.IsHeldBy.
 */
static bool isHeldBy(uint32_t id, const Capabilities &capabilities)
{
    // The whole of the third motor's block, its learnt position included. The motor count decides this
    // rather than the interface version: the first board built with three motors is generation 4, and the
    // generation 3 firmware speaks interface 1.1 with two of them, so the two questions have different
    // answers on a controller that exists.
    if ((id >= 0x0800) && (id <= 0x080F))
        return capabilities.hasThirdMotor;

    // The third part of the front flap, and again it is the motor count that decides it rather than the
    // interface version. A part needs a motor to drive it, and on a two motor controller two parts would
    // leave nothing to focus with, so on that controller these three could only ever answer "not used".
    // The firmware still holds the fields and still reads them, keeping the default that says exactly that.
    if ((id >= 0x0426) && (id <= 0x0428))
        return capabilities.hasConfigurableMotorRoles && capabilities.hasThirdMotor;

    // Which motor drives the focuser, the rotator and each part of the front flap, with the step
    // multiplier and the steps per revolution that go with them. Interface 1.1 onwards: before it the
    // assignment was a fact about the hardware rather than a setting, so there was nothing to hold.
    if ((id >= 0x0400) && (id <= 0x0442))
        return capabilities.hasConfigurableMotorRoles;

    // These three were once listed alongside the flap motor identifiers, which put them on every
    // controller from generation 2 onwards. A generation 2 controller does not answer any of them.
    if ((id >= 0x0600) && (id <= 0x0602))
        return capabilities.hasSmartSwitchDiagnostics;

    if ((id >= 0x0700) && (id <= 0x0701))
        return capabilities.hasConfigurableTemperatureLimits;

    // Listed even on a unit that has no sensor fitted, because it is how a sensor gets declared once one
    // is. The two filter coefficients stay listed for the same reason: the controller holds and answers
    // them either way, and the editor's job is to show what the controller has, not what it is using.
    if (id == Capabilities::TemperatureSensorFittedDid)
        return capabilities.hasTemperatureSensorConfiguration;

    return true;
}

/** @brief The suffix the name of every motor selection parameter ends in. */
static const char MotorSelectionSuffix[] = ".motor_id";

/**
 * @brief The motor selections a controller with a given motor count offers.
 *
 * One list per motor count, built once from the generated description so that the wording stays the
 * firmware's rather than becoming a second copy of it here. They have static storage duration because a
 * narrowed description points at one of them and is copied into every Did built from it.
 */
static const std::vector<DidOption> &narrowedSelections(const DidDescriptor &descriptor, int motorCount)
{
    static std::vector<std::vector<DidOption>> lists(static_cast<size_t>(motor::KnownMotors) + 1);

    std::vector<DidOption> &list = lists[static_cast<size_t>(motorCount)];

    if (list.empty())
    {
        for (int index = 0; index < motorCount; index++)
            list.push_back(descriptor.options[index]);

        // "Not used" keeps its name and its label and moves down to the value this controller spells it
        // with. It is the last setting of the generated list, which is where the firmware puts it.
        DidOption none = descriptor.options[descriptor.optionCount - 1];

        none.value = motorCount;
        list.push_back(none);
    }

    return list;
}

/**
 * @brief Trims a motor selection to the motors a controller actually has.
 *
 * The generated description comes from the firmware being built today, and that firmware is a three motor
 * one - so it lists Motor 1, Motor 2, Motor 3 and Not used, with Not used at 3. Neither half of that
 * survives being shown to a two motor controller. It would offer a Motor 3 there is no stepper for, and
 * the value it would write for Not used is 3, which that controller refuses: the firmware defines
 * MFNC_MOTOR_NONE as the motor count, deliberately, so "nothing" is 3 where there are three motors and 2
 * where there are two.
 *
 * Narrowed here rather than where the settings are published because it is the same question isHeldBy is
 * already answering one identifier at a time - what this controller has - and because everything that
 * reads a Did then gets the narrowed answer: the range check on a write, the label a value is shown
 * under, and the settings a client is offered. A parameter that is not a motor selection passes through
 * untouched.
 */
static DidDescriptor narrowedTo(const DidDescriptor &descriptor, const Capabilities &capabilities)
{
    const std::string name(descriptor.name);
    const size_t suffix = sizeof(MotorSelectionSuffix) - 1;

    if ((name.size() < suffix) || (name.compare(name.size() - suffix, suffix, MotorSelectionSuffix) != 0))
        return descriptor;

    const int none = capabilities.motorCount;

    // Nothing to do on the controller the description was written for, and nothing that can safely be
    // done to a list that is not the shape this expects.
    if (!descriptor.isEnumerated() || (static_cast<int>(descriptor.optionCount) <= none) || (descriptor.maximum <= none)
        || (none < 1) || (none > motor::KnownMotors))
    {
        return descriptor;
    }

    const std::vector<DidOption> &options = narrowedSelections(descriptor, none);

    DidDescriptor narrowed = descriptor;

    narrowed.maximum     = none;
    narrowed.fallback    = (descriptor.fallback == descriptor.maximum) ? none : descriptor.fallback;
    narrowed.options     = options.data();
    narrowed.optionCount = options.size();

    return narrowed;
}

std::vector<Did> buildDidCatalogue(const Capabilities &capabilities)
{
    std::vector<Did> identifiers;

    identifiers.reserve(catalogue::ParameterCount);

    for (size_t index = 0; index < catalogue::ParameterCount; index++)
    {
        const DidDescriptor &descriptor = catalogue::Parameters[index];

        // A parameter kept only for older clients is still answered by the controller, but there is no
        // reason to put it in front of a user who has never had a reason to set it.
        if (descriptor.deprecated)
            continue;

        if (isHeldBy(descriptor.id, capabilities))
            identifiers.emplace_back(narrowedTo(descriptor, capabilities));
    }

    return identifiers;
}

namespace did_file
{

/** @brief Names the storage types exactly as the Windows editor writes them. */
static const char *typeName(DidType type)
{
    switch (type)
    {
        case DidType::UInt8:
            return "UINT8";

        case DidType::UInt16:
            return "UINT16";

        case DidType::UInt32:
            return "UINT32";

        case DidType::SInt8:
            return "SINT8";

        case DidType::SInt16:
            return "SINT16";

        default:
            return "SINT32";
    }
}

/** @brief Escapes the five characters that cannot appear literally in an XML attribute. */
static std::string escape(const std::string &text)
{
    std::string escaped;

    escaped.reserve(text.size());

    for (const char character : text)
    {
        switch (character)
        {
            case '&':
                escaped += "&amp;";
                break;

            case '<':
                escaped += "&lt;";
                break;

            case '>':
                escaped += "&gt;";
                break;

            case '"':
                escaped += "&quot;";
                break;

            case '\'':
                escaped += "&apos;";
                break;

            default:
                escaped += character;
                break;
        }
    }

    return escaped;
}

void write(const std::string &fileName, const std::string &deviceDescription, const std::vector<Did> &identifiers)
{
    const std::string temporaryName = fileName + ".tmp";

    {
        std::ofstream file(temporaryName, std::ios::binary | std::ios::trunc);

        if (!file)
            throw std::runtime_error("Cannot write '" + temporaryName + "'.");

        char stamp[32]        = "";
        const std::time_t now = std::time(nullptr);
        std::tm local{};

        localtime_r(&now, &local);
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &local);

        file << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        file << "<DIDList Device=\"" << escape(deviceDescription) << "\" Exported=\"" << stamp << "\">\n";

        for (const Did &identifier : identifiers)
        {
            if (!identifier.isAvailable())
                continue;

            file << "  <DID Id=\"" << identifier.id() << "\" Value=\"" << identifier.value() << "\" Type=\""
                 << typeName(identifier.type()) << "\" Name=\"" << escape(identifier.description()) << "\" />\n";
        }

        file << "</DIDList>";

        if (!file)
            throw std::runtime_error("Writing '" + temporaryName + "' failed.");
    }

    // Moved into place only once it is complete, so that a failure part way through does not leave a
    // truncated file where a good export used to be.
    std::remove(fileName.c_str());

    if (std::rename(temporaryName.c_str(), fileName.c_str()) != 0)
        throw std::runtime_error("Cannot move '" + temporaryName + "' into place as '" + fileName + "'.");
}

/**
 * @brief Pulls one attribute out of an element's text.
 * @return True when the attribute was present
 */
static bool attribute(const std::string &element, const std::string &name, std::string &value)
{
    const std::string needle = name + "=\"";
    const size_t start       = element.find(needle);

    if (start == std::string::npos)
        return false;

    const size_t from = start + needle.size();
    const size_t end  = element.find('"', from);

    if (end == std::string::npos)
        return false;

    value = element.substr(from, end - from);

    return true;
}

size_t read(const std::string &fileName, std::vector<Did> &identifiers)
{
    std::ifstream file(fileName, std::ios::binary);

    if (!file)
        throw std::runtime_error("Cannot read '" + fileName + "'.");

    std::stringstream buffer;

    buffer << file.rdbuf();

    const std::string document = buffer.str();

    size_t applied  = 0;
    size_t position = 0;

    // Deliberately a scan for one element shape rather than an XML parser: the format is written by one
    // tool, has no nesting below the element level and no text content, and pulling in a parser to read
    // four attributes would be the only external dependency in the whole library.
    while ((position = document.find("<DID ", position)) != std::string::npos)
    {
        const size_t end = document.find('>', position);

        if (end == std::string::npos)
            break;

        const std::string element = document.substr(position, end - position);

        position = end + 1;

        std::string idText;
        std::string valueText;

        if (!attribute(element, "Id", idText) || !attribute(element, "Value", valueText))
            continue;

        const long id    = std::strtol(idText.c_str(), nullptr, 10);
        const long value = std::strtol(valueText.c_str(), nullptr, 10);

        for (Did &identifier : identifiers)
        {
            if (identifier.id() != static_cast<uint32_t>(id))
                continue;

            // Held to the storage type rather than to what this firmware accepts. A file that disagrees
            // with the catalogue this badly is more likely to be from a different product than to be
            // something worth guessing at - but a value that merely falls outside the current firmware's
            // range is still this product's, and refusing the write with a reason tells the user more
            // than dropping the line here silently would.
            if (identifier.fitsStorage(static_cast<int>(value)))
            {
                identifier.setValue(static_cast<int>(value));
                applied++;
            }

            break;
        }
    }

    return applied;
}

} // namespace did_file

} // namespace scopelink
