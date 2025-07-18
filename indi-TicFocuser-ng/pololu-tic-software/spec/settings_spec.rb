require_relative 'spec_helper'

def default_settings(product)
  raise ArgumentError if product.nil?
  File.read(File.dirname(__FILE__) +
    "/default_settings/#{product.to_s.downcase}.txt")
end

def test_settings1(product)
  raise ArgumentError if product.nil?
  File.read(File.dirname(__FILE__) +
    "/test_settings1/#{product.to_s.downcase}.txt")
end

def test_cases_for_settings_fix(product)
  defaults = YAML.load(default_settings(product))

  if product == :T500
    lowest_current = 0
    low_current = 1
    default_current = 174
    medium_current = 762
    high_current = 2056
  elsif product == :T249
    lowest_current = 0
    low_current = 40
    default_current = 200
    medium_current = 440
    high_current = 1440
  elsif product == :'36v4'
    lowest_current = 0
    low_current = 72
    default_current = 215
    medium_current = 573
    high_current = 1504
  else
    lowest_current = 0
    low_current = 64
    default_current = 192
    medium_current = 320
    high_current = 2048
  end

  if product == :N825
    non_default_rc_pin_warning =
      "Warning: On the Tic N825, the RC pin is always used for controlling " \
      "the RS-485 transceiver and cannot be used for anything else, so its " \
      "function will be changed to the default.\n"
  end

  max_current = tic_max_allowed_current(product)
  if product == :'36v4'
    max_current = 3939
  end

  cases = [
    [ { 'control_mode' => 'rc_speed', 'soft_error_response' => 'go_to_position' },
      { 'soft_error_response' => 'decel_to_hold' },
      "Warning: The soft error response cannot be \"Go to position\" in a " \
      "speed control mode, so it will be changed to \"Decelerate to hold\".\n"
    ],
    [ { 'control_mode' => 'rc_position', 'soft_error_response' => 'go_to_position' },
      { },
    ],
    [ { 'serial_baud_rate' => 115200 },
      { 'serial_baud_rate' => 115385 }
    ],
    [ { 'serial_baud_rate' => 101 },
      { 'serial_baud_rate' => 200 },
      "Warning: The serial baud rate is too low so it will be changed to 200.\n"
    ],
    [ { 'serial_baud_rate' => 115386 },
      { 'serial_baud_rate' => 115385 },
      "Warning: The serial baud rate is too high so it will be changed to 115385.\n"
    ],
    [ { 'serial_device_number' => 128 },
      { 'serial_device_number' => 0 },
      "Warning: The device number is higher than 127 " \
      "so it will be changed to 0.\n"
    ],
    [ { 'serial_alt_device_number' => 129 },
      { 'serial_alt_device_number' => 1 },
      "Warning: The alternative device number is higher than 127 " \
      "so it will be changed to 1.\n"
    ],
    [ { 'command_timeout' => 60001 },
      { 'command_timeout' => 60000 },
      "Warning: The command timeout is too high so it will be changed to 60000 ms.\n"
    ],
    [ { 'vin_calibration' => -501 },
      { 'vin_calibration' => -500 },
      "Warning: The VIN calibration is too low so it will be raised to -500.\n"
    ],
    [ { 'vin_calibration' => 501 },
      { 'vin_calibration' => 500 },
      "Warning: The VIN calibration is too high so it will be lowered to 500.\n"
    ],
    #[ { 'low_vin_shutoff_voltage' => 9001, 'low_vin_startup_voltage' => 9000,
    #    'high_vin_shutoff_voltage' => 8999 },
    #  { 'low_vin_shutoff_voltage' => 9001, 'low_vin_startup_voltage' => 9501,
    #    'high_vin_shutoff_voltage' => 10001 },
    #    "Warning: The low VIN startup voltage will be changed to 9501 mV.\n" \
    #    "Warning: The high VIN shutoff voltage will be changed to 10001 mV.\n"
    #],
    [ { 'input_min' => 9, 'input_neutral_min' => 8,
        'input_neutral_max' => 7, 'input_max' => 6, },
      { 'input_min' => 0, 'input_neutral_min' => 2015,
        'input_neutral_max' => 2080, 'input_max' => 4095, },
      "Warning: The input scaling values are out of order " \
      "so they will be reset to their default values.\n"
    ],
    [ { 'input_min' => 4096, 'input_neutral_min' => 4097,
        'input_neutral_max' => 4098, 'input_max' => 4099, },
      { 'input_min' => 4095, 'input_neutral_min' => 4095,
        'input_neutral_max' => 4095, 'input_max' => 4095, },
      "Warning: The input minimum is too high so it will be lowered to 4095.\n" \
      "Warning: The input neutral min is too high so it will be lowered to 4095.\n" \
      "Warning: The input neutral max is too high so it will be lowered to 4095.\n" \
      "Warning: The input maximum is too high so it will be lowered to 4095.\n"
    ],
    [ { 'output_min' => 1 },
      { 'output_min' => 0 },
      "Warning: The scaling output minimum is above 0 " \
      "so it will be lowered to 0.\n"
    ],
    [ { 'output_max' => -1 },
      { 'output_max' => 0 },
      "Warning: The scaling output maximum is below 0 " \
      "so it will be raised to 0.\n"
    ],
    [ { 'encoder_prescaler' => 0 },
      { 'encoder_prescaler' => 1 },
      "Warning: The encoder prescaler is zero so it will be changed to 1.\n"
    ],
    [ { 'encoder_prescaler' => 2147483648 },
      { 'encoder_prescaler' => 2147483647 },
      "Warning: The encoder prescaler is too high " \
      "so it will be lowered to 2147483647.\n"
    ],
    [ { 'encoder_postscaler' => 0 },
      { 'encoder_postscaler' => 1 },
      "Warning: The encoder postscaler is zero so it will be changed to 1.\n"
    ],
    [ { 'encoder_postscaler' => 2147483648 },
      { 'encoder_postscaler' => 2147483647 },
      "Warning: The encoder postscaler is too high " \
      "so it will be lowered to 2147483647.\n"
    ],
    [ { 'current_limit' => lowest_current },
      { }
    ],
    [ { 'current_limit' => default_current + 16 },
      { 'current_limit' => default_current },
    ],
    [ { 'current_limit' => max_current + 1 },
      { 'current_limit' => max_current },
      "Warning: The current limit is too high " \
      "so it will be lowered to #{max_current} mA.\n"
    ],
    [ { 'current_limit' => medium_current, 'current_limit_during_error' => low_current },
      { }
    ],
    [ { 'current_limit' => medium_current, 'current_limit_during_error' => high_current },
      { 'current_limit_during_error' => -1 },
      "Warning: The current limit during error is higher than the default " \
      "current limit so it will be changed to be the same.\n"
    ],
    [ { 'current_limit' => medium_current, 'current_limit_during_error' => -2 },
      { 'current_limit_during_error' => -1 },
      "Warning: The current limit during error is an invalid negative number " \
      "so it will be changed to be the same as the default current limit.\n"
    ],
    [ { 'max_speed' => 70000_0000 },
      { 'max_speed' => 50000_0000 },
      "Warning: The maximum speed is too high " \
      "so it will be lowered to 500000000 (50 kHz).\n"
    ],
    [ { 'starting_speed' => 500_0000 },
      { 'starting_speed' => 200_0000 },
      "Warning: The starting speed is greater than the maximum speed " \
      "so it will be lowered to 2000000.\n"
    ],
    [ { 'max_decel' => 0x80000000 },
      { 'max_decel' => 0x7FFFFFFF },
      "Warning: The maximum deceleration is too high " \
      "so it will be lowered to 2147483647.\n"
    ],
    [ { 'max_decel' => 99 },
      { 'max_decel' => 100 },
      "Warning: The maximum deceleration is too low " \
      "so it will be raised to 100.\n"
    ],
    [ { 'max_decel' => 0 },  # 0 means same as max_accel
      { }
    ],
    [ { 'max_accel' => 0x80000000 },
      { 'max_accel' => 0x7FFFFFFF },
      "Warning: The maximum acceleration is too high " \
      "so it will be lowered to 2147483647.\n"
    ],
    [ { 'max_accel' => 99 },
      { 'max_accel' => 100 },
      "Warning: The maximum acceleration is too low " \
      "so it will be raised to 100.\n"
    ],
    [ { 'max_accel' => 0 },
      { 'max_accel' => 100 },
      "Warning: The maximum acceleration is too low " \
      "so it will be raised to 100.\n"
    ],
    [ { 'control_mode' => 'analog_position', 'sda_config' => 'user_io active_high' },
      { 'sda_config' => 'default active_high' },
      "Warning: The SDA pin must be used as an analog input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'control_mode' => 'analog_speed', 'sda_config' => 'user_io pullup' },
      { 'sda_config' => 'default pullup' },
      "Warning: The SDA pin must be used as an analog input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'control_mode' => 'analog_speed', 'sda_config' => 'user_input pullup' },
      { }
    ],
    [ { 'control_mode' => 'rc_position', 'rc_config' => 'kill_switch active_high' },
      { 'rc_config' => 'default active_high' },
      non_default_rc_pin_warning ||
      "Warning: The RC pin must be used as an RC input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'control_mode' => 'rc_speed', 'rc_config' => 'user_io pullup' },
      { 'rc_config' => 'default pullup' },
      non_default_rc_pin_warning ||
      "Warning: The RC pin must be used as an RC input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'control_mode' => 'rc_speed', 'rc_config' => 'rc pullup' },
      non_default_rc_pin_warning ? { 'rc_config' => 'default pullup' } : { },
      non_default_rc_pin_warning || ''
    ],
    [ { 'control_mode' => 'encoder_speed', 'tx_config' => 'kill_switch active_high' },
      { 'tx_config' => 'default active_high' },
      "Warning: The TX pin must be used as an encoder input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'control_mode' => 'encoder_position', 'rx_config' => 'kill_switch pullup',
        'tx_config' => 'encoder analog' },
      { 'rx_config' => 'default pullup' },
      "Warning: The RX pin must be used as an encoder input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'rc_config' => 'user_io active_high' },
      { 'rc_config' => 'default active_high' },
      non_default_rc_pin_warning ||
      "Warning: The RC pin cannot be a user I/O pin " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'sda_config' => 'pot_power' },
      { 'sda_config' => 'default' },
      "Warning: The SDA pin cannot be used as a potentiometer power pin " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'tx_config' => 'pot_power active_high' },
      { 'tx_config' => 'default active_high' },
      "Warning: The TX pin cannot be used as a potentiometer power pin " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'rx_config' => 'pot_power active_high' },
      { 'rx_config' => 'default active_high' },
      "Warning: The RX pin cannot be used as a potentiometer power pin " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'rc_config' => 'pot_power' },
      { 'rc_config' => 'default' },
      non_default_rc_pin_warning ||
      "Warning: The RC pin cannot be used as a potentiometer power pin " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'rc_config' => 'serial pullup active_high' },
      { 'rc_config' => 'default pullup active_high' },
      non_default_rc_pin_warning ||
      "Warning: The RC pin cannot be a serial pin " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'sda_config' => 'rc analog' },
      { 'sda_config' => 'default analog' },
      "Warning: The SDA pin cannot be used as an RC input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'scl_config' => 'rc pullup' },
      { 'scl_config' => 'default pullup' },
      "Warning: The SCL pin cannot be used as an RC input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'tx_config' => 'rc active_high' },
      { 'tx_config' => 'default active_high' },
      "Warning: The TX pin cannot be used as an RC input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'rx_config' => 'rc pullup active_high' },
      { 'rx_config' => 'default pullup active_high' },
      "Warning: The RX pin cannot be used as an RC input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'scl_config' => 'encoder analog' },
      { 'scl_config' => 'default analog' },
      "Warning: The SCL pin cannot be used as an encoder input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'sda_config' => 'encoder active_high' },
      { 'sda_config' => 'default active_high' },
      "Warning: The SDA pin cannot be used as an encoder input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'rc_config' => 'encoder pullup' },
      { 'rc_config' => 'default pullup' },
      non_default_rc_pin_warning ||
      "Warning: The RC pin cannot be used as an encoder input " \
      "so its function will be changed to the default.\n"
    ],
    [ { 'rc_config' => 'user_input analog' },
      { 'rc_config' => non_default_rc_pin_warning ? 'default' : 'user_input' },
      non_default_rc_pin_warning.to_s +
      "Warning: The RC pin cannot be an analog input " \
      "so that feature will be disabled.\n"
    ],
    [ { 'scl_config' => 'user_io analog' },
      { 'scl_config' => 'default analog' },
      "Warning: The SCL pin must be used for I2C if the SDA pin is, " \
      "so the SCL and SDA pin functions will be changed to the default.\n"
    ],
    [ { 'sda_config' => 'user_io pullup' },
      { 'sda_config' => 'default pullup' },
      "Warning: The SDA pin must be used for I2C if the SCL pin is, " \
      "so the SCL and SDA pin functions will be changed to the default.\n"
    ],
    [ { 'control_mode' => 'analog_speed', 'scl_config' => 'user_io' },
      { }
    ],
  ]

  if product == :T500
    cases << [ { 'step_mode' => 16 },
               { 'step_mode' => 1 },
               "Warning: The step mode is invalid " \
               "so it will be changed to 1 (full step).\n"
             ]
  end

  if product == :T825 || product == :N825 || product == :T834
    cases << [ { 'decay_mode' => 'mode4' },
               { 'decay_mode' => case product
                                 when :T834 then 'mixed75'
                                 when :'36v4' then 'slow_mixed'
                                 else 'mixed'
                                 end
               },
             ]
  end

  if product == :T249
    cases << [ { 'step_mode' => '2_100p' }, {}]
  else
    cases << [ { 'step_mode' => '2_100p' },
               { 'step_mode' => 1 },
               "Warning: The step mode is invalid " \
               "so it will be changed to 1 (full step).\n" ]
  end

  if product == :'36v4'
    cases << [
      { 'current_limit' => 10000,
        'hp_enable_unrestricted_current_limits' => true },
      { 'current_limit' => 9095 },
      "Warning: The current limit is too high " \
      "so it will be lowered to 9095 mA.\n"
    ]
  end

  cases
