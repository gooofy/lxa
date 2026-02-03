# lxa
**Linux Amiga Emulation Layer**

lxa runs AmigaOS executables natively on Linux, similar to how WINE runs Windows programs. It combines a 68000 CPU emulator with AmigaOS library implementations, bridging system calls to Linux I/O and filesystem operations.

## Features

- ✨ **Direct execution** of Amiga programs on Linux
- 🖥️ **Interactive Shell** with scripting support
- 📁 **Virtual filesystem** with case-insensitive path resolution
- 🔄 **Preemptive multitasking** with signals and message passing
- 🛠️ **Command-line tools** (DIR, TYPE, COPY, DELETE, etc.)
- ⚙️ **Flexible configuration** via INI files and environment variables

## Quick Start

### Installation

```bash
# Build and install
./build.sh
make -C build install

# Run the Amiga Shell
usr/bin/lxa
```

On first run, lxa automatically creates `~/.lxa/` with system files.

### Running Programs

```bash
# Run the interactive Shell
usr/bin/lxa

# Execute an Amiga program
usr/bin/lxa path/to/program

# Run with a script
usr/bin/lxa SYS:System/Shell myscript.txt
```

### Example Session

```bash
$ usr/bin/lxa
lxa Shell v1.0
1.SYS:> dir
S/          C/          Libs/       Devs/       T/

1.SYS:> type SYS:S/Startup-Sequence
; Startup script
echo "Welcome to lxa!"

1.SYS:> quit
```

## Building from Source

### Prerequisites

