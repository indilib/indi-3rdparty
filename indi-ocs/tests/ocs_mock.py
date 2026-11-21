#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Ed Lee
"""Mock OCS controller and INDI test rig for the indi_ocs driver.

Replies follow OCS ProcessCmds.cpp framing: string replies end in '#',
numeric (0/1) replies are unframed, and commands that return nothing send no bytes.
An unknown command gets an unframed 0, as OCS does."""
import os
import re
import signal
import socket
import subprocess
import tempfile
import threading
import time

DEVICE = "OCS"

DEFAULT_REPLIES = {
    ":IP#": "OCS#", ":IN#": "3.12j#", ":DU#": "I#", ":IT#": "1.5,2.0#", ":GT#": "21.0,40.0#", ":It#": "8,9,-1#",
    ":Ip#": "1,2,3,4,5,6#", ":IL#": "10,11,12,13,14#", ":G1#": "10.0#", ":Gb#": "1010.0#", ":Gh#": "50.0#",
    ":Gw#": "5#", ":GR#": "3#", ":G3#": "-25.0#", ":GC#": "Clear#", ":GQ#": "21.0#", ":G2#": "-10.0#",
    ":IW#": "20,-14#", ":RS#": "i,CLOSED#", ":DZ#": "90.000#", ":GP#": "OK#", ":Gs#": "SAFE#", ":GX9F#": "30#",
    ":RSL#": "No Error#", ":GH#": "15#", ":GV#": "25#", ":GD#": "0#", ":DS#": "0", ":DN#": "0", ":DP#": "1",
    ":DQ#": "1", ":DR#": "1",
}


class MockOCS:
    """TCP server answering OCS commands. Change replies at any time through .replies."""

    def __init__(self, **replies):
        self.replies = dict(DEFAULT_REPLIES)
        self.replies.update(replies)
        self.reject_relays = False
        self.silent = False
        self.log = []
        self.lock = threading.Lock()
        self.server = socket.socket()
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind(("127.0.0.1", 0))
        self.server.listen(5)
        self.port = self.server.getsockname()[1]
        threading.Thread(target=self._accept, daemon=True).start()

    def _accept(self):
        while True:
            try:
                connection, _ = self.server.accept()
            except OSError:
                return
            threading.Thread(target=self._serve, args=(connection,), daemon=True).start()

    def reply(self, command):
        if command in self.replies:
            return self.replies[command]
        if re.fullmatch(r":Ip\d#", command):
            return f"Device {command[3]}#"
        if re.fullmatch(r":GR\d+#", command):
            return "OFF#"
        if command.startswith(":SR"):
            return "0" if self.reject_relays else "1"
        if command.startswith((":SH", ":SC", ":SD", ":R!", ":R+")):
            return "1"
        if command.startswith((":Dz", ":RO", ":RC", ":RH", ":DH", ":DC", ":DF")):
            return ""
        return "0"

    def _serve(self, connection):
        buffer = b""
        try:
            while True:
                data = connection.recv(256)
                if not data:
                    return
                buffer += data
                while b"#" in buffer:
                    end = buffer.index(b"#") + 1
                    command, buffer = buffer[:end].decode("latin1"), buffer[end:]
                    with self.lock:
                        self.log.append((time.monotonic(), command))
                    answer = "" if self.silent else self.reply(command)
                    if answer:
                        connection.sendall(answer.encode())
        except OSError:
            pass

    def commands(self, since=0.0):
        with self.lock:
            return [command for stamp, command in self.log if stamp >= since]

    def close(self):
        self.server.close()


class Rig:
    """indiserver running the driver, connected to a MockOCS over TCP."""

    def __init__(self, driver, mock):
        self.mock = mock
        self.home = tempfile.TemporaryDirectory()
        os.makedirs(os.path.join(self.home.name, ".indi"))
        self.env = dict(os.environ, HOME=self.home.name)
        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        self.port = probe.getsockname()[1]
        probe.close()
        self.output = open(os.path.join(self.home.name, "server.log"), "w")
        self.server = subprocess.Popen(["indiserver", "-u", f"/tmp/indi-ocs-test-{self.port}", "-p", str(self.port), "-r", "0",
                                        driver], env=self.env,
                                       stdout=self.output, stderr=self.output, start_new_session=True)
        wait_for(lambda: self.get("CONNECTION.CONNECT") is not None, 10, "driver to start")

    def set(self, expression):
        subprocess.run(["indi_setprop", "-p", str(self.port), f"{DEVICE}.{expression}"], env=self.env,
                       capture_output=True, timeout=10)

    def values(self, pattern):
        result = subprocess.run(["indi_getprop", "-p", str(self.port), "-t", "1", f"{DEVICE}.{pattern}"],
                                env=self.env, capture_output=True, text=True, timeout=10)
        values = {}
        for line in result.stdout.splitlines():
            name, _, value = line.partition("=")
            values[name[len(DEVICE) + 1:]] = value
        return values

    def get(self, name):
        return self.values(name).get(name)

    def connect(self):
        self.set("CONNECTION_MODE.CONNECTION_TCP=On")
        self.set(f"DEVICE_ADDRESS.ADDRESS;PORT=127.0.0.1;{self.mock.port}")
        self.set("CONNECTION.CONNECT=On")
        wait_for(lambda: self.get("CONNECTION.CONNECT") == "On", 15, "connection")
        time.sleep(2)

    def disconnect(self):
        self.set("CONNECTION.DISCONNECT=On")
        wait_for(lambda: self.get("CONNECTION.CONNECT") == "Off", 10, "disconnection")

    def alive(self):
        return self.server.poll() is None and self.get("CONNECTION.CONNECT") is not None

    def refresh_weather(self):
        self.set("WEATHER_REFRESH.REFRESH=On")
        return time.monotonic()

    def close(self):
        try:
            os.killpg(self.server.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        self.server.wait()
        self.output.close()
        self.home.cleanup()


def wait_for(condition, timeout, what):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if condition():
            return
        time.sleep(0.2)
    raise AssertionError(f"timed out waiting for {what}")


def sleep_until(start, seconds):
    remaining = start + seconds - time.monotonic()
    if remaining > 0:
        time.sleep(remaining)
