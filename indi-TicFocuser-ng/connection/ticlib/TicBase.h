// Copyright (C) Pololu Corporation.  See LICENSE.txt for details.

/// \file Tic.h
///
/// This is the main header file for the Tic Stepper Motor Controller library
/// for Arduino.
///
/// For more information about the library, see the main repository at:
/// https://github.com/pololu/tic-arduino

#pragma once

#include <stdint.h>
#include <unistd.h>
#include "Stream.h"

enum class TicProduct
{
  Unknown = 0,
  T825 = 1,
  T834 = 2,
  T500 = 3,
  T249 = 4,
  Tic36v4 = 5,
};

/// This constant is used by the library to convert between milliamps and the
/// Tic's native current unit, which is 32 mA.  This is only valid for the Tic
/// T825 and Tic T834.
const uint8_t TicCurrentUnits = 32;

/// This constant is used by the library to convert between milliamps and the
/// Tic T249 native current unit, which is 40 mA.
const uint8_t TicT249CurrentUnits = 40;

/// This is used to represent a null or missing value for some of the Tic's
/// 16-bit input variables.
const uint16_t TicInputNull = 0xFFFF;

/// This enum defines the Tic's error bits.  See the "Error handling" section of
/// the Tic user's guide for more information about what these errors mean.
///
/// See TicBase::getErrorStatus() and TicBase::getErrorsOccurred().
enum class TicError
{
  IntentionallyDeenergized = 0,
  MotorDriverError         = 1,
  LowVin                   = 2,
  KillSwitch               = 3,
  RequiredInputInvalid     = 4,
  SerialError              = 5,
  CommandTimeout           = 6,
  SafeStartViolation       = 7,
  ErrLineHigh              = 8,
  SerialFraming            = 16,
  RxOverrun                = 17,
  Format                   = 18,
  Crc                      = 19,
  EncoderSkip              = 20,
};

/// This enum defines the Tic command codes which are used for its serial, I2C,
/// and USB interface.  These codes are used by the library and you should not
/// need to use them.
enum class TicCommand
{
  SetTargetPosition                 = 0xE0,
  SetTargetVelocity                 = 0xE3,
  HaltAndSetPosition                = 0xEC,
  HaltAndHold                       = 0x89,
  GoHome                            = 0x97,
  ResetCommandTimeout               = 0x8C,
  Deenergize                        = 0x86,
  Energize                          = 0x85,
  ExitSafeStart                     = 0x83,
  EnterSafeStart                    = 0x8F,
  Reset                             = 0xB0,
  ClearDriverError                  = 0x8A,
  SetSpeedMax                       = 0xE6,
  SetStartingSpeed                  = 0xE5,
  SetAccelMax                       = 0xEA,
  SetDecelMax                       = 0xE9,
  SetStepMode                       = 0x94,
  SetCurrentLimit                   = 0x91,
  SetDecayMode                      = 0x92,
  SetAgcOption                      = 0x98,
  GetVariable                       = 0xA1,
  GetVariableAndClearErrorsOccurred = 0xA2,
  GetSetting                        = 0xA8,
};

/// This enum defines the possible operation states for the Tic.
///
/// See TicBase::getOperationState().
enum class TicOperationState
{
  Reset             = 0,
  Deenergized       = 2,
  SoftError         = 4,
  WaitingForErrLine = 6,
  StartingUp        = 8,
  Normal            = 10,
};

/// This enum defines the possible planning modes for the Tic's step generation
/// code.
///
/// See TicBase::getPlanningMode().
enum class TicPlanningMode
{
  Off            = 0,
  TargetPosition = 1,
  TargetVelocity = 2,
};

/// This enum defines the possible causes of a full microcontroller reset for
/// the Tic.
///
/// See TicBase::getDeviceReset().
enum class TicReset
{
  PowerUp        = 0,
  Brownout       = 1,
  ResetLine      = 2,
  Watchdog       = 4,
  Software       = 8,
  StackOverflow  = 16,
  StackUnderflow = 32,
};

/// This enum defines the possible decay modes.
///
/// See TicBase::getDecayMode() and TicBase::setDecayMode().
enum class TicDecayMode
{
  /// This specifies "Mixed" decay mode on the Tic T825
  /// and "Mixed 50%" on the Tic T824.
  Mixed    = 0,

  /// This specifies "Slow" decay mode.
  Slow     = 1,

  /// This specifies "Fast" decay mode.
  Fast     = 2,

  /// This is the same as TicDecayMode::Mixed, but better expresses your
  /// intent if you want to use "Mixed 50%' mode on a Tic T834.
  Mixed50 = 0,

  /// This specifies "Mixed 25%" decay mode on the Tic T824
  /// and is the same as TicDecayMode::Mixed on the Tic T825.
  Mixed25 = 3,

  /// This specifies "Mixed 75%" decay mode on the Tic T824
  /// and is the same as TicDecayMode::Mixed on the Tic T825.
  Mixed75 = 4,
};

/// This enum defines the possible step modes.
///
/// See TicBase::getStepMode() and TicBase::setStepMode().
enum class TicStepMode
{
  Full    = 0,
  Half    = 1,

  Microstep1  = 0,
  Microstep2  = 1,
  Microstep4  = 2,
  Microstep8  = 3,
  Microstep16 = 4,
  Microstep32 = 5,
  Microstep2_100p = 6,
  Microstep64 = 7,
  Microstep128 = 8,
  Microstep256 = 9,
};

/// This enum defines possible AGC modes.
///
/// See TicBase::setAgcMode() and TicBase::getAgcMode().
enum class TicAgcMode
{
  Off = 0,
  On = 1,
  ActiveOff = 2,
};

