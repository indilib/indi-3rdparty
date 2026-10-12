#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Prometheus Astro
import os
import pty
import select
import sys
import time
import json
import copy
import socket

path_file, log_file = sys.argv[1:3]
tcp = len(sys.argv) > 3 and sys.argv[3] == 'tcp'
if tcp:
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
else:
    master, slave = pty.openpty()
ready_at = time.monotonic() + 4
with open(path_file, "w", encoding="utf-8") as stream:
    stream.write(str(listener.getsockname()[1]) if tcp else os.ttyname(slave))
if tcp:
    peer, _ = listener.accept()
    master = peer.fileno()

terminated = {
    "GVP": "On-Step#",
    "TYV": '{"ok":true,"error":0,"data":{"protocol":1,"product":"Prometheus Astro Týr","version":"1.2.8","network":true,"bluetooth":true}}#',
    "GVN": "1.2.0#",
    "GXE4": "26666.667#",
    "GX92": "80.000#",
    "GX93": "100.000#",
    "GX99": "20.000#",
    "GX97": "0.468750#",
    "hI?": "1,0,32,33#",
    "GU": "nNpBaEW260#",
    "GR": "02:00:00#",
    "GD": "+45*00:00#",
    "Gm": "E#",
    "GG": "-02:00#",
}
settings = {"GXA0": "1", "GXA1": "26666.667,-1,-180,180,64,1,500,900,1200,-1,T",
    "GXA2": "28444.444,-1,-90,90,64,1,500,900,1200,0,T", "Gh": "0*", "Go": "90*",
    "GXE9": "0", "GXEA": "0", "%BR": "0", "%BD": "0", "h?": "1,0,0",
    "GG": "-02:00", "GXTR": "0", "GXTD": "0", "GXEM": "1", "GX96": "B", "A?": "900", "GE": "0"}
network = dict(apEnabled=True, stationEnabled=False, fallback=True, apSsid="Tyr", apChannel=1,
    apIp="192.168.0.1", apGateway="192.168.0.1", apSubnet="255.255.255.0",
    stations=[dict(host="tyr", ssid="", dhcp=True, ip="0.0.0.0", gateway="0.0.0.0", subnet="255.255.255.0", target="0.0.0.0") for _ in range(3)])
pending = None
bluetooth = dict(enabled=False, connected=False, armed=False, message="Test", selected=dict(name="", address=""), devices=[])
def managed(operation, log):
    global pending, network
    error, data = 0, ""
    if operation.startswith("C"):
        body = bytes.fromhex(operation[1:]).decode("ascii")
        log.write("MANAGED " + body + "\n")
        data = settings.get(body, terminated.get(body, "").rstrip("#"))
        if body.startswith("SXA") and body[3] in "12" and body[4] == ',':
            settings["GXA" + body[3]] = body[5:] + ",T"
        for write, read in {"Sh":"Gh", "So":"Go", "SXE9,":"GXE9", "SXEA,":"GXEA", "$BR":"%BR", "$BD":"%BD", "SG":"GG", "SXTR,":"GXTR", "SXTD,":"GXTD", "SXEM,":"GXEM", "SX96,":"GX96"}.items():
            if body.startswith(write):
                value = body[len(write):]
                if body == "Sh13": error = 5  # Deliberate firmware rejection.
                else: settings[read] = value
        if body.startswith(("hC1,", "hC2,")):
            offsets = settings["h?"].split(','); offsets[int(body[2])] = body[4:]; settings["h?"] = ','.join(offsets)
        for prefix, flag in {"hA":"B", "SX95,":"a", "SX98,":"u", "SX97,":"z"}.items():
            if body in (prefix + '0', prefix + '1'):
                terminated["GU"] = terminated["GU"].replace(flag, '').rstrip('#') + (flag if body.endswith('1') else '') + '#'
    elif operation == "N": data = network
    elif operation == "D": data = dict(uptimeMs=12345, resetReason="power-on")
    elif operation == "B": data = bluetooth
    elif operation == "nbegin": pending = copy.deepcopy(network)
    elif operation == "nabort": pending = None
    elif operation == "ncommit":
        if pending is None: error = 5
        else: network, pending = pending, None
    elif operation.startswith('n') and ',' in operation:
        key, encoded = operation[1:].split(',', 1); value = bytes.fromhex(encoded).decode('utf-8')
        if pending is None or value == "REJECT": error = 5
        else:
            obj = pending
            if key.startswith(('s1.', 's2.', 's3.')): obj, key = pending['stations'][int(key[1])-1], key[3:]
            obj[key] = value == '1' if key in ('apEnabled', 'stationEnabled', 'fallback', 'dhcp') else int(value) if key == 'apChannel' else value
    elif operation.startswith('b'):
        if operation == 'benable': bluetooth['enabled'] = True
        elif operation == 'bdisable': bluetooth['enabled'] = False
    else: error = 5
    return json.dumps(dict(ok=error == 0, error=error, data=data), ensure_ascii=False) + '#'
single_prefixes = ("Sr", "Sd", "SX", "St", "Sg", "Sv", "SC", "SL", "hI", "MG")
single_commands = {"MS": "0", "hP": "1", "hR": "1", "hQ": "1", "Te": "1", "Td": "1",
                   "Tn": "1", "Tr": "1", "To": "1", "T1": "1", "T2": "1"}

buffer = bytearray()
with open(log_file, "w", encoding="utf-8", buffering=1) as log:
    while True:
        ready, _, _ = select.select([master], [], [], 1)
        if not ready:
            continue
        try:
            data = os.read(master, 512)
        except OSError:
            continue
        if not data:
            continue
        buffer.extend(data)
        while b"#" in buffer:
            frame, _, remainder = buffer.partition(b"#")
            buffer = bytearray(remainder)
            body = frame.decode("ascii", errors="replace").lstrip(":")
            log.write(("NETWORK [redacted]" if body.startswith('TYn') else body) + "\n")
            if time.monotonic() < ready_at:
                continue
            reply = managed(body[2:], log) if body.startswith('TY') and body != 'TYV' else (
                settings[body] + '#' if body in settings else terminated.get(body))
            if reply is None:
                reply = single_commands.get(body)
            if body == 'Te': terminated['GU'] = terminated['GU'].replace('n', '')
            if body == 'Td' and 'n' not in terminated['GU']: terminated['GU'] = 'n' + terminated['GU']
            if reply is None and body.startswith(single_prefixes):
                reply = "1"
            if reply is not None:
                encoded = reply.encode('utf-8')
                if tcp:
                    for offset in range(0, len(encoded), 7): peer.sendall(encoded[offset:offset+7])
                else: os.write(master, encoded)
