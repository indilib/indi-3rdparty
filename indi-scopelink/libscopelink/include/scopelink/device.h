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
#include "scopelink/roles.h"
#include "scopelink/types.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

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
 * @brief What one status frame says about one motor.
 *
 * Grouped rather than left as parallel sets of numbered fields because from generation 4 a motor is
 * addressed by index: the caller has resolved a function to a motor and wants that motor's reading, and a
 * numbered field per motor would mean a switch statement at every such place.
 */
struct MotorReading
{
        /** Position in steps. */
        int position{ 0 };

        bool moving{ false };

        /** Which way it is turning, or Halted when it is not. */
        MotorDirection direction{ MotorDirection::Halted };

        /** Load in percent. */
        unsigned load{ 0 };
};

/**
 * @brief State of the front flap as a whole, as the status frame reports it.
 *
 * Not derivable from the motor states beside it in the frame, which is why the controller sends it at
 * all: a part standing still is opening while it waits out its delay, and a flap made of two parts is
 * neither open nor shut until both of them have arrived. Interface 1.1 onwards - before it the flap is
 * one motor with no sequence behind it, and its state is read off that motor.
 */
enum class FlapState
{
    Closed  = 0,
    Opening = 1,
    Open    = 2,
    Closing = 3,

    /** Stopped between the two, after a halt or a power interruption. */
    Partial = 4,

    /** This unit has no front flap. */
    NotConfigured = 5,

    /** A part still holds the default travel, so it has no open position to go to. */
    NotCalibrated = 6,

    /** A part did not reach its end position, or its motor is in error. */
    Error = 7,

    /**
     * This controller does not report a flap state, which is not a value it ever sends. Kept distinct
     * from NotConfigured so that "there is no flap" and "this controller cannot tell you about the flap"
     * do not read the same.
     */
    Unknown = 255
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

        /**
         * @brief Reads only the two identifiers both the firmware and the boot loader answer.
         *
         * The unit identifier and the software name are served by both images; the version numbers are
         * not, because they describe an interface the boot loader does not offer. So this is what can be
         * read out of a controller that is sitting in its boot loader - which is exactly when it matters,
         * because the unit identifier is what names the firmware file that will get it running again.
         */
        static Identification readCommon(Protocol &protocol);

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
        static constexpr int MaximumSupportedHardwareMajor = 4;

        /** Configuration identifier that records whether the temperature sensor is fitted. */
        static constexpr uint32_t TemperatureSensorFittedDid = 0x0302;

        int hardwareMajor{ 0 };

        /** Length of the cyclic status frame, header included. */
        size_t statusFrameLength{ 0 };

        /** Length of the freeze frame stored with each diagnostic trouble code. */
        size_t dtcSnapshotLength{ 0 };

        bool hasUsbHub{ false };

        /** How many downstream ports that hub has, and none at all when there is no hub. */
        int usbDownstreamPortCount{ 0 };

        /**
         * True when the controller reports whether a downstream port's power has failed.
         *
         * Generation 3 alone, which is why it is not simply hasUsbHub: generation 2 has no hub to report
         * on, and generation 4 spends those two bytes on four more ports.
         */
        bool hasUsbPowerFailureReporting{ false };

        /** True when the controller has the two auxiliary power outputs, and reports their state. */
        bool hasPowerSwitches{ false };

        /**
         * True when the frames still carry the four digital inputs.
         *
         * Nothing was ever wired to them and the firmware sent them as zeros, so they are stepped over
         * rather than read - but they occupy four bytes in the middle of both frames, so whether they are
         * there decides where every field behind them sits. Interface 1.1 dropped them, so this is the
         * firmware's answer inverted rather than anything about the board.
         */
        bool hasDigitalInputs{ false };

        /** True when the controller has a second motor, so that it can have a front flap at all. */
        bool hasSecondMotor{ false };

        /**
         * True when the controller has a third motor fitted.
         *
         * A fact about the board rather than about the firmware: generation 4 is the first one built with
         * three, and the generation 3 firmware speaks interface 1.1 with two of them. So this is not the
         * same question as hasConfigurableMotorRoles and cannot be folded into it - one controller has the
         * assignment without the motor, and the two are read from different halves of the identification
         * block. Written out rather than folded into a constant so that it still reads as the reason, and
         * so that it says the same thing as the Windows driver's DeviceCapabilities.
         */
        bool hasThirdMotor{ false };

