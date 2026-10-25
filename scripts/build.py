"""Portable build entry point, including Windows workspaces with Unicode names."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
EXE = '.exe' if os.name == 'nt' else ''
CC = shutil.which(os.environ.get('CC', 'gcc'))
if not CC:
    sys.exit('GCC not found. Install build-essential (Ubuntu) or an MSYS2 UCRT64 toolchain (Windows).')
os.environ['PATH'] = str(Path(CC).parent) + os.pathsep + os.environ.get('PATH', '')
PKG = shutil.which('pkg-config')
FLAGS = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Icommon', '-Iserver', '-Ivendor']
COMMON = ['common/net.c', 'common/protocol.c', 'vendor/cJSON.c']
NET = ['-lws2_32'] if os.name == 'nt' else []


def package(name, option):
    if not PKG:
        sys.exit('pkg-config not found.')
    result = subprocess.run([PKG, option, name], text=True, capture_output=True)
    if result.returncode:
        sys.exit(f'Missing dependency {name}. See README.md.\n{result.stderr}')
    return shlex.split(result.stdout)


def compile_target(name, sources, packages):
    target = str(Path('build') / (name + EXE))
    cflags, libs = [], []
    for dependency in packages:
        cflags += package(dependency, '--cflags')
        libs += package(dependency, '--libs')
    subprocess.run([CC, *FLAGS, *cflags, *sources, *COMMON, '-o', target, *libs, *NET], check=True)
    print('Built', target, flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('target', choices=['core', 'gui', 'all', 'test', 'integration'], nargs='?', default='core')
    args = parser.parse_args()
    Path('build').mkdir(exist_ok=True)
    if args.target in ('core', 'all', 'test', 'integration'):
        compile_target('chess-server', ['server/main.c', 'server/match.c', 'server/chess.c', 'server/storage.c'], ['sqlite3'])
        compile_target('test-core', ['tests/test_core.c', 'server/chess.c', 'server/storage.c'], ['sqlite3'])
    if args.target in ('gui', 'all'):
        compile_target('chess-client', ['client/main.c'], ['gtk4'])
    if args.target == 'test':
        subprocess.run([str(ROOT / 'build' / ('test-core' + EXE))], check=True)
    if args.target == 'integration':
        subprocess.run([sys.executable, 'tests/integration.py'], check=True)


if __name__ == '__main__':
    main()
