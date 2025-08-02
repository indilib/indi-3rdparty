// Functions for representing settings in memory.

#include "tic_internal.h"

struct tic_settings
{
  uint8_t product;
  uint16_t firmware_version;

  uint8_t control_mode;
  bool never_sleep;
  bool disable_safe_start;
  bool ignore_err_line_high;
  bool auto_clear_driver_error;
  uint8_t soft_error_response;
  int32_t soft_error_position;
  uint32_t serial_baud_rate;
  uint16_t serial_device_number;
  uint16_t serial_alt_device_number;
  bool serial_enable_alt_device_number;
  bool serial_14bit_device_number;
  uint16_t command_timeout;
  bool serial_crc_for_commands;
  bool serial_crc_for_responses;
  bool serial_7bit_responses;
  uint8_t serial_response_delay;
  uint16_t low_vin_timeout;
  uint16_t low_vin_shutoff_voltage;
  uint16_t low_vin_startup_voltage;
  uint16_t high_vin_shutoff_voltage;
  int16_t vin_calibration;
  uint16_t rc_max_pulse_period;
  uint16_t rc_bad_signal_timeout;
  uint8_t rc_consecutive_good_pulses;
  uint8_t input_averaging_enabled;
  uint16_t input_hysteresis;
  uint16_t input_error_min;
  uint16_t input_error_max;
  uint8_t input_scaling_degree;
  bool input_invert;
  uint16_t input_min;
  uint16_t input_neutral_min;
  uint16_t input_neutral_max;
  uint16_t input_max;
  int32_t output_min;
  int32_t output_max;
  uint32_t encoder_prescaler;
  uint32_t encoder_postscaler;
  bool encoder_unlimited;
  struct {
    uint8_t func;
    bool pullup;
    bool analog;
    bool polarity;
  } pin_settings[TIC_CONTROL_PIN_COUNT];

  bool invert_motor_direction;
  uint32_t max_speed;
  uint32_t starting_speed;
  uint32_t max_accel;
  uint32_t max_decel;

  uint32_t current_limit;
  int32_t current_limit_during_error;
  uint8_t step_mode;
  uint8_t decay_mode;

  bool auto_homing;
  bool auto_homing_forward;
  uint32_t homing_speed_towards;
  uint32_t homing_speed_away;

  uint8_t agc_mode;
  uint8_t agc_bottom_current_limit;
  uint8_t agc_current_boost_steps;
  uint8_t agc_frequency_limit;

  bool hp_enable_unrestricted_current_limits;
  uint8_t hp_toff;
  uint8_t hp_tblank;
  bool hp_abt;
  uint8_t hp_tdecay;
  uint8_t hp_decmod;
};

