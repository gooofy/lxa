# lxa
**Linux Amiga Emulation Layer**

lxa runs AmigaOS executables natively on Linux, similar to how WINE runs Windows programs. It combines a 68000 CPU emulator with AmigaOS library implementations, bridging system calls to Linux I/O and filesystem operations.

The long-term goal is for lxa to become a **viable development platform for Amiga software on Linux** — enabling Amiga developers to write, test, and run productivity applications and IDEs directly on commodity hardware (Linux PC, Raspberry Pi) without needing real Amiga hardware. RTG (Retargetable Graphics) via Picasso96 is the primary display strategy, targeting full true-color screens at arbitrary resolutions.

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

- CMake 3.16+
- GCC or Clang
- **m68k-amigaos-gcc** cross-compiler ([bebbo/amiga-gcc](https://codeberg.org/bebbo/amiga-gcc))

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

[display]
rootless_mode = true

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
- `--assign <name=path>` - Replace or create an assign from the command line
- `--assign-add <name=path>` - Append a path to an existing multi-assign
- `-a <name=path>` - Legacy alias for `--assign`
- `-d` - Enable debug output
- `-v` - Verbose mode

`rootless_mode = true` keeps the Workbench screen hidden and exposes application windows as separate host windows, which is the recommended default for app launches.

Examples:

```bash
lxa --assign APPS=../lxa-apps --assign Cluster=/path/to/Cluster APPS:Cluster2/bin/Cluster2/Editor
lxa --assign LIBS=/path/to/base/libs --assign-add LIBS=/path/to/override/libs SYS:System/Shell
```

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

### Screenshot Review Helper

```bash
python tools/screenshot_review.py screenshot.png
python tools/screenshot_review.py --model anthropic/claude-sonnet-4.6 shot1.png shot2.png
```

The helper sends screenshots to OpenRouter using `OPENROUTER_API_KEY` and prints a vision-model review that is useful when comparing `lxa` UI output to a real Amiga.

## Implemented Features

### AmigaOS Libraries

- **exec.library**: Memory management, multitasking, signals, messages
- **dos.library**: File I/O, locks, directories, processes, pattern matching (including `(a|b)` alternation), `SetFileSize`, `SetFileDate`, `ExAll`, `MakeLink`, `ReadLink`, `InternalLoadSeg`, `NewLoadSeg`, `RunCommand`, `GetSegListInfo`, CON:/RAW: handler, assign-backed `DeviceProc()`/`GetDeviceProc()` coverage
- **utility.library**: Tag lists, hooks, utility functions including direct regression coverage for tag-list cloning/filtering/mapping, pack/unpack helpers, named-object namespaces, date conversion helpers, 32-bit math helpers, `HookEntry`-driven hook dispatch, and unique ID generation
- **graphics.library**: BitMap, RastPort, drawing primitives (WritePixel, Draw, RectFill, ScrollRaster, AreaFill, pixel arrays), palette/pen matching coverage via `ObtainPen()`/`ObtainBestPenA()` regressions
- **intuition.library**: Screen/window management, IDCMP input (SIZEVERIFY, MENUVERIFY, REQSET/REQCLEAR, DELTAMOVE), gadgets, menus, EasyRequest, BOOPSI (icclass/modelclass/gadgetclass, ICA_TARGET/ICA_MAP inter-object communication)
- **gadtools.library**: GadTools gadgets (buttons, strings, checkboxes, cycles, sliders with rendered live level values, MX), menu layout helpers, and IDCMP/refresh wrapper coverage
- **asl.library**: File, font, and screen mode requesters, including public `AllocAslRequestTags()`/`AslRequestTags()` varargs entry-point coverage
- **locale.library**: Locale management, `.catalog` loading and lookup, character classification/case conversion, `StrConvert`/`StrnCmp`, FormatDate, FormatString, ParseDate
- **keymap.library**: Default-keymap queries/updates plus `MapRawKey()` / `MapANSI()` translation coverage, including dead-key and string-sequence handling
- **diskfont.library**: Font loading from `FONTS:` bitmap `.font` files, including proportional and multi-plane color bitmap fonts, covered `NewFontContents()` / `DisposeFontContents()` helpers and diskfont control-tag queries, plus Phase 88 outline-font wrappers (`E*Engine`, `OpenOutlineFont()` / `CloseOutlineFont()`), font-file write helpers, and built-in charset metadata queries
- **mathieeesingbas.library**: IEEE single-precision basic math (12 functions)
- **mathieeedoubbas.library**: IEEE double-precision basic math (12 functions)
- **input.device**: Handler-chain management (`IND_ADDHANDLER` / `IND_REMHANDLER`), held-qualifier snapshots via `PeekQualifier()`, transient qualifier-bit preservation on injected events, repeat timing configuration, and direct event injection/dispatch coverage for `IND_WRITEEVENT` / `IND_ADDEVENT`
- **gameport.device**: Covered init/open/close/expunge lifetime semantics, controller-type and trigger query/set commands, `NSCMD_DEVICEQUERY`, pending `GPD_READEVENT` requests with `AbortIO()`, and hosted mouse-event delivery through the shared input stack
- **keyboard.device**: Covered `CMD_CLEAR`, `KBD_READEVENT`, `KBD_READMATRIX`, reset-handler registration/acknowledgement, per-open read queues, shared Intuition/raw-key event capture, and current key-matrix snapshots aligned with the hosted input stack
- **narrator.device**: Hosted speech-request validation with default V37 parameters, phoneme-stream write handling, generated mouth/word/syllable sync events for `CMD_READ`, and flush/reset coverage for translator-driven speech workflows
- **parallel.device**: Hosted shared/exclusive open semantics, `PDCMD_QUERY` / `PDCMD_SETPARAMS`, loopback-backed `CMD_READ` / `CMD_WRITE`, start/stop/flush/reset handling, pending-read abort coverage, and deferred-expunge lifetime tests
- **ramdrive.device**: Hosted in-memory `RAD:`-style block storage with `CMD_READ` / `CMD_WRITE` / `TD_FORMAT`, trackdisk-compatible geometry/status queries, ETD change-count validation, and `KillRAD()` / `KillRAD0()` coverage
- **scsi.device**: Hosted `HD_SCSICMD` support for core SCSI-direct commands (`INQUIRY`, `REQUEST SENSE`, `MODE SENSE(6)`, `READ CAPACITY`, `READ/WRITE` 6/10/12, and sync/cache no-ops), encoded unit validation, per-unit logical block storage, autosense/check-condition coverage, and deferred-expunge lifetime tests
- **console.device**: Terminal I/O with CSI escape sequences, keyboard input, `CONU_LIBRARY` access, and covered keymap command/query support
- **trackdisk.device**: Hosted `.adf`-backed sector I/O (`TD_READ` / `TD_WRITE` / `TD_FORMAT` / `TD_SEEK`, plus `ETD_*` variants and raw track `TD_RAWREAD` / `TD_RAWWRITE`) with geometry/status support, classic error handling, covered `TD_ADDCHANGEINT`/`TD_REMCHANGEINT` request lifetime semantics, and deferred-expunge plus abort coverage
- **audio.device**: Channel allocation, timed `CMD_WRITE` playback, `ADCMD_SETPREC` / `ADCMD_PERVOL` / `ADCMD_FINISH` / `ADCMD_WAITCYCLE`, and end-of-sample audio interrupt delivery

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

**Version 0.9.6** - SMART_REFRESH backing store validation (Phase 132): new `BackingStoreTest` sample (`samples/intuition/backingstoretest.c`) drives a dialog-over-window obscure/uncover cycle through three deterministic stages; new `backingstore_gtest.cpp` driver (4 tests) probes pixels in the formerly-obscured region and proves Phase 111's backing store correctly restores SMART_REFRESH content on uncover. Defensive `IS_SMARTREFRESH` skip in `DamageExposedAreas()` removed; apps now receive correct IDCMP_REFRESHWINDOW messages on uncover (matching real Amiga behaviour) without losing pixels. Full suite 67/67 pass.

**Version 0.9.5** - Intuition event log + menu introspection (Phase 131): circular 256-entry event log (`lxa_push_intui_event`, `lxa_drain_intui_events`) records `OpenWindow`/`CloseWindow`/`OpenScreen`/`CloseScreen` events from `lxa_dispatch.c`; `lxa_get_menu_strip()` traverses the emulated Intuition `MenuStrip` linked list to expose menu titles, item names, enabled/checked state, and geometry; `LxaTest::DrainEvents()`, `GetMenuStrip()`, `GetMenuNames()` helpers added to the test framework. Tests in simplemenu, maxonbasic, devpac, and api drivers.

**Version 0.9.4** - Text() interception (Phase 130): `lxa_set_text_hook()` / `lxa_clear_text_hook()` via `EMU_CALL_GFX_TEXT_HOOK`; ROM `_graphics_Text()` fires the hook on every render call; semantic text assertions in api, dopus, and kickpascal drivers.

**Version 0.9.3** - Screen-mode emulation (Phase 128 + Phase 129 partial): display pipeline optimization with dirty-region scanline tracking and SSE2 planar-to-chunky acceleration; ROM-side screen-mode virtualization complete — `g_known_display_ids[]` expanded to 36 ECS entries, `GetDisplayInfoData(DTAG_DIMS)` returns realistic raster ranges, `BestModeIDA()` honours `DIPFMustHave`/`MustNotHave`, `OpenScreenTagList()` accepts RTG-depth requests and maps unknown display IDs to the closest physical mode (Wine strategy). PPaint and FinalWriter validation deferred to Phase 139 (RTG app validation via Picasso96).

**Version 0.9.1** - Memory access fast paths (Phase 127): direct bswap-based 16/32-bit RAM and ROM reads/writes; `__builtin_expect` hints on hot paths.

**Version 0.9.0** - Profiling infrastructure (Phase 126): per-EMU_CALL counters + timing; `--profile` flag; `tools/profile_report.py`; baseline latency tests across six app drivers.

**Version 0.8.90** - `lxa.c` decomposition (Phase 125): 9,960-line monolith split into `lxa_custom.c`, `lxa_dispatch.c`, `lxa_dos_host.c`, `lxa_internal.h`, `lxa_memory.h`; `lxa.c` reduced to ~1,658 lines.

See [roadmap.md](roadmap.md) for detailed status and future plans.

## Known Limitations

- CPU: 68000 only (no 68020+ or FPU)
- Third-party libraries such as `commodities.library`, `rexxsyslib.library`, `datatypes.library`, and `dopus.library` are not shipped in ROM and must be installed on disk in `LIBS:`. Stub disk libraries are provided for `amigaguide.library`, `req.library`, `reqtools.library`, `powerpacker.library`, and `arp.library` (apps can OpenLibrary them but all functions return safe failure values).
- RTG: Picasso96 (`Picasso96API.library`) and `cybergraphics.library` are not yet implemented — this is the active development focus (Phases 137–139). ECS screen modes (LORES/HIRES, PAL/NTSC) are supported; RTG true-color modes are not yet available.
- GUI Apps: SDL2 window display works for ECS-depth apps; RTG true-color display pending Phase 137.
- Networking: Not implemented.
- Audio: Hosted `audio.device` playback is available, but Paula chip-level DMA timing/mixing remains approximate rather than cycle-exact.

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
ctest --test-dir build --output-on-failure --timeout 60 -j16
```

### Code Coverage

```bash
cmake -S . -B build -DCOVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
cmake --build build --target coverage
firefox build/coverage_report/index.html
```

### Contributing

Contributions welcome! Please:
1. Read [doc/developer-guide.md](doc/developer-guide.md)
2. Write tests for new features (target 100% coverage)
3. Follow coding standards in [AGENTS.md](AGENTS.md)
4. Update documentation



## Project Information

### What is lxa?

lxa (Linux Amiga) is an emulation layer that runs AmigaOS programs on Linux without requiring a full Amiga system. Think of it as **WINE for Amiga** — it translates AmigaOS system calls to Linux equivalents on-the-fly.

The strategic goal is to become a **development platform for Amiga software on Linux**: productivity apps (Wordsworth, Amiga Writer, PageStream), IDEs (Storm C, BlitzBasic 3), and GUI toolkit applications (MUI, ReAction) should all run and be testable on commodity hardware. Because lxa targets OS-compliant applications only, it can use RTG (Retargetable Graphics) via Picasso96 rather than emulating ECS/AGA chip registers — enabling full true-color displays at any resolution.

**How it works:**
- Emulates the Motorola 68000 CPU (via Musashi)
- Implements AmigaOS libraries (exec.library, dos.library, graphics.library, intuition.library, etc.)
- Maps Amiga filesystem calls directly to Linux (WINE-like approach)
- Provides preemptive multitasking and message passing
- RTG display via Picasso96 (`Picasso96API.library`) on SDL2 — no chip-register emulation needed

### Architecture Highlights

- **No hardware emulation** — Direct Linux I/O, no floppy/hard drive emulation
- **RTG-first display** — Picasso96 for true-color at arbitrary resolution; ECS planar modes supported for legacy apps
- **Case-insensitive VFS** — Amiga paths work on Linux filesystem
- **Native performance** — Most operations run at native speed
- **Fully preemptive** — Timer-driven task switching at 50Hz

See [doc/architecture.md](doc/architecture.md) for technical details.

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contribution Areas

We welcome contributions! Priority areas:
- **RTG / Picasso96**: `Picasso96API.library` and `cybergraphics.library` implementation (Phases 137–139)
- **GUI toolkits**: MUI and ReAction/ClassAct hosting support (Phases 141–142)
- **Productivity app coverage**: Wordsworth, Amiga Writer, PageStream, Storm C, BlitzBasic 3 drivers
- **AmigaOS library completeness**: filling gaps in existing exec/dos/graphics/intuition implementations
- **Test coverage**: new host-side GTest drivers for untested apps
- Command-line tool enhancements

Please read [doc/developer-guide.md](doc/developer-guide.md) before contributing.

## Acknowledgments

- **Musashi** - 68000 CPU emulator by Karl Stenerud
- **bebbo/amiga-gcc** - m68k AmigaOS cross-compiler
- **AROS** - Open-source AmigaOS implementation (reference)
- AmigaOS community for documentation and support
