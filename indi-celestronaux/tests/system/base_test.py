import unittest
import asyncio
import os
from .indi_client import INDIClient

DEVICE_NAME = "Celestron AUX"
SIM_PORT = 2000
INDI_PORT = 7624


class SystemTestCase(unittest.IsolatedAsyncioTestCase):
    """
    Base class for system tests.
    Does NOT manage processes (handled by pytest fixtures in conftest.py).
    Focuses on connection and 'soft reset' of the mount state.
    """

    async def asyncSetUp(self):
        self.external_env = os.environ.get("INDI_EXTERNAL_ENV") == "1"
        self.client = INDIClient(port=INDI_PORT)

        try:
            await self.client.connect()
        except Exception as e:
            self.skipTest(f"Could not connect to INDI server on {INDI_PORT}: {e}")

    async def asyncTearDown(self):
        if hasattr(self, "client"):
            try:
                await self.client.disconnect()
            except:
                pass

    async def connect_to_sim(self, disable_alignment=True):
        """
        Ensures the driver is connected to simulator and in a known 'clean' state.
        Uses INDI commands for 'soft reset' instead of process restarts.
        """
        # 1. Connect driver to mount
        await self.client.wait_for_property(DEVICE_NAME, "CONNECTION")
        await self.client.set_switch(DEVICE_NAME, "CONNECTION_MODE", ["CONNECTION_TCP"])
        await self.client.set_text(
            DEVICE_NAME,
            "DEVICE_ADDRESS",
            {"PORT": str(SIM_PORT), "ADDRESS": "localhost"},
        )
        await self.client.set_switch(DEVICE_NAME, "CONNECTION", ["CONNECT"])
        await self.client.wait_for_state(DEVICE_NAME, "CONNECTION", "Ok", timeout=10)

        # 2. Synchronize basic environment
        await self.client.wait_for_any_property(
            DEVICE_NAME,
            lambda d, n, p: n in ["HORIZONTAL_COORD", "EQUATORIAL_EOD_COORD"],
            timeout=10,
        )

        import datetime

        now_utc = datetime.datetime.now(datetime.timezone.utc)
        await self.client.set_location(DEVICE_NAME, "51.5", "0.0", "50.0")
        await self.client.set_time(
            DEVICE_NAME, now_utc.strftime("%Y-%m-%dT%H:%M:%S"), "0"
        )

        # 3. Soft Reset: Unpark
        for attempt in range(3):
            try:
                park_prop = await self.client.wait_for_property(
                    DEVICE_NAME, "TELESCOPE_PARK", timeout=5
                )
                if park_prop["values"].get("UNPARK") == "On":
                    break
                await self.client.set_switch(DEVICE_NAME, "TELESCOPE_PARK", ["UNPARK"])
                await asyncio.sleep(2)
            except:
                continue

        # 4. Soft Reset: Clear Alignment
        if "ALIGNMENT_POINTSET_ACTION" in self.client.devices[DEVICE_NAME]:
            await self.client.set_switch(
                DEVICE_NAME, "ALIGNMENT_POINTSET_ACTION", ["CLEAR"]
            )
            await asyncio.sleep(1)
            if "ALIGNMENT_POINTSET_COMMIT" in self.client.devices[DEVICE_NAME]:
                await self.client.set_switch(
                    DEVICE_NAME,
                    "ALIGNMENT_POINTSET_COMMIT",
                    ["ALIGNMENT_POINTSET_COMMIT"],
                )
            await asyncio.sleep(1)

        # 5. Configure Alignment Subsystem
        if "ALIGNMENT_SUBSYSTEM_ACTIVE" in self.client.devices[DEVICE_NAME]:
            switch_name = "ALIGNMENT SUBSYSTEM ACTIVE"
            prop = self.client.get_property(DEVICE_NAME, "ALIGNMENT_SUBSYSTEM_ACTIVE")
            current_on = prop["values"].get(switch_name) == "On"
            target_on = not disable_alignment

            if current_on != target_on:
                state_str = "On" if target_on else "Off"
                xml = (
                    f'<newSwitchVector device="{DEVICE_NAME}" name="ALIGNMENT_SUBSYSTEM_ACTIVE">\n'
                    f'  <oneSwitch name="{switch_name}">{state_str}</oneSwitch>\n'
                    f"</newSwitchVector>"
                )
                await self.client.send_data(xml.encode())
                await asyncio.sleep(1)

        await self.client.set_switch(DEVICE_NAME, "DEBUG", ["ENABLE"])
        await self.client.set_switch(DEVICE_NAME, "TELESCOPE_TRACK_STATE", ["TRACK_ON"])
        await asyncio.sleep(1)
