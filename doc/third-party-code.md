# Third-party code in lxa

lxa's own code is MIT licensed. Code ported from other projects keeps its
original licence header and lives in its own directory together with the
licence text. Every ported component is listed here.

**AROS code may not be ported (decided 2026-10-10).** AROS is a reference
only. The AROS component below predates that decision and is being
replaced by lxa's own implementation in Phase 245 (the BCPL support that
Phase 239 had ported is lxa's own code since Phase 245, see `doc/bcpl.md`).

| Component | Directory | Origin | Licence | Phase |
|---|---|---|---|---|
| BGUI | `src/bgui/` | BGUI | LGPL | - |
| BOOPSI gadget classes colorwheel.gadget, gradientslider.gadget, tapedeck.gadget (LIBS:gadgets/) | `src/rom/gadgets/` (`colorwheel.c`, `colorwheel_fixmath.h`, `gradientslider.c`, `tapedeck.c`; `classlib.[ch]` is lxa's own) | AROS `workbench/classes/gadgets/{colorwheel,gradientslider,tapedeck}` | AROS Public License 1.1 (`src/rom/gadgets/LICENSE`) | 238 |

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