void tic_settings_fill_with_defaults(tic_settings * settings)
{
  if (settings == NULL) { return; }

  uint8_t product = tic_settings_get_product(settings);
  uint16_t version = tic_settings_get_firmware_version(settings);

  // Reset all fields to zero and then restore the product and
  // firmware version.
  memset(settings, 0, sizeof(tic_settings));
  tic_settings_set_product(settings, product);
  tic_settings_set_firmware_version(settings, version);

  // The product should be set beforehand, and if it is not then quit.
  if (!product)
  {
    return;
  }

  if (product == TIC_PRODUCT_N825)
  {
    tic_settings_set_serial_7bit_responses(settings, true);
    tic_settings_set_serial_response_delay(settings, 100);
  }

  tic_settings_set_auto_clear_driver_error(settings, true);
  tic_settings_set_soft_error_response(settings, TIC_RESPONSE_DECEL_TO_HOLD);
  tic_settings_set_serial_baud_rate(settings, 9600);
  tic_settings_set_serial_device_number_u16(settings, 14);
  tic_settings_set_command_timeout(settings, 1000);
  tic_settings_set_low_vin_timeout(settings, 250);
  if (product == TIC_PRODUCT_T825 || product == TIC_PRODUCT_N825)
  {
    tic_settings_set_low_vin_shutoff_voltage(settings, 6000);
    tic_settings_set_low_vin_startup_voltage(settings, 6500);
    tic_settings_set_high_vin_shutoff_voltage(settings, 35000);
    tic_settings_set_current_limit(settings, 192);
  }
  else if (product == TIC_PRODUCT_T834)
  {
    tic_settings_set_low_vin_shutoff_voltage(settings, 1900);
    tic_settings_set_low_vin_startup_voltage(settings, 2100);
    tic_settings_set_high_vin_shutoff_voltage(settings, 13000);
    tic_settings_set_current_limit(settings, 192);
  }
  else if (product == TIC_PRODUCT_T500)
  {
    tic_settings_set_low_vin_shutoff_voltage(settings, 2800);
    tic_settings_set_low_vin_startup_voltage(settings, 3000);
    tic_settings_set_high_vin_shutoff_voltage(settings, 30000);
    tic_settings_set_current_limit(settings, 174);
  }
  else if (product == TIC_PRODUCT_T249)
  {
    tic_settings_set_low_vin_shutoff_voltage(settings, 5500);
    tic_settings_set_low_vin_startup_voltage(settings, 5800);
    tic_settings_set_high_vin_shutoff_voltage(settings, 40000);
    tic_settings_set_current_limit(settings, 200);
  }
  else if (product == TIC_PRODUCT_36V4)
  {
    tic_settings_set_low_vin_shutoff_voltage(settings, 5800);
    tic_settings_set_low_vin_startup_voltage(settings, 6100);
    tic_settings_set_high_vin_shutoff_voltage(settings, 50000);
    tic_settings_set_current_limit(settings, 215);
  }
  tic_settings_set_rc_max_pulse_period(settings, 100);
  tic_settings_set_rc_bad_signal_timeout(settings, 500);
  tic_settings_set_rc_consecutive_good_pulses(settings, 2);
  tic_settings_set_input_averaging_enabled(settings, true);
  tic_settings_set_input_error_max(settings, 4095);
  tic_settings_set_input_neutral_min(settings, 2015);
  tic_settings_set_input_neutral_max(settings, 2080);
  tic_settings_set_input_max(settings, 4095);
  tic_settings_set_output_min(settings, -200);
  tic_settings_set_output_max(settings, 200);
  tic_settings_set_encoder_prescaler(settings, 1);
  tic_settings_set_encoder_postscaler(settings, 1);
  tic_settings_set_current_limit_during_error(settings, -1);

  tic_settings_set_max_speed(settings, 2000000);
  tic_settings_set_max_accel(settings, 40000);
  tic_settings_set_homing_speed_towards(settings, 1000000);
  tic_settings_set_homing_speed_away(settings, 500000);

  if (product == TIC_PRODUCT_T249)
  {
    tic_settings_set_agc_bottom_current_limit(settings,
      TIC_AGC_BOTTOM_CURRENT_LIMIT_80);
  }

  if (product == TIC_PRODUCT_36V4)
  {
    tic_settings_set_hp_toff(settings, 0x32);
    tic_settings_set_hp_tblank(settings, 0x00);
    tic_settings_set_hp_abt(settings, true);
    tic_settings_set_hp_tdecay(settings, 0x10);
    tic_settings_set_hp_decmod(settings, TIC_HP_DECMOD_AUTO_MIXED);
  }
}

tic_error * tic_settings_create(tic_settings ** settings)
{
  if (settings == NULL)
  {
    return tic_error_create("Settings output pointer is null.");
  }

  *settings = NULL;

  tic_error * error = NULL;

  tic_settings * new_settings = NULL;
  if (error == NULL)
  {
    new_settings = (tic_settings *)calloc(1, sizeof(tic_settings));
    if (new_settings == NULL) { error = &tic_error_no_memory; }
  }

  if (error == NULL)
  {
    *settings = new_settings;
    new_settings = NULL;
  }

  tic_settings_free(new_settings);

  return error;
}

void tic_settings_free(tic_settings * settings)
{
  free(settings);
}

