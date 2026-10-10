# AROS reference audit (Phase 245)

AROS code may not be copied into lxa (APL 1.1 does not fit lxa's MIT
licence). AROS may only be read to understand behaviour. This audit covers
every place in lxa's own code that said it was taken from or based on AROS
(`git grep -n -i aros -- src sys tools tests ':!src/bgui'`, 2026-10-10). Each
site was compared with `others/AROS-20231016-source`.

Verdicts:

- **rewritten**: the code was copied or closely translated, so it was
  replaced by lxa's own implementation with the same behaviour.
- **reworded**: the code is lxa's own and only the comment cited AROS. The
  comment now cites the NDK autodoc, the RKRM or the 3.1 reference.
- **interface fact**: the value is a binary interface, so it stays and only
  the internal names were changed.

| Site | Verdict | Action |
|---|---|---|
| `lxa_graphics.c` AreaMove/AreaDraw/AreaEllipse, `areaclosepolygon`, `AREAINFOFLAG_*` (two copies) | rewritten | Closely translated from AROS `areamove.c`/`areadraw.c`/`areaellipse.c`/`areafill.c`. Replaced by `gfx_area_room/last_tag/append/close_shape` and the private tags `GFX_AREA_*`. AreaEnd (already lxa's own) uses the new tags. |
| `lxa_graphics.c` AddFont, RemFont | rewritten | Same statements as AROS `addfont.c`/`remfont.c`. Restructured and commented from graphics.doc. |
| `lxa_graphics.c` FontExtent | rewritten | Translation of AROS `fontextent.c` (sentinel min/max loop). Rewritten with lxa's own variables and logic, same results. |
| `lxa_graphics.c` InitBitMap | rewritten | The BytesPerRow expression and comment came from AROS. Now `((w+15)/16)*2`, with the 3.1 behaviour cited (`tests/graphics/init_bitmap`). |
| `lxa_graphics.c` InitRastPort defaults | reworded | The values are facts verified on the reference (`tests/graphics/init_rastport` expected.ref.out). lxa's code (field-by-field, font via GfxBase) is not AROS's. |
| `lxa_graphics.c` InitVPort, InitView | reworded | AROS copies a template struct; lxa sets the fields itself. Comment cites graphics.doc. |
| `lxa_graphics.c` AskFont, GetColorMap, FreeColorMap, GetRGB4 | reworded | lxa's own structure with values from the reference (`graphics/colormap` probe). Comment cites graphics.doc. |
| `lxa_graphics.c` RectInRegion/PointInRegion private slots | reworded | AROS has no such functions; they are lxa extensions. Comments here, in `tests/graphics/regions*` and in `tests/ref_suite.yaml` were corrected. |
| `lxa_graphics.c` LxaBltBitMapArgs comment, `emucalls.h` EMU_CALL_GFX_BLT_BITMAP | reworded | Was an architecture analogy only, with no code. |
| `lxa_intuition.c` DrawBorder | rewritten | Closely translated from AROS `drawborder.c` (same comments and loop). Rewritten from intuition.doc. |
| `lxa_intuition.c` DoubleClick | rewritten | Followed AROS `doubleclick.c` (normalise to the earlier second). Rewritten. The 0.5 s fallback without preferences is unchanged. |
| `lxa_intuition.c` RemoveGadget, ReportMouse | reworded | lxa's own code (AROS RemoveGadget is a one-line RemoveGList call). Comment cites intuition.doc. |
| `lxa_intuition.c` CommKey (two comments), `lxa_notify_window_refresh`, gadget domain, cycle-gadget notes (two), OpenWindowTagList defaults | reworded | Behaviour notes only. |
| `exec.c` `__AROS_GETJUMPVEC`/`__AROS_SETVECADDR`/`__AROS_INITVEC` | rewritten | Unused AROS macros removed. MakeFunctions was restructured into one loop for both array kinds. |
| `exec.c` ReleaseSemaphore wake-up | rewritten | Closely translated from AROS `releasesemaphore.c`. Replaced by `exec_semaphore_grant`/`exec_semaphore_wake` with the same hand-over rules (shared head grants every shared request, Procure messages replied). |
| `exec.c` Exception | rewritten | Closely translated from AROS `exception.c`. Restructured; the comment cites exec.doc/RKRM. |
| `exec.c` WaitIO | rewritten | Closely translated from AROS `waitio.c` (same loop and comments). Rewritten from exec.doc. |
| `exec.c` Alert comment | reworded | Now cites exec.doc. |
| `lxa_layers.c` new-layer backfill, BeginUpdate, EndUpdate, SwapBitsRastPortClipRect, SortLayerCR | reworded | lxa's own code (SortLayerCR is a single insertion sort, AROS uses directional comparator passes; SwapBits uses a temporary bitmap). Comments cite layers.doc. |
| `lxa_layers.c` DoHookClipRects | reworded | lxa's own structure. The coordinate conversion was restated in lxa's own form. |
| `util.h` GETHEAD/GETTAIL/GETSUCC/GETPRED | reworded | The AROS 2023 `exec/lists.h` has no such macros. lxa's own; the comment cites RKRM list semantics. |
| `graphics/layersext.h` `LA_AROS` | interface fact | Tag values kept (`TAG_USER+1234`, `+3`, `+4`); the base was renamed `LA_ExtBase`. |
| `lxa_iprefs.h` SetIPrefs type codes | interface fact | Codes and layouts are the binary interface of the private slot; lxa names were already its own. Comment no longer cites AROS as a source. |
| `lxa_api.c` MenuStrip offset | reworded | Now cites NDK `intuition/intuition.h`. |
| tests: `console/sgr_unit`, `drivers/intuitext_gtest`, `exec/exec_misc`, `exec/exec_verify`, `graphics/init_bitmap`, `graphics/init_rastport` | reworded | Comments cite the NDK/3.1 reference. The printed title "Exec AROS Verification Tests" stays because the reference output (`expected.ref.out`) contains it. |

Totals: 10 rows rewritten, 13 reworded, 2 kept as interface facts (a row can cover several functions or comments).
The BCPL support and the gadget classes were replaced earlier in this phase
(`doc/bcpl.md`, `doc/gadget-classes.md`).
