#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Ed Lee
"""Run one indi_ocs driver test against the mock OCS controller.

Usage: ocs_driver_test.py <path/to/indi_ocs> <test>"""
import sys
import time

# ctest runs this from the source tree, keep it free of __pycache__
sys.dont_write_bytecode = True

from ocs_mock import MockOCS, Rig, sleep_until, wait_for  # noqa: E402


class Session:
    def __init__(self, driver, **replies):
        self.mock = MockOCS(**replies)
        self.rig = Rig(driver, self.mock)

    def __enter__(self):
        return self.mock, self.rig

    def __exit__(self, *_):
        self.rig.close()
        self.mock.close()


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f"ok: {message}")


def test_connection(driver):
    with Session(driver, **{":IP#": "LX200#"}) as (_, rig):
        rig.set("CONNECTION_MODE.CONNECTION_TCP=On")
        rig.set(f"DEVICE_ADDRESS.ADDRESS;PORT=127.0.0.1;{rig.mock.port}")
        rig.set("CONNECTION.CONNECT=On")
        time.sleep(4)
        check(rig.get("CONNECTION.CONNECT") == "Off", "handshake with a non-OCS reply is rejected")

    with Session(driver) as (mock, rig):
        rig.connect()
        parameters = rig.values("WEATHER_PARAMETERS.*")
        check(len(parameters) == 6, f"six safety weather parameters defined, got {sorted(parameters)}")
        rig.disconnect()
        rig.connect()
        check(rig.values("WEATHER_PARAMETERS.*").keys() == parameters.keys(), "reconnecting does not duplicate parameters")

        since = time.monotonic()
        rig.set("THERMOSTAT_COOL_SETPOINT.THERMOSTAT_COOL_SETPOINT=22")
        wait_for(lambda: ":SC22#" in mock.commands(since), 5, "cool setpoint command")
        check(True, "thermostat cool setpoint sends :SC22#")

        since = time.monotonic()
        rig.set("POWER_DEVICE6.POWER_DEVICE6_ON=On")
        wait_for(lambda: ":SR6,ON#" in mock.commands(since), 5, "device 6 command")
        check(rig.get("POWER_DEVICE6._STATE") == "Ok", "power device 6 switches on")


def test_roof(driver):
    with Session(driver, **{":RSL#": "Error: Open over time#"}) as (mock, rig):
        rig.connect()
        check(rig.get("Status.ROOF_LAST_ERROR") == "Error: Open over time", "roof last error is shown")

        mock.replies[":RS#"] = "i,Error: Closed/opened limit sw on#"
        wait_for(lambda: rig.get("DOME_SHUTTER._STATE") == "Alert", 5, "shutter Alert")
        check(rig.alive(), "driver survives a long roof fault message")
        mock.replies[":RS#"] = "i,No Error#"
        wait_for(lambda: rig.get("DOME_SHUTTER._STATE") == "Idle", 5, "shutter Idle")
        check(True, "shutter returns to Idle when the fault clears")

        mock.replies[":RSL#"] = "No Error#"
        rig.disconnect()
        rig.connect()
        check(rig.get("Status.ROOF_LAST_ERROR") == "", "roof last error clears when OCS reports no error")

        mock.replies[":DS#"] = ""
        rig.set("ABS_DOME_POSITION.DOME_ABSOLUTE_POSITION=120")
        time.sleep(3)
        check(rig.alive(), "driver survives a dome goto with no reply")


def test_dome_only(driver):
    with Session(driver, **{":RS#": "0", ":RSL#": "0"}) as (mock, rig):
        rig.connect()
        since = time.monotonic()
        time.sleep(4)
        polled = [c for c in mock.commands(since) if c in (":RS#", ":RSL#")]
        check(not polled, "roof is not polled when OCS has no roof")
        check(rig.get("DOME_SHUTTER._STATE") is None, "no shutter control without a roof")
        check(rig.get("ABS_DOME_POSITION.DOME_ABSOLUTE_POSITION") is not None, "dome controls remain")


