# State ownership, protocol boundary and public properties

The controller owns physical state. Driver requests are not evidence of physical
completion; clients coordinate observing workflow. Polling reconciles native
changes made through separate connections without replaying commands. During
reference commissioning the operator used MW4 as the independent position
reference; MW4 is not required at observing runtime.

- tenmicron_protocol.cpp/.h: serialized persistent TCP or POSIX raw serial I/O,
  command-specific byte/hash/no-reply shapes, exact parsers, native settings.
- motion_state.h: observed completion and bounded transition rules.
- enclosure_state.h: standard connected/open permission (no vendor light name).
- native_schedule.h: pure budget helper; not automatic stock-Ekos integration.
- tenmicron_motion_driver.cpp: INDI motion/guider adapter, timers, permissions,
  readback, operation lifecycle and recovery.
- tenmicron_driver.cpp / probe.cpp: diagnostic telescope/probe front ends;
  not a general safety supervisor or a second independent motion owner.

Ginfo supplies RA/Dec, pier, az/alt, UTC JD, status and independent settling.
Clock uses native JD via libnova; offset readback converts native GG sign to INDI.
Unknown state is not coerced safely parked. Unsupported preset readbacks remain
neutral. Complete settings rejection does not disconnect or invent unparked state;
framing/transport uncertainty closes the link, invalidates state and prohibits
blind motion replay. TCP DNS setup is not a universal hard real-time claim.

Goto uses target setters and MS/one-shot forced side. Park/unpark use hP/PO and
poll completion. Q halts slew; STOP includes tracking stop. FLIP acceptance is
not completion: opposite pier, settled target and tracking are required, with an
accepted-operation deadline. Tracking-only requests have a shorter bounded
confirmation policy and safe cancellation rules; they cannot weaken guards for
slew, manual movement, pulses or unknown roof state.

| Properties | Responsibility |
|---|---|
| NATIVE_TRANSPORT, NATIVE_ENDPOINT, NATIVE_SERIAL | Disconnected configuration, applied transport readback |
| NATIVE_IDENTITY, NATIVE_STATE, NATIVE_STATUS_TEXT | Identity and actual native physical/communication state |
| TELESCOPE_PARK, TELESCOPE_TRACK_STATE, EQUATORIAL_EOD_COORD, TELESCOPE_PIER_SIDE | Standard observing state derived from native samples |
| TELESCOPE_MOTION_NS/WE, TELESCOPE_SLEW_RATE | Manual axis requests/rate selection with normal motion guards |
| TELESCOPE_TIMED_GUIDE_NS/WE | Independent duration timers, cancellation and no-op semantics |
| NATIVE_GUIDE_RATE, NATIVE_REFRACTION, NATIVE_WEATHER_UPDATE, NATIVE_REFRESH_CONTROLS | Supported settings, ownership, explicit requests/readback |
| TIME_UTC, GEOGRAPHIC_COORD | Native current readback and explicit guarded writes |
| TELESCOPE_TRACK_MODE/RATE | Standard rate requests; no invented readback for unqueryable presets |
| NATIVE_MERIDIAN | Read-only native limits, Gmte, allowed side and stored-target destination |
| NATIVE_SCHEDULING_DATA/CONTRACT | Estimate freshness/scope, not a guaranteed flip countdown |
| NATIVE_FLIP_RESULT, NATIVE_SLEW_SIDE | Explicit flip outcome/sequence and one-shot next-slew side |
| NATIVE_STOP_ALL, NATIVE_PARK_SAFETY | Explicit full STOP and persisted interrupted-park diagnostics |
| NATIVE_ROOF_INTERLOCK, NATIVE_MOTION_STATUS, NATIVE_COMMAND_STATUS | Permission versus requested-operation diagnostics |

Initial unknown enclosure evidence has the bounded discovery policy documented in
[STARTUP-AND-ENCLOSURES.md](doc/STARTUP-AND-ENCLOSURES.md). It grants no new motion;
confirmed unsafe evidence/later loss retain STOP and recovery holds. Standard
roof state is not an independent sensor and cannot validate a lying roof driver.
A failed roof-close request must not be confused with physical closure, but a
reported Alert is not ignored by the telescope to conceal that reporting error.

Sync asserts CMCFG0 then CM; its acknowledged offset mode persists controller-
wide and is reasserted each transaction. No model refinement command is used.
Unattended flip/meridian-limit configuration is read-only here. Model operations,
firmware updates and specialist tracking workflows remain with dedicated tools.

The existing LX200 driver already supports TCP/Ginfo/native FLIP and shared base
behavior. This implementation does not prove those capabilities cannot be fixed
or extended there. Separation, repository placement, neutral property naming and
shared maintenance are explicit review topics, not claims settled by test count.
No manufacturer protocol PDF or endorsement is included.
