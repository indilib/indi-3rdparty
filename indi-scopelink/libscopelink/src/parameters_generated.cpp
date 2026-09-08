/*
    ScopeLink INDI driver - configuration data identifiers, generated

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

/*
    GENERATED FILE - do not edit.

    Produced by tools/gen_did_catalogue.py from the ScopeLink firmware description,
    params.json, interface version 1.1. Neither that script nor its input is part of
    this tree; regenerate the file there rather than editing it here.

    Everything in it is the firmware's own statement about its parameters - the limits,
    the units, the display scale, the settings an enumerated parameter offers and the
    help text. Which of them a given controller actually holds is a separate question,
    answered by buildDidCatalogue() in parameters.cpp, because params.json says nothing
    about hardware generations.
*/

#include "scopelink/parameters.h"

namespace scopelink
{
namespace catalogue
{

const char * const Product = "ScopeLink";
const int InterfaceMajor   = 1;
const int InterfaceMinor   = 1;

static const DidOption OptionsMotorDirection[] = {
    { 0, "MOTOR_DIR_NORMAL", "Normal" },
    { 1, "MOTOR_DIR_INVERTED", "Inverted" },
};

static const DidOption OptionsMotorSelection[] = {
    { 0, "MOTOR_SEL_1", "Motor 1" },
    { 1, "MOTOR_SEL_2", "Motor 2" },
    { 2, "MOTOR_SEL_3", "Motor 3" },
    { 3, "MOTOR_SEL_NONE", "Not used" },
};

static const DidOption OptionsFanControlMode[] = {
    { 0, "FAN_MODE_TEMPERATURE", "Follow the temperature difference" },
    { 1, "FAN_MODE_MANUAL", "Manual override" },
};

static const DidOption OptionsFanState[] = {
    { 0, "FAN_STATE_OFF", "Off" },
    { 1, "FAN_STATE_ON", "On" },
};

static const DidOption OptionsFeatureState[] = {
    { 0, "FEATURE_DISABLED", "Disabled" },
    { 1, "FEATURE_ENABLED", "Enabled" },
};

const DidGroupInfo Groups[] = {
    { "motor1", "Motor 1", "PARAMS_MOTOR1", 10 },
    { "motor2", "Motor 2", "PARAMS_MOTOR2", 20 },
    { "motor3", "Motor 3", "PARAMS_MOTOR3", 30 },
    { "focuser", "Focuser", "PARAMS_FOCUSER", 32 },
    { "rotator", "Rotator", "PARAMS_ROTATOR", 34 },
    { "flap", "Front flap", "PARAMS_FLAP", 36 },
    { "fan_a", "Fan A", "PARAMS_FAN_A", 40 },
    { "fan_b", "Fan B", "PARAMS_FAN_B", 50 },
    { "temperature", "Temperature processing", "PARAMS_TEMPERATURE", 60 },
    { "supply", "Sensor supply", "PARAMS_SUPPLY", 70 },
    { "smartsw", "Smart switch monitoring", "PARAMS_SMARTSW", 80 },
    { "uc_temp", "Controller temperature", "PARAMS_UC_TEMP", 90 },
    { "learnt", "Learnt positions", "PARAMS_LEARNT", 100 },
};

const size_t GroupCount = sizeof(Groups) / sizeof(Groups[0]);

const DidDescriptor Parameters[] = {
    // 0x0001 motor1.current.hold
    { 0x0001u, "motor1.current.hold", "Holding current", "motor1", "%", DidType::UInt8, false, 0, 100, 40, 1, true,
      false, nullptr, 0, nullptr,
      "Coil current the motor 1 holds a position with, as a percentage of the full scale set by the global scaler. The "
      "driver takes 32 steps, so a percentage maps onto the nearest one. Anything above 100 is treated as 100." },
    // 0x0002 motor1.current.run
    { 0x0002u, "motor1.current.run", "Running current", "motor1", "%", DidType::UInt8, false, 4, 100, 100, 1, true,
      false, nullptr, 0, nullptr,
      "Coil current the motor 1 moves with, as a percentage of the full scale set by the global scaler. Anything above "
      "100 is treated as 100. Below 4 % the driver rounds it down to no current at all." },
    // 0x0000 motor1.current.global_scaler
    { 0x0000u, "motor1.current.global_scaler", "Global current scaler", "motor1", "", DidType::UInt8, false, 0, 255, 50,
      1, false, false, nullptr, 0, nullptr,
      "Full scale the two currents above are a percentage of. Zero selects the driver's own full scale; 32 to 255 "
      "scale it down. The driver forbids 1 to 31, and a value in that window is raised to 32." },
    // 0x0008 motor1.dynamics.v_start
    { 0x0008u, "motor1.dynamics.v_start", "Start velocity", "motor1", "rev/s", DidType::UInt16, false, 0, 3662, 150,
      1000, true, false, nullptr, 0, nullptr,
      "Velocity the motor 1 begins a move at. The driver takes it in an 18 bit register that the conversion fills at "
      "about 3.66 rev/s, so every larger value asks for the same speed." },
    // 0x0009 motor1.dynamics.v_stop
    { 0x0009u, "motor1.dynamics.v_stop", "Stop velocity", "motor1", "rev/s", DidType::UInt16, false, 1, 3662, 400, 1000,
      true, false, nullptr, 0, nullptr,
      "Velocity the motor 1 finishes a move at. Zero leaves the ramp with no defined end and the driver does not "
      "accept it while positioning. The same 18 bit register puts the ceiling at about 3.66 rev/s." },
    // 0x000A motor1.dynamics.v_tran
    { 0x000Au, "motor1.dynamics.v_tran", "Transition velocity", "motor1", "rev/s", DidType::UInt16, false, 0, 14649,
      500, 1000, true, false, nullptr, 0, nullptr,
      "Velocity the motor 1 changes from the first acceleration ramp to the second at. Zero disables the first ramp. "
      "The driver takes it in a 20 bit register that the conversion fills at about 14.6 rev/s." },
    // 0x0007 motor1.dynamics.v_max
    { 0x0007u, "motor1.dynamics.v_max", "Maximum velocity", "motor1", "rev/s", DidType::UInt16, false, 1, 50000, 2000,
      1000, true, false, nullptr, 0, nullptr,
      "Fastest the motor 1 is driven. Zero never moves it. The conversion caps itself at 50 rev/s to keep its own "
      "arithmetic from overflowing, so anything above that asks for the same speed." },
    // 0x0004 motor1.dynamics.a_start
    { 0x0004u, "motor1.dynamics.a_start", "Start acceleration", "motor1", "rev/s^2", DidType::UInt16, false, 2, 65535,
      220, 1000, true, false, nullptr, 0, nullptr,
      "Acceleration used below the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "first ramp has nothing to accelerate with." },
    // 0x0006 motor1.dynamics.d_stop
    { 0x0006u, "motor1.dynamics.d_stop", "Stop deceleration", "motor1", "rev/s^2", DidType::UInt16, false, 2, 65535,
      300, 1000, true, false, nullptr, 0, nullptr,
      "Deceleration used below the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "first ramp has nothing to decelerate with." },
    // 0x0003 motor1.dynamics.a_max
    { 0x0003u, "motor1.dynamics.a_max", "Maximum acceleration", "motor1", "rev/s^2", DidType::UInt16, false, 2, 65535,
      250, 1000, true, false, nullptr, 0, nullptr,
      "Acceleration used above the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "motor 1 never gets moving." },
    // 0x0005 motor1.dynamics.d_max
    { 0x0005u, "motor1.dynamics.d_max", "Maximum deceleration", "motor1", "rev/s^2", DidType::UInt16, false, 2, 65535,
      400, 1000, true, false, nullptr, 0, nullptr,
      "Deceleration used above the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "motor 1 has no way to stop on target." },
    // 0x000B motor1.dynamics.v_stealthchop_max
    { 0x000Bu, "motor1.dynamics.v_stealthchop_max", "StealthChop upper velocity", "motor1", "rev/s", DidType::UInt16,
      false, 0, 14649, 150, 1000, true, false, nullptr, 0, nullptr,
      "Velocity above which the driver leaves the quiet StealthChop mode. Zero keeps it in StealthChop at every speed. "
      "The threshold register is 20 bits, which the conversion fills at about 14.6 rev/s." },
    // 0x000C motor1.invert_direction
    { 0x000Cu, "motor1.invert_direction", "Invert direction", "motor1", "", DidType::UInt8, false, 0, 1, 1, 1, true,
      false, OptionsMotorDirection, sizeof(OptionsMotorDirection) / sizeof(OptionsMotorDirection[0]),
      "PARAM_MOTOR1_INVERT_DIRECTION", "Which way the motor 1 turns for a rising position." },
    // 0x000D motor1.stall_sensitivity
    { 0x000Du, "motor1.stall_sensitivity", "Stall detection sensitivity", "motor1", "", DidType::SInt8, false, -64, 63,
      16, 1, true, false, nullptr, 0, nullptr,
      "Threshold the driver calls a stall at. The register field holds -64 to 63; a value outside that is brought to "
      "the nearest end." },
    // 0x000F motor1.max_position
    { 0x000Fu, "motor1.max_position", "Maximum position", "motor1", "steps", DidType::UInt32, false, 1, 2147483647,
      2147483647, 1, true, false, nullptr, 0, nullptr,
      "Furthest position the motor 1 may be sent to. A request beyond it is brought back to it. The driver holds "
      "positions in signed 32 bit registers, so the ceiling is where they would turn negative." },
    // 0x0101 motor2.current.hold
    { 0x0101u, "motor2.current.hold", "Holding current", "motor2", "%", DidType::UInt8, false, 0, 100, 40, 1, true,
      false, nullptr, 0, nullptr,
      "Coil current the motor 2 holds a position with, as a percentage of the full scale set by the global scaler." },
    // 0x0102 motor2.current.run
    { 0x0102u, "motor2.current.run", "Running current", "motor2", "%", DidType::UInt8, false, 4, 100, 100, 1, true,
      false, nullptr, 0, nullptr,
      "Coil current the motor 2 moves with, as a percentage of the full scale set by the global scaler. Below 4 % the "
      "driver rounds it down to no current at all." },
    // 0x0100 motor2.current.global_scaler
    { 0x0100u, "motor2.current.global_scaler", "Global current scaler", "motor2", "", DidType::UInt8, false, 0, 255, 50,
      1, false, false, nullptr, 0, nullptr,
      "Full scale the two currents above are a percentage of. Zero selects the driver's own full scale; 32 to 255 "
      "scale it down. The driver forbids 1 to 31, and a value in that window is raised to 32." },
    // 0x0108 motor2.dynamics.v_start
    { 0x0108u, "motor2.dynamics.v_start", "Start velocity", "motor2", "rev/s", DidType::UInt16, false, 0, 3662, 150,
      1000, true, false, nullptr, 0, nullptr,
      "Velocity the motor 2 begins a move at. The driver takes it in an 18 bit register that the conversion fills at "
      "about 3.66 rev/s, so every larger value asks for the same speed." },
    // 0x0109 motor2.dynamics.v_stop
    { 0x0109u, "motor2.dynamics.v_stop", "Stop velocity", "motor2", "rev/s", DidType::UInt16, false, 1, 3662, 400, 1000,
      true, false, nullptr, 0, nullptr,
      "Velocity the motor 2 finishes a move at. Zero leaves the ramp with no defined end and the driver does not "
      "accept it while positioning. The same 18 bit register puts the ceiling at about 3.66 rev/s." },
    // 0x010A motor2.dynamics.v_tran
    { 0x010Au, "motor2.dynamics.v_tran", "Transition velocity", "motor2", "rev/s", DidType::UInt16, false, 0, 14649,
      500, 1000, true, false, nullptr, 0, nullptr,
      "Velocity the motor 2 changes from the first acceleration ramp to the second at. Zero disables the first ramp. "
      "The driver takes it in a 20 bit register that the conversion fills at about 14.6 rev/s." },
    // 0x0107 motor2.dynamics.v_max
    { 0x0107u, "motor2.dynamics.v_max", "Maximum velocity", "motor2", "rev/s", DidType::UInt16, false, 1, 50000, 2000,
      1000, true, false, nullptr, 0, nullptr,
      "Fastest the motor 2 is driven. Zero never moves it. The conversion caps itself at 50 rev/s to keep its own "
      "arithmetic from overflowing, so anything above that asks for the same speed." },
    // 0x0104 motor2.dynamics.a_start
    { 0x0104u, "motor2.dynamics.a_start", "Start acceleration", "motor2", "rev/s^2", DidType::UInt16, false, 2, 65535,
      220, 1000, true, false, nullptr, 0, nullptr,
      "Acceleration used below the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "first ramp has nothing to accelerate with." },
    // 0x0106 motor2.dynamics.d_stop
    { 0x0106u, "motor2.dynamics.d_stop", "Stop deceleration", "motor2", "rev/s^2", DidType::UInt16, false, 2, 65535,
      300, 1000, true, false, nullptr, 0, nullptr,
      "Deceleration used below the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "first ramp has nothing to decelerate with." },
    // 0x0103 motor2.dynamics.a_max
    { 0x0103u, "motor2.dynamics.a_max", "Maximum acceleration", "motor2", "rev/s^2", DidType::UInt16, false, 2, 65535,
      250, 1000, true, false, nullptr, 0, nullptr,
      "Acceleration used above the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "motor 2 never gets moving." },
    // 0x0105 motor2.dynamics.d_max
    { 0x0105u, "motor2.dynamics.d_max", "Maximum deceleration", "motor2", "rev/s^2", DidType::UInt16, false, 2, 65535,
      400, 1000, true, false, nullptr, 0, nullptr,
      "Deceleration used above the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "motor 2 has no way to stop on target." },
    // 0x010B motor2.dynamics.v_stealthchop_max
    { 0x010Bu, "motor2.dynamics.v_stealthchop_max", "StealthChop upper velocity", "motor2", "rev/s", DidType::UInt16,
      false, 0, 14649, 150, 1000, true, false, nullptr, 0, nullptr,
      "Velocity above which the driver leaves the quiet StealthChop mode. Zero keeps it in StealthChop at every speed. "
      "The threshold register is 20 bits, which the conversion fills at about 14.6 rev/s." },
    // 0x010C motor2.invert_direction
    { 0x010Cu, "motor2.invert_direction", "Invert direction", "motor2", "", DidType::UInt8, false, 0, 1, 1, 1, true,
      false, OptionsMotorDirection, sizeof(OptionsMotorDirection) / sizeof(OptionsMotorDirection[0]),
      "PARAM_MOTOR2_INVERT_DIRECTION", "Which way the motor 2 turns for a rising position." },
    // 0x010D motor2.stall_sensitivity
    { 0x010Du, "motor2.stall_sensitivity", "Stall detection sensitivity", "motor2", "", DidType::SInt8, false, -64, 63,
      16, 1, true, false, nullptr, 0, nullptr,
      "Threshold the driver calls a stall at. The register field holds -64 to 63; a value outside that is brought to "
      "the nearest end." },
    // 0x010F motor2.max_position
    { 0x010Fu, "motor2.max_position", "Maximum position", "motor2", "steps", DidType::UInt32, false, 1, 2147483647,
      2147483647, 1, true, false, nullptr, 0, nullptr,
      "Furthest position the motor 2 may be sent to. A request beyond it is brought back to it. The driver holds "
      "positions in signed 32 bit registers, so the ceiling is where they would turn negative." },
    // 0x0801 motor3.current.hold
    { 0x0801u, "motor3.current.hold", "Holding current", "motor3", "%", DidType::UInt8, false, 0, 100, 40, 1, true,
      false, nullptr, 0, nullptr,
      "Coil current the motor 3 holds a position with, as a percentage of the full scale set by the global scaler." },
    // 0x0802 motor3.current.run
    { 0x0802u, "motor3.current.run", "Running current", "motor3", "%", DidType::UInt8, false, 4, 100, 100, 1, true,
      false, nullptr, 0, nullptr,
      "Coil current the motor 3 moves with, as a percentage of the full scale set by the global scaler. Below 4 % the "
      "driver rounds it down to no current at all." },
    // 0x0800 motor3.current.global_scaler
    { 0x0800u, "motor3.current.global_scaler", "Global current scaler", "motor3", "", DidType::UInt8, false, 0, 255, 50,
      1, false, false, nullptr, 0, nullptr,
      "Full scale the two currents above are a percentage of. Zero selects the driver's own full scale; 32 to 255 "
      "scale it down. The driver forbids 1 to 31, and a value in that window is raised to 32." },
    // 0x0808 motor3.dynamics.v_start
    { 0x0808u, "motor3.dynamics.v_start", "Start velocity", "motor3", "rev/s", DidType::UInt16, false, 0, 3662, 150,
      1000, true, false, nullptr, 0, nullptr,
      "Velocity the motor 3 begins a move at. The driver takes it in an 18 bit register that the conversion fills at "
      "about 3.66 rev/s, so every larger value asks for the same speed." },
    // 0x0809 motor3.dynamics.v_stop
    { 0x0809u, "motor3.dynamics.v_stop", "Stop velocity", "motor3", "rev/s", DidType::UInt16, false, 1, 3662, 400, 1000,
      true, false, nullptr, 0, nullptr,
      "Velocity the motor 3 finishes a move at. Zero leaves the ramp with no defined end and the driver does not "
      "accept it while positioning. The same 18 bit register puts the ceiling at about 3.66 rev/s." },
    // 0x080A motor3.dynamics.v_tran
    { 0x080Au, "motor3.dynamics.v_tran", "Transition velocity", "motor3", "rev/s", DidType::UInt16, false, 0, 14649,
      500, 1000, true, false, nullptr, 0, nullptr,
      "Velocity the motor 3 changes from the first acceleration ramp to the second at. Zero disables the first ramp. "
      "The driver takes it in a 20 bit register that the conversion fills at about 14.6 rev/s." },
    // 0x0807 motor3.dynamics.v_max
    { 0x0807u, "motor3.dynamics.v_max", "Maximum velocity", "motor3", "rev/s", DidType::UInt16, false, 1, 50000, 2000,
      1000, true, false, nullptr, 0, nullptr,
      "Fastest the motor 3 is driven. Zero never moves it. The conversion caps itself at 50 rev/s to keep its own "
      "arithmetic from overflowing, so anything above that asks for the same speed." },
    // 0x0804 motor3.dynamics.a_start
    { 0x0804u, "motor3.dynamics.a_start", "Start acceleration", "motor3", "rev/s^2", DidType::UInt16, false, 2, 65535,
      220, 1000, true, false, nullptr, 0, nullptr,
      "Acceleration used below the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "first ramp has nothing to accelerate with." },
    // 0x0806 motor3.dynamics.d_stop
    { 0x0806u, "motor3.dynamics.d_stop", "Stop deceleration", "motor3", "rev/s^2", DidType::UInt16, false, 2, 65535,
      300, 1000, true, false, nullptr, 0, nullptr,
      "Deceleration used below the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "first ramp has nothing to decelerate with." },
    // 0x0803 motor3.dynamics.a_max
    { 0x0803u, "motor3.dynamics.a_max", "Maximum acceleration", "motor3", "rev/s^2", DidType::UInt16, false, 2, 65535,
      250, 1000, true, false, nullptr, 0, nullptr,
      "Acceleration used above the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "motor 2 never gets moving." },
    // 0x0805 motor3.dynamics.d_max
    { 0x0805u, "motor3.dynamics.d_max", "Maximum deceleration", "motor3", "rev/s^2", DidType::UInt16, false, 2, 65535,
      400, 1000, true, false, nullptr, 0, nullptr,
      "Deceleration used above the transition velocity. Below 0.002 rev/s^2 the conversion rounds to zero and the "
      "motor 2 has no way to stop on target." },
    // 0x080B motor3.dynamics.v_stealthchop_max
    { 0x080Bu, "motor3.dynamics.v_stealthchop_max", "StealthChop upper velocity", "motor3", "rev/s", DidType::UInt16,
      false, 0, 14649, 150, 1000, true, false, nullptr, 0, nullptr,
      "Velocity above which the driver leaves the quiet StealthChop mode. Zero keeps it in StealthChop at every speed. "
      "The threshold register is 20 bits, which the conversion fills at about 14.6 rev/s." },
    // 0x080C motor3.invert_direction
    { 0x080Cu, "motor3.invert_direction", "Invert direction", "motor3", "", DidType::UInt8, false, 0, 1, 1, 1, true,
      false, OptionsMotorDirection, sizeof(OptionsMotorDirection) / sizeof(OptionsMotorDirection[0]),
      "PARAM_MOTOR3_INVERT_DIRECTION", "Which way the motor 3 turns for a rising position." },
    // 0x080D motor3.stall_sensitivity
    { 0x080Du, "motor3.stall_sensitivity", "Stall detection sensitivity", "motor3", "", DidType::SInt8, false, -64, 63,
      16, 1, true, false, nullptr, 0, nullptr,
      "Threshold the driver calls a stall at. The register field holds -64 to 63; a value outside that is brought to "
      "the nearest end." },
    // 0x080F motor3.max_position
    { 0x080Fu, "motor3.max_position", "Maximum position", "motor3", "steps", DidType::UInt32, false, 1, 2147483647,
      2147483647, 1, true, false, nullptr, 0, nullptr,
      "Furthest position the motor 3 may be sent to. A request beyond it is brought back to it. The driver holds "
      "positions in signed 32 bit registers, so the ceiling is where they would turn negative." },
    // 0x0400 focuser.motor_id
    { 0x0400u, "focuser.motor_id", "Motor", "focuser", "", DidType::UInt8, false, 0, 3, 0, 1, true, false,
      OptionsMotorSelection, sizeof(OptionsMotorSelection) / sizeof(OptionsMotorSelection[0]), "PARAM_FOCUSER_MOTOR_ID",
      "Which motor drives the focuser, or 'Not used' when this unit has no focuser. A motor already claimed by the "
      "rotator or by the front flap is refused: two functions commanding one motor to different positions is what this "
      "setting exists to prevent." },
    // 0x0401 focuser.step_multiplier
    { 0x0401u, "focuser.step_multiplier", "Step multiplier", "focuser", "steps", DidType::UInt16, false, 1, 1000, 1, 1,
      true, false, nullptr, 0, nullptr,
      "How many motor steps make one step as an imaging client sees them. The controller stores it and always reports "
      "raw motor steps; the driver is what divides, so ASCOM and INDI agree on what a step means because they read the "
      "same number from here." },
    // 0x0410 rotator.motor_id
    { 0x0410u, "rotator.motor_id", "Motor", "rotator", "", DidType::UInt8, false, 0, 3, 3, 1, true, false,
      OptionsMotorSelection, sizeof(OptionsMotorSelection) / sizeof(OptionsMotorSelection[0]), "PARAM_ROTATOR_MOTOR_ID",
      "Which motor drives the field rotator, or 'Not used' when this unit has no rotator. A motor already claimed by "
      "the focuser or by the front flap is refused." },
    // 0x0411 rotator.steps_per_rev
    { 0x0411u, "rotator.steps_per_rev", "Steps per revolution", "rotator", "steps", DidType::UInt32, false, 1,
      2147483647, 51200, 1, true, false, nullptr, 0, nullptr,
      "Motor steps for one full turn of the rotator, gearing included. The controller stores it and works in steps "
      "throughout; the driver is what converts to the degrees ASCOM and INDI report, for the same reason the focuser's "
      "multiplier lives here." },
    // 0x0420 flap.part1.motor_id
    { 0x0420u, "flap.part1.motor_id", "Part 1 motor", "flap", "", DidType::UInt8, false, 0, 3, 1, 1, true, false,
      OptionsMotorSelection, sizeof(OptionsMotorSelection) / sizeof(OptionsMotorSelection[0]),
      "PARAM_FLAP_PART1_MOTOR_ID",
      "Motor driving the first part of the front flap, or 'Not used' when this unit has no flap. A flap made of one "
      "part uses this alone. The three parts are opened in the order they are listed here and closed in the reverse "
      "order, so this is the part that opens first and closes last." },
    // 0x0421 flap.part1.delay_open
    { 0x0421u, "flap.part1.delay_open", "Part 1 opening delay", "flap", "ms", DidType::UInt16, false, 0, 60000, 0, 1,
      true, false, nullptr, 0, nullptr,
      "How long part 1 waits before it starts opening, measured from the moment the part in front of it in the opening "
      "order was commanded. Part 1 opens first, so this one is measured from the open command itself and is normally "
      "zero." },
    // 0x0422 flap.part1.delay_close
    { 0x0422u, "flap.part1.delay_close", "Part 1 closing delay", "flap", "ms", DidType::UInt16, false, 0, 60000, 0, 1,
      true, false, nullptr, 0, nullptr,
      "How long part 1 waits before it starts closing, measured from the moment the part in front of it in the closing "
      "order was commanded. Closing reverses the order, so part 1 closes last and this delay is measured from part 2 "
      "starting." },
    // 0x0423 flap.part2.motor_id
    { 0x0423u, "flap.part2.motor_id", "Part 2 motor", "flap", "", DidType::UInt8, false, 0, 3, 3, 1, true, false,
      OptionsMotorSelection, sizeof(OptionsMotorSelection) / sizeof(OptionsMotorSelection[0]),
      "PARAM_FLAP_PART2_MOTOR_ID",
      "Motor driving the second part of the front flap, on a flap that is split. 'Not used' ends the list: a part "
      "named after an unused one is a configuration error and is refused, because the opening order would have a hole "
      "in it." },
    // 0x0424 flap.part2.delay_open
    { 0x0424u, "flap.part2.delay_open", "Part 2 opening delay", "flap", "ms", DidType::UInt16, false, 0, 60000, 0, 1,
      true, false, nullptr, 0, nullptr,
      "How long part 2 waits after part 1 was commanded to open before it starts opening itself. This is the clearance "
      "between two halves of one flap: long enough that part 1 is out of the way, and the reason the sequencing is "
      "done here rather than in a driver that can be disconnected halfway through it." },
    // 0x0425 flap.part2.delay_close
    { 0x0425u, "flap.part2.delay_close", "Part 2 closing delay", "flap", "ms", DidType::UInt16, false, 0, 60000, 0, 1,
      true, false, nullptr, 0, nullptr,
      "How long part 2 waits after part 3 was commanded to close before it starts closing itself, or from the close "
      "command when there is no part 3. Held separately from the opening delay because the clearance a flap needs on "
      "the way in is rarely the one it needs on the way out." },
    // 0x0426 flap.part3.motor_id
    { 0x0426u, "flap.part3.motor_id", "Part 3 motor", "flap", "", DidType::UInt8, false, 0, 3, 3, 1, true, false,
      OptionsMotorSelection, sizeof(OptionsMotorSelection) / sizeof(OptionsMotorSelection[0]),
      "PARAM_FLAP_PART3_MOTOR_ID",
      "Motor driving the third part of the front flap. Only a unit that uses all three motors for the flap has one, "
      "and it opens last and closes first." },
    // 0x0427 flap.part3.delay_open
    { 0x0427u, "flap.part3.delay_open", "Part 3 opening delay", "flap", "ms", DidType::UInt16, false, 0, 60000, 0, 1,
      true, false, nullptr, 0, nullptr,
      "How long part 3 waits after part 2 was commanded to open before it starts opening itself." },
    // 0x0428 flap.part3.delay_close
    { 0x0428u, "flap.part3.delay_close", "Part 3 closing delay", "flap", "ms", DidType::UInt16, false, 0, 60000, 0, 1,
      true, false, nullptr, 0, nullptr,
      "How long part 3 waits before it starts closing, measured from the close command itself: closing reverses the "
      "order, so part 3 is the one that moves first." },
    // 0x0200 fan.a.override_state
    { 0x0200u, "fan.a.override_state", "Manual override at startup", "fan_a", "", DidType::UInt8, false, 0, 1, 0, 1,
      true, false, OptionsFanControlMode, sizeof(OptionsFanControlMode) / sizeof(OptionsFanControlMode[0]),
      "PARAM_FAN_A_OVERRIDE_STATE",
      "Whether fan A follows the temperature difference from startup, or is held at the value below until something "
      "changes it." },
    // 0x0201 fan.a.override_value
    { 0x0201u, "fan.a.override_value", "Manual override value at startup", "fan_a", "", DidType::UInt8, false, 0, 1, 0,
      1, true, false, OptionsFanState, sizeof(OptionsFanState) / sizeof(OptionsFanState[0]),
      "PARAM_FAN_A_OVERRIDE_VALUE", "State fan A is driven to at startup while manual override is on." },
    // 0x0202 fan.a.target_delta_t
    { 0x0202u, "fan.a.target_delta_t", "Target temperature difference", "fan_a", "K", DidType::UInt16, false, 0, 5000,
      25, 50, true, false, nullptr, 0, nullptr,
      "Difference between the ambient and the mirror temperature fan A aims to bring the mirror back below. A target "
      "beyond 100 K is further than the mirror and the air ever drift apart, so the fan would never come on by "
      "itself." },
    // 0x0203 fan.a.target_hysteresis
    { 0x0203u, "fan.a.target_hysteresis", "Switch-on hysteresis", "fan_a", "K", DidType::UInt16, false, 0, 5000, 20, 50,
      true, false, nullptr, 0, nullptr,
      "How far above the target the difference has to rise before fan A switches on. It switches off again at the "
      "target itself. It is added to the target, so the same 100 K ceiling applies to it." },
    // 0x0204 fan.a.startup_blow
    { 0x0204u, "fan.a.startup_blow", "Startup blow time", "fan_a", "min", DidType::UInt16, false, 0, 65535, 30, 1,
      false, false, nullptr, 0, nullptr,
      "How long fan A runs after startup regardless of the temperature difference." },
    // 0x0210 fan.b.override_state
    { 0x0210u, "fan.b.override_state", "Manual override at startup", "fan_b", "", DidType::UInt8, false, 0, 1, 0, 1,
      true, false, OptionsFanControlMode, sizeof(OptionsFanControlMode) / sizeof(OptionsFanControlMode[0]),
      "PARAM_FAN_B_OVERRIDE_STATE",
      "Whether fan B follows the temperature difference from startup, or is held at the value below until something "
      "changes it." },
    // 0x0211 fan.b.override_value
    { 0x0211u, "fan.b.override_value", "Manual override value at startup", "fan_b", "", DidType::UInt8, false, 0, 1, 0,
      1, true, false, OptionsFanState, sizeof(OptionsFanState) / sizeof(OptionsFanState[0]),
      "PARAM_FAN_B_OVERRIDE_VALUE", "State fan B is driven to at startup while manual override is on." },
    // 0x0212 fan.b.target_delta_t
    { 0x0212u, "fan.b.target_delta_t", "Target temperature difference", "fan_b", "K", DidType::UInt16, false, 0, 5000,
      25, 50, true, false, nullptr, 0, nullptr,
      "Difference between the ambient and the mirror temperature fan B aims to bring the mirror back below. A target "
      "beyond 100 K is further than the mirror and the air ever drift apart, so the fan would never come on by "
      "itself." },
    // 0x0213 fan.b.target_hysteresis
    { 0x0213u, "fan.b.target_hysteresis", "Switch-on hysteresis", "fan_b", "K", DidType::UInt16, false, 0, 5000, 20, 50,
      true, false, nullptr, 0, nullptr,
      "How far above the target the difference has to rise before fan B switches on. It switches off again at the "
      "target itself. It is added to the target, so the same 100 K ceiling applies to it." },
    // 0x0214 fan.b.startup_blow
    { 0x0214u, "fan.b.startup_blow", "Startup blow time", "fan_b", "min", DidType::UInt16, false, 0, 65535, 30, 1,
      false, false, nullptr, 0, nullptr,
      "How long fan B runs after startup regardless of the temperature difference." },
    // 0x0300 temperature.ambient_filter
    { 0x0300u, "temperature.ambient_filter", "Ambient temperature filter", "temperature", "", DidType::UInt8, false, 0,
      255, 31, 1, false, false, nullptr, 0, nullptr,
      "Weight the previous ambient reading keeps against a new one. Zero passes the reading through unfiltered; a "
      "larger value settles more slowly." },
    // 0x0301 temperature.mirror_filter
    { 0x0301u, "temperature.mirror_filter", "Mirror temperature filter", "temperature", "", DidType::UInt8, false, 0,
      255, 31, 1, false, false, nullptr, 0, nullptr,
      "Weight the previous mirror reading keeps against a new one. Zero passes the reading through unfiltered; a "
      "larger value settles more slowly." },
    // 0x0302 temperature.enabled
    { 0x0302u, "temperature.enabled", "Temperature processing enabled", "temperature", "", DidType::UInt8, false, 0, 1,
      1, 1, true, false, OptionsFeatureState, sizeof(OptionsFeatureState) / sizeof(OptionsFeatureState[0]),
      "PARAM_TEMPERATURE_ENABLED",
      "Powers the infrared sensor and lets the fans follow the temperature difference. Disabled leaves the sensor "
      "supply off, and the fans stay under manual control." },
    // 0x0500 supply.sensor.max_voltage_drop
    { 0x0500u, "supply.sensor.max_voltage_drop", "Sensor supply maximum drop", "supply", "V", DidType::UInt16, false,
      20, 3300, 150, 1000, true, false, nullptr, 0, nullptr,
      "How far the infrared sensor supply may fall below the controller supply before it is treated as an overcurrent. "
      "Reaching it switches the sensor supply off until the next reset. The reference is the 3.3 V rail, so a "
      "threshold at or above it never trips." },
    // 0x0600 smartsw.short_to_vcc_diff
    { 0x0600u, "smartsw.short_to_vcc_diff", "Short to supply threshold", "smartsw", "V", DidType::UInt16, false, 0,
      6000, 250, 1000, true, false, nullptr, 0, nullptr,
      "A switched-off output closer to the supply voltage than this is reported as shorted to it. Past roughly half "
      "the supply a healthy switched-off output looks shorted to it, so the threshold has to stay well below the "
      "rail." },
    // 0x0601 smartsw.overcurrent_diff
    { 0x0601u, "smartsw.overcurrent_diff", "Overcurrent threshold", "smartsw", "V", DidType::UInt16, false, 1, 6000,
      500, 1000, true, false, nullptr, 0, nullptr,
      "A switched-on output further below the supply voltage than this is reported as overloaded. The same threshold, "
      "the other way round, decides whether a switched-on output is carrying a load at all. Zero calls every "
      "switched-on output overloaded." },
    // 0x0602 smartsw.open_load_detection
    { 0x0602u, "smartsw.open_load_detection", "Open load detection enabled", "smartsw", "", DidType::UInt8, false, 0, 1,
      0, 1, true, false, OptionsFeatureState, sizeof(OptionsFeatureState) / sizeof(OptionsFeatureState[0]),
      "PARAM_SMARTSW_OPEN_LOAD_DETECTION", "Reports a switched-on output that draws no current as an open load." },
    // 0x0700 uc_temperature.warning_level
    { 0x0700u, "uc_temperature.warning_level", "Warning level", "uc_temp", "degC", DidType::UInt8, false, 40, 125, 70,
      1, true, false, nullptr, 0, nullptr,
      "Controller junction temperature that raises a warning. Below 40 degC it would stand permanently in a warm room, "
      "and 125 degC is the junction limit of the controller." },
    // 0x0701 uc_temperature.error_level
    { 0x0701u, "uc_temperature.error_level", "Error level", "uc_temp", "degC", DidType::UInt8, false, 40, 125, 85, 1,
      true, false, nullptr, 0, nullptr,
      "Controller junction temperature that raises a fault. It has to sit above the warning level, which nothing "
      "checks. 125 degC is the junction limit of the controller." },
    // 0x000E motor1.last_position
    { 0x000Eu, "motor1.last_position", "Last motor 1 position", "learnt", "steps", DidType::UInt32, false, 0,
      2147483647, 0, 1, true, false, nullptr, 0, nullptr, "Last stopped position of motor 1" },
    // 0x010E motor2.last_position
    { 0x010Eu, "motor2.last_position", "Last motor 2 position", "learnt", "steps", DidType::UInt32, false, 0,
      2147483647, 0, 1, true, false, nullptr, 0, nullptr, "Last stopped position of motor 2" },
    // 0x080E motor3.last_position
    { 0x080Eu, "motor3.last_position", "Last motor 3 position", "learnt", "steps", DidType::UInt32, false, 0,
      2147483647, 0, 1, true, false, nullptr, 0, nullptr, "Last stopped position of motor 3" },
};

const size_t ParameterCount = sizeof(Parameters) / sizeof(Parameters[0]);

} // namespace catalogue
} // namespace scopelink
