#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Prometheus Astro
set -euo pipefail

driver=${1:?Usage: smoke_protocol.sh /path/to/indi_tyr_telescope}
port=${TYR_INDI_PROTOCOL_PORT:-17625}
device='Prometheus Astro Tyr'
work=$(mktemp -d)

cleanup() {
    status=$?
    if [[ $status -ne 0 ]]; then
        echo '--- protocol log ---' >&2
        cat "$work/protocol.log" >&2 2>/dev/null || true
        echo '--- indiserver log ---' >&2
        cat "$work/indiserver.log" >&2 2>/dev/null || true
    fi
    for file in server.pid emulator.pid; do
        [[ -f "$work/$file" ]] && kill "$(cat "$work/$file")" 2>/dev/null || true
    done
    rm -rf "$work"
    return $status
}
trap cleanup EXIT

python3 "$(dirname "$0")/protocol_emulator.py" "$work/port" "$work/protocol.log" &
echo $! >"$work/emulator.pid"
for _ in {1..30}; do [[ -s "$work/port" ]] && break; sleep 0.1; done
serial_port=$(cat "$work/port")

mkdir -p "$work/home"
HOME="$work/home" indiserver -u "/tmp/tyr-indi-protocol-$$" -p "$port" "$driver" >"$work/indiserver.log" 2>&1 &
echo $! >"$work/server.pid"
for _ in {1..30}; do
    indi_getprop -h localhost -p "$port" -t 1 "$device.CONNECTION.CONNECT" >/dev/null 2>&1 && break
    sleep 0.1
done

setp() { indi_setprop -h localhost -p "$port" "$1"; }
getp() { indi_getprop -h localhost -p "$port" -t 2 "$1"; }
expect() {
    local actual
    actual=$(getp "$1")
    [[ "$actual" == *"=$2" ]] || { echo "Expected $1=$2, received: $actual" >&2; exit 1; }
}

setp "$device.DEVICE_AUTO_SEARCH.INDI_DISABLED=On"
setp "$device.DEVICE_PORT.PORT=$serial_port"
setp "$device.CONNECTION.CONNECT=On"
sleep 5
expect "$device.CONNECTION.CONNECT" On
expect "$device.TYR_FIRMWARE.PRODUCT" 'Prometheus Astro Týr'

setp "$device.TYR_COMPENSATION.MODEL=On"
setp "$device.TYR_COMPENSATION_AXES.BOTH_AXES=On"
setp "$device.TYR_SLEW_SPEED.DEGREES_PER_SECOND=0.5"
setp "$device.ON_COORD_SET.SYNC=On"
setp "$device.EQUATORIAL_EOD_COORD.RA=3;DEC=40"
setp "$device.TELESCOPE_MOTION_WE.MOTION_WEST=On"
setp "$device.TELESCOPE_MOTION_WE.MOTION_WEST=Off"
setp "$device.TELESCOPE_TIMED_GUIDE_NS.TIMED_GUIDE_N=100;TIMED_GUIDE_S=0"
sleep 1

grep -qx 'To' "$work/protocol.log"
grep -qx 'T2' "$work/protocol.log"
grep -Eq '^SX92,74\.99999[0-9]$' "$work/protocol.log"
grep -qx 'CS' "$work/protocol.log"
grep -qx 'RA0.500000000' "$work/protocol.log"
grep -qx 'Mw' "$work/protocol.log"
grep -qx 'Qe' "$work/protocol.log"
grep -qx 'MGn00100' "$work/protocol.log"

echo "Tyr INDI pseudo-serial protocol test passed."
