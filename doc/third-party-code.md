# Third-party code in lxa

lxa's own code is MIT licensed. Code ported from other projects keeps its
original licence header and lives in its own directory together with the
licence text. Every ported component is listed here.

| Component | Directory | Origin | Licence | Phase |
|---|---|---|---|---|
| BCPL support for dos.library (global vector, BCPL call/return, BCPL runtime routines, writef, rdargs, CLI start-up of BCPL processes) | `src/rom/bcpl/` | AROS `arch/m68k-all/dos/` (`bcpl.S`, `bcpl.inc`, `bcpl.h`, `bcpl_writef.S`, `bcpl_support.c`, `bcpl_patches.c`, `bcpl_readargs.c`, `bcpl_putpkt.c`, `callentry.S`), `rom/dos/newcliproc.c` (BCPL CliInit handling) | AROS Public License 1.1 (`src/rom/bcpl/LICENSE`) | 239 |
| BGUI | `src/bgui/` | BGUI | LGPL | - |
| BOOPSI gadget classes colorwheel.gadget, gradientslider.gadget, tapedeck.gadget (LIBS:gadgets/) | `src/rom/gadgets/` (`colorwheel.c`, `colorwheel_fixmath.h`, `gradientslider.c`, `tapedeck.c`; `classlib.[ch]` is lxa's own) | AROS `workbench/classes/gadgets/{colorwheel,gradientslider,tapedeck}` | AROS Public License 1.1 (`src/rom/gadgets/LICENSE`) | 238 |

## BCPL support (Phase 239)

AmigaOS 3.1 still runs BCPL programs: 1.x `C:` commands and the BCPL
handlers. They are entered with `a2` = the global vector, `a5`/`a6` = the
BCPL call/return routines and `a1` = the BCPL stack (frames grow upwards),
and call system routines with `movea.l n(a2),a4; moveq #k,d0; jsr (a5)`.

lxa changes to the AROS code, besides the ABI (leading underscores, stack
arguments instead of `AROS_UFH` register macros):

- `RunCommand()` and the bootstrap enter every command with the BCPL
  registers, as 3.1 does (`tests/probes/dos/entryregs.c`): `a2` = the
  process's `pr_GlobVec` (the system global vector `dl_GV`), `a5`/`a6` =
  `BCPL_jsr`/`BCPL_rts`, `a1` = the stack bottom.
- The system segments in lxa's `pr_SegList` (`[1]`, `[2]`) stand for AROS'
  `-1`/`-2` markers: installing one into a global vector fills in the
  system entries (the 1.3 command start-up code installs them into its
  private global vector).
- The routines that address memory through `a0` set it to 0 themselves:
  3.1's routines work when called with `a0` = the command arguments.
- multiply/divide use 68020 `MULS.L`/`DIVSL.L`; number output divides
  unsigned (hex/octal print the bit pattern of negative numbers).
- `rn_ConsoleSegment` is a small BCPL segment in ROM whose global 1 starts
  a CLI for KS 1.3 `Run`/`NewCLI` (`createProcBCPL`): it handles the
  startup packet like AROS' `internal_CliInitAny()` (a `dp_Type` above 1 is
  the caller's BCPL CliInit routine) and runs lxa's shell.
- Fixes to the AROS code: `getvec`'s `SetIoErr(ERROR_NO_FREE_STORE)`,
  `getword`/`putword` saving `d2`, `rename` losing its BCPL frame pointer,
  `makesysreq` passing the error code to `ErrorReport()`.
- The entries AROS left as debug-print dummies (coroutines, `longjump`,
  `sysRequest`, ...) are visible stubs (`LXA_UNIMPLEMENTED`, see
  `doc/stub-inventory.md`).

## Gadget classes (colorwheel, gradientslider, tapedeck)

The AmigaOS 3.1 BOOPSI gadget classes in `LIBS:gadgets/` are disk libraries
(`add_disk_library()` in `sys/CMakeLists.txt`) built from the AROS classes.
`classlib.c` (lxa, MIT) is the common library frame: it opens the libraries
the class needs, `MakeClass()`es + `AddClass()`es the public class at init
and removes/frees it at expunge (delayed while objects exist).

lxa changes to the AROS code, each observed on the 3.1 reference
(`tests/probes/{colorwheel,gradientslider,tapedeck}/`):

- colorwheel: `ConvertHSBToRGB()`/`ConvertRGBToHSB()` reproduce 3.1 bit for
  bit (16 bit components, hue sectors of 0x2AAB, rounding as 3.1); OM_SET
  applies the HSB attributes before the RGB ones, keeps HSB as the master
  copy and returns which parts changed; objects can be created without
  `WHEEL_Screen`; only the pen based wheel (no cybergraphics path).
- gradientslider: OM_GET knows `GRAD_CurVal` only; `GRAD_MaxVal` clamps
  instead of rescaling; OM_SET return values; 3.1's ordered dither and knob
  look (pixel identical for two-pen gradients).
- tapedeck: new objects are 202 x 16 with `GFLG_RELSPECIAL`; defaults
  `BUT_STOP`/10 frames; `TDECK_Paused` attribute; the animation controls
  drawn as 3.1 does (pixel identical), replacing AROS' prop gadget strip.
  The tape recorder look (`TDECK_Tape`) is still drawn as animation
  controls (`LXA_UNIMPLEMENTED`).
- No ghost pattern for `GA_Disabled` (3.1 draws disabled colorwheel and
  gradientslider objects like enabled ones).
