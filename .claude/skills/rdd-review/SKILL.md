---
name: rdd-review
description: Compare lxa against the real AmigaOS 3.1 reference for a scenario run - tree diff, pen-index pixel diff, then vision triage of the composite - and write findings.yaml where every finding ends in a roadmap phase or a deterministic assertion.
---

# rdd-review — reviewing lxa against the reference

Use after `python3 -m rdd run` (see the `rdd-reference` skill). RDD principle 2
(AGENTS.md §1a): **structure first, then pixels, then vision**. Vision explains;
it never is the evidence.

## 1. Produce the comparison

```bash
cd tools
python3 -m rdd run ../tests/scenarios/<s>.yaml --out ../build/rdd   # both backends
python3 -m rdd report ../build/rdd                                   # compare + HTML
```

Per snapshot you get `build/rdd/<scenario>/compare/<snapshot>/`:
`compare.json` (tree diffs, pixel regions, palette, Text(), stdout) and
`composite.png` (lxa | reference | diff, ×2, 16 px grid, yellow every 64 px).
`build/rdd/compare-summary.json` lists all snapshots with their verdict.

## 2. Read the structure first

Open `compare.json`. Tree differences often explain most pixels:
- screen fields (`wbor`, `bar_height`, `display_id`, `font`) shift every window;
- window `top`/`border`/`flags` move content;
- gadget `count`/`type`/geometry differences point at gadtools/intuition.
Note each relevant diff verbatim — it is the evidence line of a finding.

## 3. Vision triage of the composite

Open `composite.png` with the `Read` tool. Then:
1. Enumerate every differing region (use the red diff panel and the grid to
   give coordinates in bundle pixels = composite pixels / 2).
2. Classify each: `chrome` (borders, title bar, system gadgets), `gadget-imagery`,
   `text` (font, position, missing strings), `layout` (offsets/sizes),
   `missing-content`, `colour` (palette/pens), `behaviour`, `other`.
3. Merge regions with one cause (a 9 px vertical shift of all content is ONE
   layout finding, not one per gadget).
4. Name the suspected library and function (`intuition/OpenScreen`,
   `gadtools/CreateGadgetA`, …). Check `doc/stub-inventory.md` and the
   bundle's `unimplemented.json` — a hit there is usually the cause.

## 4. Write findings.yaml

Next to the composite: `build/rdd/<scenario>/compare/<snapshot>/findings.yaml`.
Format and validator: `tools/rdd/findings.py`
(`python3 -m rdd.findings <file>` must print `OK`). Every finding needs
- at least one **deterministic evidence** line (tree diff, palette diff,
  pixel region with count, Text() diff),
- an **action**: the roadmap `phase` that owns the fix and/or the
  `assertion` that will prove it fixed (usually "tree field equals reference"
  or "region pixel diff = 0").

## 5. Close the loop

- Each finding becomes a TODO in its phase (or a new numbered phase — no
  pooling sections) and, when fixed, an assertion in a golden test
  (Phase 215: `rdd promote`).
- Never "fix" a golden to match lxa (principle 4); if the reference result
  looks wrong, re-capture it.
- Findings across several apps with one root cause are clustered by the
  `compat-sweep` procedure before phases are opened.
