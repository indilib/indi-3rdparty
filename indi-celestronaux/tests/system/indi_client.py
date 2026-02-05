import asyncio
import xml.etree.ElementTree as ET
import logging
from typing import Any

logger = logging.getLogger("INDIClient")


class INDIClient:
    def __init__(self, host="localhost", port=7624):
        self.host = host
        self.port = port
        self.reader = None
        self.writer = None
        self.devices = {}
        self.connected = False
        self.listeners = []

    async def connect(self):
        try:
            self.reader, self.writer = await asyncio.open_connection(
                self.host, self.port
            )
            self.connected = True
            asyncio.create_task(self._read_loop())
            # Request all properties
            await self.send_data(b'<getProperties version="1.7" />\n')
            # Wait a bit for initial definitions to arrive
            await asyncio.sleep(1)
        except Exception as e:
            logger.error(f"Failed to connect to INDI server: {e}")
            raise

    async def disconnect(self):
        if self.writer:
            self.writer.close()
            await self.writer.wait_closed()
        self.connected = False

    async def send_data(self, data):
        if not self.writer:
            raise RuntimeError("Not connected")
        # print(f"DEBUG: Sending INDI XML: {data.decode().strip()}")
        self.writer.write(data)
        await self.writer.drain()

    async def _read_loop(self):
        parser = ET.XMLPullParser(["end"])
        parser.feed("<root>")
        while self.connected:
            try:
                if self.reader is None:
                    break
                data = await self.reader.read(4096)
                if not data:
                    break

                parser.feed(data)
                events: Any = parser.read_events()

                for event, elem in events:
                    if event == "end":
                        if elem.tag == "root":
                            continue

                        # Only handle top-level INDI tags (vectors and messages)
                        if elem.tag.endswith("Vector") or elem.tag == "message":
                            await self._handle_element(elem)
                            # Now it's safe to clear the element and its children
                            elem.clear()
                        else:
                            # For child elements (one*, def*), we let them stay in the tree
                            # until the parent vector is finished.
                            pass

            except Exception as e:
                print(f"Read loop error: {e}")
                break

    async def _handle_element(self, elem):
        tag = elem.tag
        device = elem.get("device")
        name = elem.get("name")

        if tag == "message":
            msg = elem.text
            if msg:
                print(f"INDI Message [{device}]: {msg.strip()}")
            return

        # We handle def*, set*, new* tags
        if device and name:
            if device not in self.devices:
                self.devices[device] = {}

            # Get existing property or create new one
            if name not in self.devices[device]:
                prop = {"state": "Idle", "values": {}}
            else:
                prop = self.devices[device][name]

            # Update state if present
            state = elem.get("state")
            if state:
                prop["state"] = state

            # Update values (merge)
            for child in elem:
                child_name = child.get("name")
                if child_name:
                    val = child.text
                    if val is not None:
                        val = val.strip()
                    prop["values"][child_name] = val

            self.devices[device][name] = prop
            # print(f"DEBUG: Property update: {device}.{name} = {prop['state']} values={prop['values']}")

            # Notify listeners

            for queue in self.listeners:
                await queue.put((device, name, prop))

    async def wait_for_property(self, device, name, timeout=5):
        """Waits for a specific property to receive an update."""
        queue = asyncio.Queue()
        self.listeners.append(queue)

        # Check cache first
        cached = self.get_property(device, name)
        if cached:
            self.listeners.remove(queue)
            return cached

        try:
            end_time = asyncio.get_event_loop().time() + timeout
            while True:
                remaining = end_time - asyncio.get_event_loop().time()
                if remaining <= 0:
                    raise TimeoutError(f"Property {device}.{name} not received")

                try:
                    d, n, p = await asyncio.wait_for(queue.get(), remaining)
                    if d == device and n == name:
                        return p
                except asyncio.TimeoutError:
                    raise TimeoutError(f"Property {device}.{name} not received")
        finally:
            if queue in self.listeners:
                self.listeners.remove(queue)

    async def wait_for_state(self, device, name, states, timeout=5):
        """Waits for a property to reach one of the specified states."""
        if isinstance(states, str):
            states = [states]

        def condition(prop):
            return prop["state"] in states

        return await self.wait_for_condition(device, name, condition, timeout)

    async def wait_for_any_property(self, device, condition, timeout=5):
        """Waits for any property on a device to satisfy a condition."""
        queue = asyncio.Queue()
        self.listeners.append(queue)

        # Check cache
        if device in self.devices:
            for name, prop in self.devices[device].items():
                if condition(device, name, prop):
                    self.listeners.remove(queue)
                    return name, prop

        try:
            end_time = asyncio.get_event_loop().time() + timeout
            while True:
                remaining = end_time - asyncio.get_event_loop().time()
                if remaining <= 0:
                    raise TimeoutError(f"Timeout waiting for property on {device}")

                try:
                    d, n, p = await asyncio.wait_for(queue.get(), remaining)
                    if d == device and condition(d, n, p):
                        return n, p
                except asyncio.TimeoutError:
                    raise TimeoutError(f"Timeout waiting for property on {device}")
        finally:
            if queue in self.listeners:
                self.listeners.remove(queue)

    async def wait_for_condition(self, device, name, condition, timeout=5):
        """Waits for a property to satisfy a condition function."""
        queue = asyncio.Queue()
        self.listeners.append(queue)

        cached = self.get_property(device, name)
        if cached and condition(cached):
            self.listeners.remove(queue)
            return cached

        try:
            end_time = asyncio.get_event_loop().time() + timeout
            while True:
                remaining = end_time - asyncio.get_event_loop().time()
                if remaining <= 0:
                    raise TimeoutError(
                        f"Timeout waiting for condition on {device}.{name}"
                    )

                try:
                    d, n, p = await asyncio.wait_for(queue.get(), remaining)
                    if d == device and n == name and condition(p):
                        return p
                except asyncio.TimeoutError:
                    raise TimeoutError(
                        f"Timeout waiting for condition on {device}.{name}"
                    )
        finally:
            if queue in self.listeners:
                self.listeners.remove(queue)

    def get_property(self, device, name):
        return self.devices.get(device, {}).get(name)

    async def send_def_switch(
        self, device, name, label, group, switches, permission="rw", rule="1ofMany"
    ):
        """Sends a defSwitchVector."""
        xml = f'<defSwitchVector device="{device}" name="{name}" label="{label}" group="{group}" state="Idle" perm="{permission}" rule="{rule}">\n'
        for s_name, s_label, s_state in switches:
            xml += f'  <defSwitch name="{s_name}" label="{s_label}">{s_state}</defSwitch>\n'
        xml += "</defSwitchVector>\n"
        await self.send_data(xml.encode())

    async def send_new_switch(self, device, name, on_switches):
        """Sends a newSwitchVector even if the property is not yet known or marked RO."""
        xml = f'<newSwitchVector device="{device}" name="{name}">\n'
        for s in on_switches:
            xml += f'  <oneSwitch name="{s}">On</oneSwitch>\n'
        xml += "</newSwitchVector>\n"
        await self.send_data(xml.encode())

    async def set_number(self, device, name, values):
        """Sends a newNumberVector."""
        xml = f'<newNumberVector device="{device}" name="{name}">\n'
        for k, v in values.items():
            xml += f'  <oneNumber name="{k}">{v}</oneNumber>\n'
        xml += "</newNumberVector>\n"
        await self.send_data(xml.encode())

    async def set_location(self, device, lat, lon, elev):
        """Helper to set geographic coordinates."""
        await self.set_number(
            device, "GEOGRAPHIC_COORD", {"LAT": lat, "LONG": lon, "ELEV": elev}
        )

    async def set_time(self, device, utctime, offset):
        """Helper to set time."""
        await self.set_text(device, "TIME_UTC", {"UTC": utctime, "OFFSET": offset})

    async def set_text(self, device, name, values):
        """
        values: dict of element_name -> value
        """
        xml = f'<newTextVector device="{device}" name="{name}">\n'
        for k, v in values.items():
            xml += f'  <oneText name="{k}">{v}</oneText>\n'
        xml += "</newTextVector>\n"
        await self.send_data(xml.encode())

    async def set_switch(self, device, name, on_switches):
        """
        on_switches: list of switch names to set On
        """
        xml = f'<newSwitchVector device="{device}" name="{name}">\n'
        for s in on_switches:
            xml += f'  <oneSwitch name="{s}">On</oneSwitch>\n'
        xml += "</newSwitchVector>\n"

        # print(f"DEBUG: Sending Switch XML:\n{xml}")
        await self.send_data(xml.encode())
