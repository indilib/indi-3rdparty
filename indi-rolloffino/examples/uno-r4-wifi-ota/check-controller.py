#!/usr/bin/env python3
"""Read-only diagnostics using one command connection; no motion or maintenance."""
import argparse
import importlib.util
from pathlib import Path
import socket
import time

spec = importlib.util.spec_from_file_location('uploader', Path(__file__).with_name('upload-wifi.py'))
u = importlib.util.module_from_spec(spec)
spec.loader.exec_module(u)
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--host', required=True)
p.add_argument('--parser-timeout', action='store_true')
a = p.parse_args()
with socket.create_connection((a.host,7624),timeout=8) as s:
    for cmd in ('(GET:VERSION:0)','(GET:OTA:0)','(GET:CLOSED:0)','(GET:OPENED:0)'):
        s.sendall(cmd.encode()); print(u.line(s))
    if a.parser_timeout:
        started=time.monotonic(); s.sendall(b'(GET:VERS')
        reply=u.line(s)
        if reply != '(NAK:ERROR:CommandTimeout)': raise SystemExit('FAIL: '+reply)
        print('Parser timeout:',round(time.monotonic()-started,2),'seconds')
        s.sendall(b'(GET:VERSION:0)'); print('Recovery:',u.line(s))
