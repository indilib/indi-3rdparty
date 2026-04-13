import asyncio
import os
from .indi_client import INDIClient

DEVICE_NAME = "Celestron AUX"
SIM_PORT = 2000
INDI_PORT = 7624


async def list_properties():
    client = INDIClient(port=INDI_PORT)
    await client.connect()

    await client.connect()

    print("Listing devices and properties...")
    await asyncio.sleep(2)

    for dev_name in client.devices:
        print(f"Device: {dev_name}")
        for prop_name, prop in client.devices[dev_name].items():
            print(f"  - {prop_name}")
            for val_name, val in prop.get("values", {}).items():
                print(f"    * {val_name}: {val}")

    await client.disconnect()


if __name__ == "__main__":
    asyncio.run(list_properties())
