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

#include "scopelink/roles.h"

#include "scopelink/device.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace scopelink
{

const uint32_t MotorRoles::FlapPartMotorDids[MotorRoles::MaximumFlapParts] = { 0x0420, 0x0423, 0x0426 };

MotorRoles::MotorRoles(bool configurable, bool valid, std::string problem, std::optional<int> focuser,
                       std::optional<int> rotator, std::vector<int> flapMotors, int stepMultiplier,
                       long stepsPerRevolution)
    : m_configurable(configurable), m_valid(valid), m_problem(std::move(problem)), m_focuserMotor(focuser),
      m_rotatorMotor(rotator), m_flapMotors(std::move(flapMotors)),
      m_stepMultiplier((stepMultiplier < 1) ? 1 : stepMultiplier),
      m_stepsPerRevolution((stepsPerRevolution < 1) ? 1 : stepsPerRevolution)
{
}

int MotorRoles::flapPartsOn(const Capabilities &capabilities)
{
    return std::min(MaximumFlapParts, capabilities.motorCount);
}

size_t MotorRoles::selectionCount(const Capabilities &capabilities)
{
    return FirstFlapSelection + static_cast<size_t>(flapPartsOn(capabilities));
}

std::vector<uint32_t> MotorRoles::selectionDids(const Capabilities &capabilities)
{
    std::vector<uint32_t> dids(selectionCount(capabilities), 0);

    dids[FocuserSelection] = FocuserMotorDid;
    dids[RotatorSelection] = RotatorMotorDid;

    for (int part = 0; part < flapPartsOn(capabilities); part++)
        dids[FirstFlapSelection + static_cast<size_t>(part)] = FlapPartMotorDids[part];

    return dids;
}

std::string MotorRoles::problemWith(int motorCount, const std::vector<int> &selections)
{
    if (selections.size() < FirstFlapSelection)
        return "the motor assignment is shorter than the controller's own set of selections";

    const int unused   = motorCount;
    const size_t parts = selections.size() - FirstFlapSelection;

    // 1. Nothing may name a motor this controller does not have.
    if ((selections[FocuserSelection] > unused) || (selections[RotatorSelection] > unused)
        || (selections[FocuserSelection] < 0) || (selections[RotatorSelection] < 0))
    {
        return "a function is assigned to a motor this controller does not have";
    }

    for (size_t part = 0; part < parts; part++)
    {
        const int selection = selections[FirstFlapSelection + part];

        if ((selection < 0) || (selection > unused))
            return "front flap part " + std::to_string(part + 1)
                   + " is assigned to a motor this controller does not have";
    }

    // 2. The flap parts are an ordered sequence, so an unused part ends it. A hole would leave the delays
    //    describing a sequence with a gap in the middle, which is not a thing that can be run.
    bool ended = false;

    for (size_t part = 0; part < parts; part++)
    {
        if (selections[FirstFlapSelection + part] == unused)
            ended = true;
        else if (ended)
            return "the front flap parts are not a continuous sequence, part " + std::to_string(part + 1)
                   + " is assigned while an earlier part is not";
        else
        {
            /* Another part of the flap, in order */
        }
    }

    // 3. No motor may be claimed twice. Two functions commanding one stepper to different positions is the
    //    thing this whole block of parameters exists to prevent.
    std::vector<int> claimed;

    for (int selection : selections)
    {
        if (selection != unused)
            claimed.push_back(selection);
    }

    for (size_t first = 0; first < claimed.size(); first++)
    {
        for (size_t second = first + 1; second < claimed.size(); second++)
        {
            if (claimed[first] == claimed[second])
                return "motor " + std::to_string(claimed[first] + 1) + " is claimed by more than one function";
        }
    }

    return {};
}

MotorRoles MotorRoles::legacy(const Capabilities &capabilities)
{
    std::vector<int> flap;

    if (capabilities.hasSecondMotor)
        flap.push_back(1);

    return MotorRoles(false, true, {}, 0, std::nullopt, std::move(flap), 1, 1);
}

/** @brief Reads one identifier, refusing to carry on without it. */
static uint32_t require(const MotorRoles::ValueReader &readConfigurationValue, uint32_t did, int length)
{
    const std::optional<uint32_t> value = readConfigurationValue(did, length);

    if (!value.has_value())
    {
        char identifier[8];

        snprintf(identifier, sizeof(identifier), "0x%04X", did);

        throw CommunicationError(std::string("The controller did not answer configuration identifier ") + identifier
                                 + ", which says which motor drives what. Without it the driver cannot tell which "
                                   "devices this controller offers.");
    }

    return value.value();
}

MotorRoles MotorRoles::of(const Capabilities &capabilities, const ValueReader &readConfigurationValue)
{
    if (!capabilities.hasConfigurableMotorRoles)
        return legacy(capabilities);

    const int focuser        = static_cast<int>(require(readConfigurationValue, FocuserMotorDid, 1));
    const int rotator        = static_cast<int>(require(readConfigurationValue, RotatorMotorDid, 1));
    const int multiplier     = static_cast<int>(require(readConfigurationValue, FocuserStepMultiplierDid, 2));
    const long perRevolution = static_cast<long>(require(readConfigurationValue, RotatorStepsPerRevolutionDid, 4));

    std::vector<int> flapParts;

    for (int part = 0; part < flapPartsOn(capabilities); part++)
        flapParts.push_back(static_cast<int>(require(readConfigurationValue, FlapPartMotorDids[part], 1)));

    return build(capabilities, focuser, rotator, flapParts, multiplier, perRevolution);
}

MotorRoles MotorRoles::build(const Capabilities &capabilities, int focuser, int rotator,
                             const std::vector<int> &flapParts, int stepMultiplier, long stepsPerRevolution)
{
    const int unused = capabilities.motorCount;

    std::vector<int> selections;

    selections.reserve(FirstFlapSelection + flapParts.size());
    selections.push_back(focuser);
    selections.push_back(rotator);
    selections.insert(selections.end(), flapParts.begin(), flapParts.end());

    const std::string problem = problemWith(unused, selections);

    if (!problem.empty())
        return MotorRoles(true, false, problem, std::nullopt, std::nullopt, {}, stepMultiplier, stepsPerRevolution);

    // Checked above to have no hole in it, so the parts that are used are the ones at the front of it.
    std::vector<int> flap;

    for (int part : flapParts)
    {
        if (part != unused)
            flap.push_back(part);
    }

    return MotorRoles(true, true, {}, (focuser == unused) ? std::optional<int>() : std::optional<int>(focuser),
                      (rotator == unused) ? std::optional<int>() : std::optional<int>(rotator), std::move(flap),
                      stepMultiplier, stepsPerRevolution);
}

MotorFunction MotorRoles::functionOf(int motor) const
{
    if (m_focuserMotor.has_value() && (m_focuserMotor.value() == motor))
        return MotorFunction::Focuser;

    if (m_rotatorMotor.has_value() && (m_rotatorMotor.value() == motor))
        return MotorFunction::Rotator;

    for (int flapMotor : m_flapMotors)
    {
        if (flapMotor == motor)
            return MotorFunction::Flap;
    }

    return MotorFunction::None;
}

std::string MotorRoles::toString() const
{
    if (!m_valid)
        return "unusable (" + m_problem + ")";

    std::string text;

    text += "focuser on ";
    text += hasFocuser() ? ("motor " + std::to_string(m_focuserMotor.value() + 1)) : std::string("no motor");

    text += ", rotator on ";
    text += hasRotator() ? ("motor " + std::to_string(m_rotatorMotor.value() + 1)) : std::string("no motor");

    text += ", front flap on ";

    if (!hasFlap())
    {
        text += "no motor";
    }
    else
    {
        for (size_t part = 0; part < m_flapMotors.size(); part++)
        {
            text += (part == 0) ? "motor " : " then motor ";
            text += std::to_string(m_flapMotors[part] + 1);
        }
    }

    if (m_configurable)
    {
        text += ", " + std::to_string(m_stepMultiplier) + " motor steps per focuser step, "
                + std::to_string(m_stepsPerRevolution) + " steps per rotator revolution";
    }

    return text;
}

} // namespace scopelink
