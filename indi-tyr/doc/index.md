# Prometheus Astro Tyr

Tyr is a harmonic-drive telescope mount manufactured by
[Prometheus Astro](https://prometheusastro.com/). This dedicated INDI telescope
driver supports USB serial and TCP/IP connections. Select **Prometheus Astro
Tyr** in the telescope section of your Ekos equipment profile.

## Supported features

- Equatorial coordinates, GoTo, Sync, abort and pier side.
- Manual axis movement, tracking on/off, Sidereal/Solar/Lunar/King tracking rates,
  custom tracking rates and pulse guiding.
- Home, Park and Unpark.
- Shared manual/GoTo slew speed, expressed in degrees per second.
- Refraction or pointing-model tracking compensation with one or both axes.
- Saved RA/Dec motor settings, mount geometry, altitude and meridian limits,
  backlash, Home offsets/input assignment and Auto-Home on boot.
- Automatic meridian flip, pause at Home, preferred pier side and buzzer.
- Manual firmware alignment sequence: star count, Start, Accept and Save.
- Magicsee R1 Bluetooth enable/disable, discovery and controller selection.
- AP and three station Wi-Fi profiles, DHCP/static addresses and new passwords.
- Read-only controller uptime and last-reset diagnostics.

Bluetooth and network configuration controls are shown only when the firmware
advertises those capabilities. The driver does not expose unsupported focuser
or PEC controls. It does not capture images or automatically plate-solve an
alignment model.

## Requirements and installation

- INDI **2.2.5 or newer**.
- CMake 3.16+, a C++17 compiler and `nlohmann_json` development headers 3.2+.
- Linux is the validated platform. Other platforms/architectures are not yet
  represented as tested by the manufacturer.
- Tyr firmware with management protocol version 1 for USB. Firmware **v1.2.10
  or later is required for TCP**. Boot diagnostics require v1.2.9 or later.

Use the `indi-tyr` package if it is available from your distribution or INDI
package repository. Inclusion in this source repository does not guarantee an
immediate package release for every distribution.

To build just the driver from an `indi-3rdparty` checkout on Ubuntu/Debian:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
    libindi-dev indi-bin libnova-dev nlohmann-json3-dev python3
cmake -S indi-tyr -B build-tyr -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-tyr --parallel
ctest --test-dir build-tyr --output-on-failure
sudo cmake --install build-tyr
```

Your selected package source must provide the required INDI version. No special
Tyr udev rule is required for a normal CP2102 adapter. Your account needs serial
access, normally membership of the distribution's serial-access group, such as
`dialout` on Ubuntu. Log out and back in after changing group membership.

## USB connection

1. Power the mount from its normal power supply and connect its USB cable.
2. In INDI Control Panel, open **Connection**, select **Serial**, select the
   CP2102 device (usually `/dev/ttyUSB0`) and use **9600 baud**.
3. Select **Connect** in Main Control. Wait for firmware startup if necessary.
4. Read the reported firmware identity and the current mount settings.

Only one service may own a given serial device. Close competing serial clients.
Opening USB can reset some ESP32 adapter arrangements; the driver releases
DTR/RTS and waits up to 30 seconds for startup. If Auto-Home on boot is enabled,
a reset can initiate Home. Keep the rig clear during connection and startup.

In a virtual machine, attach the CP2102 adapter to the guest first. USB capture
and adapter reset wiring are separate from the INDI driver.

## TCP/IP connection

1. Connect your computer to the mount's access point or the LAN containing Tyr.
2. In **Connection**, select **Network** and **TCP**.
3. Enter the mount's IP address or hostname and its command port, normally
   **9999**. Enter the actual address configured on your mount.
4. Connect and confirm the Tyr identity and firmware version.

The command port is separate from HTTP/web configuration and from the local
Windows ASCOM Hub. UDP is not supported. Do not select the generic OnStep driver
for this device profile. Firmware before v1.2.10 does not expose Tyr management
commands on TCP. Keep polling below ten seconds, the mount's TCP idle timeout;
the default driver polling interval is one second.

## Site and time

INDI longitude is east-positive, expressed from 0 to 360 degrees. The driver
converts it to Tyr's west-positive convention when sending site coordinates.
Set the observing location in Ekos and verify the latitude/longitude sent to Tyr.

**Client offset (includes DST)** follows INDI's local-time convention.
**Tyr UTC minus STANDARD time (no DST)** is a separate saved mount setting on
Site Management. It uses the opposite sign and excludes daylight saving.

For Athens, the Tyr setting is **-2 throughout the year**, while the client's
offset can be +3 during summer. Do not enter -2 in the client field. UTC updates
use the saved Tyr offset to compute local standard date/time; they do not replace
it with the client's DST offset. Fractional-hour zones and date rollover are
handled by the conversion.

## Settings and operation

Settings are read automatically on connection and after saves. **Read mount
settings** refreshes all settings or a selected section; use it after changes
from another interface.

| Tab | Controls |
| --- | --- |
| Main/Motion | Coordinates, tracking, manual movement, pulse guiding and shared slew speed |
| Site Management | UTC, location, Tyr standard-time offset and Park options |
| Axes | RA/Dec configuration, firmware defaults versus saved settings and geometry |
| Limits | Horizon/overhead, east/west meridian limits and backlash |
| Home | Home offsets and Auto-Home on boot |
| Meridian | Automatic flip, pause at Home, preferred pier side and buzzer |
| Tracking | Tracking offsets |
| Alignment/Tyr | Manual alignment star count, Start/Accept/Save and Continue Flip |
| Bluetooth | R1 discovery/selection, enable/disable and current state |
| Network | AP, station profiles, DHCP/static addresses and password changes |
| Tyr | Firmware identity, live status, compensation, Home input assignment, refresh and restart |

Stop movement and guiding before changing limits/backlash. Motor, Home and
network edits also require tracking off. Requests that do not meet these
conditions are rejected; the driver does not stop equipment automatically to
apply configuration. Motor, geometry and network changes require a restart.
Home-offset changes invalidate the mount's trusted Home reference.

Wi-Fi edits remain pending until **Apply**. Blank password fields retain saved
passwords. **Read Network** discards pending edits. Network saves use a transaction
and abort on failure. Password inputs are not echoed in INDI properties or driver
debug logs. Non-network multi-command saves can partially succeed if firmware
rejects a later command; readback shows actual values and marks the save Alert.

Home and alignment commands can move the mount. Check rig/cable clearance and
observing limits first. Driver Restart is explicit and disconnects the driver.

## Troubleshooting and support

- **No connection:** check power, USB permissions or TCP address/command port,
  exclusive device ownership and required firmware version.
- **TCP drops:** retain a polling interval below ten seconds and check the LAN.
- **Settings Alert:** check the mount's motion/tracking state and firmware error;
  read settings again before retrying.
- **Incorrect time:** verify the saved standard offset and the separate INDI
  client offset; do not add DST to the Tyr setting.
- **GoTo rejected:** inspect Park, altitude/meridian limits and mount status.
  Do not bypass limits to force a rejected move.

Enable INDI debug logging when requested by support, reproduce the issue, and
include driver/firmware versions, connection type and the relevant error.
Contact **support@prometheusastro.com** or use
[Prometheus Astro support](https://prometheusastro.com/contact/).

Firmware installation is outside this driver's scope. Use Tyr's supported USB
installer/Control Center or the mount's web updater for OTA updates.

## Validation

Four automated tests cover simulation, serial telescope commands, serial
configuration and TCP configuration/Sync. They use isolated profiles and emulated
hardware, including fragmented TCP replies, firmware rejection, motion guards,
network transaction rollback and password redaction.

On 12 October 2026, the manufacturer reported that real-mount USB and TCP tests
passed using INDI 0.3.0 and firmware v1.2.10. That hardware report is distinct
from the automated checks. No ARM or macOS hardware results are claimed.

## License

This driver subtree is licensed under **GPL-2.0-or-later**. See `COPYING` and
`THIRD_PARTY.md`. This licence applies to the INDI contribution and does not
change the licensing of separate Tyr applications or firmware.
