/*
 * measuretext.c - Text Measurement Sample
 *
 * This is an RKM-style sample demonstrating text measurement functions.
 * It demonstrates:
 *   - TextLength() to get the pixel width of a string
 *   - TextExtent() to get full extent information
 *   - TextFit() to determine how much text fits in a given space
 *
 * Key Functions Demonstrated:
 *   - TextLength() - quick pixel width calculation
 *   - TextExtent() - detailed extent info (bounds, kerning)
 *   - TextFit() - fitting text into a given space
 *   - SetFont() / SetSoftStyle()
 *
 * Based on RKM Graphics sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <graphics/rastport.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;
struct Library *GfxBase;

VOID main(int argc, char **argv)
{
    struct Window *win;
    struct RastPort *rp;
    struct TextExtent te;
    LONG textlen, fit;
    UBYTE *testString = "Hello, Amiga World!";
    UBYTE *longString = "This is a longer string that might not fit in the given space.";
    LONG maxWidth = 150;  /* Maximum pixel width for TextFit test */

    printf("MeasureText: Starting text measurement demonstration\n");

    if (GfxBase = OpenLibrary("graphics.library", 37))
    {
        printf("MeasureText: Opened graphics.library\n");

        if (IntuitionBase = OpenLibrary("intuition.library", 37))
        {
            printf("MeasureText: Opened intuition.library\n");

            if (win = OpenWindowTags(NULL,
                    WA_Width, 400,
                    WA_Height, 150,
                    WA_CloseGadget, TRUE,
                    WA_DragBar, TRUE,
                    WA_DepthGadget, TRUE,
                    WA_Title, "Text Measurement Demo",
                    WA_Activate, TRUE,
                    TAG_END))
            {
                printf("MeasureText: Window opened successfully\n");

                rp = win->RPort;

                /* Test 1: TextLength() - simple width measurement */
                printf("MeasureText: Test 1 - TextLength()\n");
                textlen = TextLength(rp, testString, 19);
                printf("MeasureText: String '%s'\n", testString);
                printf("MeasureText: TextLength() = %ld pixels\n", textlen);

                /* Display the string and show its measured width */
                SetAPen(rp, 1);
                Move(rp, 10, 30);
                Text(rp, testString, 19);

                /* Draw a line showing the measured width */
                SetAPen(rp, 2);
                Move(rp, 10, 35);
                Draw(rp, 10 + textlen, 35);

                /* Test 2: TextExtent() - detailed extent info */
                printf("MeasureText: Test 2 - TextExtent()\n");
                TextExtent(rp, testString, 19, &te);
                printf("MeasureText: TextExtent results:\n");
                printf("MeasureText:   Width = %d, Height = %d\n",
                       te.te_Width, te.te_Height);
                printf("MeasureText:   Extent MinX=%d MinY=%d MaxX=%d MaxY=%d\n",
                       te.te_Extent.MinX, te.te_Extent.MinY,
                       te.te_Extent.MaxX, te.te_Extent.MaxY);

                /* Test 3: TextFit() - fitting text in limited space */
                printf("MeasureText: Test 3 - TextFit()\n");
                printf("MeasureText: Testing how much of '%s' fits in %ld pixels\n",
                       longString, maxWidth);

                fit = TextFit(rp, longString, 63, &te, NULL, 1, maxWidth, 100);
                printf("MeasureText: TextFit() = %ld characters fit\n", fit);
                printf("MeasureText: Resulting extent width = %d\n", te.te_Width);

                /* Display the portion that fits */
                SetAPen(rp, 1);
                Move(rp, 10, 60);
                Text(rp, longString, fit);

                /* Draw a box showing the constraint */
                SetAPen(rp, 3);
                Move(rp, 10, 65);
                Draw(rp, 10 + maxWidth, 65);
                Move(rp, 10 + maxWidth, 45);
                Draw(rp, 10 + maxWidth, 65);

                /* Test 4: Font metrics */
                printf("MeasureText: Test 4 - Font Metrics\n");
                printf("MeasureText: Current font:\n");
                printf("MeasureText:   Name = '%s'\n",
                       rp->Font->tf_Message.mn_Node.ln_Name);
                printf("MeasureText:   YSize = %d\n", rp->Font->tf_YSize);
                printf("MeasureText:   XSize = %d\n", rp->Font->tf_XSize);
                printf("MeasureText:   Baseline = %d\n", rp->Font->tf_Baseline);

                /* Display font info in window */
                SetAPen(rp, 1);
                Move(rp, 10, 90);
                Text(rp, "Font info above ^", 17);

                printf("MeasureText: Displayed (delay 2 sec)\n");
                Delay(100);  /* 2 second delay */

                CloseWindow(win);
                printf("MeasureText: Window closed\n");
            }
            else
            {
                printf("MeasureText: Failed to open window\n");
            }

            CloseLibrary(IntuitionBase);
        }
        else
        {
            printf("MeasureText: Failed to open intuition.library v37\n");
        }

        CloseLibrary(GfxBase);
    }
    else
    {
        printf("MeasureText: Failed to open graphics.library v37\n");
    }

    printf("MeasureText: Done\n");
}
