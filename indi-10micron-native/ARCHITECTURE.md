# State ownership and protocol boundary

The mount owns physical state; the driver translates observations; clients make
requests. A command ACK is acceptance, not completion. No module may manufacture
physical park/tracking state merely from a request. Driver polling reconciles
native changes made through other clients.

Files: tenmicron_protocol.cpp/.h serialize a persistent TCP connection and parse
command-specific response shapes; motion_state.h defines observed completion;
enclosure_state.h defines standard enclosure permission; native_schedule.h is a
pure scheduling-budget helper; tenmicron_motion_driver.cpp is the INDI adapter.
tenmicron_driver.cpp is the optional-in-use diagnostic read-only telescope
executable; tenmicron-probe is a console diagnostic. The latter sets precision
but sends no requested slew/park command; do not mistake it for a safety monitor.

Native Ginfo supplies RA/Dec, pier side, azimuth, altitude, UTC JD/leap indication,
native status and independent slew/settling. Unknown/invalid data is not coerced
to safely parked. Commands have command-specific one-byte/hash/no-reply framing;
protocol timeout or framing uncertainty closes the socket rather than replaying
motion. TCP connect/DNS setup are not claimed as universal hard real-time bounds.

Goto uses native target setters and MS (or one-shot forced side). Sync explicitly
selects CMCFG0 before CM. Park/unpark use hP/PO and poll completion. Q halts slew;
STOP also disables tracking. Flip uses FLIP and requires opposite native pier
side, settled target and tracking for completion; a confirmed native refusal is
distinct from ambiguous failure. Accepted flip has a 180-second safety deadline.

| Property | Purpose |
|---|---|
| NATIVE_ENDPOINT | Editable disconnected host and TCP port |
| NATIVE_IDENTITY / NATIVE_STATUS_TEXT | Identification, state description, communication validity |
| NATIVE_STATE | Native status, settling, parked/tracking, azimuth/altitude/JD |
| NATIVE_MERIDIAN | Mount-reported slew/track limits, time to tracking end, allowed side and stored target side |
| NATIVE_SCHEDULING_DATA | Estimate valid/age/seconds and rounding reserve; not a flip countdown |
| NATIVE_SCHEDULING_CONTRACT | Contract version/scope; no stock client integration promised |
| NATIVE_FLIP_RESULT | Idle/accepted/confirmed refusal/blocked-error and response sequence |
| NATIVE_SLEW_SIDE | One-shot Auto/West/East for next slew |
| NATIVE_STOP_ALL | Explicit stop including tracking |
| NATIVE_PARK_SAFETY | Persisted diagnostic interrupted-park flags, read-only |
| NATIVE_ROOF_INTERLOCK / NATIVE_MOTION_STATUS / NATIVE_COMMAND_STATUS | Permission and command diagnostics |

Gmte supplies horizon-or-meridian tracking end in whole minutes. The driver
publishes freshness and rounding uncertainty but cannot infer a unique reason
for every tracking-to-stopped transition. Status 1 does not identify its cause.
No driver command changes unattended-flip configuration or meridian limits in
this revision; they are read-only telemetry.

Protocol reference used during development: 10Micron Mount Command Protocol
software version 2.15.1. Obtain the current manual from the manufacturer; it is
not redistributed. No manufacturer endorsement or universal firmware/model
coverage is claimed. The existing INDI LX200 10Micron driver remains separately
maintained; this submission does not assert all its behavior is defective.
