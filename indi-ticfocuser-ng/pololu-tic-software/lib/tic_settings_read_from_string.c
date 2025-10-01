// Functions for reading settings from a settings file string into memory.

#include "tic_internal.h"

static bool tic_parse_pin_config(const char * input,
  tic_settings * settings, uint8_t pin)
{
  assert(settings != NULL);

  tic_settings_set_pin_func(settings, pin, 0);
  tic_settings_set_pin_pullup(settings, pin, false);
  tic_settings_set_pin_analog(settings, pin, false);
  tic_settings_set_pin_polarity(settings, pin, false);

  char str[256];
  strcpy(str, input);

  char * p = str;
  while (true)
  {
    char * token = strtok(p, " ");
    p = NULL;
    if (token == NULL) { break; }

    if (0 == strcmp(token, "pullup"))
    {
      tic_settings_set_pin_pullup(settings, pin, true);
    }
    else if (0 == strcmp(token, "analog"))
    {
      tic_settings_set_pin_analog(settings, pin, true);
    }
    else if (0 == strcmp(token, "active_high"))
    {
      tic_settings_set_pin_polarity(settings, pin, true);
    }
    else
    {
      uint32_t pin_func;
      if (!tic_name_to_code(tic_pin_func_names, token, &pin_func))
      {
        return false;  // invalid token
      }
      tic_settings_set_pin_func(settings, pin, pin_func);
    }
  }
  return true;
}

// We apply the product name from the settings file first, and use it to set the
// defaults.
static tic_error * apply_product_name(tic_settings * settings, const char * product_name)
{
  uint32_t product;
  if (!tic_name_to_code(tic_product_names_short, product_name, &product))
  {
    return tic_error_create("Unrecognized product name.");
  }
  tic_settings_set_product(settings, product);
  tic_settings_fill_with_defaults(settings);
  return NULL;
}

