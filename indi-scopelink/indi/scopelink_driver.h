/*
    ScopeLink INDI driver

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#pragma once

#include "scopelink/device.h"
#include "scopelink/faults.h"
#include "scopelink/firmware.h"
#include "scopelink/parameters.h"
#include "scopelink/protocol.h"
#include "scopelink/roles.h"
#include "scopelink/transport.h"

#include <defaultdevice.h>
#include <indifocuserinterface.h>
#include <indirotatorinterface.h>
#include <indipropertylight.h>
#include <indipropertynumber.h>
#include <indipropertyswitch.h>
#include <indipropertytext.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace Connection
{
class Serial;
}

// Drivers conventionally reach the focuser interface through this short name. Declaring it here rather
// than relying on libindi to have done so keeps the driver building against releases that do not, and a
// second identical alias is harmless. The guard is for the case where a release spells it as a macro.
#ifndef FI
using FI = INDI::FocuserInterface;
#endif

#ifndef RI
using RI = INDI::RotatorInterface;
#endif

/**
 * @brief One ScopeLink controller, published as one INDI device.
 *
 * The Windows driver has to be three COM servers - a focuser, a rotator and a switch - that share a
 * device object and must be pointed at the same port by hand. INDI has no such constraint, so this is one
 * driver process owning one port and advertising every interface the connected hardware turns out to
 * have:
 *
 *   - focuser, wherever a motor is assigned to one
 *   - rotator, wherever a motor is assigned to one
 *   - light box, always, driving the flat panel duty cycle
 *   - dust cap, once the front flap has motors with a calibrated travel to move along
 *   - auxiliary, for the fans, the power outputs, the telemetry and the diagnostics
 *
 * "Wherever a motor is assigned to one" is the whole of what interface 1.1 changed here. Before it the
 * mapping was a fact about the board - motor 1 focused, motor 2 was the flap, nothing rotated - and after
 * it the mapping is a setting the controller holds, read once at connect time into m_roles. Nothing below
 * this line names a motor by the job it does; it asks the assignment which motor, and asks the controller
 * to move the job rather than the motor.
 *
 * Everything runs on libindi's event loop, which is single threaded. A transaction blocks that loop for
 * as long as it takes - at worst three attempts of a 100 ms receive timeout - so the polling period is
 * kept comfortably above that.
 */
class ScopeLink : public INDI::DefaultDevice, public INDI::FocuserInterface, public INDI::RotatorInterface
{
    public:
        ScopeLink();
        virtual ~ScopeLink() override = default;

        const char *getDefaultName() override;

        bool initProperties() override;
        bool updateProperties() override;

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:
        bool Disconnect() override;
        void TimerHit() override;
        bool saveConfigItems(FILE *fp) override;

        // Focuser interface
        IPState MoveAbsFocuser(uint32_t targetTicks) override;
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        bool AbortFocuser() override;
        bool SyncFocuser(uint32_t ticks) override;
        bool SetFocuserMaxPosition(uint32_t ticks) override;

        // Rotator interface
        IPState MoveRotator(double angle) override;
        bool SyncRotator(double angle) override;
        bool ReverseRotator(bool enabled) override;
        bool AbortRotator() override;

    private:
        /** How close to an end stop the flap has to stop for the move to count as having arrived. */
        static constexpr int MinimumCapTolerance = 2;

        /** Most stored faults one property can show. The wire format cannot return more in one read. */
        static constexpr size_t MaxShownFaults = 8;

        /**
         * Which motor holds which end of travel is no longer a constant.
         *
         * It used to be: 0x000F was the focuser's and 0x010F was the flap's, because motor 1 focused and
         * motor 2 was the flap on every controller that existed. From interface 1.1 the focuser can be
         * any motor, so the identifier is asked for by index - scopelink::motor::maximumPositionDid - and
         * the index comes from the assignment.
         */

        /** What the cover was last told to do, so that the poll knows when it has finished. */
        enum class CapTarget
        {
            None,
            Park,
            Unpark
        };

