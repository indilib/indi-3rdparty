#include "main_window.h"
#include "main_controller.h"
#include "config.h"
#include "to_string.h"

#include "BallScrollBar.h"
#include "current_spin_box.h"
#include "time_spin_box.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QShortcut>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cassert>
#include <cmath>

#ifdef QT_STATIC
#include <QtPlugin>
#ifdef _WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#endif
#endif
#ifdef __linux__
Q_IMPORT_PLUGIN(QLinuxFbIntegrationPlugin)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif
#ifdef __APPLE__
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#endif
#endif

#define UINT12_MAX 0xFFF // 4095

// Keyboard shortcuts:
// Alt+A: Apply Settings
// Alt+C: Set current position
// Alt+D: Device menu
// Alt+E: Decelerate motor
// Alt+F: File menu
// Alt+G: De-energize
// Alt+H: Help menu
// Alt+L: Halt motor
// Alt+N: Learn...
// Alt+O: Reset counts
// Alt+P: Set position mode
// Alt+R: Resume
// Alt+T: Set target
// Alt+V: Set velocity mode
// Alt+W: Set target when slider or entry box are changed
// Alt+Z: Return slider to zero when it is released
// Ctrl+D: Disconnect
// Ctrl+O: Open settings file...
// Ctrl+P: Apply settings
// Ctrl+S: Save settings file...

main_window::main_window(QWidget * parent)
  : QMainWindow(parent)
{
  setup_window();
}

void main_window::set_controller(main_controller * controller)
{
  this->controller = controller;
}

bootloader_window * main_window::open_bootloader_window()
{
  bootloader_window * window = new bootloader_window(this);
  connect(window, &bootloader_window::upload_complete,
    this, &main_window::upload_complete);
  window->setWindowModality(Qt::ApplicationModal);
  window->setAttribute(Qt::WA_DeleteOnClose);
  window->show();
  return window;
}

void main_window::set_update_timer_interval(uint32_t interval_ms)
{
  assert(update_timer);
  assert(interval_ms <= std::numeric_limits<int>::max());
  update_timer->setInterval(interval_ms);
}

void main_window::start_update_timer()
{
  assert(update_timer);
  update_timer->start();
}

void main_window::show_error_message(const std::string & message)
{
  QMessageBox mbox(QMessageBox::Critical, windowTitle(),
    QString::fromStdString(message), QMessageBox::NoButton, this);
  mbox.exec();
}

void main_window::show_warning_message(const std::string & message)
{
  QMessageBox mbox(QMessageBox::Warning, windowTitle(),
    QString::fromStdString(message), QMessageBox::NoButton, this);
  mbox.exec();
}

void main_window::show_info_message(const std::string & message)
{
  QMessageBox mbox(QMessageBox::Information, windowTitle(),
    QString::fromStdString(message), QMessageBox::NoButton, this);
  mbox.exec();
}

bool main_window::confirm(const std::string & question)
{
  QMessageBox mbox(QMessageBox::Question, windowTitle(),
    QString::fromStdString(question), QMessageBox::Ok | QMessageBox::Cancel, this);
  int button = mbox.exec();
  return button == QMessageBox::Ok;
}

bool main_window::warn_and_confirm(const std::string & question)
{
  QMessageBox mbox(QMessageBox::Warning, windowTitle(),
    QString::fromStdString(question), QMessageBox::Ok | QMessageBox::Cancel, this);
  mbox.setDefaultButton(QMessageBox::Cancel);
  int button = mbox.exec();
  return button == QMessageBox::Ok;
}

void main_window::set_device_list_contents(const std::vector<tic::device> & device_list)
{
  suppress_events = true;
  device_list_value->clear();
  device_list_value->addItem(tr("Not connected"), QString()); // null value
  for (const tic::device & device : device_list)
  {
    device_list_value->addItem(
      QString::fromStdString(device.get_short_name() +
        " #" + device.get_serial_number()),
      QString::fromStdString(device.get_os_id()));
  }
  suppress_events = false;
}

void main_window::set_device_list_selected(const tic::device & device)
{
  suppress_events = true;
  int index = 0;
  if (device)
  {
    index = device_list_value->findData(QString::fromStdString(device.get_os_id()));
  }
  device_list_value->setCurrentIndex(index);
  suppress_events = false;
}

void main_window::set_connection_status(const std::string & status, bool error)
{
  if (error)
  {
    connection_status_value->setStyleSheet("color: red;");
  }
  else
  {
    connection_status_value->setStyleSheet("");
  }
  connection_status_value->setText(QString::fromStdString(status));
}

void main_window::adjust_ui_for_product(uint8_t product)
{
  bool decay_mode_visible = false;
  bool agc_mode_visible = false;
  bool last_motor_driver_error_visible = false;
  bool hp_visible = false;

  switch (product)
  {
  default:
  case TIC_PRODUCT_T825:
  case TIC_PRODUCT_N825:
    set_combo_items(step_mode_value,
      { { "Full step", TIC_STEP_MODE_MICROSTEP1 },
        { "1/2 step", TIC_STEP_MODE_MICROSTEP2 },
        { "1/4 step", TIC_STEP_MODE_MICROSTEP4 },
        { "1/8 step", TIC_STEP_MODE_MICROSTEP8 },
        { "1/16 step", TIC_STEP_MODE_MICROSTEP16 },
        { "1/32 step", TIC_STEP_MODE_MICROSTEP32 } });

    set_combo_items(decay_mode_value,
      { { "Slow", TIC_DECAY_MODE_T825_SLOW },
        { "Mixed", TIC_DECAY_MODE_T825_MIXED },
        { "Fast", TIC_DECAY_MODE_T825_FAST } });
    decay_mode_visible = true;
    break;

  case TIC_PRODUCT_T834:
    set_combo_items(step_mode_value,
      { { "Full step", TIC_STEP_MODE_MICROSTEP1 },
        { "1/2 step", TIC_STEP_MODE_MICROSTEP2 },
        { "1/4 step", TIC_STEP_MODE_MICROSTEP4 },
        { "1/8 step", TIC_STEP_MODE_MICROSTEP8 },
        { "1/16 step", TIC_STEP_MODE_MICROSTEP16 },
        { "1/32 step", TIC_STEP_MODE_MICROSTEP32 } });

    set_combo_items(decay_mode_value,
      { { "Slow", TIC_DECAY_MODE_T834_SLOW },
        { "Mixed 25%", TIC_DECAY_MODE_T834_MIXED25 },
        { "Mixed 50%", TIC_DECAY_MODE_T834_MIXED50 },
        { "Mixed 75%", TIC_DECAY_MODE_T834_MIXED75 },
        { "Fast", TIC_DECAY_MODE_T834_FAST } });
    decay_mode_visible = true;
    break;

  case TIC_PRODUCT_T500:
    set_combo_items(step_mode_value,
      { { "Full step", TIC_STEP_MODE_MICROSTEP1 },
        { "1/2 step", TIC_STEP_MODE_MICROSTEP2 },
        { "1/4 step", TIC_STEP_MODE_MICROSTEP4 },
        { "1/8 step", TIC_STEP_MODE_MICROSTEP8 } });

    set_combo_items(decay_mode_value,
      { { "Auto", TIC_DECAY_MODE_T500_AUTO } });
    break;

  case TIC_PRODUCT_T249:
    set_combo_items(step_mode_value,
      { { "Full step 100%", TIC_STEP_MODE_MICROSTEP1 },
        { "1/2 step", TIC_STEP_MODE_MICROSTEP2 },
        { "1/2 step 100%", TIC_STEP_MODE_MICROSTEP2_100P },
        { "1/4 step", TIC_STEP_MODE_MICROSTEP4 },
        { "1/8 step", TIC_STEP_MODE_MICROSTEP8 },
        { "1/16 step", TIC_STEP_MODE_MICROSTEP16 },
        { "1/32 step", TIC_STEP_MODE_MICROSTEP32 } });

    set_combo_items(decay_mode_value,
      { { "Mixed", TIC_DECAY_MODE_T249_MIXED } });

    agc_mode_visible = true;
    last_motor_driver_error_visible = true;
    break;

  case TIC_PRODUCT_36V4:
    set_combo_items(step_mode_value,
      { { "Full step", TIC_STEP_MODE_MICROSTEP1 },
        { "1/2 step", TIC_STEP_MODE_MICROSTEP2 },
        { "1/4 step", TIC_STEP_MODE_MICROSTEP4 },
        { "1/8 step", TIC_STEP_MODE_MICROSTEP8 },
        { "1/16 step", TIC_STEP_MODE_MICROSTEP16 },
        { "1/32 step", TIC_STEP_MODE_MICROSTEP32 },
        { "1/64 step", TIC_STEP_MODE_MICROSTEP64 },
        { "1/128 step", TIC_STEP_MODE_MICROSTEP128 },
        { "1/256 step", TIC_STEP_MODE_MICROSTEP256 } });

    set_combo_items(decay_mode_value,
      { { "Slow", TIC_HP_DECMOD_SLOW },
        { "Slow / mixed", TIC_HP_DECMOD_SLOW_MIXED },
        { "Fast", TIC_HP_DECMOD_FAST },
        { "Mixed", TIC_HP_DECMOD_MIXED },
        { "Slow / auto-mixed", TIC_HP_DECMOD_SLOW_AUTO_MIXED },
        { "Auto-mixed", TIC_HP_DECMOD_AUTO_MIXED }});
    decay_mode_visible = true;
    last_motor_driver_error_visible = true;
    hp_visible = true;
    break;
  }

  decay_mode_label->setVisible(decay_mode_visible);
  decay_mode_value->setVisible(decay_mode_visible);

  agc_mode_label->setVisible(agc_mode_visible);
  agc_mode_value->setVisible(agc_mode_visible);
  agc_bottom_current_limit_label->setVisible(agc_mode_visible);
  agc_bottom_current_limit_value->setVisible(agc_mode_visible);
  agc_current_boost_steps_label->setVisible(agc_mode_visible);
  agc_current_boost_steps_value->setVisible(agc_mode_visible);
  agc_frequency_limit_label->setVisible(agc_mode_visible);
  agc_frequency_limit_value->setVisible(agc_mode_visible);

  last_motor_driver_error_label->setVisible(last_motor_driver_error_visible);
  last_motor_driver_error_value->setVisible(last_motor_driver_error_visible);

  hp_enable_unrestricted_current_limits_check->setVisible(hp_visible);
  hp_motor_widget->setVisible(hp_visible);

  if (hp_visible && product != TIC_PRODUCT_36V4)
  {
    // Need to update the hp_enable_unrestricted_current_limits_check tooltip.
    assert(0);
  }

  update_current_limit_table(product);
}

void main_window::update_shown_tabs()
{
  int widget_index = 0;
  for (int i = 0; i < tab_specs.count(); i++)
  {
    tab_spec & ts = tab_specs[i];
    if (ts.hidden)
    {
      // Make sure this tab is not visible.
      // Instead of just calling tab_widget.removeTab, we call `tab.setParent`
      // so that the tab is still in the tree of Qt objects and will be
      // destroyed properly (though that does not matter much for the main
      // window of the application).
      ts.tab->setParent(tab_widget);
    }
    else
    {
      // Make sure the tab is visible at widget_index.
      if (tab_widget->widget(widget_index) != ts.tab)
      {
        tab_widget->insertTab(widget_index, ts.tab, ts.name);
      }
      widget_index++;
    }
  }

  // Assumption: Any tab in the tab widget's main list already has a
  // corresponding tab spec, so we already processed it and there is no need
  // for an extra loop to hide it.
}

void main_window::update_current_limit_table(uint8_t product)
{
  size_t code_count;
  const uint8_t * code_table =
    tic_get_recommended_current_limit_codes(product, &code_count);

  QMap<int, int> mapping;
  for (size_t i = 0; i < code_count; i++)
  {
    uint8_t code = code_table[i];
    uint32_t current = tic_current_limit_code_to_ma(product, code);
    mapping.insert(code, current);
  }

  suppress_events = true;
  current_limit_value->set_mapping(mapping);
  current_limit_during_error_value->set_mapping(mapping);
  suppress_events = false;
}

void main_window::update_current_limit_warnings()
{
  int threshold = std::numeric_limits<int>::max();
  if (controller->get_product() == TIC_PRODUCT_36V4)
  {
    threshold = 4000;
  }

  current_limit_warning_label->setVisible(
    current_limit_value->value() > threshold);
  current_limit_during_error_warning_label->setVisible(
    current_limit_during_error_value->value() > threshold);
}

void main_window::set_tab_pages_enabled(bool enabled)
{
  for (int i = 0; i < tab_widget->count(); i++)
  {
    tab_widget->widget(i)->setEnabled(enabled);
  }
}

void main_window::set_manual_target_enabled(bool enabled)
{
  manual_target_widget->setEnabled(enabled);
}

void main_window::set_deenergize_button_enabled(bool enabled)
{
  deenergize_button->setEnabled(enabled);
}

void main_window::set_resume_button_enabled(bool enabled)
{
  resume_button->setEnabled(enabled);
}

void main_window::set_apply_settings_enabled(bool enabled)
{
  apply_settings_button->setEnabled(enabled);
  apply_settings_action->setEnabled(enabled);
  apply_settings_label->setVisible(enabled);
  apply_settings_button->setToolTip(
    enabled ? apply_settings_label->toolTip() : "");
}

void main_window::set_open_save_settings_enabled(bool enabled)
{
  open_settings_action->setEnabled(enabled);
  save_settings_action->setEnabled(enabled);
}

void main_window::set_disconnect_enabled(bool enabled)
{
  disconnect_action->setEnabled(enabled);
}

void main_window::set_reload_settings_enabled(bool enabled)
{
  reload_settings_action->setEnabled(enabled);
}

void main_window::set_restore_defaults_enabled(bool enabled)
{
  restore_defaults_action->setEnabled(enabled);
}

void main_window::set_clear_driver_error_enabled(bool enabled)
{
  clear_driver_error_action->setEnabled(enabled);
}

void main_window::set_go_home_enabled(bool reverse_enabled,
  bool forward_enabled)
{
  go_home_reverse_action->setEnabled(reverse_enabled);
  go_home_forward_action->setEnabled(forward_enabled);
}

void main_window::set_device_name(const std::string & name, bool link_enabled)
{
  QString text = QString::fromStdString(name);
  if (link_enabled)
  {
    text = "<a href=\"#doc\">" + text + "</a>";
  }
  device_name_value->setText(text);
}

void main_window::set_serial_number(const std::string & serial_number)
{
  serial_number_value->setText(QString::fromStdString(serial_number));
}

void main_window::set_firmware_version(const std::string & firmware_version)
{
  firmware_version_value->setText(QString::fromStdString(firmware_version));
}

void main_window::set_device_reset(const std::string & device_reset)
{
  device_reset_value->setText(QString::fromStdString(device_reset));
}

void main_window::set_up_time(uint32_t up_time)
{
  up_time_value->setText(QString::fromStdString(
    convert_up_time_to_hms_string(up_time)));
}

void main_window::set_encoder_position(int32_t encoder_position)
{
  encoder_position_value->setText(QString::number(encoder_position));
}

void main_window::set_input_before_scaling(uint16_t input_before_scaling, uint8_t control_mode)
{
  bool input_not_null = (input_before_scaling != TIC_INPUT_NULL);

  if (input_not_null)
  {
    input_before_scaling_value->setText(QString::number(input_before_scaling));

    switch (control_mode)
    {
    case TIC_CONTROL_MODE_RC_POSITION:
    case TIC_CONTROL_MODE_RC_SPEED:
      input_before_scaling_pretty->setText("(" + QString::fromStdString(
        convert_input_to_us_string(input_before_scaling)) + ")");
      break;

    case TIC_CONTROL_MODE_ANALOG_POSITION:
    case TIC_CONTROL_MODE_ANALOG_SPEED:
      input_before_scaling_pretty->setText("(" + QString::fromStdString(
        convert_input_to_v_string(input_before_scaling)) + ")");
      break;

    default:
      input_before_scaling_pretty->setText("");
    }
  }
  else
  {
    input_before_scaling_value->setText(tr("N/A"));
    input_before_scaling_pretty->setText("");
  }

  input_before_scaling_label->setEnabled(input_not_null);
  input_before_scaling_value->setEnabled(input_not_null);
  input_before_scaling_pretty->setEnabled(input_not_null);

  if (input_wizard->isVisible()) { input_wizard->handle_input(input_before_scaling); }
}

void main_window::set_input_state(const std::string & input_state, uint8_t input_state_raw)
{
  input_state_value->setText(QString::fromStdString(input_state));
  cached_input_state = input_state_raw;
}

void main_window::set_input_after_averaging(uint16_t input_after_averaging)
{
  bool input_not_null = (input_after_averaging != TIC_INPUT_NULL);

  input_after_averaging_value->setText(input_not_null ?
    QString::number(input_after_averaging) : tr("N/A"));

  input_after_averaging_label->setEnabled(input_not_null);
  input_after_averaging_value->setEnabled(input_not_null);
}

