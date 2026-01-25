# lxa
Linux Amiga emulation layer

An experiment in executing OS-friendly Amiga executables on Linux, similar to how WINE runs Windows executables on non-Windows systems.

## Overview

lxa provides a Linux-based runtime environment for AmigaOS programs by combining:
- **68k emulator**: Emulates the Motorola 68000 series CPU (using Musashi)
- **Amiga OS environment**: Implements core AmigaOS libraries (exec.library, dos.library, etc.)
- **Linux integration**: Bridges AmigaOS system calls to Linux filesystem and I/O

## Building

```bash
make all
```

This builds:
- `target/x86_64-linux/bin/lxa` - The main emulator executable
- `src/rom/lxa.rom` - The AmigaOS kickstart ROM image

## Usage

```bash
lxa [options] <loadfile>
```

### Options

- `-b <addr|sym>` - Add breakpoint at address or symbol
  - Examples: `-b _start`, `-b __acs_main`
- `-c <config>` - Specify configuration file
  - Default: `~/.lxa/config.ini`
  - See Configuration section below
- `-d` - Enable debug output
- `-r <rom>` - Specify kickstart ROM path
  - Default: `../rom/lxa.rom`
  - Can also be set in configuration file
- `-s <sysroot>` - Set AmigaOS system root directory (legacy mode)
  - Default: `/home/guenter/media/emu/amiga/FS-UAE/hdd/system/`
  - Maps to `SYS:` drive when VFS is not configured
- `-v` - Verbose mode
- `-t` - Enable CPU instruction tracing

### Examples

Run with configuration file:
```bash
lxa -c ~/.lxa/config.ini myprogram
```

Run a hunk format Amiga executable (legacy mode):
```bash
lxa -v -s ./tests/dos/helloworld ./tests/dos/helloworld/helloworld
```

Run with debugging and breakpoint:
```bash
lxa -b _start -d ./myprogram
```

## Configuration

lxa supports an optional configuration file for flexible drive mapping and system settings.

### Configuration File Location

- Default: `~/.lxa/config.ini`
- Specify with `-c <path>` option
- If not found, lxa falls back to command-line options

### Configuration File Format

```ini
# lxa configuration file

[system]
# ROM and memory settings
rom_path = /path/to/lxa.rom
ram_size = 10485760  # 10 MB

[drives]
# Map Amiga drives to Linux directories
SYS = ~/.lxa/drive_hd0
Work = ~/amiga_projects
DH1 = /mnt/data/amiga

[floppies]
# Map floppy drives to directories
DF0 = ~/.lxa/floppy0
DF1 = ~/downloads/amiga_disk
```

### Virtual File System (VFS)

lxa implements a WINE-like VFS layer that maps Amiga logical drives to Linux directories:

#### Features

- **Case-insensitive path resolution**: `SYS:s/startup` matches `SYS/S/Startup-Sequence`
- **Drive mapping**: `SYS:`, `Work:`, `DH0:` etc. map to Linux directories
- **Special devices**: `NIL:` automatically maps to `/dev/null`
- **Backward compatible**: Falls back to `-s` sysroot when VFS is not configured

#### Path Resolution Examples

| Amiga Path | Linux Path (with config above) |
|------------|--------------------------------|
| `SYS:S/Startup-Sequence` | `~/.lxa/drive_hd0/S/Startup-Sequence` |
| `sys:s/startup-sequence` | `~/.lxa/drive_hd0/S/Startup-Sequence` |
| `Work:Projects/README` | `~/amiga_projects/Projects/README` |
| `WORK:projects/readme` | `~/amiga_projects/Projects/README` |
| `NIL:` | `/dev/null` |

#### Setup Example

```bash
# Create directory structure
mkdir -p ~/.lxa/drive_hd0/{S,C,Libs,Devs}

# Create config file
cat > ~/.lxa/config.ini <<EOF
[drives]
SYS = ~/.lxa/drive_hd0
Work = ~/amiga_work
EOF

# Run lxa
lxa myprogram
```

## Architecture

lxa operates by:
1. Loading the AmigaOS kickstart ROM into memory at 0xFC0000
2. Loading the target hunk-format executable into RAM
3. Executing code through the 68k emulator
4. Intercepting AmigaOS library calls and forwarding them to Linux implementations
5. Managing memory, files, and other resources between both systems

### Memory Layout

- **RAM**: 0x000000 - 0x00FFFFF (10 MB)
- **ROM**: 0xFC0000 - 0xFFFFFF (256 KB)
- **Custom Chips**: 0xDFF000 - 0xDFFFFF

### VFS Architecture (Phase 2)

lxa uses a **WINE-like approach** for filesystem operations:

- **No hardware emulation**: No trackdisk.device or FFS/OFS filesystem emulation
- **Host-bridged I/O**: All DOS file operations (`Open`, `Read`, `Write`, etc.) are intercepted via `emucall` and handled directly by Linux system calls
- **Case-insensitive mapping**: Path components are resolved case-insensitively using `opendir`/`readdir`/`strcasecmp`
- **Efficient**: Direct directory mapping with minimal overhead

This approach provides excellent compatibility and performance while keeping the implementation simple.

## Developer Information

### Integration Tests

lxa includes an integration test framework for validating AmigaOS library implementations.

#### Test Structure

