# Makefile for lxa SYS:C commands (Phase 4)
# Cross-compiles Amiga commands using m68k-amigaos-gcc

# Amiga cross-compiler
PREFIX ?= /opt/amiga
CC = $(PREFIX)/bin/m68k-amigaos-gcc
LD = $(PREFIX)/bin/m68k-amigaos-ld
OBJCOPY = $(PREFIX)/bin/m68k-amigaos-objcopy

# Compiler flags
CFLAGS = -O2 -Wall -march=68000 -mcpu=68000
CFLAGS += -I$(PREFIX)/m68k-amigaos/include
CFLAGS += -I$(PREFIX)/m68k-amigaos/sys-include

# Linker flags - use nix20 runtime
LDFLAGS = -mcrt=nix20

# Source files (Phase 4 essentials)
COMMANDS = dir type makedir delete

# Default target
all: $(COMMANDS)

# Pattern rule for compiling C to Amiga executable
%: %.c
	@echo "CC $@"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# Clean target
clean:
	rm -f $(COMMANDS)

# Install target (copies to system directory)
install: all
	@echo "Commands built in sys/C/"

.PHONY: all clean install
