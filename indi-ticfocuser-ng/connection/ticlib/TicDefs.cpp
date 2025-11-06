// Copyright (C) Pololu Corporation.  See www.pololu.com for details.

#include "TicDefs.h"
#include "TicBase.h"

// copied from pololu-tic-software/include/tic_protocol.h and modified to use TicBase.h
#define TIC_ERROR_INTENTIONALLY_DEENERGIZED       (int)TicError::IntentionallyDeenergized
#define TIC_ERROR_MOTOR_DRIVER_ERROR              (int)TicError::MotorDriverError
#define TIC_ERROR_LOW_VIN                         (int)TicError::LowVin
#define TIC_ERROR_KILL_SWITCH                     (int)TicError::KillSwitch
#define TIC_ERROR_REQUIRED_INPUT_INVALID          (int)TicError::RequiredInputInvalid
#define TIC_ERROR_SERIAL_ERROR                    (int)TicError::SerialError
#define TIC_ERROR_COMMAND_TIMEOUT                 (int)TicError::CommandTimeout
#define TIC_ERROR_SAFE_START_VIOLATION            (int)TicError::SafeStartViolation
#define TIC_ERROR_ERR_LINE_HIGH                   (int)TicError::ErrLineHigh
#define TIC_ERROR_SERIAL_FRAMING                  (int)TicError::SerialFraming
#define TIC_ERROR_SERIAL_RX_OVERRUN               (int)TicError::RxOverrun
#define TIC_ERROR_SERIAL_FORMAT                   (int)TicError::Format
#define TIC_ERROR_SERIAL_CRC                      (int)TicError::Crc
#define TIC_ERROR_ENCODER_SKIP                    (int)TicError::EncoderSkip

#define TIC_STEP_MODE_MICROSTEP1                  (int)TicStepMode::Microstep1
#define TIC_STEP_MODE_MICROSTEP2                  (int)TicStepMode::Microstep2
#define TIC_STEP_MODE_MICROSTEP4                  (int)TicStepMode::Microstep4
#define TIC_STEP_MODE_MICROSTEP8                  (int)TicStepMode::Microstep8
#define TIC_STEP_MODE_MICROSTEP16                 (int)TicStepMode::Microstep16
#define TIC_STEP_MODE_MICROSTEP32                 (int)TicStepMode::Microstep32
#define TIC_STEP_MODE_MICROSTEP2_100P             (int)TicStepMode::Microstep2_100p
#define TIC_STEP_MODE_MICROSTEP64                 (int)TicStepMode::Microstep64
#define TIC_STEP_MODE_MICROSTEP128                (int)TicStepMode::Microstep128
#define TIC_STEP_MODE_MICROSTEP256                (int)TicStepMode::Microstep256

#define TIC_OPERATION_STATE_RESET                 (int)TicOperationState::Reset
#define TIC_OPERATION_STATE_DEENERGIZED           (int)TicOperationState::Deenergized
#define TIC_OPERATION_STATE_SOFT_ERROR            (int)TicOperationState::SoftError
#define TIC_OPERATION_STATE_WAITING_FOR_ERR_LINE  (int)TicOperationState::WaitingForErrLine
#define TIC_OPERATION_STATE_STARTING_UP           (int)TicOperationState::StartingUp
#define TIC_OPERATION_STATE_NORMAL                (int)TicOperationState::Normal

// copied from pololu-tic-software/lib/tic_names.c
const tic_name tic_error_names_ui[] =
{
  { "Intentionally de-energized", 1 << TIC_ERROR_INTENTIONALLY_DEENERGIZED },
  { "Motor driver error", 1 << TIC_ERROR_MOTOR_DRIVER_ERROR },
  { "Low VIN", 1 << TIC_ERROR_LOW_VIN },
  { "Kill switch active", 1 << TIC_ERROR_KILL_SWITCH },
  { "Required input invalid", 1 << TIC_ERROR_REQUIRED_INPUT_INVALID },
  { "Serial error", 1 << TIC_ERROR_SERIAL_ERROR },
  { "Command timeout", 1 << TIC_ERROR_COMMAND_TIMEOUT },
  { "Safe start violation", 1 << TIC_ERROR_SAFE_START_VIOLATION },
  { "ERR line high", 1 << TIC_ERROR_ERR_LINE_HIGH },
  { "Serial framing", 1 << TIC_ERROR_SERIAL_FRAMING },
  { "Serial RX overrun", 1 << TIC_ERROR_SERIAL_RX_OVERRUN },
  { "Serial format", 1 << TIC_ERROR_SERIAL_FORMAT },
  { "Serial CRC", 1 << TIC_ERROR_SERIAL_CRC },
  { "Encoder skip", 1 << TIC_ERROR_ENCODER_SKIP },
  { NULL, 0 },
};

// added
const size_t tic_error_names_ui_size = sizeof(tic_error_names_ui)/sizeof(tic_error_names_ui[0]) - 1;

// copied from pololu-tic-software/lib/tic_names.c
const tic_name tic_step_mode_names_ui[] =
{
  { "Full step", TIC_STEP_MODE_MICROSTEP1 },
  { "1/2 step", TIC_STEP_MODE_MICROSTEP2 },
  { "1/2 step 100%", TIC_STEP_MODE_MICROSTEP2_100P },
  { "1/4 step", TIC_STEP_MODE_MICROSTEP4 },
  { "1/8 step", TIC_STEP_MODE_MICROSTEP8 },
  { "1/16 step", TIC_STEP_MODE_MICROSTEP16 },
  { "1/32 step", TIC_STEP_MODE_MICROSTEP32 },
  { "1/64 step", TIC_STEP_MODE_MICROSTEP64 },
  { "1/128 step", TIC_STEP_MODE_MICROSTEP128 },
  { "1/256 step", TIC_STEP_MODE_MICROSTEP256 },
  { NULL, 0 },
};

// copied from pololu-tic-software/lib/tic_names.c
const tic_name tic_operation_state_names_ui[] =
{
  { "Reset", TIC_OPERATION_STATE_RESET },
  { "De-energized", TIC_OPERATION_STATE_DEENERGIZED },
  { "Soft error", TIC_OPERATION_STATE_SOFT_ERROR },
  { "Waiting for ERR line", TIC_OPERATION_STATE_WAITING_FOR_ERR_LINE },
  { "Starting up", TIC_OPERATION_STATE_STARTING_UP },
  { "Normal", TIC_OPERATION_STATE_NORMAL },
  { NULL, 0 },
};

// copied from pololu-tic-software/lib/tic_names.c
bool tic_code_to_name(const tic_name * table, uint32_t code, const char ** name)
{
  if (!table) { return false; }

  for (const tic_name * p = table; p->name; p++)
  {
    if (p->code == code)
    {
      if (name) { *name = p->name; }
      return true;
    }
  }

  return false;
}

// copied from pololu-tic-software/lib/tic_names.c
const char * tic_look_up_operation_state_name_ui(uint8_t operation_state)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_operation_state_names_ui, operation_state, &str);
  return str;
}

// copied from pololu-tic-software/lib/tic_names.c
const char * tic_look_up_step_mode_name_ui(uint8_t step_mode)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_step_mode_names_ui, step_mode, &str);
  return str;
}

