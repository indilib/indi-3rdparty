#pragma once

#include "tic.hpp"

class main_window;

class main_controller
{
public:
  // Stores a pointer to the window so we can update the window.
  void set_window(main_window *);

  // This is called when the program starts up.
  void start();

  // This is called when the user issues a connect command.
  void connect_device_with_os_id(const std::string & id);

  // This is called when the user issues a disconnect command. Returns true
  // on completion or false if the user cancels when prompted about settings
  // that have not been applied.
  bool disconnect_device();

  // This is called when the user wants to send the Clear Driver Error command.
  void clear_driver_error();

  // This is called when the user wants to send a 'Go home' command.
  void go_home(uint8_t direction);

  // This is called when the user issues a command to reload settings from the
  // device.
  void reload_settings(bool ask = true);

  // This is called when the user wants to restore the device to its default
  // settings.
  void restore_default_settings();

  // This is called when the user wants to upgrade some firmware.
  void upgrade_firmware();

  // This is called when it is time to check if the status of the device has
  // changed.
  void update();

  // This is called when the user tries to exit the program.  Returns true if
  // the program is actually allowed to exit.
  bool exit();

  // This is called whenever something in the model has changed that might
  // require the window to be updated.  It includes no details about what
  // exactly changed.
  void handle_model_changed();

private:
  void connect_device(const tic::device & device);
  void disconnect_device_by_error(const std::string & error_message);
  void really_disconnect();
  void set_connection_error(const std::string & error_message);

  // Returns true for success, false for failure.
  bool update_device_list();

  // True if device_list changed the last time update_device_list() was
  // called.
  bool device_list_changed;

  void show_exception(const std::exception & e, const std::string & context = "");

public:
  void set_target_position(int32_t position);
  void set_target_velocity(int32_t velocity);
  void halt_and_set_position(int32_t position);
  void halt_and_hold();
  void deenergize();
  void resume();
  void start_input_setup();

  bool warn_about_applying_high_current_settings();

  // This is called when the user wants to apply the settings.
  void apply_settings();

  void open_settings_from_file(std::string filename);
  void save_settings_to_file(std::string filename);

  // These are called when the user changes a setting.
  // [all-settings]

  void handle_control_mode_input(uint8_t control_mode);

  void handle_serial_baud_rate_input(uint32_t serial_baud_rate);
  void handle_serial_baud_rate_input_finished();
  void handle_serial_device_number_input(uint16_t number);
  void handle_serial_alt_device_number_input(uint16_t number);
  void handle_serial_enable_alt_device_number_input(bool enable);
  void handle_serial_14bit_device_number_input(bool enable);
  void handle_command_timeout_input(uint16_t command_timeout);
  void handle_serial_crc_for_commands_input(bool enabled);
  void handle_serial_crc_for_responses_input(bool enabled);
  void handle_serial_7bit_responses_input(bool enabled);
  void handle_serial_response_delay_input(uint8_t delay);

  void handle_encoder_prescaler_input(uint32_t encoder_prescaler);
  void handle_encoder_postscaler_input(uint32_t encoder_postscaler);
  void handle_encoder_unlimited_input(bool encoder_unlimited);

  void handle_input_averaging_enabled_input(bool input_averaging_enabled);
  void handle_input_hysteresis_input(uint16_t input_hysteresis);

  void handle_input_invert_input(bool input_invert);
  void handle_input_min_input(uint16_t input_min);
  void handle_input_neutral_min_input(uint16_t input_neutral_min);
  void handle_input_neutral_max_input(uint16_t input_neutral_max);
  void handle_input_max_input(uint16_t input_max);
  void handle_output_min_input(int32_t output_min);
  void handle_output_max_input(int32_t output_max);
  void handle_input_scaling_degree_input(uint8_t input_scaling_degree);

