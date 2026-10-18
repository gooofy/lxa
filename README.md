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

[audio]
enabled = true

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
- `--audio` / `--no-audio` - Play or mute host sound output

`rootless_mode = true` keeps the Workbench screen hidden and exposes application windows as separate host windows, which is the recommended default for app launches.

Sound: `[audio] enabled = false` (or `LXA_AUDIO=0`, or `--no-audio`) mutes lxa's host sound output; the emulated audio hardware keeps running, so programs behave the same. Precedence: command line > `config.ini` > `LXA_AUDIO` > default. The interactive `lxa` plays sound by default; liblxa (the test drivers, `pylxa` and the RDD twin/mass runners) is silent unless `LXA_AUDIO=1` or `[audio] enabled = true`.

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

**Version 0.12.54** - Phase 246: Picasso96 / CyberGraphX residue; milestone M4 (RTG) is complete. `p96RequestModeIDTagList()` and `CModeRequestTagList()` open the board's own screen mode requester (Board, Mode list, Ok/Cancel, Project menu) as Picasso96 2.0 does, with equal windows and pixels in three reference goldens. `p96BestModeIDTagList()` ranks modes like the reference for every captured query, and the undocumented colour conversions (cleared bitmaps, CLUT arrays, ReadPixelArray, ScalePixelArray, the R5G5B5 alpha bit) match. Only the frontmost RTG screen is shown (VP_HIDE and the private screen flag on the others), `GetColorMap()` fills pens 32-255 with the default colour table, and `SA_LikeWorkbench` implies `SA_FullPalette`.

