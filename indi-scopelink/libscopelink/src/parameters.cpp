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

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace scopelink
{

/** Number of response bytes read after a write. */
static constexpr size_t WriteAcknowledgementLength = 7;

/** Number of bytes that precede the value in a read response. */
static constexpr size_t ReadValueOffset = 7;

Did::Did(uint32_t id, DidType type, DidGroup group, std::string description)
    : m_id(id), m_type(type), m_group(group), m_description(std::move(description))
{
}

std::string Did::elementName() const
{
    char name[16];

    snprintf(name, sizeof(name), "DID_%04X", m_id);

    return name;
}

size_t Did::length() const
{
    switch (m_type)
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
        const Frame request  = { 0x00, static_cast<uint8_t>(m_id >> 8), static_cast<uint8_t>(m_id & 0xff) };
        const Frame response = device.transact(command::DataIdentifier, request, length() + ReadValueOffset);

        checkOperationAndDid(response, 0);

        switch (m_type)
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
            throw ProtocolError(m_description + " cannot hold " + std::to_string(m_value) + "; its range is "
                                + std::to_string(minimum()) + " to " + std::to_string(maximum()) + ".");
        }

        Frame payload        = { 0x01, static_cast<uint8_t>(m_id >> 8), static_cast<uint8_t>(m_id & 0xff) };
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

    if (answered != m_id)
    {
        char text[80];

        snprintf(text, sizeof(text), "Requested DID 0x%04X but the controller answered for DID 0x%04X.", m_id,
                 answered);
        throw ProtocolError(text);
    }
}

const char *didGroupName(DidGroup group)
{
    switch (group)
    {
        case DidGroup::FocuserMotor:
            return "Focuser motor";

        case DidGroup::FlapMotor:
            return "Flap motor";

        case DidGroup::Fans:
            return "Fans";

        case DidGroup::Temperature:
            return "Temperature";

        case DidGroup::SmartSwitch:
            return "Smart switch monitoring";

        default:
            return "Other";
    }
}

