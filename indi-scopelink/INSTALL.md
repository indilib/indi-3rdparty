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

Three CMake modules have to be put in place first, and all three belong to indi-3rdparty rather than to
this driver. `libindi-dev` installs `libindi.pc` but no CMake package configuration, so
`find_package(INDI)` needs `FindINDI.cmake`; `CMakeCommon.cmake` carries the repository's shared compiler
settings and is included unconditionally, because the driver is built with those settings rather than
with CMake's own defaults; and `CMakeCommon.cmake` in turn includes `UnityBuild.cmake`. Put copies where
`CMAKE_MODULE_PATH` already looks — either `cmake_modules/` here, or `../cmake_modules/` alongside this
directory, which is the layout an indi-3rdparty checkout has:

```sh
mkdir -p ../cmake_modules
for module in FindINDI CMakeCommon UnityBuild; do
  curl -fsSLo "../cmake_modules/$module.cmake" \
    "https://raw.githubusercontent.com/indilib/indi-3rdparty/master/cmake_modules/$module.cmake"
done
```

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

It also names the device. Astrolabs has been assigned two USB product identifiers under ST's vendor
identifier — `0483:a5d4` for generation 4 and onwards, `0483:a5d3` for generation 3 and below — and those
matches are exact, so a controller carrying them appears as `/dev/scopelink` as well as `/dev/ttyACMn`.
The firmware sets its own manufacturer and product strings alongside them, so the by-id name reads:

```sh
ls -l /dev/serial/by-id/
# usb-Astrolabs_ScopeLink_205E35793630-if00 -> ../../ttyACM0
```

Point a profile at `/dev/scopelink` for one controller and at the by-id name for a particular one: two
plugged in at once both claim `/dev/scopelink` and one of them wins.

**A controller flashed before the identifiers were assigned reports ST's generic pair, `0483:5740`**,
which every STM32 device running the stock USB CDC stack also reports — Nucleo boards, home-built
focusers, other vendors' controllers — and sets no product string either. The rule grants such a unit the
same access rights and deliberately gives it no name, since a `/dev/scopelink` claimed on those
identifiers would be claimed by all of them just as readily. Its stable per-unit name is the one systemd
derives from the USB serial number:

```sh
ls -l /dev/serial/by-id/
# usb-STMicroelectronics_STM32_Virtual_ComPort_in_FS_Mode_205E35793630-if00 -> ../../ttyACM0
```

macOS has no udev, so no rule is installed there and none is needed: the controller appears as
`/dev/cu.usbmodem<serial>`, which already carries the same serial number and is openable by the logged-in
user.

Every one of those names identifies one physical unit, so a saved Ekos profile keeps working across
reboots and however many other USB devices are plugged in, which `/dev/ttyACMn` does not.

**The number in that name is not the unit identifier the driver reports.** The driver's `Unit` field is
the controller's full 96 bit identifier, for example `2B002E000D4330364E353020`. The USB serial is what
ST's USB stack derives from that same identifier, and it is shorter: taking the identifier as three
little endian 32 bit words, the serial is the sum of the first and third as eight hex digits, followed by
the top half of the second as four. The unit above therefore appears as `..._ScopeLink_205E35793630-if00`,
and appeared as `..._FS_Mode_205E35793630-if00` before the identifiers were assigned: the strings around
the number are the firmware's and changed with them, the number itself did not.

To go from a device node back to a unit, ask the controller rather than doing the arithmetic:

```sh
scopelink-cli --port /dev/scopelink info
```

## Checking it works

Without hardware, using the driver's own simulation mode — turn **Simulation** on before connecting, and
pick the controller to imitate under **Simulated hardware** on the Options tab. There are four, for three
generations: a generation 3 board runs interface 1.0 and 1.1, and 1.1 lays its frames out differently, so
it is an entry of its own. Generation 4 exists only on 1.1, so it needs no second entry.

```sh
indiserver -v indi_scopelink
indi_setprop 'ScopeLink.SIMULATION.ENABLE=On'
indi_setprop 'ScopeLink.SIMULATED_GENERATION.GENERATION_3=On'
# ...or GENERATION_2, GENERATION_3_IF11 for a generation 3 controller on interface 1.1, or GENERATION_4
indi_setprop 'ScopeLink.CONNECTION.CONNECT=On'
```

The same simulated controller is also served on a pseudo terminal by `scopelink-simulator`, which is what
to use when the thing being tested is a port rather than the driver — `scopelink-cli`, a client's port
handling, or the driver's serial path itself:

```sh
scopelink-simulator --generation 3
# ScopeLink simulator, hardware generation 3 on interface 1.0
# Port: /dev/pts/7

scopelink-cli --port /dev/pts/7 info
```

`--interface-minor 1` serves the same board reflashed to interface 1.1, whose status frame is 57 bytes
rather than 60 and whose freeze frame is 44 rather than 48:

```sh
scopelink-simulator --generation 3 --interface-minor 1
```

`--generation 4` serves the three motor board with the six port hub. It comes up on interface 1.1 without
being asked, because there is no 1.0 firmware for it — asking for that pair is refused rather than served.
Its status frame is 65 bytes and its freeze frame is 48, which is the same length a generation 3 unit on
1.0 stores and a different set of fields inside it:

```sh
scopelink-simulator --generation 4
```

With hardware:

```sh
scopelink-cli --port /dev/ttyACM0 info
```
