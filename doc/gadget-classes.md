# BOOPSI gadget classes: colorwheel, gradientslider, tapedeck

AmigaOS 3.1 ships three public BOOPSI gadget classes as disk libraries in
`LIBS:gadgets/`. lxa implements them in `src/rom/gadgets/` as its own MIT
code, written from the NDK (`gadgets/colorwheel.h`, `gradientslider.h`,
`tapedeck.h` and their autodocs) and from what the 3.1 classes do on the
reference machine. They are built with `add_disk_library()` in
`sys/CMakeLists.txt`.

| File | Contents |
|---|---|
| `classlib.[ch]` | Library frame shared by the three classes: RomTag, Open/Close/Expunge, `MakeClass()` + `AddClass()` of the public class at init, delayed expunge while objects exist; gadget box, `GM_RENDER` and `OM_NOTIFY` helpers |
| `colorwheel.c` | colorwheel.gadget (class and the `ConvertHSBToRGB()`/`ConvertRGBToHSB()` library functions) |
| `colorwheel_math.h` | 16 bit HSB/RGB conversion, integer square root, direction and sine approximations |
| `gradientslider.c` | gradientslider.gadget |
| `tapedeck.c` | tapedeck.gadget |

The probes `tests/probes/colorwheel/{class,render}.c`,
`tests/probes/gradientslider/class.c` and `tests/probes/tapedeck/class.c`
check the rules below against their reference output (`*.ref.out`, captured
on AmigaOS 3.1; run by `ctest -R ref_expected_outputs`).

## Library

