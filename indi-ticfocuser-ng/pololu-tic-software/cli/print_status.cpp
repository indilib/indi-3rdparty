#include "cli.h"

static const int left_column_width = 30;
static auto left_column = std::setw(left_column_width);

static std::string pretty_up_time(uint32_t up_time)
{
  std::ostringstream ss;
  uint32_t seconds = up_time / 1000;
  uint32_t minutes = seconds / 60;
  uint16_t hours = minutes / 60;

  ss << hours <<
    ":" << std::setfill('0') << std::setw(2) << minutes % 60 <<
    ":" << std::setfill('0') << std::setw(2) << seconds % 60;
  return ss.str();
}

static std::string convert_mv_to_v_string(uint32_t mv, bool full_output)
{
  std::ostringstream ss;

  if (full_output)
  {
    ss << (mv / 1000) << "." << (mv / 100 % 10) << (mv / 10 % 10) << (mv % 10);
  }
  else
  {
    uint32_t dv = (mv + 50) / 100;
    ss << (dv / 10) << "." << (dv % 10);
  }
  ss << " V";
  return ss.str();
}

static void print_errors(uint32_t errors, const char * error_set_name)
{
  if (!errors)
  {
    std::cout << error_set_name << ": None" << std::endl;
    return;
  }

  std::cout << error_set_name << ":" << std::endl;
  for (uint32_t i = 0; i < 32; i++)
  {
    uint32_t error = (1 << i);
    if (errors & error)
    {
      std::cout << "  - " << tic_look_up_error_name_ui(error) << std::endl;
    }
  }
}

static void print_hp_driver_errors(uint32_t errors)
{
  const char * error_set_name = "Last motor driver errors";
  if (!errors)
  {
    std::cout << error_set_name << ": None" << std::endl;
    return;
  }

  std::cout << error_set_name << ":" << std::endl;
  for (uint32_t i = 0; i < 32; i++)
  {
    uint32_t error = (1 << i);
    if (errors & error)
    {
      std::cout << "  - " << tic_look_up_hp_driver_error_name_ui(error)
        << std::endl;
    }
  }
}

static std::string input_format(uint16_t input)
{
  if (input == TIC_INPUT_NULL)
  {
    return std::string("N/A");
  }

  return std::to_string(input);
}

static void print_pin_info(const tic::variables & vars,
  uint8_t pin, const char * pin_name)
{
  std::cout<< pin_name << " pin:" << std::endl;
  if (pin != TIC_PIN_NUM_RC)
  {
    std::cout << left_column << "  State: "
      << tic_look_up_pin_state_name_ui(vars.get_pin_state(pin))
      << std::endl;
    std::cout << left_column << "  Analog reading: "
      << input_format(vars.get_analog_reading(pin)) << std::endl;
  }

  // Note: The ternary operator below prevents GCC from printing 255 when we are
  // running our test that bends the rules of C/C++, withs 'vars' pointing to
  // memory that is full of 0xFF bytes.  (This is a fragile fix that could break
  // in future versions of GCC, but it does no harm.)
  std::cout << left_column << "  Digital reading: "
      << (vars.get_digital_reading(pin) ? '1' : '0') << std::endl;
}

