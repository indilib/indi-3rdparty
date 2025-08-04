#include "cli.h"

static const char help[] =
  CLI_NAME ": Pololu Tic Command-line Utility\n"
  "Version " SOFTWARE_VERSION_STRING "\n"
  "Usage: " CLI_NAME " OPTIONS\n"
  "\n"
  "General options:\n"
  "  -s, --status                 Show device settings and info.\n"
  "  --full                       When used with --status, shows more.\n"
  "  -d SERIALNUMBER              Specifies the serial number of the device.\n"
  "  --list                       List devices connected to computer.\n"
  "  --pause                      Pause program at the end.\n"
  "  --pause-on-error             Pause program at the end if an error happens.\n"
  "  -h, --help                   Show this help screen.\n"
  "\n"
  "Control commands:\n"
  "  -p, --position NUM           Set target position in microsteps.\n"
  "  --position-relative NUM      Set target position relative to current pos.\n"
  "  -y, --velocity NUM           Set target velocity in microsteps / 10000 s.\n"
  "  --halt-and-set-position NUM  Set where the controller thinks it currently is.\n"
  "  --halt-and-hold              Abruptly stop the motor.\n"
  "  --home DIR                   Drive to limit switch; DIR is 'fwd' or 'rev'.\n"
  "  --reset-command-timeout      Clears the command timeout error.\n"
  "  --deenergize                 Disable the motor driver.\n"
  "  --energize                   Stop disabling the driver.\n"
  "  --exit-safe-start            Send the exit safe start command.\n"
  "  --resume                     Equivalent to --energize with --exit-safe-start.\n"
  "  --enter-safe-start           Send the enter safe start command.\n"
  "  --reset                      Make the controller forget its current state.\n"
  "  --clear-driver-error         Attempt to clear a motor driver error.\n"
  "\n"
  "Temporary settings:\n"
  "  --max-speed NUM              Set the speed limit.\n"
  "  --starting-speed NUM         Set the starting speed.\n"
  "  --max-accel NUM              Set the acceleration limit.\n"
  "  --max-decel NUM              Set the deceleration limit.\n"
  "  --step-mode MODE             Set step mode: full, half, 1, 2, 2_100p, 4, 8,\n"
  "                               16, 32, 64, 128, 256.\n"
  "  --current NUM                Set the current limit in mA.\n"
  "  --decay MODE                 Set decay mode:\n"
  "                               Tic T825/N825: mixed, slow, or fast\n"
  "                               T834: slow, mixed25, mixed50, mixed75, or fast\n"
  "\n"
  "Temporary settings for AGC on the Tic T249:\n"
  "  --agc-mode MODE                    Set AGC mode: on, off, active_off\n"
  "  --agc-bottom-current-limit LIMIT   Set AGC bottom current limit %:\n"
  "                                     45, 50, 55, 60, 65, 70, 75, or 80.\n"
  "  --agc-current-boost-steps STEPS    Set AGC current boost steps:\n"
  "                                     5, 7, 9, or 11.\n"
  "  --agc-frequency-limit LIMIT        Set AGC frequency limit in Hz:\n"
  "                                     off, 225, 450, or 675.\n"
  "\n"
  "Permanent settings:\n"
  "  --restore-defaults           Restore device's factory settings\n"
  "  --settings FILE              Load settings file into device.\n"
  "  --get-settings FILE          Read device settings and write to file.\n"
  "  --fix-settings IN OUT        Read settings from a file and fix them.\n"
  "\n"
  "For more help, see: " DOCUMENTATION_URL "\n"
  "\n";

struct arguments
{
  bool show_status = false;

  bool full_output = false;

  bool serial_number_specified = false;
  std::string serial_number;

  bool show_list = false;

  bool pause = false;

  bool pause_on_error = false;

  bool show_help = false;

  bool set_target_position = false;
  int32_t target_position;

  bool set_target_position_relative = false;
  int32_t target_position_relative;

  bool set_target_velocity = false;
  int32_t target_velocity;

  bool halt_and_set_position = false;
  int32_t position;

  bool halt_and_hold = false;

  bool go_home = false;
  uint8_t homing_direction;

  bool reset_command_timeout = false;

  bool deenergize = false;

  bool energize = false;

  bool exit_safe_start = false;

  bool enter_safe_start = false;

  bool reset = false;

  bool clear_driver_error = false;

