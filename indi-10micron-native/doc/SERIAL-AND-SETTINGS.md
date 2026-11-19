# Serial transport and native settings

Select NATIVE_TRANSPORT=Serial while disconnected. NATIVE_SERIAL holds device
and baud; default /dev/ttyUSB0 at 9600. Prefer a stable /dev/serial/by-id path.
Supported rates: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200. Raw 8N1,
no software or hardware flow control. Match controller baud explicitly: no
command changes the controller baud, and no automatic fallback changes transport.
Serial selection/device/baud and TCP endpoint persist through Save Configuration.
Connected edits are rejected without replacing applied values.

TCP and serial share locking, command-specific response shapes, deadlines,
partial-write handling, state parsing and motion/recovery policy. Framing/timeout
uncertainty closes communication rather than replaying commands. Identification,
combined state, controls and actual transport endpoint are published. Inactive
connection fields are neutral, not false fault indicators. DNS setup is not
claimed as a universal real-time deadline; Windows-native is not implemented.

Physical reference: GM1000HPS firmware 3.4, Prolific USB serial adapter at 9600.
Connection, guide-rate write/readback/restore and manual refraction behavior were
attended. Initial temperature edits under automatic direct-weather ownership
were rejected/overwritten; this was not evidence of a broken serial link.
After the user selected manual mode, INDI temperature editing worked. The driver
now exposes weather ownership, rejects conflicting manual input clearly and
never silently disables automatic controller weather updates. The >=3.4 mode
query gate is conservative compatibility policy, not a claimed introduction date.

Only changed refraction fields are written, with correct native widths. A
complete settings rejection or readback mismatch stays local, preserves real
park/connection state and displays actual readback. Genuine transport uncertainty
still invalidates state. Omitting a client pressure edit is not a numeric zero
sentinel; zero is not treated as preserve-current. Independent pressure-only
physical write/restore remains an explicitly uncompleted evidence item.

Guide rate, refraction, supported weather mode, site and UTC offset populate from
native reads at connection and periodically when no driver operation/manual move
or guide pulse is active. Clock display follows native Ginfo JD. Native offset
sign is converted to INDI convention, including fractional offsets. Link loss
invalidates those readbacks. Time/site writes are explicit and guarded; startup
does not push host settings. Settings failure must not report physical unpark.

PTY regressions cover raw termios, fragmentation, TCP-equivalent controls,
calibration-style variable pulses, settings rejection, recovery, flips and
startup. They do not certify USB wiring, electrical faults, controller baud at
untested rates or image displacement. Serial commissioning does not require MW4
itself to use serial; the user independently checked the controller via MW4 TCP.