void main_window::set_input_after_hysteresis(uint16_t input_after_hysteresis)
{
  bool input_not_null = (input_after_hysteresis != TIC_INPUT_NULL);

  input_after_hysteresis_value->setText(input_not_null ?
    QString::number(input_after_hysteresis) : tr("N/A"));

  input_after_hysteresis_label->setEnabled(input_not_null);
  input_after_hysteresis_value->setEnabled(input_not_null);
}

void main_window::set_input_after_scaling(int32_t input_after_scaling)
{
  input_after_scaling_value->setText(QString::number(input_after_scaling));
  cached_input_after_scaling = input_after_scaling;
}

void main_window::set_vin_voltage(uint32_t vin_voltage)
{
  vin_voltage_value->setText(QString::fromStdString(
    convert_mv_to_v_string(vin_voltage)));
}

void main_window::set_operation_state(const std::string & operation_state)
{
  operation_state_value->setText(QString::fromStdString(operation_state));
}

void main_window::set_energized(bool energized)
{
  energized_value->setText(energized ? tr("Yes") : tr("No"));
}

void main_window::set_limit_active(bool forward_limit_active,
  bool reverse_limit_active)
{
  // setStyleSheet() is expensive, so only call it if something actually
  // changed.
  bool styled = !limit_active_value->styleSheet().isEmpty();
  bool want_style = false;

  if (forward_limit_active && reverse_limit_active) {
    limit_active_value->setText(tr("Both"));
    want_style = true;
  }
  else if (forward_limit_active) {
    limit_active_value->setText(tr("Forward"));
    want_style = true;
  }
  else if (reverse_limit_active) {
    limit_active_value->setText(tr("Reverse"));
    want_style = true;
  }
  else {
    limit_active_value->setText(tr("None"));
  }

  if (styled && !want_style)
  {
    limit_active_value->setStyleSheet("");
  }
  if (!styled && want_style)
  {
    limit_active_value->setStyleSheet(":enabled { background-color: yellow; }");
  }

  limit_active_label->setEnabled(true);
  limit_active_value->setEnabled(true);
}

void main_window::disable_limit_active()
{
  limit_active_value->setText(tr("N/A"));
  limit_active_value->setStyleSheet("");
  limit_active_label->setEnabled(false);
  limit_active_value->setEnabled(false);
}

void main_window::set_homing_active(bool active)
{
  homing_active_value->setText(active ? tr("Yes") : tr("No"));
}

void main_window::set_last_motor_driver_error(const char * str)
{
  last_motor_driver_error_value->setText(str);
  last_motor_driver_error_value->setToolTip("");
}

void main_window::set_last_hp_driver_errors(uint8_t errors)
{
  const char * name = tic_look_up_hp_driver_error_name_ui(errors);
  QString toolTip = "0x" + QString::number(errors, 16);
  last_motor_driver_error_value->setText(name);
  last_motor_driver_error_value->setToolTip(toolTip);
}

void main_window::set_target_position(int32_t target_position)
{
  target_label->setText(tr("Target position:"));
  target_value->setText(QString::number(target_position));
  target_velocity_pretty->setText("");
}

void main_window::set_target_velocity(int32_t target_velocity)
{
  target_label->setText(tr("Target velocity:"));
  target_value->setText(QString::number(target_velocity));
  target_velocity_pretty->setText("(" + QString::fromStdString(
    convert_speed_to_pps_string(target_velocity)) + ")");
}

void main_window::set_target_none()
{
  target_label->setText(tr("Target:"));
  target_value->setText(tr("No target"));
  target_velocity_pretty->setText("");
}

void main_window::set_current_position(int32_t current_position)
{
  current_position_value->setText(QString::number(current_position));
}

void main_window::set_position_uncertain(bool position_uncertain)
{
  position_uncertain_value->setText(position_uncertain ? tr("Yes") : tr("No"));
}

void main_window::set_current_velocity(int32_t current_velocity)
{
  current_velocity_value->setText(QString::number(current_velocity));
  current_velocity_pretty->setText("(" + QString::fromStdString(
    convert_speed_to_pps_string(current_velocity)) + ")");
}

void main_window::set_error_status(uint16_t error_status)
{
  for (int i = 0; i < 16; i++)
  {
    if (error_rows[i].stopping_value == NULL) { continue; }

    // setStyleSheet() is expensive, so only call it if something actually
    // changed. Check if there's currently a stylesheet applied and decide
    // whether we need to do anything based on that.
    bool styled = !error_rows[i].stopping_value->styleSheet().isEmpty();

    if (error_status & (1 << i))
    {
      error_rows[i].stopping_value->setText(tr("Yes"));
      if (!styled)
      {
        error_rows[i].stopping_value->setStyleSheet(
          ":enabled { background-color: red; color: white; }");
      }
    }
    else
    {
      error_rows[i].stopping_value->setText(tr("No"));
      if (styled)
      {
        error_rows[i].stopping_value->setStyleSheet("");
      }
    }
  }
}

void main_window::increment_errors_occurred(uint32_t errors_occurred)
{
  for (int i = 0; i < 32; i++)
  {
    if (error_rows[i].count_value == NULL) { continue; }

    if (errors_occurred & (1 << i))
    {
      error_rows[i].count++;
      error_rows[i].count_value->setText(QString::number(error_rows[i].count));
    }
  }
}

void main_window::reset_error_counts()
{
  for (int i = 0; i < 32; i++)
  {
    if (error_rows[i].count_value == NULL) { continue; }

    error_rows[i].count = 0;
    error_rows[i].count_value->setText(tr("-"));
  }
}

void main_window::set_control_mode(uint8_t control_mode)
{
  set_combo(control_mode_value, control_mode);
}

void main_window::set_manual_target_position_mode()
{
  suppress_events = true;
  manual_target_position_mode_radio->setChecked(true);
  suppress_events = false;
  update_manual_target_controls();
}

void main_window::set_manual_target_velocity_mode()
{
  suppress_events = true;
  manual_target_velocity_mode_radio->setChecked(true);
  suppress_events = false;
  update_manual_target_controls();
}

void main_window::set_manual_target_range(int32_t target_min, int32_t target_max)
{
  suppress_events = true;
  manual_target_scroll_bar->setMinimum(target_min);
  manual_target_scroll_bar->setMaximum(target_max);
  manual_target_scroll_bar->setPageStep(std::max((target_max - target_min) / 20, 1));
  manual_target_entry_value->setRange(target_min, target_max);

  if (manual_target_velocity_mode_radio->isChecked())
  {
    manual_target_min_pretty->setText(
      "(" + QString::fromStdString(convert_speed_to_pps_string(target_min)) + ")");
    manual_target_max_pretty->setText(
      "(" + QString::fromStdString(convert_speed_to_pps_string(target_max)) + ")");
  }
  else
  {
    manual_target_min_pretty->setText("");
    manual_target_max_pretty->setText("");
  }

  suppress_events = false;
}

void main_window::set_displayed_manual_target(int32_t target)
{
  suppress_events = true;
  manual_target_entry_value->setValue(target);
  manual_target_scroll_bar->setValue(target);

  if (manual_target_velocity_mode_radio->isChecked())
  {
    manual_target_entry_pretty->setText(
      "(" + QString::fromStdString(convert_speed_to_pps_string(target)) + ")");
  }
  else
  {
    manual_target_entry_pretty->setText("");
  }
  suppress_events = false;
}

void main_window::set_manual_target_ball_position(int32_t current_position, bool on_target)
{
  if (manual_target_position_mode_radio->isChecked())
  {
    manual_target_scroll_bar->setBallValue(current_position);
    manual_target_scroll_bar->setBallColor(on_target ? Qt::darkGreen : Qt::blue);
  }
}

void main_window::set_manual_target_ball_velocity(int32_t current_velocity, bool on_target)
{
  if (manual_target_velocity_mode_radio->isChecked())
  {
    manual_target_scroll_bar->setBallValue(current_velocity);
    manual_target_scroll_bar->setBallColor(on_target ? Qt::darkGreen : Qt::blue);
  }
}

void main_window::set_apply_settings_button_stylesheet(int offset)
{
  int base = 12;
  int left = base + offset;
  int right = base - offset;
  QString style = QString(
    "QPushButton:enabled {"
    "background-color: #1f2f93;"  // Pololu blue
    "color: white;"
    "font-weight: bold;"
    "padding: 0.3em %1px 0.3em %2px; }").arg(left).arg(right);
  apply_settings_button->setStyleSheet(style);
}

void main_window::animate_apply_settings_button()
{
  static const int8_t offsets[] = {
    // 2 seconds of stillness
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // move right and left
    1, 2, 3, 4, 3, 2, 1, 0, -1, -2, -3, -4, -3, -2, -1,
  };

  if (!apply_settings_button->isEnabled())
  {
    apply_settings_animation_count = 0;
    set_apply_settings_button_stylesheet(0);
    return;
  }

  apply_settings_animation_count++;
  if (apply_settings_animation_count >= sizeof(offsets)/sizeof(offsets[0]))
  {
    apply_settings_animation_count = 0;
  }
  set_apply_settings_button_stylesheet(offsets[apply_settings_animation_count]);
}

void main_window::set_serial_baud_rate(uint32_t serial_baud_rate)
{
  set_spin_box(serial_baud_rate_value, serial_baud_rate);
}

void main_window::set_serial_device_number(uint16_t number)
{
  set_spin_box(serial_device_number_value, number);
}

void main_window::set_serial_alt_device_number(uint16_t number)
{
  set_spin_box(serial_alt_device_number_value, number);
}

void main_window::set_serial_enable_alt_device_number(bool enable)
{
  set_check_box(serial_enable_alt_device_number_check, enable);
}

void main_window::set_serial_14bit_device_number(bool enable)
{
  set_check_box(serial_14bit_device_number_check, enable);
}

void main_window::set_command_timeout(uint16_t command_timeout)
{
  if (command_timeout == 0)
  {
    set_check_box(command_timeout_check, false);
    command_timeout_value->setEnabled(false);
    command_timeout = TIC_DEFAULT_COMMAND_TIMEOUT;
  }
  else
  {
    set_check_box(command_timeout_check, true);
    command_timeout_value->setEnabled(true);
  }
  set_double_spin_box(command_timeout_value, command_timeout / 1000.);
}

void main_window::set_serial_crc_for_commands(bool enable)
{
  set_check_box(serial_crc_for_commands_check, enable);
}

void main_window::set_serial_crc_for_responses(bool enable)
{
  set_check_box(serial_crc_for_responses_check, enable);
}

void main_window::set_serial_7bit_responses(bool enable)
{
  set_check_box(serial_7bit_responses_check, enable);
}

void main_window::set_serial_response_delay(uint8_t delay)
{
  set_spin_box(serial_response_delay_value, delay);
}

void main_window::set_encoder_prescaler(uint32_t encoder_prescaler)
{
  set_spin_box(encoder_prescaler_value, encoder_prescaler);
}

void main_window::set_encoder_postscaler(uint32_t encoder_postscaler)
{
  set_spin_box(encoder_postscaler_value, encoder_postscaler);
}

void main_window::set_encoder_unlimited(bool encoder_unlimited)
{
  set_check_box(encoder_unlimited_check, encoder_unlimited);
}

void main_window::set_input_averaging_enabled(bool input_averaging_enabled)
{
  set_check_box(input_averaging_enabled_check, input_averaging_enabled);
}

void main_window::set_input_hysteresis(uint16_t input_hysteresis)
{
  set_spin_box(input_hysteresis_value, input_hysteresis);
}

void main_window::set_input_invert(bool input_invert)
{
  set_check_box(input_invert_check, input_invert);
}

void main_window::set_input_min(uint16_t input_min)
{
  set_spin_box(input_min_value, input_min);
}

void main_window::set_input_neutral_min(uint16_t input_neutral_min)
{
  set_spin_box(input_neutral_min_value, input_neutral_min);
}

void main_window::set_input_neutral_max(uint16_t input_neutral_max)
{
  set_spin_box(input_neutral_max_value, input_neutral_max);
}

void main_window::set_input_max(uint16_t input_max)
{
  set_spin_box(input_max_value, input_max);
}

void main_window::set_output_min(int32_t output_min)
{
  set_spin_box(output_min_value, output_min);
}

void main_window::set_output_max(int32_t output_max)
{
  set_spin_box(output_max_value, output_max);
}

void main_window::set_input_scaling_degree(uint8_t input_scaling_degree)
{
  set_combo(input_scaling_degree_value, input_scaling_degree);
}

void main_window::run_input_wizard(uint8_t control_mode)
{
  input_wizard->set_control_mode(control_mode);
  int result = input_wizard->exec();
  if (result == QDialog::Accepted)
  {
    controller->handle_input_invert_input(input_wizard->learned_input_invert());
    controller->handle_input_min_input(input_wizard->learned_input_min());
    controller->handle_input_neutral_min_input(input_wizard->learned_input_neutral_min());
    controller->handle_input_neutral_max_input(input_wizard->learned_input_neutral_max());
    controller->handle_input_max_input(input_wizard->learned_input_max());
  }
}

void main_window::set_invert_motor_direction(bool invert_motor_direction)
{
  set_check_box(invert_motor_direction_check, invert_motor_direction);
}

void main_window::set_speed_max(uint32_t speed_max)
{
  set_spin_box(speed_max_value, speed_max);
  speed_max_value_pretty->setText(QString::fromStdString(
    convert_speed_to_pps_string(speed_max)));
}

void main_window::set_starting_speed(uint32_t starting_speed)
{
  set_spin_box(starting_speed_value, starting_speed);
  starting_speed_value_pretty->setText(QString::fromStdString(
    convert_speed_to_pps_string(starting_speed)));
}

void main_window::set_accel_max(uint32_t accel_max)
{
  set_spin_box(accel_max_value, accel_max);
  accel_max_value_pretty->setText(QString::fromStdString(
    convert_accel_to_pps2_string(accel_max)));
}

void main_window::set_decel_max(uint32_t decel_max)
{
  if (decel_max == 0)
  {
    set_check_box(decel_accel_max_same_check, true);
    decel_max_value->setEnabled(false);
    decel_max = accel_max_value->value();
  }
  else
  {
    set_check_box(decel_accel_max_same_check, false);
    decel_max_value->setEnabled(true);
  }
  set_spin_box(decel_max_value, decel_max);
  decel_max_value_pretty->setText(QString::fromStdString(
    convert_accel_to_pps2_string(decel_max)));
}

void main_window::set_step_mode(uint8_t step_mode)
{
  set_combo(step_mode_value, step_mode);
}

void main_window::set_current_limit(uint32_t current_limit)
{
  set_spin_box(current_limit_value, current_limit);
}

void main_window::set_decay_mode(uint8_t decay_mode)
{
  set_combo(decay_mode_value, decay_mode);
}

void main_window::set_agc_mode(uint8_t mode)
{
  set_combo(agc_mode_value, mode);

  // Note: Maybe this is ugly because it depends on the controller calling this
  // function whenever the AGC mode setting changes.
  bool agc_on = mode == TIC_AGC_MODE_ON;
  agc_bottom_current_limit_value->setEnabled(agc_on);
  agc_current_boost_steps_value->setEnabled(agc_on);
  agc_frequency_limit_value->setEnabled(agc_on);
}

void main_window::set_agc_bottom_current_limit(uint8_t limit)
{
  set_combo(agc_bottom_current_limit_value, limit);
}

void main_window::set_agc_current_boost_steps(uint8_t steps)
{
  set_combo(agc_current_boost_steps_value, steps);
}

void main_window::set_agc_frequency_limit(uint8_t limit)
{
  set_combo(agc_frequency_limit_value, limit);
}

void main_window::set_hp_tdecay(uint8_t time)
{
  set_spin_box(hp_tdecay_value, time);
}

void main_window::set_hp_tblank(uint8_t time)
{
  set_spin_box(hp_tblank_value, time);
}

void main_window::set_hp_abt(bool adaptive)
{
  set_check_box(hp_abt_check, adaptive);
}

void main_window::set_hp_toff(uint8_t time)
{
  set_spin_box(hp_toff_value, time);
}

void main_window::set_soft_error_response(uint8_t soft_error_response)
{
  suppress_events = true;
  QAbstractButton * radio = soft_error_response_radio_group->button(soft_error_response);
  if (radio)
  {
    radio->setChecked(true);
  }
  else
  {
    // The value doesn't correspond with any of the radio buttons, so clear
    // the currently selected button, if any.
    QAbstractButton * checked = soft_error_response_radio_group->checkedButton();
    if (checked)
    {
      soft_error_response_radio_group->setExclusive(false);
      checked->setChecked(false);
      soft_error_response_radio_group->setExclusive(true);
    }
  }
  suppress_events = false;
}

void main_window::set_soft_error_position(int32_t soft_error_position)
{
  set_spin_box(soft_error_position_value, soft_error_position);
}

