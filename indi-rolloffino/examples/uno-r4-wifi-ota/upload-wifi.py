#!/usr/bin/env python3
"""Explicit attended LAN update. No discovery, port forwarding or motion replay."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import socket
import subprocess
import time
import zlib
import functools

ROOT=Path(__file__).resolve().parent

def line(sock):
    result=bytearray()
    while len(result)<256:
        b=sock.recv(1)
        if not b: raise RuntimeError('Connection closed before complete reply')
        if b==b'\n': return result.decode('ascii').strip()
        result+=b
    raise RuntimeError('Reply too long')

def command(host, text, port=7624):
    with socket.create_connection((host,port),timeout=8) as sock:
        sock.sendall(text.encode('ascii'))
        return line(sock)

def parked():
    raise RuntimeError('An explicit mount park checker is required; no transfer attempted')

def inspect_image(binary,manifest,token):
    data=binary.read_bytes()
    if len(data)!=manifest['size'] or hashlib.sha256(data).hexdigest()!=manifest['sha256']:
        raise RuntimeError('Firmware differs from build manifest')
    if manifest['board']!='arduino:renesas_uno:unor4wifi': raise RuntimeError('Wrong board')
    if not 256<=len(data)<=112*1024: raise RuntimeError('Firmware size outside budget')
    if manifest['build_id'].encode() not in data or token.encode() not in data:
        raise RuntimeError('Firmware lacks expected identity or management token')
    return data

def standard_parked(mount, host, port):
    """Require an unambiguous connected, completed standard INDI park."""
    expected = {
        mount+'.CONNECTION.CONNECT': 'On',
        mount+'.TELESCOPE_PARK.PARK': 'On',
        mount+'.TELESCOPE_PARK.UNPARK': 'Off',
        mount+'.TELESCOPE_PARK._STATE': 'Ok',
    }
    result = subprocess.run(['indi_getprop', '-h', host, '-p', str(port),
                             '-t', '5', *expected],
                            capture_output=True, text=True, timeout=12)
    values = {}
    for row in result.stdout.splitlines():
        if '=' not in row:
            continue
        key, value = row.split('=', 1)
        if key in values and values[key] != value:
            raise RuntimeError('Conflicting mount telemetry; upload refused')
        values[key] = value
    if result.returncode or any(values.get(k) != v for k, v in expected.items()):
        raise RuntimeError('Connected standard INDI PARKED/Ok confirmation unavailable')

def transfer(host,data,token,port=7625):
    header=f'ROOFOTA1 {token} {len(data)} {zlib.crc32(data):08x}\n'.encode('ascii')
    with socket.create_connection((host,port),timeout=8) as sock:
        sock.settimeout(30)
        sock.sendall(header)
        sock.sendall(data)
        reply=line(sock)
        if reply!='READY': raise RuntimeError('Image rejected: '+reply)
        # CRC and flash readback succeeded, but receiver has not applied yet.
        sock.sendall(b'COMMIT\n')
        reply=line(sock)
        if reply!='APPLYING': raise RuntimeError('Commit rejected: '+reply)

def stage_update(host, data, token, expected_current, new_id, port=7624, park_check=None):
    """One roof-command session; separate OTA socket, no upload retries."""
    with socket.create_connection((host, port), timeout=8) as control:
        def query(label, text):
            print('CHECK:', label, flush=True)
            try:
                control.sendall(text.encode('ascii'))
                return line(control)
            except (OSError, RuntimeError) as error:
                raise RuntimeError(label + ' failed: ' + str(error)) from error

        current = query('Firmware version', '(GET:VERSION:0)')
        if current != '(ACK:VERSION:' + expected_current + ')':
            raise RuntimeError('Installed identity mismatch: ' + current)
        if expected_current == new_id:
            raise RuntimeError('New identity must differ for this proof update')
        print('CHECK: Mount parked', flush=True)
        (park_check or parked)()
        if query('Closed limit', '(GET:CLOSED:0)') != '(ACK:CLOSED:ON)':
            raise RuntimeError('Physical closed limit not confirmed')
        if query('Open limit', '(GET:OPENED:0)') != '(ACK:OPENED:OFF)':
            raise RuntimeError('Open limit unexpectedly active; update refused')
        print('Updating', expected_current, 'to', new_id, flush=True)
        # Mark the entry attempt before writing: a lost ACK is ambiguous.
        try:
            if query('Enter OTA maintenance', '(SET:OTA:' + token + ')') != '(ACK:OTA:MAINTENANCE)':
                raise RuntimeError('Maintenance entry refused')
            print('CHECK: Firmware transfer and explicit commit', flush=True)
            transfer(host, data, token)
        except (OSError, RuntimeError):
            # Best-effort cancel over the SAME connection. Never retry COMMIT.
            try:
                control.sendall(b'(SET:OTA:OFF)')
                line(control)
            except (OSError, RuntimeError):
                pass
            raise
    # APPLYING was acknowledged. Reboot disables maintenance; verify the new ID
    # through fresh connections below. A socket ACK is not installation proof.

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host',required=True,help='Controller LAN address, not INDI server')
    p.add_argument('--indi-host',default='localhost')
    p.add_argument('--indi-port',type=int,default=7624)
    p.add_argument('--mount',help='Exact INDI telescope device name; standard park check')
    p.add_argument('--status',action='store_true',help='Read firmware identity; no update')
    p.add_argument('--attended',action='store_true',help='Confirm Scheduler stopped, roof idle and attended')
    p.add_argument('--expected-current',help='Exact installed build ID from --status')
    a=p.parse_args()
    if a.status:
        print(command(a.host,'(GET:VERSION:0)'));return
    if not a.attended or not a.expected_current:
        p.error('Upload requires --attended and --expected-current BUILD_ID')
    if not a.mount:
        p.error('Upload requires --mount with a connected parked INDI telescope')
    token=(ROOT/'ota-token.txt').read_text().strip()
    if not re.fullmatch('[0-9a-f]{32}',token): raise RuntimeError('Invalid token file')
    manifest=json.loads((ROOT/'build/firmware.json').read_text())
    data=inspect_image(ROOT/'build'/manifest['file'],manifest,token)
    stage_update(a.host, data, token, a.expected_current, manifest['build_id'],
                 park_check=functools.partial(standard_parked, a.mount, a.indi_host, a.indi_port))
    deadline=time.monotonic()+60
    while time.monotonic()<deadline:
        try:
            if command(a.host,'(GET:VERSION:0)')=='(ACK:VERSION:'+manifest['build_id']+')':
                print('VERIFIED installed firmware:',manifest['build_id'])
                print('Reconnect RollOff ino and verify physical limits before resuming.')
                return
        except (OSError,RuntimeError):
            pass
        time.sleep(2)
    raise RuntimeError('New firmware not confirmed after commit; inspect --status. No automatic retry or motion command sent.')

if __name__=='__main__':
    try: main()
    except (OSError,ValueError,KeyError,RuntimeError,subprocess.SubprocessError) as e:
        raise SystemExit(str(e))

