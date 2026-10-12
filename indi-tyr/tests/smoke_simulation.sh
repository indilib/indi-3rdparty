#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Prometheus Astro
set -euo pipefail

driver=${1:?Usage: smoke_simulation.sh /path/to/indi_tyr_telescope}
port=${TYR_INDI_TEST_PORT:-17624}
device='Prometheus Astro Tyr'
work=$(mktemp -d)

cleanup() {
    if [[ -f "$work/pid" ]]; then
        kill "$(cat "$work/pid")" 2>/dev/null || true
    fi
    rm -rf "$work"
}
trap cleanup EXIT

mkdir -p "$work/home"
HOME="$work/home" indiserver -u "/tmp/tyr-indi-test-$$" -p "$port" "$driver" >"$work/indiserver.log" 2>&1 &
echo $! >"$work/pid"

for _ in {1..30}; do
    if indi_getprop -h localhost -p "$port" -t 1 "$device.CONNECTION.CONNECT" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

set_property() { indi_setprop -h localhost -p "$port" "$1"; }
get_property() { indi_getprop -h localhost -p "$port" -t 2 "$1"; }
expect() {
    local property=$1 expected=$2
    local actual
    actual=$(get_property "$property")
    if [[ "$actual" != *"=$expected" ]]; then
        echo "Expected $property=$expected, received: $actual" >&2
        cat "$work/indiserver.log" >&2
        exit 1
    fi
}

set_property "$device.SIMULATION.ENABLE=On"
set_property "$device.CONNECTION.CONNECT=On"
expect "$device.CONNECTION.CONNECT" On
expect "$device.TYR_FIRMWARE.PRODUCT" 'Prometheus Astro Tyr (simulation)'

set_property "$device.TYR_SLEW_SPEED.DEGREES_PER_SECOND=2"
expect "$device.TYR_SLEW_SPEED.DEGREES_PER_SECOND" 2
set_property "$device.TYR_COMPENSATION.MODEL=On"
expect "$device.TYR_COMPENSATION.MODEL" On

set_property "$device.ON_COORD_SET.SYNC=On"
set_property "$device.EQUATORIAL_EOD_COORD.RA=2;DEC=45"
sleep 1.2
expect "$device.EQUATORIAL_EOD_COORD.RA" 2
expect "$device.EQUATORIAL_EOD_COORD.DEC" 45

set_property "$device.ON_COORD_SET.SLEW=On"
set_property "$device.EQUATORIAL_EOD_COORD.RA=2.5;DEC=45.5"
sleep 2
expect "$device.EQUATORIAL_EOD_COORD.RA" 2.5
expect "$device.EQUATORIAL_EOD_COORD.DEC" 45.5

set_property "$device.TELESCOPE_TRACK_STATE.TRACK_ON=On"
expect "$device.TELESCOPE_TRACK_STATE.TRACK_ON" On
set_property "$device.TELESCOPE_HOME.SET=On"

echo "Tyr INDI simulation smoke test passed."