// Note: The range checking we do in this function is solely to make sure the
// value will fit in the argument to the tic_settings setter function.  If the
// value is otherwise outside the allowed range, that will be checked in
// tic_settings_fix.
static tic_error * apply_string_pair(tic_settings * settings,
  const char * key, const char * value, uint32_t line)
{
  if (!strcmp(key, "product"))
  {
    // We already processed the product field separately.
  }
  else if (!strcmp(key, "control_mode"))
  {
    uint32_t control_mode;
    if (!tic_name_to_code(tic_control_mode_names, value, &control_mode))
    {
      return tic_error_create("Unrecognized control_mode value.");
    }
    tic_settings_set_control_mode(settings, control_mode);
  }
  else if (!strcmp(key, "never_sleep"))
  {
    uint32_t never_sleep;
    if (!tic_name_to_code(tic_bool_names, value, &never_sleep))
    {
      return tic_error_create("Unrecognized never_sleep value.");
    }
    tic_settings_set_never_sleep(settings, never_sleep);
  }
  else if (!strcmp(key, "disable_safe_start"))
  {
    uint32_t disable_safe_start;
    if (!tic_name_to_code(tic_bool_names, value, &disable_safe_start))
    {
      return tic_error_create("Unrecognized disable_safe_start value.");
    }
    tic_settings_set_disable_safe_start(settings, disable_safe_start);
  }
  else if (!strcmp(key, "ignore_err_line_high"))
  {
    uint32_t ignore_err_line_high;
    if (!tic_name_to_code(tic_bool_names, value, &ignore_err_line_high))
    {
      return tic_error_create("Unrecognized ignore_err_line_high value.");
    }
    tic_settings_set_ignore_err_line_high(settings, ignore_err_line_high);
  }
  else if (!strcmp(key, "auto_clear_driver_error"))
  {
    uint32_t auto_clear_driver_error;
    if (!tic_name_to_code(tic_bool_names, value, &auto_clear_driver_error))
    {
      return tic_error_create("Unrecognized auto_clear_driver_error value.");
    }
    tic_settings_set_auto_clear_driver_error(settings, auto_clear_driver_error);
  }
  else if (!strcmp(key, "soft_error_response"))
  {
    uint32_t response;
    if (!tic_name_to_code(tic_response_names, value, &response))
    {
      return tic_error_create("Unrecognized soft_error_response value.");
    }
    tic_settings_set_soft_error_response(settings, response);
  }
  else if (!strcmp(key, "soft_error_position"))
  {
    int64_t position;
    if (tic_string_to_i64(value, &position))
    {
      return tic_error_create("Invalid soft_error_position value.");
    }
    if (position < INT32_MIN || position > INT32_MAX)
    {
      return tic_error_create(
        "The soft_error_position value is out of range.");
    }
    tic_settings_set_soft_error_position(settings, position);
  }
  else if (!strcmp(key, "serial_baud_rate"))
  {
    int64_t baud;
    if (tic_string_to_i64(value, &baud))
    {
      return tic_error_create("Invalid serial_baud_rate value.");
    }
    if (baud < 0 || baud >= UINT32_MAX)
    {
      return tic_error_create("The serial_baud_rate value is out of range.");
    }
    tic_settings_set_serial_baud_rate(settings, baud);
  }
  else if (!strcmp(key, "serial_device_number"))
  {
    int64_t num;
    if (tic_string_to_i64(value, &num))
    {
      return tic_error_create("Invalid serial_device_number value.");
    }
    if (num < 0 || num > 0xFFFF)
    {
      return tic_error_create("The serial_device_number value is out of range.");
    }
    tic_settings_set_serial_device_number_u16(settings, num);
  }
  else if (!strcmp(key, "serial_alt_device_number"))
  {
    int64_t num;
    if (tic_string_to_i64(value, &num))
    {
      return tic_error_create("Invalid serial_alt_device_number value.");
    }
    if (num < 0 || num > 0xFFFF)
    {
      return tic_error_create(
        "The serial_alt_device_number value is out of range.");
    }
    tic_settings_set_serial_alt_device_number(settings, num);
  }
  else if (!strcmp(key, "serial_enable_alt_device_number"))
  {
    uint32_t enabled;
    if (!tic_name_to_code(tic_bool_names, value, &enabled))
    {
      return tic_error_create(
        "Unrecognized serial_enable_alt_device_number value.");
    }
    tic_settings_set_serial_enable_alt_device_number(settings, enabled);
  }
  else if (!strcmp(key, "serial_14bit_device_number"))
  {
    uint32_t enabled;
    if (!tic_name_to_code(tic_bool_names, value, &enabled))
    {
      return tic_error_create("Unrecognized serial_14bit_device_number value.");
    }
    tic_settings_set_serial_14bit_device_number(settings, enabled);
  }
  else if (!strcmp(key, "command_timeout"))
  {
    int64_t command_timeout;
    if (tic_string_to_i64(value, &command_timeout))
    {
      return tic_error_create("Invalid command_timeout value.");
    }
    if (command_timeout < 0 || command_timeout > 0xFFFF)
    {
      return tic_error_create("The command_timeout value is out of range.");
    }
    tic_settings_set_command_timeout(settings, command_timeout);
  }
  else if (!strcmp(key, "serial_crc_for_commands") ||
    !strcmp(key, "serial_crc_enabled"))
  {
    uint32_t enabled;
    if (!tic_name_to_code(tic_bool_names, value, &enabled))
    {
      return tic_error_create("Unrecognized serial_crc_for_commands value.");
    }
    tic_settings_set_serial_crc_for_commands(settings, enabled);
  }
  else if (!strcmp(key, "serial_crc_for_responses"))
  {
    uint32_t enabled;
    if (!tic_name_to_code(tic_bool_names, value, &enabled))
    {
      return tic_error_create("Unrecognized serial_crc_for_responses value.");
    }
    tic_settings_set_serial_crc_for_responses(settings, enabled);
  }
  else if (!strcmp(key, "serial_7bit_responses"))
  {
    uint32_t enabled;
    if (!tic_name_to_code(tic_bool_names, value, &enabled))
    {
      return tic_error_create("Unrecognized serial_7bit_responses value.");
    }
    tic_settings_set_serial_7bit_responses(settings, enabled);
  }
  else if (!strcmp(key, "serial_response_delay"))
  {
    int64_t delay;
    if (tic_string_to_i64(value, &delay))
    {
      return tic_error_create("Invalid serial_response_delay value.");
    }
    if (delay < 0 || delay > 0xFF)
    {
      return tic_error_create("The serial_response_delay value is out of range.");
    }
    tic_settings_set_serial_response_delay(settings, delay);
  }
  else if (!strcmp(key, "low_vin_timeout"))
  {
    int64_t low_vin_timeout;
    if (tic_string_to_i64(value, &low_vin_timeout))
    {
      return tic_error_create("Invalid low_vin_timeout value.");
    }
    if (low_vin_timeout < 0 || low_vin_timeout > 0xFFFF)
    {
      return tic_error_create("The low_vin_timeout value is out of range.");
    }
    tic_settings_set_low_vin_timeout(settings, low_vin_timeout);
  }
  else if (!strcmp(key, "low_vin_shutoff_voltage"))
  {
    int64_t low_vin_shutoff_voltage;
    if (tic_string_to_i64(value, &low_vin_shutoff_voltage))
    {
      return tic_error_create("Invalid low_vin_shutoff_voltage value.");
    }
    if (low_vin_shutoff_voltage < 0 || low_vin_shutoff_voltage > 0xFFFF)
    {
      return tic_error_create(
        "The low_vin_shutoff_voltage value is out of range.");
    }
    tic_settings_set_low_vin_shutoff_voltage(settings, low_vin_shutoff_voltage);
  }
  else if (!strcmp(key, "low_vin_startup_voltage"))
  {
    int64_t low_vin_startup_voltage;
    if (tic_string_to_i64(value, &low_vin_startup_voltage))
    {
      return tic_error_create("Invalid low_vin_startup_voltage value.");
    }
    if (low_vin_startup_voltage < 0 || low_vin_startup_voltage > 0xFFFF)
    {
      return tic_error_create(
        "The low_vin_startup_voltage value is out of range.");
    }
    tic_settings_set_low_vin_startup_voltage(settings, low_vin_startup_voltage);
  }
  else if (!strcmp(key, "high_vin_shutoff_voltage"))
  {
    int64_t high_vin_shutoff_voltage;
    if (tic_string_to_i64(value, &high_vin_shutoff_voltage))
    {
      return tic_error_create("Invalid high_vin_shutoff_voltage value.");
    }
    if (high_vin_shutoff_voltage < 0 || high_vin_shutoff_voltage > 0xFFFF)
    {
      return tic_error_create(
        "The high_vin_shutoff_voltage value is out of range.");
    }
    tic_settings_set_high_vin_shutoff_voltage(settings,
      high_vin_shutoff_voltage);
  }
  else if (!strcmp(key, "vin_calibration"))
  {
    int64_t vin_calibration;
    if (tic_string_to_i64(value, &vin_calibration))
    {
      return tic_error_create("Invalid vin_calibration value.");
    }
    if (vin_calibration < INT16_MIN || vin_calibration > INT16_MAX)
    {
      return tic_error_create(
        "The vin_calibration value is out of range.");
    }
    tic_settings_set_vin_calibration(settings, vin_calibration);
  }
  else if (!strcmp(key, "rc_max_pulse_period"))
  {
    int64_t rc_max_pulse_period;
    if (tic_string_to_i64(value, &rc_max_pulse_period))
    {
      return tic_error_create("Invalid rc_max_pulse_period value.");
    }
    if (rc_max_pulse_period < 0 || rc_max_pulse_period > 0xFFFF)
    {
      return tic_error_create(
        "The rc_max_pulse_period value is out of range.");
    }
    tic_settings_set_rc_max_pulse_period(settings, rc_max_pulse_period);
  }
  else if (!strcmp(key, "rc_bad_signal_timeout"))
  {
    int64_t rc_bad_signal_timeout;
    if (tic_string_to_i64(value, &rc_bad_signal_timeout))
    {
      return tic_error_create("Invalid rc_bad_signal_timeout value.");
    }
    if (rc_bad_signal_timeout < 0 || rc_bad_signal_timeout > 0xFFFF)
    {
      return tic_error_create(
        "The rc_bad_signal_timeout value is out of range.");
    }
    tic_settings_set_rc_bad_signal_timeout(settings, rc_bad_signal_timeout);
  }
  else if (!strcmp(key, "rc_consecutive_good_pulses"))
  {
    int64_t rc_consecutive_good_pulses;
    if (tic_string_to_i64(value, &rc_consecutive_good_pulses))
    {
      return tic_error_create("Invalid rc_consecutive_good_pulses value.");
    }
    if (rc_consecutive_good_pulses < 0 || rc_consecutive_good_pulses > 0xFF)
    {
      return tic_error_create(
        "The rc_consecutive_good_pulses value is out of range.");
    }
    tic_settings_set_rc_consecutive_good_pulses(settings,
      rc_consecutive_good_pulses);
  }
  else if (!strcmp(key, "input_averaging_enabled"))
  {
    uint32_t input_averaging_enabled;
    if (!tic_name_to_code(tic_bool_names, value, &input_averaging_enabled))
    {
      return tic_error_create("Unrecognized input_averaging_enabled value.");
    }
    tic_settings_set_input_averaging_enabled(settings, input_averaging_enabled);
  }
  else if (!strcmp(key, "input_hysteresis"))
  {
    int64_t hysteresis;
    if (tic_string_to_i64(value, &hysteresis))
    {
      return tic_error_create("Invalid input_hysteresis value.");
    }
    if (hysteresis < 0 || hysteresis > 0xFFFF)
    {
      return tic_error_create("The input_hysteresis value is out of range.");
    }
    tic_settings_set_input_hysteresis(settings, hysteresis);
  }
  else if (!strcmp(key, "input_error_min"))
  {
    int64_t input_error_min;
    if (tic_string_to_i64(value, &input_error_min))
    {
      return tic_error_create("Invalid input_error_min value.");
    }
    if (input_error_min < 0 || input_error_min > 0xFFFF)
    {
      return tic_error_create("The input_error_min value is out of range.");
    }
    tic_settings_set_input_error_min(settings, input_error_min);
  }
  else if (!strcmp(key, "input_error_max"))
  {
    int64_t input_error_max;
    if (tic_string_to_i64(value, &input_error_max))
    {
      return tic_error_create("Invalid input_error_max value.");
    }
    if (input_error_max < 0 || input_error_max > 0xFFFF)
    {
      return tic_error_create("The input_error_max value is out of range.");
    }
    tic_settings_set_input_error_max(settings, input_error_max);
  }
  else if (!strcmp(key, "input_scaling_degree"))
  {
    uint32_t input_scaling_degree;
    if (!tic_name_to_code(tic_scaling_degree_names, value, &input_scaling_degree))
    {
      return tic_error_create("Unrecognized input_scaling_degree value.");
    }
    tic_settings_set_input_scaling_degree(settings, input_scaling_degree);
  }
  else if (!strcmp(key, "input_invert"))
  {
    uint32_t input_invert;
    if (!tic_name_to_code(tic_bool_names, value, &input_invert))
    {
      return tic_error_create("Unrecognized input_invert value.");
    }
    tic_settings_set_input_invert(settings, input_invert);
  }
  else if (!strcmp(key, "input_min"))
  {
    int64_t input_min;
    if (tic_string_to_i64(value, &input_min))
    {
      return tic_error_create("Invalid input_min value.");
    }
    if (input_min < 0 || input_min > 0xFFFF)
    {
      return tic_error_create("The input_min value is out of range.");
    }
    tic_settings_set_input_min(settings, input_min);
  }
  else if (!strcmp(key, "input_neutral_min"))
  {
    int64_t input_neutral_min;
    if (tic_string_to_i64(value, &input_neutral_min))
    {
      return tic_error_create("Invalid input_neutral_min value.");
    }
    if (input_neutral_min < 0 || input_neutral_min > 0xFFFF)
    {
      return tic_error_create("The input_neutral_min value is out of range.");
    }
    tic_settings_set_input_neutral_min(settings, input_neutral_min);
  }
  else if (!strcmp(key, "input_neutral_max"))
  {
    int64_t input_neutral_max;
    if (tic_string_to_i64(value, &input_neutral_max))
    {
      return tic_error_create("Invalid input_neutral_max value.");
    }
    if (input_neutral_max < 0 || input_neutral_max > 0xFFFF)
    {
      return tic_error_create("The input_neutral_max value is out of range.");
    }
    tic_settings_set_input_neutral_max(settings, input_neutral_max);
  }
  else if (!strcmp(key, "input_max"))
  {
    int64_t input_max;
    if (tic_string_to_i64(value, &input_max))
    {
      return tic_error_create("Invalid input_max value.");
    }
    if (input_max < 0 || input_max > 0xFFFF)
    {
      return tic_error_create("The input_max value is out of range.");
    }
    tic_settings_set_input_max(settings, input_max);
  }
  else if (!strcmp(key, "output_min"))
  {
    int64_t output_min;
    if (tic_string_to_i64(value, &output_min))
    {
      return tic_error_create("Invalid output_min value.");
    }
    if (output_min < INT32_MIN || output_min > INT32_MAX)
    {
      return tic_error_create("The output_min value is out of range.");
    }
    tic_settings_set_output_min(settings, output_min);
  }
  else if (!strcmp(key, "output_max"))
  {
    int64_t output_max;
    if (tic_string_to_i64(value, &output_max))
    {
      return tic_error_create("Invalid output_max value.");
    }
    if (output_max < INT32_MIN || output_max > INT32_MAX)
    {
      return tic_error_create("The output_max value is out of range.");
    }
    tic_settings_set_output_max(settings, output_max);
  }
  else if (!strcmp(key, "encoder_prescaler"))
  {
    int64_t encoder_prescaler;
    if (tic_string_to_i64(value, &encoder_prescaler))
    {
      return tic_error_create("Invalid encoder_prescaler value.");
    }
    if (encoder_prescaler < 0 || encoder_prescaler > UINT32_MAX)
    {
      return tic_error_create("The encoder_prescaler value is out of range.");
    }
    tic_settings_set_encoder_prescaler(settings, encoder_prescaler);
  }
  else if (!strcmp(key, "encoder_postscaler"))
  {
    int64_t encoder_postscaler;
    if (tic_string_to_i64(value, &encoder_postscaler))
    {
      return tic_error_create("Invalid encoder_postscaler value.");
    }
    if (encoder_postscaler < 0 || encoder_postscaler > UINT32_MAX)
    {
      return tic_error_create("The encoder_postscaler value is out of range.");
    }
    tic_settings_set_encoder_postscaler(settings, encoder_postscaler);
  }
  else if (!strcmp(key, "encoder_unlimited"))
  {
    uint32_t encoder_unlimited;
    if (!tic_name_to_code(tic_bool_names, value, &encoder_unlimited))
    {
      return tic_error_create("Unrecognized encoder_unlimited value.");
    }
    tic_settings_set_encoder_unlimited(settings, encoder_unlimited);
  }
  else if (!strcmp(key, "scl_config"))
  {
    if (!tic_parse_pin_config(value, settings, TIC_PIN_NUM_SCL))
    {
      return tic_error_create("Invalid scl_config value.");
    }
  }
  else if (!strcmp(key, "sda_config"))
  {
    if (!tic_parse_pin_config(value, settings, TIC_PIN_NUM_SDA))
    {
      return tic_error_create("Invalid sda_config value.");
    }
  }
  else if (!strcmp(key, "tx_config"))
  {
    if (!tic_parse_pin_config(value, settings, TIC_PIN_NUM_TX))
    {
      return tic_error_create("Invalid tx_config value.");
    }
  }
  else if (!strcmp(key, "rx_config"))
  {
    if (!tic_parse_pin_config(value, settings, TIC_PIN_NUM_RX))
    {
      return tic_error_create("Invalid rx_config value.");
    }
  }
  else if (!strcmp(key, "rc_config"))
  {
    if (!tic_parse_pin_config(value, settings, TIC_PIN_NUM_RC))
    {
      return tic_error_create("Invalid rc_config value.");
    }
  }
  else if (!strcmp(key, "current_limit"))
  {
    int64_t current_limit;
    if (tic_string_to_i64(value, &current_limit))
    {
      return tic_error_create("Invalid current_limit value.");
    }
    if (current_limit < 0 || current_limit > UINT32_MAX)
    {
      return tic_error_create("The current_limit value is out of range.");
    }
    tic_settings_set_current_limit(settings, current_limit);
  }
  else if (!strcmp(key, "current_limit_during_error"))
  {
    int64_t current_limit;
    if (tic_string_to_i64(value, &current_limit))
    {
      return tic_error_create("Invalid current_limit_during_error value.");
    }
    if (current_limit < INT32_MIN || current_limit > INT32_MAX)
    {
      return tic_error_create("The current_limit_during_error value is out of range.");
    }
    tic_settings_set_current_limit_during_error(settings, current_limit);
  }
  else if (!strcmp(key, "step_mode"))
  {
    uint32_t step_mode;
    if (!tic_name_to_code(tic_step_mode_names, value, &step_mode))
    {
      return tic_error_create("Invalid step_mode value.");
    }
    tic_settings_set_step_mode(settings, step_mode);
  }
  else if (!strcmp(key, "decay_mode"))
  {
    uint8_t decay_mode;
    if (!tic_look_up_decay_mode_code(value, 0, TIC_NAME_SNAKE_CASE, &decay_mode))
    {
      return tic_error_create("Invalid decay_mode value.");
    }
    tic_settings_set_decay_mode(settings, decay_mode);
  }
  else if (!strcmp(key, "max_speed"))
  {
    int64_t max_speed;
    if (tic_string_to_i64(value, &max_speed))
    {
      return tic_error_create("Invalid max_speed value.");
    }
    if (max_speed < 0 || max_speed > UINT32_MAX)
    {
      return tic_error_create("The max_speed value is out of range.");
    }
    tic_settings_set_max_speed(settings, max_speed);
  }
  else if (!strcmp(key, "starting_speed"))
  {
    int64_t starting_speed;
    if (tic_string_to_i64(value, &starting_speed))
    {
      return tic_error_create("Invalid starting_speed value.");
    }
    if (starting_speed < 0 || starting_speed > UINT32_MAX)
    {
      return tic_error_create("The starting_speed value is out of range.");
    }
    tic_settings_set_starting_speed(settings, starting_speed);
  }
  else if (!strcmp(key, "max_accel"))
  {
    int64_t max_accel;
    if (tic_string_to_i64(value, &max_accel))
    {
      return tic_error_create("Invalid max_accel value.");
    }
    if (max_accel < 0 || max_accel > UINT32_MAX)
    {
      return tic_error_create("The max_accel value is out of range.");
    }
    tic_settings_set_max_accel(settings, max_accel);
  }
  else if (!strcmp(key, "max_decel"))
  {
    int64_t max_decel;
    if (tic_string_to_i64(value, &max_decel))
    {
      return tic_error_create("Invalid max_decel value.");
    }
    if (max_decel < 0 || max_decel > UINT32_MAX)
    {
      return tic_error_create("The max_decel value is out of range.");
    }
    tic_settings_set_max_decel(settings, max_decel);
  }
  else if (!strcmp(key, "auto_homing"))
  {
    uint32_t auto_homing;
    if (!tic_name_to_code(tic_bool_names, value, &auto_homing))
    {
      return tic_error_create("Unrecognized auto_homing value.");
    }
    tic_settings_set_auto_homing(settings, auto_homing);
  }
  else if (!strcmp(key, "auto_homing_forward"))
  {
    uint32_t forward;
    if (!tic_name_to_code(tic_bool_names, value, &forward))
    {
      return tic_error_create("Unrecognized auto_homing_forward value.");
    }
    tic_settings_set_auto_homing_forward(settings, forward);
  }
  else if (!strcmp(key, "homing_speed_towards"))
  {
    int64_t speed;
    if (tic_string_to_i64(value, &speed))
    {
      return tic_error_create("Invalid homing_speed_towards value.");
    }
    if (speed < 0 || speed > UINT32_MAX)
    {
      return tic_error_create(
        "The homing_speed_towards value is out of range.");
    }
    tic_settings_set_homing_speed_towards(settings, speed);
  }
  else if (!strcmp(key, "homing_speed_away"))
  {
    int64_t speed;
    if (tic_string_to_i64(value, &speed))
    {
      return tic_error_create("Invalid homing_speed_away value.");
    }
    if (speed < 0 || speed > UINT32_MAX)
    {
      return tic_error_create("The homing_speed_away value is out of range.");
    }
    tic_settings_set_homing_speed_away(settings, speed);
  }
  else if (!strcmp(key, "invert_motor_direction"))
  {
    uint32_t invert;
    if (!tic_name_to_code(tic_bool_names, value, &invert))
    {
      return tic_error_create("Unrecognized invert_motor_direction value.");
    }
    tic_settings_set_invert_motor_direction(settings, invert);
  }
  else if (!strcmp(key, "agc_mode"))
  {
    uint32_t mode;
    if (!tic_name_to_code(tic_agc_mode_names, value, &mode))
    {
      return tic_error_create("Invalid agc_mode value.");
    }
    tic_settings_set_agc_mode(settings, mode);
  }
  else if (!strcmp(key, "agc_bottom_current_limit"))
  {
    uint32_t limit;
    if (!tic_name_to_code(tic_agc_bottom_current_limit_names, value, &limit))
    {
      return tic_error_create("Invalid agc_bottom_current_limit value.");
    }
    tic_settings_set_agc_bottom_current_limit(settings, limit);
  }
  else if (!strcmp(key, "agc_current_boost_steps"))
  {
    uint32_t steps;
    if (!tic_name_to_code(tic_agc_current_boost_steps_names, value, &steps))
    {
      return tic_error_create("Invalid agc_current_boost_steps value.");
    }
    tic_settings_set_agc_current_boost_steps(settings, steps);
  }
  else if (!strcmp(key, "agc_frequency_limit"))
  {
    uint32_t limit;
    if (!tic_name_to_code(tic_agc_frequency_limit_names, value, &limit))
    {
      return tic_error_create("Invalid agc_frequency_limit value.");
    }
    tic_settings_set_agc_frequency_limit(settings, limit);
  }
  else if (!strcmp(key, "hp_enable_unrestricted_current_limits"))
  {
    uint32_t enable;
    if (!tic_name_to_code(tic_bool_names, value, &enable))
    {
      return tic_error_create(
        "Unrecognized hp_enable_unrestricted_current_limits value.");
    }
    tic_settings_set_hp_enable_unrestricted_current_limits(settings, enable);
  }
  else if (!strcmp(key, "hp_toff"))
  {
    int64_t time;
    if (tic_string_to_i64(value, &time))
    {
      return tic_error_create("Invalid hp_toff value.");
    }
    if (time < 0 || time > UINT8_MAX)
    {
      return tic_error_create("The hp_toff value is out of range.");
    }
    tic_settings_set_hp_toff(settings, time);
  }
  else if (!strcmp(key, "hp_tblank"))
  {
    int64_t time;
    if (tic_string_to_i64(value, &time))
    {
      return tic_error_create("Invalid hp_tblank value.");
    }
    if (time < 0 || time > UINT8_MAX)
    {
      return tic_error_create("The hp_tblank value is out of range.");
    }
    tic_settings_set_hp_tblank(settings, time);
  }
  else if (!strcmp(key, "hp_abt"))
  {
    uint32_t adaptive;
    if (!tic_name_to_code(tic_bool_names, value, &adaptive))
    {
      return tic_error_create("Unrecognized hp_abt value.");
    }
    tic_settings_set_hp_abt(settings, adaptive);
  }
  else if (!strcmp(key, "hp_tdecay"))
  {
    int64_t time;
    if (tic_string_to_i64(value, &time))
    {
      return tic_error_create("Invalid hp_tdecay value.");
    }
    if (time < 0 || time > UINT8_MAX)
    {
      return tic_error_create("The hp_tdecay value is out of range.");
    }
    tic_settings_set_hp_tdecay(settings, time);
  }
  else if (!strcmp(key, "hp_decmod"))
  {
    uint32_t code;
    if (!tic_name_to_code(tic_hp_decmod_names_snake, value, &code))
    {
      return tic_error_create("Invalid hp_decmod value.");
    }
    tic_settings_set_hp_decmod(settings, code);
  }
  else
  {
    return tic_error_create("Unrecognized key on line %d: \"%s\".", line, key);
  }

  return NULL;
}

