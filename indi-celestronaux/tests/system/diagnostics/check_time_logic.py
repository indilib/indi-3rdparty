import asyncio
import os
from datetime import datetime, timezone
from .indi_client import INDIClient

DEVICE_NAME = "Celestron AUX"
SIM_PORT = 2000
INDI_PORT = 7624


async def check_time_logic():
    client = INDIClient(port=INDI_PORT)
    await client.connect()
    await client.wait_for_property(DEVICE_NAME, "CONNECTION")
    await client.set_switch(DEVICE_NAME, "CONNECTION", ["CONNECT"])
    await client.wait_for_state(DEVICE_NAME, "CONNECTION", "Ok")

    # 1. Set explicit location
    lat, lon = 50.1822, 19.7925
    print(f"Setting Location: Lat {lat}, Lon {lon}")
    await client.set_location(DEVICE_NAME, str(lat), str(lon), "400.0")

    # 2. Read Time properties
    print("Waiting for Time properties...")
    # LST is often in TIME_LST or calculated from UTC
    try:
        prop_lst = await client.wait_for_property(DEVICE_NAME, "TIME_LST", timeout=5)
        lst_val = float(prop_lst["values"]["LST"])
    except:
        lst_val = "N/A"

    prop_utc = await client.wait_for_property(DEVICE_NAME, "TIME_UTC", timeout=5)
    utc_str = prop_utc["values"]["UTC"]
    offset = float(prop_utc["values"]["OFFSET"])

    now_utc = datetime.now(timezone.utc)
    sys_utc_hours = now_utc.hour + now_utc.minute / 60.0 + now_utc.second / 3600.0
    sys_lst = (sys_utc_hours + lon / 15.0) % 24.0

    print(f"System UTC: {sys_utc_hours:.4f}")
    print(f"System LST: {sys_lst:.4f}")
    print(f"Driver UTC: {utc_str}")
    print(f"Driver Offset: {offset}")
    print(f"Driver LST: {lst_val}")

    # 3. Check RA at 0,0 again
    await client.set_number(
        DEVICE_NAME, "TELESCOPE_ABSOLUTE_COORD", {"AZM_STEPS": 0, "ALT_STEPS": 0}
    )
    await asyncio.sleep(2)
    prop_eq = client.get_property(DEVICE_NAME, "EQUATORIAL_EOD_COORD")
    reported_ra = float(prop_eq["values"]["RA"])

    print(f"Driver RA at 0,0: {reported_ra:.4f}")

    await client.disconnect()


if __name__ == "__main__":
    asyncio.run(check_time_logic())
