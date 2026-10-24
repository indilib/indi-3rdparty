#!/usr/bin/env python3
"""Attended relay observation only; motor power must be physically off."""
import argparse
import importlib.util
from pathlib import Path
import socket
import time
spec=importlib.util.spec_from_file_location('uploader',Path(__file__).with_name('upload-wifi.py'))
u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--host',required=True)
p.add_argument('--motor-off',action='store_true')
p.add_argument('--disconnect',action='store_true')
p.add_argument('--delay',type=int,default=0)
a=p.parse_args()
if not a.motor_off: p.error('Switch off motor power and explicitly supply --motor-off')
if not 0<=a.delay<=120: p.error('delay must be 0..120 seconds')
print('Motor must be OFF; RollOff ino disconnected. OPEN, CLOSE, STOP in that order.',flush=True)
time.sleep(a.delay)
if a.disconnect:
    for action in ('OPEN','CLOSE','ABORT'):
        with socket.create_connection((a.host,7624),timeout=8) as s:
            s.sendall(f'(SET:{action}:ON)'.encode())
            response=u.line(s)
            if response!=f'(ACK:{action}:ON)': raise SystemExit('FAIL: '+response)
            print(action,response,flush=True);time.sleep(.1)
        time.sleep(10)
else:
    with socket.create_connection((a.host,7624),timeout=8) as s:
        for action in ('OPEN','CLOSE','ABORT'):
            s.sendall(f'(SET:{action}:ON)'.encode())
            response=u.line(s)
            if response!=f'(ACK:{action}:ON)': raise SystemExit('FAIL: '+response)
            print(action,response,flush=True);time.sleep(10)
print('Completed commands; physical PASS requires your observation of every release.')