// Returns true if the node is a scalar and its value equals the given
// null-terminated string.
static bool scalar_eq(const yaml_node_t * node, const char * v)
{
  if (node == NULL) { return NULL; }
  if (node->type != YAML_SCALAR_NODE) { return false; }
  if (node->data.scalar.length != strlen(v)) { return false; }
  return !memcmp(v, node->data.scalar.value, node->data.scalar.length);
}

// Given a mapping and a key name (as a null-terminated string), gets the node
// corresponding to the value, or NULL if it could not be found.
static yaml_node_t * map_lookup(yaml_document_t * doc,
  yaml_node_t * map, const char * key_name)
{
  if (doc == NULL) { assert(0); return NULL; }
  if (map == NULL) { assert(0); return NULL; }
  if (map->type != YAML_MAPPING_NODE) { return NULL; }

  for (yaml_node_pair_t * pair = map->data.mapping.pairs.start;
       pair < map->data.mapping.pairs.top; pair++)
  {
    yaml_node_t * key = yaml_document_get_node(doc, pair->key);
    if (scalar_eq(key, key_name))
    {
      return yaml_document_get_node(doc, pair->value);
    }
  }
  return NULL;
}

#define MAX_SCALAR_LENGTH 255

// Takes a key-value pair from the YAML file, does some basic checks, creates
// proper null-terminated C strings, and then calls apply_string_pair to do the
// actual logic of parsing strings and applying the settings.
static tic_error * apply_yaml_pair(tic_settings * settings,
  const yaml_node_t * key, const yaml_node_t * value)
{
  if (key == NULL)
  {
    return tic_error_create("Internal YAML processing error: Invalid key index.");
  }
  if (value == NULL)
  {
    return tic_error_create("Internal YAML processing error: Invalid value index.");
  }

  uint32_t line = key->start_mark.line + 1;

  // Make sure the key is valid and convert it to a C string (we aren't sure
  // that libyaml always provides a null termination byte because scalars can
  // have have null bytes in them).
  if (key->type != YAML_SCALAR_NODE)
  {
    return tic_error_create(
      "YAML key is not a scalar on line %d.", line);
  }
  if (key->data.scalar.length > MAX_SCALAR_LENGTH)
  {
    return tic_error_create(
      "YAML key is too long on line %d.", line);
  }
  char key_str[MAX_SCALAR_LENGTH + 1];
  memcpy(key_str, key->data.scalar.value, key->data.scalar.length);
  key_str[key->data.scalar.length] = 0;

  // Make sure the value is valid and convert it to a C string.
  if (value->type != YAML_SCALAR_NODE)
  {
    return tic_error_create(
      "YAML value is not a scalar on line %d.", line);
  }
  if (value->data.scalar.length > MAX_SCALAR_LENGTH)
  {
    return tic_error_create(
      "YAML value is too long on line %d.", line);
  }
  char value_str[MAX_SCALAR_LENGTH + 1];
  memcpy(value_str, value->data.scalar.value, value->data.scalar.length);
  value_str[value->data.scalar.length] = 0;

  return apply_string_pair(settings, key_str, value_str, line);
}

