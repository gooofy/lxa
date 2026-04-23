# Phase 4 Implementation: Basic Userland & Metadata

This document details the Phase 4 implementation for lxa, which focuses on basic userland commands and file metadata APIs.

## Summary

Phase 4 adds essential file metadata operations and basic userland commands that form the foundation of a functional AmigaDOS environment.

## Implementation Details

### 1. Metadata APIs (src/lxa/lxa.c, src/rom/lxa_dos.c)

#### SetProtection()
- **Location:** `src/lxa/lxa.c:1350`
- **Function:** Maps Amiga protection bits to Linux file permissions
- **Emucall:** `EMU_CALL_DOS_SETPROTECTION (1019)`
- **Implementation:** 
  - Inverts Amiga protection bits (0 = protected, 1 = allowed in Amiga)
  - Maps owner/group/other bits to Unix permission bits
  - Uses `chmod()` system call
  - Default mode: 0644 if no permissions specified

#### SetComment()
- **Location:** `src/lxa/lxa.c:1395`
- **Function:** Sets file comments/metadata
- **Emucall:** `EMU_CALL_DOS_SETCOMMENT (1020)`
- **Implementation:**
  - Attempts to use Linux extended attributes (`xattr`) when available
  - Falls back to sidecar files (`.comment` extension)
  - Empty comment removes the xattr/sidecar file

### 2. SYS:C Commands (sys/C/)

All commands are cross-compiled Amiga executables using `m68k-amigaos-gcc`.

#### DIR - Directory Listing
- **Source:** `sys/C/dir.c`
- **Features:**
  - Simple or detailed (`-l`) listing
  - Hidden file support (`-a` flag)
  - Directory/file counts and total size in detailed mode
  - Protection bits display

#### TYPE - File Display
- **Source:** `sys/C/type.c`
- **Features:**
  - Text file display
  - Hex dump mode (`-h` or `-x`)
  - Line numbers (`-n`)

#### MAKEDIR - Directory Creation
- **Source:** `sys/C/makedir.c`
- **Features:**
  - Create single directories
  - Parent directory creation (`-p` flag)

#### DELETE - File/Directory Deletion
- **Source:** `sys/C/delete.c`
- **Features:**
  - Delete files
  - Delete empty directories
  - Recursive delete support (`-r` flag) - partial
  - Force mode (`-f` flag) - suppresses error messages

### 3. Build System (sys/C/Makefile)

Cross-compilation setup:
```makefile
CC = /opt/amiga/bin/m68k-amigaos-gcc
CFLAGS = -O2 -Wall -march=68000 -mcpu=68000
LDFLAGS = -mcrt=nix20
```

## Testing

The commands can be tested by:
1. Setting up a VFS configuration pointing to the sys directory
2. Running lxa with the command as the target

Example:
```bash
# Create test config
cat > /tmp/test_config.ini << EOF
[system]
rom_path = /path/to/lxa.rom
[drives]
SYS = /path/to/sys
EOF

# Run DIR command
./lxa -c /tmp/test_config.ini SYS:C/DIR
```

## What's Missing (Future Work)

### Environment Variables
- SetVar/GetVar/DeleteVar/FindVar APIs are stubbed but not implemented
- Would require storage mechanism (memory or filesystem-based)

### SetDate API
- Would set file modification times
- Maps to `utimes()` or `utimensat()` system calls

### Additional Commands
- **INFO** - System information (volumes, memory)
- **DATE** - Display/set system date

### Known Limitations
- Commands require Semaphore support (InitSemaphore/ObtainSemaphore) which is not yet implemented in exec.library
- This causes assertions when running the commands directly
- Commands can be tested in a limited environment without multitasking

## Code Changes

### Files Modified:
1. `src/lxa/lxa.c` - Added _dos_setprotection() and _dos_setcomment()
2. `src/lxa/lxa.c` - Added emucall handlers for EMU_CALL_DOS_SETPROTECTION and EMU_CALL_DOS_SETCOMMENT
3. `src/rom/lxa_dos.c` - Updated SetProtection() and SetComment() to use emucalls

### Files Created:
1. `sys/C/Makefile` - Build system for commands
2. `sys/C/dir.c` - DIR command
3. `sys/C/type.c` - TYPE command
4. `sys/C/makedir.c` - MAKEDIR command
5. `sys/C/delete.c` - DELETE command

## Architecture Notes

The implementation follows the WINE-like approach:
- DOS API calls are intercepted via emucalls
- Host-side (Linux) implementation handles actual file operations
- Amiga-side stubs marshal parameters and call emucalls
- This provides efficient filesystem access without emulating Amiga filesystems

## Next Steps (Phase 4 Continuation)

To complete Phase 4:
1. Implement SetVar/GetVar/DeleteVar/FindVar APIs
2. Implement SetDate API
3. Add INFO and DATE commands
4. Address Semaphore support in exec.library to enable full command execution

## References

- AmigaDOS documentation for protection bits
- AROS source for API reference
- Linux xattr documentation for extended attributes
