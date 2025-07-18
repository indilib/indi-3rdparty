// Functions for writing settings to the device.

#include "tic_internal.h"

static void tic_write_settings_to_buffer(const tic_settings * settings, uint8_t * buf)
{
  assert(settings != NULL);
  assert(buf != NULL);

  uint8_t product = tic_settings_get_product(settings);

  buf[TIC_SETTING_CONTROL_MODE] =
    tic_settings_get_control_mode(settings);

  buf[TIC_SETTING_OPTIONS_BYTE1] |=
    (tic_settings_get_never_sleep(settings) & 1) <<
    TIC_OPTIONS_BYTE1_NEVER_SLEEP;

  buf[TIC_SETTING_DISABLE_SAFE_START] =
    tic_settings_get_disable_safe_start(settings);

  buf[TIC_SETTING_IGNORE_ERR_LINE_HIGH] =
    tic_settings_get_ignore_err_line_high(settings);

  buf[TIC_SETTING_AUTO_CLEAR_DRIVER_ERROR] =
    tic_settings_get_auto_clear_driver_error(settings);

  buf[TIC_SETTING_SOFT_ERROR_RESPONSE] =
    tic_settings_get_soft_error_response(settings);

  {
    uint32_t position = (uint32_t)tic_settings_get_soft_error_position(settings);
    buf[TIC_SETTING_SOFT_ERROR_POSITION + 0] = position >> 0 & 0xFF;
    buf[TIC_SETTING_SOFT_ERROR_POSITION + 1] = position >> 8 & 0xFF;
    buf[TIC_SETTING_SOFT_ERROR_POSITION + 2] = position >> 16 & 0xFF;
    buf[TIC_SETTING_SOFT_ERROR_POSITION + 3] = position >> 24 & 0xFF;
  }

  {
    uint32_t baud_rate = tic_settings_get_serial_baud_rate(settings);
    uint16_t brg = tic_baud_rate_to_brg(baud_rate);
    buf[TIC_SETTING_SERIAL_BAUD_RATE_GENERATOR + 0] = brg >> 0 & 0xFF;
    buf[TIC_SETTING_SERIAL_BAUD_RATE_GENERATOR + 1] = brg >> 8 & 0xFF;
  }

  {
    uint16_t number = tic_settings_get_serial_device_number_u16(settings);
    buf[TIC_SETTING_SERIAL_DEVICE_NUMBER_LOW] = number & 0x7F;
    buf[TIC_SETTING_SERIAL_DEVICE_NUMBER_HIGH] = number >> 7 & 0x7F;
  }

  {
    uint16_t number = tic_settings_get_serial_alt_device_number(settings);
    buf[TIC_SETTING_SERIAL_ALT_DEVICE_NUMBER + 0] = number & 0x7F;
    buf[TIC_SETTING_SERIAL_ALT_DEVICE_NUMBER + 1] = number >> 7 & 0x7F;

    bool enabled = tic_settings_get_serial_enable_alt_device_number(settings);
    if (enabled)
    {
      buf[TIC_SETTING_SERIAL_ALT_DEVICE_NUMBER + 0] |= 0x80;
    }
  }

  buf[TIC_SETTING_SERIAL_OPTIONS_BYTE] |=
    (tic_settings_get_serial_14bit_device_number(settings) & 1) <<
    TIC_SERIAL_OPTIONS_BYTE_14BIT_DEVICE_NUMBER;

  {
    uint16_t command_timeout = tic_settings_get_command_timeout(settings);
    buf[TIC_SETTING_COMMAND_TIMEOUT + 0] = command_timeout >> 0 & 0xFF;
    buf[TIC_SETTING_COMMAND_TIMEOUT + 1] = command_timeout >> 8 & 0xFF;
  }

  buf[TIC_SETTING_SERIAL_OPTIONS_BYTE] |=
    (tic_settings_get_serial_crc_for_commands(settings) & 1) <<
    TIC_SERIAL_OPTIONS_BYTE_CRC_FOR_COMMANDS;

  buf[TIC_SETTING_SERIAL_OPTIONS_BYTE] |=
    (tic_settings_get_serial_crc_for_responses(settings) & 1) <<
    TIC_SERIAL_OPTIONS_BYTE_CRC_FOR_RESPONSES;

  buf[TIC_SETTING_SERIAL_OPTIONS_BYTE] |=
    (tic_settings_get_serial_7bit_responses(settings) & 1) <<
    TIC_SERIAL_OPTIONS_BYTE_7BIT_RESPONSES;

  buf[TIC_SETTING_SERIAL_RESPONSE_DELAY] =
    tic_settings_get_serial_response_delay(settings);

  {
    uint16_t low_vin_timeout = tic_settings_get_low_vin_timeout(settings);
    buf[TIC_SETTING_LOW_VIN_TIMEOUT + 0] = low_vin_timeout >> 0 & 0xFF;
    buf[TIC_SETTING_LOW_VIN_TIMEOUT + 1] = low_vin_timeout >> 8 & 0xFF;
  }

  {
    uint16_t voltage = tic_settings_get_low_vin_shutoff_voltage(settings);
    buf[TIC_SETTING_LOW_VIN_SHUTOFF_VOLTAGE + 0] = voltage >> 0 & 0xFF;
    buf[TIC_SETTING_LOW_VIN_SHUTOFF_VOLTAGE + 1] = voltage >> 8 & 0xFF;
  }

  {
    uint16_t voltage = tic_settings_get_low_vin_startup_voltage(settings);
    buf[TIC_SETTING_LOW_VIN_STARTUP_VOLTAGE + 0] = voltage >> 0 & 0xFF;
    buf[TIC_SETTING_LOW_VIN_STARTUP_VOLTAGE + 1] = voltage >> 8 & 0xFF;
  }

  {
    uint16_t voltage = tic_settings_get_high_vin_shutoff_voltage(settings);
    buf[TIC_SETTING_HIGH_VIN_SHUTOFF_VOLTAGE + 0] = voltage >> 0 & 0xFF;
    buf[TIC_SETTING_HIGH_VIN_SHUTOFF_VOLTAGE + 1] = voltage >> 8 & 0xFF;
  }
  {
    uint16_t offset = (uint16_t)tic_settings_get_vin_calibration(settings);
    buf[TIC_SETTING_VIN_CALIBRATION + 0] = offset >> 0 & 0xFF;
    buf[TIC_SETTING_VIN_CALIBRATION + 1] = offset >> 8 & 0xFF;
  }

  {
    uint16_t period = tic_settings_get_rc_max_pulse_period(settings);
    buf[TIC_SETTING_RC_MAX_PULSE_PERIOD + 0] = period >> 0 & 0xFF;
    buf[TIC_SETTING_RC_MAX_PULSE_PERIOD + 1] = period >> 8 & 0xFF;
  }

  {
    uint16_t timeout = tic_settings_get_rc_bad_signal_timeout(settings);
    buf[TIC_SETTING_RC_BAD_SIGNAL_TIMEOUT + 0] = timeout >> 0 & 0xFF;
    buf[TIC_SETTING_RC_BAD_SIGNAL_TIMEOUT + 1] = timeout >> 8 & 0xFF;
  }

  {
    uint16_t pulses = tic_settings_get_rc_consecutive_good_pulses(settings);
    buf[TIC_SETTING_RC_CONSECUTIVE_GOOD_PULSES + 0] = pulses >> 0 & 0xFF;
    buf[TIC_SETTING_RC_CONSECUTIVE_GOOD_PULSES + 1] = pulses >> 8 & 0xFF;
  }

  buf[TIC_SETTING_INPUT_AVERAGING_ENABLED] =
    tic_settings_get_input_averaging_enabled(settings);

  {
    uint16_t hysteresis = tic_settings_get_input_hysteresis(settings);
    buf[TIC_SETTING_INPUT_HYSTERESIS + 0] = hysteresis >> 0 & 0xFF;
    buf[TIC_SETTING_INPUT_HYSTERESIS + 1] = hysteresis >> 8 & 0xFF;
  }

  {
    uint16_t input_error_min = tic_settings_get_input_error_min(settings);
    buf[TIC_SETTING_INPUT_ERROR_MIN + 0] = input_error_min >> 0 & 0xFF;
    buf[TIC_SETTING_INPUT_ERROR_MIN + 1] = input_error_min >> 8 & 0xFF;
  }

  {
    uint16_t input_error_max = tic_settings_get_input_error_max(settings);
    buf[TIC_SETTING_INPUT_ERROR_MAX + 0] = input_error_max >> 0 & 0xFF;
    buf[TIC_SETTING_INPUT_ERROR_MAX + 1] = input_error_max >> 8 & 0xFF;
  }

  buf[TIC_SETTING_INPUT_SCALING_DEGREE] =
    tic_settings_get_input_scaling_degree(settings);

  buf[TIC_SETTING_INPUT_INVERT] =
    tic_settings_get_input_invert(settings);

  {
    uint16_t input_min = tic_settings_get_input_min(settings);
    buf[TIC_SETTING_INPUT_MIN + 0] = input_min >> 0 & 0xFF;
    buf[TIC_SETTING_INPUT_MIN + 1] = input_min >> 8 & 0xFF;
  }

  {
    uint16_t input_neutral_min = tic_settings_get_input_neutral_min(settings);
    buf[TIC_SETTING_INPUT_NEUTRAL_MIN + 0] = input_neutral_min >> 0 & 0xFF;
    buf[TIC_SETTING_INPUT_NEUTRAL_MIN + 1] = input_neutral_min >> 8 & 0xFF;
  }

  {
    uint16_t input_neutral_max = tic_settings_get_input_neutral_max(settings);
    buf[TIC_SETTING_INPUT_NEUTRAL_MAX + 0] = input_neutral_max >> 0 & 0xFF;
    buf[TIC_SETTING_INPUT_NEUTRAL_MAX + 1] = input_neutral_max >> 8 & 0xFF;
  }

  {
    uint16_t input_max = tic_settings_get_input_max(settings);
    buf[TIC_SETTING_INPUT_MAX + 0] = input_max >> 0 & 0xFF;
    buf[TIC_SETTING_INPUT_MAX + 1] = input_max >> 8 & 0xFF;
  }

  {
    uint32_t output = (uint32_t)tic_settings_get_output_min(settings);
    buf[TIC_SETTING_OUTPUT_MIN + 0] = output >> 0 & 0xFF;
    buf[TIC_SETTING_OUTPUT_MIN + 1] = output >> 8 & 0xFF;
    buf[TIC_SETTING_OUTPUT_MIN + 2] = output >> 16 & 0xFF;
    buf[TIC_SETTING_OUTPUT_MIN + 3] = output >> 24 & 0xFF;
  }

  {
    uint32_t output = (uint32_t)tic_settings_get_output_max(settings);
    buf[TIC_SETTING_OUTPUT_MAX + 0] = output >> 0 & 0xFF;
    buf[TIC_SETTING_OUTPUT_MAX + 1] = output >> 8 & 0xFF;
    buf[TIC_SETTING_OUTPUT_MAX + 2] = output >> 16 & 0xFF;
    buf[TIC_SETTING_OUTPUT_MAX + 3] = output >> 24 & 0xFF;
  }

  {
    uint32_t prescaler = tic_settings_get_encoder_prescaler(settings);
    buf[TIC_SETTING_ENCODER_PRESCALER + 0] = prescaler >> 0 & 0xFF;
    buf[TIC_SETTING_ENCODER_PRESCALER + 1] = prescaler >> 8 & 0xFF;
    buf[TIC_SETTING_ENCODER_PRESCALER + 2] = prescaler >> 16 & 0xFF;
    buf[TIC_SETTING_ENCODER_PRESCALER + 3] = prescaler >> 24 & 0xFF;
  }

  {
    uint32_t postscaler = tic_settings_get_encoder_postscaler(settings);
    buf[TIC_SETTING_ENCODER_POSTSCALER + 0] = postscaler >> 0 & 0xFF;
    buf[TIC_SETTING_ENCODER_POSTSCALER + 1] = postscaler >> 8 & 0xFF;
    buf[TIC_SETTING_ENCODER_POSTSCALER + 2] = postscaler >> 16 & 0xFF;
    buf[TIC_SETTING_ENCODER_POSTSCALER + 3] = postscaler >> 24 & 0xFF;
  }

  buf[TIC_SETTING_ENCODER_UNLIMITED] = tic_settings_get_encoder_unlimited(settings);

  {
    buf[TIC_SETTING_SCL_CONFIG] = tic_settings_get_pin_func(settings, TIC_PIN_NUM_SCL)
      | (tic_settings_get_pin_pullup(settings, TIC_PIN_NUM_SCL) << TIC_PIN_PULLUP)
      | (tic_settings_get_pin_analog(settings, TIC_PIN_NUM_SCL) << TIC_PIN_ANALOG);

    buf[TIC_SETTING_SDA_CONFIG] = tic_settings_get_pin_func(settings, TIC_PIN_NUM_SDA)
      | (tic_settings_get_pin_pullup(settings, TIC_PIN_NUM_SDA) << TIC_PIN_PULLUP)
      | (tic_settings_get_pin_analog(settings, TIC_PIN_NUM_SDA) << TIC_PIN_ANALOG);

    buf[TIC_SETTING_TX_CONFIG] = tic_settings_get_pin_func(settings, TIC_PIN_NUM_TX)
      | (tic_settings_get_pin_pullup(settings, TIC_PIN_NUM_TX) << TIC_PIN_PULLUP)
      | (tic_settings_get_pin_analog(settings, TIC_PIN_NUM_TX) << TIC_PIN_ANALOG);

    buf[TIC_SETTING_RX_CONFIG] = tic_settings_get_pin_func(settings, TIC_PIN_NUM_RX)
      | (tic_settings_get_pin_pullup(settings, TIC_PIN_NUM_RX) << TIC_PIN_PULLUP)
      | (tic_settings_get_pin_analog(settings, TIC_PIN_NUM_RX) << TIC_PIN_ANALOG);

    buf[TIC_SETTING_RC_CONFIG] = tic_settings_get_pin_func(settings, TIC_PIN_NUM_RC)
      | (tic_settings_get_pin_pullup(settings, TIC_PIN_NUM_RC) << TIC_PIN_PULLUP)
      | (tic_settings_get_pin_analog(settings, TIC_PIN_NUM_RC) << TIC_PIN_ANALOG);

    for (uint8_t i = 0; i < TIC_CONTROL_PIN_COUNT; i++)
    {
      uint8_t func = tic_settings_get_pin_func(settings, i);
      if (func == TIC_PIN_FUNC_KILL_SWITCH)
      {
        buf[TIC_SETTING_KILL_SWITCH_MAP] |= (1 << i);
      }
      if (func == TIC_PIN_FUNC_LIMIT_SWITCH_FORWARD)
      {
        buf[TIC_SETTING_LIMIT_SWITCH_FORWARD_MAP] |= (1 << i);
      }
      if (func == TIC_PIN_FUNC_LIMIT_SWITCH_REVERSE)
      {
        buf[TIC_SETTING_LIMIT_SWITCH_REVERSE_MAP] |= (1 << i);
      }
      if (tic_settings_get_pin_polarity(settings, i))
      {
        buf[TIC_SETTING_SWITCH_POLARITY_MAP] |= (1 << i);
      }
    }
  }

  uint8_t current_limit_code = tic_settings_get_current_limit_code(settings);
  buf[TIC_SETTING_CURRENT_LIMIT] = current_limit_code;

  buf[TIC_SETTING_CURRENT_LIMIT_DURING_ERROR] =
    tic_settings_get_current_limit_code_during_error(settings);

  uint8_t step_mode = tic_settings_get_step_mode(settings);
  buf[TIC_SETTING_STEP_MODE] = step_mode;

  if (product == TIC_PRODUCT_T825 ||
    product == TIC_PRODUCT_N825 ||
    product == TIC_PRODUCT_T834)
  {
    buf[TIC_SETTING_DECAY_MODE] = tic_settings_get_decay_mode(settings);
  }

  if (product == TIC_PRODUCT_T249)
  {
    buf[TIC_SETTING_AGC_MODE] = tic_settings_get_agc_mode(settings);
    buf[TIC_SETTING_AGC_BOTTOM_CURRENT_LIMIT] =
      tic_settings_get_agc_bottom_current_limit(settings);
    buf[TIC_SETTING_AGC_CURRENT_BOOST_STEPS] =
      tic_settings_get_agc_current_boost_steps(settings);
    buf[TIC_SETTING_AGC_FREQUENCY_LIMIT] =
      tic_settings_get_agc_frequency_limit(settings);
  }

  {
    uint32_t speed = tic_settings_get_starting_speed(settings);
    buf[TIC_SETTING_STARTING_SPEED + 0] = speed >> 0 & 0xFF;
    buf[TIC_SETTING_STARTING_SPEED + 1] = speed >> 8 & 0xFF;
    buf[TIC_SETTING_STARTING_SPEED + 2] = speed >> 16 & 0xFF;
    buf[TIC_SETTING_STARTING_SPEED + 3] = speed >> 24 & 0xFF;
  }

  {
    uint32_t speed = tic_settings_get_max_speed(settings);
    buf[TIC_SETTING_MAX_SPEED + 0] = speed >> 0 & 0xFF;
    buf[TIC_SETTING_MAX_SPEED + 1] = speed >> 8 & 0xFF;
    buf[TIC_SETTING_MAX_SPEED + 2] = speed >> 16 & 0xFF;
    buf[TIC_SETTING_MAX_SPEED + 3] = speed >> 24 & 0xFF;
  }

  {
    uint32_t accel = tic_settings_get_max_accel(settings);
    buf[TIC_SETTING_MAX_ACCEL + 0] = accel >> 0 & 0xFF;
    buf[TIC_SETTING_MAX_ACCEL + 1] = accel >> 8 & 0xFF;
    buf[TIC_SETTING_MAX_ACCEL + 2] = accel >> 16 & 0xFF;
    buf[TIC_SETTING_MAX_ACCEL + 3] = accel >> 24 & 0xFF;
  }

  {
    uint32_t decel = tic_settings_get_max_decel(settings);
    buf[TIC_SETTING_MAX_DECEL + 0] = decel >> 0 & 0xFF;
    buf[TIC_SETTING_MAX_DECEL + 1] = decel >> 8 & 0xFF;
    buf[TIC_SETTING_MAX_DECEL + 2] = decel >> 16 & 0xFF;
    buf[TIC_SETTING_MAX_DECEL + 3] = decel >> 24 & 0xFF;
  }

  buf[TIC_SETTING_OPTIONS_BYTE1] |=
    (tic_settings_get_auto_homing(settings) & 1) <<
    TIC_OPTIONS_BYTE1_AUTO_HOMING;

  buf[TIC_SETTING_OPTIONS_BYTE1] |=
    (tic_settings_get_auto_homing_forward(settings) & 1) <<
    TIC_OPTIONS_BYTE1_AUTO_HOMING_FORWARD;

  {
    uint32_t speed = tic_settings_get_homing_speed_towards(settings);
    buf[TIC_SETTING_HOMING_SPEED_TOWARDS + 0] = speed >> 0 & 0xFF;
    buf[TIC_SETTING_HOMING_SPEED_TOWARDS + 1] = speed >> 8 & 0xFF;
    buf[TIC_SETTING_HOMING_SPEED_TOWARDS + 2] = speed >> 16 & 0xFF;
    buf[TIC_SETTING_HOMING_SPEED_TOWARDS + 3] = speed >> 24 & 0xFF;
  }

  {
    uint32_t speed = tic_settings_get_homing_speed_away(settings);
    buf[TIC_SETTING_HOMING_SPEED_AWAY + 0] = speed >> 0 & 0xFF;
    buf[TIC_SETTING_HOMING_SPEED_AWAY + 1] = speed >> 8 & 0xFF;
    buf[TIC_SETTING_HOMING_SPEED_AWAY + 2] = speed >> 16 & 0xFF;
    buf[TIC_SETTING_HOMING_SPEED_AWAY + 3] = speed >> 24 & 0xFF;
  }

  buf[TIC_SETTING_INVERT_MOTOR_DIRECTION] =
    tic_settings_get_invert_motor_direction(settings);

  if (product == TIC_PRODUCT_36V4)
  {
    buf[TIC_SETTING_HP_ENABLE_UNRESTRICTED_CURRENT_LIMITS] =
      tic_settings_get_hp_enable_unrestricted_current_limits(settings);

    uint8_t * p = buf + TIC_SETTING_HP_DRIVER_REGISTERS;

    uint8_t step_mode_code = step_mode;
    if (step_mode_code >= TIC_STEP_MODE_MICROSTEP2_100P) { step_mode_code--; }
    step_mode_code &= 0xF;

    // CTRL register
    // Sets DTIME to 00 (850 ns), which hp_gate_charge_ok relies on.
    p[0] = step_mode_code << 3;
    p[1] = 0b1100;
    // TORQUE register
    p[2] = current_limit_code;
    p[3] = 0b0001;
    // OFF register
    p[4] = tic_settings_get_hp_toff(settings);
    p[5] = 0;
    // BLANK register
    p[6] = tic_settings_get_hp_tblank(settings);
    p[7] = tic_settings_get_hp_abt(settings);
    // DECAY register
    p[8] = tic_settings_get_hp_tdecay(settings);
    p[9] = tic_settings_get_hp_decmod(settings) & 7;
    // STALL register
    p[10] = 0;
    p[11] = 0xA;
    // DRIVE register
    p[12] = 0x08;
    p[13] = 0xA;
  }
}