// Validates the YAML doc and populates the settings object with the settings
// from the document.
static tic_error * read_from_yaml_doc(
  yaml_document_t * doc, tic_settings * settings)
{
  assert(doc != NULL);
  assert(settings != NULL);

  // Get the root node and make sure it is a mapping.
  yaml_node_t * root = yaml_document_get_root_node(doc);
  if (root->type != YAML_MAPPING_NODE)
  {
    return tic_error_create("YAML root node is not a mapping.");
  }

  // Process the "product" key/value pair first.
  yaml_node_t * product_value = map_lookup(doc, root, "product");
  if (product_value == NULL)
  {
    return tic_error_create("No product was specified in the settings file.");
  }
  uint32_t product_line = product_value->start_mark.line + 1;
  if (product_value->type != YAML_SCALAR_NODE)
  {
    return tic_error_create(
      "YAML product value is not a scalar on line %d.", product_line);
  }
  if (product_value->data.scalar.length > MAX_SCALAR_LENGTH)
  {
    return tic_error_create(
      "YAML product value is too long on line %d.", product_line);
  }
  char product_str[MAX_SCALAR_LENGTH + 1];
  memcpy(product_str, product_value->data.scalar.value,
    product_value->data.scalar.length);
  product_str[product_value->data.scalar.length] = 0;
  tic_error * error;
  error = apply_product_name(settings, product_str);
  if (error) { return error; }

  // Iterate over the pairs in the YAML mapping and process each one.
  for (yaml_node_pair_t * pair = root->data.mapping.pairs.start;
       pair < root->data.mapping.pairs.top; pair++)
  {
    yaml_node_t * key = yaml_document_get_node(doc, pair->key);
    yaml_node_t * value = yaml_document_get_node(doc, pair->value);
    tic_error * error = apply_yaml_pair(settings, key, value);
    if (error) { return error; }
  }

  return NULL;
}

