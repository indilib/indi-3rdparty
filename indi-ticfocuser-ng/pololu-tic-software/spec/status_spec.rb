
require_relative 'spec_helper'

FakeStatus = <<END
Name:                         Fake name
Serial number:                123
Firmware version:             9.99
Last reset:                   Stack underflow
Up time:                      1:02:05

Encoder position:             -801
RC pulse width:               18660
Input state:                  Position
Input after averaging:        0
Input after hysteresis:       0
Input after scaling:          0
Forward limit active:         No
Reverse limit active:         Yes

VIN voltage:                  7.005 V
Operation state:              Normal
Energized:                    Yes
Homing active:                No

Target position:              1234
Current position:             500
Position uncertain:           Yes
Current velocity:             951145
Max speed:                    2023714
Starting speed:               1000
Max acceleration:             34567
Max deceleration:             23456
Acting target position:       1334
Time since last step:         90
Step mode:                    1/2 step
Current limit:                3968 mA

Errors currently stopping the motor:
  - Intentionally de-energized
  - Motor driver error
  - Low VIN
  - Kill switch active
  - Required input invalid
  - Serial error
  - Command timeout
  - Safe start violation
  - ERR line high
  - (Unknown)
Errors that occurred since last check:
  - Intentionally de-energized
  - Motor driver error
  - Low VIN
  - Kill switch active
  - Required input invalid
  - Serial error
  - Command timeout
  - Safe start violation
  - ERR line high
  - (Unknown)
  - Serial framing
  - Serial RX overrun
  - Serial format
  - Serial CRC
  - Encoder skip
  - (Unknown)

SCL pin:
  State:                      High impedance
  Analog reading:             100
  Digital reading:            0
SDA pin:
  State:                      Output high
  Analog reading:             900
  Digital reading:            1
TX pin:
  State:                      Output low
  Analog reading:             200
  Digital reading:            0
RX pin:
  State:                      Pulled up
  Analog reading:             800
  Digital reading:            1
RC pin:
  Digital reading:            0

END

describe '--status' do
  it 'can get the status', usb: true do
    stdout, stderr, result = run_ticcmd('--status')
    expect(stderr).to eq ''
    expect(result).to eq 0
    expect(stdout).to_not include '(Unknown)'
    expect(stdout).to_not include '_'
    status = YAML.load(stdout)
  end

  it 'can get the full status', usb: true do
    stdout, stderr, result = run_ticcmd('--status --full')
    expect(stderr).to eq ''
    expect(result).to eq 0
    expect(stdout).to_not include '(Unknown)'
    expect(stdout).to_not include '_'
    status = YAML.load(stdout)

    # These keys are not always in the output
    unreliable_keys = [
      "Target", "Target velocity", "Target position",
      "Input before scaling",
    ]

    actual_keys = status.keys.sort
    expected_keys = YAML.load(FakeStatus).keys.sort
    if %i(T825 N825 T834).include?(tic_product)
      expected_keys << "Decay mode"
    end
    if tic_product == :T249
      expected_keys << 'AGC bottom current limit'
      expected_keys << 'AGC current boost steps'
      expected_keys << 'AGC frequency limit'
      expected_keys << 'AGC mode'
      expected_keys << 'Last motor driver error'
    end
    if tic_product == :'36v4'
      expected_keys << 'Last motor driver errors'
    end

    unexpected_keys = actual_keys - expected_keys - unreliable_keys
    expect(unexpected_keys).to eq []

    missing_keys = expected_keys - actual_keys - unreliable_keys
    expect(missing_keys).to eq []
  end
end

describe 'print_status' do
  it 'can print a fake status for testing' do
    stdout, stderr, result = run_ticcmd('-d x --test 1')
    expect(stderr).to eq ''
    expect(result).to eq 0
    YAML.load(stdout)
    expect(stdout).to eq FakeStatus
  end
end