```
tests/
├── Makefile              # Master test runner
├── Makefile.config       # Configuration (paths to lxa, ROM, toolchain)
├── test_runner.sh        # Test execution script
├── common/
│   └── test_assert.h     # Amiga-side assertion helpers
├── dos/
│   └── helloworld/       # Basic DOS I/O test
│       ├── main.c
│       ├── Makefile
│       └── expected.out
└── exec/
    └── signal_pingpong/  # Signal/message passing test
        ├── main.c        # Two-task message exchange
        ├── Makefile
        ├── run_test.sh   # Custom test runner
        └── expected.out
```

#### Running Tests

```bash
cd tests
make test              # Build and run all tests
make clean             # Clean test artifacts
make help              # Show available targets
```

#### Adding New Tests

1. Create test directory under appropriate category (e.g., `tests/dos/file_io`)
2. Add `main.c` with test code using AmigaOS APIs
3. Create `Makefile` to compile with `m68k-amigaos-gcc`
4. Create `expected.out` with expected program output
5. Add test directory to `tests/Makefile` `run-tests` target

#### Test Framework Features

- **Sysroot isolation**: Each test uses its directory as sysroot via `-s` parameter
- **Output verification**: Compares program stdout against golden file
- **Exit code checking**: Validates program returns success (0)
- **Build automation**: Automatically compiles Amiga executables with correct toolchain

#### Example Test

```c
#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

int main(void)
{
    BPTR out = Output();
    Write(out, (CONST APTR) "Hello, World!\n", 14);
    return 0;
}
```

### Amiga Toolchain

Tests require the Amiga cross-compiler:
- **Prefix**: `/opt/amiga/` (configurable in `Makefile.config`)
- **Compiler**: `m68k-amigaos-gcc`
- **Runtime**: `nix20` (newlib C runtime)

### Code Organization

- `src/lxa/` - Main emulator and 68k CPU
  - `lxa.c` - Main emulator loop
  - `vfs.c` / `vfs.h` - Virtual filesystem with case-insensitive resolution
  - `config.c` / `config.h` - Configuration file parser
  - `m68k*.c` - Musashi 68k CPU emulator
- `src/rom/` - AmigaOS library implementations
  - `exec.c` - exec.library (memory, tasks, messages, signals)
  - `lxa_dos.c` - dos.library (file I/O, processes, program loading)
  - `lxa_utility.c` - utility.library
  - etc.
- `src/include/` - Shared headers and emucalls interface
- `tests/` - Integration test suite

## Emulator Calls (lxcall)

lxa defines custom emulator calls for AmigaOS libraries to communicate with the host:

```c
#define EMU_CALL_STOP        2   // Stop emulator with return code
#define EMU_CALL_TRACE       3   // Enable/disable tracing
#define EMU_CALL_EXCEPTION   5   // Handle 68k exceptions
#define EMU_CALL_LOADFILE    9   // Load executable filename
#define EMU_CALL_SYMBOL      10  // Register symbol
#define EMU_CALL_GETSYSTIME  11  // Get system time
#define EMU_CALL_DOS_OPEN    1000 // DOS library calls
#define EMU_CALL_DOS_READ    1001
#define EMU_CALL_DOS_WRITE   1002
...
```

## Implementation Status

### ✅ Phase 1: Exec Core & Tasking Infrastructure (COMPLETE)
- Signal-based multitasking (`Wait`, `Signal`, `SetSignal`)
- Message passing (`PutMsg`, `GetMsg`, `ReplyMsg`, `WaitPort`)
- Message ports (`CreateMsgPort`, `DeleteMsgPort`, `AddPort`, `RemPort`, `FindPort`)
- Process management (`CreateNewProc`, `Exit` with proper cleanup)
- Task numbering via `RootNode->rn_TaskArray`

### ✅ Phase 2: Configuration & VFS Layer (COMPLETE)
- Configuration file system (`~/.lxa/config.ini`)
- Virtual filesystem with drive mapping
- Case-insensitive path resolution (Amiga-style over Linux filesystem)
- Special device support (`NIL:`)
- Backward compatibility with legacy sysroot mode

### 🚧 Phase 3: AmigaDOS File Management (PLANNED)
- Lock/UnLock/DupLock/SameLock
- Examine/ExNext/ExAll
- Assigns and path management

### 🚧 Phase 4+: Future Work
- Device handlers and console improvements
- Graphics/Intuition library support
- 68020+ CPU modes with FPU

## Current Limitations

- Only 68000 CPU mode currently supported
- No graphics or Intuition library support yet
- File locking (Lock/Examine) not yet implemented
- No assign support beyond VFS drive mapping

## Development Roadmap

See `roadmap.md` for the complete development plan. Key milestones:

- **Phase 1** ✅ - Exec multitasking foundation
- **Phase 2** ✅ - Configuration and VFS layer  
- **Phase 3** 🚧 - DOS file management (locks, examine, assigns)
- **Phase 4** 📋 - Device handlers and console
- **Phase 5** 📋 - Finalization and AROS reference integration

## License

MIT License

## Contributing

This is experimental code. Contributions welcome, especially:
- Phase 3 implementation (DOS file management)
- Additional library implementations
- CPU mode enhancements (68020+, FPU)
- Graphics/Intuition support
- Test coverage improvements