- CMake 3.10+
- GCC or Clang
- **m68k-amigaos-gcc** cross-compiler ([bebbo/amiga-gcc](https://github.com/bebbo/amiga-gcc))

### Build Steps

```bash
./build.sh
make -C build install
```

See [doc/building.md](doc/building.md) for detailed build instructions.

## Configuration

### First Run

lxa automatically creates `~/.lxa/` on first run with:
- Default configuration file
- System directory structure (S/, C/, Libs/, Devs/)
- Startup script

### Custom Configuration

Create or edit `~/.lxa/config.ini`:

```ini
[system]
rom_path = /usr/share/lxa/lxa.rom

[drives]
SYS = ~/.lxa/System
Work = ~/amiga_projects
DH1 = /mnt/data/amiga

[floppies]
DF0 = ~/.lxa/floppy0

[assigns]
KP2 = /path/to/kickpascal2
LIBS = /path/to/kickpascal2/libs
```

See [doc/configuration.md](doc/configuration.md) for details.

### Running Applications from App-DB

The App-DB (`../lxa-apps/`) contains Amiga applications with environment manifests:

```bash
# List available apps
./tests/run_apps.py --list

# Run an app
./tests/run_apps.py kickpascal2

# Run with custom timeout
./tests/run_apps.py kickpascal2 --timeout 30

# Run all apps
./tests/run_apps.py --all
```

Each app has an `app.json` manifest defining its assigns, required libraries, and test configuration.

## Usage

```bash
lxa [options] [program]
```

### Common Options

- `-c <config>` - Specify configuration file
- `-r <rom>` - Specify ROM path
- `-s <sysroot>` - Set system root directory
- `-d` - Enable debug output
- `-v` - Verbose mode

### Examples

```bash
# Interactive Shell
lxa

# Run a program
lxa myprogram

# Custom system root
lxa -s /path/to/amiga myprogram

# Debug mode
lxa -d -v myprogram
```

## Implemented Features

### AmigaOS Libraries

- **exec.library**: Memory management, multitasking, signals, messages
- **dos.library**: File I/O, locks, directories, processes, pattern matching, CON:/RAW: handler
- **utility.library**: Tag lists, hooks, utility functions
- **graphics.library**: BitMap, RastPort, drawing primitives (WritePixel, Draw, RectFill, ScrollRaster)
- **intuition.library**: Screen/window management, IDCMP input, gadgets
- **console.device**: Terminal I/O with CSI escape sequences, keyboard input

### System Commands

Built-in C: commands with full AmigaDOS template support:
- **DIR** - List directory contents
- **TYPE** - Display file contents
- **COPY** - Copy files and directories
- **DELETE** - Delete files and directories
- **MAKEDIR** - Create directories
- **RENAME** - Rename files
- **PROTECT** - Set file permissions
- **LIST** - Detailed directory listing
- **ASSIGN** - Manage drive assignments
- **And more...**

### Shell Features

- Interactive prompt with line editing
- Script execution (Execute API)
- Control flow: IF/ELSE/ENDIF, SKIP/LAB
- Built-in commands: ECHO, CD, PATH, PROMPT, ALIAS
- Argument parsing with ReadArgs

## Documentation

- **[Architecture](doc/architecture.md)** - Internal design and components
- **[Building](doc/building.md)** - Detailed build instructions
- **[Configuration](doc/configuration.md)** - Configuration guide
- **[Testing](doc/testing.md)** - Testing guide for developers
- **[Developer Guide](doc/developer-guide.md)** - Development workflow and standards
- **[AGENTS.md](AGENTS.md)** - AI agent development guidelines
- **[roadmap.md](roadmap.md)** - Project roadmap and status

## Current Status

**Version 0.5.6** - Phase 40 Complete (Application Rendering Fixes)

See [roadmap.md](roadmap.md) for detailed status and future plans.

## Known Limitations

- CPU: 68000 only (no 68020+ or FPU)
- Graphics: Basic screen/window support with drawing primitives
- Intuition: Screen and window management, basic IDCMP input handling
- Console: Full CSI escape sequence support, CON:/RAW: handlers
- GUI Apps: Run but graphical output goes to SDL window
- Networking: Not implemented
- Audio: Not implemented

## Troubleshooting

### "failed to open lxa.rom"

**Solution**: Specify ROM path:
```bash
lxa -r /path/to/lxa.rom myprogram
```

Or install to system location:
```bash
sudo cp build/target/rom/lxa.rom /usr/share/lxa/
```

### Commands not found in Shell

**Solution**: Set system root:
```bash
lxa -s . usr/share/lxa/System/Shell
```

### Case sensitivity issues

lxa handles case-insensitive Amiga paths automatically. If experiencing issues, enable debug mode:
```bash
lxa -d myprogram
```

See documentation for more troubleshooting tips.

## Development

### Running Tests

```bash
cd tests
make test
```

### Code Coverage

```bash
cmake -DCOVERAGE=ON ..
make coverage
firefox build/coverage/index.html
```

### Contributing

Contributions welcome! Please:
1. Read [doc/developer-guide.md](doc/developer-guide.md)
2. Write tests for new features (target 100% coverage)
3. Follow coding standards in [AGENTS.md](AGENTS.md)
4. Update documentation



## Project Information

### What is lxa?

lxa (Linux Amiga) is an emulation layer that runs AmigaOS programs on Linux without requiring a full Amiga system. Think of it as **WINE for Amiga** - it translates AmigaOS system calls to Linux equivalents on-the-fly.

**How it works:**
- Emulates the Motorola 68000 CPU (via Musashi)
- Implements AmigaOS libraries (exec.library, dos.library, etc.)
- Maps Amiga filesystem calls directly to Linux (WINE-like approach)
- Provides preemptive multitasking and message passing

### Architecture Highlights

- **No hardware emulation** - Direct Linux I/O, no floppy/hard drive emulation
- **Case-insensitive VFS** - Amiga paths work on Linux filesystem
- **Native performance** - Most operations run at native speed
- **Fully preemptive** - Timer-driven task switching at 50Hz

See [doc/architecture.md](doc/architecture.md) for technical details.

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contributing

We welcome contributions! Areas of interest:
- Additional AmigaOS library implementations
- Command-line tool enhancements
- Graphics/Intuition support
- 68020/68030/68040 CPU support
- FPU emulation
- Test coverage improvements

Please read [doc/developer-guide.md](doc/developer-guide.md) before contributing.

## Acknowledgments

- **Musashi** - 68000 CPU emulator by Karl Stenerud
- **bebbo/amiga-gcc** - m68k AmigaOS cross-compiler
- **AROS** - Open-source AmigaOS implementation (reference)
- AmigaOS community for documentation and support