tic_error * tic_settings_copy(const tic_settings * source, tic_settings ** dest)
{
  if (dest == NULL)
  {
    return tic_error_create("Settings output pointer is null.");
  }

  *dest = NULL;

  if (source == NULL)
  {
    return NULL;
  }

  tic_error * error = NULL;

  tic_settings * new_settings = NULL;
  if (error == NULL)
  {
    new_settings = (tic_settings *)calloc(1, sizeof(tic_settings));
    if (new_settings == NULL) { error = &tic_error_no_memory; }
  }

  if (error == NULL)
  {
    memcpy(new_settings, source, sizeof(tic_settings));
  }

  if (error == NULL)
  {
    *dest = new_settings;
    new_settings = NULL;
  }

  tic_settings_free(new_settings);

  return error;
}

uint32_t tic_settings_achievable_serial_baud_rate(const tic_settings * settings,
  uint32_t baud)
{
  if (settings == NULL) { return 0; }
  uint16_t brg = tic_baud_rate_to_brg(baud);
  return tic_baud_rate_from_brg(brg);
}

uint32_t tic_settings_achievable_current_limit(const tic_settings * settings,
  uint32_t current_limit)
{
  if (settings == NULL) { return 0; }
  uint8_t product = tic_settings_get_product(settings);
  uint8_t code = tic_current_limit_ma_to_code(product, current_limit);
  return tic_current_limit_code_to_ma(product, code);
}

void tic_settings_set_product(tic_settings * settings, uint8_t product)
{
  if (!settings) { return; }
  settings->product = product;
}

uint8_t tic_settings_get_product(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->product;
}

void tic_settings_set_firmware_version(tic_settings * settings,
  uint16_t version)
{
  if (!settings) { return; }
  settings->firmware_version = version;
}

uint16_t tic_settings_get_firmware_version(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->firmware_version;
}

void tic_settings_set_control_mode(tic_settings * settings,
  uint8_t control_mode)
{
  if (!settings) { return; }
  settings->control_mode = control_mode;
}

uint8_t tic_settings_get_control_mode(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->control_mode;
}

void tic_settings_set_never_sleep(tic_settings * settings, bool never_sleep)
{
  if (!settings) { return; }
  settings->never_sleep = never_sleep;
}

bool tic_settings_get_never_sleep(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->never_sleep;
}

void tic_settings_set_disable_safe_start(tic_settings * settings,
  bool disable_safe_start)
{
  if (!settings) { return; }
  settings->disable_safe_start = disable_safe_start;
}

bool tic_settings_get_disable_safe_start(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->disable_safe_start;
}

void tic_settings_set_ignore_err_line_high(tic_settings * settings,
  bool ignore_err_line_high)
{
  if (!settings) { return; }
  settings->ignore_err_line_high = ignore_err_line_high;
}

bool tic_settings_get_ignore_err_line_high(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->ignore_err_line_high;
}

void tic_settings_set_auto_clear_driver_error(tic_settings * settings,
  bool auto_clear_driver_error)
{
  if (!settings) { return; }
  settings->auto_clear_driver_error = auto_clear_driver_error;
}

bool tic_settings_get_auto_clear_driver_error(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->auto_clear_driver_error;
}

void tic_settings_set_soft_error_response(tic_settings * settings,
  uint8_t soft_error_response)
{
  if (!settings) { return; }
  settings->soft_error_response = soft_error_response;
}

uint8_t tic_settings_get_soft_error_response(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->soft_error_response;
}

void tic_settings_set_soft_error_position(tic_settings * settings,
  int32_t soft_error_position)
{
  if (!settings) { return; }
  settings->soft_error_position = soft_error_position;
}

int32_t tic_settings_get_soft_error_position(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->soft_error_position;
}

void tic_settings_set_serial_baud_rate(tic_settings * settings, uint32_t serial_baud_rate)
{
  if (!settings) { return; }
  settings->serial_baud_rate = serial_baud_rate;
}

uint32_t tic_settings_get_serial_baud_rate(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_baud_rate;
}