        // Connection and link handling ------------------------------------------------------------

        /**
         * @brief Identifies the controller once libindi has opened the port.
         * @return True when a supported ScopeLink answered.
         */
        bool handshake();

        /** @brief The body of the handshake, so that every failure path releases what it built. */
        bool identify();

        /** @brief Drops everything that depends on the port's file descriptor. */
        void releaseDevice();

        /** @brief Reports the link as lost and takes the device to a disconnected state. */
        void reportLinkLost(const std::string &reason);

        // Property construction -------------------------------------------------------------------

        void buildHardwareProperties();
        /**
         * @brief Sizes the properties whose width the connected controller decides.
         *
         * The motor load list and the USB hub lamps are as wide as the controller has motors and ports,
         * which is not known until it has identified itself. Called once per connection, before the
         * properties are published.
         */
        void sizeHardwareProperties();

        void defineHardwareProperties();
        void deleteHardwareProperties();
        void defineDeveloperProperties();
        void deleteDeveloperProperties();

        // Poll --------------------------------------------------------------------------------------

        void publishTelemetry(const scopelink::Status &status);
        void publishFocuser(const scopelink::Status &status);
        void publishRotator(const scopelink::Status &status);
        void publishCap(const scopelink::Status &status);
        void publishLightBox(const scopelink::Status &status);
        void publishFans(const scopelink::Status &status);
        void publishPower(const scopelink::Status &status);
        void publishCalibration(const scopelink::Status &status);

        // Cover ---------------------------------------------------------------------------------------

        IPState moveCap(CapTarget target);
        void adoptCapStateFromPosition(const scopelink::Status &status);

        // Motion ---------------------------------------------------------------------------------------

        /**
         * @brief Moves, syncs or stops the focuser, addressed the way this controller expects.
         * @param subFunction scopelink::function::Move, ::Sync or ::Halt
         * @param position Target in controller steps, ignored by Halt
         * @throws std::exception The controller refused the request.
         *
         * Two command sets do the same three things, and this is the one place that chooses between them.
         * A controller from interface 1.1 is asked to move the focuser and works out for itself which
         * motor that is; an older one is asked to move a motor, because its firmware knows nothing else.
         * Everything above this line asks for the focuser.
         */
        void driveFocuser(uint8_t subFunction, int position);

        /**
         * @brief Opens, closes or stops the front flap, addressed the way this controller expects.
         * @param subFunction scopelink::function::Open, ::Close or ::Halt
         *
         * The difference between the two paths is larger here than it is for the focuser. A controller
         * from interface 1.1 is asked to open the flap and runs the whole sequence itself - each part in
         * order, with the configured delay between them - where an older one is told to drive its one
         * flap motor to a step position, because a flap of one part is all its firmware could express.
         */
        void driveFlap(uint8_t subFunction);

        /** @brief Follows a flap the controller reports the state of, rather than one read off a motor. */
        void publishReportedCapState(const scopelink::Status &status);

        /** @brief What one flap state is called, for the property that shows it. */
        static const char *flapStateName(scopelink::FlapState state);

        // Rotator -------------------------------------------------------------------------------------

        /**
         * @brief The sky angle a mechanical angle corresponds to, and back again.
         *
         * A sync moves neither the mechanism nor the motor's step count: it says the mechanism is where it
         * is and the sky is somewhere else than the driver thought, so what it changes is the offset
         * between the two. Sending the controller's rotator sync instead would move the step count, which
         * is the thing every range check here rests on.
         */
        double skyOf(double mechanical) const;
        double mechanicalOf(double sky) const;

        /** @brief The mechanical angle a motor position corresponds to. */
        double mechanicalDegreesOf(int steps) const;

