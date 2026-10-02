# RDD snapshot formats

Version 1 (Phase 212). Both backends produce the **same raw artefacts**:

| Artefact | Reference (real AmigaOS 3.1) | lxa |
|---|---|---|
| Intuition tree | `lxaprobe DUMP_TREE file` | `lxa_dump_tree_json(path)` |
| Pen-index pixels | `lxaprobe SNAP file [WINDOW n]` | `lxa_capture_screen_indexed()` / `lxa_capture_window_indexed(n)` |
| `Text()` calls | `lxaprobe TEXT_START` / `TEXT_DUMP file` | `lxa_text_log_start()` / `lxa_text_log_dump()` |
| Library trace | `lxaprobe TRACE` / `TRACE_DUMP` | Phase 233 |
| Program output | (redirected to `NIL:` — CLI output via `LXAREF:` files) | `lxa_get_output()` |

`tools/rdd/bundle.py` turns them into a **snapshot bundle**, the unit the
comparator (Phase 214) and the goldens (Phase 215) work with.

## Bundle (`lxa-bundle/1`)

A directory per snapshot:

| File | Content |
|---|---|
| `meta.json` | `{"schema":"lxa-bundle/1","backend":"lxa"\|"ref","scenario","snapshot","profile","window","screen":{width,height,depth},"files":[...]}` — JSON Schema `tools/rdd/schemas/meta.schema.json` |
| `screen.png` | 8-bit palette PNG; **pixel value = pen index** (palette for display only) |
| `palette.json` | `[[r,g,b], ...]` per pen, 8-bit components |
| `tree.json` | `lxa-tree/1` (below), keys sorted |
| `text.jsonl` | optional: one `{"text","x","y"}` per `Text()` call |
| `stdout.txt` | optional: program output |
| `trace.jsonl` | optional: library call trace |
| `unimplemented.json` | optional, lxa only: stub telemetry (Phase 203) |

`tools/rdd/bundle.py:validate_bundle()` checks a bundle (JSON Schema plus
file presence, image size and pen range).

## Raw pixels (`LXASNAP1`)

```
LXASNAP1 <width> <height> <depth> <ncolors>\n
<ncolors x 3 bytes RGB, 8 bit>
<width x height bytes, pen index per pixel, row-major>
```

The reference reads pixels with `ReadPixel()` from the screen RastPort and
the palette with `GetRGB32()`; lxa reads the screen BitMap planes and its
display palette. Snapshots and pointer coordinates refer to the **front
screen** (`IntuitionBase->FirstScreen`). `WINDOW n` (n-th window of the front
screen's `FirstWindow` chain) crops the window rectangle out of the screen. Pen
indices make the comparison independent of colour depth and RGB rounding;
the palette is compared separately.

## Text log (`text.jsonl`)

One `{"text","x","y"}` per `Text()` call (`x`, `y` = RastPort pen position,
layer-relative). The reference hooks `Text()` with `SetFunction`, so it sees
only calls made through the graphics.library vector (application code and
libraries calling through `GfxBase`); ROM-internal calls (e.g. Intuition
drawing gadget labels) bypass it. lxa sees every call. Compare in the
direction "strings the reference drew that lxa did not" (`missing_in_lxa`).

## Intuition tree (`lxa-tree/1`)

JSON Schema: `tools/rdd/schemas/tree.schema.json`. All values are the raw
NDK structure fields (no normalisation — the comparator decides what to
ignore):

- `screens[]`: `title`, `default_title`, `left`, `top`, `width`, `height`,
  `depth`, `display_id` (`ColorMap->VPModeID`), `flags`, `bar_height`,
  `bar_vborder`, `bar_hborder`, `menu_vborder`, `menu_hborder`,
  `wbor` (`[left, top, right, bottom]`), `font`, `windows[]`.
- `windows[]`: `title`, `screen_title`, `app` (opened by the program under
  test; always `true` on lxa), geometry, `min_*`/`max_*`, `flags`, `idcmp`,
  `border` (`[left, top, right, bottom]`), `detail_pen`, `block_pen`, `font`
  (`RPort->Font`), `gadgets[]`, `menus[]`.
- `gadgets[]` (the `FirstGadget` chain, system gadgets included):
  `id`, `type`, `flags`, `activation`, `left`, `top`, `width`, `height`
  (raw, i.e. relative values for `GFLG_REL*` gadgets), `text[]`
  (IntuiText chain when the label is an IntuiText).
- `menus[]`: `title`, geometry, `flags`, `items[]`; `items[]`: geometry,
  `flags`, `mutualexclude`, `command` (COMMSEQ key or 0), `text`
  (IntuiText label or `null` for image items), `sub[]`.
- `font`: `{name, ysize, xsize, baseline}` or `null`.

Strings are UTF-8 (converted from Latin-1).

## Versioning

Any incompatible change bumps the version (`lxa-tree/2`, `LXASNAP2`,
`lxa-bundle/2`) in the agent, `lxa_rdd.c`, the schemas and this document in
the same commit. Checked-in goldens record the version in `meta.json`.