void tic_settings_set_serial_device_number(tic_settings * settings,
  uint8_t number)
{
  tic_settings_set_serial_device_number_u16(settings, number);
}

uint8_t tic_settings_get_serial_device_number(const tic_settings * settings)
{
  return tic_settings_get_serial_device_number_u16(settings);
}

void tic_settings_set_serial_device_number_u16(tic_settings * settings,
  uint16_t number)
{
  if (!settings) { return; }
  settings->serial_device_number = number;
}

uint16_t tic_settings_get_serial_device_number_u16(
  const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_device_number;
}

void tic_settings_set_serial_alt_device_number(tic_settings * settings,
  uint16_t number)
{
  if (!settings) { return; }
  settings->serial_alt_device_number = number;
}

uint16_t tic_settings_get_serial_alt_device_number(
  const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_alt_device_number;
}

void tic_settings_set_serial_enable_alt_device_number(
  tic_settings * settings, bool enable)
{
  if (!settings) { return; }
  settings->serial_enable_alt_device_number = enable;
}

bool tic_settings_get_serial_enable_alt_device_number(
  const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_enable_alt_device_number;
}

void tic_settings_set_serial_14bit_device_number(tic_settings * settings,
  bool enabled)
{
  if (!settings) { return; }
  settings->serial_14bit_device_number = enabled;
}

bool tic_settings_get_serial_14bit_device_number(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_14bit_device_number;
}

void tic_settings_set_command_timeout(tic_settings * settings,
  uint16_t command_timeout)
{
  if (!settings) { return; }
  settings->command_timeout = command_timeout;
}

uint16_t tic_settings_get_command_timeout(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->command_timeout;
}

void tic_settings_set_serial_crc_enabled(tic_settings * settings, bool enabled)
{
  tic_settings_set_serial_crc_for_commands(settings, enabled);
}

bool tic_settings_get_serial_crc_enabled(const tic_settings * settings)
{
  return tic_settings_get_serial_crc_for_commands(settings);
}

void tic_settings_set_serial_crc_for_commands(tic_settings * settings, bool enabled)
{
  if (!settings) { return; }
  settings->serial_crc_for_commands = enabled;
}

bool tic_settings_get_serial_crc_for_commands(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_crc_for_commands;
}

void tic_settings_set_serial_crc_for_responses(tic_settings * settings,
  bool enabled)
{
  if (!settings) { return; }
  settings->serial_crc_for_responses = enabled;
}

bool tic_settings_get_serial_crc_for_responses(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_crc_for_responses;
}

void tic_settings_set_serial_7bit_responses(tic_settings * settings,
  bool enabled)
{
  if (!settings) { return; }
  settings->serial_7bit_responses = enabled;
}

bool tic_settings_get_serial_7bit_responses(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_7bit_responses;
}

void tic_settings_set_serial_response_delay(tic_settings * settings,
  uint8_t serial_response_delay)
{
  if (!settings) { return; }
  settings->serial_response_delay = serial_response_delay;
}

uint8_t tic_settings_get_serial_response_delay(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->serial_response_delay;
}

void tic_settings_set_low_vin_timeout(tic_settings * settings,
  uint16_t low_vin_timeout)
{
  if (!settings) { return; }
  settings->low_vin_timeout = low_vin_timeout;
}

uint16_t tic_settings_get_low_vin_timeout(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->low_vin_timeout;
}

void tic_settings_set_low_vin_shutoff_voltage(tic_settings * settings,
  uint16_t low_vin_shutoff_voltage)
{
  if (!settings) { return; }
  settings->low_vin_shutoff_voltage = low_vin_shutoff_voltage;
}

uint16_t tic_settings_get_low_vin_shutoff_voltage(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->low_vin_shutoff_voltage;
}

void tic_settings_set_low_vin_startup_voltage(tic_settings * settings,
  uint16_t low_vin_startup_voltage)
{
  if (!settings) { return; }
  settings->low_vin_startup_voltage = low_vin_startup_voltage;
}