/// This enum defines possible AGC buttom current limit percentages.
///
/// See TicBase::setAgcBottomCurrentLimit() and
/// TicBase:getAgcBottomCurrentLimit().
enum class TicAgcBottomCurrentLimit
{
  P45 = 0,
  P50 = 1,
  P55 = 2,
  P60 = 3,
  P65 = 4,
  P70 = 5,
  P75 = 6,
  P80 = 7,
};

/// This enum defines possible AGC current boost steps values.
///
/// See TicBase::setAgcCurrentBoostSteps() and
/// TicBase::getAgcCurrentBoostSteps().
enum class TicAgcCurrentBoostSteps
{
  S5 = 0,
  S7 = 1,
  S9 = 2,
  S11 = 3,
};

/// This enuam defines possible AGC frequency limit values.
///
/// See TicBase::setAgcFrequencyLimit() and TicBase::getAgcFrequencyLimit().
enum class TicAgcFrequencyLimit
{
  Off = 0,
  F225Hz = 1,
  F450Hz = 2,
  F675Hz = 3,
};

/// This enum defines the Tic's control pins.
enum class TicPin
{
  SCL = 0,
  SDA = 1,
  TX  = 2,
  RX  = 3,
  RC  = 4,
};

/// This enum defines the Tic's pin states.
///
/// See TicBase::getPinState().
enum class TicPinState
{
  HighImpedance = 0,
  InputPullUp   = 1,
  OutputLow     = 2,
  OutputHigh    = 3,
};

/// This enum defines the possible states of the Tic's main input.
enum class TicInputState
{
  /// The input is not ready yet.  More samples are needed, or a command has not
  /// been received yet.
  NotReady = 0,

  /// The input is invalid.
  Invalid = 1,

  /// The input is valid and is telling the Tic to halt the motor.
  Halt = 2,

  /// The input is valid and is telling the Tic to go to a target position,
  /// which you can get with TicBase::getInputAfterScaling().
  Position = 3,

  /// The input is valid and is telling the Tic to go to a target velocity,
  /// which you can get with TicBase::getInputAfterScaling().
  Velocity = 4,
};

/// This enum defines the bits in the Tic's Misc Flags 1 register.  You should
/// not need to use this directly.  See TicBase::getEnergized() and
/// TicBase::getPositionUncertain().
enum class TicMiscFlags1
{
  Energized = 0,
  PositionUncertain = 1,
  ForwardLimitActive = 2,
  ReverseLimitActive = 3,
  HomingActive = 4,
};

/// This enum defines possible motor driver errors for the Tic T249.
///
/// See TicBase::getLastMotorDriverError().
enum class TicMotorDriverError
{
  None = 0,
  OverCurrent = 1,
  OverTemperature = 2,
};

/// This enum defines the bits in the "Last HP driver errors" variable.
///
/// See TicBase::getLastHpDriverErrors().
enum class TicHpDriverError
{
  OverTemperature = 0,
  OverCurrentA = 1,
  OverCurrentB = 2,
  PreDriverFaultA = 3,
  PreDriverFaultB = 4,
  UnderVoltage = 5,
  Verify = 7,
};

/// This is a base class used to represent a connection to a Tic.  This class
/// provides high-level functions for sending commands to the Tic and reading
/// data from it.
///
/// See the subclasses of this class, TicSerial and TicI2C.
class TicBase
{
public:

  virtual ~TicBase() {}

  /// You can use this function to specify what type of Tic you are using.
  ///
  /// Example usage (pick one of the following):
  /// ```
  /// tic.setProduct(TicProduct::T500);
  /// tic.setProduct(TicProduct::T834);
  /// tic.setProduct(TicProduct::T825);
  /// tic.setProduct(TicProduct::T249);
  /// tic.setProduct(TicProduct::Tic36v4);
  /// ```
  ///
  /// This changes the behavior of the setCurrentLimit() function.
  void setProduct(TicProduct product)
  {
    this->product = product;
  }

  /// Sets the target position of the Tic, in microsteps.
  ///
  /// Example usage:
  /// ```
  /// tic.setTargetPosition(100);
  /// ```
  ///
  /// This function sends a "Set target position" to the Tic.  If the Control
  /// mode is set to Serial/I2C/USB, the Tic will start moving the motor to
  /// reach the target position.  If the control mode is something other than
  /// Serial, this command will be silently ignored.
  ///
  /// See also getTargetPosition().
  void setTargetPosition(int32_t position)
  {
    commandW32(TicCommand::SetTargetPosition, position);
  }

  /// Sets the target velocity of the Tic, in microsteps per 10000 seconds.
  ///
  /// Example usage:
  /// ```
  /// tic.setTargetVelocity(-1800000);  // -180 steps per second
  /// ```
  ///
  /// This function sends a "Set target velocity" command to the Tic.  If the
  /// Control mode is set to Serial/I2C/USB, the Tic will start accelerating or
  /// decelerating to reach the target velocity.
  ///
  /// If the control mode is something other than Serial, this command will be
  /// silently ignored.
  ///
  /// See also getTargetVelocity().
  void setTargetVelocity(int32_t velocity)
  {
    commandW32(TicCommand::SetTargetVelocity, velocity);
  }

  /// Stops the motor abruptly without respecting the deceleration limit and
  /// sets the "Current position" variable, which represents where the Tic
  /// currently thinks the motor's output is.
  ///
  /// Example usage:
  /// ```
  /// tic.haltAndSetPosition(0);
  /// ```
  ///
  /// This function sends a "Halt and set position" command to the Tic.  Besides
  /// stopping the motor and setting the current position, this command also
  /// clears the "Postion uncertain" flag, sets the "Input state" to "halt", and
  /// clears the "Input after scaling" variable.
  ///
  /// If the control mode is something other than Serial, this command will
  /// be silently ignored.
  void haltAndSetPosition(int32_t position)
  {
    commandW32(TicCommand::HaltAndSetPosition, position);
  }

