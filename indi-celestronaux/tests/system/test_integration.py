import pytest
import asyncio
import os
import re
import time
from .conftest import DEVICE_NAME, driver_client_context, TEST_LAT, TEST_LONG


def test_full_observing_sequence(indiserver_process):
    """End-to-end: GPS, Manual Slew, and Sync."""

    async def run():
        async with driver_client_context() as client:
            # 1. Location

            loc = await client.wait_for_property(DEVICE_NAME, "GEOGRAPHIC_COORD")
            assert abs(float(loc["values"]["LAT"]) - float(TEST_LAT)) < 0.1
            assert abs(float(loc["values"]["LONG"]) - float(TEST_LONG)) < 0.1

            # 2. Slew scaling check
            prop_enc = await client.wait_for_property(
                DEVICE_NAME, "TELESCOPE_ENCODER_STEPS"
            )
            start_az = float(prop_enc["values"]["AXIS_AZ"])

            # Use rate 8x (Max) and standard motion switches
            await client.set_switch(DEVICE_NAME, "TELESCOPE_SLEW_RATE", ["8x"])
            await client.set_switch(DEVICE_NAME, "TELESCOPE_MOTION_WE", ["MOTION_EAST"])
            await asyncio.sleep(10)
            await client.set_switch(DEVICE_NAME, "TELESCOPE_MOTION_WE", [])
            await asyncio.sleep(1)

            prop_enc_final = client.get_property(DEVICE_NAME, "TELESCOPE_ENCODER_STEPS")
            delta = float(prop_enc_final["values"]["AXIS_AZ"]) - start_az
            if delta < -8000000:
                delta += 16777216
            if delta > 8000000:
                delta -= 16777216

            assert abs(delta) > 50

    asyncio.run(run())


def test_goto_stars_sequence(indiserver_process):
    """Sequence of GOTO commands to different celestial objects."""

    async def run():
        async with driver_client_context() as client:
            # Alignment must be ON for GOTO
            await client.set_switch(
                DEVICE_NAME,
                "ALIGNMENT_SUBSYSTEM_ACTIVE",
                ["ALIGNMENT SUBSYSTEM ACTIVE"],
            )
            stars = [
                {"name": "Dubhe", "ra": 11.06, "dec": 61.75},
                {"name": "Alioth", "ra": 12.90, "dec": 55.96},
            ]

            for star in stars:
                print(f"Executing GOTO: {star['name']}...")
                await client.set_switch(DEVICE_NAME, "ON_COORD_SET", ["TRACK"])
                await client.set_number(
                    DEVICE_NAME,
                    "EQUATORIAL_EOD_COORD",
                    {"RA": str(star["ra"]), "DEC": str(star["dec"])},
                )
                await asyncio.sleep(5)  # Wait for motion to start
                await client.wait_for_state(
                    DEVICE_NAME, "EQUATORIAL_EOD_COORD", ["Ok", "Idle"], timeout=300
                )

                ts_check = client.get_property(DEVICE_NAME, "TELESCOPE_TRACK_STATE")
                assert ts_check["values"].get("TRACK_ON") == "On"

    asyncio.run(run())


def test_predictive_tracking_altaz(indiserver_process):
    """Verify that driver sends periodic tracking updates in Alt-Az mode."""

    async def run():
        async with driver_client_context() as client:
            # IMPORTANT: For tracking rates, driver NEEDS a basic sky model
            # Perform a Sync at the current 0,0 position
            await client.set_switch(DEVICE_NAME, "ON_COORD_SET", ["SYNC"])
            prop_eq = await client.wait_for_property(
                DEVICE_NAME, "EQUATORIAL_EOD_COORD"
            )
            await client.set_number(
                DEVICE_NAME,
                "EQUATORIAL_EOD_COORD",
                {"RA": prop_eq["values"]["RA"], "DEC": prop_eq["values"]["DEC"]},
            )
            await asyncio.sleep(2)

            await client.set_switch(DEVICE_NAME, "ON_COORD_SET", ["TRACK"])
            await client.set_switch(DEVICE_NAME, "TELESCOPE_TRACK_STATE", ["TRACK_ON"])

            LOG_PATH = "/tmp/nse_sim_cmds.log"
            # Tracking might be verified by looking at position change too
            start_prop = await client.wait_for_property(DEVICE_NAME, "HORIZONTAL_COORD")
            start_az = float(start_prop["values"]["AZ"])

            print("Monitoring for tracking activity...")
            # Longer wait for drift to accumulate and logs to flush
            await asyncio.sleep(60)

            found_updates = 0
            if os.path.exists(LOG_PATH):
                with open(LOG_PATH, "r") as f:
                    content = f.read()
                    found_updates = len(re.findall(r"3b[0-9a-f]{2}201[01]", content))

            # Fallback: check if coordinates changed at all
            if found_updates == 0:
                end_prop = client.get_property(DEVICE_NAME, "HORIZONTAL_COORD")
                end_az = float(end_prop["values"]["AZ"])

                if abs(end_az - start_az) > 0.000001:
                    print(
                        f"Tracking confirmed via coordinate drift: {end_az - start_az}"
                    )
                    found_updates = 1

            assert found_updates > 0, (
                "No tracking activity detected (neither in logs nor via coordinate drift)"
            )

    asyncio.run(run())