uint16_t tic_settings_get_low_vin_startup_voltage(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->low_vin_startup_voltage;
}

void tic_settings_set_high_vin_shutoff_voltage(tic_settings * settings,
  uint16_t high_vin_shutoff_voltage)
{
  if (!settings) { return; }
  settings->high_vin_shutoff_voltage = high_vin_shutoff_voltage;
}

uint16_t tic_settings_get_high_vin_shutoff_voltage(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->high_vin_shutoff_voltage;
}

void tic_settings_set_vin_calibration(tic_settings * settings,
  uint16_t vin_calibration)
{
  if (!settings) { return; }
  settings->vin_calibration = vin_calibration;
}

uint16_t tic_settings_get_vin_calibration(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->vin_calibration;
}

void tic_settings_set_rc_max_pulse_period(tic_settings * settings,
  uint16_t rc_max_pulse_period)
{
  if (!settings) { return; }
  settings->rc_max_pulse_period = rc_max_pulse_period;
}

uint16_t tic_settings_get_rc_max_pulse_period(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->rc_max_pulse_period;
}

void tic_settings_set_rc_bad_signal_timeout(tic_settings * settings,
  uint16_t rc_bad_signal_timeout)
{
  if (!settings) { return; }
  settings->rc_bad_signal_timeout = rc_bad_signal_timeout;
}

uint16_t tic_settings_get_rc_bad_signal_timeout(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->rc_bad_signal_timeout;
}

void tic_settings_set_rc_consecutive_good_pulses(tic_settings * settings,
  uint8_t rc_consecutive_good_pulses)
{
  if (!settings) { return; }
  settings->rc_consecutive_good_pulses = rc_consecutive_good_pulses;
}

uint8_t tic_settings_get_rc_consecutive_good_pulses(
  const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->rc_consecutive_good_pulses;
}

void tic_settings_set_input_error_min(tic_settings * settings,
  uint16_t input_error_min)
{
  if (!settings) { return; }
  settings->input_error_min = input_error_min;
}

uint16_t tic_settings_get_input_error_min(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_error_min;
}

void tic_settings_set_input_error_max(tic_settings * settings,
  uint16_t input_error_max)
{
  if (!settings) { return; }
  settings->input_error_max = input_error_max;
}

uint16_t tic_settings_get_input_error_max(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_error_max;
}

void tic_settings_set_input_averaging_enabled(tic_settings * settings,
  bool input_averaging_enabled)
{
  if (!settings) { return; }
  settings->input_averaging_enabled = input_averaging_enabled;
}

bool tic_settings_get_input_averaging_enabled(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_averaging_enabled;
}

void tic_settings_set_input_hysteresis(tic_settings * settings, uint16_t input_hysteresis)
{
  if (!settings) { return; }
  settings->input_hysteresis = input_hysteresis;
}

uint16_t tic_settings_get_input_hysteresis(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_hysteresis;
}

void tic_settings_set_input_scaling_degree(tic_settings * settings,
  uint8_t input_scaling_degree)
{
  if (!settings) { return; }
  settings->input_scaling_degree = input_scaling_degree;
}

uint8_t tic_settings_get_input_scaling_degree(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_scaling_degree;
}

void tic_settings_set_input_invert(tic_settings * settings, bool invert)
{
  if (!settings) { return; }
  settings->input_invert = invert;
}

bool tic_settings_get_input_invert(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_invert;
}

void tic_settings_set_input_min(tic_settings * settings, uint16_t input_min)
{
  if (!settings) { return; }
  settings->input_min = input_min;
}

uint16_t tic_settings_get_input_min(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_min;
}

void tic_settings_set_input_neutral_min(tic_settings * settings,
  uint16_t input_neutral_min)
{
  if (!settings) { return; }
  settings->input_neutral_min = input_neutral_min;
}

uint16_t tic_settings_get_input_neutral_min(const tic_settings * settings)
{
  if (!settings){ return 0; }
  return settings->input_neutral_min;
}