  /// Stops the motor abruptly without respecting the deceleration limit.
  ///
  /// Example usage:
  /// ```
  /// tic.haltAndHold();
  /// ```
  ///
  /// This function sends a "Halt and hold" command to the Tic.  Besides stopping
  /// the motor, this command also sets the "Position uncertain" flag (because
  /// the abrupt stop might cause steps to be missed), sets the "Input state" to
  /// "halt", and clears the "Input after scaling" variable.
  ///
  /// If the control mode is something other than Serial/I2C/USB, ths
  /// command will be silently ignored.
  ///
  /// See also deenergize().
  void haltAndHold()
  {
    commandQuick(TicCommand::HaltAndHold);
  }

  /// Tells the Tic to start its homing procedure in the reverse direction.
  ///
  /// See the "Homing" section of the Tic user's guide for details.
  ///
  /// See also goHomeForward().
  void goHomeReverse()
  {
    commandW7(TicCommand::GoHome, 0);
  }

  /// Tells the Tic to start its homing procedure in the forward direction.
  ///
  /// See the "Homing" section of the Tic user's guide for details.
  ///
  /// See also goHomeReverse().
  void goHomeForward()
  {
    commandW7(TicCommand::GoHome, 1);
  }

  /// Prevents the "Command timeout" error from happening for some time.
  ///
  /// Example usage:
  /// ```
  /// tic.resetCommandTimeout();
  /// ```
  ///
  /// This function sends a "Reset command timeout" command to the Tic.
  void resetCommandTimeout()
  {
    commandQuick(TicCommand::ResetCommandTimeout);
  }

  /// De-energizes the stepper motor coils.
  ///
  /// Example usage:
  /// ```
  /// tic.deenergize();
  /// ```
  ///
  /// This function sends a De-energize command to the Tic, causing it to disable
  /// its stepper motor driver.  The motor will stop moving and consuming power.
  /// The Tic will set the "Intentionally de-energized" error bit, turn on its
  /// red LED, and drive its ERR line high.  This command also sets the
  /// "Position uncertain" flag (because the Tic is no longer in control of the
  /// motor's position).
  ///
  /// Note that the Energize command, which can be sent with energize(), will
  /// undo the effect of this command (except it will leave the "Position
  /// uncertain" flag set) and could make the system start up again.
  ///
  /// See also haltAndHold().
  void deenergize()
  {
    commandQuick(TicCommand::Deenergize);
  }

  /// Sends the Energize command.
  ///
  /// Example usage:
  /// ```
  /// tic.energize();
  /// ```
  ///
  /// This function sends an Energize command to the Tic, clearing the
  /// "Intentionally de-energized" error bit.  If there are no other errors,
  /// this allows the system to start up.
  void energize()
  {
    commandQuick(TicCommand::Energize);
  }

  /// Sends the "Exit safe start" command.
  ///
  /// Example usage:
  /// ```
  /// tic.exitSafeStart();
  /// ```
  ///
  /// In Serial/I2C/USB control mode, this command causes the safe start
  /// violation error to be cleared for 200 ms.  If there are no other errors,
  /// this allows the system to start up.
  void exitSafeStart()
  {
    commandQuick(TicCommand::ExitSafeStart);
  }

  /// Sends the "Enter safe start" command.
  ///
  /// Example usage:
  /// ```
  /// tic.enterSafeStart();
  /// ```
  ///
  /// This command has no effect if safe-start is disabled in the Tic's settings.
  ///
  /// In Serial/I2C/USB control mode, this command causes the Tic to stop the
  /// motor and set its safe start violation error bit.  An "Exit safe start"
  /// command is required before the Tic will move the motor again.
  ///
  /// See the Tic user's guide for information about what this command does in
  /// the other control modes.
  void enterSafeStart()
  {
    commandQuick(TicCommand::EnterSafeStart);
  }

  /// Sends the Reset command.
  ///
  /// Example usage:
  /// ```
  /// tic.reset();
  /// ```
  ///
  /// This command makes the Tic forget most parts of its current state.  For
  /// more information, see the Tic user's guide.
  void reset()
  {
    commandQuick(TicCommand::Reset);

    // The Tic's serial and I2C interfaces will be unreliable for a brief period
    // after the Tic receives the Reset command, so we delay 10 ms here.
    usleep(10);
  }

  /// Attempts to clear a motor driver error.
  ///
  /// Example usage:
  /// ```
  /// tic.clearDriverError();
  /// ```
  ///
  /// This function sends a "Clear driver error" command to the Tic.  For more
  /// information, see the Tic user's guide.
  void clearDriverError()
  {
    commandQuick(TicCommand::ClearDriverError);
  }

  /// Temporarily sets the maximum speed, in units of steps per 10000 seconds.
  ///
  /// Example usage:
  /// ```
  /// tic.setMaxSpeed(5550000);  // 555 steps per second
  /// ```
  ///
  /// This function sends a "Set max speed" command to the Tic.  For more
  /// information, see the Tic user's guide.
  ///
  /// See also getMaxSpeed().
  void setMaxSpeed(uint32_t speed)
  {
    commandW32(TicCommand::SetSpeedMax, speed);
  }