        /**
         * How many motors this controller drives.
         *
         * The one number the whole of the frame tail hangs off: the motors sit in a run of six bytes each
         * in the status frame and two each in the freeze frame, so a third motor moves everything behind
         * it. Written as a count rather than read off the generation number because there is no longer a
         * generation whose layout can be read off its number.
         */
        int motorCount{ 0 };

        /**
         * True when which motor drives the focuser, the rotator and each part of the front flap is a
         * setting rather than a fact about the board, held in configuration identifiers 0x0400 onwards.
         *
         * Taken from the interface version rather than from the hardware generation, because the two
         * answer different questions: the generation says what the board was built with and cannot change,
         * the interface version says what the firmware can be asked and does change, because a controller
         * can be reflashed. Interface 1.1 onwards; before it the assignment was fixed and there was
         * nothing to hold.
         */
        bool hasConfigurableMotorRoles{ false };

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

        /**
         * What each motor of this controller is doing, indexed the way the protocol numbers them.
         *
         * As many entries as the controller has motors, so two up to generation 3 and three from
         * generation 4. Nothing here says what any of them drives: that is the motor role assignment, and
         * a reading is turned into a device's position by asking Device::roles() which motor to look at.
         */
        std::vector<MotorReading> motors;

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

        /**
         * What the front flap is doing as a whole.
         *
         * FlapState::Unknown on a controller whose interface predates 1.1, where the flap is one motor and
         * its state is whatever that motor's reading says.
         */
        FlapState flapState{ FlapState::Unknown };

        /**
         * Whether each downstream USB hub port has power, indexed from zero.
         *
         * A list rather than a flag per port because the count is a property of the hub: none on a
         * controller without one, two on generation 3 and six on generation 4, which gained four ports in
         * the space generation 3 spent on the failure flags below.
         */
        std::vector<bool> usbDownstreamPowerActive;

        /**
         * Whether a downstream port has reported a power fault. Generation 3 only - see
         * Capabilities::hasUsbPowerFailureReporting for why nothing is lost with them.
         */
        bool usb1PowerFailure{ false };
        bool usb2PowerFailure{ false };

        /** @brief How many motors this controller reports. */
        int motorCount() const { return static_cast<int>(motors.size()); }

        /**
         * @brief What one motor is doing.
         * @param index Motor index, as the protocol numbers them
         * @return The reading, or a halted reading at zero for a motor this controller does not have
         *
         * An index the controller has no motor for is answered rather than refused. A device built around
         * a motor that has since been unassigned reads its position on every poll, and a sample that says
         * "stopped, at zero" is something a caller can act on where an exception twice a second is not.
         */
        MotorReading motor(int index) const;

        /** @brief How many downstream USB hub ports this controller reports. */
        int usbDownstreamPortCount() const { return static_cast<int>(usbDownstreamPowerActive.size()); }

        /** @brief Whether one downstream USB hub port has power, false for a port this hub does not have. */
        bool usbPowerActive(int port) const;

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

        /**
         * @brief Which motor drives what, as read when the link was opened.
         *
         * The only place a function is mapped to a motor. On a controller from before interface 1.1 it is
         * the fixed assignment that hardware was built with, stated rather than read - which is what lets
         * a caller ask the same question of every controller and stop caring which one it is talking to.
         */
        const MotorRoles &roles() const { return m_roles; }

        /**
         * @brief Reads the motor assignment again, after something has changed it.
         * @throws CommunicationError The controller did not answer one of the identifiers.
         *
         * Writing one of the 0x0400 parameters changes which devices this controller offers, and the
         * answer read at connect time is the one everything above here is holding. Nothing here rebuilds
         * those devices - that needs the clients gone - but everything that asks after this point gets
         * the new answer.
         */
        const MotorRoles &refreshMotorRoles();

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

