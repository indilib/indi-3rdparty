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

#pragma once

#include "scopelink/protocol.h"
#include "scopelink/types.h"

#include <chrono>
#include <string>

namespace scopelink
{

/** @brief What a motor is doing, as reported by its status byte. */
enum class MotorDirection
{
    Clockwise,
    CounterClockwise,
    Halted
};

/**
 * @brief Version and identification data read from the controller at connect time.
 *
 * A malformed identification block raises ProtocolError so that a device which is not a ScopeLink is
 * rejected during connect rather than appearing to connect and then failing every status parse.
 */
struct Identification
{
        int hardwareMajor{ 0 };
        int hardwareMinor{ 0 };
        int interfaceMajor{ 0 };
        int interfaceMinor{ 0 };

        /** Unique hardware identifier as 24 upper case hex characters. */
        std::string hardwareIdentifier;

        /** Firmware identification string. */
        std::string softwareIdentifier;

        /** @brief Reads the identification data from the controller. */
        static Identification read(Protocol &protocol);

        /** @brief Human readable summary, used in log messages and error text. */
        std::string toString() const;
};

/**
 * @brief Everything that varies between ScopeLink hardware generations, derived once at connect time.
 *
 * Supporting a new generation means adding one branch here rather than finding every hardware version
 * test scattered through the parsers.
 */
class Capabilities
{
    public:
        /** Oldest hardware generation this driver can talk to. */
        static constexpr int MinimumSupportedHardwareMajor = 2;

        /** Newest hardware generation this driver knows the frame layouts for. */
        static constexpr int MaximumSupportedHardwareMajor = 3;

        /** Configuration identifier that records whether the temperature sensor is fitted. */
        static constexpr uint32_t TemperatureSensorFittedDid = 0x0302;

        int hardwareMajor{ 0 };

        /** Length of the cyclic status frame, header included. */
        size_t statusFrameLength{ 0 };

        /** Length of the freeze frame stored with each diagnostic trouble code. */
        size_t dtcSnapshotLength{ 0 };

        bool hasUsbHub{ false };

        /**
         * True when the controller has smart switch monitoring: it records the diagnostics in the freeze
         * frame and it holds the configuration identifiers 0x0600 to 0x0602 that set the thresholds.
         */
        bool hasSmartSwitchDiagnostics{ false };

        bool hasAuxVoltageMonitoring{ false };

        /** True when the temperature warning and error levels are configurable, through 0x0700 and 0x0701. */
        bool hasConfigurableTemperatureLimits{ false };

        /** True when the controller records whether its temperature sensor is fitted. */
        bool hasTemperatureSensorConfiguration{ false };

        /**
         * True when the unit has a temperature sensor fitted, so ambient and mirror temperature can be
         * measured. A single infrared sensor provides both readings, so one flag covers the ambient
         * temperature, the mirror temperature and the difference between them.
         */
        bool hasTemperatureSensor{ true };

        /** @brief Reports whether this driver supports a given hardware generation. */
        static bool isSupported(int hardwareMajor);

        /**
         * @brief Derives the capability set for an identified device.
         * @param identification Identification data read from the controller
         * @param temperatureSensorFitted What configuration identifier 0x0302 says, or -1 when the
         *        controller did not answer it
         * @throws UnsupportedDeviceError The hardware generation is outside the supported range.
         *
         * Everything except the temperature sensor follows from the hardware generation alone. The sensor
         * is a fitting option rather than a generation difference, so it is the one capability that has to
         * be read out of the controller's configuration. Assuming it fitted is the safe default when the
         * identifier cannot be read: a unit that has the sensor but is wrongly told it has not would
         * silently stop offering readings the user paid for.
         */
        static Capabilities of(const Identification &identification, int temperatureSensorFitted = -1);
};

/**
 * @brief An immutable, timestamped picture of the controller state, parsed from one cyclic status frame.
 *
 * A snapshot is built in full before it is published and is never modified afterwards, so every reader
 * sees one coherent sample and can tell how old it is.
 */
class Status
{
    public:
        std::chrono::steady_clock::time_point timestamp;

        int hardwareMajor{ 0 };

