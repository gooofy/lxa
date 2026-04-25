---
name: develop-amiga
description: AmigaOS implementation rules, code style, memory management, and debugging tips.
---

# AmigaOS Development Skill

This skill covers the core principles, rules, and conventions for developing AmigaOS components in `lxa`.

## 1. Quality & Stability Requirements
**`lxa` is a runtime environment and operating system implementation.**
- **Zero Tolerance for Instability**: Memory leaks, crashes, race conditions, and undefined behavior are unacceptable.
- **100% Code Coverage**: Every line of code must be exercised by tests. No exceptions.
- **Compatibility**: We strive for 100% AmigaOS compatibility.

## 2. Mandatory Reference Sources
**CRITICAL**: Consult these sources in order of priority when implementing any component:
1. **RKRM Documentation** (`~/projects/amiga/rkrm/`) - Official behavior spec.
2. **RKRM Sample Code** (`~/projects/amiga/sample-code/rkm/`) - Official usage patterns.
3. **AROS Source Code** (`~/projects/amiga/lxa/src/lxa/others/AROS-20231016-source`) - Reference (Clean Room only).
4. **NDK/SDK** (`/opt/amiga/src/amiga-gcc/projects/NDK3.2`) - Headers/Autodocs (also in markdown format).

**Implementation Checklist:**
- [ ] Read RKRM documentation
- [ ] Study RKRM sample code
- [ ] Review AROS implementation
- [ ] Verify behavior matches RKRM
- [ ] Ensure sample programs work

## 3. Code Style & Conventions
- **Indentation**: 4 spaces.
- **Naming**: `snake_case` for internal variables/functions.
- **Comments**: C-style `/* ... */` preferred.

### ROM Code (`src/rom/`)
- **Brace Style**: **Allman** (braces on new line).
- **Types**: Amiga types (`LONG`, `BPTR`, `STRPTR`, `BOOL`).
- **Logging**: `DPRINTF(LOG_DEBUG, ...)` or `LPRINTF(LOG_ERROR, ...)`.
- **ASM**: Inline `__asm("...")` for register args.

### System Commands (`sys/`)
- **Brace Style**: **K&R** (braces on same line).
- **Types**: Amiga types (`LONG`, `BPTR`, `STRPTR`).
- **IO**: Use `Printf`, `Read`, `Write` (AmigaDOS API).

### Host Code (`src/lxa/`)
- **Brace Style**: Mixed (prefer Allman).
- **Types**: Standard C (`uint32_t`) and Amiga types for memory.
- **Memory**: Use `m68k_read/write_memory_*`.

## 4. Implementation Rules
1. **Conventions**: Respect existing naming/formatting.
2. **Safety**: Check pointers (`BADDR`), handle `IoErr()`.
3. **Scope**: Do not re-implement third-party libraries in `lxa`. They must be installed on disk and loaded normally.
4. **Proactiveness**: Add `EMU_CALL` for new packets, update roadmap.

## 5. Common Patterns
- **BPTR**: `BADDR(bptr)` (Amiga->Host), `MKBADDR(ptr)` (Host->Amiga).
- **Tags**: `GetTagData` for parsing.

## 6. Debugging & Logging
- **LPRINTF(level, ...)**: Always enabled.
- **DPRINTF(level, ...)**: Enabled with `ENABLE_DEBUG` in `src/rom/util.h`.
- **STRORNULL(s)**: Use this macro for printing strings to avoid crashes on NULL.

## 7. ROM Code Constraints
- **No Writable Static Data**: ROM is read-only.
- **Solution**: Use `AllocVec`/`AllocMem` for writable globals.

## 8. Stack Size Considerations
- **Default Stack**: 4KB (often too small for large arrays).
- **Rule**: Local arrays < 256 bytes. Use `AllocVec` for larger buffers.
- **Symptoms**: Corrupted args, pointers becoming garbage.

## 9. Memory Debugging
- Trace pointer values.
- Verify address ranges:
  - `0x400-0x20000`: System structures.
  - `0x20000-0x80000`: User memory.

## 10. GCC m68k Cross-Compiler Pitfalls

### 10.1 `move.w` Register Argument Truncation
GCC's m68k backend may emit `move.w` instead of `move.l` for d-register
arguments in inline `__asm` stubs when it believes the value fits in 16 bits.
This silently truncates 32-bit values.

**Pattern**: Any ROM function that receives coordinates, sizes, pen numbers,
or small counts through the Amiga register-calling convention.