tic_error * tic_set_settings(tic_handle * handle, const tic_settings * settings)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  if (settings == NULL)
  {
    return tic_error_create("Settings object is null.");
  }

  tic_error * error = NULL;

  tic_settings * fixed_settings = NULL;

  // Copy the settings so we can fix them without modifying the input ones,
  // which would be surprising to the caller.
  if (error == NULL)
  {
    error = tic_settings_copy(settings, &fixed_settings);
  }

  // Set the product code of the settings and fix the settings silently to make
  // sure we don't apply invalid settings to the device.  A good app will set
  // the product and call tic_settings_fix on its own before calling this
  // function, so there should be nothing to fix here.
  if (error == NULL)
  {
    const tic_device * device = tic_handle_get_device(handle);
    tic_settings_set_product(fixed_settings, tic_device_get_product(device));
    tic_settings_set_firmware_version(fixed_settings,
      tic_device_get_firmware_version(device));
    error = tic_settings_fix(fixed_settings, NULL);
  }

  // Construct a buffer holding the bytes we want to write.
  uint8_t buf[256] = { 0 };
  tic_write_settings_to_buffer(fixed_settings, buf);

  // Write the bytes to the device.
  uint8_t product = tic_device_get_product(tic_handle_get_device(handle));
  tic_settings_segments segments = tic_get_settings_segments(product);
  if (error == NULL)
  {
    error = tic_set_setting_segment(handle,
      segments.general_offset, segments.general_size,
      buf + segments.general_offset);
  }

  if (error == NULL && segments.product_specific_size)
  {
    error = tic_set_setting_segment(handle,
      segments.product_specific_offset, segments.product_specific_size,
      buf + segments.product_specific_offset);
  }

  tic_settings_free(fixed_settings);

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error applying settings to the device.");
  }

  return error;
}