        /**
         * @brief The motor position that puts the rotator at a mechanical angle.
         * @return The step count, or -1 when the travel does not reach that angle
         *
         * An angle does not name one step count. The rotator is a stepper driven between zero and a
         * calibrated end of travel, and that travel is whatever the customer's mechanism turns out to be:
         * less than a revolution on a rotator limited by a cable, and more than one on a rotator that is
         * not. So an angle corresponds to every step count congruent to it, and the one to command is the
         * reachable one nearest to where the motor already is - which is also the shortest movement, and
         * the one that does not unwind a cable it has just wound up.
         */
        long rotatorStepsFor(double mechanical, int current) const;

        /** @brief Brings an angle into the range zero up to but not including 360. */
        static double normaliseAngle(double degrees);

        // Parameters ----------------------------------------------------------------------------------

        /**
         * @brief One enumerated parameter, published as a switch rather than as a number.
         *
         * A parameter that holds one of a named set of settings is a switch vector in INDI, not a number
         * with a comment explaining what 0 and 1 mean. It costs a property of its own - a switch cannot be
         * an element of a number vector - and it is worth it: this is the half of the tab somebody writes
         * by hand in a script, and PARAM_FOCUSER_INVERT_DIRECTION.MOTOR_DIR_INVERTED says what it does.
         */
        struct ParameterSwitch
        {
                uint32_t id{ 0 };
                std::unique_ptr<INDI::PropertySwitch> property;
        };

        /**
         * @brief Everything one subsystem publishes: its numbers as one vector, its settings as switches.
         *
         * How many identifiers a group has depends on the hardware generation, so the properties are
         * built at connect time rather than declared with a fixed element count. Offering an element for
         * an identifier the controller does not hold would show a value that can never be read and, worse,
         * accept a write that goes nowhere.
         */
        struct ParameterGroup
        {
                /** The group as the firmware describes it, which is what names and labels the property. */
                const scopelink::DidGroupInfo *info{ nullptr };

                /** The group's numeric parameters, null when it has none of them. */
                std::unique_ptr<INDI::PropertyNumber> property;

                /** Identifier behind each element of that property, in element order. */
                std::vector<uint32_t> identifiers;

                /** The group's enumerated parameters, one switch vector each. */
                std::vector<ParameterSwitch> switches;
        };

        void buildParameterGroups();
        void readAllParameters();
        bool applyParameterGroup(ParameterGroup &group, double values[], char *names[], int n);
        bool applyParameterSwitch(ParameterSwitch &entry, ISState *states, char *names[], int n);
        void publishParameterValues();
        void exportParameters();
        void importParameters();
        scopelink::Did *findDid(uint32_t id);

        /**
         * @brief True when an identifier is one of the ones that says which motor drives what.
         *
         * The whole 0x0400 block, the step multiplier and the steps per revolution included: all of them
         * change something the driver worked out when it connected, and none of them can be picked up by
         * reading a travel again.
         */
        static bool isAssignmentIdentifier(uint32_t id);

        /**
         * @brief Picks up a travel that a parameter write has just changed.
         *
         * The calibration page writes an end of travel and the focuser's range has to follow it without
         * waiting for a reconnect. Which identifier holds which travel comes from the assignment, so a
         * write that changed the assignment itself goes the other way round - see refreshMotorRoles.
         */
        void adoptTravels();

        // Restarting the controller --------------------------------------------------------------------

        /**
         * @brief Asks the controller to restart, and waits for it to come back.
         *
         * The action that makes a changed motor assignment take effect. The controller reads its
         * configuration when it starts and drives from that copy, so an assignment written while it runs
         * is stored and reads back but is not acted on: the firmware goes on driving the one it booted
         * with, and refuses a function command for anything only the new one names. Nothing short of a
         * restart changes that, and until this existed the only ways to get one were to reach behind the
         * telescope for the cable or to run a firmware update, which ends with a restart for exactly this
         * reason.
         *
         * The port is let go rather than held: the controller leaves the USB bus and comes back, so the
         * descriptor the connection plugin opened is about to describe a device that is not there. The
         * driver stays connected while it waits, the way it does for a firmware update, because the wait
         * is run from the poll.
         */
        void restartController();

