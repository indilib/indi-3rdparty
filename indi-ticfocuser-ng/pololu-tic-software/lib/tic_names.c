// Functions for converting between names and numeric codes.

#include "tic_internal.h"

const tic_name tic_bool_names[] =
{
  { "true", 1 },
  { "false", 0 },
  { NULL, 0 },
};

const tic_name tic_product_names_short[] =
{
  { "T825", TIC_PRODUCT_T825 },
  { "T834", TIC_PRODUCT_T834 },
  { "T500", TIC_PRODUCT_T500 },
  { "N825", TIC_PRODUCT_N825 },
  { "T249", TIC_PRODUCT_T249 },
  { "36v4", TIC_PRODUCT_36V4 },
  { NULL, 0 },
};

const tic_name tic_product_names_ui[] =
{
  { "Tic T825 Stepper Motor Controller", TIC_PRODUCT_T825 },
  { "Tic T834 Stepper Motor Controller", TIC_PRODUCT_T834 },
  { "Tic T500 Stepper Motor Controller", TIC_PRODUCT_T500 },
  { "Tic N825 Stepper Motor Controller", TIC_PRODUCT_N825 },
  { "Tic T249 Stepper Motor Controller", TIC_PRODUCT_T249 },
  { "Tic 36v4 High-Power Stepper Motor Controller", TIC_PRODUCT_36V4 },
  { NULL, 0 },
};

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

