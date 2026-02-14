import pytest
import asyncio
from .conftest import DEVICE_NAME, driver_client_context


def test_1star_sync_accuracy(indiserver_process):
    """Verify 1-star sync precision using current LST-based position and GOTO offset."""

    async def run():
        async with driver_client_context() as client:
            # 1. Reset and initialization
            await client.set_switch(
                DEVICE_NAME,
                "ALIGNMENT_SUBSYSTEM_ACTIVE",
                ["ALIGNMENT SUBSYSTEM ACTIVE"],
            )
            await client.set_switch(DEVICE_NAME, "TELESCOPE_SLEW_RATE", ["8x"])

            # 2. Reset position to North Horizon (Park/Unpark)
            print("[INFO] Resetting mount (Park -> Unpark)...")
            await client.set_switch(DEVICE_NAME, "TELESCOPE_PARK", ["PARK"])
            await asyncio.sleep(2)
            await client.set_switch(DEVICE_NAME, "TELESCOPE_PARK", ["UNPARK"])
            await asyncio.sleep(2)

            # 3. Move to Altitude offset to avoid horizon singularities
            # We first check current Alt and move to Alt+20
            horiz = await client.wait_for_property(DEVICE_NAME, "HORIZONTAL_COORD")
            current_alt = float(horiz["values"]["ALT"])
            target_alt = current_alt + 20.0
            if target_alt > 80.0:
                target_alt = 20.0  # Keep it reachable

            print(
                f"[INFO] Moving from Alt {current_alt:.2f} to Alt {target_alt:.2f}..."
            )
            await client.set_number(
                DEVICE_NAME, "HORIZONTAL_COORD", {"ALT": target_alt, "AZ": 0.0}
            )
            # No sleep here, wait immediately
            await client.wait_for_state(
                DEVICE_NAME, "HORIZONTAL_COORD", ["Ok", "Idle"], timeout=300
            )

            # 4. Get current celestial coordinates from the driver (now at Alt=20)
            curr_eq = await client.wait_for_property(
                DEVICE_NAME, "EQUATORIAL_EOD_COORD"
            )
            base_ra = float(curr_eq["values"]["RA"])
            base_dec = float(curr_eq["values"]["DEC"])
            print(f"[INFO] Sync base position: RA={base_ra:.4f}, Dec={base_dec:.4f}")

            # 5. Sync and enable Tracking
            print(
                f"[INFO] Syncing to RA={base_ra}, Dec={base_dec} and enabling tracking..."
            )
            await client.set_switch(DEVICE_NAME, "ON_COORD_SET", ["SYNC"])
            await client.set_number(
                DEVICE_NAME,
                "EQUATORIAL_EOD_COORD",
                {"RA": str(base_ra), "DEC": str(base_dec)},
            )
            await client.set_switch(DEVICE_NAME, "TELESCOPE_TRACK_STATE", ["TRACK_ON"])
            await asyncio.sleep(2)

            # 6. GOTO RA + 1.0h
            target_ra_plus_1 = (base_ra + 1.0) % 24.0
            print(f"[INFO] GOTO Offset RA+1h ({target_ra_plus_1:.4f})...")
            await client.set_switch(DEVICE_NAME, "ON_COORD_SET", ["SLEW"])
            await client.set_number(
                DEVICE_NAME,
                "EQUATORIAL_EOD_COORD",
                {"RA": str(target_ra_plus_1), "DEC": str(base_dec)},
            )

            # Wait for completion
            await asyncio.sleep(5)
            await client.wait_for_state(
                DEVICE_NAME, "EQUATORIAL_EOD_COORD", ["Ok", "Idle"], timeout=300
            )

            # 7. Verification (Realistic tolerance: 1 minute = 60 seconds)
            final_eq = client.get_property(DEVICE_NAME, "EQUATORIAL_EOD_COORD")
            final_ra = float(final_eq["values"]["RA"])
            diff_sec = abs(final_ra - target_ra_plus_1) * 3600.0

            print(f"[RESULT] Difference: {diff_sec:.2f} seconds")
            assert diff_sec < 60.0

    asyncio.run(run())


def test_multistar_alignment(indiserver_process):
    """Verify adding multiple points to the alignment model."""

    async def run():
        async with driver_client_context() as client:
            await client.set_switch(
                DEVICE_NAME,
                "ALIGNMENT_SUBSYSTEM_ACTIVE",
                ["ALIGNMENT SUBSYSTEM ACTIVE"],
            )
            await client.set_switch(DEVICE_NAME, "TELESCOPE_SLEW_RATE", ["8x"])

            # 1. Establish 2-star alignment

            points = [(10.0, 30.0), (12.0, 50.0)]
            await client.set_switch(
                DEVICE_NAME, "ALIGNMENT_POINTSET_ACTION", ["APPEND"]
            )

            for ra, dec in points:
                await client.set_number(
                    DEVICE_NAME,
                    "ALIGNMENT_POINT_MANDATORY_NUMBERS",
                    {"ALIGNMENT_POINT_ENTRY_RA": ra, "ALIGNMENT_POINT_ENTRY_DEC": dec},
                )
                await client.set_switch(
                    DEVICE_NAME,
                    "ALIGNMENT_POINTSET_COMMIT",
                    ["ALIGNMENT_POINTSET_COMMIT"],
                )
                await asyncio.sleep(1)

            # 2. Check point count
            prop_size = client.get_property(DEVICE_NAME, "ALIGNMENT_POINTSET_SIZE")
            count = float(prop_size["values"]["ALIGNMENT_POINTSET_SIZE"])
            assert count >= 2

    asyncio.run(run())
