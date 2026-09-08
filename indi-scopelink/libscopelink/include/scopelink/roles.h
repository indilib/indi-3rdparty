/*
    ScopeLink INDI driver - which motor of a controller drives what

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#pragma once

#include "scopelink/types.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace scopelink
{

class Capabilities;

/** @brief What a ScopeLink motor has been assigned to drive. */
enum class MotorFunction
{
    /** The motor is fitted but nothing has been assigned to it. */
    None,

    Focuser,

    /** The field rotator. */
    Rotator,

    /** One part of the front flap. */
    Flap
};

/**
 * @brief Which motor of a controller drives what, and the two numbers that go with those assignments.
 *
 * Read once when the link is opened, and the only thing in this driver that maps a function to a motor.
 * It exists because from interface 1.1 the mapping is a setting rather than a fact: a controller with
 * identical steppers is told which one focuses, which one rotates and which ones the front flap is made
 * of, and until that has been read the driver does not know which devices it is able to offer.
 *
 * The dataset is written the other way round from this - the functions name their motors, not the other
 * way about - because a setting that belongs to a function is one setting, whereas the same thing
 * expressed per motor is three settings of which two are always meaningless. @ref functionOf is the
 * reverse view, for the status frame and the fault snapshots, which have motors and want names.
 *
 * Everything here is in motor steps. The step multiplier and the steps per revolution are held in the
 * controller so that this driver and the Windows one read one number rather than each keeping their own,
 * but the controller never applies them: converting to client steps and to degrees is the driver's work.
 *
 * Deliberately the same shape as the Windows driver's MotorRoles, down to the rule numbering in
 * @ref problemWith, because the thing both of them have to agree with is the firmware.
 */
class MotorRoles
{
    public:
        /** Which motor drives the focuser. */
        static constexpr uint32_t FocuserMotorDid = 0x0400;

        /** How many motor steps make one focuser step as a client sees them. */
        static constexpr uint32_t FocuserStepMultiplierDid = 0x0401;

        /** Which motor drives the field rotator. */
        static constexpr uint32_t RotatorMotorDid = 0x0410;

        /** Motor steps for one full turn of the rotator. */
        static constexpr uint32_t RotatorStepsPerRevolutionDid = 0x0411;

        /** Most parts a front flap can be split into on any controller. */
        static constexpr int MaximumFlapParts = 3;

        /** Which motor drives each part of the front flap, in opening order. */
        static const uint32_t FlapPartMotorDids[MaximumFlapParts];

        /**
         * Where the focuser's motor selection sits in a selection list.
         *
         * The selections laid out as one list, in the firmware's own order - MFNC_SEL_FOCUSER,
         * MFNC_SEL_ROTATOR and MFNC_SEL_FLAP_FIRST in mfnc.c. It is the shape the rules are about: every
         * one of them is a statement about two selections at once, so none can be checked against a
         * single parameter and all of them are checked against the list.
         */
        static constexpr size_t FocuserSelection = 0;

        /** Where the rotator's motor selection sits in a selection list. */
        static constexpr size_t RotatorSelection = 1;

        /** Where the first flap part's selection sits, the rest following in opening order. */
        static constexpr size_t FirstFlapSelection = 2;

        /**
         * @brief Reads one configuration identifier of a given width.
         * @return The value, or nothing when the controller does not answer the identifier
         */
        using ValueReader = std::function<std::optional<uint32_t>(uint32_t did, int length)>;

        /**
         * @brief How many parts the front flap can be split into on a given controller.
         *
         * Each part is driven by a motor of its own, so a controller cannot have more parts than motors.
         * This bounds what is read rather than only what may be chosen, because a controller that cannot
         * have a third part does not carry its three identifiers at all - and an identifier that is not
         * carried is indistinguishable here from one the controller failed to answer, which is a hard
         * failure. The firmware keeps the dataset field either way, holding the default that says the
         * part is not used.
         */
        static int flapPartsOn(const Capabilities &capabilities);

        /** @brief How many motor selections a controller holds. */
        static size_t selectionCount(const Capabilities &capabilities);

        /**
         * @brief The identifiers holding a controller's motor selections, in selection order.
         *
         * Only the parts this controller can have - see @ref flapPartsOn - so the list is the set of
         * identifiers it actually carries, and can be read straight through without meeting one it
         * refuses.
         */
        static std::vector<uint32_t> selectionDids(const Capabilities &capabilities);

        /**
         * @brief Reports what is wrong with a set of motor selections, in the controller's own terms.
         * @param motorCount How many motors the controller has
         * @param selections The selections, laid out as @ref FocuserSelection describes
         * @return The complaint, or an empty string when the set is one the controller will hold
         *
         * The firmware's MFNC_CheckSelections, and the only statement of the rules on this side of the
         * cable. Everything that needs to know whether an assignment is acceptable asks here: @ref build
         * for what was read, and the driver before it offers to write one. A second copy of these rules
         * anywhere would be a copy that could drift, and the thing it would drift away from is the
         * firmware.
         *
         * A selection is a motor index, or the motor count to mean "not used" - the encoding the firmware
         * uses, where the value that means nothing is one past the last motor.
         */
        static std::string problemWith(int motorCount, const std::vector<int> &selections);