const tic_name tic_decay_mode_names_generic_ui[] =
{
  { "Slow", TIC_DECAY_MODE_SLOW },
  { "Mixed", TIC_DECAY_MODE_MIXED },
  { "Fast", TIC_DECAY_MODE_FAST },
  { "Mode 3", 3 },
  { "Mode 4", 4 },
  { "Mode 5", 5 },
  { "Mode 6", 6 },
  { "Mode 7", 7 },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_t825_ui[] =
{
  { "Slow", TIC_DECAY_MODE_T825_SLOW },
  { "Mixed", TIC_DECAY_MODE_T825_MIXED },
  { "Fast", TIC_DECAY_MODE_T825_FAST },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_t834_ui[] =
{
  { "Slow", TIC_DECAY_MODE_T834_SLOW },
  { "Mixed 25%", TIC_DECAY_MODE_T834_MIXED25 },
  { "Mixed 50%", TIC_DECAY_MODE_T834_MIXED50 },
  { "Mixed 75%", TIC_DECAY_MODE_T834_MIXED75 },
  { "Fast", TIC_DECAY_MODE_T834_FAST },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_t500_ui[] =
{
  { "Auto", TIC_DECAY_MODE_T500_AUTO },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_t249_ui[] =
{
  { "Mixed", TIC_DECAY_MODE_T249_MIXED },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_generic_snake[] =
{
  { "mixed", TIC_DECAY_MODE_MIXED },
  { "slow", TIC_DECAY_MODE_SLOW },
  { "fast", TIC_DECAY_MODE_FAST },
  { "mode3", 3 },
  { "mode4", 4 },
  { "mode5", 5 },
  { "mode6", 6 },
  { "mode7", 7 },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_t825_snake[] =
{
  { "mixed", TIC_DECAY_MODE_T825_MIXED },
  { "slow", TIC_DECAY_MODE_T825_SLOW },
  { "fast", TIC_DECAY_MODE_T825_FAST },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_t834_snake[] =
{
  { "slow", TIC_DECAY_MODE_T834_SLOW },
  { "mixed25", TIC_DECAY_MODE_T834_MIXED25 },
  { "mixed50", TIC_DECAY_MODE_T834_MIXED50 },
  { "mixed75", TIC_DECAY_MODE_T834_MIXED75 },
  { "fast", TIC_DECAY_MODE_T834_FAST },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_t500_snake[] =
{
  { "auto", TIC_DECAY_MODE_T500_AUTO },
  { NULL, 0 },
};

const tic_name tic_decay_mode_names_t249_snake[] =
{
  { "mixed", TIC_DECAY_MODE_T249_MIXED },
  { NULL, 0 },
};

const tic_name tic_input_state_names_ui[] =
{
  { "Not ready", TIC_INPUT_STATE_NOT_READY },
  { "Invalid", TIC_INPUT_STATE_INVALID },
  { "Halt", TIC_INPUT_STATE_HALT },
  { "Position", TIC_INPUT_STATE_POSITION },
  { "Velocity", TIC_INPUT_STATE_VELOCITY },
  { NULL, 0 },
};

const tic_name tic_device_reset_names_ui[] =
{
  { "Power-on reset", TIC_RESET_POWER_UP },
  { "Brown-out reset", TIC_RESET_BROWNOUT },
  { "Reset pin driven low", TIC_RESET_RESET_LINE },
  { "Watchdog reset", TIC_RESET_WATCHDOG },
  { "Software reset (bootloader)", TIC_RESET_SOFTWARE },
  { "Stack overflow", TIC_RESET_STACK_OVERFLOW },
  { "Stack underflow", TIC_RESET_STACK_UNDERFLOW },
  { NULL, 0 },
};

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

const tic_name tic_step_mode_names[] =
{
  { "1", TIC_STEP_MODE_MICROSTEP1 },
  { "2", TIC_STEP_MODE_MICROSTEP2 },
  { "2_100p", TIC_STEP_MODE_MICROSTEP2_100P },
  { "4", TIC_STEP_MODE_MICROSTEP4 },
  { "8", TIC_STEP_MODE_MICROSTEP8 },
  { "16", TIC_STEP_MODE_MICROSTEP16 },
  { "32", TIC_STEP_MODE_MICROSTEP32 },
  { "64", TIC_STEP_MODE_MICROSTEP64 },
  { "128", TIC_STEP_MODE_MICROSTEP128 },
  { "256", TIC_STEP_MODE_MICROSTEP256 },
  { "full", TIC_STEP_MODE_FULL },
  { "half", TIC_STEP_MODE_HALF },
  { NULL, 0 },
};

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

const tic_name tic_pin_state_names_ui[] =
{
  { "High impedance", TIC_PIN_STATE_HIGH_IMPEDANCE },
  { "Pulled up", TIC_PIN_STATE_PULLED_UP },
  { "Output low", TIC_PIN_STATE_OUTPUT_LOW },
  { "Output high", TIC_PIN_STATE_OUTPUT_HIGH },
  { NULL, 0},
};

const tic_name tic_planning_mode_names_ui[] =
{
  { "Off", TIC_PLANNING_MODE_OFF },
  { "Target position", TIC_PLANNING_MODE_TARGET_POSITION },
  { "Target velocity", TIC_PLANNING_MODE_TARGET_VELOCITY },
  { NULL, 0 },
};

const tic_name tic_control_mode_names[] =
{
  { "serial", TIC_CONTROL_MODE_SERIAL },
  { "step_dir", TIC_CONTROL_MODE_STEP_DIR },
  { "rc_position", TIC_CONTROL_MODE_RC_POSITION },
  { "rc_speed", TIC_CONTROL_MODE_RC_SPEED },
  { "analog_position", TIC_CONTROL_MODE_ANALOG_POSITION },
  { "analog_speed", TIC_CONTROL_MODE_ANALOG_SPEED },
  { "encoder_position", TIC_CONTROL_MODE_ENCODER_POSITION },
  { "encoder_speed", TIC_CONTROL_MODE_ENCODER_SPEED },
  { NULL, 0 },
};

const tic_name tic_response_names[] =
{
  { "deenergize", TIC_RESPONSE_DEENERGIZE },
  { "halt_and_hold", TIC_RESPONSE_HALT_AND_HOLD },
  { "decel_to_hold", TIC_RESPONSE_DECEL_TO_HOLD },
  { "go_to_position", TIC_RESPONSE_GO_TO_POSITION },
  { NULL, 0},
};

const tic_name tic_scaling_degree_names[] =
{
  { "linear", TIC_SCALING_DEGREE_LINEAR },
  { "quadratic", TIC_SCALING_DEGREE_QUADRATIC },
  { "cubic", TIC_SCALING_DEGREE_CUBIC },
  { NULL, 0 },
};

const tic_name tic_pin_func_names[] =
{
  { "default", TIC_PIN_FUNC_DEFAULT },
  { "user_io", TIC_PIN_FUNC_USER_IO },
  { "user_input", TIC_PIN_FUNC_USER_INPUT },
  { "pot_power", TIC_PIN_FUNC_POT_POWER },
  { "serial", TIC_PIN_FUNC_SERIAL },
  { "rc", TIC_PIN_FUNC_RC },
  { "encoder", TIC_PIN_FUNC_ENCODER },
  { "kill_switch", TIC_PIN_FUNC_KILL_SWITCH },
  { "limit_switch_forward", TIC_PIN_FUNC_LIMIT_SWITCH_FORWARD },
  { "limit_switch_reverse", TIC_PIN_FUNC_LIMIT_SWITCH_REVERSE },
  { NULL, 0 },
};

const tic_name tic_agc_mode_names[] =
{
  { "off", TIC_AGC_MODE_OFF },
  { "on", TIC_AGC_MODE_ON },
  { "active_off", TIC_AGC_MODE_ACTIVE_OFF },
  { "false", TIC_AGC_MODE_OFF },  // for parsing YAML
  { "true", TIC_AGC_MODE_ON },    // for parsing YAML
  { NULL, 0 },
};

const tic_name tic_agc_mode_names_ui[] =
{
  { "Off", TIC_AGC_MODE_OFF },
  { "On", TIC_AGC_MODE_ON },
  { "Active off", TIC_AGC_MODE_ACTIVE_OFF },
  { NULL, 0 },
};

const tic_name tic_motor_driver_error_names_ui[] =
{
  { "None", TIC_MOTOR_DRIVER_ERROR_NONE },
  { "Overcurrent", TIC_MOTOR_DRIVER_ERROR_OVERCURRENT },
  { "Overtemperature", TIC_MOTOR_DRIVER_ERROR_OVERTEMPERATURE },
  { NULL, 0 },
};

const tic_name tic_agc_bottom_current_limit_names[] =
{
  { "45", TIC_AGC_BOTTOM_CURRENT_LIMIT_45 },
  { "50", TIC_AGC_BOTTOM_CURRENT_LIMIT_50 },
  { "55", TIC_AGC_BOTTOM_CURRENT_LIMIT_55 },
  { "60", TIC_AGC_BOTTOM_CURRENT_LIMIT_60 },
  { "65", TIC_AGC_BOTTOM_CURRENT_LIMIT_65 },
  { "70", TIC_AGC_BOTTOM_CURRENT_LIMIT_70 },
  { "75", TIC_AGC_BOTTOM_CURRENT_LIMIT_75 },
  { "80", TIC_AGC_BOTTOM_CURRENT_LIMIT_80 },
  { NULL, 0 },
};

const tic_name tic_agc_bottom_current_limit_names_ui[] =
{
  { "45%", TIC_AGC_BOTTOM_CURRENT_LIMIT_45 },
  { "50%", TIC_AGC_BOTTOM_CURRENT_LIMIT_50 },
  { "55%", TIC_AGC_BOTTOM_CURRENT_LIMIT_55 },
  { "60%", TIC_AGC_BOTTOM_CURRENT_LIMIT_60 },
  { "65%", TIC_AGC_BOTTOM_CURRENT_LIMIT_65 },
  { "70%", TIC_AGC_BOTTOM_CURRENT_LIMIT_70 },
  { "75%", TIC_AGC_BOTTOM_CURRENT_LIMIT_75 },
  { "80%", TIC_AGC_BOTTOM_CURRENT_LIMIT_80 },
  { NULL, 0 },
};

const tic_name tic_agc_current_boost_steps_names[] =
{
  { "5", TIC_AGC_CURRENT_BOOST_STEPS_5 },
  { "7", TIC_AGC_CURRENT_BOOST_STEPS_7 },
  { "9", TIC_AGC_CURRENT_BOOST_STEPS_9 },
  { "11", TIC_AGC_CURRENT_BOOST_STEPS_11 },
  { NULL, 0 },
};

const tic_name tic_agc_frequency_limit_names[] =
{
  { "off", TIC_AGC_FREQUENCY_LIMIT_OFF },
  { "225", TIC_AGC_FREQUENCY_LIMIT_225 },
  { "450", TIC_AGC_FREQUENCY_LIMIT_450 },
  { "675", TIC_AGC_FREQUENCY_LIMIT_675 },
  { "false", TIC_AGC_FREQUENCY_LIMIT_OFF },  // for parsing YAML
  { NULL, 0 },
};

const tic_name tic_agc_frequency_limit_names_ui[] =
{
  { "Off", TIC_AGC_FREQUENCY_LIMIT_OFF },
  { "225 Hz", TIC_AGC_FREQUENCY_LIMIT_225 },
  { "450 Hz", TIC_AGC_FREQUENCY_LIMIT_450 },
  { "675 Hz", TIC_AGC_FREQUENCY_LIMIT_675 },
  { NULL, 0 },
};

const tic_name tic_hp_decmod_names_snake[] =
{
  { "slow", TIC_HP_DECMOD_SLOW },
  { "slow_mixed", TIC_HP_DECMOD_SLOW_MIXED },
  { "fast", TIC_HP_DECMOD_FAST },
  { "mixed", TIC_HP_DECMOD_MIXED },
  { "slow_auto_mixed", TIC_HP_DECMOD_SLOW_AUTO_MIXED },
  { "auto_mixed", TIC_HP_DECMOD_AUTO_MIXED },
  { NULL, 0 },
};

const tic_name tic_hp_decmod_names_ui[] =
{
  { "Slow", TIC_HP_DECMOD_SLOW },
  { "Slow / mixed", TIC_HP_DECMOD_SLOW_MIXED },
  { "Fast", TIC_HP_DECMOD_FAST },
  { "Mixed", TIC_HP_DECMOD_MIXED },
  { "Slow / auto-mixed", TIC_HP_DECMOD_SLOW_AUTO_MIXED },
  { "Auto-mixed", TIC_HP_DECMOD_AUTO_MIXED },
  { NULL, 0 },
};

const tic_name tic_hp_driver_error_names_ui[] =
{
  { "None", 0 },
  { "Overtemperature", 1 << TIC_HP_DRIVER_ERROR_OTS },
  { "Overcurrent A", 1 << TIC_HP_DRIVER_ERROR_AOCP },
  { "Overcurrent B", 1 << TIC_HP_DRIVER_ERROR_BOCP },
  { "Predriver fault A", 1 << TIC_HP_DRIVER_ERROR_APDF },
  { "Predriver fault B", 1 << TIC_HP_DRIVER_ERROR_BPDF },
  { "Undervoltage", 1 << TIC_HP_DRIVER_ERROR_UVLO },
  { "Verification failure", 1 << TIC_HP_DRIVER_ERROR_VERIFY },
  { "Overcurrent", (1 << TIC_HP_DRIVER_ERROR_AOCP) | (1 << TIC_HP_DRIVER_ERROR_BOCP) },
  { "Predriver fault", (1 << TIC_HP_DRIVER_ERROR_APDF) | (1 << TIC_HP_DRIVER_ERROR_BPDF) },
  { NULL, 0 },
};

const char * tic_look_up_product_name_short(uint8_t product)
{
  const char * str = "";
  tic_code_to_name(tic_product_names_short, product, &str);
  return str;
}

const char * tic_look_up_product_name_ui(uint8_t product)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_product_names_ui, product, &str);
  return str;
}

const char * tic_look_up_error_name_ui(uint32_t error)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_error_names_ui, error, &str);
  return str;
}

const char * tic_look_up_decay_mode_name_ui(uint8_t decay_mode)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_decay_mode_names_generic_ui, decay_mode, &str);
  return str;
}

const char * tic_look_up_input_state_name_ui(uint8_t input_state)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_input_state_names_ui, input_state, &str);
  return str;
}