**Version 0.12.11** - Phase 243: RTG apps. With RTG and screenmode prefs naming an RTG mode (the reference's `rtg` profile: UAE 800x600 8 bit), the Workbench opens in that mode with the View offsets, GfxBase display limits and DisplayClip that 3.1 gives it, and `SA_LikeWorkbench` screens follow it. Final Writer reaches its editor on the 800x600 Workbench, Deluxe Paint V lists the uaegfx modes in its Screen Format dialog, and Personal Paint and ProWrite run there. Each of them, and 19 gallery scenarios, has an `rtg` reference golden. The screen title bar is now a layer in front of backdrop windows (`ShowTitle()` moves it).

**Version 0.12.8** - Phases 241/242: RTG. With `--rtg` (or `LXA_RTG=1`, `config.rtg`) lxa offers the uaegfx display modes of the reference's `rtg` profile (320x200 to 1344x1080 in CLUT, 15/16/24/32 bit formats) through the display database, `OpenScreenTagList()` opens chunky screens of those modes, and the host display shows them through an RGBA path. `AllocBitMap()` deeper than 8 planes or with an RTG friend bitmap gives chunky bitmaps; graphics, layers (backing store, backfill, ClipBlit) and Intuition draw into them. `Picasso96API.library` and `cybergraphics.library` are lxa's own disk libraries (bitmaps, locks, pixel arrays, mode info, BestModeID, RequestModeID); their behaviour is checked against the reference by the `tests/probes/rtg` probes. The rest (exact BestModeID ranking, requester windows, undocumented colour conversions) is Phase 246.

**Version 0.11.97** - Phase 240: lxa is the A4000/040 with AGA that the AmigaOS 3.1 reference runs. The CPU is a 68040 with FPU (AttnFlags, CACR/VBR, CINV/CPUSH, the FPU starts in its null state) and lxa's own `68040.library` is open from boot. GfxBase, MonitorInfo, the View and the display database are AGA (24-bit palette); bitmaps and the Workbench are interleaved; 256-colour screens share pens as on 3.1. Memory is laid out as on the A4000: 2 MB chip RAM, 8 MB fast RAM at $07800000 and 16 MB Zorro III RAM at $40000000, each with its MemHeader at the start of the region. Each change is checked by a probe captured on the reference (`exec/machine`, `exec/fpu`, `exec/memlist`, `graphics/modedb`, `graphics/mntrdb`, `graphics/bmattr`, `graphics/rgb32`).

**Version 0.11.78** - Sound output is configurable: `--audio`/`--no-audio`, `LXA_AUDIO=0/1` or `config.ini` `[audio] enabled`. Automated tests and the RDD runners (everything built on liblxa) now run silently by default; the emulated audio hardware is unaffected.

**Version 0.11.90** - icon.library's built-in default icons (disk, drawer, tool, project, trashcan) are lxa's own artwork instead of copies of the AmigaOS 3.1 images: a floppy, a file folder, a window with a cog, a page and a wire waste basket, drawn by `tools/gen_icon_defaults.py` in the 3.1 sizes and Workbench pens. The `icon/diskobj` probe now compares only the structure of the default icons, not their pixels (re-captured on the reference).

**Version 0.11.89** - Fixes two suite flakes. `dos_gtest` and the `ref_expected_outputs` shards ran the same DOS test programs in the same build directory at the same time, deleting each other's files. `dos_gtest` now runs a private copy in its RAM: directory, FileIOAdvanced and SoftLink use `PROGDIR:`, and DeleteRename no longer shares `testfile.txt` with FHUtils (output re-captured on the reference). RealtimeClockIsOptIn, the one test on the wall clock, accepts timers that fire late under host load but still fails on early ones.

**Version 0.11.88** - Phase 237e closed, and with it M3. Of the 9943 Fred Fish programs, those that hang or lack the window they open on AmigaOS 3.1 went from 32 to 14 (`doc/sweeps/2026-10-11-fish-residue3.md`); the rest is owned by later phases. Two causes of suite flakes are gone. An emulator run no longer depends on the run before it in the same process: CPU registers, FPU state, host file descriptors and host tables used to carry over. A stray write no longer starts the interactive debugger on the shared stdin. New, each checked by a probe on the reference: RastPort blits take their 3.1 time. A missing volume brings up the "Please insert volume" requester. Lock(NULL) and Lock(":") resolve as on 3.1, and ExNext() stays at the end of a directory. Commands get pr_WindowPtr 0. Offset 4 of a FileHandle is the interactive flag. DF0:/DF1: have handlers that answer with "no disk", and DoIO() hands D0 to the device. BltClear(), WA_PubScreenName NULL, serial.device and trackdisk status commands also behave as on 3.1.

**Version 0.11.77** - Phase 237d closed: the Fred Fish residue II. Of the 9943 Fish programs, those that hang or lack the window they open on AmigaOS 3.1 went from 68 to 32, and no lxa-only crash happens to more than one program (`doc/sweeps/2026-10-10-fish-residue2.md`). New since 0.11.75, each checked by a probe on the reference: a task now sleeps during the time a file operation takes, so other tasks run meanwhile. System() sleeps until its child ends, Execute() runs a single shell, and Run starts a background CLI that loads the command itself. ReadArgs() keeps the defaults a program put in its array. AllocRemember() accepts a NULL key. There are two floppy drives with their trackdisk tasks and DF0:/DF1: nodes. A RawDoFmt() PutChProc can no longer upset RawDoFmt's registers. SetTaskPri() stores the low byte of the priority, and commands see the shell variables process, RC and Result2. Mass runs now run each batch of 25 programs as the reference does: a fresh system directory per batch, each program as LXAREF:p/<id>/<name>.

**Version 0.11.75** - Phase 237d (Fred Fish residue II, still open): of the 9943 Fish programs, those that hang or lack the window they open on AmigaOS 3.1 went from 68 to 52, and no lxa-only crash happens to more than one program (`doc/sweeps/2026-10-10-fish-residue2.md`). New, each checked by a probe on the reference: Delay() ends at the end of the tick in which it runs out, file operations take their 3.1 time, files and CON:/RAW: windows have handler ports that answer packets, Info() names the volume node, AllocMem() leaves its registers as 3.1 does, asynchronous System() runs through the shell, and potgo.resource exists. Mass runs now give programs the reference's real 12 s run time, and the Cluster2 golden compares the blinking cursor exactly.

**Version 0.11.64** - Phase 237c closed: the Fred Fish residue. Of 9943 Fish programs, those that hang or lack the window they open on AmigaOS 3.1 went from 194 to 68 and the lxa-only crashes from 13 to 6 (`doc/sweeps/2026-10-10-fish-residue.md`). Every fix is checked by a probe captured on the reference: overlaid executables and the rarer hunk types, dos results in D1, a real DosList with FileLocks, disk.resource, AllocTrap, Paula audio interrupt requests, BltClear patterns, the registers the text and drawing functions leave behind, disk libraries whose init returns no base, and OpenWindow() on the Workbench for WBENCHSCREEN windows. commodities.library is now lxa's own complete implementation.

**Version 0.11.62** - Phase 245 closed: no AROS-derived code remains in lxa (AROS is a reference only). Besides the rewritten BCPL support and gadget classes, every former "based on AROS" site was audited (`doc/aros-audit.md`): area fills, AddFont/RemFont, FontExtent, DrawBorder, DoubleClick, MakeFunctions, semaphore hand-over, Exception and WaitIO are rewritten as lxa's own code with unchanged behaviour.

**Version 0.11.61** - colorwheel.gadget, gradientslider.gadget and tapedeck.gadget rewritten as lxa's own code (Phase 245; `doc/gadget-classes.md`): the 3.1 probes still match, HSB/RGB conversion is bit-identical to before on ~250 million inputs, and no AROS-licensed file remains in lxa.

**Version 0.11.60** - BCPL support rewritten as lxa's own code (Phase 245, licensing: AROS code is a reference only). The BCPL global vector, call/return, runtime routines, rdargs and CLI start-up of 1.x BCPL programs are lxa's MIT code (`src/rom/bcpl/`, `doc/bcpl.md`); all probes and BCPL programs behave as before, and DeliTracker's 1.3 `EndCLI` now exits as on AmigaOS 3.1.

**Version 0.11.45** - Corpus residue (Phase 244 closed): 25 of 36 corpus apps are now gold, 2 platinum, 9 silver (was 15 gold, 19 silver; `doc/sweeps/2026-10-10-sweep.md`) - the rest waits for BOOPSI classes, console handler ports and RTG. Library functions that return a BOOL leave a clean 32-bit D0 (42 entry points in dos, intuition, gadtools, icon, locale, workbench, asl, commodities, rexxsyslib); AmiBlitz3's template menu no longer lists StormWizard_App.wizard (probe dos/boolret).

**Version 0.11.44** - Corpus residue (Phase 244): GetScreenData() ignores the screen argument for WBENCHSCREEN and OpenScreen() refuses negative sizes other than STDSCREENWIDTH/HEIGHT as on AmigaOS 3.1 - GFA-BASIC opens its editor screen instead of 146 Workbench windows (probe intuition/screenzero).

**Version 0.11.43** - Corpus residue (Phase 244): Intuition's fillrectclass image class exists, so reqtools.library requesters open (FontView's shareware requester, tree-equal to AmigaOS 3.1; probe intuition/fillrect).

