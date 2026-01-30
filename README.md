# lxa
Linux Amiga emulation layer

An experiment in executing OS-friendly Amiga executables on Linux, similar to how WINE runs Windows executables on non-Windows systems.

## Overview

lxa provides a Linux-based runtime environment for AmigaOS programs by combining:
- **68k emulator**: Emulates the Motorola 68000 series CPU (using Musashi)
- **Amiga OS environment**: Implements core AmigaOS libraries (exec.library, dos.library, etc.)
- **Linux integration**: Bridges AmigaOS system calls to Linux filesystem and I/O

## Building

### Quick Build (Recommended)

```bash
# Full build using convenience script
./build.sh

# Install to usr/ prefix
make -C build install
```

### Manual CMake Build

```bash
# Create build directory
mkdir build && cd build

# Configure the build
cmake ..

# Build everything
make -j$(nproc)

# Install (optional)
make install
```

This builds:
- `build/host/bin/lxa` - The main emulator executable
- `build/target/rom/lxa.rom` - The AmigaOS kickstart ROM image
- `build/target/sys/C/*` - AmigaDOS commands (DIR, TYPE, DELETE, MAKEDIR)
- `build/target/sys/System/Shell` - Amiga Shell (interactive command interpreter)

After `make install`:
- `usr/bin/lxa` - Installed emulator binary
- `usr/share/lxa/` - ROM, system files, and config template

## Quick Start

lxa works like **WINE** for Windows programs - it runs Amiga executables directly on Linux:

```bash
# Build and install first
./build.sh && make -C build install

# First run - creates ~/.lxa/ automatically (like WINE's ~/.wine)
usr/bin/lxa

# Run a simple Amiga program
usr/bin/lxa tests/dos/helloworld/helloworld

# Run the Amiga Shell (interactive mode)
usr/bin/lxa

# Use DIR command in shell
1.SYS:> dir
S/
C/
Libs/
...

# Run Shell with a script file
usr/bin/lxa SYS:System/Shell myscript.txt
```

### First Run Setup

On first execution, lxa automatically creates a default environment at `~/.lxa/`:

```bash
$ usr/bin/lxa
lxa: First run detected - creating default environment at /home/user/.lxa
lxa: Copying system files from /path/to/usr/share/lxa
lxa: Environment created successfully.
lxa Shell v1.0
...
1./home/user/.lxa/System:>
```

This creates:
- `~/.lxa/config.ini` - Configuration file
- `~/.lxa/System/` - SYS: drive with S/, C/, Libs/, Devs/, T/ directories
- `~/.lxa/System/S/Startup-Sequence` - Default startup script

#### Environment Variables

- `LXA_PREFIX` - Override the default prefix directory (default: `~/.lxa`)
  ```bash
  export LXA_PREFIX=/path/to/custom/lxa
  ./target/x86_64-linux/bin/lxa myprogram
  ```

- `LXA_DATA_DIR` - Specify the system data directory (default: auto-detected)
  ```bash
  export LXA_DATA_DIR=/usr/share/lxa
  ./target/x86_64-linux/bin/lxa myprogram
  ```

## Usage

```bash
lxa [options] <loadfile>
```

### Options

- `-b <addr|sym>` - Add breakpoint at address or symbol
  - Examples: `-b _start`, `-b __acs_main`
- `-c <config>` - Specify configuration file
  - Default: `~/.lxa/config.ini`
- `-d` - Enable debug output
- `-r <rom>` - Specify kickstart ROM path (auto-detected if not specified)
  - Searches: current directory, src/rom/, ~/.lxa/, /usr/share/lxa/
- `-s <sysroot>` - Set AmigaOS system root directory
  - Default: current directory
  - Maps to `SYS:` drive
- `-v` - Verbose mode
- `-t` - Enable CPU instruction tracing

### Common Use Cases

**Run the Amiga Shell interactively:**
```bash
./target/x86_64-linux/bin/lxa sys/System/Shell
```