  void handle_invert_motor_direction_input(bool invert_motor_direction);
  void handle_speed_max_input(uint32_t speed_max);
  void handle_starting_speed_input(uint32_t starting_speed);
  void handle_accel_max_input(uint32_t accel_max);
  void handle_decel_max_input(uint32_t decel_max);
  void handle_step_mode_input(uint8_t step_mode);
  void handle_current_limit_input(uint32_t current_limit);
  void handle_decay_mode_input(uint8_t decay_mode);
  void handle_agc_mode_input(uint8_t);
  void handle_agc_bottom_current_limit_input(uint8_t);
  void handle_agc_current_boost_steps_input(uint8_t);
  void handle_agc_frequency_limit_input(uint8_t);

  void handle_hp_enable_unrestricted_current_limits_input(bool);
  void handle_hp_toff_input(uint8_t);
  void handle_hp_tblank_input(uint8_t);
  void handle_hp_abt_input(bool);
  void handle_hp_tdecay_input(uint8_t);

  void handle_soft_error_response_input(uint8_t soft_error_response);
  void handle_soft_error_position_input(int32_t soft_error_position);
  void handle_current_limit_during_error_input(int32_t current_limit_during_error);

  void handle_disable_safe_start_input(bool disable_safe_start);
  void handle_ignore_err_line_high_input(bool ignore_err_line_high);
  void handle_auto_clear_driver_error_input(bool auto_clear_driver_error);
  void handle_never_sleep_input(bool never_sleep);
  void handle_vin_calibration_input(int16_t vin_calibration);

  void handle_auto_homing_input(bool);
  void handle_auto_homing_forward_input(bool);
  void handle_homing_speed_towards_input(uint32_t speed);
  void handle_homing_speed_away_input(uint32_t speed);

  void handle_pin_func_input(uint8_t pin, uint8_t func);
  void handle_pin_pullup_input(uint8_t pin, bool pullup);
  void handle_pin_polarity_input(uint8_t pin, bool polarity);
  void handle_pin_analog_input(uint8_t pin, bool analog);

  void handle_upload_complete();

  uint8_t get_product() { return settings.get_product(); }

private:
  // This is called whenever it is possible that we have connected to a
  // different device.
  void handle_device_changed();
  void handle_variables_changed();
  void handle_settings_changed();
  void handle_settings_applied();
  void update_menu_enables();

  void initialize_manual_target();
  void update_motor_status_message(bool prompt_to_resume);

  // Holds a list of the relevant devices that are connected to the computer.
  std::vector<tic::device> device_list;

  // Holds an open handle to a device or a null handle if we are not connected.
  tic::handle device_handle;

  // True if the last connection or connection attempt resulted in an error.  If
  // true, connection_error_essage provides some information about the error.
  bool connection_error = false;
  std::string connection_error_message;

  // True if we are disconnected now and the last connection was terminated by
  // the user.
  bool disconnected_by_user = false;

  // Holds a working copy of the settings from the device, including any
  // unapplied changes.
  tic::settings settings;

  // Holds a cached copy of the settings from the device, without any unapplied
  // changes.
  tic::settings cached_settings;

  // True if the working settings have been modified by user and could be
  // different from what is cached and on the device.
  bool settings_modified = false;

  // Holds the variables/status of the device.
  tic::variables variables;

  // True if the last attempt to update the variables failed (typically due
  // to a USB error).
  bool variables_update_failed = false;

  // The number of updates to wait for before updating the
  // device list again (saves CPU time).
  uint32_t update_device_list_counter = 1;

  // True if we want to regularly send the "Reset command timeout" command to
  // this device.
  bool send_reset_command_timeout = false;

  bool suppress_high_current_limit_warning = false;
  bool suppress_potential_high_current_limit_warning = false;

  void reload_variables();

  // Returns true if we are currently connected to a device.
  bool connected() const { return device_handle; }

  static bool control_mode_is_serial(const tic::settings & s);
  static bool uses_pin_func(const tic::settings & s, uint8_t func);

  main_window * window;
};