**Version 0.11.42** - Corpus residue (Phase 244): IntuiTextLength() and PrintIText() use the IntuiText's own font (FinalWriter's dialog now has 3.1's geometry), system requesters opened before the Workbench measure their text and visit the Workbench screen (ACE's AIDE alert). New probes for requester fonts, proportional text metrics and missing fonts.

**Version 0.11.41** - Corpus residue (Phase 244): ADPro, MaxonBASIC, ProWrite and ReSource are tree-equal to AmigaOS 3.1 - GadTools labels with GT_Underscore always carry the underline text, windows opened by tags take their screen's pens, and "input.device" and "ramlib" exist as system tasks (each verified with a probe); MaxonBASIC and ProWrite now run with the Workbench 3.1 fonts as on the reference.

**Version 0.11.40** - Corpus residue (Phase 244): OpenWindow widens the borders around an application's border gadgets and marks them GACT_BORDERSNIFF as AmigaOS 3.1 does (SIGMAth, BECKERtext II), and GadTools keeps menus on the screen - items move left at the right edge, long menus continue in a second column (ADPro, MaxonBASIC, Scout). Both verified with new reference probes.

**Version 0.11.34** - Full Fred Fish mass run (Phase 237b closed): all 9943 Fish programs ran on lxa and, for the first time completely, on AmigaOS 3.1 (`doc/sweeps/2026-10-10-fish-massrun.md`). Only 13 programs still crash on lxa alone (was 299). Exception vectors now live in fast RAM as on the 68040 reference, misc.resource exists, foreign library and device code may clobber registers, `GetCC()`, the math libraries' condition codes and `BltBitMap()`'s cost match 3.1, and a stray ILLEGAL instruction is a program crash instead of a silent stop.

