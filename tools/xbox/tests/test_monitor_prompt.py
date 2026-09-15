"""Exercise real sockets with delayed, fragmented HMP command completion."""
import importlib.util
from pathlib import Path
import socket
import threading
import time

path = Path(__file__).resolve().parents[3] / 'scripts/xbox/poll_xemu_ram_log.py'
spec = importlib.util.spec_from_file_location('poll', path)
poll = importlib.util.module_from_spec(spec)
spec.loader.exec_module(poll)

def run(parts):
    client, server = socket.socketpair()
    def respond():
        try:
            assert server.recv(100) == b'xp/1wx 0x00000000\r'
            for delay, data in parts:
                time.sleep(delay)
                server.sendall(data)
        finally:
            server.close()
    thread = threading.Thread(target=respond)
    thread.start()
    try:
        return poll.monitor_cmd(client, 'xp/1wx 0x00000000')
    finally:
        client.close()
        thread.join()

reply = run([(0, b'00000000: 12345678\r\n'), (.9, b'\x1b[0m(qe'), (.01, b'mu) ')])
assert poll.parse_words(reply) == [0x12345678]
try:
    run([(0, b'00000000: 1234')])
except RuntimeError as exc:
    assert 'closed' in str(exc)
else:
    raise AssertionError('partial reply must not be accepted')
poll.monitor_cmd = lambda *args: '00000000: 12345678\n(qemu) '
try:
    poll.read_words(None, 0, 2, None)
except RuntimeError as exc:
    assert 'expected 2 words, got 1' in str(exc)
else:
    raise AssertionError('wrong memory word count must not be accepted')
print('PASS: delayed and fragmented prompt, ANSI stripping, incomplete reply rejection')
