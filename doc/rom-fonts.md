# ROM fonts (topaz 8 and topaz 9)

graphics.library's built-in fonts (`src/rom/romfont_data.h`) are generated
data. They were obtained by black-box observation of AmigaOS 3.1 (KS 40.70),
running on the reference machine, never from a Kickstart image or AmigaOS
source (clean-room, AGENTS.md §1a rule 5):

1. The probe `tests/probes/graphics/romfont.c` opens topaz 8 and topaz 9 with
   `OpenFont()` and prints every `TextFont` field, the `CharLoc`, `CharSpace`
   and `CharKern` tables, an FNV-1a hash of the glyph strike, and every glyph
   (plus the default glyph used for characters outside `LoChar..HiChar`) as
   `Text()` draws it into a bitmap, rows as hex. It also renders a sample
   string in all algorithmic styles.
2. Its output on the reference is captured with
   `cd tools && python3 -m rdd suite-ref --filter Probes/graphics/romfont --capture-ref`
   into `tests/probes/graphics/romfont.ref.out`.
3. `tools/gen_romfont.py` rebuilds each strike from the rendered glyphs:
   glyph *i* occupies the strike columns given by `CharLoc[i]` (topaz 9's
   glyphs overlap in the strike; the overlapping renders agree). The rebuilt
   strike reproduces the hash the reference printed, so it is byte-identical.
   The script writes `romfont_data.h`; ctest `romfont_data_check` keeps the two
   in sync, and `ref_expected_outputs` checks that lxa prints exactly the
   reference output of the probe.

What 3.1 reports: topaz 8 is 8x8, baseline 6, flags `ROMFONT|DESIGNED`,
`ln_Pri` 10; topaz 9 is 10x9 (`tf_XSize` 10), baseline 6, style `EXTENDED`,
flags `ROMFONT|TALLDOT|DESIGNED`, `ln_Pri` 0. Both cover characters 32-255
with no `CharSpace`/`CharKern` tables; characters 128-159 use the default
glyph (a hollow box). `OpenFont()` without an exact size returns the closest
size (5, 7 -> 8; 10..20 -> 9) and `ta_YSize` 0 matches nothing.
