/*
    ScopeLink INDI driver - fault store and EEPROM wear counters

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink/faults.h"

#include <cstdio>

namespace scopelink
{

const char *Fault::nameOf(FaultCode code)
{
    switch (code)
    {
        case FaultCode::SmEepromFault:
            return "EEPROM state machine fault";
        case FaultCode::EepromDatasetCorrupted:
            return "EEPROM dataset corrupted";
        case FaultCode::EepromDatasetReadFailure:
            return "EEPROM dataset read failure";
        case FaultCode::EepromDatasetWriteFailure:
            return "EEPROM dataset write failure";
        case FaultCode::Mlx90614CommunicationTimeout:
            return "Infrared temperature sensor not answering";
        case FaultCode::MotorDriver1CommunicationFailure:
            return "Focuser motor driver communication failure";
        case FaultCode::MotorDriver1InitialisationFailure:
            return "Focuser motor driver initialisation failure";
        case FaultCode::MotorDriver2CommunicationFailure:
            return "Flap motor driver communication failure";
        case FaultCode::MotorDriver2InitialisationFailure:
            return "Flap motor driver initialisation failure";
        case FaultCode::SupplyIrSensorOvercurrent:
            return "Infrared sensor supply overcurrent";
        case FaultCode::TargetHwidMismatch:
            return "Firmware is for a different unit";
        case FaultCode::UsbHubDs1Overcurrent:
            return "USB downstream port 1 overcurrent";
        case FaultCode::UsbHubDs2Overcurrent:
            return "USB downstream port 2 overcurrent";
        case FaultCode::SmartSwitchFanAOvercurrent:
            return "Rear fan output overcurrent";
        case FaultCode::SmartSwitchFanAOpenLoad:
            return "Rear fan output open load";
        case FaultCode::SmartSwitchFanAShortToVcc:
            return "Rear fan output short to supply";
        case FaultCode::SmartSwitchFanBOvercurrent:
            return "Side fan output overcurrent";
        case FaultCode::SmartSwitchFanBOpenLoad:
            return "Side fan output open load";
        case FaultCode::SmartSwitchFanBShortToVcc:
            return "Side fan output short to supply";
        case FaultCode::SmartSwitchAuxAOvercurrent:
            return "Auxiliary output 1 overcurrent";
        case FaultCode::SmartSwitchAuxAOpenLoad:
            return "Auxiliary output 1 open load";
        case FaultCode::SmartSwitchAuxAShortToVcc:
            return "Auxiliary output 1 short to supply";
        case FaultCode::SmartSwitchAuxBOvercurrent:
            return "Auxiliary output 2 overcurrent";
        case FaultCode::SmartSwitchAuxBOpenLoad:
            return "Auxiliary output 2 open load";
        case FaultCode::SmartSwitchAuxBShortToVcc:
            return "Auxiliary output 2 short to supply";
        case FaultCode::EcuOvertemperatureWarning:
            return "Controller temperature warning";
        case FaultCode::EcuOvertemperatureError:
            return "Controller temperature error";
    }

    return "Unknown fault";
}

std::string Fault::codeName() const
{
    char text[128];

    // The numeric code is kept alongside the description because it is what a support request has to
    // quote, and because a firmware newer than this driver can raise one the list above does not name.
    snprintf(text, sizeof(text), "%s (0x%04X)", nameOf(code), static_cast<unsigned>(code));

    return text;
}

std::string Fault::summary() const
{
    std::string text = codeName();

    text += isActive ? " - active" : " - stored";
    text += ", seen " + std::to_string(occurrenceCount) + (occurrenceCount == 1 ? " time" : " times");

    const std::string when = timestamp();

    if (!when.empty())
        text += ", first at " + when;

    if (additionalData != 0)
        text += ", data " + std::to_string(additionalData);

    return text;
}

int Fault::field(const char *name, int fallback) const
{
    for (const FaultSnapshotField &entry : snapshot)
    {
        if (entry.name == name)
            return entry.value;
    }

    return fallback;
}

std::string Fault::timestamp() const
{
    const int year = field("RTC year");

    // A controller that has never been given a clock reports a year of zero, and a timestamp of
    // 0000-00-00 tells the user less than no timestamp at all.
    if (year <= 0)
        return {};

    char text[32];

    snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d:%02d", year, field("RTC month", 0), field("RTC day", 0),
             field("RTC hour", 0), field("RTC minute", 0), field("RTC second", 0));

    return text;
}

std::string Fault::snapshotText() const
{
    std::string text;

    for (const FaultSnapshotField &entry : snapshot)
    {
        if (!text.empty())
            text += ", ";

        text += entry.name + " " + std::to_string(entry.value);
    }

    return text;
}

void Fault::buildSnapshotFields(const Frame &raw, const Capabilities &capabilities)
{
    size_t offset = 0;

    const auto add = [&](const char *name, size_t length)
    {
        snapshot.push_back({ name, static_cast<int>(byte_order::toUInt(raw, offset, length)) });
        offset += length;
    };

    add("System uptime", 4);
    add("Supply voltage", 2);
    add("IR sensor voltage", 2);
    add("Fan A voltage", 2);
    add("Fan B voltage", 2);

    if (capabilities.hasAuxVoltageMonitoring)
    {
        add("Aux A voltage", 2);
        add("Aux B voltage", 2);
    }

    add("Controller temperature", 2);
    add("Controller supply voltage", 2);
    add("RTC year", 2);
    add("RTC month", 1);
    add("RTC day", 1);
    add("RTC hour", 1);
    add("RTC minute", 1);
    add("RTC second", 1);
    add("Motor 1 status", 1);
    add("Motor 1 load", 1);
    add("Flatbox duty", 1);
    add("Motor 2 status", 1);
    add("Motor 2 load", 1);
    add("Digital input 1 state", 1);
    add("Digital input 2 state", 1);
    add("Digital input 3 state", 1);
    add("Digital input 4 state", 1);
    add("Fan A state", 1);
    add("Fan B state", 1);

    if (capabilities.hasSmartSwitchDiagnostics)
    {
        add("Aux A state", 1);
        add("Aux B state", 1);
        add("Fan A smart switch error state", 1);
        add("Fan B smart switch error state", 1);
        add("Aux A smart switch error state", 1);
        add("Aux B smart switch error state", 1);
        add("USB DS1 power active", 1);
        add("USB DS2 power active", 1);
        add("USB DS1 power failure", 1);
        add("USB DS2 power failure", 1);
    }

    if (offset != raw.size())
    {
        throw ProtocolError("The freeze frame description consumes " + std::to_string(offset)
                            + " bytes but hardware generation " + std::to_string(capabilities.hardwareMajor) + " sends "
                            + std::to_string(raw.size()) + ". The field list and the capability set disagree.");
    }
}

std::vector<Fault> Fault::readAll(Device &device)
{
    const Capabilities &capabilities = device.capabilities();

    if (!device.isIdentified())
        throw CommunicationError("The ScopeLink controller is not connected.");

    const int storedFaults = device.refreshStatus().storedFaultCount;

    if (storedFaults <= 0)
        return {};

    const size_t recordLength   = FaultHeaderLength + capabilities.dtcSnapshotLength;
    const size_t expectedLength = (recordLength * static_cast<size_t>(storedFaults)) + ResponseHeaderLength;

    // How many records come back is the controller's decision, not ours: the fault count above was read in
    // a separate transaction, and a fault logged between the two would make it stale. So the frame is read
    // at whatever length the controller says and the record count derived from that - unless the fault
    // store has grown past what the length byte can describe, where there is nothing to derive it from.
    const Frame response = (expectedLength > Protocol::MaxSelfDescribingFrameLength) ?
                               device.transact(command::ReadDtc, {}, expectedLength) :
                               device.transactVariable(command::ReadDtc, {}, ResponseHeaderLength + recordLength);

    const size_t recordBytes = response.size() - ResponseHeaderLength;

    if ((recordBytes % recordLength) != 0)
    {
        throw ProtocolError("The controller returned " + std::to_string(recordBytes)
                            + " bytes of fault records, which is not a whole number of " + std::to_string(recordLength)
                            + " byte records for hardware generation " + std::to_string(capabilities.hardwareMajor)
                            + ".");
    }

    // The two bytes between the frame header and the first record look like a response code and a record
    // count - measured 07 01 on both a generation 2 and a generation 3 controller holding one fault. They
    // are deliberately not used: the first of the two was seen to read 00 in one raw capture, so the pair
    // is not understood well enough to fail a fault store read on. The frame length is the authority.
    const size_t records = recordBytes / recordLength;

    std::vector<Fault> faults;

    faults.reserve(records);

    for (size_t index = 0; index < records; index++)
    {
        const size_t offset = ResponseHeaderLength + (index * recordLength);

        Fault fault;

        fault.code            = static_cast<FaultCode>(byte_order::toUInt(response, offset, 2));
        fault.additionalData  = byte_order::toUInt(response, offset + 2, 2);
        fault.occurrenceCount = byte_order::toUInt(response, offset + 4, 1);
        fault.isActive        = byte_order::toBool(response, offset + 5);

        const Frame raw(response.begin() + static_cast<long>(offset + FaultHeaderLength),
                        response.begin() + static_cast<long>(offset + recordLength));

        fault.buildSnapshotFields(raw, capabilities);

        faults.push_back(std::move(fault));
    }

    return faults;
}

void Fault::clear(Device &device) const
{
    const uint32_t value = static_cast<uint32_t>(code);

    device.transact(command::ClearDtc,
                    Frame{ 0x03, static_cast<uint8_t>((value >> 8) & 0xff), static_cast<uint8_t>(value & 0xff) }, 7);
}

EepromStatistics EepromStatistics::read(Device &device)
{
    const Frame response = device.transact(command::EepromStatistics, {}, 20);

    EepromStatistics statistics;

    statistics.datasetCounter          = static_cast<int>(byte_order::toUInt(response, 4, 2));
    statistics.learntDataCounter       = static_cast<int>(byte_order::toUInt(response, 6, 2));
    statistics.faultStoreBlock1Counter = static_cast<int>(byte_order::toUInt(response, 8, 2));
    statistics.faultStoreBlock2Counter = static_cast<int>(byte_order::toUInt(response, 10, 2));
    statistics.faultStoreBlock3Counter = static_cast<int>(byte_order::toUInt(response, 12, 2));
    statistics.faultStoreBlock4Counter = static_cast<int>(byte_order::toUInt(response, 14, 2));
    statistics.pageEraseCounter        = static_cast<int>(byte_order::toUInt(response, 16, 4));

    return statistics;
}

} // namespace scopelink
