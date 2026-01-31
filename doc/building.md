# Building lxa

This document provides detailed information about building lxa from source.

## Prerequisites

### Required Tools

- **CMake** 3.10 or later
- **GCC** or Clang for host compilation
- **m68k-amigaos-gcc** for cross-compilation (ROM and system tools)
- **Make**

### Installing the Amiga Cross-Compiler

The m68k-amigaos-gcc toolchain is required for building ROM and system components.

#### Linux (Debian/Ubuntu)

```bash
# Install from amiga-gcc repository
git clone https://github.com/bebbo/amiga-gcc
cd amiga-gcc
make update
make all -j$(nproc)
```

Default installation prefix: `/opt/amiga`

#### Verifying Installation

```bash
# Check compiler
/opt/amiga/bin/m68k-amigaos-gcc --version

# Check includes
ls /opt/amiga/m68k-amigaos/include/
# Should show: clib/ devices/ dos/ exec/ inline/ proto/
```

## Quick Build

The `build.sh` script handles the complete build process:

```bash
./build.sh
```

This script:
1. Creates `build/` directory
2. Runs CMake configuration
3. Builds all components (host emulator, ROM, system tools)
4. Outputs binaries to `build/`

## Manual Build Steps

For more control over the build process:

### 1. Configure with CMake

```bash
mkdir build && cd build
cmake ..
```

#### CMake Options

- `-DCMAKE_BUILD_TYPE=Debug` - Enable debug symbols and disable optimization
- `-DCMAKE_BUILD_TYPE=Release` - Enable optimizations (default)
- `-DCOVERAGE=ON` - Enable code coverage instrumentation
- `-DAMIGA_TOOLCHAIN_PREFIX=/path/to/amiga` - Override toolchain location

Example:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON ..
```

### 2. Build

```bash
make -j$(nproc)
```

Build targets:
- `make` or `make all` - Build everything
- `make lxa` - Build only the host emulator
- `make rom` - Build only the ROM
- `make sys` - Build only system commands

### 3. Install

```bash
make install
```

Default installation:
- **Binaries**: `usr/bin/lxa`
- **ROM**: `usr/share/lxa/lxa.rom`
- **System Files**: `usr/share/lxa/System/` (S/, C/, Libs/, etc.)
- **Config Template**: `usr/share/lxa/config.ini`

#### Custom Installation Prefix

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/lxa ..
make install
```

## Build Outputs

After a successful build, artifacts are located at:

```
build/
├── host/
│   └── bin/
│       └── lxa              # Host emulator executable
├── target/
│   ├── rom/
│   │   └── lxa.rom         # AmigaOS ROM image
│   └── sys/
│       ├── C/              # C: commands
│       │   ├── dir
│       │   ├── type
│       │   ├── delete
│       │   ├── makedir
│       │   ├── assign
│       │   ├── copy
│       │   ├── list
│       │   └── ...
│       └── System/
│           └── Shell       # Amiga Shell
```

## Building Individual Components

### Host Emulator Only

```bash
cd build/host
make -j$(nproc)
```

Output: `build/host/bin/lxa`

### ROM Only

```bash
cd build/target/rom
make -j$(nproc)
```

Output: `build/target/rom/lxa.rom`

### System Commands Only

```bash
cd build/target/sys
make -j$(nproc)
```

Output: `build/target/sys/C/*` and `build/target/sys/System/Shell`

### Specific Command

```bash
cd build/target/sys/C
make dir
```

## Cross-Compilation Details

### Toolchain File

The m68k cross-compilation is configured via `cmake/m68k-amigaos.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)

set(AMIGA_TOOLCHAIN_PREFIX "/opt/amiga" CACHE PATH "Amiga toolchain prefix")
set(CMAKE_C_COMPILER ${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-gcc)
set(CMAKE_CXX_COMPILER ${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-g++)

# Compiler flags for m68k
set(CMAKE_C_FLAGS_INIT "-m68000 -noixemul -Wall")
```

### Compilation Flags

**ROM Code** (src/rom/):
- `-m68000` - 68000 instruction set
- `-noixemul` - Use libnix C library (no ixemul.library dependency)
- `-fno-builtin` - Disable GCC builtins that might conflict with AmigaOS functions
- `-ffreestanding` - Freestanding environment (no hosted libc)

**System Commands** (sys/):
- `-m68000` - 68000 instruction set
- `-noixemul` - Use libnix C library
- `-Wall -Wextra` - Enable warnings

### Linker Script

The ROM uses a custom linker script (`src/rom/rom.ld`) to ensure proper memory layout:

```ld
SECTIONS
{
    .text 0xFC0000 : {
        *(.text.romstart)
        *(.text)
    }
    
    .rodata : {
        *(.rodata)
    }
    
    .data : {
        *(.data)
    }
}
```

This places:
- ROM code at 0xFC0000 (standard Amiga ROM location)
- Read-only data following code
- Data section (for initialized variables)

## Troubleshooting

### "m68k-amigaos-gcc: command not found"

**Problem**: Cross-compiler not in PATH or not installed.

**Solutions**:
```bash
# Option 1: Add to PATH
export PATH=/opt/amiga/bin:$PATH

# Option 2: Specify toolchain prefix
cmake -DAMIGA_TOOLCHAIN_PREFIX=/path/to/amiga ..

# Option 3: Install toolchain
cd /tmp
git clone https://github.com/bebbo/amiga-gcc
cd amiga-gcc
make update
make all -j$(nproc)
```

### "cannot find -lnix20"

**Problem**: libnix library not found in toolchain.

**Solution**: Ensure complete amiga-gcc installation:
```bash
cd amiga-gcc
make all -j$(nproc)  # This builds libnix too
```

### CMake Can't Find Headers

**Problem**: Amiga include files not found.

**Solution**: Check toolchain structure:
```bash
ls /opt/amiga/m68k-amigaos/include/
# Should show: clib/ devices/ dos/ exec/ inline/ proto/
```

If missing, reinstall amiga-gcc.

### Compilation Warnings

**Warning: implicit function declaration**

This usually indicates a missing header include. Add the appropriate header:
```c
#include <dos/dos.h>       // For DOS functions
#include <exec/types.h>    // For Amiga types
#include <clib/dos_protos.h>  // For function prototypes
```

**Warning: incompatible pointer type**

Check that you're using correct Amiga types:
- `LONG` not `long`
- `STRPTR` not `char*`
- `BPTR` not `void*`

## Clean Builds

### Clean Everything

```bash
rm -rf build/
./build.sh
```

### Clean Host Only

```bash
cd build/host
make clean
```

### Clean ROM Only

```bash
cd build/target/rom
make clean
```

### Clean System Commands

```bash
cd build/target/sys
make clean
```

## Development Builds

### Debug Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

Enables:
- Debug symbols (`-g`)
- No optimization (`-O0`)
- Additional debug output

### Coverage Build

```bash
cmake -DCOVERAGE=ON -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
make coverage
```

Generates code coverage report in `build/coverage/`.

### Verbose Build

```bash
make VERBOSE=1
```

Shows full compiler commands for debugging build issues.

## Incremental Builds

After making changes to source files:

```bash
cd build
make -j$(nproc)
```

CMake automatically detects changes and rebuilds only affected files.

### Forcing Rebuild

```bash
# Rebuild specific target
make clean && make lxa

# Touch file to force rebuild
touch ../src/lxa/lxa.c
make
```

## Building Tests

Tests are built separately:

```bash
cd tests
make        # Build all tests
make test   # Build and run all tests
```

See [Testing Guide](testing.md) for more information.
