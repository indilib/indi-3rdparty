// Functions for fixing settings to be valid.

#include "tic_internal.h"

static bool enum_is_valid(uint8_t code, uint8_t * valid_codes, uint8_t code_count)
{
  for (uint32_t i = 0; i < code_count; i++)
  {
    if (code == valid_codes[i]) { return true; }
  }
  return false;
}

// Fixes enumerated values to be valid (e.g. control_mode).
//
// These enumerated values could only be wrong if buggy software was used to
// write to the Tic's EEPROM.
//
// Note that for boolean settings like ignore_err_line_high, the tic_settings
// object cannot even hold invalid values, so there is no need to check them
// here.  If the EEPROM has invalid boolean values, they got corrected by
// tic_get_settings, which knows how the firmware treats booleans.
static void tic_settings_fix_enums(tic_settings * settings, tic_string * warnings)
{
  uint8_t product = tic_settings_get_product(settings);

  {
    uint8_t control_mode = tic_settings_get_control_mode(settings);
    if (control_mode > TIC_CONTROL_MODE_ENCODER_SPEED)
    {
      control_mode = TIC_CONTROL_MODE_SERIAL;
      tic_sprintf(warnings,
        "Warning: The control mode was invalid "
        "so it will be changed to Serial/I2C/USB.\n");
    }
    tic_settings_set_control_mode(settings, control_mode);
  }

  {
    uint8_t response = tic_settings_get_soft_error_response(settings);
    if (response > TIC_RESPONSE_GO_TO_POSITION)
    {
      response = TIC_RESPONSE_DECEL_TO_HOLD;
      tic_sprintf(warnings,
        "Warning: The soft error response was invalid "
        "so it will be changed to \"Decelerate to hold\".\n");
    }
    tic_settings_set_soft_error_response(settings, response);
  }

  {
    uint8_t scaling_degree = tic_settings_get_input_scaling_degree(settings);
    if (scaling_degree > TIC_SCALING_DEGREE_CUBIC)
    {
      scaling_degree = TIC_SCALING_DEGREE_LINEAR;
      tic_sprintf(warnings,
        "Warning: The scaling degree was invalid "
        "so it will be changed to linear.\n");
    }
    tic_settings_set_input_scaling_degree(settings, scaling_degree);
  }

  {
    uint8_t mode = tic_settings_get_step_mode(settings);

    uint8_t valid_step_modes[32];
    uint8_t * p = valid_step_modes;

    *p++ = TIC_STEP_MODE_MICROSTEP1;
    *p++ = TIC_STEP_MODE_MICROSTEP2;
    *p++ = TIC_STEP_MODE_MICROSTEP4;
    *p++ = TIC_STEP_MODE_MICROSTEP8;

    switch (product)
    {
    case TIC_PRODUCT_T825:
    case TIC_PRODUCT_N825:
    case TIC_PRODUCT_T834:
      *p++ = TIC_STEP_MODE_MICROSTEP16;
      *p++ = TIC_STEP_MODE_MICROSTEP32;
      break;
    case TIC_PRODUCT_T249:
      *p++ = TIC_STEP_MODE_MICROSTEP16;
      *p++ = TIC_STEP_MODE_MICROSTEP32;
      *p++ = TIC_STEP_MODE_MICROSTEP2_100P;
      break;
    case TIC_PRODUCT_36V4:
      *p++ = TIC_STEP_MODE_MICROSTEP16;
      *p++ = TIC_STEP_MODE_MICROSTEP32;
      *p++ = TIC_STEP_MODE_MICROSTEP64;
      *p++ = TIC_STEP_MODE_MICROSTEP128;
      *p++ = TIC_STEP_MODE_MICROSTEP256;
      break;
    }

    if (!enum_is_valid(mode, valid_step_modes, p - valid_step_modes))
    {
      mode = TIC_STEP_MODE_MICROSTEP1;
      tic_sprintf(warnings,
        "Warning: The step mode is invalid "
        "so it will be changed to 1 (full step).\n");
    }

    tic_settings_set_step_mode(settings, mode);
  }

  {
    uint8_t mode = tic_settings_get_decay_mode(settings);

    // The name tables can tell us whether the selected decay mode is valid.
    //
    // If it was a valid decay mode for some other product, just silently change
    // it to 0, like the firmware.  If it was a totally invalid decay mode,
    // print a warning.
    if (!tic_look_up_decay_mode_name(mode, product, 0, NULL))
    {
      if (!tic_look_up_decay_mode_name(mode, 0, 0, NULL))
      {
        tic_sprintf(warnings,
          "Warning: The decay mode is invalid "
          "so it will be changed to the default.\n");
      }
      mode = TIC_DECAY_MODE_MIXED;
    }
    tic_settings_set_decay_mode(settings, mode);
  }

  {
    uint8_t mode = tic_settings_get_agc_mode(settings);
    if (product == TIC_PRODUCT_T249)
    {
      if (!tic_code_to_name(tic_agc_mode_names, mode, NULL))
      {
        tic_sprintf(warnings,
          "Warning: The AGC mode was invalid "
          "so it will be changed to on.\n");
        mode = TIC_AGC_MODE_ON;
      }
    }
    else
    {
      mode = 0;
    }
    tic_settings_set_agc_mode(settings, mode);
  }

  {
    uint8_t limit = tic_settings_get_agc_bottom_current_limit(settings);
    if (product == TIC_PRODUCT_T249)
    {
      if (!tic_code_to_name(tic_agc_bottom_current_limit_names, limit, NULL))
      {
        tic_sprintf(warnings,
          "Warning: The AGC bottom current limit was invalid "
          "so it will be changed to 75%%.\n");
        limit = TIC_AGC_BOTTOM_CURRENT_LIMIT_75;
      }
    }
    else
    {
      limit = 0;
    }
    tic_settings_set_agc_bottom_current_limit(settings, limit);
  }

  {
    uint8_t steps = tic_settings_get_agc_current_boost_steps(settings);
    if (product == TIC_PRODUCT_T249)
    {
      if (!tic_code_to_name(tic_agc_current_boost_steps_names, steps, NULL))
      {
        tic_sprintf(warnings,
          "Warning: The AGC current boost steps setting was invalid "
          "so it will be changed to 5.\n");
        steps = TIC_AGC_CURRENT_BOOST_STEPS_5;
      }
    }
    else
    {
      steps = 0;
    }
    tic_settings_set_agc_current_boost_steps(settings, steps);
  }

  {
    uint8_t limit = tic_settings_get_agc_frequency_limit(settings);
    if (product == TIC_PRODUCT_T249)
    {
      if (!tic_code_to_name(tic_agc_frequency_limit_names, limit, NULL))
      {
        tic_sprintf(warnings,
          "Warning: The AGC frequency limit was invalid "
          "so it will be changed to off.\n");
        limit = TIC_AGC_FREQUENCY_LIMIT_OFF;
      }
    }
    else
    {
      limit = 0;
    }
    tic_settings_set_agc_frequency_limit(settings, limit);
  }

  {
    uint8_t mode = tic_settings_get_hp_decmod(settings);
    if (product == TIC_PRODUCT_36V4)
    {
      if (!tic_code_to_name(tic_hp_decmod_names_snake, mode, NULL))
      {
        tic_sprintf(warnings,
          "Warning: The decay mode was invalid "
          "so it will be changed to \"Slow / mixed\".\n");
        mode = TIC_HP_DECMOD_SLOW_MIXED;
      }
    }
    else
    {
      mode = 0;
    }
    tic_settings_set_hp_decmod(settings, mode);
  }
}