std::vector<Did> buildDidCatalogue(const Capabilities &capabilities)
{
    std::vector<Did> identifiers;

    identifiers.emplace_back(0x0000, DidType::UInt8, DidGroup::FocuserMotor, "Focuser motor global current scaler");
    identifiers.emplace_back(0x0001, DidType::UInt8, DidGroup::FocuserMotor, "Focuser motor hold current");
    identifiers.emplace_back(0x0002, DidType::UInt8, DidGroup::FocuserMotor, "Focuser motor move current");
    identifiers.emplace_back(0x0003, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor A max");
    identifiers.emplace_back(0x0004, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor A start");
    identifiers.emplace_back(0x0005, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor D max");
    identifiers.emplace_back(0x0006, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor D stop");
    identifiers.emplace_back(0x0007, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor V max");
    identifiers.emplace_back(0x0008, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor V start");
    identifiers.emplace_back(0x0009, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor V stop");
    identifiers.emplace_back(0x000a, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor V tran");
    identifiers.emplace_back(0x000b, DidType::UInt16, DidGroup::FocuserMotor, "Focuser motor V stealth chop max");
    identifiers.emplace_back(0x000c, DidType::UInt8, DidGroup::FocuserMotor, "Focuser motor invert direction");
    identifiers.emplace_back(0x000d, DidType::SInt8, DidGroup::FocuserMotor,
                             "Focuser motor stall detection sensitivity");
    identifiers.emplace_back(0x000e, DidType::UInt32, DidGroup::FocuserMotor, "Focuser motor position");
    identifiers.emplace_back(0x000f, DidType::UInt32, DidGroup::FocuserMotor, "Focuser motor maximum position");

    identifiers.emplace_back(0x0100, DidType::UInt8, DidGroup::FlapMotor, "Flap motor global current scaler");
    identifiers.emplace_back(0x0101, DidType::UInt8, DidGroup::FlapMotor, "Flap motor hold current");
    identifiers.emplace_back(0x0102, DidType::UInt8, DidGroup::FlapMotor, "Flap motor move current");
    identifiers.emplace_back(0x0103, DidType::UInt16, DidGroup::FlapMotor, "Flap motor A max");
    identifiers.emplace_back(0x0104, DidType::UInt16, DidGroup::FlapMotor, "Flap motor A start");
    identifiers.emplace_back(0x0105, DidType::UInt16, DidGroup::FlapMotor, "Flap motor D max");
    identifiers.emplace_back(0x0106, DidType::UInt16, DidGroup::FlapMotor, "Flap motor D stop");
    identifiers.emplace_back(0x0107, DidType::UInt16, DidGroup::FlapMotor, "Flap motor V max");
    identifiers.emplace_back(0x0108, DidType::UInt16, DidGroup::FlapMotor, "Flap motor V start");
    identifiers.emplace_back(0x0109, DidType::UInt16, DidGroup::FlapMotor, "Flap motor V stop");
    identifiers.emplace_back(0x010a, DidType::UInt16, DidGroup::FlapMotor, "Flap motor V tran");
    identifiers.emplace_back(0x010b, DidType::UInt16, DidGroup::FlapMotor, "Flap motor V stealth chop max");
    identifiers.emplace_back(0x010c, DidType::UInt8, DidGroup::FlapMotor, "Flap motor invert direction");
    identifiers.emplace_back(0x010d, DidType::SInt8, DidGroup::FlapMotor, "Flap motor stall detection sensitivity");
    identifiers.emplace_back(0x010e, DidType::UInt32, DidGroup::FlapMotor, "Flap motor position");
    identifiers.emplace_back(0x010f, DidType::UInt32, DidGroup::FlapMotor, "Flap motor maximum position");

    // These three were once listed alongside the flap motor identifiers, which put them on every
    // controller from generation 2 onwards. A generation 2 controller does not answer any of them.
    if (capabilities.hasSmartSwitchDiagnostics)
    {
        identifiers.emplace_back(0x0600, DidType::UInt16, DidGroup::SmartSwitch, "Short to Vcc max voltage difference");
        identifiers.emplace_back(0x0601, DidType::UInt16, DidGroup::SmartSwitch, "Overcurrent max voltage difference");
        identifiers.emplace_back(0x0602, DidType::UInt8, DidGroup::SmartSwitch, "Enable open load detection");
    }

    if (capabilities.hasConfigurableTemperatureLimits)
    {
        identifiers.emplace_back(0x0700, DidType::UInt8, DidGroup::Temperature, "Controller temperature warning level");
        identifiers.emplace_back(0x0701, DidType::UInt8, DidGroup::Temperature, "Controller temperature error level");
    }

    identifiers.emplace_back(0x0200, DidType::UInt8, DidGroup::Fans, "Rear fan override default state");
    identifiers.emplace_back(0x0201, DidType::UInt8, DidGroup::Fans, "Rear fan override default value");
    identifiers.emplace_back(0x0202, DidType::UInt16, DidGroup::Fans, "Rear fan target dT");
    identifiers.emplace_back(0x0203, DidType::UInt16, DidGroup::Fans, "Rear fan target hysteresis");
    identifiers.emplace_back(0x0204, DidType::UInt16, DidGroup::Fans, "Rear fan startup blow time");
    identifiers.emplace_back(0x0210, DidType::UInt8, DidGroup::Fans, "Side fan override default state");
    identifiers.emplace_back(0x0211, DidType::UInt8, DidGroup::Fans, "Side fan override default value");
    identifiers.emplace_back(0x0212, DidType::UInt16, DidGroup::Fans, "Side fan target dT");
    identifiers.emplace_back(0x0213, DidType::UInt16, DidGroup::Fans, "Side fan target hysteresis");
    identifiers.emplace_back(0x0214, DidType::UInt16, DidGroup::Fans, "Side fan startup blow time");

    identifiers.emplace_back(0x0300, DidType::UInt8, DidGroup::Temperature, "Ambient temperature PT1 coefficient");
    identifiers.emplace_back(0x0301, DidType::UInt8, DidGroup::Temperature, "Mirror temperature PT1 coefficient");

    // Listed even on a unit that has no sensor, because it is how a sensor gets declared once one is
    // fitted. The two coefficients above stay listed for the same reason: the controller holds and answers
    // them either way, and the editor's job is to show what the controller has, not what it is using.
    if (capabilities.hasTemperatureSensorConfiguration)
    {
        identifiers.emplace_back(Capabilities::TemperatureSensorFittedDid, DidType::UInt8, DidGroup::Temperature,
                                 "Temperature sensor fitted (0 = not fitted)");
    }

    identifiers.emplace_back(0x0500, DidType::UInt16, DidGroup::Miscellaneous, "Max voltage drop on IR sensor supply");

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

            // A value the identifier cannot hold is skipped rather than clamped. A file that disagrees
            // with the catalogue this badly is more likely to be from a different product than to be
            // something worth guessing at.
            if (identifier.isInRange(static_cast<int>(value)))
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