- Each library has version 40 and the class name as its node name
  (`colorwheel.gadget`); opening it again by its path or by its bare name
  returns the same base (exec finds a library loaded as
  `gadgets/colorwheel.gadget` by the name's file part).
- Objects are `gadgetclass` subclasses: `GadgetType` 5
  (`GTYP_CUSTOMGADGET`), `Flags` `GFLG_EXTENDED`, `Activation` 0.

## colorwheel.gadget

### ConvertHSBToRGB() / ConvertRGBToHSB()

3.1 converts with 16 bit math ("rounding errors" in the autodoc's BUGS) and
reproduces lxa's results bit for bit:

- Only the upper 16 bits of each 32 bit component are used. The 16 bit
  result is returned in both words (`0x1234` becomes `0x12341234`).
- HSB to RGB: saturation 0 gives grey (all channels = brightness).
  Otherwise the hue circle has six sectors of `0x2AAB`; within a sector the
  offset is stretched to 0..`0xFFFF` as `((offset << 16) - 1) / 0x2AAA`
  (0 for offset 0, at most `0xFFFF`). With `f(a, b) = a * b >> 16`, the
  channel levels are: full = brightness, floor = `f(b, 0xFFFF - s)`,
  falling = `f(b, 0xFFFF - f(s, ramp))`, rising =
  `f(b, 0xFFFF - f(s, 0xFFFF - ramp))`. The sectors red-yellow,
  yellow-green, ... assign them as (full, rising, floor), (falling, full,
  floor), (floor, full, rising), (floor, falling, full), (rising, floor,
  full), (full, floor, falling).
- RGB to HSB: brightness = the largest channel, saturation =
  `spread * 0xFFFF / max` (0 for black). A grey (spread 0) has no hue: 3.1
  leaves whatever value is in the hue field; lxa returns 0 from the library
  function and keeps the current hue in the class. Otherwise the hue is
  counted in sixths of `0xFFFF`: the sector of the dominant channel (red 0,
  green 2, blue 4; red wins ties, then green) plus the difference of the
  next and the previous channel over the spread, `* 0xFFFF` and rounded away
  from zero; wrapped into 0..`6 * 0xFFFF` and divided by 6.

### Attributes

- Defaults: hue 0, saturation and brightness `0xFFFFFFFF`, RGB
  (`0xFFFFFFFF`, 0, 0).
- The object keeps HSB as its master copy (16 bits per component, read back
  widened) and derives RGB from it, so a hue set as `0x20000000` reads back
  as `0x20002000`.
- `OM_SET`/`OM_NEW` apply every HSB attribute first, then the RGB
  attributes (`WHEEL_Red/Green/Blue/RGB`, full 32 bit values) replace
  channels of the colour the HSB values give, and the result is converted
  back to HSB. A grey result keeps the hue.
- `OM_SET` returns bit 0 when hue or saturation changed and bit 1 when the
  brightness changed (0 when nothing changed, e.g. a value set and set back
  within one call, or `GA_Disabled`, `GA_Left`, `WHEEL_Abbrv`).
- `WHEEL_Screen`, `WHEEL_Abbrv`, `WHEEL_MaxPens`, `WHEEL_GradientSlider`,
  `WHEEL_BevelBox` and `WHEEL_Donation` cannot be read (`OM_GET` fails).
- An object can be created without `WHEEL_Screen` (the autodoc calls the tag
  required; 3.1 accepts it). Without a screen no pens are allocated.
- `WHEEL_GradientSlider`: the slider holds the brightness as
  `GRAD_CurVal = 0xFFFF - brightness`. The wheel sets it at `OM_NEW` and
  whenever its brightness changes, and reads it back on every `OM_GET` and
  `OM_SET` (so `GRAD_CurVal 0x1000` on the slider reads as brightness
  `0xEFFFEFFF` and red `0xEFFFEFFF` from a red wheel).
- A disabled wheel is drawn like an enabled one.

### Look

- Red at the top, the hue increasing clockwise, saturation from the centre
  (0) to the rim (`0xFFFF`). The wheel is an ellipse inset 2 pixels from the
  gadget box (4 with `WHEEL_BevelBox`, which draws a raised button frame),
  with a two pixel shadow-pen outline. Without the box only the ellipse is
  drawn over the window background (through `GA_BackFill` if given).
- Colour: a cube of 6 down to 2 levels per gun. At full brightness only its
  surface is needed (`L^3 - (L-1)^3` pens); the largest cube that fits the
  screen depth and `WHEEL_MaxPens` plus the donated pens, and whose pens
  can all be obtained exactly (`ObtainBestPen()` with `PRECISION_EXACT`),
  is used, ordered dithered with a 4 x 4 Bayer matrix. Donated pens are set
  with `SetRGB32()` and used first.
- Monochrome (no cube possible, e.g. the 4 colour Workbench): a black disc,
  spokes between the colour sectors and the `WHEEL_Abbrv` letters
  ("GCBMRY": green, cyan, blue, magenta, red, yellow) at their colours.
- The knob is a 7 x 7 ring with a cross in it; what lies under it is saved
  and restored when it moves (`GREDRAW_UPDATE`).

## gradientslider.gadget

- Defaults: `GRAD_MaxVal` `0xFFFF`, `GRAD_CurVal` 0, `GRAD_SkipVal`
  `0x1111`, `GRAD_KnobPixels` 5, `PGA_Freedom` `LORIENT_HORIZ`.
- `OM_GET` knows `GRAD_CurVal` only (the others, and `GA_ID`, fail).
- `GRAD_CurVal` is clamped to `GRAD_MaxVal`; a new `GRAD_MaxVal` (any value,
  also above `0xFFFF` or 0) clamps `GRAD_CurVal` to it, it does not rescale.
  At `OM_NEW` the tags are applied in order, so `GRAD_CurVal 300,
  GRAD_MaxVal 100` gives 100, and `GRAD_CurVal 0x20000` alone gives `0xFFFF`.
- `OM_SET` returns 1 when `GRAD_CurVal` changed or `GRAD_MaxVal` or
  `GRAD_PenArray` (also NULL) were given; 0 otherwise (`GRAD_SkipVal`,
  `GRAD_KnobPixels`, `GA_Disabled`, `GA_Left`).
- Look: a button frame over the whole box; the knob track is inset 2 pixels
  left/right and 1 top/bottom; the gradient fills the track inset by one
  more pixel. No pens: background pen; one pen: that pen; more: the pens at
  evenly spaced points with 3.1's ordered dither in between - along a
  segment the level `(pos + 1) * 17 / length` selects the second pen where
  it reaches the threshold `{16,4,13,1 / 8,12,5,9 / 14,2,15,3 / 6,10,7,11}`
  at (row & 3, column & 3). The knob sits at
  `round(cur * (track - knob) / max)`: a shadow-pen bar, over a gradient
  (2 or more pens) surrounded by one pixel of background pen. A disabled
  slider is drawn like an enabled one.
- Input: a click beside the knob moves the value by `GRAD_SkipVal` towards
  the click (clamped) and ends; on the knob it drags (value =
  `knob offset * max / travel`), the menu button cancels the drag.

## tapedeck.gadget

- A new object is always 202 x 16 pixels, whatever `GA_Width`/`GA_Height`
  say, with `GFLG_RELSPECIAL` set; `GA_Width`/`GA_Height` in a later
  `OM_SET` do change the size.
- Defaults: `TDECK_Mode` `BUT_STOP`, `TDECK_Paused` FALSE, `TDECK_Frames`
  10, `TDECK_CurrentFrame` 0.
- `TDECK_Mode` takes `BUT_REWIND` .. `BUT_STOP` only (`BUT_PAUSE` and above
  are ignored); `TDECK_Paused` is stored as 0/1; `TDECK_CurrentFrame` is not
  clamped to `TDECK_Frames`; `TDECK_Tape` cannot be read; `OM_SET` always
  returns 0.
- Look (animation controls): four parts at 0, 27, 75 and 102 of 202 pixels
  (scaled to the width): rewind, play and fast forward as button frames
  (the current mode's button selected) with 8 x 7 arrow heads in the text
  pen, and the frame position part with a 6 pixel wide knob. 3.1 draws the
  play button's bottom edge one pixel to the right, in the shine pen. A
  disabled deck gets a `0x8888/0x2222` ghost pattern in the shadow pen. The
  tape recorder look (`TDECK_Tape`) is not drawn yet (`LXA_UNIMPLEMENTED`,
  see `doc/stub-inventory.md`): the animation controls are shown instead.
- Input: rewind and fast forward set their mode while held and restore the
  previous one on release; play toggles between `BUT_PLAY` and `BUT_STOP`
  when released over it; the frame position part follows the mouse.