**Version 0.11.32** - Corpus apps (Phase 238 closed): all 36 corpus apps now open their windows as on AmigaOS 3.1 - none is rated garbage or untested any more (2 platinum, 15 gold, 19 silver; `doc/sweeps/2026-10-09-sweep.md`). AmiBlitz3 starts (exec's Disable/Enable/Forbid/Permit and semaphore calls keep all registers, as documented), asl.library's screen-mode requester is identical to 3.1's, files are named by their volumes ("System:C", "Ram Disk:T") and the test harnesses get a Workbench laid out like the reference's.

**Version 0.11.31** - Fred Fish compatibility (Phase 237 closed): of the 299 Fish programs that crashed only on lxa, 9 still do (`doc/sweeps/2026-10-09-fish-crashlist.md`). The FPU is reported and complete, low memory and the vector table look like AmigaOS 3.1's, new processes, custom gadgets, input handlers, `InitResident()`, `SuperState()`, `DisplayAlert()`, `PRT:`, the console unit's font and Intuition's View now behave as on the reference, each verified with a probe.

**Version 0.11.30** - Corpus apps (Phase 238): Scout (MUI), Oberon, ADPro, Asm-One, AQB, BTII and SIGMAth reach their windows as on AmigaOS 3.1; memory is laid out as on the reference (2 MB chip plus fast RAM), exec memory pools and memory handlers are complete, and the colorwheel, gradientslider and tapedeck gadgets are ported from AROS.

**Version 0.11.29** - Legacy app items validated on AmigaOS 3.1 (Phase 234): BlitzBasic 2's editor, Cluster2's EXIT button, Directory Opus copying and its palette, and SysInfo's startup now end in reference goldens; blitter shifts and masks fixed.

**Version 0.11.28** - Milestone M2 complete (Phase 222 closed): GELs, copper lists, PaletteExtra, VideoControl, CON: window options, InitResident and library versions now match AmigaOS 3.1; about 60 reference probes and 40 goldens guard the system libraries.

**Version 0.11.27** - BCPL programs run (Phase 239, AROS m68k BCPL support): 1.x C: commands behave as on AmigaOS 3.1. Prefs fidelity (Phase 236): `C:IPrefs` applies `ENV:Sys/*.prefs` (screen mode, fonts, palette, input, locale, overscan, pointer) and lxa's defaults equal 3.1's.

**Version 0.11.26** - Fred Fish compatibility (Phase 237): exec returns sign-extended results and keeps a1/d1 like AmigaOS 3.1 across device I/O, `ParentDir()` stops at assign roots; the mass-run harness now recognises crashes on the reference (Software Failure requester). 241 of the 299 programs that crashed only on lxa now behave as on 3.1 (`doc/sweeps/2026-10-04-fish-crashlist.md`).

**Version 0.11.25** - dos.library conformance (Phase 222a): ReadArgs matches AmigaOS 3.1 over ~180 probe cases, commands read their argument line from `Input()`'s buffer, buffered I/O reads and writes in 3.1's block sizes, CacheControl is complete, and libraries report 3.1's revisions.

**Version 0.11.24** - ROM font fidelity (Phase 225): topaz 8 and a new topaz 9 are generated from a dump of AmigaOS 3.1's fonts and match it glyph for glyph, soft styles included; 15 rendering goldens are now identical to the reference. Fred Fish compatibility (Phase 237): dos.library works with any a6, `OpenLibrary()` compares versions like 3.1, bootstrapped programs get the 3.1 shell's stack and `pr_ReturnAddr`.

**Version 0.11.23** - `C:NewShell`/`C:NewCLI` (WINDOW, FROM), CON: windows are interactive, CON: specifications with an empty title open; the twin runner's lxa side starts programs with non-interactive stdin/stdout like the reference's `RUN >file` (`LXA_STDIO_FILE`): vim 5.3 re-launches itself in its own console window as on AmigaOS 3.1.

**Version 0.11.22** - API conformance (Phase 222d/e/f): Intuition windows, screens, requesters and string gadgets, every GadTools gadget kind and menu layout, locale, keymap (the complete 3.1 usa map), iffparse, icon, diskfont and clipboard.device match AmigaOS 3.1 in 22 new reference probes.

**Version 0.11.21** - Commands start with the AmigaOS 3.1 register convention (D2 = stack size, D3/D4 = arguments, A0 = the caller's buffer, requested stack size at 4(sp)): Lattice 3.03-compiled programs no longer take a false stack overflow - 99 of 299 Fred Fish programs that crashed only on lxa now run.

**Version 0.11.20** - Legacy app tests validated on AmigaOS 3.1 (Phase 224): reference goldens for DPaint, Devpac (incl. typing and Settings), Directory Opus and Typeface (incl. Preview) replace five pixel-count heuristics.

**Version 0.11.19** - console.device no longer takes over the window's IDCMP: as on AmigaOS 3.1 it receives the input events the window's own IDCMP does not request (a window with `IDCMP_RAWKEY` gets the keys, its console none), follows window resizes by itself, and reports the window address in size-window raw events. `raw_events_unit` and the new `idcmp_console` test are validated on the reference; the DPaint startup golden now waits for the 4 s ownership splash.

**Version 0.11.18** - Graphics and layers conformance (Phase 222b/c): lines, areas, flood fill, blits, Text()/TextExtent/TextFit and soft styles, layer ClipRects, damage, backfill hooks and refresh modes match AmigaOS 3.1 in six reference probes. `pr_SegList` is a segment array as on AmigaOS (detaching programs such as Directory Opus find their code at `[3]`).

**Version 0.11.17** - App sweep (Phase 231): every corpus app is launched on lxa and AmigaOS 3.1 with each menu opened; divergences are ranked and clustered, and every cluster seen in two or more apps is owned by a roadmap phase (`doc/sweeps/2026-10-03-sweep.md`).

**Version 0.11.16** - Fred Fish mass run (Phase 232): all 9932 executables from the Fred Fish collection run under lxa without crashing or stalling the emulator itself; crashes inside emulated programs behave like AmigaOS (held task / Guru) and are classified against a real AmigaOS 3.1 run (`doc/sweeps/2026-10-03-fish-massrun.md`).

**Version 0.11.15** - Rendering matches AmigaOS 3.1 (Phase 223): Workbench screen and window chrome, system gadgets, menus, requesters, BOOPSI images and GadTools gadgets are checked pixel-by-pixel against the reference in 15 gallery goldens - the remaining differences are the ROM font's glyphs (Phase 225). API conformance probes for utility/exec/dos (Phase 222a) and fixes for Fred Fish programs (`Open("*")`, CLI stack-size convention).

**Version 0.11.14** - Shell and C: commands match Workbench 3.1 (Phase 221): 11 parity scripts produce identical output on lxa and AmigaOS 3.1; 13 missing commands added (Which, SetDate, Lock, Relabel, IconX, ...). CPU faults now behave like AmigaOS (held tasks, Guru for faulting interrupt code), and several more emulator stalls found by the Fred Fish mass run are fixed.

**Version 0.11.13** - Phase 220 complete: all 236 lxa test programs were run on a real AmigaOS 3.1 system; 197 now produce identical output on both, the rest are justified lxa-only or interactive tests. Hundreds of compatibility fixes across dos, exec, graphics, intuition, gadtools, layers, console and the devices came out of it. Several emulator crashes and stalls found by the Fred Fish mass run are fixed.

**Version 0.11.12** - Reference-validated test programs (Phase 220, in progress): the dos, intuition, gadtools, icon, iffparse and datatypes test programs now produce the same output as on AmigaOS 3.1, with many compatibility fixes in those libraries. CPU exceptions follow the AmigaOS trap convention (a crashed task is held), several emulator crashes found by the Fred Fish mass run are fixed, and `LXA_OVERRIDE` can load a user's real Workbench 3.1 disk library for debugging (Phase 235).

**Version 0.11.11** - App corpus (Phase 230): 36 applications are catalogued in `apps/` with compatibility ratings derived from twin runs against AmigaOS 3.1 (`apps/compat.yaml`, `python3 -m rdd dashboard`). liblxa now reports CPU exceptions (`lxa_get_exception_log`).

**Version 0.11.10** - Relay trace (Phase 233): `LXA_TRACE` logs library calls, and `python3 -m rdd tracediff` finds the first call where lxa and AmigaOS 3.1 differ. dos.library `Close()`/`Seek()` return values now match AmigaOS; the first API probe (utility.library) and reference-captured outputs for 54 test programs are in the suite.

**Version 0.11.9** - RDD loop (Phase 216): `python3 -m rdd loop` twin-runs all scenarios, replays the goldens, clusters divergences by root cause and writes a sweep report (`doc/sweeps/`); skills `rdd-reference` and `compat-sweep`. Milestone M1 (reference oracle) is complete.

**Version 0.11.8** - Reference goldens (Phase 215): `tests/golden/` holds AmigaOS 3.1 snapshot bundles for 6 scenarios; `ctest -L golden` replays them on lxa and fails on any new divergence from the reference.

**Version 0.11.7** - Comparator and report (Phase 214): `python3 -m rdd report` diffs lxa against the reference (Intuition tree, pen-index pixels, palette, text, output) and renders side-by-side composites; reviews are recorded as `findings.yaml` (first review: `doc/findings/`).

**Version 0.11.6** - Twin runner (Phase 213): one YAML scenario (`tests/scenarios/`) runs unchanged on lxa and on real AmigaOS 3.1 (`cd tools && python3 -m rdd run ../tests/scenarios/*.yaml`), producing comparable snapshot bundles from both. The first runs already exposed divergences in Directory Opus and Deluxe Paint V startup.

**Version 0.11.5** - Snapshot bundles (Phase 212): lxa and the real-AmigaOS reference now emit identical artefacts (Intuition tree JSON, pen-index snapshots, Text() logs; format in `doc/rdd-snapshot.md`). `tools/rdd/pylxa.py` drives lxa from Python through the new `liblxa.so`.

**Version 0.11.4** - Reference guest agent (Phase 211): `lxaprobe` runs on the real AmigaOS 3.1 reference machine and, over the serial line, launches programs, waits for windows, dumps the Intuition tree as JSON, takes pen-index snapshots, injects input, hooks `Text()`, traces library calls and lists display modes. The `rtg` reference profile runs Workbench on an 800×600 uaegfx mode.

**Version 0.11.3** - Menu/input scripting (Phase 204): tests select menu entries by path (`lxa_select_menu_path(0, "Project/Save As...")`) using the live MenuStrip geometry, split drags into begin/step/end to inspect state mid-drag, and read the current input qualifier. Completes milestone M0.

**Version 0.11.2** - Reference system builder (Phase 210): `tools/refsys/` builds a fresh AmigaOS 3.1 hard-disk system from the user's Workbench ADFs and boots headless FS-UAE reference instances (A4000/040, warp mode) in about 1.6 s, as the oracle for reference-driven development.

**Version 0.11.1** - Stub telemetry (Phase 203): every stub, partial implementation and empty library vector reports through `LXA_UNIMPLEMENTED`; `lxa.log` summarises which ones a run hit, `lxa --strict-unimplemented` stops at the first stub, and `doc/stub-inventory.md` is the generated work list for the remaining system-library gaps.

**Version 0.11.0** - Deterministic virtual time and measurable coverage (Phases 201–202). Test drivers now run on a deterministic virtual clock (25 MHz virtual CPU, cycle-derived VBlank/timer/DateStamp, idle time skipped): the same scenario produces byte-identical results, the full suite runs in ~22 s (was ~110 s) and passes 20/20 runs under load. `lxa --deterministic` gives the same clock on the command line. `make coverage` (with `-DLXA_COVERAGE=ON`) measures ROM and host line coverage and writes the per-LVO table to `doc/coverage/lvo-coverage.md`.

**Version 0.10.15** - Roadmap & agent-docs reset (Phase 200): new reference-driven roadmap (Phases 200+), AGENTS.md updated for Claude Code with the RDD and clean-room rules, `apps/README.md` documents the manifest requirement.

**Version 0.10.14** - DOpus text-hook test fixture fix (Phase 159c): re-enabled `TextHookCapturesDefaultPageButtonLabels` (formerly `DISABLED_TextHookCapturesKnownDOpusLabels`). Originally scoped as a "BltBitMap glyph-blit hook + glyph atlas detector" phase under the assumption that DOpus button-bank labels rendered via a custom blit path that bypassed `_graphics_Text()`. m68k-amigaos-objdump disassembly of `bin/DOPUS/libs/dopus.library` (per AGENTS.md §6.20) disproved the hypothesis — labels render via standard `graphics.library/Text()` (LVO -60) at file offset `0x4b6e` inside per-button paint helper `0x4a3e`, called from bank-window dispatcher `0x4902`. Root cause was two compounding test-fixture defects: (a) `DOpusTextHookTest` extended `LxaTest` instead of `LxaUITest`, denying it `WaitForEventLoop()`; (b) the bare 600-iter VBlank loop didn't give dopus.library's bank-paint dispatcher time to run. Switched the fixture to `LxaUITest` and adopted the proven `WaitForWindowDrawn + WaitForEventLoop(150) + RunCyclesWithVBlank(600) + flush+settle` sequence used by the working startup smoke test. Hook now captures `DOPUS.1`, `B/R/S/A`, `?/E/F/C/I/Q`, `All`, `Copy`, `Makedir`, `Hunt`, `Run` — every visible label on the default page. `Move`/`Rename` live on additional button pages reached by clicking page-navigation gadgets and are intentionally out of scope. End-of-SetUp screenshot capture added at `/tmp/dopus_text_hook_test.png` for visual triage of future regressions. No new ROM code, no new host API.

**Version 0.10.13** - DirectoryOpus deeper workflows (Phase 159b): four new `dopus_gtest.cpp` interaction tests landed — `ZPathEntryAreaRespondsToClick` (lister-pane click survives without window loss), `ZButtonBankClickPreservesUI` (button-bank click does not crash; window count non-decreasing), `ZProjectMenuFirstItemActivates` (RMB-drag through Project menu Item[0] using `lxa_get_menu_info` for coordinates, ESC-dismiss any popup), and `ZWindowTitleDocumentsDOpusRTAbsence` (pins the "DOPUS.1" title and documents the DOpusRT/Phase 167 dependency for the version+memory string). Phase 159b item 3 (BltBitMap glyph hook to re-enable `DISABLED_TextHookCapturesKnownDOpusLabels`) promoted to its own **Phase 159c** because the implementation requires a glyph atlas detector + glyph→char mapping infrastructure that is multi-day work in its own right. 18/18 DOpus tests pass; full suite 77/77.

**Version 0.10.12** - DirectoryOpus structural characterization (Phase 159).

**Version 0.10.9** - `SetWindowTitles()` title-bar repaint (Phase 158): `_intuition_SetWindowTitles()` now redraws the window frame immediately after changing `window->Title`, so runtime title changes are visible without moving or refreshing the window. New `SetWindowTitles` sample, `setwindowtitles_gtest.cpp`, and DPaint Screen Format title-bar regression coverage.

**Version 0.10.8** - Button / menu accelerator underline (Phase 157): `_graphics_Text()` renders `FSF_UNDERLINED` text with a 1-pixel underline, and GadTools labels now translate accelerator markers into underlined button/menu text. New `text_underline_gtest.cpp` coverage.

**Version 0.10.4** - PROPGADGET recessed track-frame rendering (Phase 152): `_render_gadget()` PROPGADGET branch now draws a 3D recessed frame (shadow pen on top/left, shine pen on bottom/right) on the outer perimeter of every prop gadget that does not opt out via `PROPBORDERLESS`. Frame applies independently of `AUTOKNOB` so custom-knob prop gadgets also receive standard chrome. New `samples/intuition/propgadget.c` and `tests/drivers/propgadget_chrome_gtest.cpp` (7 tests) plus full-suite regression check (74/74 pass).

**Version 0.10.3** - DPaint Screen Format custom-panel refresh validation (Phase 151): tightened `ScreenFormatDialogSectionsContainVisibleContent` to assert the Choose Display Mode custom list contains >200 non-background pixels, the Display Information and Credits headings are visible, and the text hook captures all three semantic headings. This confirms the existing initial `IDCMP_REFRESHWINDOW` path in `OpenWindow()` triggers DPaint's custom panel paint. Targeted DPaint regression passes.

**Version 0.10.2** - `OpenMonitor()` / `CloseMonitor()` + `GfxBase->MonitorList` real implementation (Phase 150b): implemented monitor list initialization and lookup for default/PAL/NTSC monitor specs, added monitor-list coverage, and fixed DPaint Screen Format capture timing after window creation.

**Version 0.10.0** - Deferred-paint trigger / forced full redraw (Phase 149): added `lxa_force_full_redraw()` host API with `EMU_CALL_INT_FORCE_FULL_REDRAW` (3030) — ROM VBlank hook walks FirstScreen→FirstWindow chain and sends `IDCMP_REFRESHWINDOW` to all eligible windows on demand. Fixed `display_read_pixel()` rootless-mode bug: in rootless mode `g_active_display` is NULL so all `CountScreenContent` calls silently returned 0; added fallback that reads from `display_window_t` pixel buffers with correct `host_x/host_y` coordinate translation. Added `lxa_set_active_window()` / `display_set_active_by_index()` API. Re-enabled `MaxonBasicTest.ZMenuDragPixelStability` with corrected scan region. 72/72 pass.

**Version 0.9.32** - Typeface deeper workflows — textfield.gadget integration and Preview window (Phase 147c): root-caused `BGUI_EXTERNAL_GADGET` class failure (ExternalObject wrapping textfield.gadget was NULL because `InitExtClass()` fallback in `libfunc.c` was never reached); fixed by removing orphaned debug kprintfs that masked the control flow — `InitExtClass()` now correctly initialises in-process when the gadget file is absent. With ExternalObject working, `PreviewWndObj` is non-NULL and `WindowOpen` succeeds. Three new Typeface tests: `TextFieldGadgetLoads`, `PreviewWindowOpens`, `ZPreviewWindowHasContent`. 70/72 pass (2 pre-existing: maxonbasic_gtest, dopus_gtest).

**Version 0.9.31** - Typeface window geometry investigation (Phase 147b): root-cause analysis confirmed that 194×138 is the correct BGUI minimum window size for Typeface on a first-run PAL/topaz-8 system. The 280×195 reference came from a saved-prefs state on FS-UAE, not a comparable baseline. Mathematical proof: `Box.Height=138` from Typeface's do-while loop; BGUI WinSize min-width=194 from CharGadget (8 cols × 20px) + PropObject(16) + borders. `WindowGeometryMatchesTarget` test asserts 194×138 ±4px. 72/72 pass.

**Version 0.9.30** - Typeface custom-screen window chrome (Phase 147a): fixed per-screen-type host-window architecture; rootless+Workbench → each child window is its own X11 window; rootless+custom-screen → screen owns one host window with chrome rendered into the screen bitmap. New `WindowChromeIsRendered` test. 72/72 pass.

**Version 0.9.27** - Intuition WZOOM gadget and Devpac border rendering (Phase 145): added `case GTYP_WZOOM:` to `_render_window_frame()` with outlined-rectangle imagery; WZOOM gadget created in `_create_window_sys_gadgets()` when `WFLG_HASZOOM` is set; `WA_Zoom` tag now sets `WFLG_HASZOOM`; `ZipWindow` behavior wired in `_handle_sys_gadget_verify`. New `ZoomWindow` sample. New `devpac_scrollbar_gtest.cpp` (3 tests). 71/71 pass.

**Version 0.9.26** - ProWrite About dialog re-enabled (Phase 140): fixed View-menu drag coordinates (was using Project-menu x+240, now correctly targets View menu at x+396; item y was y+150 hitting Quit, now y+15 for the About item). `DismissWindow` now iterates gadgets in reverse order and clicks the first non-zero-size gadget, correctly targeting ProWrite's OK button (gadget[0], size 60×15). Diagnostic helper tests removed. Full suite 68/68.

**Version 0.9.21** - GadTools submenu pixel test re-enabled (Phase 139): `GadToolsMenuPixelTest.SubmenuHoverDoesNotCorruptLowerMainItems` re-enabled with no code change required; the hang was already resolved by the Phase 132/133 menu compose + backing-store work. Verified stable across 5 consecutive runs. Full suite 68/68.

**Version 0.9.20** - Resize-rendering test reliability fix (Phase 138): re-enabled `GadToolsGadgetsPixelTest.ResizeKeepsSizeGadgetBordersClean`. The size-gadget chrome WAS being re-rendered correctly after a resize — the test simply sampled the screen too early. `_intuition_SizeWindow` updates `Window->Width/Height` *before* invoking `_render_window_frame`, so polling `GetWindowInfo` for the final dimensions is not proof of rendering completion. Replaced the fixed 20-VBlank settle wait with an event-driven loop that polls the size-gadget pixel area until non-background pixels appear (proof that `_render_gadget` for `GTYP_SIZING` has executed). Also reverted a stray `LPRINTF(LOG_INFO)` in `_graphics_Draw` left over from Phase 134 debugging (per AGENTS §6.3). CMakeLists `gadtoolsgadgets_pixels_c_gtest` shard FILTER updated to include the re-enabled test. Full suite 68/68.

**Version 0.9.19** - RunCommand exit-code propagation (Phase 137): `_dos_RunCommand` previously installed `pr_ExitCode`/`pr_ExitData` *after* `_dos_CreateNewProc` had already enqueued the child and yielded to it, so a fast child could call `Exit(rc)` before the runcommand cleanup was wired up — the child's return code was lost and the parent always saw `result == -1`. `_dos_CreateNewProc` now honours the RKRM `NP_ExitCode` / `NP_ExitData` tags and threads a user-supplied exit callback through `lxa_dos_process_exit_cleanup` (so internal teardown still runs first), and `_dos_RunCommand` passes its result-capture callback via those tags atomically as part of process construction. `DosTest.LoadSegRuntime` re-enabled; full suite 68/68.

**Version 0.9.9** - BlitzBasic 2 ted editor deep dive (Phase 135): original blitter/copper hypothesis invalidated by instrumentation — BB2 emits zero blitter and zero copper writes during startup. Real root cause: BB2 opens an "About BlitzBasic 2" requester with garbage requested coordinates (LE=32614, TE=32729) and `WaitPort()`s on its UserPort, blocking the editor until the user clicks "OKEE DOKEE". The pre-existing off-screen clamp in `_intuition_OpenWindow` clipped the requester to a single pixel at (639,255), making the dismiss button unreachable. Fix: when a requested window position would place the window mostly off-screen, recentre it on the screen instead of clipping to the edge. Also: RAWKEY events now route to `IntuitionBase->ActiveWindow` instead of the window under the mouse pointer (real Amiga semantics), so BB2 receives keyboard input correctly after the requester is dismissed. `StartupOpensVisibleIdeWindow` flipped from upper-bound (`<10` non-grey pixels) to lower-bound (`>1000`) assertion. All 10 BB2 driver tests pass; About requester + PROJECT menu render correctly (verified via `tools/screenshot_review.py`).

**Version 0.9.8** - DirectoryOpus button bank rendering (Phase 134): root cause diagnosed via Phase 130 text hook — DOpus renders its main button bank (Copy/Move/Rename/Makedir/Hunt/Run/Comment etc.) via a VBlank-driven timer event that fires ~400 VBlanks after startup, beyond the previous 150-iteration settle window. Fixed by bumping settle to 600 VBlank iterations. `TextHookCapturesKnownDOpusLabels` now asserts specific button-bank label words. All diagnostic LPRINTFs from the previous investigation session cleaned up.

**Version 0.9.7** - Menu double-buffering (Phase 133): `_render_menu_bar()` and `_render_menu_items()` in `lxa_intuition.c` now render into a persistent off-screen compose BitMap and atomic-`BltBitMap` the completed frame to the screen, eliminating pixel flicker during menu drags on ASM-One and MaxonBASIC. `ZMenuFlickerCheck` upgraded to a ±5% pixel-stability assertion; new `ZMenuDragPixelStability` test guards the MaxonBASIC editor area against menu residue. Full suite 67/67 pass.

**Version 0.9.6** - SMART_REFRESH backing store validation (Phase 132): new `BackingStoreTest` sample (`samples/intuition/backingstoretest.c`) drives a dialog-over-window obscure/uncover cycle through three deterministic stages; new `backingstore_gtest.cpp` driver (4 tests) probes pixels in the formerly-obscured region and proves Phase 111's backing store correctly restores SMART_REFRESH content on uncover. Defensive `IS_SMARTREFRESH` skip in `DamageExposedAreas()` removed; apps now receive correct IDCMP_REFRESHWINDOW messages on uncover (matching real Amiga behaviour) without losing pixels. Full suite 67/67 pass.

**Version 0.9.5** - Intuition event log + menu introspection (Phase 131): circular 256-entry event log (`lxa_push_intui_event`, `lxa_drain_intui_events`) records `OpenWindow`/`CloseWindow`/`OpenScreen`/`CloseScreen` events from `lxa_dispatch.c`; `lxa_get_menu_strip()` traverses the emulated Intuition `MenuStrip` linked list to expose menu titles, item names, enabled/checked state, and geometry; `LxaTest::DrainEvents()`, `GetMenuStrip()`, `GetMenuNames()` helpers added to the test framework. Tests in simplemenu, maxonbasic, devpac, and api drivers.

**Version 0.9.4** - Text() interception (Phase 130): `lxa_set_text_hook()` / `lxa_clear_text_hook()` via `EMU_CALL_GFX_TEXT_HOOK`; ROM `_graphics_Text()` fires the hook on every render call; semantic text assertions in api, dopus, and kickpascal drivers.

**Version 0.9.3** - Screen-mode emulation (Phase 128 + Phase 129 partial): display pipeline optimization with dirty-region scanline tracking and SSE2 planar-to-chunky acceleration; ROM-side screen-mode virtualization complete — `g_known_display_ids[]` expanded to 36 ECS entries, `GetDisplayInfoData(DTAG_DIMS)` returns realistic raster ranges, `BestModeIDA()` honours `DIPFMustHave`/`MustNotHave`, `OpenScreenTagList()` accepts RTG-depth requests and maps unknown display IDs to the closest physical mode (Wine strategy). PPaint and FinalWriter validation deferred to Phase 139 (RTG app validation via Picasso96).

**Version 0.9.1** - Memory access fast paths (Phase 127): direct bswap-based 16/32-bit RAM and ROM reads/writes; `__builtin_expect` hints on hot paths.

**Version 0.9.0** - Profiling infrastructure (Phase 126): per-EMU_CALL counters + timing; `--profile` flag; `tools/profile_report.py`; baseline latency tests across six app drivers.

**Version 0.8.90** - `lxa.c` decomposition (Phase 125): 9,960-line monolith split into `lxa_custom.c`, `lxa_dispatch.c`, `lxa_dos_host.c`, `lxa_internal.h`, `lxa_memory.h`; `lxa.c` reduced to ~1,658 lines.

See [roadmap.md](roadmap.md) for detailed status and future plans.

## Known Limitations

- Machine: the A4000/040 of the reference is emulated (68040 with FPU, no MMU; AGA chipset; 2 MB chip, 8 MB fast and 16 MB Zorro III RAM). The 68040 MMU and the A4000 gate arrays (Ramsey, Gary) are not emulated.
- Third-party libraries such as `commodities.library`, `rexxsyslib.library`, `datatypes.library`, and `dopus.library` are not shipped in ROM and must be installed on disk in `LIBS:`. Stub disk libraries are provided for `amigaguide.library`, `req.library`, `reqtools.library`, `powerpacker.library`, and `arp.library` (apps can OpenLibrary them but all functions return safe failure values).
- RTG: enabled per run (`--rtg`, `LXA_RTG=1`, `config.rtg`). `Picasso96API.library` and `cybergraphics.library` are lxa's own; menus of windows on the 8-bit RTG Workbench still highlight differently from the reference (Phase 256).
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

lxa's own code is under the MIT License - see [LICENSE](LICENSE) for details.

Some parts are third-party code under other licences and are **not** MIT licensed: BGUI (`src/bgui/`) is under the LGPL. Each such part keeps its original licence in its own directory; [doc/third-party-code.md](doc/third-party-code.md) lists them all.

## Contribution Areas

We welcome contributions! Priority areas:
- **RTG / Picasso96**: the remaining divergences on the RTG Workbench recorded in goldens (Phase 256)
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
