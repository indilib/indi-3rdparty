import pytest
import asyncio
import os
import xml.etree.ElementTree as ET
from .conftest import DEVICE_NAME, driver_client_context


def get_config_value(property_name, switch_name=None):
    """Read a property value from the config file."""
    config_dir = os.environ.get("INDICONFIG", os.path.expanduser("~/.indi"))
    config_path = os.path.join(config_dir, "Celestron AUX_config.xml")
    if not os.path.exists(config_path):
        return None

    tree = ET.parse(config_path)
    root = tree.getroot()

    for elem in root:
        if elem.get("name") == property_name:
            if switch_name:
                for sw in elem:
                    if sw.get("name") == switch_name:
                        return sw.text.strip() if sw.text else None
            else:
                values = {}
                for sw in elem:
                    values[sw.get("name")] = sw.text.strip() if sw.text else None
                return values
    return None


def test_cordwrap_toggle_persistence(indiserver_process):
    """Enable cord wrap and verify it persists after save."""

    async def run():
        async with driver_client_context() as client:
            # Enable cord wrap
            await client.set_switch(DEVICE_NAME, "CORDWRAP", ["INDI_ENABLED"])
            await asyncio.sleep(1)

            # Verify it's enabled
            prop = client.get_property(DEVICE_NAME, "CORDWRAP")
            assert prop["values"].get("INDI_ENABLED") == "On"

            # Save config
            await client.set_switch(DEVICE_NAME, "CONFIG_PROCESS", ["CONFIG_SAVE"])
            await asyncio.sleep(2)

            # Verify config file has the correct value
            enabled = get_config_value("CORDWRAP", "INDI_ENABLED")
            assert enabled == "On", f"Config file shows CORDWRAP INDI_ENABLED={enabled}, expected On"

    asyncio.run(run())


def test_cordwrap_toggle_disable_persistence(indiserver_process):
    """Disable cord wrap and verify it persists after save."""

    async def run():
        async with driver_client_context() as client:
            # First enable, then disable
            await client.set_switch(DEVICE_NAME, "CORDWRAP", ["INDI_ENABLED"])
            await asyncio.sleep(1)
            await client.set_switch(DEVICE_NAME, "CORDWRAP", ["INDI_DISABLED"])
            await asyncio.sleep(1)

            # Verify it's disabled
            prop = client.get_property(DEVICE_NAME, "CORDWRAP")
            assert prop["values"].get("INDI_DISABLED") == "On"

            # Save config
            await client.set_switch(DEVICE_NAME, "CONFIG_PROCESS", ["CONFIG_SAVE"])
            await asyncio.sleep(2)

            # Verify config file
            disabled = get_config_value("CORDWRAP", "INDI_DISABLED")
            assert disabled == "On", f"Config file shows CORDWRAP INDI_DISABLED={disabled}, expected On"

    asyncio.run(run())


def test_cordwrap_position_persistence(indiserver_process):
    """Set cord wrap position to SE and verify it persists after save."""

    async def run():
        async with driver_client_context() as client:
            # Set position to SE
            await client.set_switch(DEVICE_NAME, "CORDWRAP_POS", ["CORDWRAP_SE"])
            await asyncio.sleep(1)

            # Verify it's set
            prop = client.get_property(DEVICE_NAME, "CORDWRAP_POS")
            assert prop["values"].get("CORDWRAP_SE") == "On"

            # Save config
            await client.set_switch(DEVICE_NAME, "CONFIG_PROCESS", ["CONFIG_SAVE"])
            await asyncio.sleep(2)

            # Verify config file
            se_on = get_config_value("CORDWRAP_POS", "CORDWRAP_SE")
            n_off = get_config_value("CORDWRAP_POS", "CORDWRAP_N")
            assert se_on == "On", f"Config file shows CORDWRAP_SE={se_on}, expected On"
            assert n_off == "Off", f"Config file shows CORDWRAP_N={n_off}, expected Off"

    asyncio.run(run())


def test_cordwrap_base_persistence(indiserver_process):
    """Set cord wrap base to Sky/Alignment and verify it persists after save."""

    async def run():
        async with driver_client_context() as client:
            # Set base to Sky/Alignment
            await client.set_switch(DEVICE_NAME, "CW_BASE", ["CW_BASE_SKY"])
            await asyncio.sleep(1)

            # Verify it's set
            prop = client.get_property(DEVICE_NAME, "CW_BASE")
            assert prop["values"].get("CW_BASE_SKY") == "On"

            # Save config
            await client.set_switch(DEVICE_NAME, "CONFIG_PROCESS", ["CONFIG_SAVE"])
            await asyncio.sleep(2)

            # Verify config file
            sky_on = get_config_value("CW_BASE", "CW_BASE_SKY")
            enc_off = get_config_value("CW_BASE", "CW_BASE_ENC")
            assert sky_on == "On", f"Config file shows CW_BASE_SKY={sky_on}, expected On"
            assert enc_off == "Off", f"Config file shows CW_BASE_ENC={enc_off}, expected Off"

    asyncio.run(run())


