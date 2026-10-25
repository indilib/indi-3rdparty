#!/usr/bin/env python3
"""Deliberately incomplete upload; never sends COMMIT. Attended, motor off."""
import argparse
import importlib.util
import json
from pathlib import Path
import socket
import time
root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('uploader',root/'upload-wifi.py')
u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--host',required=True);p.add_argument('--mount',required=True)
p.add_argument('--indi-host',default='localhost');p.add_argument('--indi-port',type=int,default=7624)
p.add_argument('--motor-off',action='store_true');p.add_argument('--attended',action='store_true')
a=p.parse_args()
if not (a.motor_off and a.attended): p.error('Requires --motor-off and --attended')
token=(root/'ota-token.txt').read_text().strip()
manifest=json.loads((root/'build/firmware.json').read_text())
data=u.inspect_image(root/'build'/manifest['file'],manifest,token)
u.standard_parked(a.mount,a.indi_host,a.indi_port)
def require(actual,expected):
    if actual!=expected: raise RuntimeError('Unexpected response: '+actual)
with socket.create_connection((a.host,7624),timeout=8) as control:
    def query(cmd):
        control.sendall(cmd.encode());return u.line(control)
    version=query('(GET:VERSION:0)')
    if not version.startswith('(ACK:VERSION:'): raise RuntimeError('No valid identity')
    require(query('(GET:CLOSED:0)'),'(ACK:CLOSED:ON)')
    require(query('(GET:OPENED:0)'),'(ACK:OPENED:OFF)')
    try:
        require(query('(SET:OTA:'+token+')'),'(ACK:OTA:MAINTENANCE)')
        with socket.create_connection((a.host,7625),timeout=8) as upload:
            upload.settimeout(20)
            upload.sendall(f'ROOFOTA1 {token} {len(data)} {u.zlib.crc32(data):08x}\n'.encode()+data[:64])
            started=time.monotonic();reply=u.line(upload)
            print(reply,'after',round(time.monotonic()-started,2),'seconds')
            require(reply,'ERROR:timeout')
    finally:
        require(query('(SET:OTA:OFF)'),'(ACK:OTA:DISABLED)')
    require(query('(GET:VERSION:0)'),version)
    require(query('(GET:OTA:0)'),'(ACK:OTA:DISABLED)')
    print('PASS: firmware unchanged, OTA disabled; no COMMIT sent')
