#pragma once

#include <array>

#include "tic.hpp"

#include "bootloader_window.h"
#include "elided_label.h"
#include "InputWizard.h"
#include "BallScrollBar.h"
#include <array>
#include <QMainWindow>
#include <QApplication>
#include <QtGui>
#include <QtCore>
#include <QScrollBar>


class BallScrollBar;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QGridLayout;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QRadioButton;
class QShortcut;
class QSpinBox;
class QTabWidget;
class QVBoxLayout;

class current_spin_box;
class time_spin_box;
class main_controller;

struct tab_spec {
  QWidget * tab = NULL;
  QString name;
  bool hidden = false;

  tab_spec(QWidget * tab, QString name, bool hidden = false) :
  tab(tab), name(name), hidden(hidden)
  {
  }
};

struct error_row
{
  unsigned int count;
  QLabel * name_label = NULL;
  QLabel * stopping_value = NULL;
  QLabel * count_value = NULL;
  QFrame * background = NULL;
};

class pin_config_row : QObject
{
  Q_OBJECT

public:
  explicit pin_config_row(QObject * parent = Q_NULLPTR) : QObject(parent)
  {}
  explicit pin_config_row(uint8_t pin_val, QObject * parent = Q_NULLPTR)
  : QObject(parent), pin(pin_val) // <--- MODIFICATION ICI : Inversion de l'ordre
  {}

  void setup(QGridLayout * layout, int row,
             const char * pullup_message = NULL, bool hide_analog = false);
  void add_funcs(uint16_t funcs);

private:
  bool window_suppress_events() const;
  void set_window_suppress_events(bool suppress_events);
  main_controller * window_controller() const;

  uint8_t pin = 255;
  QLabel * name_label = NULL;
  QComboBox * func_value = NULL;
  QCheckBox * pullup_check = NULL;
  QCheckBox * polarity_check = NULL;
  QCheckBox * analog_check = NULL;

private slots:
  void on_func_value_currentIndexChanged(int index);
  void on_pullup_check_stateChanged(int state);
  void on_polarity_check_stateChanged(int state);
  void on_analog_check_stateChanged(int state);

private:
  friend class main_window;
};