  /// Temporarily sets the starting speed, in units of steps per 10000 seconds.
  ///
  /// Example usage:
  /// ```
  /// tic.setStartingSpeed(500000);  // 50 steps per second
  /// ```
  ///
  /// This function sends a "Set starting speed" command to the Tic.  For more
  /// information, see the Tic user's guide.
  ///
  /// See also getStartingSpeed().
  void setStartingSpeed(uint32_t speed)
  {
    commandW32(TicCommand::SetStartingSpeed, speed);
  }

  /// Temporarily sets the maximum acceleration, in units of steps per second
  /// per 100 seconds.
  ///
  /// Example usage:
  /// ```
  /// tic.setMaxAccel(10000);  // 100 steps per second per second
  /// ```
  ///
  /// This function sends a "Set max acceleration" command to the Tic.  For more
  /// information, see the Tic user's guide.
  ///
  /// See also getMaxAccel().
  void setMaxAccel(uint32_t accel)
  {
    commandW32(TicCommand::SetAccelMax, accel);
  }

  /// Temporarily sets the maximum deceleration, in units of steps per second
  /// per 100 seconds.
  ///
  /// Example usage:
  /// ```
  /// tic.setMaxDecel(10000);  // 100 steps per second per second
  /// ```
  ///
  /// This function sends a "Set max deceleration" command to the Tic.  For more
  /// information, see the Tic user's guide.
  ///
  /// See also getMaxDecel().
  void setMaxDecel(uint32_t decel)
  {
    commandW32(TicCommand::SetDecelMax, decel);
  }

  /// Temporarily sets the stepper motor's step mode, which defines how many
  /// microsteps correspond to one full step.
  ///
  /// Example usage:
  /// ```
  /// tic.setStepMode(TicStepMode::Microstep8);
  /// ```
  ///
  /// This function sends a "Set step mode" command to the Tic.  For more
  /// information, see the Tic user's guide.
  ///
  /// See also getStepMode().
  void setStepMode(TicStepMode mode)
  {
    commandW7(TicCommand::SetStepMode, (uint8_t)mode);
  }

  /// Temporarily sets the stepper motor coil current limit in milliamps.  If
  /// the desired current limit is not available, this function uses the closest
  /// current limit that is lower than the desired one.
  ///
  /// When converting the current limit from milliamps to a code to send to the
  /// Tic, this function needs to know what kind of Tic you are using.  By
  /// default, this function assumes you are using a Tic T825 or Tic T834.  If
  /// you are using a different kind of Tic, we recommend calling setProduct()
  /// some time before calling setCurrentLimit().
  ///
  /// Example usage:
  /// ```
  /// tic.setCurrentLimit(500);  // 500 mA
  /// ```
  ///
  /// This function sends a "Set current limit" command to the Tic.  For more
  /// information about this command and how to choose a good current limit, see
  /// the Tic user's guide.
  ///
  /// See also getCurrentLimit().
  void setCurrentLimit(uint16_t limit);

  /// Temporarily sets the stepper motor driver's decay mode.
  ///
  /// Example usage:
  /// ```
  /// tic.setDecayMode(TicDecayMode::Slow);
  /// ```
  ///
  /// The decay modes are documented in the Tic user's guide.
  ///
  /// See also getDecayMode().
  void setDecayMode(TicDecayMode mode)
  {
    commandW7(TicCommand::SetDecayMode, (uint8_t)mode);
  }

  /// Temporarily sets the AGC mode.
  ///
  /// This is only valid for the Tic T249.
  ///
  /// See also getAgcMode().
  void setAgcMode(TicAgcMode mode)
  {
    commandW7(TicCommand::SetAgcOption, (uint8_t)mode & 0xF);
  }

  /// Temporarily sets the AGC bottom current limit.
  ///
  /// This is only valid for the Tic T249.
  ///
  /// See also getAgcBottomCurrentLimit().
  void setAgcBottomCurrentLimit(TicAgcBottomCurrentLimit limit)
  {
    commandW7(TicCommand::SetAgcOption, 0x10 | ((uint8_t)limit & 0xF));
  }

  /// Temporarily sets the AGC current boost steps.
  ///
  /// This is only valid for the Tic T249.
  ///
  /// See also getAgcCurrentBoostSteps().
  void setAgcCurrentBoostSteps(TicAgcCurrentBoostSteps steps)
  {
    commandW7(TicCommand::SetAgcOption, 0x20 | ((uint8_t)steps & 0xF));
  }

  /// Temporarily sets the AGC frequency limit.
  ///
  /// This is only valid for the Tic T249.
  ///
  /// See also getAgcFrequencyLimit().
  void setAgcFrequencyLimit(TicAgcFrequencyLimit limit)
  {
    commandW7(TicCommand::SetAgcOption, 0x30 | ((uint8_t)limit & 0xF));
  }

  /// Gets the Tic's current operation state, which indicates whether it is
  /// operating normally or in an error state.
  ///
  /// Example usage:
  /// ```
  /// if (tic.getOperationState() != TicOperationState::Normal)
  /// {
  ///   // There is an error, or the Tic is starting up.
  /// }
  /// ```
  ///
  /// For more information, see the "Error handling" section of the Tic user's
  /// guide.
  TicOperationState getOperationState()
  {
    return (TicOperationState)getVar8(VarOffset::OperationState);
  }

  /// Returns true if the motor driver is energized (trying to send current to
  /// its outputs).
  bool getEnergized()
  {
    return getVar8(VarOffset::MiscFlags1) >>
      (uint8_t)TicMiscFlags1::Energized & 1;
  }

