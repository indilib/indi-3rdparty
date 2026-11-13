# v0.19 → v0.28 observing-driver update

| Revision | Main development change |
|---|---|
| 0.20 | Standard timed-guide/manual/tracking controls and native imaging settings foundation |
| 0.21 | Parked/closed-roof settings guard correction; independent guide timers; variable pulse durations/zero no-op |
| 0.22 | POSIX serial transport with saved disconnected-only device/baud selection, shared protocol/recovery semantics |
| 0.23 | Refraction widths/changed-field writes; settings rejection separated from transport loss and false unpark display |
| 0.24 | Read/verified direct-weather ownership controls; no silent automatic-weather disable |
| 0.25 | Five-second tracking request confirmation/release; safe Park cancellation of tracking-only progress |
| 0.26 | Configuration/preset indicator reconciliation without invented preset readback or command replay |
| 0.27 | Native time/site/settings startup/live reads; bounded enclosure discovery; complete external recovery-park observation |
| 0.28 | Real captured empty Ok roof definition treated as initial unknown; deadline/fault/later-loss protections preserved |

Published update also retains upstream build defaults/install conventions, adds
libnova for native JD display, registers all 40 tests and isolates test homes and
configuration to avoid operator park/log state. Historical prototype candidate
archives, site settings, binaries and private scripts are excluded.

See VALIDATION.md and doc/CAPABILITIES.md for versioned evidence. A local 40-test
pass is not a substitute for real optical commissioning or universal support.