  bool set_max_speed = false;
  uint32_t max_speed;

  bool set_starting_speed = false;
  uint32_t starting_speed;

  bool set_max_accel = false;
  uint32_t max_accel;

  bool set_max_decel = false;
  uint32_t max_decel;

  bool set_step_mode = false;
  uint8_t step_mode;

  bool set_current_limit = false;
  uint32_t current_limit;

  bool set_decay_mode = false;
  uint8_t decay_mode;

  bool set_agc_mode = false;
  uint8_t agc_mode;

  bool set_agc_bottom_current_limit = false;
  uint8_t agc_bottom_current_limit;

  bool set_agc_current_boost_steps = false;
  uint8_t agc_current_boost_steps;

  bool set_agc_frequency_limit = false;
  uint8_t agc_frequency_limit;

  bool restore_defaults = false;

  bool set_settings = false;
  std::string set_settings_filename;

  bool get_settings = false;
  std::string get_settings_filename;

  bool fix_settings = false;
  std::string fix_settings_input_filename;
  std::string fix_settings_output_filename;

  bool get_debug_data = false;

  uint32_t test_procedure = 0;

  bool action_specified() const
  {
    return show_status ||
      show_list ||
      show_help ||
      set_target_position ||
      set_target_position_relative ||
      set_target_velocity ||
      halt_and_set_position ||
      halt_and_hold ||
      go_home ||
      reset_command_timeout ||
      deenergize ||
      energize ||
      exit_safe_start ||
      enter_safe_start ||
      reset ||
      clear_driver_error ||
      set_max_speed ||
      set_starting_speed ||
      set_max_accel ||
      set_max_decel ||
      set_step_mode ||
      set_current_limit ||
      set_decay_mode ||
      set_agc_mode ||
      set_agc_bottom_current_limit ||
      set_agc_current_boost_steps ||
      set_agc_frequency_limit ||
      restore_defaults ||
      set_settings ||
      get_settings ||
      fix_settings ||
      get_debug_data ||
      test_procedure;
  }
};

template <typename T>
static T parse_arg_int(arg_reader & arg_reader)
{
  const char * value_c = arg_reader.next();
  if (value_c == NULL)
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "Expected a number after '" + std::string(arg_reader.last()) + "'.");
  }

  T result;
  uint8_t error = string_to_int(value_c, &result);
  if (error == STRING_TO_INT_ERR_SMALL)
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The number after '" + std::string(arg_reader.last()) + "' is too small.");
  }
  if (error == STRING_TO_INT_ERR_LARGE)
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The number after '" + std::string(arg_reader.last()) + "' is too large.");
  }
  if (error)
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The number after '" + std::string(arg_reader.last()) + "' is invalid.");
  }

  return result;
}

static std::string parse_arg_string(arg_reader & arg_reader)
{
    const char * value_c = arg_reader.next();
    if (value_c == NULL)
    {
      throw exception_with_exit_code(EXIT_BAD_ARGS,
        "Expected an argument after '" +
        std::string(arg_reader.last()) + "'.");
    }
    if (value_c[0] == 0)
    {
      throw exception_with_exit_code(EXIT_BAD_ARGS,
        "Expected a non-empty argument after '" +
        std::string(arg_reader.last()) + "'.");
    }
    return std::string(value_c);
}

static uint8_t parse_arg_step_mode(arg_reader & arg_reader)
{
  std::string mode_str = parse_arg_string(arg_reader);
  if (mode_str == "1" || mode_str == "full"
    || mode_str == "Full step" || mode_str == "full step")
  {
    return TIC_STEP_MODE_MICROSTEP1;
  }
  else if (mode_str == "2" || mode_str == "half" || mode_str == "1/2 step")
  {
    return TIC_STEP_MODE_MICROSTEP2;
  }
  else if (mode_str == "2_100p")
  {
    return TIC_STEP_MODE_MICROSTEP2_100P;
  }
  else if (mode_str == "4" || mode_str == "1/4 step")
  {
    return TIC_STEP_MODE_MICROSTEP4;
  }
  else if (mode_str == "8" || mode_str == "1/8 step")
  {
    return TIC_STEP_MODE_MICROSTEP8;
  }
  else if (mode_str == "16" || mode_str == "1/16 step")
  {
    return TIC_STEP_MODE_MICROSTEP16;
  }
  else if (mode_str == "32" || mode_str == "1/32 step")
  {
    return TIC_STEP_MODE_MICROSTEP32;
  }
  else if (mode_str == "64" || mode_str == "1/64 step")
  {
    return TIC_STEP_MODE_MICROSTEP64;
  }
  else if (mode_str == "128" || mode_str == "1/128 step")
  {
    return TIC_STEP_MODE_MICROSTEP128;
  }
  else if (mode_str == "256" || mode_str == "1/256 step")
  {
    return TIC_STEP_MODE_MICROSTEP256;
  }
  else
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The step mode specified is invalid.");
  }
}

