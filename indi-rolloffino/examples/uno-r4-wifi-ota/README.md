# UNO R4 WiFi roof controller with bounded relay pulses and OTA

An additional RollOff ino controller example for a motor controller with separate
OPEN, CLOSE and STOP dry-contact inputs. The motor controller must enforce travel
limits and obstruction protection. This sketch pulses buttons; it does not drive
a motor or stop travel when a pulse expires. No INDI driver changes are required.

Start with [SETUP.md](SETUP.md). [COMMISSIONING.md](COMMISSIONING.md) separates
hardware evidence from host tests. [PROTOCOL.md](PROTOCOL.md) describes updates.

Firmware is the same source commissioned on October 24, 2026. Publication tools
now accept an explicit controller address and standard INDI telescope park check;
these generalized tool changes have host tests but still need an end-to-end Pi
run. Do not replace a commissioned installation merely to publish this example.

Supported reference hardware: Arduino UNO R4 WiFi, official Arduino 4 Relays
Shield, active-low normally-open end switches. This is not a drop-in replacement
for single-button openers, direct motor drivers, other relay polarities, or
existing examples using A0/A1 limit inputs. This example uses D2/D3.

Commands use TCP 7624 on the Arduino; custom OTA uses TCP 7625. USB is for initial
programming, not a serial roof-command interface. No Arduino IDE OTA discovery.
Only one roof-command client is supported at a time.

Run host checks (Python 3.10+, g++ with C++17):

```bash
python3 tests/run.py
python3 tests/test_client.py
python3 test_session.py
python3 test_standard_park.py
```

`baseline/Rolloff.ino` is the original user sketch retained solely to reproduce
the stuck-relay bug; it is not the upstream Wi-Fi example and must not be flashed.
Test stub headers belong only under tests/stubs; never copy them into Rolloff.

## Limits

Trusted LAN only. Token and binary travel without TLS; CRC32 is not a signature.
Do not forward ports to the internet. Maintenance requires closed/not-open limits,
inactive relays and authentication. Uploader also requires an explicitly named,
connected, parked INDI mount and an attended invocation. It does not park anything.
Standard INDI park reports are only as reliable as that mount driver; independently
confirm clearance. There is no requirement for a particular mount manufacturer.

Staging does not apply until a separate COMMIT. Power loss during flash copy may
require USB recovery: no dual-image rollback. Software deadlines require the MCU
and library calls to return; no hardware watchdog or Wi-Fi reconnection is added.
Disconnect releases relay contacts, not a physical emergency stop. Events are a
16-entry volatile ring cleared on reboot. LOCKED/AUXSTATE return OFF and do not
represent wired lock/aux sensors. Extra driver Actions are not advertised.

Upstream placement is proposed, not maintainer-approved. Repository code and
dependency licensing must be confirmed before submission; see PUSH.md at the
release root. Do not claim our private baseline bug was reproduced upstream.