        /** @brief Looks for the restarted controller, and reconnects to it once it answers. */
        void tickControllerRestart();

        /**
         * @brief Gives up the exclusive claim on the port before the descriptor holding it is closed.
         *
         * libindi opens a port exclusively - tty_connect sets TIOCEXCL - and on a pseudo terminal that
         * claim outlives the descriptor that made it: the master end keeps the pair alive, so the flag is
         * never cleared and every later open of the slave is refused with EBUSY. It leaves a driver that
         * cannot reconnect to scopelink-simulator after a disconnection, cannot reopen the port after a
         * restart, and cannot hand it to a firmware update - none of which is anything to do with the
         * simulator, and all of which reads as "port is already used by another driver or process".
         *
         * A real controller hides it: its device node goes away with the device, and the claim goes with
         * the node. So this costs a failed ioctl on hardware and is the difference between working and
         * not against the simulator the manual tells people to develop against.
         */
        void clearPortExclusivity();

        /**
         * @brief Opens the port on its own and asks whatever is behind it to identify itself.
         * @return True when a controller answered
         *
         * A whole handshake rather than a look at whether the device node is back. The node reappears as
         * soon as the kernel has enumerated the device, which is before its firmware is answering, and a
         * reconnection attempted then fails for a reason that has nothing to do with the restart.
         */
        bool controllerAnswers();

        // Diagnostics ----------------------------------------------------------------------------------

        void readFaults();
        void clearFaults();
        void readEepromStatistics();
        void publishLinkHealth();

        // Firmware update -------------------------------------------------------------------------------

        /** @brief Reads the firmware file named in the property and checks it against this controller. */
        bool checkFirmwareFile(std::unique_ptr<scopelink::FirmwareFile> &file);

        /**
         * @brief Refuses the update, with a reason, unless everything about the moment is right.
         *
         * The one destructive thing this driver can do, and the only one whose preconditions are worth
         * spelling out in one place: no simulated controller to pretend on, no second update already
         * running, and no motor part way through a move - a controller that loses its firmware while it
         * is driving one stops wherever it happens to be and its stored position then means nothing.
         */
        bool canStartFirmwareUpdate();

        void startFirmwareUpdate();

        /** @brief Advances a running update and deals with whatever it has finished as. */
        void tickFirmwareUpdate();

        void publishFirmwareProgress();

        /**
         * @brief Writes the controller's whole configuration to a file before the erase.
         * @return True when there is a usable backup to restore from afterwards
         *
         * The configuration is expected to survive - the boot loader erases only the firmware slot, and
         * the parameters live outside it - so this is insurance rather than the normal path. It is worth
         * the one pass of reads all the same: a firmware that lays that memory out differently would
         * leave every stored value meaning something else, and the file is the only thing that could put
         * one back by hand.
         */
        bool saveConfigurationForUpdate();

        /** @brief Puts back anything the new firmware does not already have right. */
        void restoreConfigurationAfterUpdate();

        /**
         * @brief Hands the port back to the connection plugin and rebuilds everything on top of it.
         * @param failure What to log when the controller cannot be opened again
         *
         * A full reconnect rather than an adopted descriptor, and it is the same job after a firmware
         * update as after a restart: the controller that comes back need not answer the way the one that
         * went away did. Its interface version, the length of its status frame, the set of configuration
         * identifiers it holds and - since interface 1.1 - which motor drives what are all things this
         * driver reads once, at connect, and every one of them may have changed. So everything is torn
         * down and built again against whatever is actually there.
         */
        void reopenController(const char *failure);

        /**
         * @brief Identifies a controller that answers only as a boot loader.
         * @return True when a boot loader answered
         *
         * An update that fails leaves the controller waiting in its boot loader with no firmware to
         * start, and a boot loader answers none of the identifiers a connection normally turns on. Rather
         * than refusing the connection - which would leave the user with a unit no version of this driver
         * could reach again - the driver connects in a reduced state that offers the firmware page and
         * nothing else, which is the one thing that can get the controller running again.
         */
        bool identifyBootloader();

