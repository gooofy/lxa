# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.2** | **Phase 45 Complete** | **36 Integration Tests Passing**

The lxa project has achieved a comprehensive AmigaOS-compatible environment with 95%+ library compatibility across Exec, DOS, Graphics, Intuition, and system libraries.

---

## Core Philosophy & Boundaries

### WINE-like DOS Approach
Instead of emulating hardware-level disk controllers and running Amiga-native filesystems (like OFS/FFS) on top, `lxa` maps Amiga logical drives directly to Linux directories.

- **Drives as Directories**: `DH0:`, `DF0:`, etc., are backed by standard Linux directories.
- **Host Interception**: DOS calls (`Open`, `Read`, `Lock`, etc.) are handled by the host `lxa` process using Linux system calls.
- **Auto-Provisioning**: On first run, `lxa` creates a default `~/.lxa` structure with `SYS:`, `S:`, `C:`, etc., and populates them with a basic `Startup-Sequence` and essential tools.

### Support Boundaries

| Feature | Status | Note |
| :--- | :--- | :--- |
| **Exec Multitasking** | ✅ Full | Signal-based multitasking, Message Ports, Semaphores |
| **DOS Filesystem** | ✅ Full | Mapped to Linux via VFS layer |
| **Interactive Shell** | ✅ Full | Amiga-compatible Shell with prompt, history, scripting |
| **Assigns** | ✅ Full | Logical assigns, multi-directory assigns (Paths) |
| **Graphics/Intuition** | ✅ 95% | SDL2-based rendering, windows, screens, gadgets |
| **Floppy Emulation** | ✅ Basic | Floppies appear as directories |

---

## Completed Phases (1-41)

### Foundation (Phases 1-12)
- ✅ Exec multitasking with preemptive scheduling
- ✅ DOS filesystem with case-insensitive VFS
- ✅ Interactive Shell with scripting support
- ✅ 100+ built-in commands (DIR, TYPE, COPY, etc.)
- ✅ utility.library, mathffp.library, mathtrans.library

### Graphics & UI (Phases 13-25)
- ✅ SDL2 integration for display output
- ✅ graphics.library - drawing primitives, blitting, text
- ✅ intuition.library - screens, windows, gadgets, IDCMP
- ✅ layers.library - clipping, layer management
- ✅ console.device - full ANSI/CSI escape sequences
- ✅ Rootless windowing, mouse tracking, keyboard input

### Application Support (Phases 26-35)
- ✅ Real-world app testing: KickPascal 2, GFA Basic, Devpac, MaxonBASIC, EdWordPro, SysInfo
- ✅ Library stubs: gadtools, workbench, asl, commodities, rexxsyslib, iffparse, reqtools, diskfont, icon, expansion, translator, locale, keymap
- ✅ 68030 CPU support, custom chip registers, CIA resources
- ✅ ROM robustness: pointer validation, crash recovery

### Library Completion (Phases 36-38)
- ✅ **exec.library** - 100% complete (172 functions)
- ✅ **graphics.library** - 90%+ complete (text, area fills, blitting, display modes)
- ✅ **intuition.library** - 95%+ complete (windows, screens, gadgets, BOOPSI, IDCMP)

### Testing & Fixes (Phases 39-40)
- ✅ **Phase 39**: Re-testing identified rendering issues in applications
- ✅ **Phase 39b**: Enhanced test infrastructure (bitmap capture, validation API, screenshot comparison)
- ✅ **Phase 40**: Fixed GFA Basic screen height bug (21px→256px), verified Devpac rendering pipeline

### IFF & Datatypes (Phases 41-42)
- ✅ **Phase 41**: iffparse.library full implementation
  - AllocIFF, FreeIFF, InitIFFasDOS
  - OpenIFF, CloseIFF with read/write modes
  - PushChunk, PopChunk with IFFSIZE_UNKNOWN support
  - ReadChunkBytes, WriteChunkBytes
  - ParseIFF with IFFPARSE_SCAN and IFFPARSE_RAWSTEP
  - GoodID, GoodType, IDtoStr validation functions
  - CurrentChunk, ParentChunk navigation
- ✅ **Phase 42**: datatypes.library (Basic Framework)
  - NewDTObjectA, DisposeDTObject - create and destroy datatype objects
  - GetDTAttrsA, SetDTAttrsA - get and set object attributes
  - ObtainDataTypeA, ReleaseDataType - datatype descriptor management
  - Basic datatypesclass BOOPSI class with OM_NEW, OM_DISPOSE, OM_SET, OM_GET
  - Support for DTA_TopVert, DTA_TotalVert and other scroll attributes
  - GetDTString for localized error messages
  - AddDTObject, RemoveDTObject for window integration
  - RefreshDTObjectA, DoAsyncLayout, DoDTMethodA for rendering
  - GetDTMethods, GetDTTriggerMethods for method queries
