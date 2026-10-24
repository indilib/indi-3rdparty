# Reproducible setup — Linux / Raspberry Pi

Execute commands from this example directory. Expected results and stop conditions
are part of each step. Windows-native setup is not validated; use the Pi terminal.

## 1. Hardware and wiring

Required: UNO R4 WiFi (not R4 Minima), official Arduino 4 Relays Shield, stable
board-compatible power, USB data cable, 2.4 GHz compatible Wi-Fi, two independent
NO dry-contact position switches, and a motor controller with separate momentary
OPEN/CLOSE/STOP inputs and its own working limits/obstruction protection.

| Function | Arduino pin | Shield relay / wiring |
|---|---|---|
| OPEN | D4 | Relay 1, NO and COM across OPEN button input |
| CLOSE | D7 | Relay 2, NO and COM across CLOSE button input |
| STOP | D8 | Relay 3, NO and COM across STOP button input |
| Unused | D12 | Relay 4, leave contacts disconnected |
| Fully open switch | D2 | NO switch between D2 and Arduino GND |
| Fully closed switch | D3 | NO switch between D3 and Arduino GND |

INPUT_PULLUP makes a closed switch LOW/active. No external pull-down resistor is
needed for this wiring. Never connect motor voltage to D2/D3. Relay dry contacts
do not require sharing Arduino ground with the motor circuit. Join relay COMs to
the motor controller's button common only if its manual specifies that common;
do not assume it is Arduino GND. This guide does not cover mains wiring.

Shield outputs are active HIGH in this sketch (`INVERT_RELAY_LOGIC=true`). Confirm
the actual shield/polarity before connecting motor controls. Do not change polarity
solely because a terminal is labeled NO: coil drive and contact choice differ.

Before installation: stop Scheduler, independently park the mount, close the roof,
and switch off motor power. Leave Arduino power on for output tests. A broken
NO sensor wire looks inactive, so these inputs alone cannot diagnose every fault.
Check brackets, magnets, wiring and the motor's own limit sensors too.

## 2. Install tools

Install Python 3.10+, g++ and Arduino CLI. On Debian/Pi:

```bash
sudo apt update
sudo apt install python3 g++ curl
mkdir -p "$HOME/.local/bin"
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh -o /tmp/install-arduino-cli.sh
BINDIR="$HOME/.local/bin" sh /tmp/install-arduino-cli.sh 1.5.1
export PATH="$HOME/.local/bin:$PATH"
arduino-cli version
arduino-cli core update-index
arduino-cli core install arduino:renesas_uno@1.6.0
arduino-cli lib install ArduinoOTA@1.1.1
arduino-cli core list
arduino-cli lib list
```

Expected: core 1.6.0 and ArduinoOTA 1.1.1. WiFiS3 comes with the core. If versions
differ or installation fails, stop. Do not substitute test stubs. Official sources:
https://arduino.github.io/arduino-cli/ and https://github.com/JAndrassy/ArduinoOTA.
The installer downloads executable tools; inspect it first if required locally.

## 3. Private configuration and build

```bash
cp Rolloff/secrets.h.example Rolloff/secrets.h
nano Rolloff/secrets.h
python3 prepare.py
python3 build.py
```

Set WIFI_SSID and WIFI_PASSWORD inside the quoted C strings; escape backslashes
and double quotes if present. Do not leave example values. prepare.py creates a
random token and build ID, and preserves an existing token on later runs.
Expected: compiled identity and build/firmware.json; no upload performed.
Core version is checked by build.py. Confirm the compiler selected ArduinoOTA
1.1.1 in its output. Stop on any compile error. Reference build was 71,184 bytes
flash and 9,712 bytes globals; toolchain changes may affect size.

Private files: Rolloff/secrets.h, ota-token.txt, build/, build-identity.json.
Binaries contain credentials. Keep a private backup of source/settings/token for
recovery; never publish prepared directories or binaries. .gitignore is supplied,
but is not a substitute for reviewing staged files.

## 4. First USB installation

Keep motor power OFF, Scheduler stopped and RollOff ino disconnected. Identify
the UNO, not a focuser/camera:

```bash
arduino-cli board list
ls -l /dev/serial/by-id/
read -r -p 'Exact UNO USB device path: ' ROOF_USB
arduino-cli upload --fqbn arduino:renesas_uno:unor4wifi --port "$ROOF_USB" --input-dir build
```

Expected: upload success. If access denied, use the core's documented Linux
post_install.sh permissions procedure, reconnect USB, and retry the identified
port. Never guess an Unknown ttyUSB device. Preserve a known USB recovery path.

Find the board's DHCP address in your router (or serial startup at 115200 baud),
reserve its lease, then close the serial monitor. Set your address below:

```bash
read -r -p 'Controller LAN IP address: ' ROOF_HOST
python3 upload-wifi.py --host "$ROOF_HOST" --status
python3 check-controller.py --host "$ROOF_HOST" --parser-timeout
```

Expected: your compiled identity, OTA DISABLED, CLOSED ON, OPENED OFF and roughly
one-second CommandTimeout with successful subsequent VERSION. If the physical
roof position differs, do not enable OTA. Test each limit switch mechanically
with motor off and verify the corresponding reading changes. Both active is a
wiring/position fault. Disconnect USB while retaining normal power and repeat
VERSION to confirm headless operation. Do not disconnect the board's sole supply.