The Shell provides:
- Interactive prompt: `1.SYS:> `
- Internal commands: CD, ECHO, PROMPT, PATH
- Control flow: IF/ELSE/ENDIF, SKIP/LAB
- Tools: ALIAS, WHICH, WHY, FAULT

**Run a program from the current directory:**
```bash
# The current directory becomes SYS: by default
./target/x86_64-linux/bin/lxa myprogram
```

**Run with debugging:**
```bash
./target/x86_64-linux/bin/lxa -b _start -d ./myprogram
```

**Specify custom ROM location:**
```bash
./target/x86_64-linux/bin/lxa -r /path/to/lxa.rom myprogram
```

**Use custom system root:**
```bash
./target/x86_64-linux/bin/lxa -s /path/to/amiga/files sys/System/Shell
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

### ✅ Phase 3: First-Run Experience & Filesystem API (COMPLETE)
- Automatic `~/.lxa` environment bootstrap on first run
- Lock/UnLock/DupLock/SameLock
- Examine/ExNext/Info
- ParentDir/CurrentDir/CreateDir
- DeleteFile/Rename/NameFromLock

### ✅ Phase 4: Basic Userland & Metadata (COMPLETE)
**Status**: All C: commands now use AmigaDOS template syntax

#### Completed:
- **Pattern Matching APIs** - Full wildcard support (`#?`, `?`, `#`)
  - `ParsePattern()` and `MatchPattern()` implemented
  - Supports wildcards: `#?` (any string), `?` (single char), `%` (escape)
  - Also accepts `*` as convenience equivalent to `#?`
- **ReadArgs() Implementation** - AmigaDOS template-based command parser
  - Template syntax: `/A` (required), `/M` (multiple), `/S` (switch), `/K` (keyword), `/N` (numeric)
  - Full template parsing for all C: commands
- **Semaphore Support** - C runtime compatibility
  - `InitSemaphore()`, `ObtainSemaphore()`, `ReleaseSemaphore()`, `AttemptSemaphore()`
- **SetProtection/SetComment APIs** - File metadata operations
  - Maps Amiga protection bits to Linux permissions
  - Stores comments via xattr or sidecar files
- **C: Commands with Amiga Syntax** (in `sys/C/`):
  - **DIR**: `DIR [dir|pattern] [OPT keywords] [ALL] [DIRS] [FILES] [INTER]`
    - Pattern matching for filtering files
    - ALL shows hidden files, DIRS/FILES filter by type
    - OPT=L for detailed listing
  - **TYPE**: `TYPE FROM/A/M [TO filename] [HEX] [NUMBER]`
    - Multiple file support with FROM/A/M
    - TO keyword for output redirection
    - HEX for hex dump, NUMBER for line numbering
  - **DELETE**: `DELETE FILE/M/A [ALL] [QUIET] [FORCE]`
    - Multiple files/patterns with FILE/M/A
    - ALL for recursive directory deletion
    - QUIET suppresses output, FORCE ignores errors
  - **MAKEDIR**: `MAKEDIR NAME/M/A`
    - Multiple directory creation with /M modifier

### 🚧 Phase 4b: Environment & Timestamps (Future)
- Environment variables (GetVar/SetVar/DeleteVar/FindVar)
- SetDate API for file timestamps
- PROTECT, FILENOTE, RENAME commands

### ✅ Phase 5: Interactive Shell & Scripting (COMPLETE)
- Interactive Shell with AmigaDOS I/O (no stdio dependency)
- Execute() API for script execution
- Control flow: IF/ELSE/ENDIF, SKIP/LAB
- Shell commands: ECHO (NOLINE), ASK, ALIAS, WHICH, WHY, FAULT
- Command-line argument passing: `lxa Shell script.txt`
- Piped input support: `echo "dir" | lxa Shell`

### ✅ Phase 6: Build System Migration (COMPLETE)
- CMake-based build system with cross-compilation support
- FHS-compliant installation structure (`bin/`, `share/lxa/`)
- `LXA_PREFIX` environment variable support for custom user environments
- Automatic ROM discovery in installation directories
- System template copying from `share/lxa/System` on first run

