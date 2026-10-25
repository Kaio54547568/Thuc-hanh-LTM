"""Run inside xvfb-run on Linux; GUI tests use actual GTK callbacks, no mock TCP."""
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
with socket.socket() as probe:
    probe.bind(('127.0.0.1', 0))
    port = str(probe.getsockname()[1])
with tempfile.TemporaryDirectory(prefix='chess-gui-') as folder:
    server = subprocess.Popen(['./build/chess-server', '127.0.0.1', port, str(Path(folder) / 'test.db')], stdout=subprocess.DEVNULL)
    children = []
    try:
        for attempt in range(60):
            try:
                with socket.create_connection(('127.0.0.1', int(port)), timeout=1):
                    break
            except OSError:
                time.sleep(.05)
        else:
            raise AssertionError('Server not ready')
        for name in ['Loc', 'Viet']:
            children.append(subprocess.Popen(['./build/test-gui', name, port]))
        codes = [process.wait(timeout=30) for process in children]
        assert codes == [0, 0], codes
        print('PASS two GTK windows with a real server; screenshots in build/gui-Loc.png and build/gui-Viet.png')
    finally:
        for process in children:
            if process.poll() is None:
                process.kill(); process.wait()
        server.terminate(); server.wait(timeout=5)
