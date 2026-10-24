#!/usr/bin/env python3
import contextlib
import importlib.util
import io
from pathlib import Path
import socket
import struct
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).parent
spec = importlib.util.spec_from_file_location('uploader', ROOT / 'upload-wifi.py')
u = importlib.util.module_from_spec(spec)
spec.loader.exec_module(u)


class RoofServer:
    """Real TCP command server rejecting a rapid second client connection."""
    def __init__(self, bad_limit=False):
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(3)
        self.listener.settimeout(0.1)
        self.port = self.listener.getsockname()[1]
        self.connections, self.commands, self.errors = 0, [], []
        self.bad_limit = bad_limit
        self.stop = threading.Event()
        self.worker = threading.Thread(target=self.serve)

    def __enter__(self):
        self.worker.start()
        return self

    def __exit__(self, *args):
        self.stop.set()
        self.worker.join(3)
        self.listener.close()
        assert not self.worker.is_alive()
        assert not self.errors, self.errors

    def serve(self):
        closed = None
        replies = {
            '(GET:VERSION:0)': '(ACK:VERSION:old)',
            '(GET:CLOSED:0)': '(ACK:CLOSED:OFF)' if self.bad_limit else '(ACK:CLOSED:ON)',
            '(GET:OPENED:0)': '(ACK:OPENED:OFF)',
            '(SET:OTA:' + 'a' * 32 + ')': '(ACK:OTA:MAINTENANCE)',
            '(SET:OTA:OFF)': '(ACK:OTA:DISABLED)',
        }
        try:
            while not self.stop.is_set():
                try:
                    conn, _ = self.listener.accept()
                except socket.timeout:
                    continue
                self.connections += 1
                with conn:
                    if closed is not None and time.monotonic() - closed < 1:
                        conn.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
                        continue
                    conn.settimeout(0.1)
                    buffer = ''
                    while not self.stop.is_set():
                        try:
                            char = conn.recv(1)
                        except socket.timeout:
                            continue
                        if not char:
                            break
                        buffer += char.decode('ascii')
                        if char == b')':
                            self.commands.append(buffer)
                            conn.sendall((replies.get(buffer, '(NAK:ERROR:Unknown)') + '\n').encode())
                            buffer = ''
                closed = time.monotonic()
        except Exception as error:
            self.errors.append(error)


class SessionTests(unittest.TestCase):
    def test_old_separate_connections_reproduce_reset(self):
        with RoofServer() as server:
            self.assertEqual(u.command('127.0.0.1', '(GET:CLOSED:0)', server.port), '(ACK:CLOSED:ON)')
            with self.assertRaises((OSError, RuntimeError)):
                u.command('127.0.0.1', '(GET:OPENED:0)', server.port)

    def stage(self, server):
        with contextlib.redirect_stdout(io.StringIO()):
            u.stage_update('127.0.0.1', b'firmware', 'a'*32, 'old', 'new', server.port)

    def test_single_session_preflight_and_transfer_once(self):
        with RoofServer() as server, patch.object(u, 'parked') as park, patch.object(u, 'transfer') as transfer:
            self.stage(server)
            self.assertEqual(server.connections, 1)
            self.assertEqual(len(server.commands), 4)
            park.assert_called_once()
            transfer.assert_called_once_with('127.0.0.1', b'firmware', 'a'*32)

    def test_bad_limit_never_enters_maintenance(self):
        with RoofServer(bad_limit=True) as server, patch.object(u, 'parked'), patch.object(u, 'transfer') as transfer:
            with self.assertRaisesRegex(RuntimeError, 'closed limit'):
                self.stage(server)
            transfer.assert_not_called()
            self.assertFalse(any(x.startswith('(SET:OTA:') for x in server.commands))

    def test_bad_mount_never_enters_maintenance(self):
        with RoofServer() as server, patch.object(u, 'parked', side_effect=RuntimeError('not parked')), patch.object(u, 'transfer') as transfer:
            with self.assertRaisesRegex(RuntimeError, 'not parked'):
                self.stage(server)
            transfer.assert_not_called()
            self.assertEqual(server.commands, ['(GET:VERSION:0)'])

    def test_transfer_error_cancels_same_connection_no_retry(self):
        with RoofServer() as server, patch.object(u, 'parked'), patch.object(u, 'transfer', side_effect=ConnectionResetError('lost connection')) as transfer:
            with self.assertRaises(ConnectionResetError):
                self.stage(server)
            transfer.assert_called_once()
            self.assertEqual(server.connections, 1)
            self.assertEqual(server.commands[-1], '(SET:OTA:OFF)')

    def test_wrong_identity_never_transfers(self):
        with RoofServer() as server, patch.object(u, 'parked'), patch.object(u, 'transfer') as transfer:
            with contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(RuntimeError, 'identity mismatch'):
                u.stage_update('127.0.0.1', b'firmware', 'a'*32, 'unexpected', 'new', server.port)
            transfer.assert_not_called()


if __name__ == '__main__':
    unittest.main(verbosity=2)