void main_window::set_current_limit_during_error(int32_t current_limit_during_error)
{
  if (current_limit_during_error == -1)
  {
    set_check_box(current_limit_during_error_check, false);
    current_limit_during_error_value->setEnabled(false);
    current_limit_during_error = current_limit_value->value();
  }
  else
  {
    set_check_box(current_limit_during_error_check, true);
    current_limit_during_error_value->setEnabled(true);
  }
  set_spin_box(current_limit_during_error_value, current_limit_during_error);
}

void main_window::set_disable_safe_start(bool disable_safe_start)
{
  set_check_box(disable_safe_start_check, disable_safe_start);
}

void main_window::set_ignore_err_line_high(bool ignore_err_line_high)
{
  set_check_box(ignore_err_line_high_check, ignore_err_line_high);
}

void main_window::set_auto_clear_driver_error(bool auto_clear_driver_error)
{
  set_check_box(auto_clear_driver_error_check, auto_clear_driver_error);
}

void main_window::set_never_sleep(bool never_sleep)
{
  set_check_box(never_sleep_check, never_sleep);
}

void main_window::set_hp_enable_unrestricted_current_limits(bool enable)
{
  set_check_box(hp_enable_unrestricted_current_limits_check, enable);
}

void main_window::set_vin_calibration(int16_t vin_calibration)
{
  set_spin_box(vin_calibration_value, vin_calibration);
}

void main_window::set_auto_homing(bool auto_homing)
{
  set_check_box(auto_homing_check, auto_homing);

  // Note: Maybe this is ugly because it depends on the controller calling this
  // function whenever the auto_homing_check is changed.
  auto_homing_direction_label->setEnabled(auto_homing);
  auto_homing_direction_value->setEnabled(auto_homing);
}

void main_window::set_auto_homing_forward(bool forward)
{
  set_combo(auto_homing_direction_value, forward);
}

void main_window::set_homing_speed_towards(uint32_t speed)
{
  set_spin_box(homing_speed_towards_value, speed);
  homing_speed_towards_value_pretty->setText(QString::fromStdString(
    convert_speed_to_pps_string(speed)));
}

void main_window::set_homing_speed_away(uint32_t speed)
{
  set_spin_box(homing_speed_away_value, speed);
  homing_speed_away_value_pretty->setText(QString::fromStdString(
    convert_speed_to_pps_string(speed)));
}

void main_window::set_pin_func(uint8_t pin, uint8_t func)
{
  set_combo(pin_config_rows[pin]->func_value, func);
}

void main_window::set_pin_pullup(uint8_t pin, bool pullup, bool enabled)
{
  QCheckBox * check = pin_config_rows[pin]->pullup_check;
  if (check != NULL)
  {
    set_check_box(check, pullup);
    check->setEnabled(enabled);
  }
}

void main_window::set_pin_polarity(uint8_t pin, bool polarity, bool enabled)
{
  QCheckBox * check = pin_config_rows[pin]->polarity_check;
  if (check != NULL)
  {
    set_check_box(check, polarity);
    check->setEnabled(enabled);
  }
}

void main_window::set_pin_analog(uint8_t pin, bool analog, bool enabled)
{
  QCheckBox * check = pin_config_rows[pin]->analog_check;
  if (check != NULL)
  {
    set_check_box(check, analog);
    check->setEnabled(enabled);
  }
}

void main_window::set_motor_status_message(const std::string & message, bool stopped)
{
  // setStyleSheet() is expensive, so only call it if something actually
  // changed. Check if there's currently a stylesheet applied and decide
  // whether we need to do anything based on that.
  bool styled = !motor_status_value->styleSheet().isEmpty();

  if (!styled && stopped)
  {
    motor_status_value->setStyleSheet("color: red;");
  }
  else if (styled && !stopped)
  {
    motor_status_value->setStyleSheet("");
  }
  motor_status_value->setText(QString::fromStdString(message));
}

void main_window::set_combo_items(QComboBox * combo,
  std::vector<std::pair<const char *, uint32_t>> items)
{
  suppress_events = true;
  while (combo->count()) { combo->removeItem(combo->count() - 1); }
  for (const auto & item : items)
  {
    combo->addItem(item.first, item.second);
  }
  suppress_events = false;
}

void main_window::set_combo(QComboBox * combo, uint32_t value)
{
  suppress_events = true;
  combo->setCurrentIndex(combo->findData(value));
  suppress_events = false;
}

void main_window::set_spin_box(QSpinBox * spin, int value)
{
  // Only set the QSpinBox's value if the new value is numerically different.
  // This prevents, for example, a value of "0000" from being changed to "0"
  // while you're trying to change "10000" to "20000".
  if (spin->value() != value)
  {
    suppress_events = true;
    spin->setValue(value);
    suppress_events = false;
  }
}

void main_window::set_double_spin_box(QDoubleSpinBox * spin, double value)
{
  // Only set the QSpinBox's value if the new value is numerically different.
  // This prevents, for example, a value of "0000" from being changed to "0"
  // while you're trying to change "10000" to "20000".
  if (spin->value() != value)
  {
    suppress_events = true;
    spin->setValue(value);
    suppress_events = false;
  }
}

void main_window::set_check_box(QCheckBox * check, bool value)
{
  suppress_events = true;
  check->setChecked(value);
  suppress_events = false;
}

void main_window::update_manual_target_controls()
{
  if (manual_target_position_mode_radio->isChecked())
  {
    set_target_button->setText(tr("Se&t target position"));

    manual_target_min_value->setMinimum(INT32_MIN);
    manual_target_min_value->setValue(manual_target_position_min);
    manual_target_max_value->setMaximum(INT32_MAX);
    manual_target_max_value->setValue(manual_target_position_max);

    if (cached_input_state == TIC_INPUT_STATE_POSITION)
    {
      set_displayed_manual_target(cached_input_after_scaling);
    }
    else
    {
      set_displayed_manual_target(0);
    }
  }
  else
  {
    set_target_button->setText(tr("Se&t target velocity"));

    manual_target_min_value->setMinimum(-TIC_MAX_ALLOWED_SPEED);
    manual_target_min_value->setValue(manual_target_velocity_min);
    manual_target_max_value->setMaximum(TIC_MAX_ALLOWED_SPEED);
    manual_target_max_value->setValue(manual_target_velocity_max);

    if (cached_input_state == TIC_INPUT_STATE_VELOCITY)
    {
      set_displayed_manual_target(cached_input_after_scaling);
    }
    else
    {
      set_displayed_manual_target(0);
    }
  }
}

void main_window::center_at_startup_if_needed()
{
  // Center the window.  This fixes a strange bug on the Raspbian Jessie where
  // the window would appear in the upper left with its title bar off the
  // screen.  On other platforms, the default window position did not make much
  // sense, so it is nice to center it.
  //
  // In case this causes problems, you can set the TICGUI_CENTER environment
  // variable to "N".
  //
  // NOTE: This position issue on Raspbian is a bug in Qt that should be fixed.
  // Another workaround for it was to uncomment the lines in retranslate() that
  // set up errors_stopping_header_label, error_rows[*].name_label, and
  // manual_target_velocity_mode_radio, but then the Window would strangely
  // start in the lower right.
  auto env = QProcessEnvironment::systemEnvironment();
  if (env.value("TICGUI_CENTER") != "N")
  {
    setGeometry(
      QStyle::alignedRect(
        Qt::LeftToRight,
        Qt::AlignCenter,
        size(),
        qApp->primaryScreen()->availableGeometry()
        )
      );
  }
}

void main_window::showEvent(QShowEvent * /* event */)
{
  if (!start_event_reported)
  {
    start_event_reported = true;
    center_at_startup_if_needed();
    controller->start();
  }
}

void main_window::closeEvent(QCloseEvent * event)
{
  if (!controller->exit())
  {
    // User canceled exit when prompted about settings that have not been applied.
    event->ignore();
  }
}

void main_window::on_open_settings_action_triggered()
{
  QString filename = QFileDialog::getOpenFileName(this,
    tr("Open Settings File"), directory_hint + "/tic_settings.txt",
    tr("Text files (*.txt)"));

  if (!filename.isNull())
  {
    directory_hint = QFileInfo(filename).canonicalPath();
    controller->open_settings_from_file(filename.toStdString());
  }
}

void main_window::on_save_settings_action_triggered()
{
  QString filename = QFileDialog::getSaveFileName(this,
    tr("Save Settings File"), directory_hint + "/tic_settings.txt",
    tr("Text files (*.txt)"));

  if (!filename.isNull())
  {
    directory_hint = QFileInfo(filename).canonicalPath();
    controller->save_settings_to_file(filename.toStdString());
  }
}

void main_window::on_disconnect_action_triggered()
{
  controller->disconnect_device();
}

void main_window::on_clear_driver_error_action_triggered()
{
  controller->clear_driver_error();
}

void main_window::on_go_home_reverse_action_triggered()
{
  controller->go_home(TIC_GO_HOME_REVERSE);
}

void main_window::on_go_home_forward_action_triggered()
{
  controller->go_home(TIC_GO_HOME_FORWARD);
}

void main_window::on_reload_settings_action_triggered()
{
  controller->reload_settings();
}

void main_window::on_restore_defaults_action_triggered()
{
  controller->restore_default_settings();
}

void main_window::on_update_timer_timeout()
{
  controller->update();
  animate_apply_settings_button();
}

void main_window::on_device_name_value_linkActivated()
{
  on_documentation_action_triggered();
}

void main_window::on_documentation_action_triggered()
{
  QDesktopServices::openUrl(QUrl(DOCUMENTATION_URL));
}

void main_window::on_about_action_triggered()
{
  QMessageBox::about(this, tr("About") + " " + windowTitle(),
    QString("<h2>") + windowTitle() + "</h2>" +
    tr("<h4>Version %1</h4>"
      "<h4>Copyright &copy; %2 Pololu Corporation</h4>"
      "<p>See LICENSE.html for copyright and license information.</p>"
      "<p><a href=\"%3\">Online documentation</a></p>")
    .arg(SOFTWARE_VERSION_STRING, SOFTWARE_YEAR, DOCUMENTATION_URL));
}

void main_window::on_device_list_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }

  if (controller->disconnect_device())
  {
    QString id = device_list_value->itemData(index).toString();
    if (!id.isEmpty())
    {
      controller->connect_device_with_os_id(id.toStdString());
    }
  }
  else
  {
    // User canceled disconnect when prompted about settings that have not been
    // applied. Reset the selected device.
    controller->handle_model_changed();
  }
}

void main_window::on_deenergize_button_clicked()
{
  controller->deenergize();
}

void main_window::on_resume_button_clicked()
{
  controller->resume();
}

void main_window::on_errors_reset_counts_button_clicked()
{
  reset_error_counts();
}

void main_window::on_manual_target_position_mode_radio_toggled(bool /* checked */)
{
  if (suppress_events) { return; }
  update_manual_target_controls();
}

void main_window::on_manual_target_scroll_bar_valueChanged(int value)
{
  if (suppress_events) { return; }
  manual_target_entry_value->setValue(value);
}

void main_window::on_manual_target_scroll_bar_scrollingFinished()
{
  if (suppress_events) { return; }
  if (auto_zero_target_check->isChecked())
  {
    manual_target_scroll_bar->setValue(0);
  }
}

void main_window::on_manual_target_min_value_valueChanged(int value)
{
  if (suppress_events) { return; }

  if (manual_target_position_mode_radio->isChecked())
  {
    manual_target_position_min = value;
  }
  else
  {
    manual_target_velocity_min = value;
  }
  set_manual_target_range(value, manual_target_max_value->value());
}

void main_window::on_manual_target_max_value_valueChanged(int value)
{
  if (suppress_events) { return; }

  if (manual_target_position_mode_radio->isChecked())
  {
    manual_target_position_max = value;
  }
  else
  {
    manual_target_velocity_max = value;
  }
  set_manual_target_range(manual_target_min_value->value(), value);
}

void main_window::on_manual_target_entry_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  manual_target_scroll_bar->setValue(value);

  if (auto_set_target_check->isChecked())
  {
    on_set_target_button_clicked();
  }

  if (manual_target_velocity_mode_radio->isChecked())
  {
    manual_target_entry_pretty->setText(
      "(" + QString::fromStdString(convert_speed_to_pps_string(value)) + ")");
  }
  else
  {
    manual_target_entry_pretty->setText("");
  }
}

void main_window::on_manual_target_return_key_shortcut_activated()
{
  // Set target if enter is pressed on scroll bar or target entry spin box.
  if (manual_target_scroll_bar->hasFocus())
  {
    on_set_target_button_clicked();
  }
  else if (manual_target_entry_value->hasFocus())
  {
    manual_target_entry_value->interpretText();
    manual_target_entry_value->selectAll();
    on_set_target_button_clicked();
  }
  // Set range limit if enter is pressed on range limit spin boxes.
  else if (manual_target_min_value->hasFocus())
  {
    manual_target_min_value->interpretText();
    manual_target_min_value->selectAll();
    on_manual_target_min_value_valueChanged(manual_target_min_value->value());
  }
  else if (manual_target_max_value->hasFocus())
  {
    manual_target_max_value->interpretText();
    manual_target_max_value->selectAll();
    on_manual_target_max_value_valueChanged(manual_target_max_value->value());
  }
}

void main_window::on_set_target_button_clicked()
{
  if (manual_target_position_mode_radio->isChecked())
  {
    controller->set_target_position(manual_target_entry_value->value());
  }
  else
  {
    controller->set_target_velocity(manual_target_entry_value->value());
  }
}

void main_window::on_auto_set_target_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  if (state == Qt::Checked)
  {
    on_set_target_button_clicked();
    auto_zero_target_check->setEnabled(true);
  }
  else
  {
    auto_zero_target_check->setEnabled(false);
    auto_zero_target_check->setChecked(false);
  }
}

void main_window::on_auto_zero_target_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  if (state == Qt::Checked)
  {
    manual_target_scroll_bar->setValue(0);
  }
}

void main_window::on_halt_button_clicked()
{
  controller->halt_and_hold();
}

void main_window::on_set_current_position_button_clicked()
{
  controller->halt_and_set_position(current_position_entry_value->value());
}

void main_window::on_decelerate_button_clicked()
{
  controller->set_target_velocity(0);
}

void main_window::on_apply_settings_action_triggered()
{
  controller->apply_settings();
}

void main_window::on_upgrade_firmware_action_triggered()
{
  controller->upgrade_firmware();
}

void main_window::on_control_mode_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  uint8_t control_mode = control_mode_value->itemData(index).toUInt();
  controller->handle_control_mode_input(control_mode);
}

void main_window::on_serial_baud_rate_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_serial_baud_rate_input(value);
}

void main_window::on_serial_baud_rate_value_editingFinished()
{
  if (suppress_events) { return; }
  controller->handle_serial_baud_rate_input_finished();
}

void main_window::on_serial_device_number_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_serial_device_number_input(value);
}

void main_window::on_serial_alt_device_number_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_serial_alt_device_number_input(value);
}

void main_window::on_serial_enable_alt_device_number_check_stateChanged(
  int state)
{
  bool enable = state == Qt::Checked;
  serial_alt_device_number_value->setEnabled(enable);
  if (suppress_events) { return; }
  controller->handle_serial_enable_alt_device_number_input(enable);
}

void main_window::on_serial_14bit_device_number_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_serial_14bit_device_number_input(state == Qt::Checked);
}

void main_window::on_command_timeout_check_stateChanged(int state)
{
  // Note: set_command_timeout() (called by controller) takes care of enabling/
  // disabling the command_timeout_value spin box.
  if (suppress_events) { return; }
  if (state == Qt::Checked)
  {
    controller->handle_command_timeout_input(
      std::round(command_timeout_value->value() * 1000));
  }
  else
  {
    controller->handle_command_timeout_input(0);
  }
}

void main_window::on_serial_crc_for_commands_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_serial_crc_for_commands_input(state == Qt::Checked);
}

void main_window::on_serial_crc_for_responses_check_stateChanged(
  int state)
{
  if (suppress_events) { return; }
  controller->handle_serial_crc_for_responses_input(state == Qt::Checked);
}

void main_window::on_serial_7bit_responses_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_serial_7bit_responses_input(state == Qt::Checked);
}

void main_window::on_serial_response_delay_value_valueChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_serial_response_delay_input(state);
}

void main_window::on_command_timeout_value_valueChanged(double /* value */)
{
  if (suppress_events) { return; }
  controller->handle_command_timeout_input(
    std::round(command_timeout_value->value() * 1000));
}

void main_window::on_encoder_prescaler_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_encoder_prescaler_input(value);
}

void main_window::on_encoder_postscaler_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_encoder_postscaler_input(value);
}

void main_window::on_encoder_unlimited_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_encoder_unlimited_input(state == Qt::Checked);
}

void main_window::on_input_averaging_enabled_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_input_averaging_enabled_input(state == Qt::Checked);
}

void main_window::on_input_hysteresis_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_input_hysteresis_input(value);
}

void main_window::on_input_learn_button_clicked()
{
  controller->start_input_setup();
}

void main_window::on_input_invert_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_input_invert_input(state == Qt::Checked);
}

void main_window::on_input_min_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_input_min_input(value);
}

void main_window::on_input_neutral_min_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_input_neutral_min_input(value);
}