void print_status(const tic::variables & vars,
  const tic::settings & settings,
  const std::string & name,
  const std::string & serial_number,
  const std::string & firmware_version,
  bool full_output)
{
  // The output here is YAML so that people can more easily write scripts that
  // use it.

  uint8_t product = tic_settings_get_product(settings.get_pointer());

  std::cout << std::left << std::setfill(' ');

  std::cout << left_column << "Name: "
    << name << std::endl;

  std::cout << left_column << "Serial number: "
    << serial_number << std::endl;

  std::cout << left_column << "Firmware version: "
    << firmware_version << std::endl;

  std::cout << left_column << "Last reset: "
    << tic_look_up_device_reset_name_ui(vars.get_device_reset())
    << std::endl;

  std::cout << left_column << "Up time: "
    << pretty_up_time(vars.get_up_time())
    << std::endl;

  std::cout << std::endl;

  std::cout << left_column << "Encoder position: "
      << vars.get_encoder_position()
      << std::endl;

  if (full_output)
  {
    std::cout << left_column << "RC pulse width: "
      << input_format(vars.get_rc_pulse_width())
      << std::endl;
  }

  std::cout << left_column << "Input state: "
    << tic_look_up_input_state_name_ui(vars.get_input_state())
    << std::endl;

  std::cout << left_column << "Input after averaging: "
    << input_format(vars.get_input_after_averaging())
    << std::endl;

  std::cout << left_column << "Input after hysteresis: "
    << input_format(vars.get_input_after_hysteresis())
    << std::endl;

  if (settings)
  {
    std::cout << left_column << "Input before scaling: "
      << input_format(vars.get_input_before_scaling(settings))
      << std::endl;
  }

  std::cout << left_column << "Input after scaling: "
    << vars.get_input_after_scaling()
    << std::endl;

  std::cout << left_column << "Forward limit active: "
    << (vars.get_forward_limit_active() ? "Yes" : "No")
    << std::endl;

  std::cout << left_column << "Reverse limit active: "
    << (vars.get_reverse_limit_active() ? "Yes" : "No")
    << std::endl;

  std::cout << std::endl;

  std::cout << left_column << "VIN voltage: "
    << convert_mv_to_v_string(vars.get_vin_voltage(), full_output)
    << std::endl;

  std::cout << left_column << "Operation state: "
    << tic_look_up_operation_state_name_ui(vars.get_operation_state())
    << std::endl;

  std::cout << left_column << "Energized: "
    << (vars.get_energized() ? "Yes" : "No")
    << std::endl;

  std::cout << left_column << "Homing active: "
    << (vars.get_homing_active() ? "Yes" : "No")
    << std::endl;

  if (product == TIC_PRODUCT_T249)
  {
    std::cout << left_column << "Last motor driver error: "
      << tic_look_up_motor_driver_error_name_ui(vars.get_last_motor_driver_error())
      << std::endl;
  }

  std::cout << std::endl;

  uint8_t planning_mode = vars.get_planning_mode();

  if (planning_mode == TIC_PLANNING_MODE_TARGET_POSITION)
  {
    std::cout << left_column << "Target position: "
      << vars.get_target_position() << std::endl;
  }
  else if (planning_mode == TIC_PLANNING_MODE_TARGET_VELOCITY)
  {
    std::cout << left_column << "Target velocity: "
      << vars.get_target_velocity() << std::endl;
  }
  else
  {
    std::cout << left_column << "Target: " << "No target" << std::endl;
  }

  std::cout << left_column << "Current position: "
            << vars.get_current_position() << std::endl;

  std::cout << left_column << "Position uncertain: "
            << (vars.get_position_uncertain() ? "Yes" : "No")
            << std::endl;

  std::cout << left_column << "Current velocity: "
    << vars.get_current_velocity() << std::endl;

  if (full_output)
  {
    std::cout << left_column << "Max speed: "
      << vars.get_max_speed() << std::endl;

    std::cout << left_column << "Starting speed: "
      << vars.get_starting_speed() << std::endl;

    std::cout << left_column << "Max acceleration: "
      << vars.get_max_accel() << std::endl;

    std::cout << left_column << "Max deceleration: "
      << vars.get_max_decel() << std::endl;

    std::cout << left_column << "Acting target position: "
      << vars.get_acting_target_position() << std::endl;

    std::cout << left_column << "Time since last step: "
      << vars.get_time_since_last_step() << std::endl;

    std::cout << left_column << "Step mode: "
      << tic_look_up_step_mode_name_ui(vars.get_step_mode())
      << std::endl;

    std::cout << left_column << "Current limit: "
      << vars.get_current_limit() << " mA"
      << std::endl;

    if (product == TIC_PRODUCT_T825 ||
      product == TIC_PRODUCT_N825 ||
      product == TIC_PRODUCT_T834)
    {
      const char * decay_name;
      tic_look_up_decay_mode_name(vars.get_decay_mode(), product, 0, &decay_name);
      std::cout << left_column << "Decay mode: " << decay_name << std::endl;
    }

    if (product == TIC_PRODUCT_T249)
    {
      std::cout << left_column << "AGC mode: "
        << tic_look_up_agc_mode_name_ui(vars.get_agc_mode())
        << std::endl;

      std::cout << left_column << "AGC bottom current limit: "
        << tic_look_up_agc_bottom_current_limit_name_ui(vars.get_agc_bottom_current_limit())
        << std::endl;

      std::cout << left_column << "AGC current boost steps: "
        << tic_look_up_agc_current_boost_steps_name_ui(vars.get_agc_current_boost_steps())
        << std::endl;

      std::cout << left_column << "AGC frequency limit: "
        << tic_look_up_agc_frequency_limit_name_ui(vars.get_agc_frequency_limit())
        << std::endl;
    }
  }

  std::cout << std::endl;

  print_errors(vars.get_error_status(),
    "Errors currently stopping the motor");
  print_errors(vars.get_errors_occurred(),
    "Errors that occurred since last check");
  if (product == TIC_PRODUCT_36V4)
  {
    print_hp_driver_errors(vars.get_last_hp_driver_errors());
  }
  std::cout << std::endl;

  if (full_output)
  {
    print_pin_info(vars, TIC_PIN_NUM_SCL, "SCL");
    print_pin_info(vars, TIC_PIN_NUM_SDA, "SDA");
    print_pin_info(vars, TIC_PIN_NUM_TX, "TX");
    print_pin_info(vars, TIC_PIN_NUM_RX, "RX");
    print_pin_info(vars, TIC_PIN_NUM_RC, "RC");
    std::cout << std::endl;
  }
}
