# Capability and evidence matrix — v0.28

Implemented does not mean every path is hardware commissioned. Emulator tests
exercise real executables over loopback TCP and serial PTY; electrical/optical
behavior is separate. Earlier hardware results are versioned below.

| Function | v0.28 implementation | Evidence / remaining gate |
|---|---|---|
| TCP connection/state | Persistent native protocol, exact reply framing, current state adoption | GM1000HPS hardware + emulators |
| Serial | Raw 8N1 termios, supported baud selection, shared transaction/state path | Physical Prolific/9600 connection and settings on v0.22–0.24; v0.28 PTY tests; serial optical motion not claimed |
| Goto, Park, Unpark | Fresh guards, native completion/settling, no ACK-as-completion | Earlier attended Goto/Unpark cycles; v0.28 normal Park and parked/closed restart passed |
| Tracking | On/Off bounded confirmation; true readback; no automatic resume | v0.24 On/Off and v0.25 ignored-request recovery observed; v0.28 tracking adoption on full restart passed |
| Manual axes/slew rate | Standard N/S/E/W requests, halt/cancel guards and selectable rates | Emulator coverage; complete current-candidate physical axis sweep not asserted |
| Timed guide pulses | Independent timers, variable whole-ms durations, bounded native maximum, zero no-op | v0.24 eight N/S/E/W 100/250-ms lifecycles observed; actual camera displacement still pending |
| Guide rate | Native getter/writer/readback and idle inter-pulse changes | Parked physical write/readback/restore + TCP/PTY tests |
| Refraction | Changed fields only, native field widths, local settings rejection and actual readback | Serial/manual-temperature behavior observed; independent pressure-only restore not fully recorded |
| Weather ownership | Manual/auto-when-not-tracking/auto-continuous read/write/readback | User confirmed INDI/MW4 agreement; legacy/automatic-conflict tests; controller automatic mode is never silently disabled |
| Time/site | Native JD/UTC offset/site read at connect/live; explicit guarded writes | Physical startup population and independent checks; round-trip write/conversion tests in emulators |
| Offset Sync | CMCFG0 + CM, strict padded-reply normalization, true coordinates | Earlier real acceptance/padded-reply incident; complete new optical centering loop not claimed |
| Explicit native flip | Acceptance/refusal/uncertainty; opposite side + settled target/tracking completion | Protocol/real-driver TCP/PTY regressions + 180-s failure case; complete sky/capture recovery pending |
| Meridian telemetry | Limits, allowed side, stored target side, Gmte freshness/uncertainty | Native telemetry and tests; Gmte is tracking end, not guaranteed flip time |
| Enclosure startup | Five-second bounded discovery; initial empty definition is unknown, not closure | Captured actual sequence reproduced STOP on v0.27 and passes v0.28; hardware stationary/tracking startup passed |
| Enclosure loss/fault | Motion block; active-mount STOP and retained recovery hold | Emulator negative cases; no deliberate real closure on an unparked mount |
| External changes | Periodic supported settings/state refresh; complete external park can reconcile hold with valid proof | MW4-referenced hardware feedback plus TCP/PTY tests; static parked flag alone cannot clear interrupted recovery |
| Reconnect | Optional telemetry recovery holds motion; no blind command replay/capture restart | TCP/PTY tests and attended normal reconnect |
| Model building/editing | Deliberately delegated | Controller/MW4 tools; not a driver feature-parity claim |

The observing scope is connections, motion, tracking, guiding, parking, settings
needed for imaging, truthful state and automation integration. The prior PR's
v0.19 exclusions of serial/pulses/manual axes/settings are superseded; model
management and universal model/firmware coverage are not added by that change.