        int supplyVoltage{ 0 };           /**< Main supply, millivolts. */
        int sensorSupplyVoltage{ 0 };     /**< IR sensor supply, millivolts. */
        int fanAVoltage{ 0 };             /**< Rear fan, millivolts. */
        int fanBVoltage{ 0 };             /**< Side fan, millivolts. */
        int controllerTemperature{ 0 };   /**< Controller die temperature, degrees Celsius. */
        int controllerSupplyVoltage{ 0 }; /**< Controller supply, millivolts. */

        int motor1Position{ 0 }; /**< Focuser motor position, steps. */
        int motor2Position{ 0 }; /**< Flap motor position, steps. */
        bool motor1Moving{ false };
        bool motor2Moving{ false };
        MotorDirection motor1Direction{ MotorDirection::Halted };
        MotorDirection motor2Direction{ MotorDirection::Halted };
        unsigned motor1Load{ 0 }; /**< Focuser motor load, percent. */
        unsigned motor2Load{ 0 }; /**< Flap motor load, percent. */

        int ambientTemperatureRaw{ 0 }; /**< Ambient temperature, 1/50 Kelvin as reported. */
        int mirrorTemperatureRaw{ 0 };  /**< Mirror temperature, 1/50 Kelvin as reported. */
        bool ambientTemperatureValid{ false };
        bool mirrorTemperatureValid{ false };

        int storedFaultCount{ 0 };
        int activeFaultCount{ 0 };
        int cpuLoad{ 0 };
        int peakCpuLoad{ 0 };
        int stackUsage{ 0 };
        int i2cErrorCounter{ 0 };

        bool powerSwitch1State{ false };
        bool powerSwitch2State{ false };

        bool fanAManualOverrideEnabled{ false };
        bool fanBManualOverrideEnabled{ false };
        bool fanAManualOverrideState{ false };
        bool fanBManualOverrideState{ false };
        int fanATargetDT{ 0 }; /**< Rear fan target temperature difference, 1/50 Kelvin. */
        int fanBTargetDT{ 0 }; /**< Side fan target temperature difference, 1/50 Kelvin. */

        int flatboxDuty{ 0 }; /**< Flat box duty cycle, percent. */

        bool usb1PowerActive{ false };
        bool usb2PowerActive{ false };
        bool usb1PowerFailure{ false };
        bool usb2PowerFailure{ false };

        /** @brief Ambient temperature in degrees Celsius. Only meaningful when it is valid. */
        double ambientTemperatureCelsius() const { return toCelsius(ambientTemperatureRaw); }

        /** @brief Mirror temperature in degrees Celsius. Only meaningful when it is valid. */
        double mirrorTemperatureCelsius() const { return toCelsius(mirrorTemperatureRaw); }

        /** @brief Converts a raw 1/50 Kelvin reading to degrees Celsius. */
        static double toCelsius(int raw) { return (raw / 50.0) - 273.0; }

        /** @brief How long ago this sample was taken, in milliseconds. */
        long ageMs() const;

        /** @brief True when the sample is older than the given limit. */
        bool isStale(long maximumAgeMs) const { return ageMs() > maximumAgeMs; }

        /**
         * @brief Parses a status frame.
         * @throws ProtocolError The frame does not match the expected layout.
         */
        static Status parse(const Frame &frame, const Capabilities &capabilities);

    private:
        /** Value the controller reports for a temperature sensor that is not answering. */
        static constexpr int SensorNotRespondingRaw = 0xFFFF;

        /** Coldest reading treated as believable, -100 degrees Celsius in 1/50 Kelvin. */
        static constexpr int MinimumPlausibleTemperatureRaw = (273 - 100) * 50;

        /** Warmest reading treated as believable, +100 degrees Celsius in 1/50 Kelvin. */
        static constexpr int MaximumPlausibleTemperatureRaw = (273 + 100) * 50;

