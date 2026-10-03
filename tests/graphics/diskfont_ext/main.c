/*
 * Test: graphics/diskfont_ext
 *
 * lxa-only (tests/ref_suite.yaml): the diskfont.library V45+ functions
 * (GetDiskFontCtrl, SetDiskFontCtrlA, outline fonts, WriteFontContents,
 * WriteDiskFontHeaderA, ObtainCharsetInfo) do not exist in the AmigaOS 3.1
 * diskfont.library V40 of the reference machine.
 */

#define DISKFONT_EXT 1
#include "../diskfont_contents/main.c"