        void buildFirmwareProperties();

        // Calibration ----------------------------------------------------------------------------------

        /** @brief Everything that differs between the motors the calibration page can drive. */
        struct MotorChannel
        {
                std::string name;
                int motorId{ 0 };
                uint32_t savedPositionDid{ 0 };
                uint32_t maximumPositionDid{ 0 };
        };

        /**
         * @brief Names the motors the calibration page offers, one per motor the controller has.
         *
         * One entry per stepper rather than one per job, because a job is a setting: a controller can
         * have a motor assigned to nothing, and that motor still has to be calibrated before it can
         * usefully be assigned to anything. The label says what each one currently drives.
         */
        void buildCalibrationChannels();

        MotorChannel selectedChannel() const;
        void calibrationJog(int steps);
        void calibrationMarkMinimum();
        void calibrationMarkMaximum();
        void calibrationReset();

        // Helpers ----------------------------------------------------------------------------------------

        /** @brief Reads a motor's calibrated end of travel out of the controller. */
        bool readMotorTravel(uint32_t maximumPositionDid, int &travel);

        /** @brief True when the driver is connected and the controller has been identified. */
        bool isReady() const;

        /**
         * @brief True when the controller takes commands addressed to a function rather than to a motor.
         *
         * Interface 1.1 onwards, which is the same thing as holding the assignment: a controller that can
         * be told which motor drives the flap is one that has to be asked to open the flap, because the
         * order and the delays between its parts are its business and not this driver's.
         */
        bool commandsFunctions() const;

        /**
         * @brief True when this unit has a front flap that can be driven to a known end of travel.
         *
         * A flap motor is not enough on its own: without a calibrated travel there is no position to open
         * to, so the cover is neither published nor claimed as a dust cap. Every part has to have one - a
         * two part flap with one calibrated part cannot be opened any more than a flap with none can.
         */
        bool hasUsableFlap() const { return m_flapTravel > 0; }

        /** @brief Reads the assignment again after something has written one of its identifiers. */
        void refreshMotorRoles();

        /**
         * @brief Applies what the assignment says, once it has been read or re-read.
         *
         * The travels, the step multiplier and the calibration channels all hang off which motor drives
         * what, so they are worked out in one place rather than at each of the two points an assignment
         * can arrive from.
         */
        bool adoptMotorRoles();

        Connection::Serial *m_serialConnection{ nullptr };

        /**
         * The link the protocol layer runs on: a descriptor borrowed from the connection plugin, or a
         * simulated controller when the device is in simulation mode. Nothing above this line knows which.
         */
        std::unique_ptr<scopelink::ISerialTransport> m_transport;
        std::unique_ptr<scopelink::Protocol> m_protocol;
        std::unique_ptr<scopelink::Device> m_device;

        std::vector<scopelink::Did> m_catalogue;

        /**
         * Which motor drives what, as the controller holds it.
         *
         * A copy of what the device read rather than a question asked of it each time, because it is
         * consulted on every poll and in places where the link may already have been given back. Kept in
         * step with the device's own copy by refreshMotorRoles().
         */
        scopelink::MotorRoles m_roles;

        /** The motors the calibration page can drive, one per motor this controller has. */
        std::vector<MotorChannel> m_calibrationChannels;

        /** Focuser travel in controller steps, as calibrated. */
        int m_focuserTravel{ 0 };

        /**
         * Flap travel in controller steps, as calibrated, for the part that opens first. Zero when there
         * is no flap, or when any of its parts has never been calibrated.
         */
        int m_flapTravel{ 0 };

        /** Rotator travel in controller steps, as calibrated. Zero when there is no rotator. */
        int m_rotatorTravel{ 0 };

