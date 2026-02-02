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
3. **AROS Source Code** (`others/AROS-20231016-source/`) - Reference (Clean Room only).
4. **NDK/SDK** (`/opt/amiga/src/amiga-gcc/projects/NDK3.2`) - Headers/Autodocs.

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
3. **Proactiveness**: Add `EMU_CALL` for new packets, update roadmap.

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
