// Functions for converting settings to a settings file string.

#include "tic_internal.h"

static void print_pin_config_to_yaml(tic_string * str,
  const tic_settings * settings, uint8_t pin,  const char * config_name)
{
  assert(str != NULL);
  assert(config_name != NULL);

  const char * pullup_str = "";
  if (tic_settings_get_pin_pullup(settings, pin)) { pullup_str = " pullup"; }

  const char * analog_str = "";
  if (tic_settings_get_pin_analog(settings, pin)) { analog_str = " analog"; }

  const char * polarity_str = "";
  if (tic_settings_get_pin_polarity(settings, pin)) { polarity_str = " active_high"; }

  const char * func_str = "";
  tic_code_to_name(tic_pin_func_names,
    tic_settings_get_pin_func(settings, pin), &func_str);

  tic_sprintf(str, "%s: %s%s%s%s\n", config_name, func_str,
    pullup_str, analog_str, polarity_str);
}

tic_error * tic_settings_to_string(const tic_settings * settings, char ** string)
{
  if (string == NULL)
  {
    return tic_error_create("String output pointer is null.");
  }

  *string = NULL;

  if (settings == NULL)
  {
    return tic_error_create("Settings pointer is null.");
  }

  tic_error * error = NULL;

  tic_string str;
  tic_string_setup(&str);

  tic_sprintf(&str, "# Pololu Tic USB Stepper Controller settings file.\n");
  tic_sprintf(&str, "# " DOCUMENTATION_URL "\n");

  uint8_t product = tic_settings_get_product(settings);

  {
    const char * product_str = tic_look_up_product_name_short(product);
    tic_sprintf(&str, "product: %s\n", product_str);
  }

  {
    uint8_t control_mode = tic_settings_get_control_mode(settings);
    const char * mode_str = "";
    tic_code_to_name(tic_control_mode_names, control_mode, &mode_str);
    tic_sprintf(&str, "control_mode: %s\n", mode_str);
  }

  {
    bool never_sleep = tic_settings_get_never_sleep(settings);
    tic_sprintf(&str, "never_sleep: %s\n", never_sleep ? "true" : "false");
  }

  {
    bool disable_safe_start = tic_settings_get_disable_safe_start(settings);
    tic_sprintf(&str, "disable_safe_start: %s\n",
      disable_safe_start ? "true" : "false");
  }

  {
    bool ignore_err_line_high = tic_settings_get_ignore_err_line_high(settings);
    tic_sprintf(&str, "ignore_err_line_high: %s\n",
      ignore_err_line_high ? "true" : "false");
  }

  {
    bool auto_clear = tic_settings_get_auto_clear_driver_error(settings);
    tic_sprintf(&str, "auto_clear_driver_error: %s\n",
      auto_clear ? "true" : "false");
  }

  {
    uint8_t response = tic_settings_get_soft_error_response(settings);
    const char * response_str = "";
    tic_code_to_name(tic_response_names, response, &response_str);
    tic_sprintf(&str, "soft_error_response: %s\n", response_str);
  }

  {
    int32_t position = tic_settings_get_soft_error_position(settings);
    tic_sprintf(&str, "soft_error_position: %d\n", position);
  }

  {
    uint32_t baud = tic_settings_get_serial_baud_rate(settings);
    tic_sprintf(&str, "serial_baud_rate: %u\n", baud);
  }

  {
    uint16_t number = tic_settings_get_serial_device_number_u16(settings);
    tic_sprintf(&str, "serial_device_number: %u\n", number);
  }

  {
    uint16_t number = tic_settings_get_serial_alt_device_number(settings);
    tic_sprintf(&str, "serial_alt_device_number: %u\n", number);
  }

  {
    bool enabled = tic_settings_get_serial_enable_alt_device_number(settings);
    tic_sprintf(&str, "serial_enable_alt_device_number: %s\n",
      enabled ? "true" : "false");
  }

  {
    bool enabled = tic_settings_get_serial_14bit_device_number(settings);
    tic_sprintf(&str, "serial_14bit_device_number: %s\n",
      enabled ? "true" : "false");
  }

  {
    uint16_t command_timeout = tic_settings_get_command_timeout(settings);
    tic_sprintf(&str, "command_timeout: %u\n", command_timeout);
  }

  {
    bool enabled = tic_settings_get_serial_crc_for_commands(settings);
    tic_sprintf(&str, "serial_crc_for_commands: %s\n",
      enabled ? "true" : "false");
  }

  {
    bool enabled = tic_settings_get_serial_crc_for_responses(settings);
    tic_sprintf(&str, "serial_crc_for_responses: %s\n",
      enabled ? "true" : "false");
  }

  {
    bool enabled = tic_settings_get_serial_7bit_responses(settings);
    tic_sprintf(&str, "serial_7bit_responses: %s\n",
      enabled ? "true" : "false");
  }

  {
    uint8_t delay = tic_settings_get_serial_response_delay(settings);
    tic_sprintf(&str, "serial_response_delay: %u\n", delay);
  }

  if (0) // not implemented in firmware
  {
    uint16_t low_vin_timeout = tic_settings_get_low_vin_timeout(settings);
    tic_sprintf(&str, "low_vin_timeout: %u\n", low_vin_timeout);
  }

  if (0) // not implemented in firmware
  {
    uint16_t voltage = tic_settings_get_low_vin_shutoff_voltage(settings);
    tic_sprintf(&str, "low_vin_shutoff_voltage: %u\n", voltage);
  }

  if (0) // not implemented in firmware
  {
    uint16_t voltage = tic_settings_get_low_vin_startup_voltage(settings);
    tic_sprintf(&str, "low_vin_startup_voltage: %u\n", voltage);
  }

  if (0) // not implemented in firmware
  {
    uint16_t voltage = tic_settings_get_high_vin_shutoff_voltage(settings);
    tic_sprintf(&str, "high_vin_shutoff_voltage: %u\n", voltage);
  }

  {
    int16_t offset = tic_settings_get_vin_calibration(settings);
    tic_sprintf(&str, "vin_calibration: %d\n", offset);
  }

  if (0) // not implemented in firmware
  {
    uint16_t pulse_period = tic_settings_get_rc_max_pulse_period(settings);
    tic_sprintf(&str, "rc_max_pulse_period: %u\n", pulse_period);
  }

  if (0) // not implemented in firmware
  {
    uint16_t timeout = tic_settings_get_rc_bad_signal_timeout(settings);
    tic_sprintf(&str, "rc_bad_signal_timeout: %u\n", timeout);
  }

  if (0) // not implemented in firmware
  {
    uint16_t pulses = tic_settings_get_rc_consecutive_good_pulses(settings);
    tic_sprintf(&str, "rc_consecutive_good_pulses: %u\n", pulses);
  }

  {
    bool enabled = tic_settings_get_input_averaging_enabled(settings);
    tic_sprintf(&str, "input_averaging_enabled: %s\n",
      enabled ? "true" : "false");
  }

  {
    uint16_t input_hysteresis = tic_settings_get_input_hysteresis(settings);
    tic_sprintf(&str, "input_hysteresis: %u\n", input_hysteresis);
  }

  if (0) // not implemented in firmware
  {
    uint16_t input_error_min = tic_settings_get_input_error_min(settings);
    tic_sprintf(&str, "input_error_min: %u\n", input_error_min);
  }

  if (0) // not implemented in firmware
  {
    uint16_t input_error_max = tic_settings_get_input_error_max(settings);
    tic_sprintf(&str, "input_error_max: %u\n", input_error_max);
  }

  {
    uint8_t degree = tic_settings_get_input_scaling_degree(settings);
    const char * degree_str = "";
    tic_code_to_name(tic_scaling_degree_names, degree, &degree_str);
    tic_sprintf(&str, "input_scaling_degree: %s\n", degree_str);
  }

  {
    bool input_invert = tic_settings_get_input_invert(settings);
    tic_sprintf(&str, "input_invert: %s\n", input_invert ? "true" : "false");
  }

  {
    uint16_t input_min = tic_settings_get_input_min(settings);
    tic_sprintf(&str, "input_min: %u\n", input_min);
  }

  {
    uint16_t input_neutral_min = tic_settings_get_input_neutral_min(settings);
    tic_sprintf(&str, "input_neutral_min: %u\n", input_neutral_min);
  }

  {
    uint16_t input_neutral_max = tic_settings_get_input_neutral_max(settings);
    tic_sprintf(&str, "input_neutral_max: %u\n", input_neutral_max);
  }

  {
    uint16_t input_max = tic_settings_get_input_max(settings);
    tic_sprintf(&str, "input_max: %u\n", input_max);
  }

  {
    int32_t output = tic_settings_get_output_min(settings);
    tic_sprintf(&str, "output_min: %d\n", output);
  }

  {
    int32_t output = tic_settings_get_output_max(settings);
    tic_sprintf(&str, "output_max: %d\n", output);
  }

  {
    uint32_t encoder_prescaler = tic_settings_get_encoder_prescaler(settings);
    tic_sprintf(&str, "encoder_prescaler: %u\n", encoder_prescaler);
  }

  {
    uint32_t encoder_postscaler = tic_settings_get_encoder_postscaler(settings);
    tic_sprintf(&str, "encoder_postscaler: %u\n", encoder_postscaler);
  }

  {
    bool encoder_unlimited = tic_settings_get_encoder_unlimited(settings);
    tic_sprintf(&str, "encoder_unlimited: %s\n", encoder_unlimited ? "true" : "false");
  }

  {
    print_pin_config_to_yaml(&str, settings, TIC_PIN_NUM_SCL, "scl_config");
    print_pin_config_to_yaml(&str, settings, TIC_PIN_NUM_SDA, "sda_config");
    print_pin_config_to_yaml(&str, settings, TIC_PIN_NUM_TX, "tx_config");
    print_pin_config_to_yaml(&str, settings, TIC_PIN_NUM_RX, "rx_config");
    print_pin_config_to_yaml(&str, settings, TIC_PIN_NUM_RC, "rc_config");
  }

  {
    bool invert = tic_settings_get_invert_motor_direction(settings);
    tic_sprintf(&str, "invert_motor_direction: %s\n",
      invert ? "true" : "false");
  }

  {
    uint32_t max_speed = tic_settings_get_max_speed(settings);
    tic_sprintf(&str, "max_speed: %u\n", max_speed);
  }

  {
    uint32_t starting_speed = tic_settings_get_starting_speed(settings);
    tic_sprintf(&str, "starting_speed: %u\n", starting_speed);
  }

  {
    uint32_t accel = tic_settings_get_max_accel(settings);
    tic_sprintf(&str, "max_accel: %u\n", accel);
  }

  {
    uint32_t decel = tic_settings_get_max_decel(settings);
    tic_sprintf(&str, "max_decel: %u\n", decel);
  }

  {
    uint8_t mode = tic_settings_get_step_mode(settings);
    const char * name = "";
    tic_code_to_name(tic_step_mode_names, mode, &name);
    tic_sprintf(&str, "step_mode: %s\n", name);
  }

  {
    uint32_t current = tic_settings_get_current_limit(settings);
    tic_sprintf(&str, "current_limit: %u\n", current);
  }

  {
    int32_t current = tic_settings_get_current_limit_during_error(settings);
    tic_sprintf(&str, "current_limit_during_error: %d\n", current);
  }

  // The decay mode setting for the Tic T500 and T249 is useless because there
  // is only one allowed value, so don't write it to the settings file.
  if (product == TIC_PRODUCT_T825 ||
    product == TIC_PRODUCT_N825 ||
    product == TIC_PRODUCT_T834)
  {
    uint8_t mode = tic_settings_get_decay_mode(settings);
    const char * name;
    tic_look_up_decay_mode_name(mode, product, TIC_NAME_SNAKE_CASE, &name);
    tic_sprintf(&str, "decay_mode: %s\n", name);
  }

  {
    bool auto_homing = tic_settings_get_auto_homing(settings);
    tic_sprintf(&str, "auto_homing: %s\n", auto_homing ? "true" : "false");
  }

  {
    bool forward = tic_settings_get_auto_homing_forward(settings);
    tic_sprintf(&str, "auto_homing_forward: %s\n", forward ? "true" : "false");
  }

  {
    uint32_t speed = tic_settings_get_homing_speed_towards(settings);
    tic_sprintf(&str, "homing_speed_towards: %u\n", speed);
  }

  {
    uint32_t speed = tic_settings_get_homing_speed_away(settings);
    tic_sprintf(&str, "homing_speed_away: %u\n", speed);
  }

  if (product == TIC_PRODUCT_T249)
  {
    uint8_t mode = tic_settings_get_agc_mode(settings);
    const char * name;
    tic_code_to_name(tic_agc_mode_names, mode, &name);
    tic_sprintf(&str, "agc_mode: %s\n", name);
  }

  if (product == TIC_PRODUCT_T249)
  {
    uint8_t limit = tic_settings_get_agc_bottom_current_limit(settings);
    const char * name;
    tic_code_to_name(tic_agc_bottom_current_limit_names, limit, &name);
    tic_sprintf(&str, "agc_bottom_current_limit: %s\n", name);
  }

  if (product == TIC_PRODUCT_T249)
  {
    uint8_t steps = tic_settings_get_agc_current_boost_steps(settings);
    const char * name;
    tic_code_to_name(tic_agc_current_boost_steps_names, steps, &name);
    tic_sprintf(&str, "agc_current_boost_steps: %s\n", name);
  }

  if (product == TIC_PRODUCT_T249)
  {
    uint8_t mode = tic_settings_get_agc_frequency_limit(settings);
    const char * name;
    tic_code_to_name(tic_agc_frequency_limit_names, mode, &name);
    tic_sprintf(&str, "agc_frequency_limit: %s\n", name);
  }

  bool hp = product == TIC_PRODUCT_36V4;

  if (hp)
  {
    bool enable =
      tic_settings_get_hp_enable_unrestricted_current_limits(settings);
    tic_sprintf(&str, "hp_enable_unrestricted_current_limits: %s\n",
      enable ? "true" : "false");
  }

  if (hp)
  {
    uint8_t time = tic_settings_get_hp_toff(settings);
    tic_sprintf(&str, "hp_toff: %d\n", time);
  }

  if (hp)
  {
    uint8_t time = tic_settings_get_hp_tblank(settings);
    tic_sprintf(&str, "hp_tblank: %d\n", time);
  }

  if (hp)
  {
    bool adaptive = tic_settings_get_hp_abt(settings);
    tic_sprintf(&str, "hp_abt: %s\n", adaptive ? "true" : "false");
  }

  if (hp)
  {
    uint8_t time = tic_settings_get_hp_tdecay(settings);
    tic_sprintf(&str, "hp_tdecay: %d\n", time);
  }

  if (hp)
  {
    uint8_t mode = tic_settings_get_hp_decmod(settings);
    const char * name;
    tic_code_to_name(tic_hp_decmod_names_snake, mode, &name);
    tic_sprintf(&str, "hp_decmod: %s\n", name);
  }

  if (error == NULL && str.data == NULL)
  {
    error = &tic_error_no_memory;
  }

  if (error == NULL)
  {
    *string = str.data;
    str.data = NULL;
  }

  tic_string_free(str.data);

  return error;
}