        /** Motor steps for one full turn of the rotator, gearing included. */
        long m_stepsPerRevolution{ 1 };

        /**
         * Degrees between the mechanism and the sky, as a sync left it.
         *
         * Saved with the configuration, because a sky calibration that had to be redone every time a
         * client reconnected would not be worth doing at all.
         */
        double m_rotatorSyncOffset{ 0 };

        /** Which way the sky angle runs against the mechanical one. */
        bool m_rotatorReverse{ false };

        /** Where the rotator was last told to go, in sky degrees. */
        double m_rotatorTarget{ 0 };

        int m_rotatorSettle{ 0 };

        /**
         * Client steps to controller steps.
         *
         * A driver setting up to interface 1.1 and the controller's own from there on, because both
         * drivers read it from the same place and a focuser step then means the same thing on either
         * platform. Applied at connect either way.
         */
        int m_stepMultiplier{ 1 };

        CapTarget m_capTarget{ CapTarget::None };

        /** Where the focuser was last told to go, in client steps, so the poll can tell arrival from a stall. */
        double m_focusTarget{ 0 };

        /**
         * Polls to ignore before a motor that is neither moving nor at its target counts as stalled.
         *
         * The controller has been measured to report a motor as moving in the very status frame that
         * follows the command, but one poll of grace costs nothing and is the difference between a
         * slightly slow controller and a driver that reports a stall on every move.
         */
        int m_focusSettle{ 0 };
        int m_capSettle{ 0 };

        /**
         * Duty cycle the flat panel returns to when it is switched on. Kept here rather than read back
         * from the controller because the controller only knows the duty it is running, which is zero
         * while the panel is off - polling it back would erase the setting the user just typed.
         */
        int m_lightIntensity{ 0 };

        /** Number of consecutive polls that have failed, which is what decides when to give up. */
        int m_failedPolls{ 0 };

        /**
         * A restart in progress: the port it is waiting on, when to give up, and when to look again.
         *
         * The port is remembered rather than asked for while the wait runs, because the connection plugin
         * has been disconnected and the wait is what decides whether it is ever reconnected. Nothing here
         * outlives the wait; m_restarting is what says there is one.
         */
        bool m_restarting{ false };
        std::string m_restartPort;
        std::chrono::steady_clock::time_point m_restartDeadline;
        std::chrono::steady_clock::time_point m_restartNextProbe;

        /**
         * The port an update runs on, and the update itself; both null when there is none. The update
         * needs a port it owns, because the controller leaves the USB bus twice while it runs and the
         * descriptor the connection plugin opened dies with it - so for the length of the update the
         * plugin's descriptor is closed and this one takes its place.
         */
        std::unique_ptr<scopelink::PosixSerialTransport> m_updatePort;
        std::unique_ptr<scopelink::FirmwareUpdate> m_update;

        /** The configuration as it was before the update, to be put back afterwards. */
        std::vector<scopelink::Did> m_savedConfiguration;

        /**
         * The unit that configuration was read out of.
         *
         * It survives a failed update on purpose, so that a retry can still put back what the first
         * attempt saved - a controller left in its boot loader has no firmware to read a configuration
         * from, and the copy in memory is the only one that has not been through a file. That is exactly
         * why it has to be stamped with whose it is: what survives a failed update also survives
         * unplugging that controller and connecting a different one.
         */
        std::string m_savedConfigurationUnit;

        /** Where that configuration was written, so the user can be told and can restore it by hand. */
        std::string m_configurationBackup;

        /** True once the configuration has been dealt with, so a closing restart does not repeat it. */
        bool m_configurationRestored{ false };

        /** Last message published from the update, so that the log gets one line per step, not per tick. */
        std::string m_lastUpdateMessage;

        /**
         * True when the controller answered only as a boot loader. Everything except the firmware page is
         * unavailable in that state, because there is no firmware behind it to answer.
         */
        bool m_bootloaderOnly{ false };