class main_window : public QMainWindow
{
  Q_OBJECT

public:
  QLayout* setup_manual_target_layout();
  QLayout* setup_errors_layout();
  QLayout* setup_error_table_layout();
  QLayout* setup_motor_settings_layout();
  QWidget* setup_hp_motor_widget();
  QWidget* setup_control_mode_widget();
  QGroupBox* setup_encoder_settings_box();
  QGroupBox* setup_scaling_settings_box();
  QGroupBox* setup_pin_config_box();
  QGroupBox* setup_error_settings_box();
  QGroupBox* setup_misc_settings_box();
  explicit main_window(QWidget *parent = nullptr);
  ~main_window();
  void set_controller(main_controller * controller);
  bootloader_window * open_bootloader_window();
  void set_update_timer_interval(uint32_t interval_ms);
  void start_update_timer();
  void show_error_message(const std::string & message);
  void show_warning_message(const std::string & message);
  void show_info_message(const std::string & message);
  bool confirm(const std::string & question);
  bool warn_and_confirm(const std::string & question);
  void set_device_list_contents(const std::vector<tic::device> & device_list);
  void set_device_list_selected(const tic::device & device);
  void set_connection_status(const std::string & status, bool error);
  void adjust_ui_for_product(uint8_t product);
  void update_shown_tabs();
  void update_current_limit_table(uint8_t product);
  void update_current_limit_warnings();
  void set_tab_pages_enabled(bool enabled);
  void set_manual_target_enabled(bool enabled);
  void set_deenergize_button_enabled(bool enabled);
  void set_resume_button_enabled(bool enabled);
  void set_apply_settings_enabled(bool enabled);
  void set_open_save_settings_enabled(bool enabled);
  void set_disconnect_enabled(bool enabled);
  void set_clear_driver_error_enabled(bool enabled);
  void set_go_home_enabled(bool reverse_enabled, bool forward_enabled);
  void set_reload_settings_enabled(bool enabled);
  void set_restore_defaults_enabled(bool enabled);
  void set_device_name(const std::string & name, bool link_enabled);
  void set_serial_number(const std::string & serial_number);
  void set_firmware_version(const std::string & firmware_version);
  void set_device_reset(const std::string & device_reset);
  void set_up_time(uint32_t up_time);
  void set_encoder_position(int32_t encoder_position);
  void set_input_state(const std::string & input_state, uint8_t input_state_raw);
  void set_input_after_averaging(uint16_t input_after_averaging);
  void set_input_after_hysteresis(uint16_t input_after_hysteresis);
  void set_input_before_scaling(uint16_t input_before_scaling, uint8_t control_mode);
  void set_input_after_scaling(int32_t input_after_scaling);
  void set_vin_voltage(uint32_t vin_voltage);
  void set_operation_state(const std::string & operation_state);
  void set_energized(bool energized);
  void set_limit_active(bool forward, bool reverse);
  void disable_limit_active();
  void set_homing_active(bool active);
  void set_last_motor_driver_error(const char * str);
  void set_last_hp_driver_errors(uint8_t errors);
  void set_target_position(int32_t target_position);
  void set_target_velocity(int32_t target_velocity);
  void set_target_none();
  void set_current_position(int32_t current_position);
  void set_position_uncertain(bool position_uncertain);
  void set_current_velocity(int32_t current_velocity);
  void set_error_status(uint16_t error_status);
  void increment_errors_occurred(uint32_t errors_occurred);
  void reset_error_counts();
  void set_manual_target_position_mode();
  void set_manual_target_velocity_mode();
  void set_manual_target_range(int32_t target_min, int32_t target_max);
  void set_displayed_manual_target(int32_t target);
  void set_manual_target_ball_position(int32_t current_position, bool on_target);
  void set_manual_target_ball_velocity(int32_t current_velocity, bool on_target);
  void set_apply_settings_button_stylesheet(int offset);
  void animate_apply_settings_button();
  void set_control_mode(uint8_t control_mode);
  void set_serial_baud_rate(uint32_t serial_baud_rate);
  void set_serial_device_number(uint16_t number);
  void set_serial_alt_device_number(uint16_t number);
  void set_serial_enable_alt_device_number(bool enable);
  void set_serial_14bit_device_number(bool enable);
  void set_command_timeout(uint16_t command_timeout);
  void set_serial_crc_for_commands(bool enable);
  void set_serial_crc_for_responses(bool enable);
  void set_serial_7bit_responses(bool enabled);
  void set_serial_response_delay(uint8_t serial_response_delay);
  void set_encoder_prescaler(uint32_t encoder_prescaler);
  void set_encoder_postscaler(uint32_t encoder_postscaler);
  void set_encoder_unlimited(bool encoder_unlimited);
  void set_input_averaging_enabled(bool input_averaging_enabled);
  void set_input_hysteresis(uint16_t input_hysteresis);
  void set_input_invert(bool input_invert);
  void set_input_min(uint16_t input_min);
  void set_input_neutral_min(uint16_t input_neutral_min);
  void set_input_neutral_max(uint16_t input_neutral_max);
  void set_input_max(uint16_t input_max);
  void set_output_min(int32_t output_min);
  void set_output_max(int32_t output_max);
  void set_input_scaling_degree(uint8_t input_scaling_degree);
  void run_input_wizard(uint8_t control_mode);
  void set_invert_motor_direction(bool invert_motor_direction);
  void set_speed_max(uint32_t speed_max);
  void set_starting_speed(uint32_t starting_speed);
  void set_accel_max(uint32_t accel_max);
  void set_decel_max(uint32_t decel_max);
  void set_decel_accel_max_same(bool decel_accel_max_same);
  void set_step_mode(uint8_t step_mode);
  void set_current_limit(uint32_t current_limit);
  void set_decay_mode(uint8_t decay_mode);
  void set_agc_mode(uint8_t);
  void set_agc_bottom_current_limit(uint8_t);
  void set_agc_current_boost_steps(uint8_t);
  void set_agc_frequency_limit(uint8_t);
  void set_hp_toff(uint8_t);
  void set_hp_tblank(uint8_t);
  void set_hp_abt(bool);
  void set_hp_tdecay(uint8_t);
  void set_soft_error_response(uint8_t soft_error_response);
  void set_soft_error_position(int32_t soft_error_position);
  void set_current_limit_during_error(int32_t current_limit_during_error);
  void set_disable_safe_start(bool disable_safe_start);
  void set_ignore_err_line_high(bool ignore_err_line_high);
  void set_auto_clear_driver_error(bool auto_clear_driver_error);
  void set_never_sleep(bool never_sleep);
  void set_hp_enable_unrestricted_current_limits(bool enable);
  void set_vin_calibration(int16_t vin_calibration);
  void set_auto_homing(bool auto_homing);
  void set_auto_homing_forward(bool forward);
  void set_homing_speed_towards(uint32_t speed);
  void set_homing_speed_away(uint32_t speed);
  void set_pin_func(uint8_t pin, uint8_t func);
  void set_pin_pullup(uint8_t pin, bool pullup, bool enabled);
  void set_pin_polarity(uint8_t pin, bool polarity, bool enabled);
  void set_pin_analog(uint8_t pin, bool analog, bool enabled);
  void set_motor_status_message(const std::string & message, bool stopped = true);

private:
  QLabel * errors_stopping_header_label = NULL;
  QLabel * errors_count_header_label = NULL;
  QGroupBox * serial_settings_box = NULL;
  QGridLayout * serial_settings_box_layout = NULL;
  QLabel * serial_baud_rate_label = NULL;
  QLabel * serial_device_number_label = NULL;
  QLabel * serial_response_delay_label = NULL;
  QGroupBox * encoder_settings_box = NULL;
  QGridLayout * encoder_settings_box_layout = NULL;
  QLabel * encoder_prescaler_label = NULL;
  QLabel * encoder_postscaler_label = NULL;
  QGroupBox * conditioning_settings_box = NULL;
  QGridLayout * conditioning_settings_box_layout = NULL;
  QLabel * input_hysteresis_label = NULL;
  QGroupBox * scaling_settings_box = NULL;
  QGridLayout * scaling_settings_box_layout = NULL;
  QPushButton * input_learn_button = NULL;
  QLabel * scaling_input_label = NULL;
  QLabel * scaling_target_label = NULL;
  QLabel * scaling_max_label = NULL;
  QLabel * scaling_neutral_max_label = NULL;
  QLabel * scaling_neutral_min_label = NULL;
  QLabel * scaling_min_label = NULL;
  QLabel * input_scaling_degree_label = NULL;
  QLabel * hp_tblank_label = NULL;
  QGroupBox * motor_settings_box = NULL;
  QWidget* setup_input_motor_settings_page_widget();
  QLayout* setup_homing_settings_layout();
  QGroupBox* setup_homing_settings_box();
  QDoubleSpinBox * manual_target_min_value = NULL;
  QDoubleSpinBox * manual_target_max_value = NULL;
  QGridLayout * input_motor_settings_page_layout = nullptr;
  QLabel *device_reset_label = nullptr;
  QLabel *firmware_version_value = nullptr;
  QLabel *device_name_label = nullptr;
  QLabel *device_name_value = nullptr;
  QLabel *serial_number_label = nullptr;
  QLabel *serial_number_value = nullptr;
  QLabel *firmware_version_label = nullptr;
  QLabel *device_reset_value = nullptr;
  QLabel *up_time_label = nullptr;
  QLabel *up_time_value = nullptr;
  QLabel *advanced_tab_label = nullptr;
  QComboBox * step_mode_value;
  QPushButton * set_target_button;
  QLabel *control_mode_label = nullptr;
  QMenuBar * menu_bar;
  QMenu * file_menu;
  QAction * exit_action;
  QMenu * device_menu;
  QAction * upgrade_firmware_action;
  QMenu * help_menu;
  QAction * documentation_action;
  QAction * about_action;
  QLabel *baud_rate_label = nullptr;
  QHBoxLayout * header_layout;
  QLabel * device_list_label;
  QLabel *scl_pin_label = nullptr;
  QWidget * status_page_widget;
  QGridLayout * status_page_layout;
  QLabel *sd_pin_label = nullptr;
  QWidget* setup_errors_widget();
  QWidget* setup_manual_target_widget();
  QWidget* setup_motor_settings_widget();
  QWidget* setup_homing_settings_widget();
  QWidget* setup_advanced_settings_page_widget();
  QGroupBox* setup_errors_box();
  QGroupBox* setup_input_status_box();
  QGroupBox* setup_operation_status_box();
  QGroupBox* setup_manual_target_box();
  QGroupBox* device_info_box;
  QGridLayout* device_info_box_layout;
  QLabel *encoder_position_label = nullptr;
  QTimer *update_timer;
  QLabel *connection_status_value;
  bool compact;
  QWidget *central_widget;
  QVBoxLayout *central_widget_layout;
  QIcon program_icon;
  QLabel *input_state_label = nullptr;
  void setup_menu_bar();
  QLayout* setup_header();
  QWidget* setup_tab_widget();
  QLayout* setup_footer();
  void retranslate();
  void adjust_sizes();
  void add_tab(QWidget * tab, QString name, bool hidden);
  tab_spec & find_tab_spec(QWidget * tab);
  QWidget* setup_status_page_widget();
  QLabel *input_after_scaling_label = nullptr;
  void setup_window();
  QWidget *setup_device_info_box();
  void upload_complete();
  QLabel *vin_voltage_label = nullptr;
  void set_combo_items(QComboBox * combo,
  std::vector<std::pair<const char *, uint32_t>> items);
  QLabel *operation_state_label = nullptr;
  void set_combo(QComboBox * combo, uint32_t value);
  void set_spin_box(QSpinBox * box, int value);
  void set_double_spin_box(QDoubleSpinBox * spin, double value);
  void set_check_box(QCheckBox * check, bool value);
  QLabel *energized_label = nullptr;
  void update_manual_target_controls();
  QLabel *homing_active_label = nullptr;
  void center_at_startup_if_needed();
  QCheckBox * hp_abt_check;
  QLabel * hp_tdecay_label;
  time_spin_box * hp_tdecay_value;
  QLabel *current_position_label = nullptr;
  QLabel *decay_mode_label = nullptr;
  QComboBox *decay_mode_value = nullptr;
  QLabel *agc_mode_label = nullptr;
  QComboBox *agc_mode_value = nullptr;
  QLabel *agc_bottom_current_limit_label = nullptr;
  QComboBox *agc_bottom_current_limit_value = nullptr;
  QLabel *agc_current_boost_steps_label = nullptr;
  QComboBox *agc_current_boost_steps_value = nullptr;
  QLabel *agc_frequency_limit_label = nullptr;
  QComboBox *agc_frequency_limit_value = nullptr;
  QLabel *last_motor_driver_error_label = nullptr;
  QLabel *last_motor_driver_error_value = nullptr;
  QLabel *position_uncertain_label = nullptr;
  QWidget *hp_motor_widget = nullptr;
  QLabel *current_velocity_label = nullptr;
  QList<tab_spec> tab_specs;
  QTabWidget *tab_widget = nullptr;
  QLabel *current_position_halts_label = nullptr;
  current_spin_box *current_limit_value = nullptr;
  QLabel *current_limit_warning_label = nullptr;
  QLabel *speed_max_label = nullptr;
  QWidget *manual_target_widget = nullptr;
  QLabel *accel_max_label = nullptr;
  QAction *apply_settings_action = nullptr;
  QAction *open_settings_action = nullptr;
  QAction *save_settings_action = nullptr;
  QAction *disconnect_action = nullptr;
  QAction *clear_driver_error_action = nullptr;
  QAction *go_home_reverse_action = nullptr;
  QAction *go_home_forward_action = nullptr;
  QAction *reload_settings_action = nullptr;
  QAction *restore_defaults_action = nullptr;
  QLabel *starting_speed_label = nullptr;
  QLabel *decel_max_label = nullptr;
  QLabel *encoder_position_value = nullptr;
  QLabel *input_state_value = nullptr;
  QLabel *input_after_averaging_value = nullptr;
  QLabel *input_after_averaging_label = nullptr;
  QLabel *input_after_hysteresis_value = nullptr;
  QLabel *input_after_hysteresis_label = nullptr;
  QLabel *input_after_scaling_value = nullptr;
  QLabel *vin_voltage_value = nullptr;
  QLabel *operation_state_value = nullptr;
  QLabel *energized_value = nullptr;
  QLabel *limit_active_value = nullptr;
  QLabel *limit_active_label = nullptr;
  QLabel *homing_active_value = nullptr;
  QLabel *step_mode_label = nullptr;
  QLabel *input_before_scaling_value = nullptr;
  QLabel *input_before_scaling_pretty = nullptr;
  QLabel *input_before_scaling_label = nullptr;
  QLabel *current_limit_label = nullptr;
  QLabel *hp_toff_label = nullptr;
  QList<error_row> error_rows;
  QWidget * advanced_settings_page_widget;
  QGridLayout * advanced_settings_page_layout;
  QGroupBox * pin_config_box;
  QGridLayout * pin_config_box_layout;
  std::array<pin_config_row *, 5> pin_config_rows;
  QGroupBox * error_settings_box;
  QGridLayout * error_settings_box_layout;
  QButtonGroup * soft_error_response_radio_group;
  QSpinBox * soft_error_position_value;
  QCheckBox * current_limit_during_error_check;
  current_spin_box * current_limit_during_error_value;
  QLabel * current_limit_during_error_warning_label;
  QGroupBox * misc_settings_box;
  QCheckBox * disable_safe_start_check;
  QCheckBox * ignore_err_line_high_check;
  QCheckBox * auto_clear_driver_error_check;
  QCheckBox * never_sleep_check;
  QCheckBox * hp_enable_unrestricted_current_limits_check;
  QLabel * vin_calibration_label;
  QSpinBox * vin_calibration_value;
  QGroupBox * homing_settings_box = nullptr;
  QLabel * auto_homing_label;
  QCheckBox * auto_homing_check;
  QLabel * auto_homing_direction_label;
  QComboBox * auto_homing_direction_value;
  QLabel * homing_speed_towards_label;
  QSpinBox * homing_speed_towards_value;
  QLabel * homing_speed_towards_value_pretty;
  QLabel * homing_speed_away_label;
  QSpinBox * homing_speed_away_value;
  QLabel * homing_speed_away_value_pretty;
  QGroupBox *input_status_box = nullptr;
  QGridLayout *input_status_box_layout = nullptr;
  QGroupBox *operation_status_box = nullptr;
  QGridLayout *operation_status_box_layout = nullptr;
  QGroupBox *manual_target_box = nullptr;
  QGroupBox *errors_box = nullptr;
  QPushButton *set_current_position_button = nullptr;
  QPushButton *decelerate_button = nullptr;
  QPushButton *halt_button = nullptr;
  QPushButton *errors_reset_counts_button = nullptr;
  QShortcut *manual_target_return_key_shortcut = nullptr;
  QShortcut *manual_target_enter_key_shortcut = nullptr;
  QVBoxLayout *manual_target_mode_layout = nullptr;
  time_spin_box *hp_toff_value = nullptr;
  time_spin_box *hp_tblank_value = nullptr;
  QWidget * input_motor_settings_page_widget = nullptr;
  QWidget * control_mode_widget = nullptr;
  QGridLayout * control_mode_widget_layout = nullptr;
  QGroupBox* setup_serial_settings_box();
  QGroupBox* setup_conditioning_settings_box();
  QGroupBox* setup_motor_settings_box();
  QHBoxLayout * footer_layout;
  QPushButton * deenergize_button;
  QPushButton * resume_button;
  elided_label * motor_status_value;
  QLabel * apply_settings_label;
  QPushButton * apply_settings_button;
  uint32_t apply_settings_animation_count = 0;
  main_controller * controller;
  friend class pin_config_row;
  QLabel *target_label = nullptr;
  QLabel *target_value = nullptr;
  QLabel *target_velocity_pretty = nullptr;
  QLabel *current_position_value = nullptr;
  QLabel *position_uncertain_value = nullptr;
  QLabel *current_velocity_value = nullptr;
  QLabel *current_velocity_pretty = nullptr;
  QLabel *manual_target_min_pretty = nullptr;
  QLabel *manual_target_max_pretty = nullptr;
  QSpinBox *serial_baud_rate_value = nullptr;
  QSpinBox *serial_device_number_value = nullptr;
  QCheckBox *serial_enable_alt_device_number_check = nullptr;
  QCheckBox *serial_14bit_device_number_check = nullptr;
  QSpinBox *serial_response_delay_value = nullptr;
  QCheckBox *serial_crc_for_commands_check = nullptr;
  QCheckBox *serial_crc_for_responses_check = nullptr;
  QCheckBox *serial_7bit_responses_check = nullptr;
  QCheckBox *command_timeout_check = nullptr;
  QSpinBox *encoder_prescaler_value = nullptr;
  QSpinBox *encoder_postscaler_value = nullptr;
  QCheckBox *encoder_unlimited_check = nullptr;
  QCheckBox *input_averaging_enabled_check = nullptr;
  QSpinBox *input_hysteresis_value = nullptr;
  QCheckBox *input_invert_check = nullptr;
  QSpinBox *input_min_value = nullptr;
  QSpinBox *input_neutral_min_value = nullptr;
  QSpinBox *input_neutral_max_value = nullptr;
  QSpinBox *input_max_value = nullptr;
  QSpinBox *output_min_value = nullptr;
  QSpinBox *output_max_value = nullptr;
  QComboBox *input_scaling_degree_value = nullptr;
  InputWizard *input_wizard = nullptr;
  QCheckBox *invert_motor_direction_check = nullptr;
  QSpinBox *speed_max_value = nullptr;
  QLabel *speed_max_value_pretty = nullptr;
  QSpinBox *starting_speed_value = nullptr;
  QLabel *starting_speed_value_pretty = nullptr;
  QSpinBox *accel_max_value = nullptr;
  QLabel *accel_max_value_pretty = nullptr;
  QSpinBox *decel_max_value = nullptr;
  QLabel *decel_max_value_pretty = nullptr;
  QCheckBox *decel_accel_max_same_check = nullptr;
  bool suppress_events = false;
  bool start_event_reported = false;
  QString directory_hint;
  int32_t manual_target_position_min = 0;
  int32_t manual_target_position_max = 0;
  int32_t manual_target_velocity_min = 0;
  int32_t manual_target_velocity_max = 0;
  uint8_t cached_input_state = 0;
  int32_t cached_input_after_scaling = 0;
  QComboBox *device_list_value = nullptr;
  QSpinBox *manual_target_entry_value = nullptr;
  BallScrollBar *manual_target_scroll_bar = nullptr;
  QCheckBox *auto_zero_target_check = nullptr;
  QCheckBox *auto_set_target_check = nullptr;
  QLabel *manual_target_entry_pretty = nullptr;
  QRadioButton *manual_target_position_mode_radio = nullptr;
  QRadioButton *manual_target_velocity_mode_radio = nullptr;
  QSpinBox *current_position_entry_value = nullptr;
  QComboBox *control_mode_value = nullptr;
  QDoubleSpinBox *command_timeout_value = nullptr;
  QSpinBox *serial_alt_device_number_value = nullptr;

protected:
  void showEvent(QShowEvent *) override;
  void closeEvent(QCloseEvent *) override;

private slots:
  void on_current_limit_during_error_check_stateChanged(int state);
  void on_current_limit_during_error_value_valueChanged(int value);
  void on_disable_safe_start_check_stateChanged(int state);
  void on_ignore_err_line_high_check_stateChanged(int state);
  void on_auto_clear_driver_error_check_stateChanged(int state);
  void on_never_sleep_check_stateChanged(int state);
  void on_hp_enable_unrestricted_current_limits_check_stateChanged(int state);
  void on_vin_calibration_value_valueChanged(int value);
  void on_auto_homing_check_stateChanged(int state);
  void on_auto_homing_direction_value_currentIndexChanged(int index);
  void on_homing_speed_towards_value_valueChanged(int value);
  void on_homing_speed_away_value_valueChanged(int value);
  void on_soft_error_position_value_valueChanged(int value);
  void on_hp_tdecay_value_valueChanged(int value);
  void on_hp_abt_check_stateChanged(int state);
  void on_soft_error_response_radio_group_buttonToggled(int id, bool checked);
  void on_open_settings_action_triggered();
  void on_save_settings_action_triggered();
  void on_disconnect_action_triggered();
  void on_clear_driver_error_action_triggered();
  void on_go_home_reverse_action_triggered();
  void on_go_home_forward_action_triggered();
  void on_reload_settings_action_triggered();
  void on_restore_defaults_action_triggered();
  void on_update_timer_timeout();
  void on_device_name_value_linkActivated();
  void on_documentation_action_triggered();
  void on_about_action_triggered();
  void on_device_list_value_currentIndexChanged(int index);
  void on_deenergize_button_clicked();
  void on_resume_button_clicked();
  void on_errors_reset_counts_button_clicked();
  void on_manual_target_position_mode_radio_toggled(bool checked);
  void on_manual_target_scroll_bar_valueChanged(int value);
  void on_manual_target_scroll_bar_scrollingFinished();
  void on_manual_target_min_value_valueChanged(int value);
  void on_manual_target_max_value_valueChanged(int value);
  void on_manual_target_entry_value_valueChanged(int value);
  void on_manual_target_return_key_shortcut_activated();
  void on_set_target_button_clicked();
  void on_auto_set_target_check_stateChanged(int state);
  void on_auto_zero_target_check_stateChanged(int state);
  void on_set_current_position_button_clicked();
  void on_halt_button_clicked();
  void on_decelerate_button_clicked();
  void on_apply_settings_action_triggered();
  void on_upgrade_firmware_action_triggered();
  void on_control_mode_value_currentIndexChanged(int index);
  void on_serial_baud_rate_value_valueChanged(int value);
  void on_serial_baud_rate_value_editingFinished();
  void on_serial_device_number_value_valueChanged(int value);
  void on_serial_alt_device_number_value_valueChanged(int value);
  void on_serial_enable_alt_device_number_check_stateChanged(int state);
  void on_serial_14bit_device_number_check_stateChanged(int state);
  void on_command_timeout_check_stateChanged(int state);
  void on_command_timeout_value_valueChanged(double value);
  void on_serial_crc_for_commands_check_stateChanged(int state);
  void on_serial_crc_for_responses_check_stateChanged(int state);
  void on_serial_7bit_responses_check_stateChanged(int state);
  void on_serial_response_delay_value_valueChanged(int value);
  void on_encoder_prescaler_value_valueChanged(int value);
  void on_encoder_postscaler_value_valueChanged(int value);
  void on_encoder_unlimited_check_stateChanged(int state);
  void on_input_hysteresis_value_valueChanged(int value);
  void on_input_averaging_enabled_check_stateChanged(int state);
  void on_input_learn_button_clicked();
  void on_input_invert_check_stateChanged(int state);
  void on_input_min_value_valueChanged(int value);
  void on_input_neutral_min_value_valueChanged(int value);
  void on_input_neutral_max_value_valueChanged(int value);
  void on_input_max_value_valueChanged(int value);
  void on_output_min_value_valueChanged(int value);
  void on_output_max_value_valueChanged(int value);
  void on_input_scaling_degree_value_currentIndexChanged(int index);
  void on_invert_motor_direction_check_stateChanged(int state);
  void on_speed_max_value_valueChanged(int value);
  void on_starting_speed_value_valueChanged(int value);
  void on_accel_max_value_valueChanged(int value);
  void on_decel_max_value_valueChanged(int value);
  void on_decel_accel_max_same_check_stateChanged(int state);
  void on_step_mode_value_currentIndexChanged(int index);
  void on_current_limit_value_valueChanged(int value);
  void on_decay_mode_value_currentIndexChanged(int index);
  void on_agc_mode_value_currentIndexChanged(int index);
  void on_agc_bottom_current_limit_value_currentIndexChanged(int index);
  void on_agc_current_boost_steps_value_currentIndexChanged(int index);
  void on_agc_frequency_limit_value_currentIndexChanged(int index);
  void on_hp_toff_value_valueChanged(int value);
  void on_hp_tblank_value_valueChanged(int value);
};