### 📋 Phase 7: Future Work
- Assignment API (AssignLock, AssignPath)
- System management tools
- Graphics/Intuition library support
- 68020+ CPU modes with FPU

## Current Limitations

- **CPU Support**: Only 68000 mode currently supported (no 68020+ or FPU)
- **Graphics**: No Intuition library support yet
- **Assigns**: No assign support beyond VFS drive mapping
- **ExAll()**: Not yet implemented (use ExNext for directory enumeration)
- **Environment Variables**: GetVar/SetVar not yet implemented
- **Child Process Timing**: External command output may appear slightly delayed due to cooperative multitasking limitations (being addressed in Phase 6.5)

## Development Roadmap

See `roadmap.md` for the complete development plan. Key milestones:

- **Phase 1** ✅ - Exec multitasking foundation
- **Phase 2** ✅ - Configuration and VFS layer
- **Phase 3** ✅ - First-run experience & filesystem API (locks, examine, directories)
- **Phase 4** ✅ - Basic userland & metadata (commands with AmigaDOS templates)
- **Phase 5** ✅ - Interactive shell & scripting
- **Phase 6** ✅ - Build system migration to CMake
- **Phase 7** 📋 - System management & assignments
- **Phase 8** 📋 - Advanced utilities & finalization

### Current Status

**Phase 6 Complete** - Modern CMake build system with:
- Cross-compilation support for m68k-amigaos
- FHS-compliant installation structure
- `LXA_PREFIX` environment variable for custom environments
- Automatic ROM discovery in system directories
- System template copying on first run

**Phase 6.5 In Progress** - Fixing cooperative multitasking for proper child process synchronization.

**Next: Phase 7** - System Management & Assignments
See `roadmap.md` for upcoming features and detailed implementation plans.

## Troubleshooting

### Script file not found when running `lxa Shell script.txt`
**Problem**: The script path must be relative to the current directory or use SYS: prefix.

**Solution**:
```bash
# Create a script in the current directory
cat > myscript.txt << 'EOF'
echo Hello from script
quit
EOF

# Run with relative path
./target/x86_64-linux/bin/lxa sys/System/Shell myscript.txt

# Or use SYS: prefix if script is in the project root
./target/x86_64-linux/bin/lxa -s . sys/System/Shell SYS:myscript.txt
```

### "LoadSeg() Open() for name=X failed"
**Problem**: The program file cannot be found. This usually means the path is incorrect or the file doesn't exist.

**Solutions**:
```bash
# Option 1: Run from project root with commands in sys/C/
cd /path/to/lxa
./target/x86_64-linux/bin/lxa -s . sys/C/dir

# Option 2: Ensure commands are built
make -C sys/C

# Option 3: Check the file exists
ls -la sys/C/dir
file sys/C/dir  # Should show "AmigaOS loadseg()ble executable/binary"

# Option 4: Specify full path
./target/x86_64-linux/bin/lxa /full/path/to/myprogram
```

### Commands not found in Shell
**Problem**: The Shell searches for commands in SYS:C/ and SYS:System/C/. If these directories don't exist or aren't on the search path, commands won't be found.

**Solution**:
```bash
# Ensure commands are built
make -C sys/C

# Use -s . to set current directory as SYS:
./target/x86_64-linux/bin/lxa -s . sys/System/Shell

# Or use explicit paths
1.SYS:> SYS:System/C/dir
```

### "failed to open lxa.rom"
**Problem**: ROM file not found in default locations.

**Solutions**:
```bash
# Option 1: Copy ROM to current directory
cp src/rom/lxa.rom .

# Option 2: Specify ROM path explicitly
./target/x86_64-linux/bin/lxa -r /path/to/lxa.rom myprogram

# Option 3: Install to system location
sudo cp src/rom/lxa.rom /usr/share/lxa/
```

## License

MIT License

## Contributing

This is experimental code. Contributions welcome, especially:
- Phase 4 implementation (basic userland commands)
- Additional library implementations
- CPU mode enhancements (68020+, FPU)
- Graphics/Intuition support
- Test coverage improvements