const char * tic_look_up_device_reset_name_ui(uint8_t device_reset)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_device_reset_names_ui, device_reset, &str);
  return str;
}

const char * tic_look_up_operation_state_name_ui(uint8_t operation_state)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_operation_state_names_ui, operation_state, &str);
  return str;
}

const char * tic_look_up_step_mode_name_ui(uint8_t step_mode)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_step_mode_names_ui, step_mode, &str);
  return str;
}

const char * tic_look_up_pin_state_name_ui(uint8_t pin_state)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_pin_state_names_ui, pin_state, &str);
  return str;
}

const char * tic_look_up_planning_mode_name_ui(uint8_t planning_mode)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_planning_mode_names_ui, planning_mode, &str);
  return str;
}

const char * tic_look_up_hp_decmod_name_ui(uint8_t mode)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_hp_decmod_names_ui, mode, &str);
  return str;
}

bool tic_look_up_decay_mode_name(uint8_t decay_mode,
  uint8_t product, uint32_t flags, const char ** name)
{
  const tic_name * name_table = NULL;

  if (flags & TIC_NAME_SNAKE_CASE)
  {
    if (name) { *name = ""; }

    switch (product)
    {
    case 0:
      name_table = tic_decay_mode_names_generic_snake;
      break;
    case TIC_PRODUCT_T825:
    case TIC_PRODUCT_N825:
      name_table = tic_decay_mode_names_t825_snake;
      break;
    case TIC_PRODUCT_T834:
      name_table = tic_decay_mode_names_t834_snake;
      break;
    case TIC_PRODUCT_T500:
      name_table = tic_decay_mode_names_t500_snake;
      break;
    case TIC_PRODUCT_T249:
      name_table = tic_decay_mode_names_t249_snake;
      break;
    }
  }
  else
  {
    if (name) { *name = "(Unknown)"; }

    switch (product)
    {
    case 0:
      name_table = tic_decay_mode_names_generic_ui;
      break;
    case TIC_PRODUCT_T825:
    case TIC_PRODUCT_N825:
      name_table = tic_decay_mode_names_t825_ui;
      break;
    case TIC_PRODUCT_T834:
      name_table = tic_decay_mode_names_t834_ui;
      break;
    case TIC_PRODUCT_T500:
      name_table = tic_decay_mode_names_t500_ui;
      break;
    case TIC_PRODUCT_T249:
      name_table = tic_decay_mode_names_t249_ui;
      break;
    }
  }

  return tic_code_to_name(name_table, decay_mode, name);
}

