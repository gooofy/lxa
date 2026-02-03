# Test: intuition/requester_basic

# Paths
LXA_BIN := ../../../build/host/bin/lxa
LXA_ROM := ../../../build/target/rom/lxa.rom
LXA_SYS := ../../../build/target/sys
TEST_RUNNER := ../../test_runner.sh

# Amiga compiler
CC := /opt/amiga/bin/m68k-amigaos-gcc
CFLAGS := -march=68000 -mcpu=68000 -O2 -Wall -Werror -I../../../src/include -mcrt=nix20
LDFLAGS := -march=68000 -mcpu=68000 -mcrt=nix20

# Test binary
TARGET := requester_basic
OBJS := main.o

.PHONY: all clean run-test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run-test: $(TARGET)
	@echo "Running test: intuition/requester_basic..."
	@bash $(TEST_RUNNER) $(LXA_BIN) $(LXA_ROM) $(TARGET) expected.out actual.out $(LXA_SYS)
	@echo "PASS: intuition/requester_basic"

clean:
	rm -f $(TARGET) $(OBJS) actual.out