def test_cordwrap_full_config_persistence(indiserver_process):
    """Set all cord wrap properties and verify they persist together after save."""

    async def run():
        async with driver_client_context() as client:
            # Set all cord wrap properties
            await client.set_switch(DEVICE_NAME, "CORDWRAP", ["INDI_ENABLED"])
            await asyncio.sleep(0.5)
            await client.set_switch(DEVICE_NAME, "CORDWRAP_POS", ["CORDWRAP_SE"])
            await asyncio.sleep(0.5)
            await client.set_switch(DEVICE_NAME, "CW_BASE", ["CW_BASE_SKY"])
            await asyncio.sleep(1)

            # Verify all properties
            toggle = client.get_property(DEVICE_NAME, "CORDWRAP")
            pos = client.get_property(DEVICE_NAME, "CORDWRAP_POS")
            base = client.get_property(DEVICE_NAME, "CW_BASE")

            assert toggle["values"].get("INDI_ENABLED") == "On"
            assert pos["values"].get("CORDWRAP_SE") == "On"
            assert base["values"].get("CW_BASE_SKY") == "On"

            # Save config
            await client.set_switch(DEVICE_NAME, "CONFIG_PROCESS", ["CONFIG_SAVE"])
            await asyncio.sleep(2)

            # Verify config file has all values
            assert get_config_value("CORDWRAP", "INDI_ENABLED") == "On"
            assert get_config_value("CORDWRAP_POS", "CORDWRAP_SE") == "On"
            assert get_config_value("CORDWRAP_POS", "CORDWRAP_N") == "Off"
            assert get_config_value("CW_BASE", "CW_BASE_SKY") == "On"

    asyncio.run(run())


def test_cordwrap_position_multiple(indiserver_process):
    """Test setting different positions and verify each persists."""

    positions = [
        ("CORDWRAP_NE", "CORDWRAP_N"),
        ("CORDWRAP_E", "CORDWRAP_NE"),
        ("CORDWRAP_S", "CORDWRAP_E"),
        ("CORDWRAP_SW", "CORDWRAP_S"),
        ("CORDWRAP_W", "CORDWRAP_SW"),
        ("CORDWRAP_NW", "CORDWRAP_W"),
        ("CORDWRAP_N", "CORDWRAP_NW"),
    ]

    async def run():
        async with driver_client_context() as client:
            for target, previous in positions:
                # Set position
                await client.set_switch(DEVICE_NAME, "CORDWRAP_POS", [target])
                await asyncio.sleep(0.5)

                # Verify
                prop = client.get_property(DEVICE_NAME, "CORDWRAP_POS")
                assert prop["values"].get(target) == "On", f"{target} not set"
                assert prop["values"].get(previous) == "Off", f"{previous} still on"

                # Save config
                await client.set_switch(DEVICE_NAME, "CONFIG_PROCESS", ["CONFIG_SAVE"])
                await asyncio.sleep(1)

                # Verify config
                target_val = get_config_value("CORDWRAP_POS", target)
                previous_val = get_config_value("CORDWRAP_POS", previous)
                assert target_val == "On", f"Config: {target}={target_val}, expected On"
                assert previous_val == "Off", f"Config: {previous}={previous_val}, expected Off"

    asyncio.run(run())


def test_cordwrap_disable_does_not_affect_position(indiserver_process):
    """Verify that disabling cord wrap does not change the position setting."""

    async def run():
        async with driver_client_context() as client:
            # Set position to SE
            await client.set_switch(DEVICE_NAME, "CORDWRAP_POS", ["CORDWRAP_SE"])
            await asyncio.sleep(0.5)

            # Enable cord wrap
            await client.set_switch(DEVICE_NAME, "CORDWRAP", ["INDI_ENABLED"])
            await asyncio.sleep(0.5)

            # Save config
            await client.set_switch(DEVICE_NAME, "CONFIG_PROCESS", ["CONFIG_SAVE"])
            await asyncio.sleep(1)

            # Disable cord wrap
            await client.set_switch(DEVICE_NAME, "CORDWRAP", ["INDI_DISABLED"])
            await asyncio.sleep(0.5)

            # Verify position is still SE
            pos = client.get_property(DEVICE_NAME, "CORDWRAP_POS")
            assert pos["values"].get("CORDWRAP_SE") == "On"

            # Save config again
            await client.set_switch(DEVICE_NAME, "CONFIG_PROCESS", ["CONFIG_SAVE"])
            await asyncio.sleep(1)

            # Verify config still has SE
            se_val = get_config_value("CORDWRAP_POS", "CORDWRAP_SE")
            assert se_val == "On", f"After disable, CORDWRAP_SE={se_val}, expected On"

    asyncio.run(run())
