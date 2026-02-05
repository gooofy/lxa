/*
 * availfonts.c - Available Fonts Sample
 *
 * This is an RKM-style sample demonstrating font enumeration.
 * It uses AvailFonts() to list all available fonts.
 *
 * Key Functions Demonstrated:
 *   - AvailFonts() to get list of all fonts
 *   - OpenDiskFont() / CloseFont() to open fonts
 *   - TextLength() to measure text width
 *   - Font iteration and display
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <libraries/diskfont.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>

#include <stdio.h>
#include <string.h>

struct Library *DiskfontBase;
struct Library *IntuitionBase;
struct Library *GfxBase;

ULONG StrLen(UBYTE *string)
{
    ULONG x = 0;
    while (string[x++]);
    return(--x);
}

VOID main(int argc, char **argv)
{
    struct AvailFontsHeader *afh;
    struct AvailFonts *afont;
    struct TextFont *font;
    LONG afsize, afshortage;
    UWORD i;
    UWORD max_fonts = 10;  /* Limit output for testing */

    printf("AvailFonts: Starting font enumeration demonstration\n");

    if (DiskfontBase = OpenLibrary("diskfont.library", 37))
    {
        printf("AvailFonts: Opened diskfont.library\n");

        if (IntuitionBase = OpenLibrary("intuition.library", 37))
        {
            printf("AvailFonts: Opened intuition.library\n");

            if (GfxBase = OpenLibrary("graphics.library", 37))
            {
                printf("AvailFonts: Opened graphics.library\n");

                /* First call with size 0 to get required buffer size */
                afsize = AvailFonts(NULL, 0, AFF_MEMORY | AFF_DISK);
                printf("AvailFonts: Initial buffer size needed: %ld bytes\n", afsize);

                /* Allocate buffer and try to get font list */
                afh = NULL;
                do
                {
                    afh = (struct AvailFontsHeader *)AllocMem(afsize, MEMF_CLEAR);
                    if (afh)
                    {
                        afshortage = AvailFonts((STRPTR)afh, afsize, 
                                                AFF_MEMORY | AFF_DISK);
                        if (afshortage)
                        {
                            printf("AvailFonts: Buffer shortage: %ld bytes\n", afshortage);
                            FreeMem(afh, afsize);
                            afsize += afshortage;
                            afh = (struct AvailFontsHeader *)(-1L);
                        }
                    }
                } while (afshortage && afh);

                if (afh && afh != (struct AvailFontsHeader *)(-1L))
                {
                    printf("AvailFonts: Found %d font entries\n", afh->afh_NumEntries);

                    /* Point to first AvailFonts entry */
                    afont = (struct AvailFonts *)&(afh[1]);

                    /* List the fonts (limit to max_fonts for testing) */
                    for (i = 0; i < afh->afh_NumEntries && i < max_fonts; i++)
                    {
                        printf("AvailFonts: Font %d: '%s' size=%d type=%s\n",
                               i,
                               afont->af_Attr.ta_Name ? afont->af_Attr.ta_Name : "(null)",
                               afont->af_Attr.ta_YSize,
                               (afont->af_Type & AFF_MEMORY) ? "MEMORY" :
                               (afont->af_Type & AFF_DISK) ? "DISK" : "OTHER");

                        /* Try to open the font */
                        font = OpenDiskFont(&(afont->af_Attr));
                        if (font)
                        {
                            printf("AvailFonts:   -> OpenDiskFont() succeeded, actual size=%d\n",
                                   font->tf_YSize);
                            CloseFont(font);
                        }
                        else
                        {
                            printf("AvailFonts:   -> OpenDiskFont() failed\n");
                        }

                        afont++;
                    }

                    if (afh->afh_NumEntries > max_fonts)
                    {
                        printf("AvailFonts: (%d more fonts not shown)\n",
                               afh->afh_NumEntries - max_fonts);
                    }

                    FreeMem(afh, afsize);
                    printf("AvailFonts: Freed font buffer\n");
                }
                else
                {
                    printf("AvailFonts: Failed to allocate font buffer\n");
                }

                CloseLibrary(GfxBase);
            }
            else
            {
                printf("AvailFonts: Failed to open graphics.library v37\n");
            }
            CloseLibrary(IntuitionBase);
        }
        else
        {
            printf("AvailFonts: Failed to open intuition.library v37\n");
        }
        CloseLibrary(DiskfontBase);
    }
    else
    {
        printf("AvailFonts: Failed to open diskfont.library v37\n");
    }
    printf("AvailFonts: Done\n");
}