void main_window::on_input_neutral_max_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_input_neutral_max_input(value);
}

void main_window::on_input_max_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_input_max_input(value);
}

void main_window::on_output_min_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_output_min_input(value);
}

void main_window::on_output_max_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_output_max_input(value);
}

void main_window::on_input_scaling_degree_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  uint8_t input_scaling_degree = input_scaling_degree_value->itemData(index).toUInt();
  controller->handle_input_scaling_degree_input(input_scaling_degree);
}

void main_window::on_invert_motor_direction_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_invert_motor_direction_input(state == Qt::Checked);
}

void main_window::on_speed_max_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_speed_max_input(value);
}

void main_window::on_starting_speed_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_starting_speed_input(value);
}

void main_window::on_accel_max_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_accel_max_input(value);
}

void main_window::on_decel_max_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_decel_max_input(value);
}

void main_window::on_decel_accel_max_same_check_stateChanged(int state)
{
  // Note: set_decel_max() (called by controller) takes care of enabling/
  // disabling the decel_max_value spin box.
  if (suppress_events) { return; }
  if (state == Qt::Checked)
  {
    controller->handle_decel_max_input(0);
  }
  else
  {
    controller->handle_decel_max_input(decel_max_value->value());
  }
}

void main_window::on_step_mode_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  uint8_t step_mode = step_mode_value->itemData(index).toUInt();
  controller->handle_step_mode_input(step_mode);
}

void main_window::on_current_limit_value_valueChanged(int value)
{
  update_current_limit_warnings();
  if (suppress_events) { return; }
  controller->handle_current_limit_input(value);
}

void main_window::on_decay_mode_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  uint8_t decay_mode = decay_mode_value->itemData(index).toUInt();
  controller->handle_decay_mode_input(decay_mode);
}

void main_window::on_agc_mode_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  uint8_t mode = agc_mode_value->itemData(index).toUInt();
  controller->handle_agc_mode_input(mode);
}

void main_window::on_agc_bottom_current_limit_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  uint8_t limit = agc_bottom_current_limit_value->itemData(index).toUInt();
  controller->handle_agc_bottom_current_limit_input(limit);
}

void main_window::on_agc_current_boost_steps_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  uint8_t steps = agc_current_boost_steps_value->itemData(index).toUInt();
  controller->handle_agc_current_boost_steps_input(steps);
}

void main_window::on_agc_frequency_limit_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  uint8_t limit = agc_frequency_limit_value->itemData(index).toUInt();
  controller->handle_agc_frequency_limit_input(limit);
}

void main_window::on_hp_tdecay_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_hp_tdecay_input(value);
}

void main_window::on_hp_toff_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_hp_toff_input(value);
}

void main_window::on_hp_tblank_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_hp_tblank_input(value);
}

void main_window::on_hp_abt_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_hp_abt_input(state == Qt::Checked);
}

void main_window::on_soft_error_response_radio_group_buttonToggled(int id, bool checked)
{
  if (suppress_events) { return; }
  if (checked) { controller->handle_soft_error_response_input(id); }
}

void main_window::on_soft_error_position_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_soft_error_position_input(value);
}

void main_window::on_current_limit_during_error_check_stateChanged(int state)
{
  // Note: set_current_limit_during_error() (called by controller) takes care of
  // enabling/disabling the current_limit_during_error_value spin box.
  if (suppress_events) { return; }
  if (state == Qt::Checked)
  {
    controller->handle_current_limit_during_error_input(
      current_limit_during_error_value->value());
  }
  else
  {
    controller->handle_current_limit_during_error_input(-1);
  }
}

void main_window::on_current_limit_during_error_value_valueChanged(int value)
{
  update_current_limit_warnings();
  if (suppress_events) { return; }
  controller->handle_current_limit_during_error_input(value);
}

void main_window::on_disable_safe_start_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_disable_safe_start_input(state == Qt::Checked);
}

void main_window::on_ignore_err_line_high_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_ignore_err_line_high_input(state == Qt::Checked);
}

void main_window::on_auto_clear_driver_error_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_auto_clear_driver_error_input(state == Qt::Checked);
}

void main_window::on_never_sleep_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_never_sleep_input(state == Qt::Checked);
}

void main_window::on_hp_enable_unrestricted_current_limits_check_stateChanged(
  int state)
{
  if (suppress_events) { return; }
  controller->handle_hp_enable_unrestricted_current_limits_input(state == Qt::Checked);
}

void main_window::on_vin_calibration_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_vin_calibration_input(value);
}

void main_window::on_auto_homing_check_stateChanged(int state)
{
  if (suppress_events) { return; }
  controller->handle_auto_homing_input(state == Qt::Checked);
}

void main_window::on_auto_homing_direction_value_currentIndexChanged(int index)
{
  if (suppress_events) { return; }
  bool forward = auto_homing_direction_value->itemData(index).toUInt();
  controller->handle_auto_homing_forward_input(forward);
}

void main_window::on_homing_speed_towards_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_homing_speed_towards_input(value);
}

void main_window::on_homing_speed_away_value_valueChanged(int value)
{
  if (suppress_events) { return; }
  controller->handle_homing_speed_away_input(value);
}

void pin_config_row::on_func_value_currentIndexChanged(int index)
{
  if (window_suppress_events()) { return; }
  uint8_t func = func_value->itemData(index).toUInt();
  window_controller()->handle_pin_func_input(pin, func);
}

void pin_config_row::on_pullup_check_stateChanged(int state)
{
  if (window_suppress_events()) { return; }
  window_controller()->handle_pin_pullup_input(pin, state == Qt::Checked);
}

void pin_config_row::on_polarity_check_stateChanged(int state)
{
  if (window_suppress_events()) { return; }
  window_controller()->handle_pin_polarity_input(pin, state == Qt::Checked);
}

void pin_config_row::on_analog_check_stateChanged(int state)
{
  if (window_suppress_events()) { return; }
  window_controller()->handle_pin_analog_input(pin, state == Qt::Checked);
}

void main_window::upload_complete()
{
  controller->handle_upload_complete();
}

// On macOS, field labels are usually right-aligned, but we want to
// use the fusion style so we will do left-alignment instead.
//
// Code we used to use for macOS:
// #ifdef __APPLE__
// #define FIELD_LABEL_ALIGNMENT Qt::AlignRight
// #define INDENT(s) ((s) + QString("    "))
// #endif
#define FIELD_LABEL_ALIGNMENT Qt::AlignLeft
#define INDENT(s) (QString("    ") + (s))


static void setup_read_only_text_field(QGridLayout * layout, int row, int from_col,
  int value_col_span, QLabel ** label, QLabel ** value)
{
  QLabel * new_value = new QLabel();
  new_value->setTextInteractionFlags(Qt::TextSelectableByMouse);

  QLabel * new_label = new QLabel();
  new_label->setBuddy(new_value);

  layout->addWidget(new_label, row, from_col, FIELD_LABEL_ALIGNMENT);
  layout->addWidget(new_value, row, from_col + 1, 1, value_col_span, Qt::AlignLeft);

  if (label) { *label = new_label; }
  if (value) { *value = new_value; }
}

static void setup_read_only_text_field(QGridLayout * layout, int row, int from_col,
  QLabel ** label, QLabel ** value)
{
  setup_read_only_text_field(layout, row, from_col, 1, label, value);
}

static void setup_read_only_text_field(QGridLayout * layout, int row,
  QLabel ** label, QLabel ** value)
{
  setup_read_only_text_field(layout, row, 0, 1, label, value);
}

static void setup_error_row(QGridLayout * layout, int row, error_row & er)
{
  er.count = 0;

  er.name_label = new QLabel();
  // Add left margin to offset from edge of row background fill.
  er.name_label->setContentsMargins(
    er.name_label->style()->pixelMetric(QStyle::PM_LayoutLeftMargin), 0, 0, 0);

  er.stopping_value = new QLabel();
  er.stopping_value->setAlignment(Qt::AlignCenter);

  er.count_value = new QLabel();
  // Add right margin to offset from edge of row background fill.
  er.count_value->setContentsMargins(
    0, 0, er.count_value->style()->pixelMetric(QStyle::PM_LayoutRightMargin), 0);
  // Set a fixed size for performance, big enough to display the largest
  // possible count.
  er.count_value->setText(QString::number(UINT_MAX));
  er.count_value->setFixedSize(er.count_value->sizeHint());

  er.background = new QFrame();

  if (row & 1)
  {
    // NOTE: The background color doesn't work with the fusion style on macOS,
    // it seems to be the same gray as the normal background.
    er.background->setStyleSheet("background-color: palette(alternate-base);");
  }

  // Increase the width of the Yes/No label to make it have a good width when
  // highlighted red. Increase the minimum height of the row in the layout to
  // make up for the vertical spacing being removed.
  {
    er.stopping_value->setText("Yes");
    er.stopping_value->setFixedWidth(er.stopping_value->sizeHint().width() +
      2 * er.stopping_value->fontMetrics().height());
    layout->setRowMinimumHeight(row, er.stopping_value->sizeHint().height() +
      er.background->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
    er.stopping_value->setText("");
  }

  layout->addWidget(er.background, row, 0, 1, 3);
  layout->addWidget(er.name_label, row, 0, FIELD_LABEL_ALIGNMENT);
  layout->addWidget(er.stopping_value, row, 1, Qt::AlignCenter);
  layout->addWidget(er.count_value, row, 2, Qt::AlignLeft);
}

bool pin_config_row::window_suppress_events() const
{
  return ((main_window *)parent())->suppress_events;
}

void pin_config_row::set_window_suppress_events(bool suppress_events)
{
  ((main_window *)parent())->suppress_events = suppress_events;
}

main_controller * pin_config_row::window_controller() const
{
  return ((main_window *)parent())->controller;
}

void pin_config_row::setup(QGridLayout * layout, int row,
  const char * pullup_message, bool hide_analog)
{
  // Note that the slots must be manually connected because connectSlotsByName()
  // requires the object emitting the signal to be a child of the object that
  // owns the slot, which is not true here (the widgets are children of a layout
  // in the main window).

  name_label = new QLabel();

  func_value = new QComboBox();
  connect(func_value, SIGNAL(currentIndexChanged(int)), this,
    SLOT(on_func_value_currentIndexChanged(int)));

  QWidget * pullup_item;
  if (pullup_message)
  {
    QLabel * pullup_label = new QLabel();
    pullup_label->setText(pullup_message);
    pullup_item = pullup_label;
  }
  else
  {
    pullup_check = new QCheckBox();
    connect(pullup_check, SIGNAL(stateChanged(int)), this,
      SLOT(on_pullup_check_stateChanged(int)));
    pullup_item = pullup_check;
  }

  polarity_check = new QCheckBox();
  connect(polarity_check, SIGNAL(stateChanged(int)), this,
    SLOT(on_polarity_check_stateChanged(int)));

  if (!hide_analog)
  {
    analog_check = new QCheckBox();
    connect(analog_check, SIGNAL(stateChanged(int)), this,
      SLOT(on_analog_check_stateChanged(int)));
  }

  layout->addWidget(name_label, row, 0, FIELD_LABEL_ALIGNMENT);
  layout->addWidget(func_value, row, 1);
  layout->addItem(new QSpacerItem(func_value->fontMetrics().height(), 1), row, 2);
  layout->addWidget(pullup_item, row, 3);
  layout->addWidget(polarity_check, row, 4);
  if (analog_check != NULL)
  {
    layout->addWidget(analog_check, row, 5);
  }
}

static const std::array<const char *, 10> pin_func_names =
{
  "Default",
  "User I/O",
  "User input",
  "Potentiometer power",
  "Serial",
  "RC input",
  "Encoder input",
  "Kill switch",
  "Limit switch forward",
  "Limit switch reverse",
};

void pin_config_row::add_funcs(uint16_t funcs)
{
  set_window_suppress_events(true);

  for (size_t  i = 0; i < pin_func_names.size(); i++)
  {
    if (funcs & (1 << i))
    {
      func_value->addItem(pin_func_names[i], static_cast<int>(i));
    }
  }

  set_window_suppress_events(false);
}

void main_window::setup_window()
{
  // If the TICGUI_COMPACT environment variable is set to "Y", we enable
  // "compact" mode, which is suitable for systems where the regular layout
  // would not fit (e.g. a Linux system with a 1024x768 monitor).
  auto env = QProcessEnvironment::systemEnvironment();
  if (env.value("TICGUI_COMPACT") == "Y")
  {
    compact = true;
  }

  QString style_name = QApplication::style()->objectName();
  QString stylesheet;

  // Make buttons a little bit bigger so they're easier to click.  However, this
  // causes problems with the native Macintosh style, making the buttons
  // actually look narrower.
  if (style_name != "macintosh")
  {
    stylesheet += "QPushButton { padding: 0.3em 1em; }\n";
  }

  // By default, the fusion style makes the scroll bar look bad, having a border on the
  // top but no borders on the bottom.  This line seems to make it use a totally different
  // style which makes it look more like a normal Windows scrollbar, and thus better.
  if (style_name == "fusion")
  {
    stylesheet += "QScrollBar#manual_target_scroll_bar { border: 0; }\n";
  }

  setStyleSheet(stylesheet);

  setup_menu_bar();

  central_widget = new QWidget();
  QVBoxLayout * layout = central_widget_layout = new QVBoxLayout();

  layout->addLayout(setup_header());
  layout->addWidget(setup_tab_widget());
  layout->addLayout(setup_footer());

  central_widget->setLayout(layout);
  setCentralWidget(central_widget);

  retranslate();

  adjust_sizes();

  update_manual_target_controls();
  on_manual_target_min_value_valueChanged(manual_target_min_value->value());
  on_manual_target_max_value_valueChanged(manual_target_max_value->value());

  input_wizard = new InputWizard(this);

  directory_hint = QDir::homePath(); // user's home directory

  program_icon = QIcon(":app_icon");
  setWindowIcon(program_icon);

  update_timer = new QTimer(this);
  update_timer->setObjectName("update_timer");

  QMetaObject::connectSlotsByName(this);
}

void main_window::setup_menu_bar()
{
  menu_bar = new QMenuBar();

  file_menu = menu_bar->addMenu("");

  open_settings_action = new QAction(this);
  open_settings_action->setObjectName("open_settings_action");
  open_settings_action->setShortcut(Qt::CTRL | Qt::Key_O);
  file_menu->addAction(open_settings_action);

  save_settings_action = new QAction(this);
  save_settings_action->setObjectName("save_settings_action");
  save_settings_action->setShortcut(Qt::CTRL | Qt::Key_S);
  file_menu->addAction(save_settings_action);

  file_menu->addSeparator();

  exit_action = new QAction(this);
  exit_action->setObjectName("exit_action");
  exit_action->setShortcut(QKeySequence::Quit);
  connect(exit_action, SIGNAL(triggered()), this, SLOT(close()));
  file_menu->addAction(exit_action);

  device_menu = menu_bar->addMenu("");

  disconnect_action = new QAction(this);
  disconnect_action->setObjectName("disconnect_action");
  disconnect_action->setShortcut(Qt::CTRL | Qt::Key_D);
  device_menu->addAction(disconnect_action);

  clear_driver_error_action = new QAction(this);
  clear_driver_error_action->setObjectName("clear_driver_error_action");
  device_menu->addAction(clear_driver_error_action);

  go_home_reverse_action = new QAction(this);
  go_home_reverse_action->setObjectName("go_home_reverse_action");
  device_menu->addAction(go_home_reverse_action);

  go_home_forward_action = new QAction(this);
  go_home_forward_action->setObjectName("go_home_forward_action");
  device_menu->addAction(go_home_forward_action);

  device_menu->addSeparator();

  reload_settings_action = new QAction(this);
  reload_settings_action->setObjectName("reload_settings_action");
  device_menu->addAction(reload_settings_action);

  restore_defaults_action = new QAction(this);
  restore_defaults_action->setObjectName("restore_defaults_action");
  device_menu->addAction(restore_defaults_action);

  apply_settings_action = new QAction(this);
  apply_settings_action->setObjectName("apply_settings_action");
  apply_settings_action->setShortcut(Qt::CTRL | Qt::Key_P);
  device_menu->addAction(apply_settings_action);

  upgrade_firmware_action = new QAction(this);
  upgrade_firmware_action->setObjectName("upgrade_firmware_action");
  device_menu->addAction(upgrade_firmware_action);

  help_menu = menu_bar->addMenu("");

  documentation_action = new QAction(this);
  documentation_action->setObjectName("documentation_action");
  documentation_action->setShortcut(QKeySequence::HelpContents);
  help_menu->addAction(documentation_action);

  about_action = new QAction(this);
  about_action->setObjectName("about_action");
  about_action->setShortcut(QKeySequence::WhatsThis);
  help_menu->addAction(about_action);

  setMenuBar(menu_bar);
}

QLayout * main_window::setup_header()
{
  QHBoxLayout * layout = header_layout = new QHBoxLayout();

  device_list_label = new QLabel();
  device_list_value = new QComboBox();
  device_list_value->setObjectName("device_list_value");
  device_list_value->addItem(tr("Not connected"), QString()); // null value
  connection_status_value = new QLabel();

  // Make the device list wide enough to display the short name and serial
  // number of the Tic.
  {
    QComboBox tmp_box;
    tmp_box.addItem("TXXXXX: #1234567890123456");
    device_list_value->setMinimumWidth(tmp_box.sizeHint().width() * 105 / 100);
  }

  layout->addWidget(device_list_label);
  layout->addWidget(device_list_value);
  layout->addWidget(connection_status_value, 1, Qt::AlignLeft);

  return header_layout;
}

void main_window::add_tab(QWidget * tab, QString name, bool hidden)
{
  tab_specs.append(tab_spec(tab, name, hidden));
}

tab_spec & main_window::find_tab_spec(QWidget * tab)
{
  for (tab_spec & ts : tab_specs)
  {
    if (ts.tab == tab) { return ts; }
  }
  static tab_spec invalid_tab_spec(nullptr, "");
  return invalid_tab_spec;
}

QWidget * main_window::setup_tab_widget()
{
  tab_widget = new QTabWidget();

  if (compact)
  {
    add_tab(setup_status_page_widget(), tr("Status"), false);
    add_tab(setup_errors_widget(), tr("Errors"), false);
    add_tab(setup_manual_target_widget(), tr("Set target"), false);
    add_tab(setup_input_motor_settings_page_widget(), tr("Input"), false);
    add_tab(setup_motor_settings_widget(), tr("Motor"), false);
    add_tab(setup_homing_settings_widget(), tr("Homing"), false);
    add_tab(setup_advanced_settings_page_widget(), tr("Advanced"), false);
  }
  else
  {
    add_tab(setup_status_page_widget(), tr("Status"), false);
    add_tab(setup_input_motor_settings_page_widget(), tr("Input and motor settings"), false);
    add_tab(setup_advanced_settings_page_widget(), tr("Advanced settings"), false);
  }
  update_shown_tabs();

  // Let the user specify which tab to start on.  Handy for development.
  auto env = QProcessEnvironment::systemEnvironment();
  tab_widget->setCurrentIndex(env.value("TICGUI_TAB").toInt());

  return tab_widget;
}

//// status page

QWidget * main_window::setup_status_page_widget()
{
  status_page_widget = new QWidget();
  QGridLayout * layout = status_page_layout = new QGridLayout();

  layout->addWidget(setup_device_info_box(), 0, 0, 1, 2);
  if (!compact)
  {
    layout->addWidget(setup_errors_box(), 0, 2, 2, 1);
  }
  layout->addWidget(setup_input_status_box(), 1, 0);
  layout->addWidget(setup_operation_status_box(), 1, 1);
  if (!compact)
  {
    layout->addWidget(setup_manual_target_box(), 2, 0, 1, 4);
  }

  layout->setColumnStretch(3, 1);
  layout->setRowStretch(3, 1);

  status_page_widget->setLayout(layout);
  return status_page_widget;
}

QWidget * main_window::setup_device_info_box()
{
  device_info_box = new QGroupBox();
  QGridLayout * layout = device_info_box_layout = new QGridLayout();
  layout->setColumnStretch(3, 1);
  int row = 0;

  setup_read_only_text_field(layout, row++, &device_name_label, &device_name_value);
  device_name_value->setObjectName("device_name_value");
  device_name_value->setTextInteractionFlags(Qt::TextBrowserInteraction);

  setup_read_only_text_field(layout, row++, &serial_number_label, &serial_number_value);
  setup_read_only_text_field(layout, row++, &firmware_version_label, &firmware_version_value);
  setup_read_only_text_field(layout, row++, &device_reset_label, &device_reset_value);
  setup_read_only_text_field(layout, row++, &up_time_label, &up_time_value);


  // Make the right column wide enough to display the name of the Tic,
  // which should be the widest thing that needs to fit in that column.
  // This is important for making sure that the sizeHint of the overall main
  // window has a good width before we set the window to be a fixed size.
  {
    QLabel tmp_label;
    tmp_label.setText("Tic USB Stepper Motor Controller TXXXXX");
    layout->setColumnMinimumWidth(1, tmp_label.sizeHint().width());
  }

  device_info_box->setLayout(layout);
  return device_info_box;
}

QGroupBox * main_window::setup_input_status_box()
{
  input_status_box = new QGroupBox();
  QGridLayout * layout = input_status_box_layout = new QGridLayout();
  layout->setColumnStretch(2, 1);
  int row = 0;

  setup_read_only_text_field(layout, row++, 0, 2, &encoder_position_label, &encoder_position_value);
  setup_read_only_text_field(layout, row++, 0, 2, &input_state_label, &input_state_value);
  setup_read_only_text_field(layout, row++, 0, 2, &input_after_averaging_label, &input_after_averaging_value);
  setup_read_only_text_field(layout, row++, 0, 2, &input_after_hysteresis_label, &input_after_hysteresis_value);
  {
    setup_read_only_text_field(layout, row, &input_before_scaling_label, &input_before_scaling_value);
    input_before_scaling_pretty = new QLabel();
    layout->addWidget(input_before_scaling_pretty, row, 2, Qt::AlignLeft);
    row++;
  }
  setup_read_only_text_field(layout, row++, 0, 2, &input_after_scaling_label, &input_after_scaling_value);
  setup_read_only_text_field(layout, row++, 0, 2, &limit_active_label, &limit_active_value);

  // Set fixed sizes for performance.
  {
    input_after_scaling_value->setText(QString::number(-INT_MAX));

    encoder_position_value->setFixedSize(input_after_scaling_value->sizeHint());
    input_state_value->setFixedSize(input_after_scaling_value->sizeHint());
    input_after_averaging_value->setFixedSize(input_after_scaling_value->sizeHint());
    input_after_hysteresis_value->setFixedSize(input_after_scaling_value->sizeHint());
    input_after_scaling_value->setFixedSize(input_after_scaling_value->sizeHint());
    limit_active_value->setFixedSize(input_after_scaling_value->sizeHint());

    input_before_scaling_value->setText(QString::number(4500 * 12));
    input_before_scaling_value->setFixedSize(input_before_scaling_value->sizeHint());

    input_before_scaling_pretty->setText("(" + QString::fromStdString(
      convert_input_to_us_string(4500 * 12)) + ")");
    input_before_scaling_pretty->setFixedSize(input_before_scaling_pretty->sizeHint());
  }

  layout->setRowStretch(row, 1);

  input_status_box->setLayout(layout);
  return input_status_box;
}


QGroupBox * main_window::setup_operation_status_box()
{
  operation_status_box = new QGroupBox();
  QGridLayout * layout = operation_status_box_layout = new QGridLayout();
  layout->setColumnStretch(3, 1);
  int row = 0;

  setup_read_only_text_field(layout, row++, 0, 3, &vin_voltage_label, &vin_voltage_value);
  setup_read_only_text_field(layout, row++, 0, 3, &operation_state_label, &operation_state_value);
  setup_read_only_text_field(layout, row++, 0, 3, &energized_label, &energized_value);
  setup_read_only_text_field(layout, row++, 0, 3, &homing_active_label, &homing_active_value);
  setup_read_only_text_field(layout, row++, 0, 3,
    &last_motor_driver_error_label, &last_motor_driver_error_value);
  layout->setAlignment(last_motor_driver_error_value, Qt::AlignBottom);
  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);

  {
    setup_read_only_text_field(layout, row, &target_label, &target_value);
    target_velocity_pretty = new QLabel();
    layout->addWidget(target_velocity_pretty, row, 2, 1, 2, Qt::AlignLeft);
    row++;
  }
  {
    setup_read_only_text_field(layout, row, &current_position_label, &current_position_value);
    setup_read_only_text_field(layout, row, 2, &position_uncertain_label, &position_uncertain_value);
    row++;
  }
  {
    setup_read_only_text_field(layout, row, &current_velocity_label, &current_velocity_value);
    current_velocity_pretty = new QLabel();
    layout->addWidget(current_velocity_pretty, row, 2, 1, 2, Qt::AlignLeft);
    row++;
  }

  // Set fixed sizes for performance.
  {
    target_value->setText(QString::number(INT32_MIN));

    vin_voltage_value->setFixedSize(target_value->sizeHint());
    target_value->setFixedSize(target_value->sizeHint());
    current_position_value->setFixedSize(target_value->sizeHint());
    current_velocity_value->setFixedSize(target_value->sizeHint());

    target_velocity_pretty->setText("(" + QString::fromStdString(
      convert_speed_to_pps_string(-TIC_MAX_ALLOWED_SPEED)) + ")");

    target_velocity_pretty->setFixedSize(target_velocity_pretty->sizeHint());
    current_velocity_pretty->setFixedSize(target_velocity_pretty->sizeHint());
  }

  layout->setRowStretch(row, 1);

  operation_status_box->setLayout(layout);
  return operation_status_box;
}