tic_error * tic_settings_read_from_string(const char * string,
  tic_settings ** settings)
{
  if (string == NULL)
  {
    return tic_error_create("Settings input string is null.");
  }

  if (settings == NULL)
  {
    return tic_error_create("Settings output pointer is null.");
  }

  tic_error * error = NULL;

  // Allocate a new settings object.
  tic_settings * new_settings = NULL;
  if (error == NULL)
  {
    error = tic_settings_create(&new_settings);
  }

  // Make a YAML parser.
  bool parser_initialized = false;
  yaml_parser_t parser;
  if (error == NULL)
  {
    int success = yaml_parser_initialize(&parser);
    if (success)
    {
      parser_initialized = true;
    }
    else
    {
      error = tic_error_create("Failed to initialize YAML parser.");
    }
  }

  // Construct a YAML document using the parser.
  bool document_initialized = false;
  yaml_document_t doc;
  if (error == NULL)
  {
    yaml_parser_set_input_string(&parser, (const uint8_t *)string, strlen(string));
    int success = yaml_parser_load(&parser, &doc);
    if (success)
    {
      document_initialized = true;
    }
    else
    {
      error = tic_error_create("Failed to load document: %s at line %u.",
        parser.problem, (unsigned int)parser.problem_mark.line + 1);
    }
  }

  // Proces the YAML document.
  if (error == NULL)
  {
    error = read_from_yaml_doc(&doc, new_settings);
  }

  // Success!  Pass the settings to the caller.
  if (error == NULL)
  {
    *settings = new_settings;
    new_settings = NULL;
  }

  if (document_initialized)
  {
    yaml_document_delete(&doc);
  }

  if (parser_initialized)
  {
    yaml_parser_delete(&parser);
  }

  tic_settings_free(new_settings);

  if (error != NULL)
  {
    error = tic_error_add(error, "There was an error reading the settings file.");
  }

  return error;
}
