# Tic Stepper Motor Controller software

[www.pololu.com](https://www.pololu.com/)

This repository contains the source code of the configuration and control software for
the [Tic Stepper Motor Controller](https://www.pololu.com/tic).
There are two programs: the Pololu Tic Command-line Utility
(ticcmd) and the Pololu Tic Control Center (ticgui).

## Installation

Installers for this software are available for download from the
[Tic Stepper Motor Controller User's Guide](https://www.pololu.com/docs/0J71).

See [BUILDING.md](BUILDING.md) for information about how to compile the software
from source.

## Version history

- 1.8.3 (2025-05-01):
  - Fixed a bug in tic_get_variables that prevented it from fetching the
    Tic T249-specific variables, including 'Last motor driver error' and
    the four AGC variables.  Those variables were always reported as 0.
  - Made a small change to the bootloader GUI code that should generally
    make the progress bar update more reliably, and seems to fix the
    timeout issue that has been happening during firmware updates in
    later versions of macOS.
- 1.8.2 (2020-11-16):
  - Started using a newer version of libusbp so that the Windows version of
    this software might work with Tic devices that are connected through
    certain special USB hubs (no actual code changes in this repository).
- 1.8.1 (2020-06-03):
  - Fixed a bug introduced in version 1.8.0 that prevented the `tic_get_settings`
    and `tic_set_settings` functions from properly loading or storing the
    AGC settings for the Tic T249.
- 1.8.0 (2019-09-16):
  - Added support for the Tic 36v4.
- 1.7.0 (2019-01-31):
  - Added support for the Tic T249 and Tic N825.
  - Added support for the new features in firmware version 1.06: limit switches,
    the "Go home" command, automatic homing, 14-bit serial device numbers,
    alternative serial device numbers,
    CRC for serial responses, and 7-bit serial responses.
  - Added the `--position-relative` option to `ticcmd`.
- 1.6.2 (2018-03-14):
  - Call `SetProcessDPIAware()` to fix issues on Windows caused by dragging the
    application between monitors with different DPIs.
- 1.6.1 (2018-03-13):
  - Fix a bug in the controls for setting current limits that could be triggered
    by pressing Enter.
- 1.6.0 (2018-03-13):
  - Added support for the Tic T500.
  - Improved the controls for setting the current limits: the up and down arrows
    only step to current limits that are exactly achievable.
- 1.5.0 (2017-11-22):
  - Added support for building installers for macOS.
  - Fixed some bugs affecting the software on macOS (caused by `strtoll`).
  - Made Qt's Fusion style be the default style on non-Windows systems.
  - Fixed some styling issues.
  - Fixed the firmware upgrade code to work with the Tic T834.
- 1.4.0 (2017-11-02):
  - Added support for the Tic T834.
  - ticgui: To avoid unexpected motion when connecting to a Tic with safe start
    is disabled, only start sending the "Reset command timeout" to the Tic after
    the user clicks the Resume button or specifies a target position or
    velocity.
  - ticgui: Added a menu item for sending the "Clear driver error" command.
  - Show all the digits of the VIN voltage reading in `ticcmd --status --full`.
  - libpololu-tic: Added
    `tic_look_up_product_name_short()`,
    `tic_look_up_product_name_ui()`,
    `tic_look_up_decay_mode_name()`,
    `tic_look_up_decay_mode_code()`,
    `tic_get_max_allowed_current()`,
    `tic::device.get_product()`.
  - Fixed a bug that prevented reading or writing encoder prescaler values greater than 255.
  - Other minor bug fixes.
- 1.3.0 (2017-09-11):
  - Added support for building installers for Linux.
  - ticgui: Added the compact layout.  Can be enabled with `TICGUI_COMPACT=Y`.
  - ticgui: Center the window at startup.  Can be disabled with `TICGUI_CENTER=N`.
- 1.2.0 (2017-08-28):
  - ticgui: Added support for the "Serial response delay" setting.
  - ticgui: Fixed how error messages at program startup are handled.  They are now shown after the window is displayed, which fixes a bug that caused the windows to start in the upper left corner with its title bar off the screen.
  - ticgui: Improved the pin configuration interface by hiding or disabling checkboxes appropriately, and labeling pins that are permanently pulled up or down.
  - ticgui: Fixed the lower bound for "Encoder prescaler" and "Encoder postscaler" inputs.
  - ticcmd: Added `--pause` and `--pause-on-error`.
  - libpololu-tic: Added `tic_device_get_product()`.
  - Renamed the "Kill switch" error bit to "Kill switch active" everywhere.
  - CMake: install libpololu-tic, its headers, and a .pc file to the system.
  - Nix: Added support for 32-bit Linux and Raspberry Pi.
  - Fixed some compilation issues for macOS and GCC 4.9.
- 1.1.0 (2017-08-02):
  - ticgui: Added support for firmware upgrades.
  - ticgui: Improved some of the text.
  - ticcmd: Allowed '#' at the beginning of the serial number argument.
  - libpololu-tic: Added `tic_start_bootloader()`.
  - libpololu-tic and ticcmd: Added support for the "Serial response delay" setting.
- 1.0.0 (2017-07-18): Original release.
