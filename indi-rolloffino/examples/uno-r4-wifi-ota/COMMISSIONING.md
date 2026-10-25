# Evidence — October 24, 2026

Reference install: UNO R4 WiFi, Arduino 4 Relays Shield, Blutezeit gate-opener
button inputs, RollOff ino, Pi Linux arm64. Board core 1.6.0, ArduinoOTA 1.1.1.
Firmware installed by USB then updated over LAN from
roof-wifi-20261021T010236Z-3f4494 to roof-wifi-20261024T143136Z-b11f3c.

| Check | Result / evidence |
|---|---|
| Actual MCU compile | 71,184 bytes program, 9,712 global RAM reported on Pi |
| OTA update | Uploader verified new running identity after explicit commit |
| Incomplete command | CommandTimeout at 1.09 seconds; VERSION then succeeded on same socket |
| OPEN/CLOSE/STOP pulses | User observed all three relays/light/click then release around two seconds, motor OFF |
| TCP client disconnect | User observed much shorter flash/click and release for all three, motor OFF |
| Partial upload | Only 64 bytes sent; ERROR:timeout at 10.18 seconds; same firmware and OTA DISABLED confirmed |
| INDI full roof cycle | Physical CLOSED → BETWEEN_LIMITS → OPEN and OPEN → BETWEEN_LIMITS → CLOSED; parked mount |

UNKNOWN roof status during commissioning was explained by RollOff ino being
disconnected; after reconnection physical readings were valid. Earlier failures
also involved damaged/loose sensor wiring and motor limit sensors. No claim that
the relay bug explains those incidents.

Host doubles reproduce the original *user sketch* failure: relay stays ON beyond
3 seconds after disconnect at 100 ms. Candidate tests cover 35 sketch scenarios
and 18 receiver scenarios. Client tests include READY/COMMIT and single-command-
session behavior. These are not real flash power-interruption tests.

Not yet established: powered mid-travel Stop with this revision, silent Wi-Fi
loss, Wi-Fi automatic reconnection, hardware watchdog, power loss during apply,
extended unattended operation and other roof/mount combinations. USB-independent
power removal/retest should be recorded explicitly at the next commissioning.

## Publication-tool validation on the Pi

A fresh working directory was copied from the publication example, with existing
private Wi-Fi settings/token imported rather than re-created. The generalized
standard INDI park check passed against the connected 10Micron Native Motion
device. The generalized uploader then completed maintenance entry, transfer and
explicit commit, and verified running identity roof-wifi-20261024T154550Z-23cac0
(previously roof-wifi-20261024T143136Z-b11f3c).

After RollOff ino reconnection, reported physical inputs were OPENED=Idle,
CLOSED=Ok, MOVING=Idle; LOCK and AUXILIARY were Idle. An initial status timeout
cleared after disconnecting the competing RollOff ino command client.

All 69 host checks/scenarios also passed from the submission copy on the Pi.
This validates the publication uploader end to end on the reference installation.
It is not a complete first-time-user walkthrough: dependency installation and
initial USB setup were not repeated from a blank environment, and existing
private settings were reused. Other mount/roof combinations remain untested.
