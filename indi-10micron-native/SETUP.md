# Setup and commissioning — v0.28

## Build privately first

Linux/Pi dependencies: C++17 compiler, CMake >=3.18, pkg-config, INDI development
libraries >=2.2.1, libnova development files, and Python 3 for tests. Verify the
actual installed INDI version; package names alone are not a version guarantee.

```bash
pkg-config --modversion libindi
cmake -S indi-10micron-native -B build-10micron-native \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_INDI_DRIVER=ON -DBUILD_TESTING=ON \
  -DCMAKE_CXX_FLAGS=-Werror \
  -DCMAKE_INSTALL_PREFIX="$PWD/stage-10micron-native"
cmake --build build-10micron-native -j2
ctest --test-dir build-10micron-native -N
ctest --test-dir build-10micron-native --output-on-failure -j2
cmake --install build-10micron-native
```

Run from the parent of indi-10micron-native. With driver/testing/Python enabled,
expect **40 tests**, including the accepted-flip timeout (~182 seconds).
Production defaults build drivers ON and tests OFF; no Python is required for
that ordinary build. Tests use loopback controllers/serial PTYs and isolated
configuration/home directories. No skipped or missing suite counts as a pass.

GNUInstallDirs controls binary/registry install paths. This prefix is private
staging, not a system install. Do not overwrite package-managed binaries or
switch a running profile. Keep executable and registry XML routing consistent;
avoid an older duplicate earlier on PATH. Preserve existing executables/settings
for rollback. Installing this driver does not install companion KStars patches.

## Transport

NATIVE_TRANSPORT selects TCP or Serial while disconnected. For TCP set the actual
controller NATIVE_ENDPOINT; the generic default is 192.168.0.1:3490. Controller
TCP 3490 is not the INDI server's usual client port 7624.

For Serial set NATIVE_SERIAL device and baud, normally a stable by-id device
and the controller's configured rate. Defaults are /dev/ttyUSB0 and 9600. It is
raw 8N1 without flow control. See [serial/settings](doc/SERIAL-AND-SETTINGS.md).
Do not change controller baud or transport while connected. Save configuration
explicitly, restart and verify applied selection/readback. No automatic fallback.

## Initial state and enclosure

Keep the reference mount parked for the first connection and independently check
clearance. Stop old INDI profiles before selecting staged executables. In an
observatory, start the actual roof driver in the intended server/profile, set its
exact Active Dome name and Dome locks while mount disconnected, and configure
reciprocal mount locking in the roof. Open-air use leaves Dome ignored selected.
Do not bypass the interlock to make an error indicator disappear.

Verify DRIVER_INFO=0.28, correct active transport/endpoint, native status/settling,
standard park/tracking selection, UTC/offset/site and supported settings. These
populate from native readback, not a host time/site push. A parked mount behind a
closed roof should show native parked status 5 and blocked roof permission; this
is correct. An initial wait may last at most five seconds. Never move solely to
make a preset light green when the protocol has no unambiguous mode getter.

## Attended gates

1. Parked/closed profile restart: correct park/tracking adoption, closed permission
   blocked, no unsolicited tracking or motion.
2. Physically open roof and safe clearance before Unpark. Reference controller
   may stay stopped until a first slew; ignored Tracking On must fail within its
   bounded window and not leave all future safe motion locked indefinitely.
3. Safe attended Goto/Abort/recovery; compare actual coordinates/park with the
   controller/MW4 rather than trusting a UI flag. Coordinate separate clients;
   shared target registers/settings require one intended writer.
4. Already-tracking restart with physically open roof: adopt tracking without
   STOP. To stop a profile while preserving tracking, disconnect mount first;
   roof-first disconnection correctly removes permission from an active driver.
5. Native Park must complete from telemetry. Obtain independent physical safe-
   parked confirmation before approving roof closure. STOP alone is not Park.
6. Image-based guide displacement, non-guided dither/settle and full flip/capture
   recovery require usable sky. Do not label a timer-only guide test optical proof.

No deliberate real roof closure against an unparked mount is a valid negative
test. Emulators cover closed/fault/missing/later-loss reports, transport ambiguity
and deadlines. Current paired RollOff ino reporting correction is separate; see
[startup compatibility](doc/STARTUP-AND-ENCLOSURES.md).

## Settings and recovery

Guide rate/refraction/weather ownership are described in the serial/settings
page. Native automatic weather mode must not be silently disabled for a manual
edit. Settings rejection and communication uncertainty are different outcomes.
Explicit time/location writes are guarded. Model building and model loading
remain with dedicated controller/MW4 workflows. Offset Sync selects persistent
controller-wide CMCFG0 each time; it is not model-point creation.

Gmte is a tracking-end estimate (horizon or meridian), not a guaranteed flip-ready
time. [Legacy Ekos flips](LEGACY-EKOS-FLIPS.md) covers ordinary HA planning and
forced/native-flip distinctions; the existing LX200 driver also supports native
FLIP. Native-aware KStars planning/capture changes are separate from this PR.

Communication uncertainty invalidates telemetry and holds new motion. No command
is blindly replayed and no capture automatically resumes. An interrupted-park
warning must not be edited away. A complete observed recovery park with valid
roof proof differs from a static parked flag. Preserve logs before reconnecting.

To roll back, stop the intended profile and restore the old executable/registry
routing. Private staging has not changed system binaries or controller settings.
