# Startup reconciliation and enclosure compatibility

v0.26 could STOP an already-tracking mount because initial roof evidence had not
yet arrived. v0.27 added bounded discovery, but a real capture showed RollOff ino
initially defines DOME_PARK as Ok with both PARK/UNPARK Off. It then reports the
current open position. Treating that empty definition as an explicit unsafe
observation still caused a startup STOP.

The captured sequence is retained as a small standard-property fixture. Replaying
it on an emulated tracking mount fails on unchanged v0.27 and passes v0.28 over
TCP and serial PTY. Only an initial empty Ok definition gets bounded discovery;
empty updates, Busy/Alert, contradictory selections, real closure, later loss and
timeout retain protection. Repeated unknown definitions do not extend the original
five-second deadline. New movement is not allowed until valid connected/open
proof is present. Existing current open reports suffice, without an open edge.

Attended v0.28 checks on October 25 passed stationary startup, already-tracking
mount reconnect and a complete profile restart with mount and roof drivers
initializing together. All 59 valid native samples in the latter remained status
0/tracking 1/unparked/no settling. The real initial empty roof definition was
captured; interlock went Busy to Ok without Alert or STOP. Park, physical park
confirmation, normal roof closure and parked/closed profile restart also passed.

A separate compatibility problem was observed: Ekos Observatory weather alert
handling requested roof Park after its configured/default thirty-second delay.
The roof correctly refused while mount-lock was active but left DOME_PARK Alert
with UNPARK On, despite open sensors. That later request is not the original
startup trigger and was not a human close request. v0.28 must not simply ignore
that Alert. The separate local RollOff ino correction records rejected commands
separately and preserves measured open/Ok only after fresh validated limits; real
closed/unknown/faulted evidence remains blocking. Paired hardware checks passed
through the naturally occurring rejected close without stopping tracking.

That roof patch is outside this telescope PR. The paired hardware result does
not certify every roof driver's reporting or make custom sensor lights an
alternative permission source. Current standard state must be trustworthy.

To stop a profile while intentionally preserving external tracking, disconnect
the mount first. Disconnecting the roof first removes permission from a connected
active driver and may correctly trigger STOP. Never bypass locks to avoid it.
Compare actual mount position against controller/MW4 and physical clearance;
no software Park flag alone is a roof-clearance guarantee.