        /**
         * @brief Moves a motor to an absolute position.
         *
         * Addressed to a stepper rather than to what it drives, which is still how a travel is calibrated
         * on every controller: finding where a flap stands open means driving its motor before anything
         * knows what its open position is. Everything that is not calibration goes through the function
         * commands below on a controller that has them, and such a controller refuses this one for a
         * motor it is currently driving as part of a flap sequence.
         */
        void moveMotor(int motorId, int position);

        /** @brief Adopts a position as the current motor position without moving. */
        void syncMotor(int motorId, int position);

        /** @brief Stops a motor immediately. */
        void haltMotor(int motorId);

        /**
         * @brief Moves the focuser to an absolute position, in motor steps.
         * @throws CommunicationError The controller refused the request, with the reason it gave.
         *
         * The function commands say what is to be moved and let the controller decide which motor that
         * is. They exist from interface 1.1 and are refused before it, so a caller picks between these
         * and the motor commands above by asking Capabilities::hasConfigurableMotorRoles.
         */
        void moveFocuser(int position);

        /** @brief Adopts a position as the focuser's current position without moving. */
        void syncFocuser(int position);

        /** @brief Stops the focuser immediately. */
        void haltFocuser();

        /** @brief Moves the field rotator to an absolute position, in motor steps. */
        void moveRotator(int position);

        /** @brief Stops the field rotator immediately. */
        void haltRotator();

        /**
         * @brief Starts opening the front flap.
         *
         * Returns as soon as the controller has taken the request, which is not when the flap is open. A
         * flap of several parts opens them in a configured order with a configured delay between them,
         * and the whole of that runs in the controller - watch Status::flapState for the end of it.
         */
        void openFlap();

        /** @brief Starts closing the front flap. */
        void closeFlap();

        /** @brief Stops the front flap where it is, abandoning the sequence. */
        void haltFlap();

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
         * @brief Asks the controller to restart into its boot loader, for a firmware update.
         * @return True when the controller acknowledged the request
         *
         * Reported rather than thrown. The controller answers first and restarts a few milliseconds
         * later, so an acknowledgement says the request was understood, not that the restart has
         * happened - and a controller that restarted before its answer got out cannot be told apart from
         * one that never heard the request. Only the caller, which is about to go looking for a boot
         * loader, can decide what a missing answer should mean.
         */
        bool requestJumpToBootloader();

        /**
         * @brief Asks the controller to restart.
         * @return True when the controller acknowledged the request
         *
         * Used at the end of a firmware update, so that the unit the user is left with is one that
         * started its new firmware against the configuration it will run on from now on, rather than one
         * whose parameters were written underneath it while it ran. Reported rather than thrown for the
         * same reason as @ref requestJumpToBootloader.
         */
        bool requestReset();

        /**
         * @brief Reads a configuration identifier of a given width, reporting one the controller will not
         *        answer as nothing rather than as a failure.
         * @param did Identifier to read
         * @param length Its width in bytes, 1, 2 or 4
         *
         * Capability discovery and the motor assignment are both read while the link is being opened, and
         * an identifier that a given firmware build does not know about is a normal answer there rather
         * than a reason to refuse the connection. Whether a missing one is fatal is the caller's
         * decision: it is a default for a capability and a refused connection for an assignment.
         */
        std::optional<uint32_t> tryReadConfigurationValue(uint32_t did, int length);

        /** @brief Reads a single byte configuration identifier, or -1 when it is not answered. */
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

        /**
         * @brief Sends a function request that carries no position.
         * @return What the controller answered
         */
        FunctionResponse sendFunction(uint8_t function, uint8_t subFunction);

        /** @brief Sends a function request that carries a position, in motor steps. */
        FunctionResponse sendFunction(uint8_t function, uint8_t subFunction, int position);

        /**
         * @brief Turns anything but an acceptance into an error that says what the controller objected to.
         * @param name What to call the function in the message
         * @param response What the controller answered
         * @throws CommunicationError The response was not FunctionResponse::Ok.
         */
        static void requireFunction(const char *name, FunctionResponse response);

        Identification m_identification;
        Capabilities m_capabilities;
        MotorRoles m_roles;
        Status m_status;

        bool m_identified{ false };
        bool m_hasStatus{ false };
};

} // namespace scopelink