static uint8_t parse_arg_decay_mode(arg_reader & arg_reader)
{
  std::string decay_str = parse_arg_string(arg_reader);

  uint8_t code;

  if (!tic_look_up_decay_mode_code(decay_str.c_str(),
      0, TIC_NAME_UI | TIC_NAME_SNAKE_CASE, &code))
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The decay mode specified is invalid.");
  }

  return code;
}

static uint8_t parse_arg_agc_mode(arg_reader & arg_reader)
{
  std::string str = parse_arg_string(arg_reader);
  if (str == "on")
  {
    return TIC_AGC_MODE_ON;
  }
  else if (str == "off")
  {
    return TIC_AGC_MODE_OFF;
  }
  else if (str == "active_off")
  {
    return TIC_AGC_MODE_ACTIVE_OFF;
  }
  else
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The AGC mode specified is invalid.");
  }
}

static uint8_t parse_arg_agc_bottom_current_limit(arg_reader & arg_reader)
{
  std::string str = parse_arg_string(arg_reader);
  if (str == "45")
  {
    return TIC_AGC_BOTTOM_CURRENT_LIMIT_45;
  }
  else if (str == "50")
  {
    return TIC_AGC_BOTTOM_CURRENT_LIMIT_50;
  }
  else if (str == "55")
  {
    return TIC_AGC_BOTTOM_CURRENT_LIMIT_55;
  }
  else if (str == "60")
  {
    return TIC_AGC_BOTTOM_CURRENT_LIMIT_60;
  }
  else if (str == "65")
  {
    return TIC_AGC_BOTTOM_CURRENT_LIMIT_65;
  }
  else if (str == "70")
  {
    return TIC_AGC_BOTTOM_CURRENT_LIMIT_70;
  }
  else if (str == "75")
  {
    return TIC_AGC_BOTTOM_CURRENT_LIMIT_75;
  }
  else if (str == "80")
  {
    return TIC_AGC_BOTTOM_CURRENT_LIMIT_80;
  }
  else
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The AGC bottom current limit specified is invalid.");
  }
}

static uint8_t parse_arg_agc_current_boost_steps(arg_reader & arg_reader)
{
  std::string str = parse_arg_string(arg_reader);
  if (str == "5")
  {
    return TIC_AGC_CURRENT_BOOST_STEPS_5;
  }
  else if (str == "7")
  {
    return TIC_AGC_CURRENT_BOOST_STEPS_7;
  }
  else if (str == "9")
  {
    return TIC_AGC_CURRENT_BOOST_STEPS_9;
  }
  else if (str == "11")
  {
    return TIC_AGC_CURRENT_BOOST_STEPS_11;
  }
  else
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The AGC current boost steps number specified is invalid.");
  }
}

static uint8_t parse_arg_agc_frequency_limit(arg_reader & arg_reader)
{
  std::string str = parse_arg_string(arg_reader);
  if (str == "off")
  {
    return TIC_AGC_FREQUENCY_LIMIT_OFF;
  }
  else if (str == "225")
  {
    return TIC_AGC_FREQUENCY_LIMIT_225;
  }
  else if (str == "450")
  {
    return TIC_AGC_FREQUENCY_LIMIT_450;
  }
  else if (str == "675")
  {
    return TIC_AGC_FREQUENCY_LIMIT_675;
  }
  else
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The AGC frequency limit specified is invalid.");
  }
}

static uint8_t parse_arg_homing_direction(arg_reader & arg_reader)
{
  std::string str = parse_arg_string(arg_reader);
  if (str == "fwd" || str == "forward")
  {
    return TIC_GO_HOME_FORWARD;
  }
  else if (str == "rev" || str == "reverse")
  {
    return TIC_GO_HOME_REVERSE;
  }
  else
  {
    throw exception_with_exit_code(EXIT_BAD_ARGS,
      "The homing direction specified is invalid.");
  }
}

