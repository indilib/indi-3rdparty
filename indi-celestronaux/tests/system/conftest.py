import pytest
import asyncio
import subprocess
import time
import os
import socket
import sys
from .indi_client import INDIClient
from contextlib import asynccontextmanager

# Configuration constants
DEVICE_NAME = "Celestron AUX"
SIM_PORT = 2000
INDI_PORT = 7624

# Default Geographic Coordinates (can be overridden by environment)
TEST_LAT = os.environ.get("INDI_LAT", "50.06")
TEST_LONG = os.environ.get("INDI_LONG", "19.94")
TEST_ELEV = os.environ.get("INDI_ELEV", "200.0")

# Prioritize local build, allow override for system driver
# Use relative path if we are in the project root
DEFAULT_DRIVER = os.path.abspath("build/indi_celestron_aux")
DRIVER_EXEC = os.environ.get("INDI_DRIVER_PATH", DEFAULT_DRIVER)

# Prioritize caux-sim, allow override
SIM_EXEC = os.environ.get("INDI_SIM_EXEC", "caux-sim")


def is_port_open(port):
    """Check if a local port is already in use."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(("localhost", port)) == 0


@pytest.fixture(scope="session")
def simulator_process():
    """
    Manages the simulator lifecycle.
    Uses existing one if port is occupied, otherwise starts a new one.
    """
    proc = None
    if is_port_open(SIM_PORT):
        print(f"\n[INFO] Using existing simulator on port {SIM_PORT}")
    else:
        # Check if simulator exists in PATH
        if (
            subprocess.call(
                ["which", SIM_EXEC],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            != 0
        ):
            pytest.fail(
                f"Simulator '{SIM_EXEC}' not found in PATH. Please install it or set INDI_SIM_EXEC."
            )

        print(f"\n[INFO] Starting simulator: {SIM_EXEC}")
        if SIM_EXEC.endswith(".py"):
            cmd = [sys.executable, "-u", SIM_EXEC, "-t", "-p", str(SIM_PORT)]
        else:
            cmd = [
                SIM_EXEC,
                "-t",
                "-p",
                str(SIM_PORT),
                "--log-file",
                "/tmp/nse_sim_cmds.log",
                "--log-categories",
                "31",
                "--web",
                "--web-host",
                "0.0.0.0",
                "--web-port",
                "8080",
            ]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(2)

    yield proc

    if proc:
        print("[INFO] Stopping internal simulator...")
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()


@pytest.fixture(scope="session")
def indiserver_process(simulator_process):
    """
    Manages the indiserver lifecycle.
    """
    proc = None
    if is_port_open(INDI_PORT):
        print(f"[INFO] Using existing INDI server on port {INDI_PORT}")
    else:
        # Check if driver executable exists
        if not os.path.exists(DRIVER_EXEC):
            if not ("/" in DRIVER_EXEC or "\\" in DRIVER_EXEC):
                if (
                    subprocess.call(
                        ["which", DRIVER_EXEC],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                    )
                    != 0
                ):
                    pytest.fail(
                        f"Driver '{DRIVER_EXEC}' not found in PATH. Please build it or set INDI_DRIVER_PATH."
                    )
            else:
                pytest.fail(
                    f"Driver not found at {DRIVER_EXEC}. Please build the project first."
                )

        print(f"[INFO] Starting indiserver with driver: {DRIVER_EXEC}")
        log_file = open("/tmp/indi_celestron_aux.log", "w")
        cmd = ["indiserver", "-vv", "-p", str(INDI_PORT), "-r", "0", DRIVER_EXEC]
        proc = subprocess.Popen(cmd, stdout=log_file, stderr=log_file)
        time.sleep(3)

    yield proc

    if proc:
        print("[INFO] Stopping internal indiserver...")
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()


@asynccontextmanager
async def driver_client_context():
    """
    Async context manager that provides a connected INDI client and performs a soft reset.
    """
    client = INDIClient(port=INDI_PORT)
    await client.connect()

    try:
        # 1. Ensure basic connection
        await client.wait_for_property(DEVICE_NAME, "CONNECTION")
        await client.set_switch(DEVICE_NAME, "CONNECTION_MODE", ["CONNECTION_TCP"])
        await client.set_text(
            DEVICE_NAME,
            "DEVICE_ADDRESS",
            {"PORT": str(SIM_PORT), "ADDRESS": "localhost"},
        )
        await client.set_switch(DEVICE_NAME, "CONNECTION", ["CONNECT"])
        await client.wait_for_state(DEVICE_NAME, "CONNECTION", "Ok", timeout=15)

        # 2. Wait for coords
        await client.wait_for_any_property(
            DEVICE_NAME,
            lambda d, n, p: n in ["HORIZONTAL_COORD", "EQUATORIAL_EOD_COORD"],
            timeout=15,
        )

        # 3. GPS/Time sync - Propagation of coordinates
        import datetime

        now_utc = datetime.datetime.now(datetime.timezone.utc)
        print(f"\n[INFO] Synchronizing Location: {TEST_LAT}N, {TEST_LONG}E")
        await client.set_location(DEVICE_NAME, TEST_LAT, TEST_LONG, TEST_ELEV)
        await client.set_time(DEVICE_NAME, now_utc.strftime("%Y-%m-%dT%H:%M:%S"), "0")

        # 4. Unpark
        for _ in range(3):
            try:
                p = await client.wait_for_property(
                    DEVICE_NAME, "TELESCOPE_PARK", timeout=5
                )
                if p["values"].get("UNPARK") == "On":
                    break
                await client.set_switch(DEVICE_NAME, "TELESCOPE_PARK", ["UNPARK"])
                await asyncio.sleep(2)
            except:
                continue

        # 5. Clear Alignment model
        if "ALIGNMENT_POINTSET_ACTION" in client.devices[DEVICE_NAME]:
            await client.set_switch(DEVICE_NAME, "ALIGNMENT_POINTSET_ACTION", ["CLEAR"])
            await asyncio.sleep(1)
            if "ALIGNMENT_POINTSET_COMMIT" in client.devices[DEVICE_NAME]:
                await client.set_switch(
                    DEVICE_NAME,
                    "ALIGNMENT_POINTSET_COMMIT",
                    ["ALIGNMENT_POINTSET_COMMIT"],
                )
            await asyncio.sleep(1)

        await client.set_switch(DEVICE_NAME, "DEBUG", ["ENABLE"])
        await client.set_switch(DEVICE_NAME, "TELESCOPE_TRACK_STATE", ["TRACK_ON"])

        yield client
    finally:
        try:
            # SAFETY: Ensure all motion is stopped before disconnecting
            # This is critical for hardware safety to prevent runaway slewing.
            await client.set_switch(DEVICE_NAME, "TELESCOPE_ABORT_MOTION", ["ABORT"])
            await asyncio.sleep(1)
        except:
            pass
        await client.disconnect()