  /// Gets a flag that indicates whether there has been external confirmation that
  /// the value of the Tic's "Current position" variable is correct.
  ///
  /// For more information, see the "Error handling" section of the Tic user's
  /// guide.
  bool getPositionUncertain()
  {
    return getVar8(VarOffset::MiscFlags1) >>
      (uint8_t)TicMiscFlags1::PositionUncertain & 1;
  }

  /// Returns true if one of the forward limit switches is active.
  bool getForwardLimitActive()
  {
    return getVar8(VarOffset::MiscFlags1) >>
      (uint8_t)TicMiscFlags1::ForwardLimitActive & 1;
  }

  /// Returns true if one of the reverse limit switches is active.
  bool getReverseLimitActive()
  {
    return getVar8(VarOffset::MiscFlags1) >>
      (uint8_t)TicMiscFlags1::ReverseLimitActive & 1;
  }

  /// Returns true if the Tic's homing procedure is running.
  bool getHomingActive()
  {
    return getVar8(VarOffset::MiscFlags1) >>
      (uint8_t)TicMiscFlags1::HomingActive & 1;
  }

  /// Gets the errors that are currently stopping the motor.
  ///
  /// Each bit in the returned register represents a different error.  The bits
  /// are defined in ::TicError enum.
  ///
  /// Example usage:
  /// ```
  /// uint16_t errors = tic.getErrorStatus();
  /// if (errors & (1 << (uint8_t)TicError::LowVin))
  /// {
  ///   // handle loss of power
  /// }
  /// ```
  uint16_t getErrorStatus()
  {
    return getVar16(VarOffset::ErrorStatus);
  }

  /// Gets the errors that have occurred since the last time this function was called.
  ///
  /// Note that the Tic Control Center constantly clears the bits in this
  /// register, so if you are running the Tic Control Center then you will not
  /// be able to reliably detect errors with this function.
  ///
  /// Each bit in the returned register represents a different error.  The bits
  /// are defined in ::TicError enum.
  ///
  /// Example usage:
  /// ```
  /// uint32_t errors = tic.getErrorsOccurred();
  /// if (errors & (1 << (uint8_t)TicError::MotorDriverError))
  /// {
  ///   // handle a motor driver error
  /// }
  /// ```
  uint32_t getErrorsOccurred()
  {
    uint32_t result;
    getSegment(TicCommand::GetVariableAndClearErrorsOccurred,
      VarOffset::ErrorsOccurred, 4, &result);
    return result;
  }

  /// Returns the current planning mode for the Tic's step generation code.
  ///
  /// This tells us whether the Tic is sending steps, and if it is sending
  /// steps, tells us whether it is in Target Position or Target Velocity mode.
  ///
  /// Example usage:
  /// ```
  /// if (tic.getPlanningMode() == TicPlanningMode::TargetPosition)
  /// {
  ///     // The Tic is moving the stepper motor to a target position, or has
  ///     // already reached it and is at rest.
  /// }
  /// ```
  TicPlanningMode getPlanningMode()
  {
    return (TicPlanningMode)getVar8(VarOffset::PlanningMode);
  }

  /// Gets the target position, in microsteps.
  ///
  /// This is only relevant if the planning mode from getPlanningMode() is
  /// TicPlanningMode::Position.
  ///
  /// See also setTargetPosition().
  int32_t getTargetPosition()
  {
    return getVar32(VarOffset::TargetPosition);
  }

  /// Gets the target velocity, in microsteps per 10000 seconds.
  ///
  /// This is only relevant if the planning mode from getPlanningMode() is
  /// TicPlanningMode::Velocity.
  ///
  /// See also setTargetVelocity().
  int32_t getTargetVelocity()
  {
    return getVar32(VarOffset::TargetVelocity);
  }

  /// Gets the current maximum speed, in microsteps per 10000 seconds.
  ///
  /// This is the current value, which could differ from the value in the Tic's
  /// settings.
  ///
  /// See also setMaxSpeed().
  uint32_t getMaxSpeed()
  {
    return getVar32(VarOffset::SpeedMax);
  }

  /// Gets the starting speed in microsteps per 10000 seconds.
  ///
  /// This is the current value, which could differ from the value in the
  /// Tic's settings.
  ///
  /// Example usage:
  /// ```
  /// uint32_t startingSpeed = tic.getStartingSpeed();
  /// ```
  ///
  /// See also setStartingSpeed().
  uint32_t getStartingSpeed()
  {
    return getVar32(VarOffset::StartingSpeed);
  }

  /// Gets the maximum acceleration, in microsteps per second per 100 seconds.
  ///
  /// This is the current value, which could differ from the value in the Tic's
  /// settings.
  ///
  /// Example usage:
  /// ```
  /// uint32_t accelMax = tic.getMaxAccel();
  /// ```
  ///
  /// See also setMaxAccel().
  uint32_t getMaxAccel()
  {
    return getVar32(VarOffset::AccelMax);
  }

  /// Gets the maximum deceleration, in microsteps per second per 100 seconds.
  ///
  /// This is the current value, which could differ from the value in the Tic's
  /// settings.
  ///
  /// Example usage:
  /// ```
  /// uint32_t decelMax = tic.getMaxDecel();
  /// ```
  ///
  /// See also setMaxDecel().
  uint32_t getMaxDecel()
  {
    return getVar32(VarOffset::DecelMax);
  }

  /// Gets the current position of the stepper motor, in microsteps.
  ///
  /// Note that this just tracks steps that the Tic has commanded the stepper
  /// driver to take; it could be different from the actual position of the
  /// motor for various reasons.
  ///
  /// For an example of how to use this this, see the SerialPositionControl
  /// example or the I2CPositionControl exmaple.
  ///
  /// See also haltAndSetPosition().
  int32_t getCurrentPosition()
  {
    return getVar32(VarOffset::CurrentPosition);
  }