static arguments parse_args(int argc, char ** argv)
{
  arg_reader arg_reader(argc, argv);
  arguments args;

  while (1)
  {
    const char * arg_c = arg_reader.next();
    if (arg_c == NULL)
    {
      break;  // Done reading arguments.
    }

    std::string arg = arg_c;

    if (arg == "-s" || arg == "--status")
    {
      args.show_status = true;
    }
    else if (arg == "--full")
    {
      args.full_output = true;
    }
    else if (arg == "-d" || arg == "--serial")
    {
      args.serial_number_specified = true;
      args.serial_number = parse_arg_string(arg_reader);

      // Remove a pound sign at the beginning of the string because people might
      // copy that from the GUI.
      if (args.serial_number[0] == '#')
      {
        args.serial_number.erase(0, 1);
      }
    }
    else if (arg == "--list")
    {
      args.show_list = true;
    }
    else if (arg == "--pause")
    {
      args.pause = true;
    }
    else if (arg == "--pause-on-error")
    {
      args.pause_on_error = true;
    }
    else if (arg == "-h" || arg == "--help" ||
      arg == "--h" || arg == "-help" || arg == "/help" || arg == "/h")
    {
      args.show_help = true;
    }
    else if (arg == "-p" || arg == "--position")
    {
      args.set_target_position = true;
      args.set_target_position_relative = false;
      args.target_position = parse_arg_int<int32_t>(arg_reader);
    }
    else if (arg == "--position-relative")
    {
      args.set_target_position_relative = true;
      args.set_target_position = false;
      args.target_position_relative = parse_arg_int<int32_t>(arg_reader);
    }
    else if (arg == "-y" || arg == "--velocity")
    {
      args.set_target_velocity = true;
      args.target_velocity = parse_arg_int<int32_t>(arg_reader);
    }
    else if (arg == "--halt-and-set-position")
    {
      args.halt_and_set_position = true;
      args.position = parse_arg_int<int32_t>(arg_reader);
    }
    else if (arg == "--halt-and-hold")
    {
      args.halt_and_hold = true;
    }
    else if (arg == "--home" || arg == "--go-home")
    {
      args.go_home = true;
      args.homing_direction = parse_arg_homing_direction(arg_reader);
    }
    else if (arg == "--reset-command-timeout")
    {
      args.reset_command_timeout = true;
    }
    else if (arg == "--deenergize" || arg == "--de-energize")
    {
      args.deenergize = true;
    }
    else if (arg == "--energize")
    {
      args.energize = true;
    }
    else if (arg == "--exit-safe-start")
    {
      args.exit_safe_start = true;
    }
    else if (arg == "--resume")
    {
      args.energize = args.exit_safe_start = true;
    }
    else if (arg == "--enter-safe-start")
    {
      args.enter_safe_start = true;
    }
    else if (arg == "--reset")
    {
      args.reset = true;
    }
    else if (arg == "--clear-driver-error")
    {
      args.clear_driver_error = true;
    }
    else if (arg == "--max-speed")
    {
      args.set_max_speed = true;
      args.max_speed = parse_arg_int<uint32_t>(arg_reader);
    }
    else if (arg == "--starting-speed")
    {
      args.set_starting_speed = true;
      args.starting_speed = parse_arg_int<uint32_t>(arg_reader);
    }
    else if (arg == "--max-accel")
    {
      args.set_max_accel = true;
      args.max_accel = parse_arg_int<uint32_t>(arg_reader);
    }
    else if (arg == "--max-decel")
    {
      args.set_max_decel = true;
      args.max_decel = parse_arg_int<uint32_t>(arg_reader);
    }
    else if (arg == "--step-mode")
    {
      args.set_step_mode = true;
      args.step_mode = parse_arg_step_mode(arg_reader);
    }
    else if (arg == "--current" || arg == "--current-limit")
    {
      args.set_current_limit = true;
      args.current_limit = parse_arg_int<uint32_t>(arg_reader);
    }
    else if (arg == "--decay" || arg == "--decay-mode")
    {
      args.set_decay_mode = true;
      args.decay_mode = parse_arg_decay_mode(arg_reader);
    }
    else if (arg == "--agc-mode")
    {
      args.set_agc_mode = true;
      args.agc_mode = parse_arg_agc_mode(arg_reader);
    }
    else if (arg == "--agc-bottom-current-limit")
    {
      args.set_agc_bottom_current_limit = true;
      args.agc_bottom_current_limit = parse_arg_agc_bottom_current_limit(arg_reader);
    }
    else if (arg == "--agc-current-boost-steps")
    {
      args.set_agc_current_boost_steps = true;
      args.agc_current_boost_steps = parse_arg_agc_current_boost_steps(arg_reader);
    }
    else if (arg == "--agc-frequency-limit")
    {
      args.set_agc_frequency_limit = true;
      args.agc_frequency_limit = parse_arg_agc_frequency_limit(arg_reader);
    }
    else if (arg == "--restore-defaults" || arg == "--restoredefaults")
    {
      args.restore_defaults = true;
    }
    else if (arg == "--settings" || arg == "--set-settings" || arg == "--configure")
    {
      args.set_settings = true;
      args.set_settings_filename = parse_arg_string(arg_reader);
    }
    else if (arg == "--get-settings" || arg == "--getconf")
    {
      args.get_settings = true;
      args.get_settings_filename = parse_arg_string(arg_reader);
    }
    else if (arg == "--fix-settings")
    {
      args.fix_settings = true;
      args.fix_settings_input_filename = parse_arg_string(arg_reader);
      args.fix_settings_output_filename = parse_arg_string(arg_reader);
    }
    else if (arg == "--debug")
    {
      // This is an unadvertized option for helping customers troubleshoot
      // issues with their device.
      args.get_debug_data = true;
    }
    else if (arg == "--test")
    {
      // This option helps us test the software.
      args.test_procedure = parse_arg_int<uint32_t>(arg_reader);
    }
    else
    {
      throw exception_with_exit_code(EXIT_BAD_ARGS,
        std::string("Unknown option: '") + arg + "'.");
    }
  }
  return args;
}

