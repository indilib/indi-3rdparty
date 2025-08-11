// Function for reading settings from the Tic over USB.

#include "tic_internal.h"

static void write_buffer_to_settings(const uint8_t * buf, tic_settings * settings)
{
  uint8_t product = tic_settings_get_product(settings);

  {
    uint8_t control_mode = buf[TIC_SETTING_CONTROL_MODE];
    tic_settings_set_control_mode(settings, control_mode);
  }

  {
    bool never_sleep = buf[TIC_SETTING_OPTIONS_BYTE1] >>
      TIC_OPTIONS_BYTE1_NEVER_SLEEP & 1;
    tic_settings_set_never_sleep(settings, never_sleep);
  }

  {
    bool disable_safe_start = buf[TIC_SETTING_DISABLE_SAFE_START] & 1;
    tic_settings_set_disable_safe_start(settings, disable_safe_start);
  }

  {
    bool ignore_err_line_high = buf[TIC_SETTING_IGNORE_ERR_LINE_HIGH] & 1;
    tic_settings_set_ignore_err_line_high(settings, ignore_err_line_high);
  }

  {
    bool auto_clear_driver_error = buf[TIC_SETTING_AUTO_CLEAR_DRIVER_ERROR] & 1;
    tic_settings_set_auto_clear_driver_error(settings, auto_clear_driver_error);
  }

  {
    uint8_t response = buf[TIC_SETTING_SOFT_ERROR_RESPONSE];
    tic_settings_set_soft_error_response(settings, response);
  }

  {
    int32_t position = read_i32(buf + TIC_SETTING_SOFT_ERROR_POSITION);
    tic_settings_set_soft_error_position(settings, position);
  }

  {
    uint16_t brg = read_u16(buf + TIC_SETTING_SERIAL_BAUD_RATE_GENERATOR);
    uint32_t baud_rate = tic_baud_rate_from_brg(brg);
    tic_settings_set_serial_baud_rate(settings, baud_rate);
  }

  {
    uint16_t number =
      (buf[TIC_SETTING_SERIAL_DEVICE_NUMBER_LOW] & 0x7F) |
      ((buf[TIC_SETTING_SERIAL_DEVICE_NUMBER_HIGH] & 0x7F) << 7);
    tic_settings_set_serial_device_number_u16(settings, number);
  }

  {
    uint16_t number =
      (buf[TIC_SETTING_SERIAL_ALT_DEVICE_NUMBER] & 0x7F) |
      ((buf[TIC_SETTING_SERIAL_ALT_DEVICE_NUMBER + 1] & 0x7F) << 7);
    tic_settings_set_serial_alt_device_number(settings, number);

    bool enable = buf[TIC_SETTING_SERIAL_ALT_DEVICE_NUMBER] >> 7 & 1;
    tic_settings_set_serial_enable_alt_device_number(settings, enable);
  }

  {
    bool enabled = buf[TIC_SETTING_SERIAL_OPTIONS_BYTE] >>
      TIC_SERIAL_OPTIONS_BYTE_14BIT_DEVICE_NUMBER & 1;
    tic_settings_set_serial_14bit_device_number(settings, enabled);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_COMMAND_TIMEOUT;
    uint16_t command_timeout = p[0] + (p[1] << 8);
    tic_settings_set_command_timeout(settings, command_timeout);
  }

  {
    bool enabled = buf[TIC_SETTING_SERIAL_OPTIONS_BYTE] >>
      TIC_SERIAL_OPTIONS_BYTE_CRC_FOR_COMMANDS & 1;
    tic_settings_set_serial_crc_for_commands(settings, enabled);
  }

  {
    bool enabled = buf[TIC_SETTING_SERIAL_OPTIONS_BYTE] >>
      TIC_SERIAL_OPTIONS_BYTE_CRC_FOR_RESPONSES & 1;
    tic_settings_set_serial_crc_for_responses(settings, enabled);
  }

  {
    bool enabled = buf[TIC_SETTING_SERIAL_OPTIONS_BYTE] >>
      TIC_SERIAL_OPTIONS_BYTE_7BIT_RESPONSES & 1;
    tic_settings_set_serial_7bit_responses(settings, enabled);
  }

  {
    uint8_t delay = buf[TIC_SETTING_SERIAL_RESPONSE_DELAY];
    tic_settings_set_serial_response_delay(settings, delay);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_LOW_VIN_TIMEOUT;
    uint16_t low_vin_timeout = p[0] + (p[1] << 8);
    tic_settings_set_low_vin_timeout(settings, low_vin_timeout);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_LOW_VIN_SHUTOFF_VOLTAGE;
    uint16_t low_vin_shutoff_voltage = p[0] + (p[1] << 8);
    tic_settings_set_low_vin_shutoff_voltage(settings,
      low_vin_shutoff_voltage);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_LOW_VIN_STARTUP_VOLTAGE;
    uint16_t low_vin_startup_voltage = p[0] + (p[1] << 8);
    tic_settings_set_low_vin_startup_voltage(settings,
      low_vin_startup_voltage);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_HIGH_VIN_SHUTOFF_VOLTAGE;
    uint16_t high_vin_shutoff_voltage = p[0] + (p[1] << 8);
    tic_settings_set_high_vin_shutoff_voltage(settings,
      high_vin_shutoff_voltage);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_VIN_CALIBRATION;
    int16_t vin_calibration = p[0] + (p[1] << 8);
    tic_settings_set_vin_calibration(settings, vin_calibration);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_RC_MAX_PULSE_PERIOD;
    uint16_t rc_max_pulse_period = p[0] + (p[1] << 8);
    tic_settings_set_rc_max_pulse_period(settings,
      rc_max_pulse_period);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_RC_BAD_SIGNAL_TIMEOUT;
    uint16_t rc_bad_signal_timeout = p[0] + (p[1] << 8);
    tic_settings_set_rc_bad_signal_timeout(settings,
      rc_bad_signal_timeout);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_RC_CONSECUTIVE_GOOD_PULSES;
    uint16_t rc_consecutive_good_pulses = p[0] + (p[1] << 8);
    tic_settings_set_rc_consecutive_good_pulses(settings,
      rc_consecutive_good_pulses);
  }

  {
    bool input_averaging_enabled =
      buf[TIC_SETTING_INPUT_AVERAGING_ENABLED] & 1;
    tic_settings_set_input_averaging_enabled(settings, input_averaging_enabled);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_INPUT_HYSTERESIS;
    uint16_t input_hysteresis = p[0] + (p[1] << 8);
    tic_settings_set_input_hysteresis(settings, input_hysteresis);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_INPUT_ERROR_MIN;
    uint16_t input_error_min = p[0] + (p[1] << 8);
    tic_settings_set_input_error_min(settings, input_error_min);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_INPUT_ERROR_MAX;
    uint16_t input_error_max = p[0] + (p[1] << 8);
    tic_settings_set_input_error_max(settings, input_error_max);
  }

  {
    uint8_t input_scaling_degree = buf[TIC_SETTING_INPUT_SCALING_DEGREE];
    tic_settings_set_input_scaling_degree(settings, input_scaling_degree);
  }

  {
    bool input_invert = buf[TIC_SETTING_INPUT_INVERT] & 1;
    tic_settings_set_input_invert(settings, input_invert);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_INPUT_MIN;
    uint16_t input_min = p[0] + (p[1] << 8);
    tic_settings_set_input_min(settings, input_min);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_INPUT_NEUTRAL_MIN;
    uint16_t input_neutral_min = p[0] + (p[1] << 8);
    tic_settings_set_input_neutral_min(settings, input_neutral_min);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_INPUT_NEUTRAL_MAX;
    uint16_t input_neutral_max = p[0] + (p[1] << 8);
    tic_settings_set_input_neutral_max(settings, input_neutral_max);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_INPUT_MAX;
    uint16_t input_max = p[0] + (p[1] << 8);
    tic_settings_set_input_max(settings, input_max);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_OUTPUT_MIN;
    int32_t output_min = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_output_min(settings, output_min);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_OUTPUT_MAX;
    int32_t output_max = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_output_max(settings, output_max);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_ENCODER_PRESCALER;
    uint32_t prescaler = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_encoder_prescaler(settings, prescaler);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_ENCODER_POSTSCALER;
    uint32_t postscaler = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_encoder_postscaler(settings, postscaler);
  }

  {
    bool encoder_unlimited = buf[TIC_SETTING_ENCODER_UNLIMITED] & 1;
    tic_settings_set_encoder_unlimited(settings, encoder_unlimited);
  }

  {
    uint8_t config;

    config = buf[TIC_SETTING_SCL_CONFIG];
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_SCL, config & TIC_PIN_FUNC_MASK);
    tic_settings_set_pin_analog(settings, TIC_PIN_NUM_SCL, config >> TIC_PIN_ANALOG & 1);
    tic_settings_set_pin_pullup(settings, TIC_PIN_NUM_SCL, config >> TIC_PIN_PULLUP & 1);

    config = buf[TIC_SETTING_SDA_CONFIG];
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_SDA, config & TIC_PIN_FUNC_MASK);
    tic_settings_set_pin_analog(settings, TIC_PIN_NUM_SDA, config >> TIC_PIN_ANALOG & 1);
    tic_settings_set_pin_pullup(settings, TIC_PIN_NUM_SDA, config >> TIC_PIN_PULLUP & 1);

    config = buf[TIC_SETTING_TX_CONFIG];
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_TX, config & TIC_PIN_FUNC_MASK);
    tic_settings_set_pin_analog(settings, TIC_PIN_NUM_TX, config >> TIC_PIN_ANALOG & 1);
    tic_settings_set_pin_pullup(settings, TIC_PIN_NUM_TX, config >> TIC_PIN_PULLUP & 1);

    config = buf[TIC_SETTING_RX_CONFIG];
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_RX, config & TIC_PIN_FUNC_MASK);
    tic_settings_set_pin_analog(settings, TIC_PIN_NUM_RX, config >> TIC_PIN_ANALOG & 1);
    tic_settings_set_pin_pullup(settings, TIC_PIN_NUM_RX, config >> TIC_PIN_PULLUP & 1);

    config = buf[TIC_SETTING_RC_CONFIG];
    tic_settings_set_pin_func(settings, TIC_PIN_NUM_RC, config & TIC_PIN_FUNC_MASK);
    tic_settings_set_pin_analog(settings, TIC_PIN_NUM_RC, config >> TIC_PIN_ANALOG & 1);
    tic_settings_set_pin_pullup(settings, TIC_PIN_NUM_RC, config >> TIC_PIN_PULLUP & 1);

    uint8_t switch_polarity = buf[TIC_SETTING_SWITCH_POLARITY_MAP];
    for (uint8_t i = 0; i < TIC_CONTROL_PIN_COUNT; i++)
    {
      tic_settings_set_pin_polarity(settings, i,
        switch_polarity >> i & 1);
    }
  }

  {
    uint8_t code = buf[TIC_SETTING_CURRENT_LIMIT];
    tic_settings_set_current_limit_code(settings, code);
  }

  {
    uint8_t code = buf[TIC_SETTING_CURRENT_LIMIT_DURING_ERROR];
    tic_settings_set_current_limit_code_during_error(settings, code);
  }

  {
    uint8_t step_mode = buf[TIC_SETTING_STEP_MODE];
    tic_settings_set_step_mode(settings, step_mode);
  }

  if (product == TIC_PRODUCT_T825 ||
    product == TIC_PRODUCT_N825 ||
    product == TIC_PRODUCT_T834)
  {
    uint8_t decay_mode = buf[TIC_SETTING_DECAY_MODE];
    tic_settings_set_decay_mode(settings, decay_mode);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_STARTING_SPEED;
    uint32_t starting_speed = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_starting_speed(settings, starting_speed);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_MAX_SPEED;
    uint32_t max_speed = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_max_speed(settings, max_speed);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_MAX_ACCEL;
    uint32_t max_accel = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_max_accel(settings, max_accel);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_MAX_DECEL;
    uint32_t max_decel = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_max_decel(settings, max_decel);
  }

  {
    bool auto_homing = buf[TIC_SETTING_OPTIONS_BYTE1] >>
      TIC_OPTIONS_BYTE1_AUTO_HOMING & 1;
    tic_settings_set_auto_homing(settings, auto_homing);
  }

  {
    bool forward = buf[TIC_SETTING_OPTIONS_BYTE1] >>
      TIC_OPTIONS_BYTE1_AUTO_HOMING_FORWARD & 1;
    tic_settings_set_auto_homing_forward(settings, forward);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_HOMING_SPEED_TOWARDS;
    uint32_t speed = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_homing_speed_towards(settings, speed);
  }

  {
    const uint8_t * p = buf + TIC_SETTING_HOMING_SPEED_AWAY;
    uint32_t speed = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
    tic_settings_set_homing_speed_away(settings, speed);
  }

  {
    bool invert = buf[TIC_SETTING_INVERT_MOTOR_DIRECTION] & 1;
    tic_settings_set_invert_motor_direction(settings, invert);
  }

  if (product == TIC_PRODUCT_T249)
  {
    {
      uint8_t mode = buf[TIC_SETTING_AGC_MODE];
      tic_settings_set_agc_mode(settings, mode);
    }

    {
      uint8_t limit = buf[TIC_SETTING_AGC_BOTTOM_CURRENT_LIMIT];
      tic_settings_set_agc_bottom_current_limit(settings, limit);
    }

    {
      uint8_t steps = buf[TIC_SETTING_AGC_CURRENT_BOOST_STEPS];
      tic_settings_set_agc_current_boost_steps(settings, steps);
    }

    {
      uint8_t limit = buf[TIC_SETTING_AGC_FREQUENCY_LIMIT];
      tic_settings_set_agc_frequency_limit(settings, limit);
    }
  }

  if (product == TIC_PRODUCT_36V4)
  {
    tic_settings_set_hp_enable_unrestricted_current_limits(settings,
      buf[TIC_SETTING_HP_ENABLE_UNRESTRICTED_CURRENT_LIMITS] & 1);

    const uint8_t * p = buf + TIC_SETTING_HP_DRIVER_REGISTERS;
    tic_settings_set_hp_toff(settings, p[4]);
    tic_settings_set_hp_tblank(settings, p[6]);
    tic_settings_set_hp_abt(settings, p[7] & 1);
    tic_settings_set_hp_tdecay(settings, p[8]);
    tic_settings_set_hp_decmod(settings, p[9] & 7);
  }
}