QLayout * main_window::setup_manual_target_layout()
{
  QGridLayout * layout = new QGridLayout();
  int row = 0;

  {
    QVBoxLayout * layout = manual_target_mode_layout = new QVBoxLayout;

    manual_target_position_mode_radio = new QRadioButton();
    manual_target_position_mode_radio->setObjectName("manual_target_position_mode_radio");
    manual_target_position_mode_radio->setChecked(true);

    manual_target_velocity_mode_radio = new QRadioButton();
    manual_target_velocity_mode_radio->setObjectName("manual_target_velocity_mode_radio");

    layout->addWidget(manual_target_position_mode_radio);
    layout->addWidget(manual_target_velocity_mode_radio);
    layout->addStretch(1);

    QMargins margins = layout->contentsMargins();
    margins.setRight(fontMetrics().height());
    layout->setContentsMargins(margins);
  }
  layout->addLayout(manual_target_mode_layout, row, 0, 2, 1);

  {
    manual_target_scroll_bar = new BallScrollBar(Qt::Horizontal);
    manual_target_scroll_bar->setObjectName("manual_target_scroll_bar");
    manual_target_scroll_bar->setSingleStep(1);
    manual_target_scroll_bar->setFocusPolicy(Qt::ClickFocus);
    manual_target_scroll_bar->setBallVisible(true);
    layout->addWidget(manual_target_scroll_bar, row, 1, 1, 5);
    row++;
  }

  {
    manual_target_min_value = new QDoubleSpinBox();
    manual_target_min_value->setObjectName("manual_target_min_value");
    manual_target_min_value->setMaximum(0);
    manual_target_min_value->setKeyboardTracking(false);

    manual_target_max_value = new QDoubleSpinBox();
    manual_target_max_value->setObjectName("manual_target_max_value");
    manual_target_max_value->setMinimum(0);
    manual_target_max_value->setKeyboardTracking(false);

    manual_target_entry_value = new QSpinBox();
    manual_target_entry_value->setObjectName("manual_target_entry_value");
    // Don't emit valueChanged while user is typing (e.g. if the user enters 500,
    // we don't want to set speeds of 5, 50, and 500).
    manual_target_entry_value->setKeyboardTracking(false);

    set_target_button = new QPushButton();
    set_target_button->setObjectName("set_target_button");

    layout->addWidget(manual_target_min_value, row, 1, Qt::AlignLeft);
    layout->addWidget(manual_target_entry_value, row, 3);
    layout->addWidget(set_target_button, row, 4, Qt::AlignLeft);
    layout->addWidget(manual_target_max_value, row, 5, Qt::AlignRight);
    row++;
  }

  {
    manual_target_min_pretty = new QLabel();
    manual_target_max_pretty = new QLabel();
    manual_target_entry_pretty = new QLabel();

    // Set fixed size for performance.
    {
      manual_target_entry_pretty->setText(
        "(" + QString::fromStdString(convert_speed_to_pps_string(-TIC_MAX_ALLOWED_SPEED)) + ")");
      manual_target_entry_pretty->setFixedSize(
        manual_target_entry_pretty->sizeHint());
    }

    layout->addWidget(manual_target_min_pretty, row, 1, Qt::AlignLeft);
    layout->addWidget(manual_target_entry_pretty, row, 3, 1, 2, Qt::AlignLeft);
    layout->addWidget(manual_target_max_pretty, row, 5,  Qt::AlignRight);
    row++;
  }

  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);

  {
    QVBoxLayout * checks_layout = new QVBoxLayout();

    auto_set_target_check = new QCheckBox();
    auto_set_target_check->setObjectName("auto_set_target_check");
    auto_set_target_check->setChecked(true);

    auto_zero_target_check = new QCheckBox();
    auto_zero_target_check->setObjectName("auto_zero_target_check");

    checks_layout->addStretch(1);
    checks_layout->addWidget(auto_set_target_check);
    checks_layout->addWidget(auto_zero_target_check);

    int col_span = compact ? 5 : 3;
    layout->addLayout(checks_layout, row, 0, 2, col_span);
  }

  {
    current_position_entry_value = new QSpinBox();
    current_position_entry_value->setObjectName("manual_target_entry_value");
    current_position_entry_value->setRange(INT32_MIN, INT32_MAX);

    set_current_position_button = new QPushButton();
    set_current_position_button->setObjectName("set_current_position_button");

    current_position_halts_label = new QLabel();

    if (compact)
    {
      QHBoxLayout * set_position_layout = new QHBoxLayout();
      set_position_layout->addWidget(current_position_entry_value);
      set_position_layout->addWidget(set_current_position_button);
      set_position_layout->addWidget(current_position_halts_label);
      set_position_layout->addStretch(1);
      layout->addLayout(set_position_layout, row + 2, 0, 1, 5);
    }
    else
    {
      layout->addWidget(current_position_entry_value, row + 1, 3);
      layout->addWidget(set_current_position_button, row + 1, 4, Qt::AlignLeft);
      layout->addWidget(current_position_halts_label, row + 1, 5, Qt::AlignLeft);
    }
  }

  {
    decelerate_button = new QPushButton();
    decelerate_button->setObjectName("decelerate_button");

    halt_button = new QPushButton();
    halt_button->setObjectName("halt_button");

    int col_span = compact ? 3 : 1;
    int col = 5 - (col_span - 1);

    layout->addWidget(decelerate_button, row, col, 1, col_span, Qt::AlignRight);
    layout->addWidget(halt_button, row + 1, col, 1, col_span, Qt::AlignRight);
  }

  // Make spin boxes wide enough to display the largest possible values.
  {
    manual_target_entry_value->setMinimum(INT32_MIN);

    manual_target_min_value->setMinimumWidth(
      manual_target_entry_value->sizeHint().width());
    manual_target_max_value->setMinimumWidth(
      manual_target_entry_value->sizeHint().width());
    manual_target_entry_value->setMinimumWidth(
      manual_target_entry_value->sizeHint().width());
    current_position_entry_value->setMinimumWidth(
      manual_target_entry_value->sizeHint().width());
  }

  // Add shortcuts to set target if enter is pressed on scroll bar or target
  // entry spin box and set range limits if enter is pressed on range limit spin
  // boxes.
  {
    manual_target_return_key_shortcut = new QShortcut(manual_target_widget);
    manual_target_return_key_shortcut->setObjectName("manual_target_return_key_shortcut");
    manual_target_return_key_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    manual_target_return_key_shortcut->setKey(Qt::Key_Return);
    manual_target_enter_key_shortcut = new QShortcut(manual_target_widget);
    manual_target_enter_key_shortcut->setObjectName("manual_target_enter_key_shortcut");
    manual_target_enter_key_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    manual_target_enter_key_shortcut->setKey(Qt::Key_Enter);

    // Use the return key shortcut's signal handlers for the enter key shortcut,
    // too.
    connect(manual_target_enter_key_shortcut, SIGNAL(activated()), this,
      SLOT(on_manual_target_return_key_shortcut_activated()));
  }

  layout->setColumnStretch(1, 1);
  layout->setColumnStretch(5, 1);
  layout->setRowStretch(7, 1);
  return layout;
}

QGroupBox * main_window::setup_manual_target_box()
{
  QGroupBox * manual_target_widget = manual_target_box = new QGroupBox();
  manual_target_widget->setLayout(setup_manual_target_layout());
  return manual_target_widget;
}

QWidget * main_window::setup_manual_target_widget()
{
  manual_target_widget = new QWidget();
  manual_target_widget->setLayout(setup_manual_target_layout());
  return manual_target_widget;
}

QLayout * main_window::setup_errors_layout()
{
  QVBoxLayout * layout = new QVBoxLayout();

  layout->addLayout(setup_error_table_layout());

  {
    errors_reset_counts_button = new QPushButton();
    errors_reset_counts_button->setObjectName("errors_reset_counts_button");
    layout->addWidget(errors_reset_counts_button, 0, Qt::AlignRight);
  }

  layout->addStretch(1);

  reset_error_counts();

  return layout;
}

QGroupBox * main_window::setup_errors_box()
{
  errors_box = new QGroupBox();
  errors_box->setLayout(setup_errors_layout());
  return errors_box;
}

QWidget * main_window::setup_errors_widget()
{
  QWidget * widget = new QWidget();
  widget->setLayout(setup_errors_layout());
  return widget;
}