static tic::handle handle(device_selector & selector)
{
  tic::device device = selector.select_device();
  return tic::handle(device);
}

static void print_list(device_selector & selector)
{
  for (const tic::device & instance : selector.list_devices())
  {
    std::cout << std::left << std::setfill(' ');
    std::cout << std::setw(17) << instance.get_serial_number() + "," << " ";
    std::cout << std::setw(45) << instance.get_name();
    std::cout << std::endl;
  }
}

static void set_current_limit_after_warning(device_selector & selector, uint32_t current_limit)
{
  tic::handle handle = ::handle(selector);
  uint8_t product = handle.get_device().get_product();

  uint32_t max_current = tic_get_max_allowed_current(product);
  if (current_limit > max_current)
  {
    current_limit = max_current;
    std::cerr
      << "Warning: The current limit was too high "
      << "so it will be lowered to " << current_limit << " mA." << std::endl;
  }

  handle.set_current_limit(current_limit);
}

static void get_status(device_selector & selector, bool full_output)
{
  tic::device device = selector.select_device();
  tic::handle handle(device);
  tic::settings settings = handle.get_settings();
  tic::variables vars = handle.get_variables(true);
  std::string name = device.get_name();
  std::string serial_number = device.get_serial_number();
  std::string firmware_version = handle.get_firmware_version_string();
  print_status(vars, settings, name, serial_number, firmware_version, full_output);
}

static void restore_defaults(device_selector & selector)
{
  handle(selector).restore_defaults();
}

static void get_settings(device_selector & selector,
  const std::string & filename)
{
  tic::settings settings = handle(selector).get_settings();

  std::string warnings;
  settings.fix(&warnings);
  std::cerr << warnings;

  std::string settings_string = settings.to_string();

  write_string_to_file_or_pipe(filename, settings_string);
}

static void set_settings(device_selector & selector,
  const std::string & filename)
{
  std::string settings_string = read_string_from_file_or_pipe(filename);
  tic::settings settings = tic::settings::read_from_string(settings_string);

  tic::device device = selector.select_device();

  tic_settings_set_product(settings.get_pointer(),
    device.get_product());
  tic_settings_set_firmware_version(settings.get_pointer(),
    device.get_firmware_version());

  std::string warnings;
  settings.fix(&warnings);
  std::cerr << warnings;

  tic::handle handle(device);
  handle.set_settings(settings);
  handle.reinitialize();
}