static bool is_limit_switch(uint8_t pin_func)
{
  return pin_func == TIC_PIN_FUNC_LIMIT_SWITCH_FORWARD ||
    pin_func == TIC_PIN_FUNC_LIMIT_SWITCH_REVERSE;
}

static void tic_settings_fix_core(tic_settings * settings, tic_string * warnings)
{
  tic_settings_fix_enums(settings, warnings);

  uint8_t product = tic_settings_get_product(settings);
  uint16_t firmware_version = tic_settings_get_firmware_version(settings);

  uint8_t control_mode = tic_settings_get_control_mode(settings);

  bool speed_control_mode = false;

  switch (control_mode)
  {
  case TIC_CONTROL_MODE_RC_SPEED:
  case TIC_CONTROL_MODE_ANALOG_SPEED:
  case TIC_CONTROL_MODE_ENCODER_SPEED:
    speed_control_mode = true;
    break;
  }

  bool analog_control_mode = false;

  switch (control_mode)
  {
  case TIC_CONTROL_MODE_ANALOG_POSITION:
  case TIC_CONTROL_MODE_ANALOG_SPEED:
    analog_control_mode = true;
    break;
  }

  {
    uint8_t response = tic_settings_get_soft_error_response(settings);
    if (response == TIC_RESPONSE_GO_TO_POSITION && speed_control_mode)
    {
      response = TIC_RESPONSE_DECEL_TO_HOLD;
      tic_sprintf(warnings,
        "Warning: The soft error response cannot be \"Go to position\" in a "
        "speed control mode, so it will be changed to \"Decelerate to hold\".\n");
    }
    tic_settings_set_soft_error_response(settings, response);
  }

  {
    uint32_t baud = tic_settings_get_serial_baud_rate(settings);
    if (baud < TIC_MIN_ALLOWED_BAUD_RATE)
    {
      baud = TIC_MIN_ALLOWED_BAUD_RATE;
      tic_sprintf(warnings,
        "Warning: The serial baud rate is too low "
        "so it will be changed to %u.\n", baud);
    }
    if (baud > TIC_MAX_ALLOWED_BAUD_RATE)
    {
      baud = TIC_MAX_ALLOWED_BAUD_RATE;
      tic_sprintf(warnings,
        "Warning: The serial baud rate is too high "
        "so it will be changed to %u.\n",
        baud);
    }

    baud = tic_settings_achievable_serial_baud_rate(settings, baud);
    tic_settings_set_serial_baud_rate(settings, baud);
  }

  {
    uint16_t device_number =
      tic_settings_get_serial_device_number_u16(settings);
    uint16_t alt_device_number =
      tic_settings_get_serial_alt_device_number(settings);
    bool enable_14bit = tic_settings_get_serial_14bit_device_number(settings);
    bool enable_alt = tic_settings_get_serial_enable_alt_device_number(settings);

    if (enable_14bit && firmware_version && firmware_version < 0x0105)
    {
      enable_14bit = false;
      tic_sprintf(warnings,
        "Warning: The firmware version on your device does not support "
        "14-bit device numbers, so that option will be disabled.  "
        "See " DOCUMENTATION_URL " for firmware upgrade instructions.\n");
    }

    if (enable_alt && firmware_version && firmware_version < 0x0105)
    {
      enable_alt = false;
      tic_sprintf(warnings,
        "Warning: The firmware version on your device does not support "
        "the alternative device number, so it will be disabled.  "
        "See " DOCUMENTATION_URL " for firmware upgrade instructions.\n");
    }

    uint16_t mask = 0x7F;
    if (enable_14bit)
    {
      mask = 0x3FFF;
    }

    if (device_number > mask)
    {
      device_number &= mask;
      tic_sprintf(warnings,
        "Warning: The device number is higher than %u "
        "so it will be changed to %u.\n", mask, device_number);
    }

    if (alt_device_number > mask)
    {
      alt_device_number &= mask;
      tic_sprintf(warnings,
        "Warning: The alternative device number is higher than %u "
        "so it will be changed to %u.\n", mask, alt_device_number);
    }

    tic_settings_set_serial_device_number_u16(settings, device_number);
    tic_settings_set_serial_alt_device_number(settings, alt_device_number);
    tic_settings_set_serial_14bit_device_number(settings, enable_14bit);
    tic_settings_set_serial_enable_alt_device_number(settings, enable_alt);
  }

  {
    uint16_t command_timeout = tic_settings_get_command_timeout(settings);
    if (command_timeout > TIC_MAX_ALLOWED_COMMAND_TIMEOUT)
    {
      command_timeout = TIC_MAX_ALLOWED_COMMAND_TIMEOUT;
      tic_sprintf(warnings,
        "Warning: The command timeout is too high "
        "so it will be changed to %u ms.\n", command_timeout);
    }
    tic_settings_set_command_timeout(settings, command_timeout);
  }

  {
    bool enabled = tic_settings_get_serial_crc_for_responses(settings);
    if (enabled && firmware_version && firmware_version < 0x0105)
    {
      enabled = false;
      tic_sprintf(warnings,
        "Warning: The firmware version on your device does not support "
        "CRC for serial responses, so that option will be disabled.  "
        "See " DOCUMENTATION_URL " for firmware upgrade instructions.\n");
    }
    tic_settings_set_serial_crc_for_responses(settings, enabled);
  }

  {
    bool enabled = tic_settings_get_serial_7bit_responses(settings);
    if (enabled && firmware_version && firmware_version < 0x0105)
    {
      enabled = false;
      tic_sprintf(warnings,
        "Warning: The firmware version on your device does not support "
        "7-bit serial responses, so that option will be disabled.  "
        "See " DOCUMENTATION_URL " for firmware upgrade instructions.\n");
    }
    tic_settings_set_serial_7bit_responses(settings, enabled);
  }

  {
    uint16_t low_shutoff = tic_settings_get_low_vin_shutoff_voltage(settings);
    uint16_t low_startup = tic_settings_get_low_vin_startup_voltage(settings);
    uint16_t high_shutoff = tic_settings_get_high_vin_shutoff_voltage(settings);

    // Move low_shutoff down a little bit to prevent overflows below.
    if (low_shutoff > 64000)
    {
      low_shutoff = 64000;
      tic_sprintf(warnings,
        "Warning: The low VIN shutoff voltage will be changed to %u mV.\n",
        low_shutoff);
    }

    if (low_startup < low_shutoff)
    {
      low_startup = low_shutoff + 500;
      tic_sprintf(warnings,
        "Warning: The low VIN startup voltage will be changed to %u mV.\n",
        low_startup);
    }

    if (high_shutoff < low_startup)
    {
      high_shutoff = low_startup + 500;
      tic_sprintf(warnings,
        "Warning: The high VIN shutoff voltage will be changed to %u mV.\n",
        high_shutoff);
    }

    tic_settings_set_low_vin_shutoff_voltage(settings, low_shutoff);
    tic_settings_set_low_vin_startup_voltage(settings, low_startup);
    tic_settings_set_high_vin_shutoff_voltage(settings, high_shutoff);
  }

  {
    int16_t calibration = tic_settings_get_vin_calibration(settings);

    if (calibration < -500)
    {
      calibration = -500;
      tic_sprintf(warnings,
        "Warning: The VIN calibration is too low "
        "so it will be raised to -500.\n");
    }

    if (calibration > 500)
    {
      calibration = 500;
      tic_sprintf(warnings,
        "Warning: The VIN calibration is too high "
        "so it will be lowered to 500.\n");
    }

    tic_settings_set_vin_calibration(settings, calibration);
  }

  {
    uint16_t min = tic_settings_get_input_min(settings);
    uint16_t neutral_min = tic_settings_get_input_neutral_min(settings);
    uint16_t neutral_max = tic_settings_get_input_neutral_max(settings);
    uint16_t max = tic_settings_get_input_max(settings);

    if (min > neutral_min || neutral_min > neutral_max || neutral_max > max)
    {
      min = 0;
      neutral_min = 2015;
      neutral_max = 2080;
      max = 4095;

      tic_sprintf(warnings,
        "Warning: The input scaling values are out of order "
        "so they will be reset to their default values.\n");
    }

    if (min > 4095)
    {
      tic_sprintf(warnings,
        "Warning: The input minimum is too high "
        "so it will be lowered to 4095.\n");
      min = 4095;
    }
    if (neutral_min > 4095)
    {
      tic_sprintf(warnings,
        "Warning: The input neutral min is too high "
        "so it will be lowered to 4095.\n");
      neutral_min = 4095;
    }
    if (neutral_max > 4095)
    {
      tic_sprintf(warnings,
        "Warning: The input neutral max is too high "
        "so it will be lowered to 4095.\n");
      neutral_max = 4095;
    }
    if (max > 4095)
    {
      tic_sprintf(warnings,
        "Warning: The input maximum is too high "
        "so it will be lowered to 4095.\n");
      max = 4095;
    }

    tic_settings_set_input_min(settings, min);
    tic_settings_set_input_neutral_min(settings, neutral_min);
    tic_settings_set_input_neutral_max(settings, neutral_max);
    tic_settings_set_input_max(settings, max);
  }

  {
    int32_t output_min = tic_settings_get_output_min(settings);
    if (output_min > 0)
    {
      output_min = 0;
      tic_sprintf(warnings,
        "Warning: The scaling output minimum is above 0 "
        "so it will be lowered to 0.\n");
    }
    tic_settings_set_output_min(settings, output_min);
  }

  {
    int32_t output_max = tic_settings_get_output_max(settings);
    if (output_max < 0)
    {
      output_max = 0;
      tic_sprintf(warnings,
        "Warning: The scaling output maximum is below 0 "
        "so it will be raised to 0.\n");
    }
    tic_settings_set_output_max(settings, output_max);
  }

  {
    uint32_t prescaler = tic_settings_get_encoder_prescaler(settings);

    if (prescaler > TIC_MAX_ALLOWED_ENCODER_PRESCALER)
    {
      prescaler = TIC_MAX_ALLOWED_ENCODER_PRESCALER;
      tic_sprintf(warnings,
        "Warning: The encoder prescaler is too high "
        "so it will be lowered to %u.\n", prescaler);
    }

    if (prescaler < 1)
    {
      prescaler = 1;
      tic_sprintf(warnings,
        "Warning: The encoder prescaler is zero "
        "so it will be changed to 1.\n");
    }

    tic_settings_set_encoder_prescaler(settings, prescaler);
  }

  {
    uint32_t postscaler = tic_settings_get_encoder_postscaler(settings);

    if (postscaler > TIC_MAX_ALLOWED_ENCODER_POSTSCALER)
    {
      postscaler = TIC_MAX_ALLOWED_ENCODER_POSTSCALER;
      tic_sprintf(warnings,
        "Warning: The encoder postscaler is too high "
        "so it will be lowered to %u.\n", postscaler);
    }

    if (postscaler < 1)
    {
      postscaler = 1;
      tic_sprintf(warnings,
        "Warning: The encoder postscaler is zero "
        "so it will be changed to 1.\n");
    }

    tic_settings_set_encoder_postscaler(settings, postscaler);
  }

  {
    uint32_t current = tic_settings_get_current_limit(settings);
    uint32_t max_current = tic_get_max_allowed_current(product);
    if (product == TIC_PRODUCT_36V4 &&
      !tic_settings_get_hp_enable_unrestricted_current_limits(settings))
    {
      max_current = 3939;
    }

    if (current > max_current)
    {
      current = max_current;
      tic_sprintf(warnings,
        "Warning: The current limit is too high "
        "so it will be lowered to %u mA.\n", current);
    }

    current = tic_settings_achievable_current_limit(settings, current);
    tic_settings_set_current_limit(settings, current);

    int32_t current_during_error =
      tic_settings_get_current_limit_during_error(settings);

    if (current_during_error > (int32_t)current)
    {
      current_during_error = -1;
      tic_sprintf(warnings,
        "Warning: The current limit during error is higher than "
        "the default current limit so it will be changed to be the same.\n");
    }

    if (current_during_error < -1)
    {
      current_during_error = -1;
      tic_sprintf(warnings,
        "Warning: The current limit during error is an invalid negative number "
        "so it will be changed to be the same as the default current limit.\n");
    }

    if (current_during_error >= 0)
    {
      current_during_error = tic_settings_achievable_current_limit(
        settings, current_during_error);
    }
    tic_settings_set_current_limit_during_error(settings, current_during_error);
  }

  if (firmware_version && firmware_version < 0x0106 &&
    tic_settings_get_auto_homing(settings))
  {
    tic_settings_set_auto_homing(settings, false);
    tic_sprintf(warnings,
      "Warning: The firmware version on your device does not support"
      "auto homing (or homing in general), so it will be disabled.");

    // Note: It would also be nice to check that the user has enabled proper
    // limit switches and disable auto homing if needed.
  }

  {
    uint32_t max_speed = tic_settings_get_max_speed(settings);
    uint32_t starting_speed = tic_settings_get_starting_speed(settings);
    uint32_t homing_speed_towards = tic_settings_get_homing_speed_towards(settings);
    uint32_t homing_speed_away = tic_settings_get_homing_speed_away(settings);

    if (max_speed > TIC_MAX_ALLOWED_SPEED)
    {
      max_speed = TIC_MAX_ALLOWED_SPEED;
      uint32_t max_speed_khz = max_speed / TIC_SPEED_UNITS_PER_HZ / 1000;
      tic_sprintf(warnings,
        "Warning: The maximum speed is too high "
        "so it will be lowered to %u (%u kHz).\n",
        max_speed, max_speed_khz);
    }

    if (starting_speed > max_speed)
    {
      starting_speed = max_speed;
      tic_sprintf(warnings,
        "Warning: The starting speed is greater than the maximum speed "
        "so it will be lowered to %u.\n", starting_speed);
    }

    // We'll let the homing speed be higher than the max speed because I think
    // people will be annoyed if they are playing around with low max_speeds and
    // they get warnings about the homing speeds, which they probably do not
    // care about.

    if (homing_speed_towards > TIC_MAX_ALLOWED_SPEED)
    {
      homing_speed_towards = TIC_MAX_ALLOWED_SPEED;
      tic_sprintf(warnings,
        "Warning: The homing speed towards is too high "
        "so it will be lowered to %u.\n", homing_speed_towards);
    }

    if (homing_speed_away > TIC_MAX_ALLOWED_SPEED)
    {
      homing_speed_away = TIC_MAX_ALLOWED_SPEED;
      tic_sprintf(warnings,
        "Warning: The homing speed away is too high "
        "so it will be lowered to %u.\n", homing_speed_away);
    }

    tic_settings_set_max_speed(settings, max_speed);
    tic_settings_set_starting_speed(settings, starting_speed);
    tic_settings_set_homing_speed_towards(settings, homing_speed_towards);
    tic_settings_set_homing_speed_away(settings, homing_speed_away);
  }

  {
    uint32_t max_accel = tic_settings_get_max_accel(settings);

    if (max_accel > TIC_MAX_ALLOWED_ACCEL)
    {
      max_accel = TIC_MAX_ALLOWED_ACCEL;
      tic_sprintf(warnings,
        "Warning: The maximum acceleration is too high "
        "so it will be lowered to %u.\n", max_accel);
    }

    if (max_accel < TIC_MIN_ALLOWED_ACCEL)
    {
      max_accel = TIC_MIN_ALLOWED_ACCEL;
      tic_sprintf(warnings,
        "Warning: The maximum acceleration is too low "
        "so it will be raised to %u.\n", max_accel);
    }

    tic_settings_set_max_accel(settings, max_accel);
  }

  {
    uint32_t max_decel = tic_settings_get_max_decel(settings);

    if (max_decel > TIC_MAX_ALLOWED_ACCEL)
    {
      max_decel = TIC_MAX_ALLOWED_ACCEL;
      tic_sprintf(warnings,
        "Warning: The maximum deceleration is too high "
        "so it will be lowered to %u.\n", max_decel);
    }

    if (max_decel != 0 && max_decel < TIC_MIN_ALLOWED_ACCEL)
    {
      max_decel = TIC_MIN_ALLOWED_ACCEL;
      tic_sprintf(warnings,
        "Warning: The maximum deceleration is too low "
        "so it will be raised to %u.\n", max_decel);
    }

    tic_settings_set_max_decel(settings, max_decel);
  }

  {
    uint8_t scl_func = tic_settings_get_pin_func(settings, TIC_PIN_NUM_SCL);
    uint8_t sda_func = tic_settings_get_pin_func(settings, TIC_PIN_NUM_SDA);
    uint8_t tx_func = tic_settings_get_pin_func(settings, TIC_PIN_NUM_TX);
    uint8_t rx_func = tic_settings_get_pin_func(settings, TIC_PIN_NUM_RX);
    uint8_t rc_func = tic_settings_get_pin_func(settings, TIC_PIN_NUM_RC);
    bool rc_analog = tic_settings_get_pin_analog(settings, TIC_PIN_NUM_RC);

    // First, we make sure the pins are configured to provide the primary
    // input that will be used to control the motor.

    switch (control_mode)
    {
    case TIC_CONTROL_MODE_ANALOG_POSITION:
    case TIC_CONTROL_MODE_ANALOG_SPEED:
      if (sda_func != TIC_PIN_FUNC_DEFAULT &&
        sda_func != TIC_PIN_FUNC_USER_INPUT)
      {
        sda_func = TIC_PIN_FUNC_DEFAULT;
        tic_sprintf(warnings,
          "Warning: The SDA pin must be used as an analog input "
          "so its function will be changed to the default.\n");
      }
      break;
    case TIC_CONTROL_MODE_RC_POSITION:
    case TIC_CONTROL_MODE_RC_SPEED:
      // Skip this warning for the N825, because the RC pin function
      // will be set to its default later and that part gives a better
      // error message.
      if (rc_func != TIC_PIN_FUNC_DEFAULT &&
        rc_func != TIC_PIN_FUNC_RC && product != TIC_PRODUCT_N825)
      {
        rc_func = TIC_PIN_FUNC_DEFAULT;
        tic_sprintf(warnings,
          "Warning: The RC pin must be used as an RC input "
          "so its function will be changed to the default.\n");
      }
      break;
    case TIC_CONTROL_MODE_ENCODER_POSITION:
    case TIC_CONTROL_MODE_ENCODER_SPEED:
      if (tx_func != TIC_PIN_FUNC_DEFAULT &&
        tx_func != TIC_PIN_FUNC_ENCODER)
      {
        tx_func = TIC_PIN_FUNC_DEFAULT;
        tic_sprintf(warnings,
          "Warning: The TX pin must be used as an encoder input "
          "so its function will be changed to the default.\n");
      }
      if (rx_func != TIC_PIN_FUNC_DEFAULT &&
        rx_func != TIC_PIN_FUNC_ENCODER)
      {
        rx_func = TIC_PIN_FUNC_DEFAULT;
        tic_sprintf(warnings,
          "Warning: The RX pin must be used as an encoder input "
          "so its function will be changed to the default.\n");
      }
      break;
    }

    // Next, we make sure no pin is configured to do something that it cannot
    // do.  These checks are in order by pin function.

    if (product == TIC_PRODUCT_N825 && rc_func != TIC_PIN_FUNC_DEFAULT)
    {
      rc_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: On the Tic N825, the RC pin is always used for controlling "
        "the RS-485 transceiver and cannot be used for anything else, so its "
        "function will be changed to the default.\n");
      // This might change in future firmware versions.
    }

    if (rc_func == TIC_PIN_FUNC_USER_IO)
    {
      rc_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The RC pin cannot be a user I/O pin "
        "so its function will be changed to the default.\n");
    }

    if (sda_func == TIC_PIN_FUNC_POT_POWER)
    {
      sda_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The SDA pin cannot be used as a potentiometer power pin "
        "so its function will be changed to the default.\n");
    }

    if (tx_func == TIC_PIN_FUNC_POT_POWER)
    {
      tx_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The TX pin cannot be used as a potentiometer power pin "
        "so its function will be changed to the default.\n");
    }

    if (rx_func == TIC_PIN_FUNC_POT_POWER)
    {
      rx_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The RX pin cannot be used as a potentiometer power pin "
        "so its function will be changed to the default.\n");
    }

    if (rc_func == TIC_PIN_FUNC_POT_POWER)
    {
      rc_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The RC pin cannot be used as a potentiometer power pin "
        "so its function will be changed to the default.\n");
    }

    if (rc_func == TIC_PIN_FUNC_SERIAL)
    {
      rc_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The RC pin cannot be a serial pin "
        "so its function will be changed to the default.\n");
    }

    if (sda_func == TIC_PIN_FUNC_RC)
    {
      sda_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The SDA pin cannot be used as an RC input "
        "so its function will be changed to the default.\n");
    }

    if (scl_func == TIC_PIN_FUNC_RC)
    {
      scl_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The SCL pin cannot be used as an RC input "
        "so its function will be changed to the default.\n");
    }

    if (tx_func == TIC_PIN_FUNC_RC)
    {
      tx_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The TX pin cannot be used as an RC input "
        "so its function will be changed to the default.\n");
    }

    if (rx_func == TIC_PIN_FUNC_RC)
    {
      rx_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The RX pin cannot be used as an RC input "
        "so its function will be changed to the default.\n");
    }

    if (scl_func == TIC_PIN_FUNC_ENCODER)
    {
      scl_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The SCL pin cannot be used as an encoder input "
        "so its function will be changed to the default.\n");
    }

    if (sda_func == TIC_PIN_FUNC_ENCODER)
    {
      sda_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The SDA pin cannot be used as an encoder input "
        "so its function will be changed to the default.\n");
    }

    if (rc_func == TIC_PIN_FUNC_ENCODER)
    {
      rc_func = TIC_PIN_FUNC_DEFAULT;
      tic_sprintf(warnings,
        "Warning: The RC pin cannot be used as an encoder input "
        "so its function will be changed to the default.\n");
    }

    if (firmware_version && firmware_version < 0x0105)
    {
      bool warn = false;
      if (is_limit_switch(scl_func))
      {
        warn = true;
        scl_func = TIC_PIN_FUNC_DEFAULT;
      }
      if (is_limit_switch(sda_func))
      {
        warn = true;
        sda_func = TIC_PIN_FUNC_DEFAULT;
      }
      if (is_limit_switch(tx_func))
      {
        warn = true;
        tx_func = TIC_PIN_FUNC_DEFAULT;
      }
      if (is_limit_switch(rx_func))
      {
        warn = true;
        rx_func = TIC_PIN_FUNC_DEFAULT;
      }
      if (is_limit_switch(rc_func))
      {
        warn = true;
        rc_func = TIC_PIN_FUNC_DEFAULT;
      }
      if (warn)
      {
        tic_sprintf(warnings,
          "Warning: The firmware version on your device does not support "
          "limit switches, so any pin configured as a limit switch "
          "will be changed to its default function.  "
          "See " DOCUMENTATION_URL " for firmware upgrade instructions.\n");
      }
    }

    // Next, enforce proper values for pin booleans.
    if (rc_analog)
    {
      rc_analog = false;
      tic_sprintf(warnings,
        "Warning: The RC pin cannot be an analog input "
        "so that feature will be disabled.\n");
    }

    // Note: aren't enforcing proper values for the "pullup" boolean yet.  That
    // setting is more of a suggestion from the firmware; the RC line cannot
    // have a pull-up and the TX and RX lines always do if they are inputs.
    // The firmware's default settings for TX and RX don't set the pull-up bit,
    // so it would be bad to complain to the user about that.

    // Finally, if one of the SCL/SDA pins is configured for I2C, make sure the other one
    // is configured that way too.  This should be last because other checks in this
    // code might change SCL or SDA to be used for I2C.
    bool scl_is_i2c = (scl_func == TIC_PIN_FUNC_DEFAULT && !analog_control_mode) ||
      (scl_func == TIC_PIN_FUNC_SERIAL);
    bool sda_is_i2c = (sda_func == TIC_PIN_FUNC_DEFAULT && !analog_control_mode) ||
      (sda_func == TIC_PIN_FUNC_SERIAL);
    if (sda_is_i2c != scl_is_i2c)
    {
      scl_func = TIC_PIN_FUNC_DEFAULT;
      sda_func = TIC_PIN_FUNC_DEFAULT;
      if (sda_is_i2c)
      {
        tic_sprintf(warnings,
          "Warning: The SCL pin must be used for I2C if the SDA pin is, "
          "so the SCL and SDA pin functions will be changed to the default.\n");
      }
      else
      {
        tic_sprintf(warnings,
          "Warning: The SDA pin must be used for I2C if the SCL pin is, "
          "so the SCL and SDA pin functions will be changed to the default.\n");
      }
    }

    tic_settings_set_pin_func(settings, TIC_PIN_NUM_SCL, scl_func);
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_SDA, sda_func);
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_TX, tx_func);
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_RX, rx_func);
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_RC, rc_func);
    tic_settings_set_pin_analog(settings, TIC_PIN_NUM_RC, rc_analog);
  }

  if (product == TIC_PRODUCT_36V4)
  {
    if (!tic_settings_hp_gate_charge_ok(settings))
    {
      uint8_t toff = tic_settings_get_hp_toff(settings);
      while (true)
      {
        if (toff >= 255) { break; }   // Shouldn't happen.
        toff++;
        tic_settings_set_hp_toff(settings, toff);
        if (tic_settings_hp_gate_charge_ok(settings)) { break; }
      }
      uint32_t toff_ns = tic_settings_get_hp_toff_ns(settings);
      tic_sprintf(warnings,
        "Warning: The fixed off time will be increased to %u ns, "
        "which is the minimum valid value given other settings.", toff_ns);
    }
  }
}

tic_error * tic_settings_fix(tic_settings * settings, char ** warnings)
{
  if (warnings) { *warnings = NULL; }

  if (settings == NULL)
  {
    return tic_error_create("Tic settings pointer is null.");
  }

  // Make a string to store the warnings we accumulate in this function.
  tic_string str;
  if (warnings)
  {
    tic_string_setup(&str);
  }
  else
  {
    tic_string_setup_dummy(&str);
  }

  tic_settings_fix_core(settings, &str);

  #ifndef NDEBUG
  {
    // In debug builds, assert that this function is idempotent.
    tic_string str2;
    tic_string_setup(&str2);
    tic_settings_fix_core(settings, &str2);
    assert(str2.data != NULL && str2.data[0] == 0);
    tic_string_free(str2.data);
  }
  #endif

  if (warnings && str.data == NULL)
  {
    // Memory allocation for the warning string failed at some point.
    return &tic_error_no_memory;
  }

  if (warnings)
  {
    *warnings = str.data;
    str.data = NULL;
  }

  return NULL;
}
