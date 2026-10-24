# 10Micron Native Motion — v0.19

An additional INDI telescope driver that communicates directly with a 10Micron
controller over TCP. It preserves combined native state, separates command
acceptance from observed completion, and reconciles changes made outside INDI.
It does not replace the existing LX200 10Micron driver or change its configuration.

Reference hardware is a GM1000HPS. Other models have not been commissioned.
Requires firmware 2.14.9 or newer with the documented Ginfo format, INDI >=2.2.1,
C++17, CMake >=3.18, pkg-config and a POSIX socket platform. Linux is tested;
Windows-native builds are not supported by this socket implementation.

See [SETUP.md](SETUP.md) for repeatable build/configuration/commissioning and
[VALIDATION.md](VALIDATION.md) for evidence and limits. [ARCHITECTURE.md](ARCHITECTURE.md)
describes the controller/driver/client boundary and native properties.

## Supported interface

| Capability | Behavior |
|---|---|
| Network connection | Configurable NATIVE_ENDPOINT host/port; default 192.168.0.1:3490 |
| Pointing and state | Ginfo RA/Dec, pier side, altitude/azimuth, UTC JD, native status and settling |
| Goto | Native command rejection retained; completion requires telemetry |
| Park/unpark | Controller owns park position; repeat settled unpark is idempotent |
| Tracking / abort | Tracking on/off; standard abort is slew halt, which may retain tracking |
| Stop including tracking | NATIVE_STOP_ALL; separate from standard Abort |
| Offset sync | Explicit CMCFG0 then CM; no model-point creation; controller-wide mode persists |
| Meridian | Limits, stored-target destination side and tracking-end estimate telemetry |
| Explicit flip | Native FLIP with accepted/rejected outcome, pier-transition and settled completion |
| Enclosure | Standard Active Dome + Dome Policy; ignored by default for outdoor use |
| Configuration | Endpoint, Active Dome/policy and interrupted-park warning persistence |

Manual axis motion, pulse guiding, guide rates, selectable tracking rates,
time/location upload, refraction-data upload and model construction are not
implemented here. Existing mount/MW4 setup supplies those controller settings.
There is no general raw-command console. Do not infer full functionality from
the existing LX200 driver's wider feature set.

## Enclosure policy

No enclosure: leave Dome ignored selected and Active Dome blank. No arming switch
or startup script is required. With an enclosure: while disconnected, set its
exact Active Dome name and select Dome locks. Motion then requires its connected
CONNECTION/Ok and unparked DOME_PARK/Ok. Once DOME_SHUTTER has been observed,
open/Ok shutter status is required too. Busy, Alert, incomplete/contradictory,
deleted or disconnected evidence blocks motion. A missing configured dome never
silently becomes outdoor mode. Explicit Dome ignored remains intentional even
if a dome name is saved.

Permission loss while active requests a safety STOP and retains a recovery hold;
restoring the dome alone does not resume tracking. Deliberate reconnect or a
completed recovery park with enclosure permission releases that hold. An explicit
interrupted-park warning persists; do not equate a STOP-created native parked flag
with a safely completed park. Configure reciprocal roof/mount clearance separately.

Standard enclosure reports are not independent roof sensors and cannot detect a
driver that continually reports false open status. No universal freshness timeout
or private RollOff ino field is assumed. Site-specific physical-limit checks may
still be useful; they are not a prerequisite to ordinary mount operation.

## Ekos compatibility

Ordinary INDI Goto, Park, Unpark, tracking and offset sync work without custom
scripts. Native capture-budget gating and native-aware flip orchestration were
tested with separate KStars patches and are not included in this driver PR.
Stock Ekos flip behavior must be commissioned separately; scheduling telemetry
is not a promise that stock Ekos consumes it or displays our proposed countdown.
Gmte is time until a tracking limit (horizon or meridian), not time until flip.

See [Legacy Ekos flip setup](LEGACY-EKOS-FLIPS.md) for HA units, controller
side-selection/limit compatibility, exposure margins and attended verification.

Licensing/attribution and repository placement are review items before final
acceptance. No manufacturer protocol PDF or third-party driver source is bundled.
The supplied source retains the contributor's code, with no invented attribution.