static void fix_settings(const std::string & input_filename,
  const std::string & output_filename)
{
  std::string in_str = read_string_from_file_or_pipe(input_filename);
  tic::settings settings = tic::settings::read_from_string(in_str);

  std::string warnings;
  settings.fix(&warnings);
  std::cerr << warnings;

  write_string_to_file_or_pipe(output_filename, settings.to_string());
}

static void set_target_position_relative(device_selector & selector,
  int32_t target_position_relative)
{
  tic::device device = selector.select_device();
  tic::handle handle(device);
  tic::variables variables = handle.get_variables();
  int32_t position = (uint32_t)variables.get_current_position() +
    (uint32_t)target_position_relative;
  handle.set_target_position(position);
}

static void print_debug_data(device_selector & selector)
{
  tic::device device = selector.select_device();
  tic::handle handle(device);

  std::vector<uint8_t> data(4096, 0);
  handle.get_debug_data(data);

  for (const uint8_t & byte : data)
  {
    std::cout << std::setfill('0') << std::setw(2) << std::hex
              << (unsigned int)byte << ' ';
  }
  std::cout << std::endl;
}

static void test_procedure(device_selector & selector, uint32_t procedure)
{
  if (procedure == 1)
  {
    // Print some fake variable data to test our print_status().
    tic::variables fake_vars(tic_variables_fake());
    tic::settings settings;
    print_status(fake_vars, settings, "Fake name", "123", "9.99", true);
  }
  else if (procedure == 2)
  {
    tic::device device = selector.select_device();
    tic::handle handle(device);
    while (1)
    {
      tic::variables vars = handle.get_variables();
      std::cout << vars.get_analog_reading(TIC_PIN_NUM_SDA) << ','
                << vars.get_target_position() << ','
                << vars.get_acting_target_position() << ','
                << vars.get_current_position() << ','
                << vars.get_current_velocity() << ','
                << std::endl;
    }
  }
  else if (procedure == 3)
  {
    // Test the current limit features.

    std::vector<uint8_t> products = {
      TIC_PRODUCT_T825,
      TIC_PRODUCT_T834,
      TIC_PRODUCT_T500,
      TIC_PRODUCT_N825,
      TIC_PRODUCT_T249,
      TIC_PRODUCT_36V4,
    };

    for (uint8_t product : products)
    {
      std::vector<uint8_t> codes = tic::get_recommended_current_limit_codes(product);
      uint32_t max_current = tic_get_max_allowed_current(product);
      std::cout << tic_look_up_product_name_short(product) << std::endl;
      uint32_t last_ma = 0;
      uint32_t last_code = 0;
      for (uint8_t code : codes)
      {
        uint32_t ma = tic::current_limit_code_to_ma(product, code);
        std::cout << (uint32_t)code << "," << ma << std::endl;
        if (ma > max_current)
        {
          std::cerr << "Product = " << (int)product << std::endl;
          std::cerr << "Bad code = " << (int)code << std::endl;
          std::cerr << "Current = " << ma << std::endl;
          std::cerr << "Max current = " << max_current << std::endl;
          throw std::runtime_error("Recommended current code with default settings "
            "gives current limit that is too large.");
        }
        if (ma < last_ma)
        {
          throw std::runtime_error("Recommend currents are not in ascending order.");
        }
        for (uint32_t i = last_ma; i < ma; i++)
        {
          if (tic::current_limit_ma_to_code(product, i) != last_code)
          {
            throw std::runtime_error("current_limit_ma_to_code returned wrong value");
          }
        }
        last_ma = ma;
        last_code = code;
      }
      for (uint32_t i = last_ma; i < last_ma + 1000; i++)
      {
        if (tic::current_limit_ma_to_code(product, i) != last_code)
        {
          throw std::runtime_error("current_limit_ma_to_code returned wrong value");
        }
      }
      if (last_ma != max_current)
      {
        throw std::runtime_error("Last recommended current code is not the max current.");
      }
    }
  }
  else
  {
    throw std::runtime_error("Unknown test procedure.");
  }
}