## 5. Output checks with motor OFF

Run a controlled one-at-a-time relay test using the helper below. Observe relay
LED and click; each releases in about two seconds. --disconnect closes the command
socket 0.1 seconds after acknowledgment: each then releases promptly. No automatic
motion retry is allowed. --delay 60 is optional if walking to the controller.

```bash
python3 test-relays.py --host "$ROOF_HOST" --motor-off
python3 test-relays.py --host "$ROOF_HOST" --motor-off --disconnect
```

Stop if any output stays active or more than one actuates. Do not restore motor
power until outputs are inactive. LEDs/clicks show coil behavior; use a meter if
contact release itself is in doubt.

## 6. Configure INDI and verify ordinary movement

Start your existing INDI/Ekos profile with RollOff ino under Domes. In its
Connection settings select network/TCP and enter the Arduino LAN address and
port 7624, then Connect and Save configuration. UI wording varies with version.
The connection is TCP, not UDP or serial. Configure the roof/mount lock policies
for your equipment. Mount clearance remains an installation responsibility.

Two separate endpoints can use 7624: INDI_HOST:7624 is indiserver; ROOF_HOST:7624
is the Arduino. Never put the Pi address into the Arduino host field by mistake.

Read physical states through INDI (replace device name if renamed):

```bash
indi_getprop -t 5 'RollOff ino.CONNECTION.*' 'RollOff ino.ROOF STATUS.*' 'RollOff ino.DOME_PARK.*'
```

Expected: connected, OPENED Idle, CLOSED Ok and MOVING Idle for a closed roof.
No properties/UNKNOWN usually means driver disconnected or unavailable; contradictory
limits require inspection. Logical DOME_PARK alone is not physical-limit proof.

With mount independently parked and roof path clear, restore motor power and
supervise Open, Stop during travel, then complete Open and Close. Wait for each
action before the next. Verify actual roof stops and physical limits agree.
Firmware commissioning recorded a complete Open/Close; powered mid-travel Stop
is still a required site commissioning check. If no motion occurs, check motor
power, obstruction, mechanics, motor limit sensors and relay contacts before
assuming a network error. Do not increase force or repeatedly retry blindly.

## 7. Subsequent Wi-Fi update

Stop Scheduler; park mount independently; close roof. Keep mount driver connected
but disconnect RollOff ino (one command client). For first update commissioning,
keep motor OFF. Later updates still require an idle, closed, attended roof.

```bash
read -r -p 'Exact INDI telescope device name: ' ROOF_MOUNT
python3 upload-wifi.py --host "$ROOF_HOST" --status
read -r -p 'Installed build ID (without ACK wrapper): ' ROOF_OLD_BUILD
python3 prepare.py
python3 build.py
python3 upload-wifi.py --host "$ROOF_HOST" --mount "$ROOF_MOUNT" --attended --expected-current "$ROOF_OLD_BUILD"
```

For remote indiserver add --indi-host HOST --indi-port PORT. Requires indi_getprop
on the upload computer. The generic check requires connected + PARK On + UNPARK
Off + park vector Ok. Drivers without these properties are refused; no automatic
bypass. This publication client generalization needs live commissioning. Chris's
successful OTA used the earlier native-specific check and the same transfer code.

Expected: VERIFIED installed firmware with the NEW build ID. If reset/timeout
occurs, query --status first; do not blindly retry COMMIT. A new ID proves rebooted
new image, not merely accepted bytes. If the old ID remains, diagnose before
retrying. If unreachable, restore via identified USB using preserved source/token.
Never remove power during apply. Reconnect RollOff ino and verify limits before
resuming. Repeat supervised movement after initial commissioning.

With motor still OFF and RollOff ino disconnected, test an incomplete transfer:

```bash
python3 interrupted-upload.py --host "$ROOF_HOST" --mount "$ROOF_MOUNT" --motor-off --attended
```

Expected: ERROR:timeout around ten seconds, then PASS with unchanged firmware
and OTA disabled. No COMMIT is sent. A failure is a stop condition: read status
and resolve it before returning to motor-enabled commissioning.

## 8. Common failures

| Symptom | Check / action |
|---|---|
| Command connection refused/reset | Correct LAN address, Wi-Fi/power, competing RollOff ino or other client; disconnect competing clients |
| INDI park check refused | Exact device name, INDI host/port, connected completed park/Ok; independent clearance |
| OTA maintenance refused | Closed ON, opened OFF, inactive outputs, matching token |
| Firmware manifest mismatch | Rebuild matching source/identity; do not edit manifest/hash to bypass |
| ERROR:timeout | Incomplete upload; verify old identity; maintenance cancels explicitly or expires |
| Wrong identity after apply | Read status, preserve logs; no automatic retry; USB recovery if needed |
| Roof won't move but connected | Motor power/controller limits, loose sensors, obstruction, contacts; ACK is not movement |
| One end switch never changes | Mounting/magnet/wire and D2/D3; do not infer position from DOME_PARK |

Record firmware ID, board/core/library versions, relay/limit checks and physical
Open/Stop/Close results before unattended use. Keep your normal external roof logs.
