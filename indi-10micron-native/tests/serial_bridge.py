"""PTY transport for the existing independent loopback mount emulators."""
import os, pty, select, socket, termios, threading

def configure(send, port):
    master, slave = pty.openpty()
    path = os.ttyname(slave)
    # Keep slave open until the driver opens it; avoid a pre-connect EIO race.
    def relay():
        with socket.create_connection(('127.0.0.1', port)) as peer:
            while True:
                readable, _, _ = select.select([master, peer], [], [], 30)
                if not readable: return
                for src in readable:
                    try: data = os.read(master, 4096) if src == master else peer.recv(4096)
                    except OSError: return
                    if not data: return
                    if src == master: peer.sendall(data)
                    else:
                        for byte in data: os.write(master, bytes([byte]))  # fragmented replies
    threading.Thread(target=relay, daemon=True).start()
    send('newTextVector', 'NATIVE_SERIAL', {'DEVICE': path, 'BAUD': '9600'})
    send('newSwitchVector', 'NATIVE_TRANSPORT', {'TCP': 'Off', 'SERIAL': 'On'})
    return slave

def check(slave):
    cfg = termios.tcgetattr(slave)
    assert cfg[4] == termios.B9600 and cfg[5] == termios.B9600, cfg
    assert cfg[2] & termios.CSIZE == termios.CS8
    assert not cfg[2] & (termios.PARENB | termios.CSTOPB | termios.CRTSCTS)
    assert not cfg[0] & (termios.IXON | termios.IXOFF)
    assert not cfg[3] & (termios.ICANON | termios.ECHO)
