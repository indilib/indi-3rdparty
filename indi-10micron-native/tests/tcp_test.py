"""Integration tests for the read-only probe; no hardware required."""
import socket
import subprocess
import sys
import threading
import time

probe = sys.argv[1]
expected = [b':U2#', b':GVP#', b':GVN#', b':GVZ#', b':Ginfo#']

def run(mode):
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    commands, errors = [], []
    replies = {b':GVP#': b'10micron GM1000HPS#', b':GVN#': b'2.15.1#',
               b':GVZ#': b'Q-TYPE2012#',
               b':Ginfo#': b'12.5,-20,E,123,45,2461318.5L,0,1,extra#'}
    def serve():
        try:
            listener.settimeout(3)
            with listener.accept()[0] as conn:
                conn.settimeout(3)
                buf = b''
                while True:
                    data = conn.recv(128)
                    if not data:
                        break
                    buf += data
                    while b'#' in buf:
                        command, buf = buf.split(b'#', 1)
                        command += b'#'
                        commands.append(command)
                        if command == b':U2#':
                            continue
                        if command not in replies:
                            raise AssertionError(f'unexpected command {command}')
                        if mode == 'timeout':
                            time.sleep(0.2)
                            return
                        if mode == 'drop':
                            conn.sendall(b'partial')
                            return
                        if mode == 'oversized':
                            conn.sendall(b'x' * 4097 + b'#')
                            return
                        reply = replies[command]
                        if mode == 'malformed' and command == b':Ginfo#':
                            reply = b'12,91,E,123,45,2461318,0,0#'
                        if mode == 'fragmented':
                            for b in reply:
                                conn.sendall(bytes([b]))
                                time.sleep(0.0002)
                        else:
                            conn.sendall(reply)
        except Exception as exc:
            errors.append(exc)
        finally:
            listener.close()
    worker = threading.Thread(target=serve)
    worker.start()
    result = subprocess.run([probe, '127.0.0.1', str(port), '100'],
                            capture_output=True, text=True, timeout=4)
    worker.join(4)
    assert not worker.is_alive()
    assert not errors, errors
    assert commands == expected[:len(commands)], commands
    if mode in ('normal', 'fragmented'):
        assert result.returncode == 0, result.stderr
        assert commands == expected
        assert 'Native status: 0 (tracking)' in result.stdout
        assert 'Leap second: 1' in result.stdout
    else:
        assert result.returncode == 1, result.stdout
        if mode in ('timeout', 'drop', 'oversized'):
            assert 'request=:GVP# phase=read' in result.stderr, result.stderr
            assert 'elapsed_ms=' in result.stderr and 'deadline_ms=100' in result.stderr
            assert 'received_bytes=' in result.stderr
        if mode == 'timeout':
            assert 'timeout' in result.stderr and 'received_bytes=0' in result.stderr
        if mode == 'drop':
            assert 'received_bytes=7' in result.stderr, result.stderr
    print(f'{mode}: passed')

for mode in ('normal', 'fragmented', 'timeout', 'drop', 'oversized', 'malformed'):
    run(mode)
