/* RGBBoxes.c - Adapted from RKM Graphics Library sample
 *
 * Demonstrates custom ViewPort construction with colored boxes.
 * Based on the RKM "RGBBoxes" example from:
 *   RKRM: Libraries, Chapter "Graphics Primitives"
 *
 * This version uses the 1.3-compatible code path since lxa does not
 * yet support the Release 2 extended structures (GfxNew, VideoControl).
 *
 * The sample:
 * 1. Creates a custom View and ViewPort
 * 2. Allocates a 2-plane BitMap (640x400)
 * 3. Sets up a 4-color palette (black, red, green, blue) via LoadRGB4
 * 4. Draws three colored boxes directly into bitplane memory
 * 5. Displays for 10 seconds, then restores the original view
 */

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <libraries/dos.h>

#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>
#include <stdlib.h>

#define DEPTH   2       /* Number of bitplanes */
#define WIDTH   640     /* HIRES width */
#define HEIGHT  400     /* Interlaced height */

VOID drawFilledBox(WORD, WORD);
VOID cleanup(int);
VOID fail(STRPTR);

struct GfxBase *GfxBase = NULL;

/* Construct a simple display. Global for easy cleanup. */
struct View view;
struct View *oldview = NULL;
struct ViewPort viewPort = { 0 };
struct BitMap bitMap = { 0 };
struct ColorMap *cm = NULL;

UBYTE *displaymem = NULL;   /* Pointer for writing to BitMap memory */

#define BLACK   0x000       /* RGB values for the four colors */
#define RED     0xf00
#define GREEN   0x0f0
#define BLUE    0x00f

VOID main(VOID)
{
    WORD depth, box;
    struct RasInfo rasInfo;

    /* Offsets in BitMap where boxes will be drawn.
     * Each box is WIDTH/2 wide and HEIGHT/2 tall.
     * BytesPerRow for 640 pixels = 80 bytes.
     *
     * Box 1 (red):   row 10, col 10 bytes -> offset = 10*80 + 10 = 810
     * Box 2 (green): row 25, col 25 bytes -> offset = 25*80 + 25 = 2025
     * Box 3 (blue):  row 40, col 40 bytes -> offset = 40*80 + 40 = 3240
     */
    static SHORT boxoffsets[] = { 810, 2025, 3240 };

    static UWORD colortable[] = { BLACK, RED, GREEN, BLUE };

    /* Open the graphics library */
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 33L);
    if (GfxBase == NULL)
        fail("Could not open graphics library\n");

    /* Save current View to restore later */
    oldview = GfxBase->ActiView;

    InitView(&view);
    view.Modes |= LACE;    /* Set interlace mode */

    /* Initialize the BitMap for RasInfo */
    InitBitMap(&bitMap, DEPTH, WIDTH, HEIGHT);

    /* Set plane pointers to NULL for cleanup safety */
    for (depth = 0; depth < DEPTH; depth++)
        bitMap.Planes[depth] = NULL;

    /* Allocate space for BitMap */
    for (depth = 0; depth < DEPTH; depth++)
    {
        bitMap.Planes[depth] = (PLANEPTR)AllocRaster(WIDTH, HEIGHT);
        if (bitMap.Planes[depth] == NULL)
            fail("Could not get BitPlanes\n");
    }

    /* Initialize the RasInfo */
    rasInfo.BitMap = &bitMap;
    rasInfo.RxOffset = 0;
    rasInfo.RyOffset = 0;
    rasInfo.Next = NULL;

    /* Initialize the ViewPort */
    InitVPort(&viewPort);
    view.ViewPort = &viewPort;
    viewPort.RasInfo = &rasInfo;
    viewPort.DWidth = WIDTH;
    viewPort.DHeight = HEIGHT;

    /* Set display mode */
    viewPort.Modes = HIRES | LACE;

    /* Initialize the ColorMap (4 entries for 2 planes) */
    cm = GetColorMap(4L);
    if (cm == NULL)
        fail("Could not get ColorMap\n");

    /* Attach the ColorMap (1.3-style) */
    viewPort.ColorMap = cm;

    /* Set colors: black, red, green, blue */
    LoadRGB4(&viewPort, colortable, 4);

    printf("RGBBoxes: ColorMap loaded (BLACK, RED, GREEN, BLUE)\n");

    /* Construct preliminary Copper instruction list */
    MakeVPort(&view, &viewPort);

    /* Merge into a real Copper list */
    MrgCop(&view);

    /* Clear the bitplanes */
    for (depth = 0; depth < DEPTH; depth++)
    {
        displaymem = (UBYTE *)bitMap.Planes[depth];
        BltClear(displaymem, (bitMap.BytesPerRow * bitMap.Rows), 1L);
    }

    /* Display the custom view */
    LoadView(&view);

    printf("RGBBoxes: Custom view loaded (%dx%d, %d planes)\n",
           WIDTH, HEIGHT, DEPTH);

    /* Draw three colored boxes: red, green, blue.
     * Each box is WIDTH/2 pixels wide and HEIGHT/2 pixels tall.
     * Draw into both planes to produce true colors:
     *   Color 1 (RED):   plane0=1, plane1=0
     *   Color 2 (GREEN): plane0=0, plane1=1
     *   Color 3 (BLUE):  plane0=1, plane1=1
     */
    for (box = 1; box <= 3; box++)
    {
        for (depth = 0; depth < DEPTH; depth++)
        {
            displaymem = bitMap.Planes[depth] + boxoffsets[box - 1];
            drawFilledBox(box, depth);
        }
    }

    printf("RGBBoxes: Drew 3 colored boxes (red, green, blue)\n");

    /* Pause for 10 seconds */
    printf("RGBBoxes: Waiting 10 seconds...\n");
    Delay(10L * TICKS_PER_SECOND);

    /* Restore the original view */
    LoadView(oldview);
    WaitTOF();

    /* Deallocate the hardware Copper list created by MrgCop() */
    FreeCprList(view.LOFCprList);
    if (view.SHFCprList)
        FreeCprList(view.SHFCprList);   /* Interlace short frame list */

    /* Free all intermediate Copper lists from MakeVPort() */
    FreeVPortCopLists(&viewPort);

    printf("RGBBoxes: Done.\n");
    cleanup(RETURN_OK);
}


