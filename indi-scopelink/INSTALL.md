# Installing indi-scopelink

## From a package

Once the driver is part of an INDI release:

```sh
sudo apt install indi-scopelink
```

## Building as part of indi-3rdparty

This directory sits in a checkout of [indi-3rdparty](https://github.com/indilib/indi-3rdparty) as
`indi-scopelink/`, registered in that repository's top-level `CMakeLists.txt` with:

```cmake
option(WITH_SCOPELINK "Install ScopeLink Driver" On)

## Astrolabs ScopeLink
if(WITH_SCOPELINK)
  add_subdirectory(indi-scopelink)
endif(WITH_SCOPELINK)
```

Then build it the way the repository documents:

```sh
sudo apt install build-essential cmake pkg-config libindi-dev
mkdir -p build/indi-scopelink
cd build/indi-scopelink
cmake -DCMAKE_INSTALL_PREFIX=/usr . ../../indi-3rdparty/indi-scopelink
make -j$(nproc)
sudo make install
```

`FindINDI.cmake` and `CMakeCommon.cmake` come from the checkout's own `cmake_modules/`, which is why
neither is duplicated here.

## Building on its own

The driver also builds outside an indi-3rdparty checkout, which is how it is developed:

```sh
sudo apt install build-essential cmake pkg-config libindi-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

One thing has to be put in place first. `libindi-dev` installs `libindi.pc` but no CMake package
configuration, so `find_package(INDI)` needs `FindINDI.cmake`, and that module belongs to indi-3rdparty
rather than to this driver. Put a copy where `CMAKE_MODULE_PATH` already looks — either `cmake_modules/`
here, or `../cmake_modules/` alongside this directory, which is the layout an indi-3rdparty checkout has:

```sh
mkdir -p ../cmake_modules
curl -fsSLo ../cmake_modules/FindINDI.cmake \
  https://raw.githubusercontent.com/indilib/indi-3rdparty/master/cmake_modules/FindINDI.cmake
```

`CMakeCommon.cmake` is included only if it is found, so a standalone build without it works; it just
compiles with CMake's own defaults instead of the repository's shared flags.

The unit tests are built only in a standalone build, because `add_test` belongs to whichever project is
at the top of the tree.

## Requirements

- libindi 2.0.0 or later. The driver uses the `INDI::Property` API throughout, which earlier releases do
  not have. Built and tested against 2.2.2.
- A C++17 compiler.
- No other dependencies.

## After installing

On Linux, reload the udev rules so that the controller is reachable without root:

```sh
sudo udevadm control --reload-rules && sudo udevadm trigger
```

That lets the user at the console open the controller without being added to the `dialout` group.

The rule creates no device name of its own. ScopeLink uses ST's generic USB identifiers, `0483:5740`,
which every STM32 device running the stock USB CDC stack also reports — Nucleo boards, home-built
focusers, other vendors' controllers — and the firmware sets no product string either, so a rule
claiming `/dev/scopelink` would claim it for all of them just as readily. A product identifier of our own
has been requested from STMicroelectronics; once it is assigned the rule can name the device exactly.

The stable per-unit name to use in the meantime is the one systemd derives from the USB serial number:

```sh
ls -l /dev/serial/by-id/
# usb-STMicroelectronics_STM32_Virtual_ComPort_in_FS_Mode_205E35793630-if00 -> ../../ttyACM0
```

macOS has no udev, so no rule is installed there and none is needed: the controller appears as
`/dev/cu.usbmodem<serial>`, which already carries the same serial number and is openable by the logged-in
user.

It identifies one physical unit, so a saved Ekos profile keeps working across reboots and however many
other USB devices are plugged in, which `/dev/ttyACMn` does not.

**The number in that name is not the unit identifier the driver reports.** The driver's `Unit` field is
the controller's full 96 bit identifier, for example `2B002E000D4330364E353020`. The USB serial is what
ST's USB stack derives from that same identifier, and it is shorter: taking the identifier as three
little endian 32 bit words, the serial is the sum of the first and third as eight hex digits, followed by
the top half of the second as four. The unit above therefore appears as `..._FS_Mode_205E35793630-if00`.

To go from a device node back to a unit, ask the controller rather than doing the arithmetic:

```sh
scopelink-cli --port /dev/serial/by-id/usb-STMicroelectronics_STM32_Virtual_ComPort_in_FS_Mode_205E35793630-if00 info
```

## Checking it works

Without hardware, using the driver's own simulation mode — turn **Simulation** on before connecting, and
pick the generation to imitate under **Simulated hardware** on the Options tab:

```sh
indiserver -v indi_scopelink
indi_setprop 'ScopeLink.SIMULATION.ENABLE=On'
indi_setprop 'ScopeLink.SIMULATED_GENERATION.GENERATION_3=On'
indi_setprop 'ScopeLink.CONNECTION.CONNECT=On'
```

The same simulated controller is also served on a pseudo terminal by `scopelink-simulator`, which is what
to use when the thing being tested is a port rather than the driver — `scopelink-cli`, a client's port
handling, or the driver's serial path itself:

```sh
scopelink-simulator --generation 3
# ScopeLink simulator, hardware generation 3
# Port: /dev/pts/7

scopelink-cli --port /dev/pts/7 info
```

With hardware:

```sh
scopelink-cli --port /dev/ttyACM0 info
```
