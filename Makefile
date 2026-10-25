CC = gcc
PKG_CONFIG ?= pkg-config
PYTHON ?= python3
CPPFLAGS += -Icommon -Iserver -Ivendor
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
COMMON = common/net.c common/protocol.c vendor/cJSON.c
ifeq ($(OS),Windows_NT)
EXE = .exe
NETLIB = -lws2_32
endif
SQL_FLAGS := $(shell $(PKG_CONFIG) --cflags sqlite3)
SQL_LIBS := $(shell $(PKG_CONFIG) --libs sqlite3)
GTK_FLAGS = $(shell $(PKG_CONFIG) --cflags gtk4)
GTK_LIBS = $(shell $(PKG_CONFIG) --libs gtk4)
.PHONY: all core gui test integration gui-test clean
all: core gui
core: build/chess-server$(EXE) build/test-core$(EXE)
gui: build/chess-client$(EXE)
build:
	mkdir -p build
build/chess-server$(EXE): server/main.c server/match.c server/chess.c server/storage.c $(COMMON) common/*.h server/*.h vendor/cJSON.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SQL_FLAGS) server/main.c server/match.c server/chess.c server/storage.c $(COMMON) -o $@ $(SQL_LIBS) $(NETLIB)
build/chess-client$(EXE): client/main.c $(COMMON) common/*.h vendor/cJSON.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_FLAGS) client/main.c $(COMMON) -o $@ $(GTK_LIBS) $(NETLIB)
build/test-core$(EXE): tests/test_core.c server/chess.c server/storage.c $(COMMON) common/*.h server/*.h vendor/cJSON.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SQL_FLAGS) tests/test_core.c server/chess.c server/storage.c $(COMMON) -o $@ $(SQL_LIBS) $(NETLIB)
test: core
	./build/test-core$(EXE)
integration: core
	$(PYTHON) tests/integration.py
build/test-gui$(EXE): tests/test_gui.c client/main.c $(COMMON) common/*.h vendor/cJSON.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_FLAGS) tests/test_gui.c $(COMMON) -o $@ $(GTK_LIBS) $(NETLIB)
gui-test: core build/test-gui$(EXE)
	GTK_A11Y=none GSK_RENDERER=cairo xvfb-run -a python3 tests/gui_smoke.py
clean:
	rm -f build/chess-server* build/chess-client* build/test-core*