tic_error * tic_get_settings(tic_handle * handle, tic_settings ** settings)
{
  if (settings == NULL)
  {
    return tic_error_create("Settings output pointer is null.");
  }

  *settings = NULL;

  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  // Allocate the new settings object.
  tic_settings * new_settings = NULL;
  if (error == NULL)
  {
    error = tic_settings_create(&new_settings);
  }

  // Specify what type of device these settings are for.
  if (error == NULL)
  {
    const tic_device * device = tic_handle_get_device(handle);
    tic_settings_set_product(new_settings, tic_device_get_product(device));
    tic_settings_set_firmware_version(new_settings,
      tic_device_get_firmware_version(device));
  }

  // Read all the settings from the device.
  uint8_t product = tic_device_get_product(tic_handle_get_device(handle));
  tic_settings_segments segments = tic_get_settings_segments(product);
  uint8_t buf[256] = { 0 };
  if (error == NULL)
  {
    error = tic_get_setting_segment(handle,
      segments.general_offset, segments.general_size,
      buf + segments.general_offset);
  }

  if (error == NULL && segments.product_specific_size)
  {
    error = tic_get_setting_segment(handle,
      segments.product_specific_offset, segments.product_specific_size,
      buf + segments.product_specific_offset);
  }

  // Store the settings in the new settings object.
  if (error == NULL)
  {
    write_buffer_to_settings(buf, new_settings);
  }

  // Pass the new settings to the caller.
  if (error == NULL)
  {
    *settings = new_settings;
    new_settings = NULL;
  }

  tic_settings_free(new_settings);

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error reading settings from the device.");
  }

  return error;
}

tic_settings_segments tic_get_settings_segments(uint8_t product)
{
  tic_settings_segments segments = {
    .general_offset = 1,
    .general_size = (TIC_SETTING_SERIAL_ALT_DEVICE_NUMBER + 2) - 1
  };

  if (product == TIC_PRODUCT_T249)
  {
    segments.general_size = TIC_SETTING_AGC_FREQUENCY_LIMIT + 1;
  }

  if (product == TIC_PRODUCT_36V4)
  {
    segments.general_size =
      TIC_SETTING_HP_ENABLE_UNRESTRICTED_CURRENT_LIMITS + 1;
    segments.product_specific_offset = TIC_SETTING_HP_DRIVER_REGISTERS;
  }

  if (segments.product_specific_offset)
  {
    segments.product_specific_size = 256 - segments.product_specific_offset;
  }

  return segments;
}