**Fix**: Apply `(LONG)(WORD)val` sign-extension before the inline asm boundary:
```c
/* In the ROM function stub: */
LONG __saveds _lxa_Move(register __a1 struct RastPort *rp __asm("a1"),
                        register __d0 LONG x __asm("d0"),
                        register __d1 LONG y __asm("d1"))
{
    /* x and y might be truncated to 16-bit by GCC */
    x = (LONG)(WORD)x;  /* force sign-extension */
    y = (LONG)(WORD)y;
    /* ... */
}
```

**Do NOT apply** to genuinely 32-bit values: IFF chunk types, DoPkt action
codes, math operands, tag IDs, memory sizes, APTR/BPTR values.

**Audit rule**: Every new ROM function taking coordinate/size parameters MUST
be checked for this bug. The Phase 105 sweep fixed 92+ functions across 13
ROM files — use those as a reference.

### 10.2 `a2` Register Clobber
GCC's m68k register allocator sometimes clobbers `a2` across function calls in
complex ROM code (observed in layer-creation functions).

**Symptoms**: A pointer argument suddenly points to garbage after a nested ROM
function call (via JSR).

**Fix**: Apply `__attribute__((optimize("O0")))` to the affected function.

**Note**: This is a blunt workaround. If you find a more targeted fix (e.g.,
adding `a2` to the clobber list of specific inline asm), that is preferred.

### 10.3 Inline ASM Best Practices
- Always list all modified registers in the clobber list.
- Use `volatile` on asm statements that have side effects.
- When in doubt, inspect the generated assembly with `m68k-amigaos-objdump -d`.

## 11. Musashi CPU Emulator Notes

### Edge-Triggered IRQs
Musashi will NOT re-trigger an interrupt at the same level unless the line is
lowered first. Always pulse:
```c
m68k_set_irq(0);
m68k_set_irq(3);  /* Now the CPU sees a fresh rising edge */
```

### Cycle Accuracy
Musashi provides total instruction cycle counts but not per-bus-access timing.
For lxa's purposes this is sufficient, but be aware that cycle-exact DMA
contention is not modeled.

## 12. App Compatibility Patterns

### Applications Without COMMSEQ
Some apps (e.g., ASM-One V1.48) display keyboard shortcuts in menus but do NOT
set the Intuition `COMMSEQ` flag. They handle shortcuts internally via raw IDCMP
keyboard events. For these apps, menu items can only be triggered via RMB drag,
not CommKey delivery.

### In-Place Rendering
Some apps render command responses in-place (same window) rather than opening a
new window. Detect success via pixel count change instead of waiting for a new
window to appear.

## 13. Disassembling Amiga Binaries to Diagnose App Misbehaviour

When an app misbehaves in `lxa` and source is unavailable, disassembly is the
fastest way to discover the exact ROM API contract the app expects. This
technique was used in Phase 151 to locate DPaint's mode-list populator from a
single clue ("Screen Format dialog body is blank") in under 30 minutes.

### 13.1 Toolchain

```bash
export PATH=/opt/amiga/bin:$PATH    # required — tools are NOT in default PATH

m68k-amigaos-objdump -D <binary>    # full disassembly; works on hunk format
m68k-amigaos-strings -t x <binary>  # all strings with hex offsets
m68k-amigaos-nm <binary>            # symbols if present (often stripped)
m68k-amigaos-readelf -a <binary>    # rarely useful for hunk binaries
```

`m68k-amigaos-objdump` understands AmigaOS `loadseg()`-able executables
directly. It reports `file format amiga` and produces section disassembly
beginning at offset 0 of `.text`. No need to extract sections manually.

### 13.2 Workflow for "App opens window but body is blank"

1. **Find the window's title string** in the strings dump:
   `m68k-amigaos-strings -t x app | grep -i "screen format"` →
   gives a file offset; subtract the `.text` base (usually 0) to get the
   hunk-internal address.

2. **Find xrefs to that string** by grepping the disassembly for the address
   as an immediate or PC-relative load. Look for `move.l #<addr>,...`,
   `pea <addr>(pc)`, `lea <addr>(pc),...`. Many apps use a string-id table
   (`(LONG id, STRPTR str)`) and dereference it via a `GetString(id)` helper —
   in that case grep the table region for the string address.

3. **Locate the OpenWindow call** in the surrounding routine. The standard
   Intuition LVO offsets are:
   - `-204` `OpenWindow`
   - `-606` `OpenWindowTagList`
   - `-612` `OpenWindowTags` (varargs entry, tail-calls TagList)
   - The library base is in `a6` per Amiga ABI, loaded from a base-relative
     slot on `a4` (e.g. `movea.l 2920(a4),a6` ⇒ IntuitionBase if 2920 is the
     OpenLibrary slot).

