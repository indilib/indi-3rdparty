# 10Micron Native Motion — v0.28

Native INDI observing driver for 10Micron controllers over **TCP or serial**.
The controller owns physical state; requests are not treated as completed until
native telemetry confirms the result. External changes made through MW4 or other
clients are reconciled without replaying motion or configuration writes.

Reference hardware: GM1000HPS firmware 3.4 on Linux/Pi, INDI 2.2.1. The protocol
requires firmware >=2.14.9 with documented Ginfo. Other models/firmware still need
commissioning. Requires C++17, CMake >=3.18, pkg-config, INDI >=2.2.1 and libnova.
The transport implementation uses POSIX sockets/termios; Windows-native is not
claimed. Python 3 is needed for the full test suite, not ordinary driver builds.

See [user documentation](doc/index.md), [setup](SETUP.md),
[capability/evidence matrix](doc/CAPABILITIES.md), [validation](VALIDATION.md),
[architecture](ARCHITECTURE.md) and [changes since v0.19](CHANGELOG.md).

## Implemented observing interface

| Area | Behavior |
|---|---|
| Transport | TCP or serial; disconnected-only endpoint/device/baud edits; saved selection |
| Native state | Ginfo pointing, pier side, altitude/azimuth, JD, status and settling |
| Motion | Goto, one-shot side choice, Park/Unpark, tracking, manual N/S/E/W and slew rates |
| Guiding | Standard timed pulses, independent RA/DEC timers, variable durations, zero no-op |
| Tracking | Sidereal/solar/lunar/custom requests; bounded On/Off confirmation and truthful readback |
| Sync | Explicit CMCFG0 then CM offset sync; no model point is added |
| Settings | Guide rate; refraction temperature/pressure; supported direct-weather update modes |
| Clock/site | Native startup/live UTC, offset and site readback; explicit guarded time/location writes |
| Meridian | Read-only limits/tracking-end telemetry; explicit FLIP acceptance/refusal/completion |
| Safety | Standard enclosure policy, bounded initial report reconciliation, STOP/recovery hold |
| Recovery | Settings rejection separated from communication failure; no blind motion replay |

Model creation/editing/loading/analysis, firmware maintenance and specialist
satellite workflows remain with dedicated controller/MW4 tools. MW4 is not a
runtime dependency for observing. This is an observing interface, not a promise
to expose every manufacturer command or duplicate model-management software.

The existing LX200 10Micron driver already supports TCP, Ginfo and native FLIP.
The proposed distinction is the observed operation lifecycle, command-specific
framing, recovery/interlock behavior and regression coverage, not exclusive
access to those commands. Whether this should remain separate or be integrated
into the existing driver remains a maintainer review decision.

## Enclosure and startup

Open-air use defaults to Dome ignored. With an enclosure, configure its exact
Active Dome name and Dome locks while disconnected, with reciprocal roof/mount
locking. Motion requires connected/Ok plus unparked/open standard properties;
when an optional shutter is advertised it must also be open/Ok.

Initial silence/Idle definitions and an empty Ok DOME_PARK definition receive
at most five seconds of discovery, with repeated current-property requests.
New motion stays blocked. Existing native tracking is adopted without a reset.
Valid current open snapshots suffice; an open transition is not required.
Explicit closure/fault and later permission loss retain immediate STOP for an
active mount; unconfirmed evidence at expiry also retains STOP. No automatic
tracking/capture restart occurs when permission returns.

A roof driver's Alert remains blocking, even with UNPARK selected. A separate
RollOff ino rejected-close reporting correction was tested alongside v0.28;
that unrelated driver patch is not silently included in this PR. See
[startup evidence and compatibility](doc/STARTUP-AND-ENCLOSURES.md).

## Evidence and limits

The v0.28 development source passed 40/40 tests against installed INDI 2.2.1.
The publication checkout is rebuilt and retested separately; final results are
recorded in VALIDATION.md. Tests use loopback TCP/serial PTY emulators and
isolated configuration/home directories, not physical mount connections.

Attended hardware checks cover serial connection/settings and TCP controls,
startup adoption while tracking, ordinary Park, parked/closed restart and the
paired roof correction. Actual image displacement, non-guided dithering/settling
and complete flip/capture recovery remain clear-sky gates. Companion native-aware
KStars work is separate: stock Ekos support must not be inferred from telemetry
properties alone. See [legacy Ekos flip behavior](LEGACY-EKOS-FLIPS.md).

No manufacturer protocol PDF, private observatory configuration, binaries or
backup trees are included. Repository placement, naming, licensing/attribution
and maintenance integration remain review topics.