        /**
         * @brief The assignment every controller before interface 1.1 has, which is the one it was built
         *        with.
         *
         * Motor 1 focuses and motor 2, where there is one, is the flap. Neither the firmware nor this
         * driver could express anything else, so it is stated here rather than read - which is what lets
         * the rest of the driver ask the same question of every controller and stop caring which one it
         * is talking to. The step multiplier is not part of it: on these controllers it is a driver
         * setting, and the focuser takes it from there.
         */
        static MotorRoles legacy(const Capabilities &capabilities);

        /**
         * @brief Reads the assignment from a controller that holds one.
         * @throws CommunicationError A controller that should hold the assignment did not answer one of
         *         its identifiers.
         *
         * A controller from before interface 1.1 is answered from @ref legacy without a transaction being
         * sent, so this is safe to call on any of them.
         *
         * An identifier that does not come back is a failure rather than a default. The values decide
         * which devices the driver offers and which motor each one drives, so guessing at one would mean
         * connecting a client to a focuser that might be the flap.
         */
        static MotorRoles of(const Capabilities &capabilities, const ValueReader &readConfigurationValue);

        /**
         * @brief Builds an assignment from raw parameter values, checking it the way the controller does.
         *
         * The rules are the firmware's, deliberately: a driver that accepted an assignment the controller
         * had refused would offer a device that answered every command with "not configured". They are
         * stated once, in @ref problemWith; this lays the arguments out as the list that one works on and
         * turns its answer into an assignment.
         */
        static MotorRoles build(const Capabilities &capabilities, int focuser, int rotator,
                                const std::vector<int> &flapParts, int stepMultiplier, long stepsPerRevolution);

        /**
         * @brief An assignment that names nothing, for a link that has not been opened.
         *
         * Not configurable and not in breach of any rule, because there is no controller to have read one
         * from - every hasX() below answers false, which is what a caller asking before connect should be
         * told.
         */
        MotorRoles() = default;

        /** @brief True when the assignment was read from the controller rather than assumed. */
        bool isConfigurable() const { return m_configurable; }

        /**
         * @brief True when the assignment the controller holds is one this driver is willing to act on.
         *
         * False means the stored bytes break one of the three rules. The controller refuses a write that
         * would produce such a combination, so reaching here means its parameter block was defaulted or
         * is older than the rules - and the firmware, which checks the same three, will already be
         * reporting every function as unconfigured and refusing to move anything. This driver agrees with
         * it rather than offering devices the controller will not drive.
         */
        bool isValid() const { return m_valid; }

        /** @brief What is wrong with the assignment, empty when there is nothing wrong with it. */
        const std::string &problem() const { return m_problem; }

        /** @brief Motor driving the focuser, or nothing when this unit has no focuser. */
        const std::optional<int> &focuserMotor() const { return m_focuserMotor; }

        /** @brief Motor driving the field rotator, or nothing when this unit has no rotator. */
        const std::optional<int> &rotatorMotor() const { return m_rotatorMotor; }

        /**
         * @brief Motors driving the parts of the front flap, in opening order. Empty when there is no
         *        flap.
         *
         * The order is the order they open in, and the reverse of the order they close in. It is the
         * order the parts are listed in the dataset, which is why the list may not have a hole in it:
         * part 2 being unused while part 3 is not would leave the delays describing a sequence with a gap
         * in the middle of it.
         */
        const std::vector<int> &flapMotors() const { return m_flapMotors; }

        /** @brief How many motor steps make one focuser step as a client sees them. At least one. */
        int focuserStepMultiplier() const { return m_stepMultiplier; }

        /** @brief Motor steps for one full turn of the rotator, gearing included. At least one. */
        long rotatorStepsPerRevolution() const { return m_stepsPerRevolution; }

        bool hasFocuser() const { return m_focuserMotor.has_value(); }
        bool hasRotator() const { return m_rotatorMotor.has_value(); }
        bool hasFlap() const { return !m_flapMotors.empty(); }

        /** @brief Reports what a motor has been assigned to drive. */
        MotorFunction functionOf(int motor) const;

        /** @brief Human readable summary, for the connection log. */
        std::string toString() const;

    private:
        MotorRoles(bool configurable, bool valid, std::string problem, std::optional<int> focuser,
                   std::optional<int> rotator, std::vector<int> flapMotors, int stepMultiplier,
                   long stepsPerRevolution);

        bool m_configurable{ false };
        bool m_valid{ true };
        std::string m_problem;

        std::optional<int> m_focuserMotor;
        std::optional<int> m_rotatorMotor;
        std::vector<int> m_flapMotors;

        int m_stepMultiplier{ 1 };
        long m_stepsPerRevolution{ 1 };
};

} // namespace scopelink