  /// Gets the current velocity of the stepper motor, in microsteps per 10000
  /// seconds.
  ///
  /// Note that this is just the velocity used in the Tic's step planning
  /// algorithms, and it might not correspond to the actual velocity of the
  /// motor for various reasons.
  ///
  /// Example usage:
  /// ```
  /// int32_t velocity = tic.getCurrentVelocity();
  /// ```
  int32_t getCurrentVelocity()
  {
    return getVar32(VarOffset::CurrentVelocity);
  }

  /// Gets the acting target position, in microsteps.
  ///
  /// This is a variable used in the Tic's target position step planning
  /// algorithm, and it could be invalid while the motor is stopped.
  ///
  /// This is mainly intended for getting insight into how the Tic's algorithms
  /// work or troubleshooting issues, and most people should not use this.
  uint32_t getActingTargetPosition()
  {
    return getVar32(VarOffset::ActingTargetPosition);
  }

  /// Gets the time since the last step, in timer ticks.
  ///
  /// Each timer tick represents one third of a microsecond.  The Tic only
  /// updates this variable every 5 milliseconds or so, and it could be invalid
  /// while the motor is stopped.
  ///
  /// This is mainly intended for getting insight into how the Tic's algorithms
  /// work or troubleshooting issues, and most people should not use this.
  uint32_t getTimeSinceLastStep()
  {
    return getVar32(VarOffset::TimeSinceLastStep);
  }

  /// Gets the cause of the controller's last full microcontroller reset.
  ///
  /// Example usage:
  /// ```
  /// if (tic.getDeviceReset() == TicReset::Brownout)
  /// {
  ///   // There was a brownout reset; the power supply could not keep up.
  /// }
  /// ```
  ///
  /// The Reset command (reset()) does not affect this variable.
  TicReset getDeviceReset()
  {
    return (TicReset)getVar8(VarOffset::DeviceReset);
  }

  /// Gets the current measurement of the VIN voltage, in millivolts.
  ///
  /// Example usage:
  /// ```
  /// uint16_t power = tic.getVinVoltage();
  /// ```
  uint16_t getVinVoltage()
  {
    return getVar16(VarOffset::VinVoltage);
  }

  /// Gets the time since the last full reset of the Tic's microcontroller, in
  /// milliseconds.
  ///
  /// Example usage:
  /// ```
  /// uint32_t upTime = tic.getUpTime();
  /// ```
  ///
  /// A Reset command (reset())does not count.
  uint32_t getUpTime()
  {
    return getVar32(VarOffset::UpTime);
  }

  /// Gets the raw encoder count measured from the Tic's RX and TX lines.
  ///
  /// Example usage:
  /// ```
  /// int32_t encoderPosition = getEncoderPosition();
  /// ```
  int32_t getEncoderPosition()
  {
    return getVar32(VarOffset::EncoderPosition);
  }

  /// Gets the raw pulse width measured on the Tic's RC input, in units of
  /// twelfths of a microsecond.
  ///
  /// Returns TicInputNull if the RC input is missing or invalid.
  ///
  /// Example usage:
  /// ```
  /// uint16_t pulseWidth = tic.getRCPulseWidth();
  /// if (pulseWidth != TicInputNull && pulseWidth > 18000)
  /// {
  ///   // Pulse width is greater than 1500 microseconds.
  /// }
  /// ```
  uint16_t getRCPulseWidth()
  {
    return getVar16(VarOffset::RCPulseWidth);
  }

  /// Gets the analog reading from the specified pin.
  ///
  /// The reading is left-justified, so 0xFFFF represents a voltage equal to the
  /// Tic's 5V pin (approximately 4.8 V).
  ///
  /// Returns TicInputNull if the analog reading is disabled or not ready.
  ///
  /// Example usage:
  /// ```
  /// uint16_t reading = getAnalogReading(TicPin::SDA);
  /// if (reading != TicInputNull && reading < 32768)
  /// {
  ///   // The reading is less than about 2.4 V.
  /// }
  /// ```
  uint16_t getAnalogReading(TicPin pin)
  {
    uint8_t offset = VarOffset::AnalogReadingSCL + 2 * (uint8_t)pin;
    return getVar16(offset);
  }

  /// Gets a digital reading from the specified pin.
  ///
  /// Returns `true` for high and `false` for low.
  ///
  /// Example usage:
  /// ```
  /// if (tic.getDigitalReading(TicPin::RC))
  /// {
  ///   // Something is driving the RC pin high.
  /// }
  /// ```
  bool getDigitalReading(TicPin pin)
  {
    uint8_t readings = getVar8(VarOffset::DigitalReadings);
    return (readings >> (uint8_t)pin) & 1;
  }

  /// Gets the current state of a pin, i.e. what kind of input or output it is.
  ///
  /// Note that the state might be misleading if the pin is being used as a
  /// serial or I2C pin.
  ///
  /// Example usage:
  ///
  /// ```
  /// if (tic.getPinState(TicPin::SCL) == TicPinState::OutputHigh)
  /// {
  ///   // SCL is driving high.
  /// }
  /// ```
  TicPinState getPinState(TicPin pin)
  {
    uint8_t states = getVar8(VarOffset::PinStates);
    return (TicPinState)(states >> (2 * (uint8_t)pin) & 0b11);
  }

  /// Gets the current step mode of the stepper motor.
  ///
  /// Example usage:
  /// ```
  /// if (tic.getStepMode() == TicStepMode::Microstep8)
  /// {
  ///   // The Tic is currently using 1/8 microsteps.
  /// }
  /// ```
  TicStepMode getStepMode()
  {
    return (TicStepMode)getVar8(VarOffset::StepMode);
  }

