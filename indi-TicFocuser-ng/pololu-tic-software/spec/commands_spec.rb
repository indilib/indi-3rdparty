require_relative 'spec_helper'

describe 'commands for controlling the motor', usb: true do
  before(:all) do
    tic_change_settings do |s|
      s['control_mode'] = 'serial'
      s['max_speed'] = '500000000'
      s['command_timeout'] = '0'
      s['disable_safe_start'] = false
    end

    # Get out of safe-start.
    stdout, stderr, result = run_ticcmd('--energize --exit-safe-start')
    expect(stderr).to eq ''
    expect(stdout).to eq ''
    expect(result).to eq 0
  end

  describe 'Set target position' do
    it 'lets you set the position' do
      stdout, stderr, result = run_ticcmd('-p 230000')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      expect(tic_get_status['Target position']).to eq 230000

      stdout, stderr, result = run_ticcmd('--position -44')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      expect(tic_get_status['Target position']).to eq -44
    end
  end

  describe 'Set target velocity' do
    it 'lets you set the velocity' do
      stdout, stderr, result = run_ticcmd('-y 100000')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      expect(tic_get_status['Target velocity']).to eq 100000

      stderr, stdout, result = run_ticcmd('--velocity -1')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      expect(tic_get_status['Target velocity']).to eq -1
    end
  end

  describe 'Halt and set position' do
    it 'lets you set the current position' do
      stdout, stderr, result = run_ticcmd('--halt-and-set-position 2146054486')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      expect(tic_get_status['Current position']).to eq 2146054486

      stderr, stdout, result = run_ticcmd('--halt-and-set-position 10')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      expect(tic_get_status['Current position']).to eq 10
    end
  end

  describe 'Halt and hold' do
    it 'lets you clear a target position or velocity' do
      stdout, stderr, result = run_ticcmd('-p 230000')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      expect(tic_get_status['Target position']).to eq 230000

      stdout, stderr, result = run_ticcmd('--halt-and-hold')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      # Ruby converts 'off' to false
      expect(tic_get_status['Target']).to eq 'No target'
    end
  end

  describe 'Reset command timeout' do
    it 'runs' do
      stdout, stderr, result = run_ticcmd('--reset-command-timeout')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
    end
  end

  describe 'De-energize/energize' do
    it 'works' do
      stdout, stderr, result = run_ticcmd('--deenergize')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      errors = tic_get_status['Errors currently stopping the motor']
      expect(errors).to be_include 'Intentionally de-energized'

      stdout, stderr, result = run_ticcmd('--energize')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0

      errors = tic_get_status['Errors currently stopping the motor']
      expect(errors).to_not be_include 'Intentionally de-energized'
    end
  end

  describe 'Exit safe start' do
    it 'runs' do
      stdout, stderr, result = run_ticcmd('--exit-safe-start')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
    end
  end

  describe 'Enter safe start' do
    it 'runs' do
      stdout, stderr, result = run_ticcmd('--enter-safe-start')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
    end
  end

  describe 'Reset' do
    it 'runs' do
      stdout, stderr, result = run_ticcmd('--reset')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
    end
  end

  describe 'Clear driver error' do
    it 'runs' do
      stdout, stderr, result = run_ticcmd('--clear-driver-error')
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
    end
  end
end

describe 'Set max speed' do
  it 'works', usb: true do
    ['10000', '2000000']. each do |mode|
      stdout, stderr, result = run_ticcmd("--max-speed #{mode}")
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
      expect(tic_get_status['Max speed'].to_s).to eq mode
    end
  end
end

describe 'Set starting speed' do
  it 'works', usb: true do
    ['512', '0']. each do |mode|
      stdout, stderr, result = run_ticcmd("--starting-speed #{mode}")
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
      expect(tic_get_status['Starting speed'].to_s).to eq mode
    end
  end
end

describe 'Set max acceleration' do
  it 'works', usb: true do
    ['10000', '2000000']. each do |mode|
      stdout, stderr, result = run_ticcmd("--max-accel #{mode}")
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
      expect(tic_get_status['Max acceleration'].to_s).to eq mode
    end
  end
end

describe 'Set max deceleration' do
  it 'works', usb: true do
    ['10000', '2000000']. each do |mode|
      stdout, stderr, result = run_ticcmd("--max-decel #{mode}")
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
      expect(tic_get_status['Max deceleration'].to_s).to eq mode
    end
  end
end

describe 'Set step mode' do
  it 'complains if the step mode is invalid' do
    stdout, stderr, result = run_ticcmd('-d x --step-mode foobar')
    expect(stderr).to eq "Error: The step mode specified is invalid.\n"
    expect(stdout).to eq ''
    expect(result).to eq EXIT_BAD_ARGS
  end

  it 'works', usb: true do
    modes = ['Full step', '1/2 step', '1/32 step']
    if tic_product == :T500
      modes = ['Full step', '1/2 step', '1/8 step']
    end
    modes.each do |mode|
      stdout, stderr, result = run_ticcmd("--step-mode \"#{mode}\"")
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
      expect(tic_get_status['Step mode'].to_s).to eq mode
    end
  end
end


describe 'Set current limit command' do
  it 'works', usb: true do
    # Note: This test can fail if the "current limit during error" setting is used.
    currents = [32, 64]
    if tic_product == :T500
      currents = [1, 174]
    end
    if tic_product == :T249
      currents = [40, 80]
    end
    if tic_product == :'36v4'
      currents = [72, 143]
    end
    currents.each do |limit|
      stdout, stderr, result = run_ticcmd("--current #{limit}")
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
      expect(tic_get_status['Current limit']).to eq "#{limit} mA"
    end
  end
end

describe 'Set decay mode' do
  it 'complains if the decay mode is invalid' do
    stdout, stderr, result = run_ticcmd('-d x --decay foobar')
    expect(stderr).to eq "Error: The decay mode specified is invalid.\n"
    expect(stdout).to eq ''
    expect(result).to eq EXIT_BAD_ARGS
  end

  it 'works', usb: true do
    case tic_product
    when :T825, :N825
      decay_modes = {
        'mixed' => 'Mixed',
        'fast' => 'Fast',
        'slow' => 'Slow',
      }
    when :T834
      decay_modes = {
        'mixed' => 'Mixed 50%',
        'mixed50' => 'Mixed 50%',
        'fast' => 'Fast',
        'slow' => 'Slow',
        'mixed25' => 'Mixed 25%',
        'mixed75' => 'Mixed 75%',
      }
    when :T500, :T249
      # The board only has one decay mode so skip this test.
      next
    when :'36v4'
      # Cannot set the decay mode at runtime.
      next
    end

    decay_modes.each do |cli_name, ui_name|
      stdout, stderr, result = run_ticcmd("--decay #{cli_name}")
      expect(stderr).to eq ''
      expect(stdout).to eq ''
      expect(result).to eq 0
      expect(tic_get_status['Decay mode']).to eq ui_name
    end
  end
end


