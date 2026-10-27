# 10Micron native observing driver

## Device and scope

Use with a controller exposing documented combined Ginfo state (firmware
>=2.14.9). GM1000HPS/3.4 is the hardware reference. See
[capabilities and their evidence](CAPABILITIES.md); other models are not declared
commissioned by software tests.

## Installation and configuration

Follow [SETUP.md](../SETUP.md) for dependencies, isolated build/install and profile
selection. Both binary and registry XML must be visible to the actual indiserver.
NATIVE_TRANSPORT selects TCP or Serial while disconnected. NATIVE_ENDPOINT is
the controller host/port, not the INDI server endpoint. NATIVE_SERIAL is the
serial device/baud. Save explicitly and verify DRIVER_INFO=0.28 after connection.
See [serial/settings details](SERIAL-AND-SETTINGS.md).

For open-air use leave Dome ignored. For an enclosure configure exact Active
Dome and Dome locks, reciprocal mount clearance, and independent physical checks.
No private roof light name, arming switch or custom startup script is required.
A closed enclosure correctly blocks new motion even when the mount is parked.

## Observing

Use ordinary INDI Goto/Park/Unpark, tracking and offset Sync. Standard Abort halts
slew and may retain tracking; Stop Including Tracking is separate. Sync asserts
controller-wide CMCFG0 on each request and leaves that mode selected. Coordinate
other writers; separate MW4 connections do not isolate target registers/settings.

Timed-guide durations must be whole milliseconds, 0..9999. Zero is a no-op.
RA and DEC timers are independent; opposing nonzero directions in the same
request are rejected. A Busy-to-Idle pulse lifecycle is not an optical calibration
or settling measurement. Measure actual displacement before choosing dither
or calibration durations. Guide rate is explicitly read and writable with guards.

Startup reads current native state/settings; it does not enable tracking, unpark,
slew or restore controller settings just to fill UI fields. Unpark need not enable
tracking before the first slew on this reference mount. An uncompleted Tracking
On request can report bounded failure without blocking later safe operations.
Preset indicators stay neutral when no unambiguous getter exists.

## Logging and diagnosis

Retain INDI messages, Mount/Capture logs and ordered raw vectors beginning before
connection. Note version, transport, controller firmware, actual status/settling,
standard park/tracking selection, roof connection/park/shutter state and command
result. Distinguish a real STOP diagnostic from a blocked request's explanatory
text. Do not override valid controller/MW4 position evidence with a stale UI flag.
Do not distribute controller credentials, private site configuration or API keys.

See [startup/enclosure behavior](STARTUP-AND-ENCLOSURES.md),
[architecture/property map](../ARCHITECTURE.md), [validation](../VALIDATION.md)
and [original flip-stall evidence](LEGACY-FLIP-INCIDENT.md).