- ✅ **Phase 43**: console.device Advanced Features
  - CONU_CHARMAP and CONU_SNIPMAP unit modes for character map binding
  - RawKeyConvert() library function (wrapper around MapRawKey)
  - CSI 'r' (DECSTBM) - Set Top and Bottom Margins for scrolling regions
  - CSI '{' (Amiga-specific) - Set scrolling region from top
  - Scrolling functions respect scroll_top and scroll_bottom boundaries
- ✅ **Phase 44**: BOOPSI & GadTools Visual Completion
  - ROM size increased to 512KB to accommodate new functionality
  - propgclass GM_HANDLEINPUT - proportional gadget dragging with mouse
  - strgclass GM_RENDER - cursor display at BufferPos
  - strgclass GM_HANDLEINPUT - full keyboard input (Return, Escape, Backspace, Delete, arrows, character input)
  - DrawBevelBoxA - actual bevel box rendering with shine/shadow pens
  - GTBB_Recessed, GT_VisualInfo, GTBB_FrameType tag support
  - Frame types: BBFT_BUTTON, BBFT_RIDGE, BBFT_DISPLAY
  - ASL FileRequester - full GUI implementation with window, gadgets, directory listing
  - AllocAslRequest, FreeAslRequest, AslRequest functions
  - ASLFR_* tag parsing for all requester options

---

## Active Phase

### Phase 45: Async I/O & Timer Completion (Complete)
**Goal**: Implement proper asynchronous I/O for devices.

- [x] **timer.device Async** - TR_ADDREQUEST with delay queue and VBlank-driven expiration
  - Host-side timer queue (64 pending requests)
  - EMU_CALL_TIMER_ADD/REMOVE/CHECK/GET_EXPIRED emucalls
  - VBlank hook integration for timer checking
  - Proper ReplyMsg() from m68k side to wake waiting tasks
  - AbortIO support for timer cancellation
  - Integration test: devices/timer_async
- [x] **console.device Async** - Non-blocking reads with pending queue
  - ConsoleDevBase structure for tracking open console units
  - CMD_READ goes async when no input available (pending_read field)
  - VBlank hook processes pending reads when input arrives
  - AbortIO properly cancels pending reads with IOERR_ABORTED
  - Fixed exec.c AbortIO to use proper register calling convention
  - Integration test: devices/console_async
- [x] **Event Loop Integration** - WaitPort/Wait() work correctly with async I/O

---

## Future Phases

### Phase 46: Directory Opus Deep Dive
**Goal**: Full Directory Opus 4 compatibility.

- [ ] Stack corruption analysis during cleanup
- [ ] dopus.library task cleanup debugging
- [ ] Memory corruption detection (guards/canaries)
- [ ] powerpacker.library - basic decompression
- [ ] inovamusic.library - stub if needed

### Phase 47: Pixel Array Operations
**Goal**: Implement optimized pixel array functions using AROS-style blitting.

**Background**: Simple WritePixel() loops caused crashes. AROS uses specialized `write_pixels_8()` helpers.

- [ ] ReadPixelLine8, WritePixelLine8 - horizontal lines
- [ ] ReadPixelArray8, WritePixelArray8 - rectangular regions
- [ ] WriteChunkyPixels - fast chunky writes with bytesperrow
- [ ] Handle BitMap formats (interleaved, standard)
- [ ] Layer/clipping integration

---

## Version History

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
| 0.6.2 | 45 | console.device async (CMD_READ pending queue, VBlank hook, AbortIO), exec.c AbortIO fix |
| 0.6.1 | 45 | timer.device async (TR_ADDREQUEST delay queue, VBlank hook, AbortIO) |
| 0.6.0 | 44 | BOOPSI visual completion (propgclass, strgclass input), GadTools DrawBevelBoxA, ASL FileRequester GUI, ROM size 512KB |
| 0.5.9 | 43 | console.device advanced features (CONU_CHARMAP, RawKeyConvert, scrolling regions) |
| 0.5.8 | 42 | datatypes.library basic framework |
| 0.5.7 | 41 | iffparse.library full implementation |
| 0.5.6 | 40 | GFA Basic height fix, Devpac rendering verified |
| 0.5.5 | 39b | Test validation infrastructure |
| 0.5.4 | 38 | Intuition library completion |
| 0.5.3 | 37 | Graphics library completion |
| 0.5.2 | 36 | Exec library completion |

---
