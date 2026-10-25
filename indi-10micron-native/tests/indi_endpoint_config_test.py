#!/usr/bin/env python3
"""Real INDI executable config regression; no server or hardware connection."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import xml.etree.ElementTree as ET

DEVICE = '10Micron Native Motion'

def run_case(binary, config, save=False):
    env = os.environ.copy()
    env['INDICONFIG'] = str(config)
    test_home = config.parent / 'test-home'
    (test_home / '.indi').mkdir(parents=True, exist_ok=True)
    env['HOME'] = str(test_home)
    proc = subprocess.Popen([binary], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, env=env)
    vectors, errors, stderr = [], [], []
    def read():
        parser = ET.XMLPullParser(events=('end',))
        parser.feed('<stream>')
        try:
            for line in proc.stdout:
                if line.lstrip().startswith('<?xml'):
                    continue
                parser.feed(line)
                for _, elem in parser.read_events():
                    if elem.tag.endswith('Vector'):
                        vectors.append((dict(elem.attrib), {e.get('name'): (e.text or '').strip() for e in elem}))
                        elem.clear()
        except Exception as exc:
            errors.append(exc)
    threading.Thread(target=read, daemon=True).start()
    threading.Thread(target=lambda: stderr.extend(proc.stderr), daemon=True).start()
    def send(xml):
        proc.stdin.write(xml + '\n')
        proc.stdin.flush()
    def until(predicate):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if predicate(): return
            if proc.poll() is not None: raise AssertionError('Driver exited: ' + ''.join(stderr))
            time.sleep(0.01)
        raise AssertionError((vectors[-5:], errors, stderr))
    try:
        send('<getProperties version="1.7"/>')
        until(lambda: any(a.get('name') == 'NATIVE_ENDPOINT' for a, _ in vectors))
        if save:
            send(f'<newTextVector device="{DEVICE}" name="NATIVE_ENDPOINT"><oneText name="HOST">192.0.2.73</oneText><oneText name="PORT">43579</oneText></newTextVector>')
            until(lambda: any(a.get('name') == 'NATIVE_ENDPOINT' and v.get('HOST') == '192.0.2.73' for a, v in vectors))
            send(f'<newTextVector device="{DEVICE}" name="NATIVE_SERIAL"><oneText name="DEVICE">/dev/serial/by-id/test-10u</oneText><oneText name="BAUD">19200</oneText></newTextVector>')
            send(f'<newSwitchVector device="{DEVICE}" name="NATIVE_TRANSPORT"><oneSwitch name="TCP">Off</oneSwitch><oneSwitch name="SERIAL">On</oneSwitch></newSwitchVector>')
            until(lambda: any(a.get('name') == 'NATIVE_TRANSPORT' and v.get('SERIAL') == 'On' for a,v in vectors))
            save_marker = len(vectors)
            send(f'<newSwitchVector device="{DEVICE}" name="CONFIG_PROCESS"><oneSwitch name="CONFIG_SAVE">On</oneSwitch></newSwitchVector>')
            until(lambda: any(a.get('name') == 'CONFIG_PROCESS' and a.get('state') == 'Ok' for a, _ in vectors[save_marker:]))
            until(lambda: config.exists() and '192.0.2.73' in config.read_text())
            root = ET.parse(config).getroot()
            saved = {e.get('name'): e.text.strip() for vector in root if vector.get('name') == 'NATIVE_ENDPOINT' for e in vector}
            assert saved == {'HOST': '192.0.2.73', 'PORT': '43579'}, saved
        else:
            endpoint = next(v for a, v in vectors if a.get('name') == 'NATIVE_ENDPOINT')
            expected = {'HOST': '192.0.2.73', 'PORT': '43579'} if config.exists() else {'HOST': '192.168.0.1', 'PORT': '3490'}
            assert endpoint == expected, (endpoint, expected)
            until(lambda: any(a.get('name')=='NATIVE_SERIAL' for a,_ in vectors))
            serial=next(v for a,v in vectors if a.get('name')=='NATIVE_SERIAL')
            mode=next(v for a,v in vectors if a.get('name')=='NATIVE_TRANSPORT')
            assert serial['BAUD']==('19200' if config.exists() else '9600'),serial
            assert mode['SERIAL']==('On' if config.exists() else 'Off'),mode
            if config.exists(): assert serial['DEVICE']=='/dev/serial/by-id/test-10u',serial
        # No CONNECTION request: this test must never talk to hardware.
        assert not errors, errors
    finally:
        proc.terminate()
        try: proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill(); proc.wait()

with tempfile.TemporaryDirectory(prefix='10u-endpoint-') as folder:
    config = Path(folder) / 'config.xml'
    run_case(sys.argv[1], config)
    run_case(sys.argv[1], config, save=True)
    run_case(sys.argv[1], config)
def enclosure_case(binary, config, expected_dome, expected_policy, change=None, manual_save=False):
    """Start a real driver process without CONNECTION; inspect/save standard config."""
    env = os.environ.copy()
    env['INDICONFIG'] = str(config)
    test_home = config.parent / 'test-home'
    (test_home / '.indi').mkdir(parents=True, exist_ok=True)
    env['HOME'] = str(test_home)
    proc = subprocess.Popen([binary], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, env=env)
    vectors, errors, stderr = [], [], []
    def read():
        parser = ET.XMLPullParser(events=('end',))
        parser.feed('<stream>')
        try:
            for line in proc.stdout:
                if line.lstrip().startswith('<?xml'): continue
                parser.feed(line)
                for _, elem in parser.read_events():
                    if elem.tag.endswith('Vector'):
                        vectors.append((dict(elem.attrib), {e.get('name'): (e.text or '').strip() for e in elem}))
                        elem.clear()
        except Exception as exc:
            errors.append(exc)
    threading.Thread(target=read, daemon=True).start()
    threading.Thread(target=lambda: stderr.extend(proc.stderr), daemon=True).start()
    def send(xml):
        proc.stdin.write(xml + '\n'); proc.stdin.flush()
    def until(predicate):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if predicate(): return
            if proc.poll() is not None: raise AssertionError('Driver exited: ' + ''.join(stderr))
            time.sleep(0.01)
        raise AssertionError((vectors[-8:], errors, stderr))
    def saved():
        try:
            return {v.get('name'): {e.get('name'): (e.text or '').strip() for e in v}
                    for v in ET.parse(config).getroot()}
        except (OSError, ET.ParseError): return {}
    try:
        send('<getProperties version="1.7"/>')
        until(lambda: all(any(a.get('name') == n for a, _ in vectors)
                          for n in ('ACTIVE_DEVICES', 'DOME_POLICY', 'NATIVE_ENDPOINT')))
        active = next(v for a, v in vectors if a.get('name') == 'ACTIVE_DEVICES')
        assert active['ACTIVE_DOME'] == expected_dome, ('Active Dome on restart', active, expected_dome)
        policy = next(v for a, v in vectors if a.get('name') == 'DOME_POLICY')
        assert policy[expected_policy] == 'On', ('Dome policy on restart', policy, expected_policy)
        if change is not None:
            from xml.sax.saxutils import escape
            marker = len(vectors)
            send(f'<newTextVector device="{DEVICE}" name="ACTIVE_DEVICES"><oneText name="ACTIVE_DOME">{escape(change)}</oneText><oneText name="ACTIVE_GPS">Test GPS</oneText></newTextVector>')
            until(lambda: any(a.get('name') == 'ACTIVE_DEVICES' and v.get('ACTIVE_DOME') == change
                              for a, v in vectors[marker:]))
            # The base INDI handler auto-saves Active Devices, without CONFIG_SAVE.
            until(lambda: saved().get('ACTIVE_DEVICES', {}).get('ACTIVE_DOME') == change)
            assert saved()['ACTIVE_DEVICES']['ACTIVE_GPS'] == 'Test GPS'
        if manual_save:
            send(f'<newSwitchVector device="{DEVICE}" name="DOME_POLICY"><oneSwitch name="DOME_IGNORED">Off</oneSwitch><oneSwitch name="DOME_LOCKS">On</oneSwitch></newSwitchVector>')
            until(lambda: any(a.get('name') == 'DOME_POLICY' and v.get('DOME_LOCKS') == 'On' for a, v in vectors))
            save_marker = len(vectors)
            send(f'<newSwitchVector device="{DEVICE}" name="CONFIG_PROCESS"><oneSwitch name="CONFIG_SAVE">On</oneSwitch></newSwitchVector>')
            until(lambda: any(a.get('name') == 'CONFIG_PROCESS' and a.get('state') == 'Ok' for a, _ in vectors[save_marker:]))
            until(lambda: saved().get('DOME_POLICY', {}).get('DOME_LOCKS') == 'On')
        assert not errors, errors
        # Deliberately never send CONNECTION: all tests are hardware-free.
    finally:
        proc.terminate()
        try: proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill(); proc.wait()

with tempfile.TemporaryDirectory(prefix='10u-enclosure-config-') as folder:
    config = Path(folder) / 'config.xml'
    enclosure_case(sys.argv[1], config, '', 'DOME_IGNORED', change='Example Enclosure')
    enclosure_case(sys.argv[1], config, 'Example Enclosure', 'DOME_IGNORED', manual_save=True)
    enclosure_case(sys.argv[1], config, 'Example Enclosure', 'DOME_LOCKS', change='RollOff ino', manual_save=True)
    enclosure_case(sys.argv[1], config, 'RollOff ino', 'DOME_LOCKS', change='')
    enclosure_case(sys.argv[1], config, '', 'DOME_LOCKS')
print('Endpoint and enclosure defaults, automatic/manual saves, named/empty Active Dome, policy, and process-restart restoration passed; no hardware used.')