end

describe 'settings' do
  specify 'settings test 1', usb: true do
    # Set the settings to something kind of random.
    stdout, stderr, result = run_ticcmd('--set-settings -',
      input: test_settings1(tic_product))
    expect(stderr).to eq ""
    expect(stdout).to eq ""
    expect(result).to eq 0

    # Read the settings back and make sure they match.
    stdout, stderr, result = run_ticcmd('--get-settings -')
    expect(stderr).to eq ""
    expect(YAML.load(stdout)).to eq YAML.load(test_settings1(tic_product))
    expect(result). to eq 0

    # Restore the device to its default settings.
    stdout, stderr, result = run_ticcmd('--restore-defaults')
    expect(stdout).to eq ""
    expect(stderr).to eq ""
    expect(result).to eq 0

    # Check that the settings on the device match what we expect the default
    # settings to be, except for serial_enable_alt_serial_number.
    # (See comment in tic_settings_fill_with_defaults about why it is special.)
    stdout, stderr, result = run_ticcmd('--get-settings -')
    expect(stderr).to eq ""
    defaults_from_device = YAML.load(stdout)
    defaults = YAML.load(default_settings(tic_product))
    expect(defaults_from_device).to eq defaults
    expect(result).to eq 0
  end

  TicProductSymbols.each do |product|
    specify "tic_settings_fill_with_defaults is correct for #{product}" do
      stdin = "product: #{product}"
      stdout, stderr, result = run_ticcmd('--fix-settings - -', input: stdin)
      expect(stderr).to eq ""
      fixed = YAML.load(stdout)
      defaults = YAML.load(default_settings(product))
      expect(fixed).to eq defaults
      expect(result).to eq 0
    end

    specify "tic_settings_fix is correct for #{product}" do
      defaults = YAML.load(default_settings(product))
      test_cases_for_settings_fix(product).each do |input_part, output_part, warnings|
        warnings ||= ""
        if !warnings.empty? && !warnings.end_with?("\n")
          raise "Warnings should end with newline: #{warnings.inspect}"
        end
        #p input_part
        input = defaults.merge(input_part)
        output = input.merge(output_part)

        stdout, stderr, result = run_ticcmd('--fix-settings - -',
          input: YAML.dump(input))
        expect(stderr).to eq warnings
        expect(YAML.load(stdout)).to eq output
        expect(result).to eq 0
      end
    end
  end

  describe 'integer processor' do
    let (:head) { "product: T825\n" }

    it "rejects numbers that are too big" do
      stdout, stderr, result = run_ticcmd('--fix-settings - -',
        input: "#{head}serial_baud_rate: 1111111111111111111")
      expect(stderr).to eq \
        "Error: There was an error reading the settings file.  " \
        "The serial_baud_rate value is out of range.\n"
      expect(result).to eq 2
    end

    it "rejects numbers that are too small" do
      stdout, stderr, result = run_ticcmd('--fix-settings - -',
        input: "#{head}serial_baud_rate: -1111111111111111111")
      expect(stderr).to eq \
        "Error: There was an error reading the settings file.  " \
        "The serial_baud_rate value is out of range.\n"
      expect(result).to eq 2
    end

    it "rejects numbers that are too small" do
      stdout, stderr, result = run_ticcmd('--fix-settings - -',
        input: "#{head}serial_baud_rate: -1111111111111111111")
      expect(stderr).to eq \
        "Error: There was an error reading the settings file.  " \
        "The serial_baud_rate value is out of range.\n"
      expect(result).to eq 2
    end

    it "rejects numbers with junk at the end" do
      stdout, stderr, result = run_ticcmd('--fix-settings - -',
        input: "#{head}serial_baud_rate: -11a")
      expect(stderr).to eq \
        "Error: There was an error reading the settings file.  " \
        "Invalid serial_baud_rate value.\n"
      expect(result).to eq 2
    end
  end
end