        /**
         * Whether each interface's properties are currently published.
         *
         * Both are conditional now - a controller can have no focuser assigned as easily as it can have
         * no rotator - and the interface that defined them is the one that has to delete them again. A
         * flag rather than re-deriving the condition on the way out, because by then the assignment it
         * would be derived from has been given back with the link.
         */
        bool m_focuserPublished{ false };
        bool m_rotatorPublished{ false };

        /**
         * The unit's own identifier, kept here rather than read back from the device because it has to be
         * available in both states: it is what names the firmware file, and a controller sitting in its
         * boot loader is exactly when that matters and exactly when there is no identified device to ask.
         */
        std::string m_hardwareIdentifier;

        // Properties ---------------------------------------------------------------------------------------

        INDI::PropertyText IdentificationTP{ 4 };

        INDI::PropertyNumber FocusTemperatureNP{ 1 };

        INDI::PropertySwitch CapParkSP{ 2 };
        INDI::PropertyNumber CapPositionNP{ 2 };

        /**
         * What the flap is doing as a whole, published only where the controller reports it.
         *
         * Not derivable from the position beside it: a part standing still is opening while it waits out
         * its delay, and a flap of two parts is neither open nor shut until both have arrived.
         */
        INDI::PropertyText FlapStateTP{ 1 };

        /** Degrees between the mechanism and the sky, shown so that a sync is visible rather than felt. */
        INDI::PropertyNumber RotatorOffsetNP{ 1 };

        INDI::PropertySwitch LightSP{ 2 };
        INDI::PropertyNumber LightIntensityNP{ 1 };

        INDI::PropertyNumber RailsNP{ 3 };
        INDI::PropertyNumber TemperatureNP{ 3 };
        /** Sized at connect time from the motor count - two up to generation 3, three from generation 4. */
        INDI::PropertyNumber MotorLoadNP{ 2 };
        INDI::PropertyNumber ControllerNP{ 7 };

        INDI::PropertySwitch FanOverrideSP{ 2 };
        INDI::PropertySwitch FanStateSP{ 2 };
        INDI::PropertyNumber FanTargetNP{ 2 };

        INDI::PropertySwitch AuxPowerSP{ 2 };
        /**
         * Sized at connect time: two ports and two fault lamps on generation 3, six ports and no fault
         * lamps on generation 4.
         */
        INDI::PropertyLight UsbHubLP{ 4 };

        INDI::PropertyNumber StepMultiplierNP{ 1 };
        INDI::PropertySwitch DeveloperSP{ 2 };

        /** Generation the simulated controller reports. Only consulted while simulation is on. */
        INDI::PropertySwitch SimulatedGenerationSP{ 4 };

        std::vector<ParameterGroup> m_parameterGroups;
        INDI::PropertySwitch ParametersActionSP{ 3 };
        INDI::PropertyText ParametersFileTP{ 1 };

        /** On the parameters tab because that is where the assignment that needs it is edited. */
        INDI::PropertySwitch RestartSP{ 1 };

        INDI::PropertyNumber FaultCountNP{ 2 };
        INDI::PropertySwitch DiagnosticActionSP{ 3 };
        INDI::PropertyText FaultStoreTP{ MaxShownFaults };
        INDI::PropertyNumber EepromNP{ 7 };
        INDI::PropertyText LinkHealthTP{ 1 };

        INDI::PropertyText FirmwareFileTP{ 1 };
        INDI::PropertySwitch FirmwareActionSP{ 2 };
        INDI::PropertyNumber FirmwareProgressNP{ 1 };
        INDI::PropertyText FirmwareStatusTP{ 1 };

        /** Sized at connect time from the motor count, and labelled with what each motor drives. */
        INDI::PropertySwitch CalibrationMotorSP{ 2 };
        INDI::PropertyNumber CalibrationStatusNP{ 4 };
        INDI::PropertyNumber CalibrationStepNP{ 1 };
        INDI::PropertySwitch CalibrationActionSP{ 6 };
};