  /// Gets the stepper motor coil current limit in milliamps.
  ///
  /// This is the value being used now, which could differ from the value in the
  /// Tic's settings.
  ///
  /// Example usage:
  /// ```
  /// uint16_t current = tic.getCurrentLimit();
  /// ```
  ///
  /// By default, this function assumes you are using a Tic T825 or Tic T834.
  /// If you are using a different kind of Tic, we recommend calling
  /// setProduct() some time before calling getCurrentLimit().
  ///
  /// See also setCurrentLimit().
  uint16_t getCurrentLimit();

  /// Gets the current decay mode of the stepper motor driver.
  ///
  /// Example usage:
  /// ```
  /// if (tic.getDecayMode() == TicDecayMode::Slow)
  /// {
  ///   // The Tic is in slow decay mode.
  /// }
  /// ```
  ///
  /// See setDecayMode().
  TicDecayMode getDecayMode()
  {
    return (TicDecayMode)getVar8(VarOffset::DecayMode);
  }

  /// Gets the current state of the Tic's main input.
  ///
  /// Example usage:
  /// ```
  /// if (tic.getInputState() == TicInputState::Position)
  /// {
  ///   // The Tic's input is specifying a target position.
  /// }
  /// ```
  ///
  /// See TicInputState for more information.
  TicInputState getInputState()
  {
    return (TicInputState)getVar8(VarOffset::InputState);
  }

  /// Gets a variable used in the process that converts raw RC and analog values
  /// into a motor position or speed.  This is mainly for debugging your input
  /// scaling settings in an RC or analog mode.
  ///
  /// A value of TicInputNull means the input value is not available.
  uint16_t getInputAfterAveraging()
  {
    return getVar16(VarOffset::InputAfterAveraging);
  }

  /// Gets a variable used in the process that converts raw RC and analog values
  /// into a motor position or speed.  This is mainly for debugging your input
  /// scaling settings in an RC or analog mode.
  ///
  /// A value of TicInputNull means the input value is not available.
  uint16_t getInputAfterHysteresis()
  {
    return getVar16(VarOffset::InputAfterHysteresis);
  }

  /// Gets the value of the Tic's main input after scaling has been applied.
  ///
  /// If the input is valid, this number is the target position or target
  /// velocity specified by the input.
  ///
  /// Example usage:
  /// ```
  /// if (tic.getInputAfter
  /// ```
  ///
  /// See also getInputState().
  int32_t getInputAfterScaling()
  {
    return getVar32(VarOffset::InputAfterScaling);
  }

  /// Gets the cause of the last motor driver error.
  ///
  /// This is only valid for the Tic T249.
  TicMotorDriverError getLastMotorDriverError()
  {
    return (TicMotorDriverError)getVar8(VarOffset::LastMotorDriverError);
  }

  /// Gets the AGC mode.
  ///
  /// This is only valid for the Tic T249.
  ///
  /// See also setAgcMode().
  TicAgcMode getAgcMode()
  {
    return (TicAgcMode)getVar8(VarOffset::AgcMode);
  }

  /// Gets the AGC bottom current limit.
  ///
  /// This is only valid for the Tic T249.
  ///
  /// See also setAgcBottomCurrentLimit().
  TicAgcBottomCurrentLimit getAgcBottomCurrentLimit()
  {
    return (TicAgcBottomCurrentLimit)getVar8(VarOffset::AgcBottomCurrentLimit);
  }

  /// Gets the AGC current boost steps.
  ///
  /// This is only valid for the Tic T249.
  ///
  /// See also setAgcCurrentBoostSteps().
  TicAgcCurrentBoostSteps getAgcCurrentBoostSteps()
  {
    return (TicAgcCurrentBoostSteps)getVar8(VarOffset::AgcCurrentBoostSteps);
  }

  /// Gets the AGC frequency limit.
  ///
  /// This is only valid for the Tic T249.
  ///
  /// See also setAgcFrequencyLimit().
  TicAgcFrequencyLimit getAgcFrequencyLimit()
  {
    return (TicAgcFrequencyLimit)getVar8(VarOffset::AgcFrequencyLimit);
  }

  /// Gets the "Last HP driver errors" variable.
  ///
  /// Each bit in this register represents an error.  If the bit is 1, the
  /// error was one of the causes of the Tic's last motor driver error.
  ///
  /// This is only valid for the Tic 36v4.
  uint8_t getLastHpDriverErrors()
  {
    return getVar8(VarOffset::LastHpDriverErrors);
  }

  /// Gets a contiguous block of settings from the Tic's EEPROM.
  ///
  /// The maximum length that can be fetched is 15 bytes.
  ///
  /// Example usage:
  /// ```
  /// // Get the Tic's serial device number.
  /// uint8_t deviceNumber;
  /// tic.getSetting(7, 1, &deviceNumber);
  /// ```
  ///
  /// This library does not attempt to interpret the settings and say what they
  /// mean.  If you are interested in how the settings are encoded in the Tic's
  /// EEPROM, see the "Settings reference" section of the Tic user's guide.
  void getSetting(uint8_t offset, uint8_t length, uint8_t * buffer)
  {
    getSegment(TicCommand::GetSetting, offset, length, buffer);
  }