bool tic_look_up_decay_mode_code(const char * name,
  uint8_t product, uint32_t flags, uint8_t * code)
{
  if (code) { *code = 0; }

  if (!name) { return false; }

  uint32_t result;

  if (flags & TIC_NAME_SNAKE_CASE)
  {
    if (tic_name_to_code(tic_decay_mode_names_generic_snake, name, &result))
    {
      *code = result;
      return true;
    }

    if (!product || product == TIC_PRODUCT_T825 || product == TIC_PRODUCT_N825)
    {
      if (tic_name_to_code(tic_decay_mode_names_t825_snake, name, &result))
      {
        *code = result;
        return true;
      }
    }

    if (!product || product == TIC_PRODUCT_T834)
    {
      if (tic_name_to_code(tic_decay_mode_names_t834_snake, name, &result))
      {
        *code = result;
        return true;
      }
    }

    if (!product || product == TIC_PRODUCT_T500)
    {
      if (tic_name_to_code(tic_decay_mode_names_t500_snake, name, &result))
      {
        *code = result;
        return true;
      }
    }

    if (!product || product == TIC_PRODUCT_T249)
    {
      if (tic_name_to_code(tic_decay_mode_names_t249_snake, name, &result))
      {
        *code = result;
        return true;
      }
    }
  }

  if (flags & TIC_NAME_UI)
  {
    if (tic_name_to_code(tic_decay_mode_names_generic_ui, name, &result))
    {
      *code = result;
      return true;
    }

    if (!product || product == TIC_PRODUCT_T825 || product == TIC_PRODUCT_N825)
    {
      if (tic_name_to_code(tic_decay_mode_names_t825_ui, name, &result))
      {
        *code = result;
        return true;
      }
    }

    if (!product || product == TIC_PRODUCT_T834)
    {
      if (tic_name_to_code(tic_decay_mode_names_t834_ui, name, &result))
      {
        *code = result;
        return true;
      }
    }

    if (!product || product == TIC_PRODUCT_T500)
    {
      if (tic_name_to_code(tic_decay_mode_names_t500_ui, name, &result))
      {
        *code = result;
        return true;
      }
    }

    if (!product || product == TIC_PRODUCT_T249)
    {
      if (tic_name_to_code(tic_decay_mode_names_t249_ui, name, &result))
      {
        *code = result;
        return true;
      }
    }
  }

  return false;
}

