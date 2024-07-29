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

# Labels

Each input and output label may be changed. Next time the driver runs, the new label shall be loaded up and used.

# Applicable boards

Tested on Raspberry PI and Orange PI, but should work under any board support by libgpiod. Make sure you have proper permissions and that the user running the driver is part of the gpio group (at least on Raspberry PI).

# TODO

Need to adopt libgpiod 2.x+ but it is still not in widespread use in most distributions.
