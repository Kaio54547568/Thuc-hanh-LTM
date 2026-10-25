"""Real TCP integration; Python is test tooling, application code is C."""
import json
import os
from pathlib import Path
import socket
import sqlite3
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / 'build' / ('chess-server.exe' if os.name == 'nt' else 'chess-server')


def encode(message):
    data = json.dumps(message, separators=(',', ':')).encode()
    return struct.pack('!I', len(data)) + data


class Peer:
    def __init__(self, port):
        self.sock = socket.create_connection(('127.0.0.1', port), timeout=3)
        self.sock.settimeout(4)
        self.seq = 0

    def send(self, kind, **fields):
        self.seq += 1
        request = dict(type=kind, request_id=str(self.seq), **fields)
        self.sock.sendall(encode(request))
        return str(self.seq)

    def exact(self, n):
        data = b''
        while len(data) < n:
            part = self.sock.recv(n - len(data))
            if not part:
                raise EOFError('Server disconnected')
            data += part
        return data

    def receive(self):
        size, = struct.unpack('!I', self.exact(4))
        assert 0 < size <= 16384
        return json.loads(self.exact(size))

    def until(self, kind):
        for _ in range(40):
            event = self.receive()
            if event['type'] == kind:
                return event
        raise AssertionError(f'No {kind}')

    def close(self):
        self.sock.close()


def run():
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix='chess-e2e-') as folder:
        database = Path(folder) / 'test.db'
        log = open(Path(folder) / 'server.log', 'w+')
        process = subprocess.Popen([str(SERVER), '127.0.0.1', str(port), str(database)], stdout=log, stderr=log)
        peers = []
        try:
            for _ in range(60):
                if process.poll() is not None:
                    raise AssertionError('Server exited at startup')
                try:
                    a = Peer(port)
                    peers.append(a)
                    break
                except OSError:
                    time.sleep(.05)
            else:
                raise AssertionError('Server did not listen')
            # Split every byte, including the length prefix, across writes.
            for byte in encode(dict(type='HELLO', request_id='split', name='Loc')):
                a.sock.sendall(bytes([byte]))
            assert a.until('WELCOME')['request_id'] == 'split'
            b = Peer(port); peers.append(b)
            # Coalesce two full application messages into one send.
            b.sock.sendall(encode(dict(type='HELLO', request_id='join', name='Viet')) + encode(dict(type='PING', request_id='ping')))
            b.until('WELCOME')
            assert b.until('PONG')['request_id'] == 'ping'
            a.send('LIST')
            while True:
                lobby = a.until('LOBBY')
                if len(lobby['players']) == 2:
                    break
            duplicate = Peer(port); peers.append(duplicate)
            duplicate.send('HELLO', name='Loc')
            assert duplicate.until('ERROR')['code'] == 'NAME_TAKEN'
            duplicate.close(); peers.remove(duplicate)
            a.send('CHALLENGE', target='Loc')
            assert a.until('ERROR')['code'] == 'BAD_TARGET'
            a.send('CHALLENGE', target='Viet'); a.until('CHALLENGE_SENT'); b.until('INVITE')
            b.send('DECLINE'); assert a.until('INVITE_CLOSED')['reason'] == 'declined'; b.until('OK')
            a.send('CHALLENGE', target='Viet'); a.until('CHALLENGE_SENT'); b.until('INVITE'); b.send('ACCEPT')
            first = a.until('STATE'); second = b.until('STATE')
            assert first['board'] == second['board'] and first['white'] == 'Loc'
            room = first['room_id']
            b.send('MOVE', room_id=room, uci='e7e5')
            assert b.until('ERROR')['code'] == 'NOT_YOUR_TURN'
            a.send('MOVE', room_id=room + 1, uci='e2e4')
            assert a.until('ERROR')['code'] == 'BAD_ROOM'
            a.send('MOVE', room_id=room, uci='e2e5')
            assert a.until('ERROR')['code'] == 'ILLEGAL_MOVE'
            # Fool's mate verifies move relay, checkmate, stored outcome.
            for peer, uci in [(a, 'f2f3'), (b, 'e7e5'), (a, 'g2g4'), (b, 'd8h4')]:
                reqid = peer.send('MOVE', room_id=room, uci=uci)
                left, right = a.until('STATE'), b.until('STATE')
                assert left == right and left['request_id'] == reqid and left['last_move'] == uci
            assert a.until('GAME_OVER')['reason'] == 'checkmate'
            assert b.until('GAME_OVER')['result'] == '0-1'
            a.send('MOVE', room_id=room, uci='a2a3')
            assert a.until('ERROR')['code'] == 'NOT_IN_GAME'
            # New game with opposite colors, then resignation.
            b.send('CHALLENGE', target='Loc'); b.until('CHALLENGE_SENT'); a.until('INVITE'); a.send('ACCEPT')
            current = a.until('STATE'); b.until('STATE'); assert current['white'] == 'Viet'
            a.send('RESIGN', room_id=current['room_id'])
            assert a.until('GAME_OVER')['result'] == '1-0'
            b.until('GAME_OVER')
            # Abrupt disconnect releases room and presence.
            a.send('CHALLENGE', target='Viet'); a.until('CHALLENGE_SENT'); b.until('INVITE'); b.send('ACCEPT')
            a.until('STATE'); b.until('STATE'); b.close(); peers.remove(b)
            assert a.until('GAME_OVER')['reason'] == 'disconnect'
            a.send('LIST')
            while True:
                players = a.until('LOBBY')['players']
                if len(players) == 1:
                    break
            assert players[0] == dict(name='Loc', status='ready')
            # Invalid frame disconnects only the offender.
            bad = Peer(port); peers.append(bad)
            bad.sock.sendall(struct.pack('!I', 16385))
            assert bad.sock.recv(1) == b''
            bad.close(); peers.remove(bad)
            a.send('PING'); a.until('PONG')
            with sqlite3.connect(database) as db:
                games = db.execute('SELECT result,reason FROM games ORDER BY id').fetchall()
                assert games == [('0-1', 'checkmate'), ('1-0', 'resign'), ('1-0', 'disconnect')], games
                assert db.execute('SELECT uci FROM moves ORDER BY game_id,ply').fetchall() == [('f2f3',), ('e7e5',), ('g2g4',), ('d8h4',)]
                assert db.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
            db.close()
            print('PASS TCP: split/coalesced frames, lobby, unique names, invitations, legal moves, turn/room errors, checkmate, resignation, disconnect, SQLite persistence')
        except BaseException:
            log.flush(); log.seek(0); print(log.read())
            raise
        finally:
            for peer in peers:
                peer.close()
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
            log.close()


if __name__ == '__main__':
    run()
