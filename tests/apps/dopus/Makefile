# Note: Apps tests are one level deeper, so we need an extra ../
SELF_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BASEDIR := $(SELF_DIR)../../..

TOOLCHAIN_AMIGAOS_PREFIX = /opt/amiga
CC_AMIGAOS = $(TOOLCHAIN_AMIGAOS_PREFIX)/bin/m68k-amigaos-gcc

LXA_BIN = $(BASEDIR)/build/host/bin/lxa
LXA_ROM = $(BASEDIR)/build/target/rom/lxa.rom
LXA_SYS = $(BASEDIR)/build/target/sys

GREEN = \033[0;32m
RED = \033[0;31m
RESET = \033[0m

TARGET = dopus

CFLAGS = -march=68000 -mcpu=68000 -O2 -Wall -Werror -I$(BASEDIR)/src/include -mcrt=nix20
LDFLAGS = -march=68000 -mcpu=68000 -mcrt=nix20

OBJS = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC_AMIGAOS) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC_AMIGAOS) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) actual.out

run-test: $(TARGET)
	@echo "Running test: apps/dopus..."
	@rm -f actual.out
	@cd "$(dir $(firstword $(MAKEFILE_LIST)))" && \
	timeout 60 ../app_test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" "$(HOME)/.lxa/System/Apps" || \
	true
	@diff -q expected.out actual.out && \
		echo "$(GREEN)PASS: apps/dopus$(RESET)" || \
		(echo "$(RED)FAIL: apps/dopus$(RESET)" && diff expected.out actual.out)

.PHONY: all clean run-test
