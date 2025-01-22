# INDI GPIO

Uses libgpiod to access GPIOs lines. It checks whether the lines are INPUT or OUTPUT and presents each type in their own tab in the driver.

Analog lines are not supported.

The lines kernel names must start with GPIO to be added, any other lines are ignored. Other lines might be supported in the future but adding ALL lines can cause system freeze and lockup since libgpiod get function can change the direction of a system-critical line and cause the system to freeze.

# Naming Convention

## Inputs

Inputs are named DIGITAL_INPUT_N where N starts from 1 to N, the maximum line count. When snooping, use this and NOT the property label.

## Outputs

Outputs are named DIGITAL_OUTPUT_N where N starts from 1 to N, the maximum line count.

Currently outputs are NOT READ from the board due to a limitation in libgpiod. Therefore, all outputs start with an unknown state until you toggle them in the driver. If you can figure out a safe way to read the output status without changing line direction, please submit a PR.

### Pulse Mode

Normally, output are toggled ON or OFF. You can assign a specific pulse in milliseconds to each output GPIO. If a pulse duration is set, the pin would be held high for this many milliseconds then toggled off.

Pulse mode can be utilized to operate **Garage Door Openers**, especially in conjunction with INDI Universal Rolloff Roof driver.

# Labels

Each input and output label may be changed. Next time the driver runs, the new label shall be loaded up and used.

# Applicable boards

Tested on Raspberry PI and Orange PI, but should work under any board support by libgpiod. Make sure you have proper permissions and that the user running the driver is part of the gpio group (at least on Raspberry PI).

# PWM

For Raspberry PI, enable PWM in /boot/firmware/config.txt:
```
dtparam=audio=off
dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4
```

Audio=off is very important otherwise the PWM functionality may not work. This should enable GPIO 12 and 13 hardware PWM.

Before using the PWM, you must map the GPIOs to the PWM channels. After changing the mapping, the device should be restarted.

## Raspberry PI 5

For PWM GPIO12 and GPIO13, they are mapping to pwmchip2 which has 4 PWM channels. Therefore, map pwm0 to 12 and pmw1 to 13 and then restart the board.

Before using the driver

# TODO

Need to adopt libgpiod 2.x+ but it is still not in widespread use in most distributions.
