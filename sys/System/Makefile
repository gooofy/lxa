# Makefile for lxa SYS:System tools (Phase 5)
# Cross-compiles Amiga system tools using m68k-amigaos-gcc

# Amiga cross-compiler
PREFIX ?= /opt/amiga
CC = $(PREFIX)/bin/m68k-amigaos-gcc
LD = $(PREFIX)/bin/m68k-amigaos-ld
OBJCOPY = $(PREFIX)/bin/m68k-amigaos-objcopy

# Compiler flags
CFLAGS = -O2 -Wall -march=68000 -mcpu=68000
CFLAGS += -I$(PREFIX)/m68k-amigaos/include
CFLAGS += -I$(PREFIX)/m68k-amigaos/sys-include
# Include project headers if needed
# CFLAGS += -I../../src/include

# Linker flags - use nix20 runtime
LDFLAGS = -mcrt=nix20

# Source files
TOOLS = Shell

# Default target
all: $(TOOLS)

# Pattern rule for compiling C to Amiga executable
%: %.c
	@echo "CC $@"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# Clean target
clean:
	rm -f $(TOOLS)

# Install target
install: all
	@echo "System tools built in sys/System/"

.PHONY: all clean install
