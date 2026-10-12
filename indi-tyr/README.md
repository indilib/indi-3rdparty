# Prometheus Astro Tyr INDI driver

Licensed under GPL-2.0-or-later. See `COPYING` and `THIRD_PARTY.md`.
The end-user driver guide is [doc/index.md](doc/index.md).

This directory contains the native Linux INDI telescope driver for Tyr. It is a
dedicated driver rather than a renamed generic OnStep driver: unsupported focuser
and PEC controls are absent, and Tyr Home inputs and compensation are exposed as
Tyr properties.

Version 0.3 adds TCP/IP alongside USB and mount configuration to the standard telescope
surface required for initial Ekos integration: coordinates, GoTo, Sync, abort,
manual movement, tracking modes, pulse guiding, pier side, Home, Park, site/time,
compensation and Home-input assignment. Simulation mode permits UI and state
testing without moving a mount.

Build on Ubuntu with INDI development headers and `nlohmann-json3-dev` installed:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The test suite starts the driver in simulation and against a pseudo-serial Tyr
controller. The protocol test verifies command
framing, reply consumption, Sync, compensation, precise shared slew speed,
manual movement and pulse guiding without opening real hardware.
The emulator delays its startup beyond one command timeout and reports the
legacy `On-Step` identifier alongside the official `Prometheus Astro Týr` USB
identity. Tests use temporary configuration directories to protect real ports.

Run in simulation without installing system-wide:

```bash
indiserver -v ./build/indi_tyr_telescope
```

Select Simulation in the driver's Options tab before connecting. Physical tests
require exclusive access to Tyr's CP2102 serial device and an attended safe rig.
Opening a CP2102 connection can reset the controller and trigger its configured
automatic Home routine. The driver releases DTR/RTS and allows up to 30 seconds
on the same connection for firmware startup. Identity comes from `TYV`, since
the legacy `GVP` response remains `On-Step` on Tyr firmware.

Tyr stores the opposite of the site's standard UTC offset. When an INDI client
supplies UTC, the driver reads that saved mount offset and writes the matching
local standard date and time. Daylight-saving time is therefore not added to the
mount setting.

The Site Management page shows **Client offset (includes DST)** separately from
**Tyr UTC minus STANDARD time (no DST)**. In Athens during summer these are +3
and -2 respectively. The client field follows INDI's convention; it must not be
changed to -2. The mount field is read from Tyr and remains -2 year-round. UTC
updates use that saved mount value to calculate local standard date/time,
including date rollover and fractional-hour zones, without changing the offset.

## Configuration in Ekos / INDI Control Panel

After connecting, the driver reads configuration from Tyr. The **Read mount
settings** buttons refresh all settings or a selected section. Changes made by
another client require a refresh. The driver reads current axis settings before
merging a partial edit, so untouched motor fields are preserved.

| Tab | Configuration |
| --- | --- |
| Axes | Saved RA/Dec steps per degree, direction, position limits, six driver parameters, firmware-default/saved-settings selection, geometry |
| Limits | Horizon/overhead, east/west meridian limits in degrees, RA/Dec backlash in arcseconds |
| Home | RA/Dec switch offsets in arcseconds, Auto-Home on boot; input assignment remains in the existing Tyr controls |
| Meridian | Automatic flip, pause at Home, buzzer, preferred pier side |
| Tracking | RA/Dec tracking offsets; tracking rates, compensation mode and axes, and shared precise slew speed remain available |
| Site | Mount standard-time offset, using UTC minus local standard time; never include DST |
| Alignment | Manual alignment star count; Start, Accept, Save and Continue Flip actions in the Tyr tab |
| Bluetooth | Enable/disable, scan, forget and select Magicsee R1 by MAC address/type; connection state and scanned devices |
| Network | AP mode and channel, fallback, three station profiles, DHCP/static addresses, AP/station/admin passwords |
| Tyr | Configuration refresh, restart, driver type, alignment status, uptime/reset information and command error |

Settings use Tyr's framed `TYC` management commands, including explicit firmware
errors for legacy commands that return no serial reply. Invalid numeric requests
are rejected before any requested field is written. Some multi-field non-network
writes can still partially succeed if the firmware rejects a later command;
the driver reads them back and marks the operation Alert. Network saves use
`nbegin` / field writes / `ncommit`, and abort on failure.

Stop motion and guiding before changing limits/backlash, and stop tracking before
changing motor, Home or network settings. These checks reject the request rather
than stopping equipment automatically. Motor, geometry and network changes need
a controller restart to take effect. Home-offset edits invalidate the mount's
Home reference; follow the normal Home procedure afterward. Starting alignment
can move the mount under firmware control. Restart is an explicit button action
and disconnects the driver.

Wi-Fi edits are **pending until Apply**. Blank password fields retain saved
passwords; the password controls and driver logs never echo password values.
Text fields follow the firmware's 31-byte limit. Read Network discards pending
network edits, including pending passwords. Unsupported Bluetooth/network
sections are hidden according to controller capabilities. No configuration is
automatically written on connection.

Install the Ubuntu amd64 package with `sudo apt install ./indi-tyr_0.3.0_amd64.deb`,
then restart Ekos's INDI server. This build targets INDI 2.2.5 on Ubuntu 26.04;
other distributions/architectures should build from source.

For network connections, choose **Network** in the Connection tab, enter the
mount's IP address/hostname and TCP command port (normally 9999), then connect.
Use TCP rather than UDP. Network management requires Tyr firmware **v1.2.10 or
later**; earlier firmware exposes these commands only on USB. Port 9999 is the
mount command service, separate from its web page and the local ASCOM Hub.
USB remains available with the same saved serial settings. The polling interval
should stay below the firmware's ten-second TCP idle timeout.

The Linux Control Center UI, object-catalog editing and firmware
updates remain separate work. This driver does not implement automated plate
solving or a pointing-model builder; manual alignment controls operate the
firmware's existing alignment sequence.