// A note about ordering: We want to do all the setting stuff first because it
// could affect subsequent options.  We want to show the status last, because it
// could be affected by options before it.
static void run(const arguments & args)
{
  if (args.show_help || !args.action_specified())
  {
    std::cout << help;
    return;
  }

  device_selector selector;
  if (args.serial_number_specified)
  {
    selector.specify_serial_number(args.serial_number);
  }

  if (args.show_list)
  {
    print_list(selector);
    return;
  }

  if (args.fix_settings)
  {
    fix_settings(args.fix_settings_input_filename,
      args.fix_settings_output_filename);
  }

  if (args.get_settings)
  {
    get_settings(selector, args.get_settings_filename);
  }

  if (args.restore_defaults)
  {
    restore_defaults(selector);
  }

  if (args.set_settings)
  {
    set_settings(selector, args.set_settings_filename);
  }

  if (args.reset)
  {
    handle(selector).reset();
  }

  if (args.set_max_speed)
  {
    handle(selector).set_max_speed(args.max_speed);
  }

  if (args.set_starting_speed)
  {
    handle(selector).set_starting_speed(args.starting_speed);
  }

  if (args.set_max_accel)
  {
    handle(selector).set_max_accel(args.max_accel);
  }

  if (args.set_max_decel)
  {
    handle(selector).set_max_decel(args.max_decel);
  }

  // Should be before any commands that might start the motor moving again so
  // the Tic does not mistakenly use an old target value for a few milliseconds.
  if (args.set_target_position)
  {
    handle(selector).set_target_position(args.target_position);
  }

  if (args.set_target_position_relative)
  {
    set_target_position_relative(selector, args.target_position_relative);
  }

  // Should be before any commands that might start the motor moving again so
  // the Tic does not mistakenly use an old target value for a few milliseconds.
  if (args.set_target_velocity)
  {
    handle(selector).set_target_velocity(args.target_velocity);
  }

  if (args.halt_and_hold)
  {
    handle(selector).halt_and_hold();
  }

  if (args.go_home)
  {
    handle(selector).go_home(args.homing_direction);
  }

  if (args.reset_command_timeout)
  {
    handle(selector).reset_command_timeout();
  }

  if (args.energize)
  {
    handle(selector).energize();
  }

  // This should be after energize so that --resume does things in the same
  // order as the GUI.
  if (args.exit_safe_start)
  {
    handle(selector).exit_safe_start();
  }

  if (args.enter_safe_start)
  {
    handle(selector).enter_safe_start();
  }

  if (args.halt_and_set_position)
  {
    handle(selector).halt_and_set_position(args.position);
  }

  if (args.set_step_mode)
  {
    handle(selector).set_step_mode(args.step_mode);
  }

  if (args.set_current_limit)
  {
    set_current_limit_after_warning(selector, args.current_limit);
  }

  if (args.set_decay_mode)
  {
    handle(selector).set_decay_mode(args.decay_mode);
  }

  if (args.set_agc_mode)
  {
    handle(selector).set_agc_mode(args.agc_mode);
  }

  if (args.set_agc_bottom_current_limit)
  {
    handle(selector).set_agc_bottom_current_limit(args.agc_bottom_current_limit);
  }

  if (args.set_agc_current_boost_steps)
  {
    handle(selector).set_agc_current_boost_steps(args.agc_current_boost_steps);
  }

  if (args.set_agc_frequency_limit)
  {
    handle(selector).set_agc_frequency_limit(args.agc_frequency_limit);
  }

  if (args.clear_driver_error)
  {
    handle(selector).clear_driver_error();
  }

  if (args.deenergize)
  {
    handle(selector).deenergize();
  }

  if (args.get_debug_data)
  {
    print_debug_data(selector);
  }

  if (args.test_procedure)
  {
    test_procedure(selector, args.test_procedure);
  }

  if (args.show_status)
  {
    get_status(selector, args.full_output);
  }
}

int main(int argc, char ** argv)
{
  int exit_code = 0;

  arguments args;
  try
  {
    args = parse_args(argc, argv);
    run(args);
  }
  catch (const exception_with_exit_code & error)
  {
    std::cerr << "Error: " << error.what() << std::endl;
    exit_code = error.get_code();
  }
  catch (const std::exception & error)
  {
    std::cerr << "Error: " << error.what() << std::endl;
    exit_code = EXIT_OPERATION_FAILED;
  }

  if (args.pause || (args.pause_on_error && exit_code))
  {
    std::cout << "Press enter to continue." << std::endl;
    char input;
    std::cin.get(input);
  }

  return exit_code;
}