/*
 * fail(): print error and cleanup
 */
VOID fail(STRPTR errorstring)
{
    printf(errorstring);
    cleanup(RETURN_FAIL);
}


/*
 * cleanup(): free everything that was allocated
 */
VOID cleanup(int returncode)
{
    WORD depth;

    /* Free the ColorMap */
    if (cm)
        FreeColorMap(cm);

    /* Free the BitPlanes */
    for (depth = 0; depth < DEPTH; depth++)
    {
        if (bitMap.Planes[depth])
            FreeRaster(bitMap.Planes[depth], WIDTH, HEIGHT);
    }

    /* Close the graphics library */
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);

    exit(returncode);
}


/*
 * drawFilledBox(): create a WIDTH/2 by HEIGHT/2 box of color
 *                  "fillcolor" into the given bitplane.
 *
 * The color is decomposed into plane bits:
 *   If bit 'plane' of fillcolor is set, fill with 0xFF (all pixels on)
 *   Otherwise fill with 0x00 (all pixels off)
 */
VOID drawFilledBox(WORD fillcolor, WORD plane)
{
    UBYTE value;
    WORD boxHeight, boxWidth, width;

    /* WIDTH/2 pixels, 8 pixels per byte */
    boxWidth = (WIDTH / 2) / 8;
    boxHeight = HEIGHT / 2;

    value = ((fillcolor & (1 << plane)) != 0) ? 0xff : 0x00;

    for (; boxHeight; boxHeight--)
    {
        for (width = 0; width < boxWidth; width++)
            *displaymem++ = value;

        displaymem += (bitMap.BytesPerRow - boxWidth);
    }
}