        /**
         * @brief Decides whether a raw temperature reading should be believed.
         *
         * A controller whose MLX90614 infrared sensor has stopped answering reports 0xFFFF, which converts
         * to a perfectly plausible looking -273.0 degrees Celsius. Reporting that as a real reading is
         * worse than reporting nothing, so the sentinel is rejected, as is anything outside a generous band.
         */
        static bool isTemperaturePlausible(int raw);
};

/**
 * @brief The single owner of the link to one ScopeLink controller.
 *
 * Identification and capability discovery, the status sample, and the device operations more than one
 * part of the driver needs. Unlike the Windows implementation there is no poll thread and no connection
 * counting: libindi's event loop is the only thread, and it is the loop's timer that asks for a fresh
 * status sample.
 */
class Device
{
    public:
        /** Age at which a status sample stops being usable. */
        static constexpr long StatusMaxAgeMs = 2000;

        /** Receive timeout for a single frame. */
        static constexpr int ReceiveTimeoutMs = 100;

        explicit Device(Protocol &protocol);

        void setLogger(Logger logger);

        /**
         * @brief Identifies the controller, discovers its capabilities and takes the first status sample.
         * @throws CommunicationError The controller could not be reached.
         * @throws UnsupportedDeviceError The device is not a supported ScopeLink.
         */
        void open();

        /** @brief Forgets everything learned about the controller. */
        void reset();

        bool isIdentified() const { return m_identified; }
        const Identification &identification() const { return m_identification; }
        const Capabilities &capabilities() const { return m_capabilities; }

        /** @brief The most recent status sample. */
        const Status &status() const { return m_status; }

        /** @brief True when a status sample has been taken and is still fresh enough to use. */
        bool hasFreshStatus() const;

        /**
         * @brief Returns the most recent status sample, or throws if there is not a usable one.
         * @throws CommunicationError There is no recent status sample.
         */
        const Status &requireStatus() const;

        /**
         * @brief Reads a fresh status sample immediately and publishes it.
         * @throws CommunicationError The status could not be read.
         */
        const Status &refreshStatus();

        /** @brief Moves a motor to an absolute position. */
        void moveMotor(int motorId, int position);

        /** @brief Adopts a position as the current motor position without moving. */
        void syncMotor(int motorId, int position);

        /** @brief Stops a motor immediately. */
        void haltMotor(int motorId);

        /**
         * @brief Sets a fan's manual override enable flag.
         * @param fanIndex 0 for the rear fan, 1 for the side fan
         */
        void setFanOverrideEnabled(int fanIndex, bool enabled);

        /** @brief Sets a fan's commanded state while it is under manual control. */
        void setFanOverrideState(int fanIndex, bool on);

        /**
         * @brief Sets a fan's target temperature difference.
         * @param targetKelvin Target difference in Kelvin
         */
        void setFanTarget(int fanIndex, double targetKelvin);

        /** @brief Sets the flat box duty cycle, 0 to 100 percent. */
        void setFlatboxDuty(int percent);

        /** @brief Switches an auxiliary power output. */
        void setPowerSwitch(int index, bool on);

        /**
         * @brief Reads a single byte configuration identifier, reporting one the controller will not
         *        answer as -1 rather than as a failure.
         *
         * Capability discovery runs while the link is being opened, and an identifier that a given
         * firmware build does not know about is a normal answer rather than a reason to refuse the
         * connection.
         */
        int tryReadConfigurationByte(uint32_t did);

        /** @brief Performs a transaction with the controller. */
        Frame transact(uint8_t command, const Frame &payload, size_t expectedResponseLength);

        /** @brief Performs a transaction whose response length the controller decides. */
        Frame transactVariable(uint8_t command, const Frame &payload, size_t minimumResponseLength);

        Protocol &protocol() { return m_protocol; }

        /** @brief One line summary of link health, for logs and support requests. */
        std::string healthSummary() const;

    private:
        /**
         * @brief Sends the host clock to the controller so that freeze frames carry a usable timestamp.
         *
         * The controller rejecting this is treated as proof that whatever is on the port is not a
         * ScopeLink, which is the same conclusion the Windows driver draws.
         */
        void sendRtcTimestamp();

        void log(const char *scope, const std::string &message) const;

        Protocol &m_protocol;
        Logger m_logger;

        Identification m_identification;
        Capabilities m_capabilities;
        Status m_status;

        bool m_identified{ false };
        bool m_hasStatus{ false };
};

} // namespace scopelink