QLayout * main_window::setup_error_table_layout()
{
  QGridLayout * layout = new QGridLayout();
  layout->setHorizontalSpacing(fontMetrics().height());
  // Remove spaces between rows so row background fill looks good.
  layout->setVerticalSpacing(0);
  int row = 0;

   {
     errors_stopping_header_label = new QLabel();
     errors_count_header_label = new QLabel();
     layout->addWidget(errors_stopping_header_label, row, 1, Qt::AlignCenter);
     layout->addWidget(errors_count_header_label, row, 2, Qt::AlignLeft);
     row++;
   }

  setup_error_row(layout, row++, error_rows[TIC_ERROR_INTENTIONALLY_DEENERGIZED]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_MOTOR_DRIVER_ERROR]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_LOW_VIN]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_KILL_SWITCH]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_REQUIRED_INPUT_INVALID]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_COMMAND_TIMEOUT]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_SAFE_START_VIOLATION]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_ERR_LINE_HIGH]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_SERIAL_ERROR]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_SERIAL_FRAMING]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_SERIAL_RX_OVERRUN]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_SERIAL_FORMAT]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_SERIAL_CRC]);
  setup_error_row(layout, row++, error_rows[TIC_ERROR_ENCODER_SKIP]);

  // Adjust height of header row to match error rows.
  layout->setRowMinimumHeight(0, layout->rowMinimumHeight(1));

  layout->setColumnStretch(2, 1);

  return layout;
}

// [all-settings]

//// input and motor settings page

QWidget * main_window::setup_input_motor_settings_page_widget()
{
  input_motor_settings_page_widget = new QWidget();
  QGridLayout * layout = input_motor_settings_page_layout = new QGridLayout();

  layout->addWidget(setup_control_mode_widget(), 0, 0);
  layout->addWidget(setup_serial_settings_box(), 1, 0, 1, 2);
  layout->addWidget(setup_encoder_settings_box(), 2, 0);
  layout->addWidget(setup_conditioning_settings_box(), 3, 0);
  layout->addWidget(setup_scaling_settings_box(), 2, 1, 2, 1);
  if (!compact)
  {
    layout->addWidget(setup_motor_settings_box(), 1, 2, 3, 1);
  }

  layout->setColumnStretch(3, 1);
  layout->setRowStretch(4, 1);

  input_motor_settings_page_widget->setLayout(layout);
  return input_motor_settings_page_widget;
}