  /// Returns 0 if the last communication with the device was successful, and
  /// non-zero if there was an error.
  uint8_t getLastError()
  {
    return _lastError;
  }

protected:
  /// Zero if the last communication with the device was successful, non-zero
  /// otherwise.
  uint8_t _lastError = 0;

private:
  enum VarOffset
  {
    OperationState        = 0x00, // uint8_t
    MiscFlags1            = 0x01, // uint8_t
    ErrorStatus           = 0x02, // uint16_t
    ErrorsOccurred        = 0x04, // uint32_t
    PlanningMode          = 0x09, // uint8_t
    TargetPosition        = 0x0A, // int32_t
    TargetVelocity        = 0x0E, // int32_t
    StartingSpeed         = 0x12, // uint32_t
    SpeedMax              = 0x16, // uint32_t
    DecelMax              = 0x1A, // uint32_t
    AccelMax              = 0x1E, // uint32_t
    CurrentPosition       = 0x22, // int32_t
    CurrentVelocity       = 0x26, // int32_t
    ActingTargetPosition  = 0x2A, // int32_t
    TimeSinceLastStep     = 0x2E, // uint32_t
    DeviceReset           = 0x32, // uint8_t
    VinVoltage            = 0x33, // uint16_t
    UpTime                = 0x35, // uint32_t
    EncoderPosition       = 0x39, // int32_t
    RCPulseWidth          = 0x3D, // uint16_t
    AnalogReadingSCL      = 0x3F, // uint16_t
    AnalogReadingSDA      = 0x41, // uint16_t
    AnalogReadingTX       = 0x43, // uint16_t
    AnalogReadingRX       = 0x45, // uint16_t
    DigitalReadings       = 0x47, // uint8_t
    PinStates             = 0x48, // uint8_t
    StepMode              = 0x49, // uint8_t
    CurrentLimit          = 0x4A, // uint8_t
    DecayMode             = 0x4B, // uint8_t
    InputState            = 0x4C, // uint8_t
    InputAfterAveraging   = 0x4D, // uint16_t
    InputAfterHysteresis  = 0x4F, // uint16_t
    InputAfterScaling     = 0x51, // uint16_t
    LastMotorDriverError  = 0x55, // uint8_t
    AgcMode               = 0x56, // uint8_t
    AgcBottomCurrentLimit = 0x57, // uint8_t
    AgcCurrentBoostSteps  = 0x58, // uint8_t
    AgcFrequencyLimit     = 0x59, // uint8_t
    LastHpDriverErrors    = 0xFF, // uint8_t
  };

  uint8_t getVar8(uint8_t offset)
  {
    uint8_t result;
    getSegment(TicCommand::GetVariable, offset, 1, &result);
    return result;
  }

  uint16_t getVar16(uint8_t offset)
  {
    uint8_t buffer[2];
    getSegment(TicCommand::GetVariable, offset, 2, &buffer);
    return ((uint16_t)buffer[0] << 0) | ((uint16_t)buffer[1] << 8);
  }

  uint32_t getVar32(uint8_t offset)
  {
    uint8_t buffer[4];
    getSegment(TicCommand::GetVariable, offset, 4, buffer);
    return ((uint32_t)buffer[0] << 0) |
      ((uint32_t)buffer[1] << 8) |
      ((uint32_t)buffer[2] << 16) |
      ((uint32_t)buffer[3] << 24);
  }

  virtual void commandQuick(TicCommand cmd) = 0;
  virtual void commandW32(TicCommand cmd, uint32_t val) = 0;
  virtual void commandW7(TicCommand cmd, uint8_t val) = 0;
  virtual void getSegment(TicCommand cmd, uint8_t offset,
    uint8_t length, void * buffer) = 0;

  TicProduct product = TicProduct::Unknown;
};

/// Represents a serial connection to a Tic.
///
/// For the high-level commands you can use on this object, see TicBase.
class TicSerial : public TicBase
{
public:
  /// Creates a new TicSerial object.
  ///
  /// The `stream` argument should be a hardware or software serial object.
  /// This class will store a pointer to it and use it to communicate with the
  /// Tic.  You should initialize it and set it to use the correct baud rate
  /// before sending commands with this class.
  ///
  /// The `deviceNumber` argument is optional.  If it is omitted or 255, the
  /// TicSerial object will use the compact protocol.  If it is a number between
  /// 0 and 127, it specifies the device number to use in Pololu protocol,
  /// allowing you to control multiple Tic controllers on a single serial bus.
  ///
  /// For example, to use the first open hardware serial port to send compact
  /// protocol commands to one Tic, write this at the top of your sketch:
  /// ```
  /// TicSerial tic(SERIAL_PORT_HARDWARE_OPEN);
  /// ```
  ///
  /// For example, to use a SoftwareSerial port and send Pololu protocol
  /// commands to two different Tic controllers, write this at the top of your sketch:
  ///
  /// ```
  /// #include <SoftwareSerial.h>
  /// SoftwareSerial ticSerial(10, 11);
  /// TicSerial tic1(ticSerial, 14);
  /// TicSerial tic2(ticSerial, 15);
  /// ```
  TicSerial(Stream & stream, uint8_t deviceNumber = 255) :
    _stream(&stream),
    _deviceNumber(deviceNumber)
  {
  }

  /// Gets the serial device number specified in the constructor.
  uint8_t getDeviceNumber() { return _deviceNumber; }

private:
  Stream * const _stream;
  const uint8_t _deviceNumber;

  void commandQuick(TicCommand cmd) { sendCommandHeader(cmd); }
  void commandW32(TicCommand cmd, uint32_t val);
  void commandW7(TicCommand cmd, uint8_t val);
  uint8_t commandR8(TicCommand cmd);
  void getSegment(TicCommand cmd, uint8_t offset,
    uint8_t length, void * buffer);

  void sendCommandHeader(TicCommand cmd);
  void serialW7(uint8_t val) { _stream->write(val & 0x7F); }
};