const char * tic_look_up_motor_driver_error_name_ui(uint8_t error)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_motor_driver_error_names_ui, error, &str);
  return str;
}

const char * tic_look_up_agc_mode_name_ui(uint8_t error)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_agc_mode_names_ui, error, &str);
  return str;
}

const char * tic_look_up_agc_bottom_current_limit_name_ui(uint8_t limit)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_agc_bottom_current_limit_names_ui, limit, &str);
  return str;
}

const char * tic_look_up_agc_current_boost_steps_name_ui(uint8_t steps)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_agc_current_boost_steps_names, steps, &str);
  return str;
}

const char * tic_look_up_agc_frequency_limit_name_ui(uint8_t limit)
{
  const char * str = "(Unknown)";
  tic_code_to_name(tic_agc_frequency_limit_names_ui, limit, &str);
  return str;
}

const char * tic_look_up_hp_driver_error_name_ui(uint8_t error)
{
  const char * str;
  if (error & 0b10111111)
  {
    // The error code consists solely of bits that correspond to valid errors.
    // If tic_code_to_name cannot find a name, it means there were
    // multiple valid error bits set.
    str = "(Multiple)";
  }
  else
  {
    str = "(Unknown)";
  }
  tic_code_to_name(tic_hp_driver_error_names_ui, error, &str);
  return str;
}

bool tic_name_to_code(const tic_name * table, const char * name, uint32_t * code)
{
  if (code) { *code = 0; }

  if (!table) { return false; }
  if (!name) { return false; }

  for (const tic_name * p = table; p->name; p++)
  {
    if (!strcmp(p->name, name))
    {
      if (code) { *code = p->code; }
      return true;
    }
  }

  return false;
}

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