QWidget * main_window::setup_control_mode_widget()
{
  control_mode_widget = new QWidget();
  QGridLayout * layout = control_mode_widget_layout = new QGridLayout();
  layout->setColumnStretch(1, 1);
  layout->setContentsMargins(0, 0, 0, 0);
  int row = 0;

  {
    control_mode_value = new QComboBox();
    control_mode_value->setObjectName("control_mode_value");
    control_mode_value->addItem(u8"Serial\u2009/\u2009I\u00B2C\u2009/\u2009USB", TIC_CONTROL_MODE_SERIAL);
    control_mode_value->addItem("RC position", TIC_CONTROL_MODE_RC_POSITION);
    control_mode_value->addItem("RC speed", TIC_CONTROL_MODE_RC_SPEED);
    control_mode_value->addItem("Analog position", TIC_CONTROL_MODE_ANALOG_POSITION);
    control_mode_value->addItem("Analog speed", TIC_CONTROL_MODE_ANALOG_SPEED);
    control_mode_value->addItem("Encoder position", TIC_CONTROL_MODE_ENCODER_POSITION);
    control_mode_value->addItem("Encoder speed", TIC_CONTROL_MODE_ENCODER_SPEED);
    control_mode_value->addItem("STEP/DIR", TIC_CONTROL_MODE_STEP_DIR);
    control_mode_label = new QLabel();
    control_mode_label->setBuddy(control_mode_value);
    layout->addWidget(control_mode_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(control_mode_value, row, 1, Qt::AlignLeft);
    row++;
  }

  control_mode_widget->setLayout(layout);
  return control_mode_widget;
}

QGroupBox * main_window::setup_serial_settings_box()
{
  serial_settings_box = new QGroupBox();
  QGridLayout * layout = serial_settings_box_layout = new QGridLayout();

  {
    serial_baud_rate_value = new QSpinBox();
    serial_baud_rate_value->setObjectName("serial_baud_rate_value");
    serial_baud_rate_value->setRange(
      TIC_MIN_ALLOWED_BAUD_RATE, TIC_MAX_ALLOWED_BAUD_RATE);
    serial_baud_rate_label = new QLabel();
    serial_baud_rate_label->setBuddy(serial_baud_rate_value);
    layout->addWidget(serial_baud_rate_label, 0, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(serial_baud_rate_value, 0, 1, Qt::AlignLeft);
  }

  {
    serial_device_number_value = new QSpinBox();
    serial_device_number_value->setObjectName("serial_device_number_value");
    serial_device_number_value->setRange(0, 0x3FFF);
    serial_device_number_label = new QLabel();
    serial_device_number_label->setBuddy(serial_device_number_value);
    layout->addWidget(serial_device_number_label, 1, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(serial_device_number_value, 1, 1, Qt::AlignLeft);
  }

  {
    serial_enable_alt_device_number_check = new QCheckBox();
    serial_enable_alt_device_number_check->setObjectName(
      "serial_enable_alt_device_number_check");
    serial_alt_device_number_value = new QSpinBox();
    serial_alt_device_number_value->setObjectName("serial_alt_device_number_value");
    serial_alt_device_number_value->setRange(0, 0x3FFF);
    serial_alt_device_number_value->setEnabled(false);
    layout->addWidget(serial_enable_alt_device_number_check, 2, 0, Qt::AlignLeft);
    layout->addWidget(serial_alt_device_number_value, 2, 1, Qt::AlignLeft);
  }

  {
    serial_14bit_device_number_check = new QCheckBox();
    serial_14bit_device_number_check->setObjectName(
      "serial_14bit_device_number_check");
    layout->addWidget(serial_14bit_device_number_check, 3, 0, 1, 2, Qt::AlignLeft);
  }

  {
    serial_response_delay_value = new QSpinBox();
    serial_response_delay_value->setObjectName("serial_response_delay_value");
    serial_response_delay_value->setSuffix(" \u00b5s");
    serial_response_delay_value->setRange(0, UINT8_MAX);
    serial_response_delay_label = new QLabel();
    serial_response_delay_label->setBuddy(serial_response_delay_value);
    layout->addWidget(serial_response_delay_label, 4, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(serial_response_delay_value, 4, 1, Qt::AlignLeft);
  }

  layout->addItem(new QSpacerItem(fontMetrics().height(), 1), 0, 2);

  {
    command_timeout_check = new QCheckBox();
    command_timeout_check->setObjectName("command_timeout_check");
    command_timeout_value = new QDoubleSpinBox();
    command_timeout_value->setObjectName("command_timeout_value");
    command_timeout_value->setRange(0.001, TIC_MAX_ALLOWED_COMMAND_TIMEOUT / 1000);
    command_timeout_value->setDecimals(3);
    command_timeout_value->setSuffix(" s");
    layout->addWidget(command_timeout_check, 0, 3, Qt::AlignLeft);
    layout->addWidget(command_timeout_value, 0, 4, Qt::AlignLeft);
  }

  {
    serial_crc_for_commands_check = new QCheckBox();
    serial_crc_for_commands_check->setObjectName("serial_crc_for_commands_check");
    layout->addWidget(serial_crc_for_commands_check, 1, 3, 1, 2, Qt::AlignLeft);
  }

  {
    serial_crc_for_responses_check = new QCheckBox();
    serial_crc_for_responses_check->setObjectName(
      "serial_crc_for_responses_check");
    layout->addWidget(serial_crc_for_responses_check, 2, 3, 1, 2,
      Qt::AlignLeft);
  }

  {
    serial_7bit_responses_check = new QCheckBox();
    serial_7bit_responses_check->setObjectName("serial_7bit_responses_check");
    layout->addWidget(serial_7bit_responses_check, 3, 3, 1, 2, Qt::AlignLeft);
  }

  layout->setColumnStretch(5, 1);
  layout->setRowStretch(1, 1);

  serial_settings_box->setLayout(layout);
  return serial_settings_box;
}

QGroupBox * main_window::setup_encoder_settings_box()
{
  encoder_settings_box = new QGroupBox();
  QGridLayout * layout = encoder_settings_box_layout = new QGridLayout();
  int row = 0;

  {
    encoder_prescaler_value = new QSpinBox();
    encoder_prescaler_value->setObjectName("encoder_prescaler_value");
    encoder_prescaler_value->setRange(1, INT32_MAX);
    encoder_prescaler_label = new QLabel();
    encoder_prescaler_label->setBuddy(encoder_prescaler_value);
    layout->addWidget(encoder_prescaler_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(encoder_prescaler_value, row, 1, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    encoder_postscaler_value = new QSpinBox();
    encoder_postscaler_value->setObjectName("encoder_postscaler_value");
    encoder_postscaler_value->setRange(1, INT32_MAX);
    encoder_postscaler_label = new QLabel();
    encoder_postscaler_label->setBuddy(encoder_postscaler_value);
    layout->addWidget(encoder_postscaler_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(encoder_postscaler_value, row, 1, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    encoder_unlimited_check = new QCheckBox();
    encoder_unlimited_check->setObjectName("encoder_unlimited_check");
    layout->addWidget(encoder_unlimited_check, row, 0, 1, 3, Qt::AlignLeft);
    row++;
  }

  layout->setColumnStretch(1, 1);
  layout->setRowStretch(row, 1);

  encoder_settings_box->setLayout(layout);
  return encoder_settings_box;
}

QGroupBox * main_window::setup_conditioning_settings_box()
{
  conditioning_settings_box = new QGroupBox();
  QGridLayout * layout = conditioning_settings_box_layout = new QGridLayout();
  int row = 0;

  {
    input_averaging_enabled_check = new QCheckBox();
    input_averaging_enabled_check->setObjectName("input_averaging_enabled_check");
    layout->addWidget(input_averaging_enabled_check, row, 0, 1, 3, Qt::AlignLeft);
    row++;
  }

  {
    input_hysteresis_value = new QSpinBox();
    input_hysteresis_value->setObjectName("input_hysteresis_value");
    input_hysteresis_value->setRange(0, UINT16_MAX);
    input_hysteresis_label = new QLabel();
    input_hysteresis_label->setBuddy(input_hysteresis_value);
    layout->addWidget(input_hysteresis_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(input_hysteresis_value, row, 1, 1, 2, Qt::AlignLeft);
    row++;
  }

  layout->setColumnStretch(1, 1);
  layout->setRowStretch(row, 1);

  conditioning_settings_box->setLayout(layout);
  return conditioning_settings_box;
}

QGroupBox * main_window::setup_scaling_settings_box()
{
  scaling_settings_box = new QGroupBox();
  QGridLayout * layout = scaling_settings_box_layout = new QGridLayout();
  int row = 0;

  {
    input_invert_check = new QCheckBox();
    input_invert_check->setObjectName("input_invert_check");
    input_learn_button = new QPushButton();
    input_learn_button->setObjectName("input_learn_button");
    layout->addWidget(input_invert_check, row, 0, 1, 2, Qt::AlignLeft);
    layout->addWidget(input_learn_button, row, 2, Qt::AlignRight);
    row++;
  }

  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);

  {
    scaling_input_label = new QLabel();
    scaling_target_label = new QLabel();
    layout->addWidget(scaling_input_label, row, 1, Qt::AlignLeft);
    layout->addWidget(scaling_target_label, row, 2, Qt::AlignLeft);
    row++;
  }

  {
    scaling_max_label = new QLabel();
    input_max_value = new QSpinBox();
    input_max_value->setObjectName("input_max_value");
    input_max_value->setRange(0, UINT12_MAX);
    output_max_value = new QSpinBox();
    output_max_value->setObjectName("output_max_value");
    output_max_value->setRange(0, INT32_MAX);
    layout->addWidget(scaling_max_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(input_max_value, row, 1, Qt::AlignLeft);
    layout->addWidget(output_max_value, row, 2, Qt::AlignLeft);
    row++;
  }

  {
    scaling_neutral_max_label = new QLabel();
    input_neutral_max_value = new QSpinBox();
    input_neutral_max_value->setObjectName("input_neutral_max_value");
    input_neutral_max_value->setRange(0, UINT12_MAX);
    layout->addWidget(scaling_neutral_max_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(input_neutral_max_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    scaling_neutral_min_label = new QLabel();
    input_neutral_min_value = new QSpinBox();
    input_neutral_min_value->setObjectName("input_neutral_min_value");
    input_neutral_min_value->setRange(0, UINT12_MAX);
    layout->addWidget(scaling_neutral_min_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(input_neutral_min_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    scaling_min_label = new QLabel();
    input_min_value = new QSpinBox();
    input_min_value->setObjectName("input_min_value");
    input_min_value->setRange(0, UINT12_MAX);
    output_min_value = new QSpinBox();
    output_min_value->setObjectName("output_min_value");
    output_min_value->setRange(-INT32_MAX, 0);
    layout->addWidget(scaling_min_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(input_min_value, row, 1, Qt::AlignLeft);
    layout->addWidget(output_min_value, row, 2, Qt::AlignLeft);
    row++;
  }

  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);

  {
    input_scaling_degree_value = new QComboBox();
    input_scaling_degree_value->setObjectName("input_scaling_degree_value");
    input_scaling_degree_value->addItem("1 - Linear", TIC_SCALING_DEGREE_LINEAR);
    input_scaling_degree_value->addItem("2 - Quadratic", TIC_SCALING_DEGREE_QUADRATIC);
    input_scaling_degree_value->addItem("3 - Cubic", TIC_SCALING_DEGREE_CUBIC);
    input_scaling_degree_label = new QLabel();
    input_scaling_degree_label->setBuddy(input_scaling_degree_value);
    layout->addWidget(input_scaling_degree_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(input_scaling_degree_value, row, 1, 1, 2, Qt::AlignLeft);
    row++;
  }

  layout->setColumnStretch(2, 1);
  layout->setRowStretch(row, 1);

  // Make both of these the same width. (output_min_value width changes slightly
  // because the size hint is a little off at this point)
  output_min_value->setMinimumWidth(output_min_value->sizeHint().width());
  output_max_value->setMinimumWidth(output_min_value->sizeHint().width());

  scaling_settings_box->setLayout(layout);
  return scaling_settings_box;
}

QLayout * main_window::setup_motor_settings_layout()
{
  QGridLayout * layout = new QGridLayout();
  int row = 0;

  {
    invert_motor_direction_check = new QCheckBox();
    invert_motor_direction_check->setObjectName("invert_motor_direction_check");
    layout->addWidget(invert_motor_direction_check, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);

  {
    speed_max_value = new QSpinBox();
    speed_max_value->setObjectName("speed_max_value");
    speed_max_value->setRange(0, TIC_MAX_ALLOWED_SPEED);
    speed_max_label = new QLabel();
    speed_max_label->setBuddy(speed_max_value);
    speed_max_value_pretty = new QLabel();
    layout->addWidget(speed_max_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(speed_max_value, row, 1, Qt::AlignLeft);
    layout->addWidget(speed_max_value_pretty, row, 2, Qt::AlignLeft);
    row++;
  }

  {
    starting_speed_value = new QSpinBox();
    starting_speed_value->setObjectName("starting_speed_value");
    starting_speed_value->setRange(0, TIC_MAX_ALLOWED_SPEED);
    starting_speed_label = new QLabel();
    starting_speed_label->setBuddy(starting_speed_value);
    starting_speed_value_pretty = new QLabel();
    layout->addWidget(starting_speed_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(starting_speed_value, row, 1, Qt::AlignLeft);
    layout->addWidget(starting_speed_value_pretty, row, 2, Qt::AlignLeft);
    row++;
  }

  {
    accel_max_value = new QSpinBox();
    accel_max_value->setObjectName("accel_max_value");
    accel_max_value->setRange(TIC_MIN_ALLOWED_ACCEL, TIC_MAX_ALLOWED_ACCEL);
    accel_max_label = new QLabel();
    accel_max_label->setBuddy(accel_max_value);
    accel_max_value_pretty = new QLabel();
    layout->addWidget(accel_max_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(accel_max_value, row, 1, Qt::AlignLeft);
    layout->addWidget(accel_max_value_pretty, row, 2, Qt::AlignLeft);

    // Make the right column wide enough to display the largest possible pretty
    // values.
    {
      accel_max_value_pretty->setText(QString::fromStdString(
        convert_accel_to_pps2_string(TIC_MAX_ALLOWED_ACCEL)));
      layout->setColumnMinimumWidth(2, accel_max_value_pretty->sizeHint().width());
    }

    row++;
  }

  {
    decel_max_value = new QSpinBox();
    decel_max_value->setObjectName("decel_max_value");
    decel_max_value->setRange(TIC_MIN_ALLOWED_ACCEL, TIC_MAX_ALLOWED_ACCEL);
    decel_max_label = new QLabel();
    decel_max_label->setBuddy(decel_max_value);
    decel_max_value_pretty = new QLabel();
    layout->addWidget(decel_max_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(decel_max_value, row, 1, Qt::AlignLeft);
    layout->addWidget(decel_max_value_pretty, row, 2, Qt::AlignLeft);
    row++;
  }

  {
    decel_accel_max_same_check = new QCheckBox();
    decel_accel_max_same_check->setObjectName("decel_accel_max_same_check");
    layout->addWidget(decel_accel_max_same_check, row, 0, 1, 3, Qt::AlignLeft);
    row++;
  }

  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);

  {
    step_mode_value = new QComboBox();
    step_mode_value->setObjectName("step_mode_value");
    step_mode_value->addItem("1/2 step 100%", 0);  // reserve space
    step_mode_label = new QLabel();
    step_mode_label->setBuddy(step_mode_value);
    layout->addWidget(step_mode_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(step_mode_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    QGridLayout * current_limit_layout = new QGridLayout();

    current_limit_value = new current_spin_box();
    current_limit_value->setObjectName("current_limit_value");
    current_limit_value->setRange(0, 9999);
    current_limit_value->setSuffix(" mA");
    current_limit_label = new QLabel();
    current_limit_label->setBuddy(current_limit_value);

    current_limit_warning_label = new QLabel();
    current_limit_warning_label->setObjectName("current_limit_warning_label");
    current_limit_warning_label->setStyleSheet("color: red;");
    // TODO: Hide this label and the other one like it in the advanced
    // settings tab so the user cannot see them before connecting to a Tic.

    current_limit_layout->addWidget(current_limit_value, 0, 0);
    current_limit_layout->addWidget(current_limit_warning_label, 0, 1,
      Qt::AlignLeft);

    layout->addWidget(current_limit_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addLayout(current_limit_layout, row, 1, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    decay_mode_value = new QComboBox();
    decay_mode_value->setObjectName("decay_mode_value");
    decay_mode_value->addItem("Slow / auto-mixed", 0);  // reserve space
    decay_mode_label = new QLabel();
    decay_mode_label->setBuddy(decay_mode_value);
    layout->addWidget(decay_mode_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(decay_mode_value, row, 1, Qt::AlignLeft);
    row++;
  }

  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);

  {
    agc_mode_value = new QComboBox();
    agc_mode_value->setObjectName("agc_mode_value");
    agc_mode_value->addItem("Off", TIC_AGC_MODE_OFF);
    agc_mode_value->addItem("On", TIC_AGC_MODE_ON);
    agc_mode_value->addItem("Active off", TIC_AGC_MODE_ACTIVE_OFF);
    agc_mode_label = new QLabel();
    agc_mode_label->setBuddy(agc_mode_value);
    layout->addWidget(agc_mode_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(agc_mode_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    agc_bottom_current_limit_value = new QComboBox();
    agc_bottom_current_limit_value->setObjectName("agc_bottom_current_limit_value");
    agc_bottom_current_limit_value->addItem("45%", TIC_AGC_BOTTOM_CURRENT_LIMIT_45);
    agc_bottom_current_limit_value->addItem("50%", TIC_AGC_BOTTOM_CURRENT_LIMIT_50);
    agc_bottom_current_limit_value->addItem("55%", TIC_AGC_BOTTOM_CURRENT_LIMIT_55);
    agc_bottom_current_limit_value->addItem("60%", TIC_AGC_BOTTOM_CURRENT_LIMIT_60);
    agc_bottom_current_limit_value->addItem("65%", TIC_AGC_BOTTOM_CURRENT_LIMIT_65);
    agc_bottom_current_limit_value->addItem("70%", TIC_AGC_BOTTOM_CURRENT_LIMIT_70);
    agc_bottom_current_limit_value->addItem("75%", TIC_AGC_BOTTOM_CURRENT_LIMIT_75);
    agc_bottom_current_limit_value->addItem("80%", TIC_AGC_BOTTOM_CURRENT_LIMIT_80);
    agc_bottom_current_limit_label = new QLabel();
    agc_bottom_current_limit_label->setBuddy(agc_bottom_current_limit_value);
    layout->addWidget(agc_bottom_current_limit_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(agc_bottom_current_limit_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    agc_current_boost_steps_value = new QComboBox();
    agc_current_boost_steps_value->setObjectName("agc_current_boost_steps_value");
    agc_current_boost_steps_value->addItem("5", TIC_AGC_CURRENT_BOOST_STEPS_5);
    agc_current_boost_steps_value->addItem("7", TIC_AGC_CURRENT_BOOST_STEPS_7);
    agc_current_boost_steps_value->addItem("9", TIC_AGC_CURRENT_BOOST_STEPS_9);
    agc_current_boost_steps_value->addItem("11", TIC_AGC_CURRENT_BOOST_STEPS_11);
    agc_current_boost_steps_label = new QLabel();
    agc_current_boost_steps_label->setBuddy(agc_current_boost_steps_value);
    layout->addWidget(agc_current_boost_steps_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(agc_current_boost_steps_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    agc_frequency_limit_value = new QComboBox();
    agc_frequency_limit_value->setObjectName("agc_frequency_limit_value");
    agc_frequency_limit_value->addItem("Off", TIC_AGC_FREQUENCY_LIMIT_OFF);
    agc_frequency_limit_value->addItem("225 Hz", TIC_AGC_FREQUENCY_LIMIT_225);
    agc_frequency_limit_value->addItem("450 Hz", TIC_AGC_FREQUENCY_LIMIT_450);
    agc_frequency_limit_value->addItem("675 Hz", TIC_AGC_FREQUENCY_LIMIT_675);
    agc_frequency_limit_label = new QLabel();
    agc_frequency_limit_label->setBuddy(agc_frequency_limit_value);
    layout->addWidget(agc_frequency_limit_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(agc_frequency_limit_value, row, 1, Qt::AlignLeft);
    row++;
  }

  layout->addWidget(setup_hp_motor_widget(), row++, 0, 1, 3);

  layout->setColumnStretch(2, 1);
  layout->setRowStretch(row, 1);

  return layout;
}

QWidget * main_window::setup_hp_motor_widget()
{
  hp_motor_widget = new QWidget();
  QGridLayout * layout = new QGridLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  size_t minimum_time_box_width;
  {
    QSpinBox tmp_box;
    tmp_box.setSpecialValueText("99.99 \u00b5s");
    minimum_time_box_width = tmp_box.sizeHint().width();
  }

  int row = 0;

  {
    hp_toff_value = new time_spin_box();
    hp_toff_value->setObjectName("hp_toff_value");
    hp_toff_value->setMinimumWidth(minimum_time_box_width);
    hp_toff_value->set_decimals(1);
    QMap <int, int> mapping;
    for (int i = 0; i < 0x100; i++) { mapping.insert(i, (i + 1) * 500); }
    hp_toff_value->set_mapping(mapping);
    hp_toff_value->setSuffix(" \u00b5s");
    hp_toff_label = new QLabel();
    hp_toff_label->setBuddy(hp_toff_value);
    layout->addWidget(hp_toff_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(hp_toff_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    hp_tblank_value = new time_spin_box();
    hp_tblank_value->setObjectName("hp_tblank_value");
    hp_tblank_value->setMinimumWidth(minimum_time_box_width);
    hp_tblank_value->set_decimals(2);
    QMap<int, int> mapping;
    for (int i = 0x32; i < 0x100; i++) { mapping.insert(i, i * 20); }
    hp_tblank_value->set_mapping(mapping);
    hp_tblank_value->setSuffix(" \u00b5s");
    hp_tblank_label = new QLabel();
    hp_tblank_label->setBuddy(hp_tblank_value);
    layout->addWidget(hp_tblank_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(hp_tblank_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    hp_abt_check = new QCheckBox();
    hp_abt_check->setObjectName("hp_abt_check");
    layout->addWidget(hp_abt_check, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    hp_tdecay_value = new time_spin_box();
    hp_tdecay_value->setObjectName("hp_tdecay_value");
    hp_tdecay_value->setMinimumWidth(minimum_time_box_width);
    hp_tdecay_value->set_decimals(1);
    QMap<int, int> mapping;
    for (int i = 0; i < 0x100; i++) { mapping.insert(i, i * 500); }
    hp_tdecay_value->set_mapping(mapping);
    hp_tdecay_value->setSuffix(" \u00b5s");
    hp_tdecay_label = new QLabel();
    hp_tdecay_label->setBuddy(hp_tdecay_value);
    layout->addWidget(hp_tdecay_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(hp_tdecay_value, row, 1, Qt::AlignLeft);
    row++;
  }

  layout->setColumnStretch(2, 1);
  layout->setRowStretch(row, 1);

  hp_motor_widget->setLayout(layout);
  return hp_motor_widget;
}

QGroupBox * main_window::setup_motor_settings_box()
{
  motor_settings_box = new QGroupBox();
  motor_settings_box->setLayout(setup_motor_settings_layout());
  return motor_settings_box;
}

QWidget * main_window::setup_motor_settings_widget()
{
  QWidget * widget = new QWidget();
  widget->setLayout(setup_motor_settings_layout());
  return widget;
}

//// advanced settings page

QWidget * main_window::setup_advanced_settings_page_widget()
{
  advanced_settings_page_widget = new QWidget();
  QGridLayout * layout = advanced_settings_page_layout = new QGridLayout();

  layout->addWidget(setup_pin_config_box(), 0, 0, 1, 2);
  layout->addWidget(setup_error_settings_box(), 1, 0);
  layout->addWidget(setup_misc_settings_box(), 1, 1);
  if (!compact)
  {
    layout->addWidget(setup_homing_settings_box(), 2, 0, 1, 2);
  }

  layout->setColumnStretch(2, 1);
  layout->setRowStretch(3, 1);

  advanced_settings_page_widget->setLayout(layout);
  return advanced_settings_page_widget;
}

QGroupBox * main_window::setup_pin_config_box()
{
  pin_config_box = new QGroupBox();
  QGridLayout * layout = pin_config_box_layout = new QGridLayout();
  int row = 0;

  const uint32_t universal_funcs =
    (1 << TIC_PIN_FUNC_DEFAULT) |
    (1 << TIC_PIN_FUNC_USER_INPUT) |
    (1 << TIC_PIN_FUNC_KILL_SWITCH) |
    (1 << TIC_PIN_FUNC_LIMIT_SWITCH_FORWARD) |
    (1 << TIC_PIN_FUNC_LIMIT_SWITCH_REVERSE);

  pin_config_rows[TIC_PIN_NUM_SCL] = new pin_config_row(TIC_PIN_NUM_SCL, this);
  pin_config_rows[TIC_PIN_NUM_SCL]->setup(layout, row++);
  pin_config_rows[TIC_PIN_NUM_SCL]->add_funcs(universal_funcs |
    (1 << TIC_PIN_FUNC_USER_IO) |
    (1 << TIC_PIN_FUNC_POT_POWER) |
    (1 << TIC_PIN_FUNC_SERIAL));

  pin_config_rows[TIC_PIN_NUM_SDA] = new pin_config_row(TIC_PIN_NUM_SDA, this);
  pin_config_rows[TIC_PIN_NUM_SDA]->setup(layout, row++);
  pin_config_rows[TIC_PIN_NUM_SDA]->add_funcs(universal_funcs |
    (1 << TIC_PIN_FUNC_USER_IO) |
    (1 << TIC_PIN_FUNC_SERIAL));

  pin_config_rows[TIC_PIN_NUM_TX] = new pin_config_row(TIC_PIN_NUM_TX, this);
  pin_config_rows[TIC_PIN_NUM_TX]->setup(layout, row++, "(always pulled up)");
  pin_config_rows[TIC_PIN_NUM_TX]->add_funcs(universal_funcs |
    (1 << TIC_PIN_FUNC_USER_IO) |
    (1 << TIC_PIN_FUNC_SERIAL) |
    (1 << TIC_PIN_FUNC_ENCODER));

  pin_config_rows[TIC_PIN_NUM_RX] = new pin_config_row(TIC_PIN_NUM_RX, this);
  pin_config_rows[TIC_PIN_NUM_RX]->setup(layout, row++, "(always pulled up)");
  pin_config_rows[TIC_PIN_NUM_RX]->add_funcs(universal_funcs |
    (1 << TIC_PIN_FUNC_USER_IO) |
    (1 << TIC_PIN_FUNC_SERIAL) |
    (1 << TIC_PIN_FUNC_ENCODER));

  pin_config_rows[TIC_PIN_NUM_RC] = new pin_config_row(TIC_PIN_NUM_RC, this);
  pin_config_rows[TIC_PIN_NUM_RC]->setup(layout, row++, "(always pulled down)", true);
  pin_config_rows[TIC_PIN_NUM_RC]->add_funcs(universal_funcs |
    (1 << TIC_PIN_FUNC_RC));

  layout->setColumnStretch(5, 1);
  layout->setRowStretch(row, 1);

  pin_config_box->setLayout(layout);
  return pin_config_box;
}

QGroupBox * main_window::setup_error_settings_box()
{
  error_settings_box = new QGroupBox();
  QGridLayout * layout = error_settings_box_layout = new QGridLayout();
  int row = 0;

  soft_error_response_radio_group = new QButtonGroup(this);
  soft_error_response_radio_group->setObjectName("soft_error_response_radio_group");

  {
    soft_error_response_radio_group->addButton(new QRadioButton(), TIC_RESPONSE_DEENERGIZE);
    layout->addWidget(soft_error_response_radio_group->button(TIC_RESPONSE_DEENERGIZE),
      row++, 0, 1, 2, Qt::AlignLeft);
  }

  {
    soft_error_response_radio_group->addButton(new QRadioButton(), TIC_RESPONSE_HALT_AND_HOLD);
    layout->addWidget(soft_error_response_radio_group->button(TIC_RESPONSE_HALT_AND_HOLD),
      row++, 0, 1, 2, Qt::AlignLeft);
  }

  {
    soft_error_response_radio_group->addButton(new QRadioButton(), TIC_RESPONSE_DECEL_TO_HOLD);
    layout->addWidget(soft_error_response_radio_group->button(TIC_RESPONSE_DECEL_TO_HOLD),
      row++, 0, 1, 2, Qt::AlignLeft);
  }

  {
    soft_error_response_radio_group->addButton(new QRadioButton(), TIC_RESPONSE_GO_TO_POSITION);
    soft_error_position_value = new QSpinBox();
    soft_error_position_value->setObjectName("soft_error_position_value");
    soft_error_position_value->setRange(INT32_MIN, INT32_MAX);
    layout->addWidget(soft_error_response_radio_group->button(TIC_RESPONSE_GO_TO_POSITION),
      row, 0, Qt::AlignLeft);
    layout->addWidget(soft_error_position_value, row, 1, Qt::AlignLeft);
    row++;
  }

  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);


  {
    current_limit_during_error_check = new QCheckBox();
    current_limit_during_error_check->setObjectName("current_limit_during_error_check");
    layout->addWidget(current_limit_during_error_check, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    QGridLayout * clde_layout = new QGridLayout();

    current_limit_during_error_value = new current_spin_box();
    current_limit_during_error_value->setObjectName("current_limit_during_error_value");
    current_limit_during_error_value->setRange(0, 9999);
    current_limit_during_error_value->setSuffix(" mA");

    current_limit_during_error_warning_label = new QLabel();
    current_limit_during_error_warning_label->setObjectName(
      "current_limit_during_error_warning_label");
    current_limit_during_error_warning_label->setStyleSheet("color: red;");

    QCheckBox * dummy_checkbox = new QCheckBox();
    QSizePolicy sp = dummy_checkbox->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    dummy_checkbox->setSizePolicy(sp);
    dummy_checkbox->setVisible(false);

    clde_layout->addWidget(dummy_checkbox, 0, 0);
    clde_layout->addWidget(current_limit_during_error_value, 0, 1);
    clde_layout->addWidget(current_limit_during_error_warning_label, 0, 2);
    clde_layout->setColumnStretch(3, 1);
    clde_layout->setRowStretch(1, 1);

    layout->addLayout(clde_layout, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  layout->setColumnStretch(1, 1);
  layout->setRowStretch(row, 1);

  error_settings_box->setLayout(layout);
  return error_settings_box;
}

QGroupBox * main_window::setup_misc_settings_box()
{
  misc_settings_box = new QGroupBox();
  QGridLayout * layout = new QGridLayout();
  int row = 0;

  {
    disable_safe_start_check = new QCheckBox();
    disable_safe_start_check->setObjectName("disable_safe_start_check");
    layout->addWidget(disable_safe_start_check, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    ignore_err_line_high_check = new QCheckBox();
    ignore_err_line_high_check->setObjectName("ignore_err_line_high_check");
    layout->addWidget(ignore_err_line_high_check, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    auto_clear_driver_error_check = new QCheckBox();
    auto_clear_driver_error_check->setObjectName("auto_clear_driver_error_check");
    layout->addWidget(auto_clear_driver_error_check, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    never_sleep_check = new QCheckBox();
    never_sleep_check->setObjectName("never_sleep_check");
    layout->addWidget(never_sleep_check, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    hp_enable_unrestricted_current_limits_check = new QCheckBox();
    hp_enable_unrestricted_current_limits_check->setObjectName(
      "hp_enable_unrestricted_current_limits_check");
    layout->addWidget(hp_enable_unrestricted_current_limits_check,
      row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  layout->addItem(new QSpacerItem(1, fontMetrics().height()), row++, 0);

  {
    vin_calibration_value = new QSpinBox();
    vin_calibration_value->setObjectName("vin_calibration_value");
    vin_calibration_value->setRange(-500, 500);
    vin_calibration_label = new QLabel();
    vin_calibration_label->setBuddy(vin_calibration_value);
    layout->addWidget(vin_calibration_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(vin_calibration_value, row, 1, Qt::AlignLeft);
    row++;
  }

  layout->setColumnStretch(2, 1);
  layout->setRowStretch(row, 1);

  misc_settings_box->setLayout(layout);
  return misc_settings_box;
}

QLayout * main_window::setup_homing_settings_layout()
{
  QGridLayout * layout = new QGridLayout();
  int row = 0;

  {
    auto_homing_check = new QCheckBox();
    auto_homing_check->setObjectName("auto_homing_check");
    layout->addWidget(auto_homing_check, row, 0, 1, 2, Qt::AlignLeft);
    row++;
  }

  {
    auto_homing_direction_value = new QComboBox();
    auto_homing_direction_value->setObjectName("auto_homing_direction_value");
    auto_homing_direction_value->addItem("Reverse", 0);
    auto_homing_direction_value->addItem("Forward", 1);
    auto_homing_direction_label = new QLabel();
    auto_homing_direction_label->setBuddy(auto_homing_direction_value);
    layout->addWidget(auto_homing_direction_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(auto_homing_direction_value, row, 1, Qt::AlignLeft);
    row++;
  }

  {
    homing_speed_towards_value = new QSpinBox();
    homing_speed_towards_value->setObjectName("homing_speed_towards_value");
    homing_speed_towards_value->setRange(0, TIC_MAX_ALLOWED_SPEED);
    homing_speed_towards_label = new QLabel();
    homing_speed_towards_label->setBuddy(homing_speed_towards_value);
    homing_speed_towards_value_pretty = new QLabel();
    layout->addWidget(homing_speed_towards_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(homing_speed_towards_value, row, 1, Qt::AlignLeft);
    layout->addWidget(homing_speed_towards_value_pretty, row, 2, Qt::AlignLeft);
    row++;
  }

  {
    homing_speed_away_value = new QSpinBox();
    homing_speed_away_value->setObjectName("homing_speed_away_value");
    homing_speed_away_value->setRange(0, TIC_MAX_ALLOWED_SPEED);
    homing_speed_away_label = new QLabel();
    homing_speed_away_label->setBuddy(homing_speed_towards_value);
    homing_speed_away_value_pretty = new QLabel();
    layout->addWidget(homing_speed_away_label, row, 0, FIELD_LABEL_ALIGNMENT);
    layout->addWidget(homing_speed_away_value, row, 1, Qt::AlignLeft);
    layout->addWidget(homing_speed_away_value_pretty, row, 2, Qt::AlignLeft);
    row++;
  }

  layout->setColumnStretch(3, 1);
  layout->setRowStretch(row, 1);
  return layout;
}

QGroupBox * main_window::setup_homing_settings_box()
{
  homing_settings_box = new QGroupBox();
  homing_settings_box->setLayout(setup_homing_settings_layout());
  return homing_settings_box;
}

QWidget * main_window::setup_homing_settings_widget()
{
  QWidget * widget = new QWidget();
  widget->setLayout(setup_homing_settings_layout());
  return widget;
}


//// end of pages

QLayout * main_window::setup_footer()
{
  deenergize_button = new QPushButton();
  deenergize_button->setObjectName("deenergize_button");
  deenergize_button->setStyleSheet(":enabled { background-color: red; "
    "color: white; font-weight: bold; }");

  resume_button = new QPushButton();
  resume_button->setObjectName("resume_button");
  resume_button->setStyleSheet(":enabled { background-color: green; "
    "color: white; font-weight: bold; }");

  motor_status_value = new elided_label();

  apply_settings_label = new QLabel();
  apply_settings_label->setStyleSheet("QLabel { color: #1f2f93; }");

  apply_settings_button = new QPushButton();
  connect(apply_settings_button, SIGNAL(clicked()),
    apply_settings_action, SLOT(trigger()));
  set_apply_settings_button_stylesheet(0);

  QHBoxLayout * layout = footer_layout = new QHBoxLayout();
  layout->addWidget(deenergize_button,0, Qt::AlignLeft);
  layout->addWidget(resume_button, 0, Qt::AlignLeft);
  layout->addWidget(motor_status_value, 1);
  layout->addWidget(apply_settings_label, 0, Qt::AlignRight);
  layout->addWidget(apply_settings_button, 0, Qt::AlignRight);
  return layout;
}

void main_window::retranslate()
{
  setWindowTitle(tr("Pololu Tic Control Center"));

  file_menu->setTitle(tr("&File"));
  open_settings_action->setText(tr("&Open settings file..."));
  save_settings_action->setText(tr("&Save settings file..."));
  exit_action->setText(tr("E&xit"));
  device_menu->setTitle(tr("&Device"));
  disconnect_action->setText(tr("&Disconnect"));
  clear_driver_error_action->setText(tr("&Clear driver error"));
  go_home_reverse_action->setText(tr("Go &home reverse"));
  go_home_forward_action->setText(tr("Go h&ome forward"));
  reload_settings_action->setText(tr("Re&load settings from device"));
  restore_defaults_action->setText(tr("&Restore default settings"));
  apply_settings_action->setText(tr("&Apply settings"));
  upgrade_firmware_action->setText(tr("&Upgrade firmware..."));
  help_menu->setTitle(tr("&Help"));
  documentation_action->setText(tr("&Online documentation..."));
  about_action->setText(tr("&About..."));

  device_list_label->setText(tr("Connected to:"));

  //// status page

  device_info_box->setTitle(tr("Device info"));
  device_name_label->setText(tr("Name:"));
  serial_number_label->setText(tr("Serial number:"));
  firmware_version_label->setText(tr("Firmware version:"));
  device_reset_label->setText(tr("Last reset:"));
  up_time_label->setText(tr("Up time:"));

  input_status_box->setTitle(tr("Inputs"));
  encoder_position_label->setText(tr("Encoder position:"));
  input_state_label->setText(tr("Input state:"));
  input_after_averaging_label->setText(tr("Input after averaging:"));
  input_after_hysteresis_label->setText(tr("Input after hysteresis:"));
  input_before_scaling_label->setText(tr("Input before scaling:"));
  input_after_scaling_label->setText(tr("Input after scaling:"));

  operation_status_box->setTitle(tr("Operation"));
  vin_voltage_label->setText(tr("VIN voltage:"));
  operation_state_label->setText(tr("Operation state:"));
  energized_label->setText(tr("Energized:"));
  limit_active_label->setText(tr("Limit switches active:"));
  homing_active_label->setText(tr("Homing active:"));
  last_motor_driver_error_label->setText(tr("Last motor\ndriver error:"));
  set_target_none();
  current_position_label->setText(tr("Current position:"));
  position_uncertain_label->setText(tr("Uncertain:"));
  current_velocity_label->setText(tr("Current velocity:"));

  if (errors_box)
  {
    errors_box->setTitle(tr("Errors"));
  }
  errors_stopping_header_label->setText(tr("Stopping motor?"));
  errors_count_header_label->setText(tr("Count"));
  error_rows[TIC_ERROR_INTENTIONALLY_DEENERGIZED].name_label->setText(tr("Intentionally de-energized"));
  error_rows[TIC_ERROR_MOTOR_DRIVER_ERROR]       .name_label->setText(tr("Motor driver error"));
  error_rows[TIC_ERROR_LOW_VIN]                  .name_label->setText(tr("Low VIN"));
  error_rows[TIC_ERROR_KILL_SWITCH]              .name_label->setText(tr("Kill switch active"));
  error_rows[TIC_ERROR_REQUIRED_INPUT_INVALID]   .name_label->setText(tr("Required input invalid"));
  error_rows[TIC_ERROR_COMMAND_TIMEOUT]          .name_label->setText(tr("Command timeout"));
  error_rows[TIC_ERROR_SAFE_START_VIOLATION]     .name_label->setText(tr("Safe start violation"));
  error_rows[TIC_ERROR_ERR_LINE_HIGH]            .name_label->setText(tr("ERR line high"));
  error_rows[TIC_ERROR_SERIAL_ERROR]             .name_label->setText(tr("Serial errors:"));
  error_rows[TIC_ERROR_SERIAL_FRAMING]           .name_label->setText(INDENT(tr("Framing")));
  error_rows[TIC_ERROR_SERIAL_RX_OVERRUN]        .name_label->setText(INDENT(tr("RX overrun")));
  error_rows[TIC_ERROR_SERIAL_FORMAT]            .name_label->setText(INDENT(tr("Format")));
  error_rows[TIC_ERROR_SERIAL_CRC]               .name_label->setText(INDENT(tr("CRC")));
  error_rows[TIC_ERROR_ENCODER_SKIP]             .name_label->setText(tr("Encoder skip"));
  errors_reset_counts_button->setText(tr("Reset c&ounts"));

  if (manual_target_box)
  {
    manual_target_box->setTitle(tr(u8"Set target (Serial\u2009/\u2009I\u00B2C\u2009/\u2009USB mode only)"));
  }
  manual_target_position_mode_radio->setText(tr("Set &position"));
  manual_target_velocity_mode_radio->setText(tr("Set &velocity"));
  update_manual_target_controls();
  set_current_position_button->setText(tr("Set &current position"));
  current_position_halts_label->setText(tr("(will halt motor)"));
  auto_set_target_check->setText(tr("Set target &when slider or entry box are changed"));
  auto_zero_target_check->setText(tr("Return slider to &zero when it is released"));
  halt_button->setText(tr("Ha&lt motor"));
  decelerate_button->setText(tr("D&ecelerate motor"));

  //// settings page
  // [all-settings]

  control_mode_label->setText(tr("Control mode:"));

  serial_settings_box->setTitle(tr("Serial"));
  serial_baud_rate_label->setText(tr("Baud rate:"));
  serial_device_number_label->setText(tr("Device number:"));
  serial_enable_alt_device_number_check->setText(tr(
    "Alternative device number:"));
  serial_14bit_device_number_check->setText(tr("Enable 14-bit device number"));
  command_timeout_check->setText(tr("Enable command timeout:"));
  serial_crc_for_commands_check->setText(tr("Enable CRC for commands"));
  serial_crc_for_responses_check->setText(tr("Enable CRC for responses"));
  serial_7bit_responses_check->setText(tr("Enable 7-bit responses"));
  serial_response_delay_label->setText(tr("Response delay:"));
  serial_response_delay_value->setToolTip(tr(
      "The minimum time the Tic delays before replying to a serial command and "
      "the minimum time the Tic will stretch the I\u00B2C clock."));

  encoder_settings_box->setTitle(tr("Encoder"));
  encoder_prescaler_label->setText(tr("Prescaler:"));
  encoder_postscaler_label->setText(tr("Postscaler:"));
  encoder_unlimited_check->setText(tr("Enable unbounded position control"));

  conditioning_settings_box->setTitle(tr("Input conditioning"));
  input_averaging_enabled_check->setText(tr("Enable input averaging"));
  input_hysteresis_label->setText(tr("Input hysteresis:"));

  scaling_settings_box->setTitle(tr("RC and analog scaling"));
  input_learn_button->setText(tr("Lear&n..."));
  input_invert_check->setText(tr("Invert input direction"));
  scaling_input_label->setText(tr("Input"));
  scaling_target_label->setText(tr("Target"));
  scaling_min_label->setText(tr("Minimum:"));
  scaling_neutral_min_label->setText(tr("Neutral min:"));
  scaling_neutral_max_label->setText(tr("Neutral max:"));
  scaling_max_label->setText(tr("Maximum:"));
  input_scaling_degree_label->setText(tr("Scaling degree:"));

  if (motor_settings_box)
  {
    motor_settings_box->setTitle(tr("Motor"));
  }
  invert_motor_direction_check->setText(tr("Invert motor direction"));
  speed_max_label->setText(tr("Max speed:"));
  starting_speed_label->setText(tr("Starting speed:"));
  accel_max_label->setText(tr("Max acceleration:"));
  decel_max_label->setText(tr("Max deceleration:"));
  decel_accel_max_same_check->setText(tr("Use max acceleration limit for deceleration"));
  step_mode_label->setText(tr("Step mode:"));
  current_limit_label->setText(tr("Current limit:"));
  current_limit_warning_label->setText(tr("WARNING: high current"));
  decay_mode_label->setText(tr("Decay mode:"));
  agc_mode_label->setText(tr("AGC mode:"));
  agc_bottom_current_limit_label->setText(tr("AGC bottom current limit:"));
  agc_current_boost_steps_label->setText(tr("AGC current boost steps:"));
  agc_frequency_limit_label->setText(tr("AGC frequency limit:"));
  hp_toff_label->setText(tr("Fixed off time:"));
  hp_tblank_label->setText(tr("Current trip blanking time:"));
  hp_abt_check->setText(tr("Enable adaptive blanking time"));
  hp_tdecay_label->setText(tr("Mixed decay transition time:"));

  //// advanced settings page

  pin_config_box->setTitle(tr("Pin configuration"));
  pin_config_rows[TIC_PIN_NUM_SCL]->name_label->setText("SCL:");
  pin_config_rows[TIC_PIN_NUM_SDA]->name_label->setText(u8"SDA\u200A/\u200AAN:");
  pin_config_rows[TIC_PIN_NUM_TX]->name_label->setText("TX:");
  pin_config_rows[TIC_PIN_NUM_RX]->name_label->setText("RX:");
  pin_config_rows[TIC_PIN_NUM_RC]->name_label->setText("RC:");

  for (pin_config_row * pcr : pin_config_rows)
  {
    if (pcr->pullup_check != NULL)
    {
      pcr->pullup_check->setText(tr("Pull-up"));
    }
    if (pcr->polarity_check != NULL)
    {
      pcr->polarity_check->setText(tr("Active high"));
    }
    if (pcr->analog_check != NULL)
    {
      pcr->analog_check->setText(tr("Analog"));
    }
  }

  error_settings_box->setTitle(tr("Soft error response"));
  soft_error_response_radio_group->button(TIC_RESPONSE_DEENERGIZE)->setText(tr("De-energize"));
  soft_error_response_radio_group->button(TIC_RESPONSE_HALT_AND_HOLD)->setText(tr("Halt and hold"));
  soft_error_response_radio_group->button(TIC_RESPONSE_DECEL_TO_HOLD)->setText(tr("Decelerate to hold"));
  soft_error_response_radio_group->button(TIC_RESPONSE_GO_TO_POSITION)->setText(tr("Go to position:"));
  current_limit_during_error_check->setText(tr("Use different current limit during soft error:"));
  current_limit_during_error_warning_label->setText(tr("WARNING: high current"));

  misc_settings_box->setTitle(tr("Miscellaneous"));
  disable_safe_start_check->setText(tr("Disable safe start"));
  ignore_err_line_high_check->setText(tr("Ignore ERR line high"));
  auto_clear_driver_error_check->setText(tr("Automatically clear driver errors"));
  hp_enable_unrestricted_current_limits_check->setText(tr(
    "Enable unrestricted current limits"));
  hp_enable_unrestricted_current_limits_check->setToolTip(tr(
    "When checked, allows current limits above 4000 mA on the Tic 36v4,\n" \
    "potentially resulting in overheating and permanent damage!"
  ));
  never_sleep_check->setText(tr("Never sleep (ignore USB suspend)"));
  vin_calibration_label->setText(tr("VIN measurement calibration:"));

  if (homing_settings_box)
  {
    homing_settings_box->setTitle(tr("Homing"));
  }
  auto_homing_check->setText(tr("Enable automatic homing"));
  auto_homing_direction_label->setText(tr("Automatic homing direction:"));
  homing_speed_towards_label->setText(tr("Homing speed towards:"));
  homing_speed_away_label->setText(tr("Homing speed away:"));


  //// end pages

  deenergize_button->setText(tr("De-ener&gize"));
  resume_button->setText(tr("&Resume"));
  apply_settings_label->setText(tr("There are unapplied changes."));
  apply_settings_label->setToolTip(tr(
    "You changed some settings but have not saved them to your device yet."
  ));
  apply_settings_button->setText(apply_settings_action->text());
}

// things that need to be resized after text is set
void main_window::adjust_sizes()
{
  halt_button->setMinimumWidth(decelerate_button->sizeHint().width());
}

main_window::~main_window()
{
  // Nettoyage si nécessaire. Si rien à nettoyer,
  // le destructeur peut être vide mais doit quand même être défini.
}
