# Validation and review limits — October 25, 2026

## Current publication update

v0.28 driver runtime sources match the current staged/tested candidate; upstream
build defaults (drivers ON, tests OFF) and GNUInstallDirs are retained. libnova
is added for native-JD display, with all 40 tests registered when testing/Python
are enabled. Test home/configuration isolation is explicit. No private site
configuration, backup tree, manufacturer PDF or binary is submitted.

Fresh publication checkout: **40/40 tests passed in 220.52 seconds** against
installed INDI 2.2.1 on the Pi, Release C++17 with warnings as errors. The retained
accepted-flip timeout passed in 182.37 s; TCP/serial startup regressions in
26.50/29.50 s. Both executables and registry XML installed into a private stage.
A default configure independently confirmed driver builds ON and tests OFF.
Real operator park metadata contents and modification time were unchanged.

The earlier local v0.28 candidate passed 40/40 in 219.66 s (182.39-s timeout);
the results above are a separate fresh publication-source build/run, not reused
candidate numbers. Runtime .cpp/.h/XML inputs match the hardware-tested v0.28
candidate; publication differences are build defaults/install conventions,
documentation and additional explicit test home isolation.

## Versioned hardware evidence

| Reference path | Evidence and scope |
|---|---|
| GM1000HPS firmware 3.4 / serial 9600 | Prolific adapter connection, parked guide-rate write/readback/restore, manual refraction behavior; user confirmed weather-mode agreement with independent MW4 connection during v0.22–0.24 work |
| TCP v0.24 | Eight 100/250-ms directional pulse Busy→Idle lifecycles; tracking retained; ordinary On/Off/Goto/Park observed. Optical displacement is not proven |
| v0.25 | Ignored pre-first-slew Tracking On bounded failure released; subsequent safe slew worked without reconnect |
| v0.28 + separate roof local correction | Stationary startup; already-tracking mount-only reconnect; full profile restart with both drivers initializing together. All 59 valid samples in full restart stayed status 0/tracking 1/unparked/not settling; no STOP |
| Paired roof behavior | Naturally occurring thirty-second weather-close request refused by mount lock while verified open state/permission remained valid; no deliberate dangerous closure |
| v0.28 Park/closure | Normal native Park completed; user confirmed safe parked position; explicit roof closure reached closed-only limit and standard closed/Ok |
| Parked/closed restart | All 57 valid samples stayed status 5/parked 1/tracking 0/not settling; closed permission correctly blocked; no unsolicited movement |

These are attended development/operator records. Reference environment is Linux
Pi/aarch64, installed INDI 2.2.1 and a separate patched development KStars 3.8.5.
The native/roof companion changes and KStars patches must not be presented as
stock-Ekos guarantees or as part of this telescope-only PR. The independent roof
reporting correction is documented in doc/STARTUP-AND-ENCLOSURES.md. No direct
controller-wire/relay trace is claimed by an INDI-level capture.

Earlier v0.19 was tested against INDI 2.2.1 and 2.2.5 in development and had a Pi
19/19 publication run. Those historical results do not certify v0.28 against every
later INDI version. Earlier serial hardware evidence is not a claim that every
v0.28 serial motion/optical path was newly commissioned.

## What the tests exercise

Protocol parsing, exact response framing, TCP/serial fragmentation, termios,
native state transitions, rejection/uncertainty, command deadlines, guide timers,
manual/guide cancellation, settings guards/rejections/readback, direct-weather
ownership, fractional clock offsets, configuration persistence, native flips,
recovery, enclosure connection/park/shutter ordering and initial empty definitions.
The hardware-captured startup fixture reproduces STOP on unchanged v0.27 and
passes on v0.28; placeholder definitions, missing evidence expiry and actual
closed/fault/later-loss cases remain negative tests.

## Open commissioning/review items

- Image-based guide displacement/calibration, non-guided dither/settle and a full
  native flip with post-flip alignment/guiding/capture resumption.
- Broader models, firmware, USB adapters/baud rates and roof implementations.
- Complete current-candidate physical manual-axis/serial motion checks.
- Independent pressure-only physical change/restore, not conflated with earlier
  successful manual-temperature editing or valid pressure rejection.
- Full autonomous-flip recovery and normal later motion/park coherence.
- Actual stock Ekos forced/native flip settings and separate KStars orchestration.
- Repository placement/shared LX200 maintenance, neutral property interface,
  licensing/attribution and public naming.

No destructive real roof-fault injection, every-mode completeness or generic
unattended readiness is asserted. See doc/LEGACY-FLIP-INCIDENT.md for the actual
historical failure/configuration evidence and explicitly missing settings/trace.