void tic_settings_set_input_neutral_max(tic_settings * settings,
  uint16_t input_neutral_max)
{
  if (!settings) { return; }
  settings->input_neutral_max = input_neutral_max;
}

uint16_t tic_settings_get_input_neutral_max(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_neutral_max;
}

void tic_settings_set_input_max(tic_settings * settings, uint16_t input_max)
{
  if (!settings) { return; }
  settings->input_max = input_max;
}

uint16_t tic_settings_get_input_max(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->input_max;
}

void tic_settings_set_output_min(tic_settings * settings, int32_t output_min)
{
  if (!settings) { return; }
  settings->output_min = output_min;
}

int32_t tic_settings_get_output_min(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->output_min;
}

void tic_settings_set_output_max(tic_settings * settings, int32_t output_max)
{
  if (!settings) { return; }
  settings->output_max = output_max;
}

int32_t tic_settings_get_output_max(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->output_max;
}

void tic_settings_set_encoder_prescaler(tic_settings * settings,
  uint32_t encoder_prescaler)
{
  if (!settings) { return; }
  settings->encoder_prescaler = encoder_prescaler;
}

uint32_t tic_settings_get_encoder_prescaler(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->encoder_prescaler;
}

void tic_settings_set_encoder_postscaler(tic_settings * settings,
  uint32_t encoder_postscaler)
{
  if (!settings) { return; }
  settings->encoder_postscaler = encoder_postscaler;
}

uint32_t tic_settings_get_encoder_postscaler(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->encoder_postscaler;
}

void tic_settings_set_encoder_unlimited(tic_settings * settings,
  bool encoder_unlimited)
{
  if (!settings) { return; }
  settings->encoder_unlimited = encoder_unlimited;
}

bool tic_settings_get_encoder_unlimited(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->encoder_unlimited;
}

void tic_settings_set_pin_func(tic_settings * settings, uint8_t pin, uint8_t func)
{
  if (!settings || pin >= TIC_CONTROL_PIN_COUNT) { return; }
  settings->pin_settings[pin].func = func;
}

uint8_t tic_settings_get_pin_func(const tic_settings * settings, uint8_t pin)
{
  if (!settings || pin >= TIC_CONTROL_PIN_COUNT) { return 0; }
  return settings->pin_settings[pin].func;
}

void tic_settings_set_pin_pullup(tic_settings * settings, uint8_t pin,
  bool pullup)
{
  if (!settings || pin >= TIC_CONTROL_PIN_COUNT) { return; }
  settings->pin_settings[pin].pullup = pullup;
}

bool tic_settings_get_pin_pullup(const tic_settings * settings, uint8_t pin)
{
  if (!settings || pin >= TIC_CONTROL_PIN_COUNT) { return 0; }
  return settings->pin_settings[pin].pullup;
}

void tic_settings_set_pin_analog(tic_settings * settings, uint8_t pin,
  bool analog)
{
  if (!settings || pin >= TIC_CONTROL_PIN_COUNT) { return; }
  settings->pin_settings[pin].analog = analog;
}

bool tic_settings_get_pin_analog(const tic_settings * settings, uint8_t pin)
{
  if (!settings || pin >= TIC_CONTROL_PIN_COUNT) { return 0; }
  return settings->pin_settings[pin].analog;
}

void tic_settings_set_pin_polarity(tic_settings * settings, uint8_t pin,
  bool polarity)
{
  if (!settings || pin >= TIC_CONTROL_PIN_COUNT) { return; }
  settings->pin_settings[pin].polarity = polarity;
}

bool tic_settings_get_pin_polarity(const tic_settings * settings, uint8_t pin)
{
  if (!settings || pin >= TIC_CONTROL_PIN_COUNT) { return 0; }
  return settings->pin_settings[pin].polarity;
}

void tic_settings_set_invert_motor_direction(tic_settings * settings, bool invert)
{
  if (!settings) { return; }
  settings->invert_motor_direction = invert;
}

bool tic_settings_get_invert_motor_direction(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->invert_motor_direction;
}

