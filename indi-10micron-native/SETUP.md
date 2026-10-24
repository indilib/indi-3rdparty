# Setup and commissioning

These commands target Linux/Pi. Build in a new checkout; preserve existing driver
executables/settings for rollback. Do not install or launch against an active
observing session. Host tests below use local emulators, not real mount hardware.

## Dependencies and isolated build

On Debian/Pi install build tools and INDI development packages, then verify the
actual INDI version. Distribution repositories may provide an older version;
stop rather than assuming the package name guarantees >=2.2.1.

```bash
sudo apt install build-essential cmake pkg-config python3 libindi-dev
pkg-config --modversion libindi
cmake -S indi-10micron-native -B build-10micron-native \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_INDI_DRIVER=ON -DBUILD_TESTING=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/stage-10micron-native"
cmake --build build-10micron-native -j2
ctest --test-dir build-10micron-native --output-on-failure
cmake --install build-10micron-native
```

Execute from the parent of indi-10micron-native. Expected: all 19 tests pass,
including indi-flip-timeout (~182 seconds), and staging contains both executables
and registry XML. No SKIP is equivalent to validation. Python must be found to
register loopback/INDI integration tests; verify `ctest -N` lists 19. Source-only
test builds without INDI register fewer tests and do not validate driver callbacks.

The stage directory is private installation output, not a system install. CMake
defaults now enable drivers and disable tests; explicit flags above enable all
tests. Do not run sudo cmake --install against the staging build by mistake.

For full production installation, after attended commissioning and backups,
configure a separate install prefix consistent with your INDI installation
(commonly /usr/local), rebuild and install deliberately. Registry XML and binary
must both be visible to the actual indiserver/Ekos profile. Avoid duplicate
copies with the same executable name earlier on PATH. Linux package-manager
owned /usr/bin files should not be overwritten as a first commissioning step.

## First attended connection using only the staged motion driver

Stop the existing INDI profile before this isolated test. Keep the real mount
parked and roof clearance independently confirmed. Never run two motion drivers
against the same controller for commissioning. This standalone command starts
only the mount, no roof controller:

```bash
indiserver -v "$PWD/stage-10micron-native/bin/indi_10micron_native_motion"
```

Connect an INDI client to this server (default localhost:7624). Mount network
endpoint is a separate address: typically mount TCP port 3490, not indiserver
7624. Set NATIVE_ENDPOINT HOST and PORT while disconnected; use the controller's
actual LAN settings, not the generic default. Save configuration explicitly.

Portable/open-air setup: Dome ignored, no Active Dome. Observatory setup: load
the actual enclosure driver into your intended profile/server, set its exact
Active Dome name and Dome locks while mount disconnected, configure reciprocal
mount locking in the enclosure, then save. A standalone mount-only server cannot
receive a configured enclosure's reports unless a client forwards those snoops.

Connect and read:

```bash
indi_getprop -t 5 '10Micron Native Motion.DRIVER_INFO.*' \
  '10Micron Native Motion.NATIVE_STATUS_TEXT.*' \
  '10Micron Native Motion.NATIVE_STATE.*' \
  '10Micron Native Motion.TELESCOPE_PARK.*' \
  '10Micron Native Motion.NATIVE_ROOF_INTERLOCK.*'
```

Expected version 0.19, healthy connected telemetry, and observed native parked
status 5 if physically parked. Roof interlock should be blocked if a configured
roof is closed; that is correct. Connection reconciles controller state, it does
not unpark or slew. Disconnect/restart and confirm endpoint, Active Dome and
policy are restored before proceeding. Wrong firmware/endpoint means stop and
correct it; never move the telescope simply to make a status light green.

## Physical commissioning

1. For an observatory, confirm roof OPEN with physical sensors before Unpark.
   Standard dome unpark status may disagree with the physical roof.
2. Use normal INDI Unpark; await settled native unparked status. Repeat Unpark
   should report already settled rather than send another motion command.
3. Supervise a clear-sky Goto and verify native slew/settle completion. Compare
   reported coordinates/pier side with the controller. Avoid daylight targets
   near the Sun. Do not send arbitrary example RA/Dec without checking geometry.
4. Test standard Abort separately from native Stop Including Tracking; confirm
   controller state. Stop is not necessarily a safe park for roof closure.
5. Request Park and await native parked plus settled confirmation. Close roof
   only after independent clearance checks. Do not accept a stale logical flag.
6. Restart and verify saved configuration. Preserve failed/interrupted park
   warnings and use documented recovery; never edit persisted flags to clear them.

Configured-enclosure disconnect/reconnect and retained recovery hold are covered
by emulators; real fault injection remains attended commissioning work. Do not
close a real roof onto an unparked telescope to test interlocks.

Sync changes the controller-wide CMCFG mode to offset-only CMCFG0. Coordinate
with MW4/model-building tools; the driver does not restore another client's model
refinement mode. Time, site, clock, refraction and mount limits remain configured
on the controller. No weather shutdown is implemented in this driver.

## Troubleshooting / rollback

| Symptom | Action |
|---|---|
| No connection | Actual mount address/port, LAN routing, firmware >=2.14.9, competing clients |
| Blocked configured enclosure | Exact saved name, connected/Ok, unparked/Ok, optional shutter open/Ok |
| Settings lost at restart | Explicit Save, config directory permissions, duplicate executable versions; endpoint regression covers restore |
| Communication failure | Outcome uncertain; telemetry becomes Alert/unknown; reconnect deliberately and inspect native state |
| Park warning after STOP | Treat as interrupted operation; do not close roof merely because status says parked |
| Flip rejected | Native controller decides eligibility; tracking-end estimate is not flip-ready time |
| Flip accepted but times out | Safety STOP/retained Alert; no blind flip retry; inspect physical/native state |

To roll back, stop this server/client session and restore the previous profile
and executable routing. Stage-only installation has not overwritten system
files. Save diagnostic logs before restarting; do not rely on disappearing GUI
module history. Do not copy site credentials/configuration into an upstream PR.
