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
- `-d` - Enable debug output
- `-r <rom>` - Specify kickstart ROM path
  - Default: `../rom/lxa.rom`
- `-s <sysroot>` - Set AmigaOS system root directory
  - Default: `/home/guenter/media/emu/amiga/FS-UAE/hdd/system/`
  - Used for file path resolution when Amiga programs open files
- `-v` - Verbose mode
- `-t` - Enable CPU instruction tracing

### Examples

Run a hunk format Amiga executable:
```bash
lxa -v -s ./tests/dos/helloworld ./tests/dos/helloworld/helloworld
```

Run with debugging and breakpoint:
```bash
lxa -b _start -d ./myprogram
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

## Developer Information

### Integration Tests

lxa includes an integration test framework for validating AmigaOS library implementations.

#### Test Structure

```
tests/
├── Makefile              # Master test runner
├── Makefile.config        # Configuration (paths to lxa, ROM, toolchain)
├── test_runner.sh        # Test execution script
├── common/
│   └── test_assert.h  # Amiga-side assertion helpers
└── dos/
    └── helloworld/
        ├── main.c         # Test program
        ├── Makefile        # Test build file
        └── expected.out    # Expected output
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
- `src/rom/` - AmigaOS library implementations
  - `exec.c` - exec.library
  - `lxa_dos.c` - dos.library
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

## Limitations

- Only 68000 CPU mode currently supported
- No图形 (graphics) or Intuition library support yet
- Limited filesystem support (basic file I/O only)
- No process spawning or multitasking support

## License

MIT License

## Contributing

This is experimental code. Contributions welcome, especially:
- Additional library implementations
- CPU mode enhancements (68020+, FPU)
- Graphics/Intuition support
- Filesystem improvements
