# Legacy Ekos hour-angle flips

Use this with the traditional HA-driven planner. Native-aware scheduling and
countdown display are separate KStars changes. No universal safe offset is
established here for every target, mount limit and exposure sequence.

## Units

For an ordinary upper-meridian crossing, HA = local sidereal time minus target
RA: negative before transit, zero at transit, positive after. The offset is
relative to transit, not sequence start. Check whether your UI displays hours
or degrees. Pier-side/lower-culmination and circumpolar cases need separate checks.

| HA hours | Degrees past meridian | Approximate time after transit |
|---|---|---|
| 0.00 | 0 | Transit |
| 0.05 | 0.75 | 3 minutes |
| 0.10 | 1.5 | 6 minutes |
| 0.20 | 3 | 12 minutes |
| 0.25 | 3.75 | 15 minutes |

One HA hour is 15 degrees; sidereal time runs slightly faster than civil time,
so these elapsed times are rounded. 0.1 HA is not 0.1 degree or one minute.

## Configure and demonstrate

1. Choose a flip authority. For an Ekos-requested test, enable Ekos flipping and
   configure the controller consistently. Our reference mount had autonomous
   flip disabled so Ekos owned the request. Set this through the controller/MW4;
   the driver only reports UNATTENDED_FLIP. This does not disable protective
   tracking/horizon limits. Autonomous flip with image recovery is a separate
   mode to commission, not a guaranteed fallback.
2. Record slew and tracking meridian limits, permitted object sides, site/time,
   target geometry and mechanical/cable clearance. Keep NATIVE_SLEW_SIDE on Auto
   for legacy testing; explicit native FLIP/forced-side tests are different.
3. Under supervision determine when an ordinary re-slew to the target actually
   selects the opposite pier side. Zero/positive HA alone does not prove this:
   the controller can retain the current side inside its allowed overlap.
4. Put the Ekos threshold inside that demonstrated eligibility window and well
   before the tracking stop or earlier horizon/clearance restriction. Neither
   numeric controller limit is an instruction to copy it into the Ekos field.
5. Allow for the longest remaining exposure, download/decision delay and actual
   flip/slew/settling time. Shorten exposures or use another verified setup if
   those cannot fit. Do not count automatic retries as the safety margin.
6. Start tracking/capturing before transit. Keep the capture count and Scheduler
   job long enough for threshold + exposure wait + flip + post-flip recovery +
   several resumed science frames. A job ending before then cannot test a flip.
7. Verify changed pier side, native tracking/settling, alignment/guiding recovery
   as configured and resumed science frames. A completed slew alone is not proof.

The inspected stock path coordinates with Capture, then re-slews to the target
expecting the controller to choose the other side. It is not automatically the
native FLIP operation. Its start requires tracking; if the mount stops first,
do not assume the legacy planner will recover. It can retry after four minutes
when pier side remains unchanged; that retry may exceed a narrow tracking window.

Example arithmetic only: a demonstrated opposite-side threshold of 3 degrees
is 0.20 HA (~12 minutes past transit); a 4-degree tracking stop is ~16 minutes,
leaving about four minutes total. A two-minute exposure consumes much of that
before download, flip and settling. This is not a recommended universal setting.
Neither 0 nor 0.1 HA guarantees a flip because the slew limit is 3 degrees.

## Diagnostics

```bash
indi_getprop -t 5 \
  '10Micron Native Motion.TELESCOPE_PIER_SIDE.*' \
  '10Micron Native Motion.NATIVE_STATE.*' \
  '10Micron Native Motion.NATIVE_MERIDIAN.*' \
  '10Micron Native Motion.NATIVE_SCHEDULING_DATA.*'
```

TIME_TO_END_MIN/TRACK_END_ESTIMATE_SEC estimate a tracking limit (horizon or
meridian), not flip time. STORED_TARGET_DESTINATION_SIDE concerns the stored
controller target, not an arbitrary catalog object. Do not subtract a limit
angle from tracking-end data and call it a guaranteed flip countdown.

Save Mount/Capture/Align/Guide logs. Look for flip planned, waiting, re-slew,
pier side unchanged, mount not tracking and completion messages, plus native
state/pier observations. Wording and behavior vary by KStars version.

## Sources and scope

Inspected October 24, 2026: checkMeridianFlip, startMeridianFlip and
updateTelescopeCoord in current upstream legacy source:
https://github.com/KDE/kstars/blob/master/kstars/ekos/manager/meridianflipstate.cpp

Units: https://kstars-docs.kde.org/en/user_manual/ai-sidereal.html

Native semantics were developed against 10Micron Mount Command Protocol 2.15.1;
obtain it from the manufacturer. Current source does not prove every release
behaves identically. First legacy flip must be attended; no single HA setting
is claimed universally safe for stock Ekos.