4. **Inspect the post-OpenWindow code path** (the next 30–100 instructions
   after `jsr -606(a6)`). Look for:
   - `jsr -84(a6)` on GadToolsBase ⇒ `GT_RefreshWindow` (app self-refreshes;
     it does NOT wait for `IDCMP_REFRESHWINDOW`).
   - `jsr -42(a6)` on GadToolsBase ⇒ `GT_SetGadgetAttrsA` (populates
     listview/cycle/slider gadgets — labels, top-pick, selected, etc.).
   - `jsr -714(a6)` on GfxBase ⇒ `OpenMonitor` (probes monitor list; if
     lxa returns NULL, the app's mode enumerator yields zero entries).
   - `jsr -720(a6)` on GfxBase ⇒ `NextDisplayInfo` (mode enumeration loop).
   - `jsr -252(a6)` on IntuitionBase ⇒ `ScreenToFront`.
   - A `Wait()`/`WaitPort()`/`GT_GetIMsg` loop ⇒ the app is reactive only;
     the blank panel is NOT due to a missing `IDCMP_REFRESHWINDOW`.

### 13.3 Recognising Library Bases via OpenLibrary Sequences

App startup (typically the first ~1000 instructions) contains a chain of
`OpenLibrary` calls. Each stores the returned base into an `a4`-relative slot.
Recognise the pattern:

```
lea     <name>(pc),a1        ; "intuition.library"
moveq   #<version>,d0
movea.l _AbsExecBase,a6
jsr     -552(a6)             ; OpenLibrary
move.l  d0,2920(a4)          ; IntuitionBase
```

Build a base-slot table early — every subsequent `movea.l N(a4),a6` becomes
trivially identifiable:

| Slot | Library |
|---|---|
| 2916(a4) | GfxBase (example, varies per app) |
| 2920(a4) | IntuitionBase |
| 2876(a4) | GadToolsBase |
| 2888(a4) | UtilityBase |

### 13.4 Hunk-Format Quirks

- `objdump` reports section disassembly starting at offset 0 of each hunk,
  not at a runtime virtual address. All addresses you see in the dump are
  **hunk-internal offsets**. At runtime, `LoadSeg()` allocates each hunk
  somewhere in chip/fast RAM and patches relocations. Cross-references
  between hunks appear as relocs in the dump.
- Data hunks (BSS, DATA, CHIP) are listed separately. PC-relative loads
  inside `.text` reach data hunk content via the relocation tables; objdump
  shows the resolved offset.
- Common LVO offsets (memorise these — they appear constantly):
  - exec: `-198 AllocMem`, `-210 FreeMem`, `-294 OpenLibrary`,
    `-414 PutMsg`, `-372 GetMsg`, `-384 WaitPort`, `-318 Wait`.
  - intuition: `-30 OpenWindow`, `-36 CloseWindow`, `-198 PrintIText`,
    `-204 RefreshGadgets`, `-468 NewModifyProp`, `-606 OpenWindowTagList`.
  - graphics: `-30 BltClear`, `-60 Text`, `-72 ?`, `-240 Draw`, `-246 Move`,
    `-252 SetAPen`, `-714 OpenMonitor`, `-720 NextDisplayInfo`,
    `-756 GetDisplayInfoData`.

### 13.5 When to Delegate to a Subagent

Disassembly output for a 600 KB binary is ~10 MB of text — far too large for
the main context. **Always delegate** the actual disassembly sweep to a
`general` subagent with a focused prompt that asks for:
- the address of a specific string,
- the routine that opens the corresponding window,
- the first 30–50 instructions after that `OpenWindow` returns,
- a list of library calls (LVO offsets) in that routine.

Phase 151's prompt template is preserved in git history and is a good
starting point. The subagent returns a 1–2 page summary; the main context
then translates findings into a targeted fix.

### 13.6 Verifying the Hypothesis Without Running the App

Once the disassembly identifies a suspect ROM call (e.g. `OpenMonitor(NULL,
0)`), grep the lxa source to check the implementation status before changing
anything else:

```bash
grep -n "_graphics_OpenMonitor\b" src/rom/lxa_graphics.c
```

If the function body contains `return NULL;` with a comment like *"apps
should handle NULL gracefully"*, the hypothesis is essentially confirmed
(see AGENTS.md §6.21). Implement the function properly per RKRM/NDK and
add a regression test.
