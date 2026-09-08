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

/**
 * @brief The faults a ScopeLink controller can record.
 *
 * These are identities, not wire values. The number a controller sends for a given fault depends on its
 * generation - generation 4 inserted the third motor's two failures and four more USB ports into the
 * middle of the list, which moved everything below them by six - so a number that arrives is looked up
 * with Fault::codeFor rather than cast. Decoding a generation 4 store with the older numbering turns an
 * over temperature error into a smart switch open load, which is the same kind of quiet mis-reading the
 * per-generation freeze frame layout exists to prevent and is harder to notice.
 *
 * The order is the order the faults were introduced in, which up to IndependentWatchdogReset is also the
 * numbering every controller before generation 4 uses. That is a coincidence worth nothing: both tables
 * in faults.cpp are written out in their own controller's order rather than derived from this list.
 */
enum class FaultCode
{
    /**
     * A number this driver has no name for.
     *
     * A controller running firmware newer than this driver can report a fault the driver was built
     * before. Reported as this rather than refused: an unnamed fault is still worth showing a user
     * together with the freeze frame taken when it happened, and Fault::wireCode keeps the number.
     */
    Unknown = -1,

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
    EcuOvertemperatureError,

    /** The previous run ended in an independent watchdog reset. */
    IndependentWatchdogReset,

    // Generation 4 onwards. Listed last because this is an introduction order, not a wire order - on the
    // controller that has them, the third motor's failures sit beside the first two motors' and the four
    // extra ports beside the first two ports.
    MotorDriver3CommunicationFailure,
    MotorDriver3InitialisationFailure,
    UsbHubDs3Overcurrent,
    UsbHubDs4Overcurrent,
    UsbHubDs5Overcurrent,
    UsbHubDs6Overcurrent,

    /**
     * The stored assignment of motors to the focuser, the rotator and the front flap is unusable.
     *
     * Generation 4 onwards. The controller refuses a write that would produce one, so this means its
     * parameter block was defaulted or predates the rules. While it stands the controller reports every
     * function as unconfigured and moves nothing.
     */
    MotorConfigurationInvalid
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

        /**
         * The number this controller sends for it.
         *
         * Kept beside the identity because it is what goes back to clear the fault, because it is what a
         * support request has to quote against the firmware's own log, and because it is the only thing
         * there is to show for a fault whose identity came out FaultCode::Unknown.
         */
        int wireCode{ 0 };

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

        /**
         * @brief What fault a controller means by a number.
         * @param wireCode The number, as it came over the wire
         * @param hardwareMajor Generation of the controller that sent it
         * @return The fault, or FaultCode::Unknown for a number this driver has no name for
         */
        static FaultCode codeFor(int wireCode, int hardwareMajor);

        /**
         * @brief What number a controller uses for a fault.
         * @param code The fault
         * @param hardwareMajor Generation of the controller
         * @return The number, or -1 when this generation has no such fault
         */
        static int wireCodeOf(FaultCode code, int hardwareMajor);

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