void tic_settings_set_max_speed(tic_settings * settings, uint32_t max_speed)
{
  if (!settings) { return; }
  settings->max_speed = max_speed;
}

uint32_t tic_settings_get_max_speed(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->max_speed;
}

void tic_settings_set_starting_speed(tic_settings * settings, uint32_t starting_speed)
{
  if (!settings) { return; }
  settings->starting_speed = starting_speed;
}

uint32_t tic_settings_get_starting_speed(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->starting_speed;
}

void tic_settings_set_max_accel(tic_settings * settings, uint32_t max_accel)
{
  if (!settings) { return; }
  settings->max_accel = max_accel;
}

uint32_t tic_settings_get_max_accel(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->max_accel;
}

void tic_settings_set_max_decel(tic_settings * settings, uint32_t max_decel)
{
  if (!settings) { return; }
  settings->max_decel = max_decel;
}

uint32_t tic_settings_get_max_decel(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->max_decel;
}

void tic_settings_set_step_mode(tic_settings * settings, uint8_t step_mode)
{
  if (!settings) { return; }
  settings->step_mode = step_mode;
}

uint8_t tic_settings_get_step_mode(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->step_mode;
}

void tic_settings_set_current_limit(tic_settings * settings,
  uint32_t current_limit)
{
  if (!settings) { return; }
  settings->current_limit = current_limit;
}

uint32_t tic_settings_get_current_limit(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->current_limit;
}

void tic_settings_set_current_limit_code(tic_settings * settings, uint8_t code)
{
  if (!settings) { return; }
  uint8_t product = tic_settings_get_product(settings);
  settings->current_limit = tic_current_limit_code_to_ma(product, code);
}

uint8_t tic_settings_get_current_limit_code(const tic_settings * settings)
{
  if (!settings) { return 0; }
  uint8_t product = tic_settings_get_product(settings);
  return tic_current_limit_ma_to_code(product, settings->current_limit);
}

void tic_settings_set_current_limit_during_error(tic_settings * settings,
  int32_t current_limit)
{
  if (!settings) { return; }
  settings->current_limit_during_error = current_limit;
}

int32_t tic_settings_get_current_limit_during_error(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->current_limit_during_error;
}

void tic_settings_set_current_limit_code_during_error(tic_settings * settings, uint8_t code)
{
  if (!settings) { return; }
  uint8_t product = tic_settings_get_product(settings);
  if (code == 0xFF)
  {
    settings->current_limit_during_error = -1;
  }
  else
  {
    settings->current_limit_during_error =
      tic_current_limit_code_to_ma(product, code);
  }
}

uint8_t tic_settings_get_current_limit_code_during_error(const tic_settings * settings)
{
  if (!settings) { return 0; }
  if (settings->current_limit_during_error < 0)
  {
    return 0xFF;
  }
  else
  {
    uint8_t product = tic_settings_get_product(settings);
    return tic_current_limit_ma_to_code(product,
      settings->current_limit_during_error);
  }
}

void tic_settings_set_decay_mode(tic_settings * settings, uint8_t decay_mode)
{
  if (!settings) { return; }
  settings->decay_mode = decay_mode;
}

uint8_t tic_settings_get_decay_mode(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->decay_mode;
}

void tic_settings_set_auto_homing(tic_settings * settings, bool enabled)
{
  if (!settings) { return; }
  settings->auto_homing = enabled;
}

bool tic_settings_get_auto_homing(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->auto_homing;
}

void tic_settings_set_auto_homing_forward(tic_settings * settings, bool forward)
{
  if (!settings) { return; }
  settings->auto_homing_forward = forward;
}

bool tic_settings_get_auto_homing_forward(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->auto_homing_forward;
}

void tic_settings_set_homing_speed_towards(tic_settings * settings, uint32_t speed)
{
  if (!settings) { return; }
  settings->homing_speed_towards = speed;
}

uint32_t tic_settings_get_homing_speed_towards(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->homing_speed_towards;
}

void tic_settings_set_homing_speed_away(tic_settings * settings, uint32_t speed)
{
  if (!settings) { return; }
  settings->homing_speed_away = speed;
}

