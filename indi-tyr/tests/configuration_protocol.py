#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Prometheus Astro
"""Exercise settings against a pseudo-serial controller; never opens mount hardware."""
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

device = 'Prometheus Astro Tyr'
tcp = len(sys.argv) > 2 and sys.argv[2] == 'tcp'
with tempfile.TemporaryDirectory() as directory:
    work = pathlib.Path(directory)
    processes = []
    output = open(work / 'server.log', 'w+')
    try:
        processes.append(subprocess.Popen([sys.executable, str(pathlib.Path(__file__).with_name('protocol_emulator.py')), str(work / 'port'), str(work / 'protocol.log')] + (['tcp'] if tcp else [])))
        deadline = time.monotonic() + 5
        while not (work / 'port').exists():
            assert time.monotonic() < deadline
            time.sleep(.05)
        env = dict(os.environ, HOME=directory)
        processes.append(subprocess.Popen(['indiserver', '-u', '/tmp/tyr-settings-' + str(os.getpid()), '-p', '17626', sys.argv[1]], env=env, stdout=output, stderr=output))
        deadline = time.monotonic() + 5
        while True:
            try:
                connection = socket.create_connection(('localhost', 17626), timeout=.2)
                break
            except OSError:
                assert time.monotonic() < deadline
                time.sleep(.05)
        parser = ET.XMLPullParser(events=['end'])
        parser.feed('<stream>')
        properties = {}
        sequence = 0
        def pump():
            global sequence
            try: chunk = connection.recv(65536)
            except socket.timeout: return
            assert chunk, 'INDI connection closed'
            parser.feed(chunk.decode('utf-8'))
            for _, element in parser.read_events():
                if element.tag.endswith('Vector'):
                    sequence += 1
                    properties[element.get('name')] = (sequence, element.get('state'), {child.get('name'): (child.text or '').strip() for child in element})
        def wait(name, after=0, state=None):
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                value = properties.get(name)
                if value and value[0] > after and (state is None or value[1] == state): return value
                pump()
            raise AssertionError(f'Timed out: {name}, state={state}, last={properties.get(name)}')
        def send(kind, name, values, state=None):
            before = sequence
            body = ''.join(f'<one{kind} name="{escape(key)}">{escape(str(value))}</one{kind}>' for key, value in values.items())
            connection.sendall(f'<new{kind}Vector device="{device}" name="{name}">{body}</new{kind}Vector>'.encode())
            return wait(name, before, state)
        connection.sendall(b'<getProperties version="1.7"/>')
        wait('CONNECTION')
        if tcp:
            send('Switch', 'CONNECTION_MODE', {'CONNECTION_TCP': 'On'})
            send('Text', 'DEVICE_ADDRESS', {'ADDRESS': '127.0.0.1', 'PORT': (work / 'port').read_text()})
        else:
            send('Switch', 'DEVICE_AUTO_SEARCH', {'INDI_DISABLED': 'On'})
            send('Text', 'DEVICE_PORT', {'PORT': (work / 'port').read_text()})
        send('Switch', 'CONNECTION', {'CONNECT': 'On'}, 'Ok')
        wait('TYR_BLUETOOTH_STATE', state='Ok')  # Final initial settings read.
        assert float(wait('TYR_AXIS_RA')[2]['STEPS_PER_DEGREE']) == 26666.667
        assert float(wait('TYR_STANDARD_TIME_OFFSET')[2]['HOURS']) == -2
        if tcp:
            send('Switch', 'ON_COORD_SET', {'SYNC': 'On'})
            send('Number', 'EQUATORIAL_EOD_COORD', {'RA': 3, 'DEC': 40}, 'Ok')
            assert 'CS\n' in (work / 'protocol.log').read_text()
        # KStars' summer +3 must not become Tyr -3 or shift the UTC instant.
        send('Text', 'TIME_UTC', {'UTC': '2026-10-12T17:05:20', 'OFFSET': '3'}, 'Ok')
        assert properties['TIME_UTC'][2]['OFFSET'] == '3', 'Keep the INDI client offset contract'
        assert float(properties['TYR_STANDARD_TIME_OFFSET'][2]['HOURS']) == -2
        time_log = (work / 'protocol.log').read_text()
        assert 'SC10/12/26\n' in time_log and 'SL19:05:20\n' in time_log
        assert 'SG-03' not in time_log, 'Client DST must not overwrite the standard zone'
        send('Number', 'TYR_HOME_OFFSETS', {'RA': 120, 'DEC': -240}, 'Ok')
        assert float(properties['TYR_HOME_OFFSETS'][2]['DEC']) == -240
        send('Number', 'TYR_MERIDIAN_LIMITS', {'EAST': 1.25, 'WEST': 2.5}, 'Ok')
        send('Number', 'TYR_STANDARD_TIME_OFFSET', {'HOURS': -5.75}, 'Ok')
        send('Text', 'TIME_UTC', {'UTC': '2026-10-12T23:30:00', 'OFFSET': '5.75'}, 'Ok')
        time_log = (work / 'protocol.log').read_text()
        assert 'SC10/13/26\n' in time_log and 'SL05:15:00\n' in time_log, 'Fractional offset with date rollover'
        send('Number', 'TYR_AXIS_DEC', {'STEPS_PER_DEGREE': 28444.444, 'PARAMETER_5': 900}, 'Ok')
        send('Switch', 'TYR_AUTO_HOME', {'OFF': 'On'}, 'Ok')
        assert properties['TYR_AUTO_HOME'][2]['OFF'] == 'On'
        send('Number', 'TYR_ALTITUDE_LIMITS', {'HORIZON': 13}, 'Alert')
        assert float(properties['TYR_ALTITUDE_LIMITS'][2]['HORIZON']) == 0, 'Rejected firmware write must retain old value'
        send('Number', 'TYR_HOME_OFFSETS', {'RA': 600, 'DEC': 1.5}, 'Alert')
        assert float(properties['TYR_HOME_OFFSETS'][2]['RA']) == 120, 'Invalid second field must prevent first write'
        send('Switch', 'TELESCOPE_TRACK_STATE', {'TRACK_ON': 'On'}, 'Busy')
        send('Number', 'TYR_HOME_OFFSETS', {'RA': 999}, 'Alert')
        assert float(properties['TYR_HOME_OFFSETS'][2]['RA']) == 120
        send('Switch', 'TELESCOPE_TRACK_STATE', {'TRACK_OFF': 'On'})
        send('Text', 'TYR_NETWORK_AP', {'SSID': 'Customer WiFi'}, 'Busy')
        # Password property deliberately never echoes inputs.
        connection.sendall(f'<newTextVector device="{device}" name="TYR_NETWORK_NEW_PASSWORDS"><oneText name="AP">test-secret-123</oneText></newTextVector>'.encode())
        send('Switch', 'TYR_NETWORK_APPLY', {'APPLY': 'On'}, 'Ok')
        assert properties['TYR_NETWORK_AP'][2]['SSID'] == 'Customer WiFi'
        send('Text', 'TYR_NETWORK_AP', {'SSID': 'REJECT'}, 'Busy')
        send('Switch', 'TYR_NETWORK_APPLY', {'APPLY': 'On'}, 'Alert')
        send('Switch', 'TYR_READ_SETTINGS', {'NETWORK': 'On'}, 'Ok')
        assert properties['TYR_NETWORK_AP'][2]['SSID'] == 'Customer WiFi', 'Failed transaction must not commit'
        send('Switch', 'TYR_BLUETOOTH_ACTION', {'ENABLE': 'On'}, 'Ok')
        wait('TYR_BLUETOOTH_STATE', state='Ok')
        log = (work / 'protocol.log').read_text()
        for command in ['hC1,120', 'hC2,-240', 'SXE9,5', 'SXEA,10', 'SG-05:45', 'hA0']:
            assert 'MANAGED ' + command + '\n' in log, command
        for forbidden in ['hC1,600', 'hC1,999']:
            assert 'MANAGED ' + forbidden + '\n' not in log, forbidden
        assert 'test-secret-123' not in log
        assert 'test-secret-123' not in (work / 'server.log').read_text()
        assert all('test-secret-123' not in str(value) for value in properties.values())
        print('Tyr configuration protocol checks passed: readback, units, rejection, motion guard, atomic network save, secret redaction.')
    except Exception:
        output.flush()
        print((work / 'server.log').read_text(), file=sys.stderr)
        if (work / 'protocol.log').exists(): print((work / 'protocol.log').read_text(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes):
            process.terminate()
            try: process.wait(timeout=3)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
        output.close()
