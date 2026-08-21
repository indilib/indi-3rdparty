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

#pragma once

#include "scopelink/device.h"
#include "scopelink/types.h"

#include <string>
#include <vector>

namespace scopelink
{

/** @brief The diagnostic trouble codes the controller can raise, in the firmware's own numbering. */
enum class FaultCode
{
    SmEepromFault = 0,
    EepromDatasetCorrupted,
    EepromDatasetReadFailure,
    EepromDatasetWriteFailure,
    Mlx90614CommunicationTimeout,
    MotorDriver1CommunicationFailure,
    MotorDriver1InitialisationFailure,
    MotorDriver2CommunicationFailure,
    MotorDriver2InitialisationFailure,
    SupplyIrSensorOvercurrent,
    TargetHwidMismatch,
    UsbHubDs1Overcurrent,
    UsbHubDs2Overcurrent,
    SmartSwitchFanAOvercurrent,
    SmartSwitchFanAOpenLoad,
    SmartSwitchFanAShortToVcc,
    SmartSwitchFanBOvercurrent,
    SmartSwitchFanBOpenLoad,
    SmartSwitchFanBShortToVcc,
    SmartSwitchAuxAOvercurrent,
    SmartSwitchAuxAOpenLoad,
    SmartSwitchAuxAShortToVcc,
    SmartSwitchAuxBOvercurrent,
    SmartSwitchAuxBOpenLoad,
    SmartSwitchAuxBShortToVcc,
    EcuOvertemperatureWarning,
    EcuOvertemperatureError
};

/** @brief One named field of a fault freeze frame. */
struct FaultSnapshotField
{
        std::string name;
        int value{ 0 };
};

/**
 * @brief One diagnostic trouble code with the freeze frame captured when it was raised.
 */
class Fault
{
    public:
        FaultCode code{ FaultCode::SmEepromFault };
        uint32_t additionalData{ 0 };
        uint32_t occurrenceCount{ 0 };
        bool isActive{ false };

        std::vector<FaultSnapshotField> snapshot;

        /** @brief What to call this code in a user interface. */
        std::string codeName() const;

        /** @brief One line summary: code, whether it is present now, and how often it has been seen. */
        std::string summary() const;

        /** @brief The whole freeze frame as one block of text, for the log. */
        std::string snapshotText() const;

        /** @brief Timestamp the freeze frame carries, or an empty string if it has none. */
        std::string timestamp() const;

        /**
         * @brief Reads all stored codes with their freeze frames.
         * @throws CommunicationError The fault store could not be read.
         */
        static std::vector<Fault> readAll(Device &device);

        /**
         * @brief Clears this code from the controller's fault store.
         * @throws CommunicationError The code could not be cleared.
         */
        void clear(Device &device) const;

        /** @brief What to call a code in a user interface, for codes that have not been read. */
        static const char *nameOf(FaultCode code);

    private:
        /** Length of the per-fault header that precedes the freeze frame. */
        static constexpr size_t FaultHeaderLength = 6;

        /** Length of the response header that precedes the first fault. */
        static constexpr size_t ResponseHeaderLength = 6;

        /**
         * @brief Describes the freeze frame for the connected hardware generation.
         *
         * The fields are walked in order and each one advances a running offset, because the generations
         * do not share a single offset table: generation 2 has no auxiliary output voltages, so every
         * field from the controller temperature onwards sits four bytes earlier than it does on generation
         * 3. The final length check makes any future mismatch fail loudly and immediately rather than
         * quietly decoding one generation's frame with another's offsets.
         */
        void buildSnapshotFields(const Frame &raw, const Capabilities &capabilities);

        /** @brief Value of a named field, or @p fallback when the frame does not carry it. */
        int field(const char *name, int fallback = -1) const;
};

/** @brief EEPROM wear counters read from the controller. */
struct EepromStatistics
{
        int pageEraseCounter{ 0 };
        int datasetCounter{ 0 };
        int learntDataCounter{ 0 };
        int faultStoreBlock1Counter{ 0 };
        int faultStoreBlock2Counter{ 0 };
        int faultStoreBlock3Counter{ 0 };
        int faultStoreBlock4Counter{ 0 };

        /** @brief Reads the wear counters from the controller. */
        static EepromStatistics read(Device &device);
};

} // namespace scopelink