uint32_t tic_settings_get_homing_speed_away(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->homing_speed_away;
}

void tic_settings_set_agc_mode(tic_settings * settings, uint8_t mode)
{
  if (!settings) { return; }
  settings->agc_mode = mode;
}

uint8_t tic_settings_get_agc_mode(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->agc_mode;
}

void tic_settings_set_agc_bottom_current_limit(tic_settings * settings,
  uint8_t limit)
{
  if (!settings) { return; }
  settings->agc_bottom_current_limit = limit;
}

uint8_t tic_settings_get_agc_bottom_current_limit(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->agc_bottom_current_limit;
}

void tic_settings_set_agc_current_boost_steps(tic_settings * settings,
  uint8_t steps)
{
  if (!settings) { return; }
  settings->agc_current_boost_steps = steps;
}

uint8_t tic_settings_get_agc_current_boost_steps(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->agc_current_boost_steps;
}

void tic_settings_set_agc_frequency_limit(tic_settings * settings, uint8_t limit)
{
  if (!settings) { return; }
  settings->agc_frequency_limit = limit;
}

uint8_t tic_settings_get_agc_frequency_limit(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->agc_frequency_limit;
}

void tic_settings_set_hp_enable_unrestricted_current_limits(
  tic_settings * settings, bool enable)
{
  if (!settings) { return; }
  settings->hp_enable_unrestricted_current_limits = enable;
}

bool tic_settings_get_hp_enable_unrestricted_current_limits(
  const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->hp_enable_unrestricted_current_limits;
}

void tic_settings_set_hp_toff(tic_settings * settings, uint8_t time)
{
  if (!settings) { return; }
  settings->hp_toff = time;
}

uint8_t tic_settings_get_hp_toff(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->hp_toff;
}

void tic_settings_set_hp_tblank(tic_settings * settings, uint8_t time)
{
  if (!settings) { return; }
  settings->hp_tblank = time;
}

uint8_t tic_settings_get_hp_tblank(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->hp_tblank;
}

void tic_settings_set_hp_abt(tic_settings * settings, bool adaptive)
{
  if (!settings) { return; }
  settings->hp_abt = adaptive;
}

bool tic_settings_get_hp_abt(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->hp_abt;
}

void tic_settings_set_hp_tdecay(tic_settings * settings, uint8_t time)
{
  if (!settings) { return; }
  settings->hp_tdecay = time;
}

uint8_t tic_settings_get_hp_tdecay(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->hp_tdecay;
}

void tic_settings_set_hp_decmod(tic_settings * settings, uint8_t mode)
{
  if (!settings) { return; }
  settings->hp_decmod = mode;
}

uint8_t tic_settings_get_hp_decmod(const tic_settings * settings)
{
  if (!settings) { return 0; }
  return settings->hp_decmod;
}

static uint32_t tic_settings_get_hp_tblank_ns(
    const tic_settings * settings)
{
  uint8_t tblank = tic_settings_get_hp_tblank(settings);
  if (tblank < 0x32) { tblank = 0x32; }
  return tblank * 20;
}

uint32_t tic_settings_get_hp_toff_ns(const tic_settings * settings)
{
  uint8_t toff = tic_settings_get_hp_toff(settings);
  return (toff + 1) * 500;
}

// Return true if equation 3 in the DRV8711 datasheet is satisfied.
bool tic_settings_hp_gate_charge_ok(const tic_settings * settings)
{
  uint32_t tblank_ns = tic_settings_get_hp_tblank_ns(settings);
  if (tic_settings_get_hp_abt(settings)) { tblank_ns /= 2; }

  uint32_t toff_ns = tic_settings_get_hp_toff_ns(settings);

  // Assumption: We use a dead time (DTIME) of 850 ns.
  uint32_t dtime_ns = 850;

  // Assumption: The gate charge of the MOSFETs is 20 nC or less.

  return 2 * dtime_ns + tblank_ns + toff_ns > 4000;
}