def test_relays(driver):
    with Session(driver) as (mock, rig):
        rig.connect()
        mock.reject_relays = True
        rig.set("POWER_DEVICE3.POWER_DEVICE3_ON=On")
        wait_for(lambda: rig.get("POWER_DEVICE3._STATE") == "Alert", 5, "rejected relay Alert")
        check(rig.get("POWER_DEVICE3.POWER_DEVICE3_OFF") == "On", "rejected relay command keeps the old state")
        mock.reject_relays = False
        rig.set("POWER_DEVICE3.POWER_DEVICE3_ON=On")
        wait_for(lambda: rig.get("POWER_DEVICE3._STATE") == "Ok", 5, "accepted relay Ok")
        check(rig.get("POWER_DEVICE3.POWER_DEVICE3_ON") == "On", "accepted relay command switches on")

        since = time.monotonic()
        rig.set("LIGHT_WRW.WRW_ON=On")
        wait_for(lambda: ":SR10,ON#" in mock.commands(since), 5, "light command")
        check(rig.get("LIGHT_WRW.WRW_ON") == "On", "light switches on")


def test_weather_rule(driver):
    with Session(driver) as (mock, rig):
        rig.connect()
        wind = lambda: rig.get("WEATHER_STATUS.WEATHER_WIND")
        value = lambda: rig.get("WEATHER_PARAMETERS.WEATHER_WIND")

        mock.replies[":Gw#"] = "nan#"
        start = rig.refresh_weather()
        sleep_until(start, 4)
        check(wind() == "Ok", "one bad reading is tolerated")
        mock.replies[":Gw#"] = "6#"
        sleep_until(start, 13)
        check(wind() == "Ok" and float(value()) == 6, "a valid reading at the re-poll keeps Ok")

        mock.replies[":Gw#"] = "nan#"
        start = rig.refresh_weather()
        sleep_until(start, 7)
        check(wind() == "Ok", "no Alert before the re-poll")
        sleep_until(start, 13)
        check(wind() == "Alert" and float(value()) == -9999, "two bad readings in a row raise Alert")

        mock.replies[":Gw#"] = "6#"
        start = rig.refresh_weather()
        wait_for(lambda: wind() == "Ok", 5, "recovery")
        check(True, "a valid reading clears the Alert")

        for state, expected in (("3#", "Ok"), ("2#", "Alert"), ("1#", "Alert")):
            mock.replies[":GR#"] = state
            rig.refresh_weather()
            time.sleep(2)
            check(rig.get("WEATHER_STATUS.WEATHER_RAIN") == expected, f"rain state {state[0]} is {expected}")


def test_weather_connect(driver):
    cases = (
        ("named sensor reading NaN is kept", {":Gw#": "nan#", ":GwN#": "Cup Anemometer#"}, True),
        ("unnamed sensor reading NaN is not fitted", {":Gw#": "nan#", ":GwN#": "None#"}, False),
        ("sensor reading NaN on firmware without names is not fitted", {":Gw#": "nan#"}, False),
        ("sensor reading 0 (no wind) is fitted", {":Gw#": "0#"}, True),
        ("unsupported command (unframed 0) is not fitted", {":Gw#": "0"}, False),
    )
    for label, replies, fitted in cases:
        with Session(driver, **replies) as (_, rig):
            rig.connect()
            check((rig.get("WEATHER_PARAMETERS.WEATHER_WIND") is not None) == fitted, label)


def test_comms_brief_silence(driver):
    # The mock reports roof delays of 1.5 s and 2.0 s, so silence is only treated as lost after 8.5 s
    with Session(driver) as (mock, rig):
        rig.connect()
        mock.silent = True
        time.sleep(5)
        mock.silent = False
        since = time.monotonic()
        time.sleep(3)
        sent = len(mock.commands(since))
        check(sent >= 3, f"a 5 second silence does not stop normal polling ({sent} commands in 3 s)")


def test_comms_lost(driver):
    with Session(driver) as (mock, rig):
        rig.connect()
        mock.silent = True
        time.sleep(15)
        since = time.monotonic()
        time.sleep(10)
        sent = len(mock.commands(since))
        check(sent <= 3, f"a silent controller is only tried every 5 seconds ({sent} commands in 10 s)")

        start = rig.refresh_weather()
        sleep_until(start, 13)
        check(rig.get("WEATHER_STATUS.WEATHER_WIND") == "Alert", "weather goes to Alert without replies")

        mock.silent = False
        time.sleep(7)
        rig.refresh_weather()
        wait_for(lambda: rig.get("WEATHER_STATUS.WEATHER_WIND") == "Ok", 10, "weather recovery")
        since = time.monotonic()
        time.sleep(4)
        check(len(mock.commands(since)) >= 3, "normal polling resumes when OCS replies again")


TESTS = {name[len("test_"):]: test for name, test in globals().items() if name.startswith("test_")}

if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[2] not in TESTS:
        sys.exit(f"usage: {sys.argv[0]} <indi_ocs> <{'|'.join(TESTS)}>")
    TESTS[sys.argv[2]](sys.argv[1])
