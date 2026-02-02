#include <exec/types.h>
#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <intuition/preferences.h>

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>
#include <clib/layers_protos.h>
#include <inline/layers.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"

extern struct GfxBase *GfxBase;
extern struct Library *LayersBase;
extern struct UtilityBase *UtilityBase;

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "intuition"
#define EXLIBVER   " 40.1 (2022/03/21)"

char __aligned _g_intuition_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_intuition_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_intuition_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_intuition_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;

/* Forward declarations */
ULONG _intuition_OpenWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"));
struct Screen * _intuition_OpenScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                        register const struct NewScreen * newScreen __asm("a0"));

/* Forward declaration for internal helper */
static BOOL _post_idcmp_message(struct Window *window, ULONG class, UWORD code, 
                                 UWORD qualifier, APTR iaddress, WORD mouseX, WORD mouseY);

// libBase: IntuitionBase
// baseType: struct IntuitionBase *
// libname: intuition.library

struct IntuitionBase * __g_lxa_intuition_InitLib    ( register struct IntuitionBase *intuitionb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: WARNING: InitLib() unimplemented STUB called.\n");
    return intuitionb;
}

struct IntuitionBase * __g_lxa_intuition_OpenLib ( register struct IntuitionBase  *IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_intuition: OpenLib() called\n");
    // FIXME IntuitionBase->dl_lib.lib_OpenCnt++;
    // FIXME IntuitionBase->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return IntuitionBase;
}

BPTR __g_lxa_intuition_CloseLib ( register struct IntuitionBase  *intuitionb __asm("a6"))
{
    return NULL;
}

BPTR __g_lxa_intuition_ExpungeLib ( register struct IntuitionBase  *intuitionb      __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_intuition_ExtFuncLib(void)
{
    return NULL;
}

VOID _intuition_OpenIntuition ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: OpenIntuition() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_Intuition ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct InputEvent * iEvent __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: Intuition() unimplemented STUB called.\n");
    assert(FALSE);
}

UWORD _intuition_AddGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register UWORD position __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: AddGadget() window=0x%08lx gadget=0x%08lx pos=%d\n",
             (ULONG)window, (ULONG)gadget, position);

    if (!window || !gadget)
        return 0;

    /* Ensure gadget is not linked to another list */
    gadget->NextGadget = NULL;

    /* Count existing gadgets and find insertion point */
    UWORD count = 0;
    struct Gadget *prev = NULL;
    struct Gadget *curr = window->FirstGadget;
    
    while (curr && count < position) {
        count++;
        prev = curr;
        curr = curr->NextGadget;
    }
    
    /* Insert the gadget */
    if (!prev) {
        /* Insert at beginning */
        gadget->NextGadget = window->FirstGadget;
        window->FirstGadget = gadget;
    } else {
        /* Insert after prev */
        gadget->NextGadget = prev->NextGadget;
        prev->NextGadget = gadget;
    }
    
    /* Return the actual position where gadget was inserted */
    return count;
}

BOOL _intuition_ClearDMRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ClearDMRequest() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _intuition_ClearMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ClearMenuStrip() window=0x%08lx\n", (ULONG)window);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: ClearMenuStrip() called with NULL window\n");
        return;
    }

    window->MenuStrip = NULL;
}

VOID _intuition_ClearPointer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ClearPointer() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_CloseScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    ULONG display_handle;
    UBYTE i;

    DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() screen=0x%08lx\n", (ULONG)screen);

    if (!screen)
    {
        LPRINTF (LOG_ERROR, "_intuition: CloseScreen() called with NULL screen\n");
        return FALSE;
    }

    /* TODO: Check if any windows are still open on this screen */

    /* Get the display handle */
    display_handle = (ULONG)screen->ExtData;

    /* Close host display */
    if (display_handle)
    {
        emucall1(EMU_CALL_INT_CLOSE_SCREEN, display_handle);
    }

    /* Free bitplanes */
    for (i = 0; i < screen->BitMap.Depth; i++)
    {
        if (screen->BitMap.Planes[i])
        {
            FreeRaster(screen->BitMap.Planes[i], screen->Width, screen->Height);
        }
    }

    /* Unlink screen from IntuitionBase screen list */
    if (IntuitionBase->FirstScreen == screen)
    {
        IntuitionBase->FirstScreen = screen->NextScreen;
    }
    else
    {
        struct Screen *prev = IntuitionBase->FirstScreen;
        while (prev && prev->NextScreen != screen)
        {
            prev = prev->NextScreen;
        }
        if (prev)
        {
            prev->NextScreen = screen->NextScreen;
        }
    }
    
    /* Update active screen if necessary */
    if (IntuitionBase->ActiveScreen == screen)
    {
        IntuitionBase->ActiveScreen = IntuitionBase->FirstScreen;
    }

    /* Free the Screen structure */
    FreeMem(screen, sizeof(struct Screen));

    DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() done\n");

    return TRUE;
}

VOID _intuition_CloseWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() window=0x%08lx\n", (ULONG)window);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: CloseWindow() called with NULL window\n");
        return;
    }

    /* Phase 15: Close the rootless host window if one exists */
    if (window->UserData)
    {
        DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() closing rootless window_handle=0x%08lx\n",
                 (ULONG)window->UserData);
        emucall1(EMU_CALL_INT_CLOSE_WINDOW, (ULONG)window->UserData);
        window->UserData = NULL;
    }

    /* TODO: Reply to any pending IDCMP messages */

    /* Free the IDCMP message port if we created it */
    if (window->UserPort)
    {
        /* Remove and delete the port */
        /* Note: caller should have already drained the messages */
        DeleteMsgPort(window->UserPort);
        window->UserPort = NULL;
    }

    /* Free system gadgets we created */
    {
        struct Gadget *gad = window->FirstGadget;
        struct Gadget *next;
        while (gad)
        {
            next = gad->NextGadget;
            /* Only free gadgets we allocated (system gadgets) */
            if (gad->GadgetType & GTYP_SYSGADGET)
            {
                FreeMem(gad, sizeof(struct Gadget));
            }
            gad = next;
        }
        window->FirstGadget = NULL;
    }

    /* Unlink window from screen's window list */
    if (window->WScreen)
    {
        struct Window **wp = &window->WScreen->FirstWindow;
        while (*wp)
        {
            if (*wp == window)
            {
                *wp = window->NextWindow;
                break;
            }
            wp = &(*wp)->NextWindow;
        }
    }

    /* Free the Window structure */
    FreeMem(window, sizeof(struct Window));

    DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() done\n");
}

LONG _intuition_CloseWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: CloseWorkBench() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _intuition_CurrentTime ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG * seconds __asm("a0"),
                                                        register ULONG * micros __asm("a1"))
{
    struct timeval tv;
    emucall1(EMU_CALL_GETSYSTIME, (ULONG)&tv);
    
    if (seconds)
        *seconds = tv.tv_secs;
    if (micros)
        *micros = tv.tv_micro;
    
    DPRINTF(LOG_DEBUG, "_intuition: CurrentTime() -> seconds=%lu, micros=%lu\n",
            tv.tv_secs, tv.tv_micro);
}

BOOL _intuition_DisplayAlert ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG alertNumber __asm("d0"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register UWORD height __asm("d1"))
{
    /*
     * DisplayAlert() shows a hardware/software alert (Guru Meditation).
     * We log the alert and return TRUE (user pressed left mouse = continue).
     * The string format is: y_position, string_chars, continuation_byte...
     */
    DPRINTF (LOG_ERROR, "_intuition: DisplayAlert() alertNumber=0x%08lx string=0x%08lx height=%u\n",
             alertNumber, (ULONG)string, (unsigned)height);
    
    /* Parse the alert string if possible */
    if (string) {
        /* Skip y position byte, print the text */
        const char *p = (const char *)string;
        if (*p) {
            p++;  /* Skip y position */
            DPRINTF (LOG_ERROR, "_intuition: DisplayAlert() message: %s\n", p);
        }
    }
    
    /* Return TRUE = left mouse button (continue), FALSE = right mouse (reboot) */
    return TRUE;
}

VOID _intuition_DisplayBeep ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    /*
     * DisplayBeep() flashes the screen colors as an audio/visual alert.
     * We just log it as a no-op.
     */
    DPRINTF (LOG_DEBUG, "_intuition: DisplayBeep() screen=0x%08lx (no-op)\n", (ULONG)screen);
}

BOOL _intuition_DoubleClick ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG sSeconds __asm("d0"),
                                                        register ULONG sMicros __asm("d1"),
                                                        register ULONG cSeconds __asm("d2"),
                                                        register ULONG cMicros __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: DoubleClick() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _intuition_DrawBorder ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register const struct Border * border __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: DrawBorder() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_DrawImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: DrawImage() rp=0x%08lx image=0x%08lx at %d,%d\n",
             (ULONG)rp, (ULONG)image, (int)leftOffset, (int)topOffset);
    
    if (!rp || !image)
        return;
    
    /* Walk the Image chain and render each image */
    while (image)
    {
        WORD x = leftOffset + image->LeftEdge;
        WORD y = topOffset + image->TopEdge;
        
        DPRINTF (LOG_DEBUG, "_intuition: DrawImage() rendering image: w=%d h=%d depth=%d leftEdge=%d topEdge=%d -> x=%d y=%d\n",
                 (int)image->Width, (int)image->Height, (int)image->Depth,
                 (int)image->LeftEdge, (int)image->TopEdge, (int)x, (int)y);
        
        if (image->ImageData && image->Width > 0 && image->Height > 0)
        {
            /* Render image data using BltTemplate or pixel-by-pixel */
            /* For now, use a simple blit approach */
            
            UWORD wordsPerRow = (image->Width + 15) / 16;
            UBYTE planePick = image->PlanePick;
            UBYTE planeOnOff = image->PlaneOnOff;
            UWORD *data = image->ImageData;
            
            for (WORD py = 0; py < image->Height; py++)
            {
                for (WORD px = 0; px < image->Width; px++)
                {
                    UWORD wordIndex = px / 16;
                    UWORD bitIndex = 15 - (px % 16);
                    
                    /* Build the color from all planes */
                    UBYTE color = 0;
                    UWORD *planeData = data;
                    
                    for (WORD plane = 0; plane < image->Depth && plane < 8; plane++)
                    {
                        if (planePick & (1 << plane))
                        {
                            /* This plane has data */
                            UWORD word = planeData[py * wordsPerRow + wordIndex];
                            if (word & (1 << bitIndex))
                                color |= (1 << plane);
                            planeData += wordsPerRow * image->Height;
                        }
                        else
                        {
                            /* Use PlaneOnOff for this plane */
                            if (planeOnOff & (1 << plane))
                                color |= (1 << plane);
                        }
                    }
                    
                    /* Only draw non-zero pixels (or all if depth is 0) */
                    if (color != 0 || image->Depth == 0)
                    {
                        SetAPen(rp, color);
                        WritePixel(rp, x + px, y + py);
                    }
                }
            }
        }
        
        image = image->NextImage;
    }
}

VOID _intuition_EndRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"),
                                                        register struct Window * window __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: EndRequest() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Preferences * _intuition_GetDefPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Preferences * preferences __asm("a0"),
                                                        register WORD size __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetDefPrefs() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

struct Preferences * _intuition_GetPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Preferences * preferences __asm("a0"),
                                                        register WORD size __asm("d0"))
{
    /*
     * GetPrefs() copies the current Intuition preferences into the user's buffer.
     * We provide reasonable defaults for a standard PAL/NTSC Workbench setup.
     */
    DPRINTF (LOG_DEBUG, "_intuition: GetPrefs() preferences=0x%08lx size=%d\n", 
             (ULONG)preferences, (int)size);
    
    if (!preferences || size <= 0) {
        return NULL;
    }
    
    /* Clear the buffer first */
    char *p = (char *)preferences;
    for (int i = 0; i < size && i < (int)sizeof(struct Preferences); i++) {
        p[i] = 0;
    }
    
    /* Fill in sensible defaults - only fill what fits in the provided buffer */
    struct Preferences defaults;
    char *d = (char *)&defaults;
    for (int i = 0; i < (int)sizeof(defaults); i++) d[i] = 0;
    
    defaults.FontHeight = 8;          /* TOPAZ_EIGHTY - standard 80-col font */
    defaults.PrinterPort = 0;         /* PARALLEL_PRINTER */
    defaults.BaudRate = 5;            /* BAUD_9600 */
    
    /* Key repeat: ~500ms delay, ~100ms repeat */
    defaults.KeyRptDelay.tv_secs = 0;
    defaults.KeyRptDelay.tv_micro = 500000;
    defaults.KeyRptSpeed.tv_secs = 0;
    defaults.KeyRptSpeed.tv_micro = 100000;
    
    /* Double-click: ~500ms */
    defaults.DoubleClick.tv_secs = 0;
    defaults.DoubleClick.tv_micro = 500000;
    
    /* Pointer offsets */
    defaults.XOffset = -1;
    defaults.YOffset = -1;
    defaults.PointerTicks = 1;
    
    /* Workbench colors (standard 4-color palette) */
    defaults.color0 = 0x0AAA;         /* Grey background */
    defaults.color1 = 0x0000;         /* Black */
    defaults.color2 = 0x0FFF;         /* White */
    defaults.color3 = 0x068B;         /* Blue */
    
    /* Pointer colors */
    defaults.color17 = 0x0E44;        /* Orange-red */
    defaults.color18 = 0x0000;        /* Black */
    defaults.color19 = 0x0EEC;        /* Yellow */
    
    /* View offsets (PAL defaults) */
    defaults.ViewXOffset = 0;
    defaults.ViewYOffset = 0;
    defaults.ViewInitX = 0x0081;      /* Standard HIRES offset */
    defaults.ViewInitY = 0x002C;      /* Standard PAL offset */
    
    defaults.EnableCLI = TRUE | (1 << 14);  /* SCREEN_DRAG enabled */
    
    /* Printer defaults */
    defaults.PrinterType = 0x07;      /* EPSON */
    defaults.PrintPitch = 0;          /* PICA */
    defaults.PrintQuality = 0;        /* DRAFT */
    defaults.PrintSpacing = 0;        /* SIX_LPI */
    defaults.PrintLeftMargin = 5;
    defaults.PrintRightMargin = 75;
    defaults.PrintImage = 0;          /* IMAGE_POSITIVE */
    defaults.PrintAspect = 0;         /* ASPECT_HORIZ */
    defaults.PrintShade = 1;          /* SHADE_GREYSCALE */
    defaults.PrintThreshold = 7;
    
    /* Paper defaults */
    defaults.PaperSize = 0;           /* US_LETTER */
    defaults.PaperLength = 66;        /* 66 lines per page */
    defaults.PaperType = 0;           /* FANFOLD */
    
    /* Serial defaults: 8N1 */
    defaults.SerRWBits = 0;           /* 8 read, 8 write bits */
    defaults.SerStopBuf = 0x01;       /* 1 stop bit, 1024 buf */
    defaults.SerParShk = 0x02;        /* No parity, no handshake */
    
    defaults.LaceWB = 0;              /* No interlace */
    
    /* Copy to user buffer */
    int copy_size = size < (int)sizeof(struct Preferences) ? size : (int)sizeof(struct Preferences);
    for (int i = 0; i < copy_size; i++) {
        p[i] = d[i];
    }
    
    return preferences;
}

VOID _intuition_InitRequester ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: InitRequester() unimplemented STUB called.\n");
    assert(FALSE);
}

struct MenuItem * _intuition_ItemAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Menu * menuStrip __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    struct Menu *menu;
    struct MenuItem *item = NULL;
    WORD i;
    WORD menuNum;
    WORD itemNum;
    WORD subNum;

    DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() menuStrip=0x%08lx menuNumber=0x%04x\n",
             (ULONG)menuStrip, (UWORD)menuNumber);

    /* MENUNULL means no menu selected */
    if (menuNumber == MENUNULL)
    {
        return NULL;
    }

    if (!menuStrip)
    {
        LPRINTF (LOG_ERROR, "_intuition: ItemAddress() called with NULL menuStrip\n");
        return NULL;
    }

    /* Extract menu, item, and sub-item numbers from packed value */
    menuNum = MENUNUM(menuNumber);
    itemNum = ITEMNUM(menuNumber);
    subNum = SUBNUM(menuNumber);

    DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() menu=%d item=%d sub=%d\n",
             menuNum, itemNum, subNum);

    /* Navigate to the correct Menu */
    menu = (struct Menu *)menuStrip;
    for (i = 0; menu && i < menuNum; i++)
    {
        menu = menu->NextMenu;
    }

    if (!menu)
    {
        DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() menu not found\n");
        return NULL;
    }

    /* Navigate to the correct MenuItem */
    item = menu->FirstItem;
    for (i = 0; item && i < itemNum; i++)
    {
        item = item->NextItem;
    }

    if (!item)
    {
        DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() item not found\n");
        return NULL;
    }

    /* If there's a sub-item and it's specified, navigate to it */
    if (subNum != NOSUB && item->SubItem)
    {
        item = item->SubItem;
        for (i = 0; item && i < subNum; i++)
        {
            item = item->NextItem;
        }
    }

    DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() returning 0x%08lx\n", (ULONG)item);
    return item;
}

BOOL _intuition_ModifyIDCMP ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register ULONG flags __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ModifyIDCMP() window=0x%08lx flags=0x%08lx\n",
             (ULONG)window, (ULONG)flags);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: ModifyIDCMP() called with NULL window\n");
        return FALSE;
    }

    /* If flags are being cleared and we had IDCMP active, remove the port */
    if (flags == 0 && window->UserPort)
    {
        /* Drain any pending messages first */
        struct Message *msg;
        while ((msg = GetMsg(window->UserPort)) != NULL)
        {
            ReplyMsg(msg);
        }
        
        DeleteMsgPort(window->UserPort);
        window->UserPort = NULL;
    }
    /* If we're setting flags and didn't have a port, create one */
    else if (flags != 0 && !window->UserPort)
    {
        window->UserPort = CreateMsgPort();
        if (!window->UserPort)
        {
            LPRINTF (LOG_ERROR, "_intuition: ModifyIDCMP() failed to create port\n");
            return FALSE;
        }
    }

    /* Update the flags */
    window->IDCMPFlags = flags;

    return TRUE;
}

/*
 * Find the window at a given screen position
 * Returns the topmost window containing the point, or NULL if none
 */
static struct Window * _find_window_at_pos(struct Screen *screen, WORD x, WORD y)
{
    struct Window *window;
    struct Window *found = NULL;
    
    if (!screen)
        return NULL;
    
    /* Walk through windows - first window is frontmost due to our list order */
    for (window = screen->FirstWindow; window; window = window->NextWindow)
    {
        /* Check if point is inside window bounds */
        if (x >= window->LeftEdge && 
            x < window->LeftEdge + window->Width &&
            y >= window->TopEdge &&
            y < window->TopEdge + window->Height)
        {
            /* Found a window - since we iterate front-to-back, take the first match */
            if (!found)
                found = window;
        }
    }
    
    return found;
}

/*
 * Find a gadget at a given position within a window
 * Coordinates are window-relative
 * Returns the gadget or NULL if none found
 */
static struct Gadget * _find_gadget_at_pos(struct Window *window, WORD relX, WORD relY)
{
    struct Gadget *gad;
    WORD gx0, gy0, gx1, gy1;
    
    if (!window)
        return NULL;
    
    /* Walk through gadget list */
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        /* Skip disabled gadgets */
        if (gad->Flags & GFLG_DISABLED)
            continue;
        
        /* Calculate gadget bounds (handling relative positioning) */
        gx0 = gad->LeftEdge;
        gy0 = gad->TopEdge;
        gx1 = gx0 + gad->Width;
        gy1 = gy0 + gad->Height;
        
        /* Handle GFLG_REL* flags for relative positioning */
        if (gad->Flags & GFLG_RELRIGHT)
            gx0 += window->Width - 1;
        if (gad->Flags & GFLG_RELBOTTOM)
            gy0 += window->Height - 1;
        if (gad->Flags & GFLG_RELWIDTH)
            gx1 = gx0 + gad->Width + window->Width;
        if (gad->Flags & GFLG_RELHEIGHT)
            gy1 = gy0 + gad->Height + window->Height;
        
        /* Update end coordinates if relative flags changed start */
        if (gad->Flags & GFLG_RELRIGHT)
            gx1 = gx0 + gad->Width;
        if (gad->Flags & GFLG_RELBOTTOM)
            gy1 = gy0 + gad->Height;
        
        /* Check if point is inside gadget bounds */
        if (relX >= gx0 && relX < gx1 && relY >= gy0 && relY < gy1)
        {
            DPRINTF(LOG_DEBUG, "_intuition: _find_gadget_at_pos() hit gadget type=0x%04x at (%d,%d)\n",
                    gad->GadgetType, (int)relX, (int)relY);
            return gad;
        }
    }
    
    return NULL;
}

/*
 * Check if a point is inside a gadget (for RELVERIFY validation)
 * Used when mouse button is released to verify we're still inside the gadget
 */
static BOOL _point_in_gadget(struct Window *window, struct Gadget *gad, WORD relX, WORD relY)
{
    WORD gx0, gy0, gx1, gy1;
    
    if (!window || !gad)
        return FALSE;
    
    /* Calculate gadget bounds */
    gx0 = gad->LeftEdge;
    gy0 = gad->TopEdge;
    gx1 = gx0 + gad->Width;
    gy1 = gy0 + gad->Height;
    
    /* Handle GFLG_REL* flags */
    if (gad->Flags & GFLG_RELRIGHT)
        gx0 += window->Width - 1;
    if (gad->Flags & GFLG_RELBOTTOM)
        gy0 += window->Height - 1;
    if (gad->Flags & GFLG_RELWIDTH)
        gx1 = gx0 + gad->Width + window->Width;
    if (gad->Flags & GFLG_RELHEIGHT)
        gy1 = gy0 + gad->Height + window->Height;
    
    if (gad->Flags & GFLG_RELRIGHT)
        gx1 = gx0 + gad->Width;
    if (gad->Flags & GFLG_RELBOTTOM)
        gy1 = gy0 + gad->Height;
    
    return (relX >= gx0 && relX < gx1 && relY >= gy0 && relY < gy1);
}

/*
 * Handle system gadget action when mouse is released inside the gadget
 * This is called after RELVERIFY confirms the click
 */
static void _handle_sys_gadget_verify(struct Window *window, struct Gadget *gadget)
{
    UWORD sysType;
    
    if (!window || !gadget)
        return;
    
    /* Must be a system gadget */
    if (!(gadget->GadgetType & GTYP_SYSGADGET))
        return;
    
    sysType = gadget->GadgetType & GTYP_SYSTYPEMASK;
    
    DPRINTF(LOG_DEBUG, "_intuition: _handle_sys_gadget_verify() sysType=0x%04x\n", sysType);
    
    switch (sysType)
    {
        case GTYP_CLOSE:
            /* Fire IDCMP_CLOSEWINDOW message */
            DPRINTF(LOG_DEBUG, "_intuition: Close gadget clicked - posting IDCMP_CLOSEWINDOW\n");
            _post_idcmp_message(window, IDCMP_CLOSEWINDOW, 0, 0, window, 
                               window->MouseX, window->MouseY);
            break;
        
        case GTYP_WDEPTH:
            /* TODO: Implement WindowToBack/WindowToFront toggle */
            DPRINTF(LOG_DEBUG, "_intuition: Depth gadget clicked - TODO: window depth change\n");
            /* For now, post IDCMP_CHANGEWINDOW as a notification */
            _post_idcmp_message(window, IDCMP_CHANGEWINDOW, 0, 0, window,
                               window->MouseX, window->MouseY);
            break;
        
        case GTYP_WDRAGGING:
            /* Drag bar - handled separately during mouse move */
            break;
        
        case GTYP_SIZING:
            /* Resize gadget - handled separately during mouse move */
            break;
        
        case GTYP_WZOOM:
            /* Zoom gadget - TODO: implement ZipWindow behavior */
            DPRINTF(LOG_DEBUG, "_intuition: Zoom gadget clicked - TODO: ZipWindow\n");
            break;
    }
}

/* State for tracking active gadget during mouse press
 * Note: These MUST NOT have initializers - uninitialized statics go into .bss (RAM),
 * while initialized statics go into .data which is in ROM (read-only). */
static struct Gadget *g_active_gadget;
static struct Window *g_active_window;

/* State for menu bar handling
 * Note: These MUST NOT have initializers because initialized static data goes
 * into .data section which is placed in ROM (read-only). Uninitialized statics
 * go into .bss which is in RAM. We initialize them in _enter_menu_mode(). */
static BOOL g_menu_mode;                    /* TRUE when right mouse button held */
static struct Window *g_menu_window;        /* Window whose menu is being shown */
static struct Menu *g_active_menu;          /* Currently highlighted menu */
static struct MenuItem *g_active_item;      /* Currently highlighted item */
static UWORD g_menu_selection;              /* Encoded menu selection */

/* Menu bar saved screen area (for restoration) - TODO: implement proper saving
static PLANEPTR g_menu_save_buffer = NULL;
static WORD g_menu_save_x = 0;
static WORD g_menu_save_y = 0;
static WORD g_menu_save_width = 0;
static WORD g_menu_save_height = 0;
*/

/*
 * Encode menu selection into FULLMENUNUM format
 * menuNum: 0-31, itemNum: 0-63, subNum: 0-31
 */
static UWORD _encode_menu_selection(WORD menuNum, WORD itemNum, WORD subNum)
{
    if (menuNum == NOMENU || itemNum == NOITEM)
        return MENUNULL;
    
    UWORD code = (menuNum & 0x1F);              /* Bits 0-4: menu number */
    code |= ((itemNum & 0x3F) << 5);            /* Bits 5-10: item number */
    if (subNum != NOSUB)
        code |= ((subNum & 0x1F) << 11);        /* Bits 11-15: sub-item number */
    
    return code;
}

/*
 * Find which menu title is at a given screen X position
 * Returns the menu or NULL if no menu at that position
 */
static struct Menu * _find_menu_at_x(struct Window *window, WORD screenX)
{
    struct Menu *menu;
    struct Screen *screen;
    WORD barHBorder;
    
    if (!window || !window->MenuStrip || !window->WScreen)
        return NULL;
    
    screen = window->WScreen;
    barHBorder = screen->BarHBorder;
    
    for (menu = window->MenuStrip; menu; menu = menu->NextMenu)
    {
        /* Menu positions are stored relative to screen left edge */
        WORD menuLeft = barHBorder + menu->LeftEdge;
        WORD menuRight = menuLeft + menu->Width;
        
        if (screenX >= menuLeft && screenX < menuRight)
            return menu;
    }
    
    return NULL;
}

/*
 * Find which menu item is at a given position within the menu drop-down
 * x, y are relative to the menu drop-down origin
 */
static struct MenuItem * _find_item_at_pos(struct Menu *menu, WORD x, WORD y)
{
    struct MenuItem *item;
    
    if (!menu || !menu->FirstItem)
        return NULL;
    
    for (item = menu->FirstItem; item; item = item->NextItem)
    {
        /* Skip disabled items */
        if (!(item->Flags & ITEMENABLED))
            continue;
        
        if (x >= item->LeftEdge && x < item->LeftEdge + item->Width &&
            y >= item->TopEdge && y < item->TopEdge + item->Height)
        {
            return item;
        }
    }
    
    return NULL;
}

/*
 * Get the index of a menu in the menu strip (0-based)
 */
static WORD _get_menu_index(struct Window *window, struct Menu *targetMenu)
{
    struct Menu *menu;
    WORD index = 0;
    
    if (!window || !window->MenuStrip)
        return NOMENU;
    
    for (menu = window->MenuStrip; menu; menu = menu->NextMenu, index++)
    {
        if (menu == targetMenu)
            return index;
    }
    
    return NOMENU;
}

/*
 * Get the index of an item in a menu (0-based)
 */
static WORD _get_item_index(struct Menu *menu, struct MenuItem *targetItem)
{
    struct MenuItem *item;
    WORD index = 0;
    
    if (!menu || !menu->FirstItem)
        return NOITEM;
    
    for (item = menu->FirstItem; item; item = item->NextItem, index++)
    {
        if (item == targetItem)
            return index;
    }
    
    return NOITEM;
}

/*
 * Render the menu bar background on the screen's title bar area
 */
static void _render_menu_bar(struct Window *window)
{
    struct Screen *screen;
    struct RastPort *rp;
    struct Menu *menu;
    WORD barHeight, barHBorder, barVBorder;
    WORD x, y;
    
    if (!window || !window->WScreen)
        return;
    
    screen = window->WScreen;
    rp = &screen->RastPort;
    
    /* Validate RastPort has a valid BitMap */
    if (!rp->BitMap || !rp->BitMap->Planes[0])
    {
        DPRINTF(LOG_ERROR, "_intuition: _render_menu_bar() invalid RastPort BitMap\n");
        return;
    }
    
    DPRINTF(LOG_DEBUG, "_intuition: _render_menu_bar() screen=%08lx rp=%08lx bm=%08lx planes[0]=%08lx\n",
            (ULONG)screen, (ULONG)rp, (ULONG)rp->BitMap, (ULONG)rp->BitMap->Planes[0]);
    DPRINTF(LOG_DEBUG, "_intuition: _render_menu_bar() width=%d height=%d barHeight=%d\n",
            screen->Width, screen->Height, screen->BarHeight + 1);
    
    barHeight = screen->BarHeight + 1;  /* BarHeight is one less than actual */
    barHBorder = screen->BarHBorder;
    barVBorder = screen->BarVBorder;
    
    /* Fill menu bar background with pen 1 (standard Amiga look) */
    SetAPen(rp, 1);
    RectFill(rp, 0, 0, screen->Width - 1, barHeight - 1);
    
    /* Draw bottom border line */
    SetAPen(rp, 0);
    Move(rp, 0, barHeight - 1);
    Draw(rp, screen->Width - 1, barHeight - 1);
    
    /* Render menu titles */
    if (window->MenuStrip)
    {
        SetAPen(rp, 0);    /* Text in black */
        SetBPen(rp, 1);    /* Background */
        SetDrMd(rp, JAM2);
        
        for (menu = window->MenuStrip; menu; menu = menu->NextMenu)
        {
            if (!menu->MenuName)
                continue;
            
            x = barHBorder + menu->LeftEdge;
            y = barVBorder;
            
            /* Highlight active menu */
            if (menu == g_active_menu)
            {
                /* Draw highlighted background */
                SetAPen(rp, 0);
                RectFill(rp, x - 2, 0, x + menu->Width + 1, barHeight - 2);
                SetAPen(rp, 1);
                SetBPen(rp, 0);
            }
            else
            {
                SetAPen(rp, 0);
                SetBPen(rp, 1);
            }
            
            /* Draw menu title text */
            Move(rp, x, y + rp->TxBaseline);
            Text(rp, (STRPTR)menu->MenuName, strlen((const char *)menu->MenuName));
        }
    }
}

/*
 * Render the drop-down menu for the active menu
 */
static void _render_menu_items(struct Window *window)
{
    struct Screen *screen;
    struct RastPort *rp;
    struct Menu *menu;
    struct MenuItem *item;
    struct IntuiText *it;
    WORD barHeight;
    WORD menuX, menuY, menuWidth, menuHeight;
    WORD itemY;
    
    if (!window || !window->WScreen || !g_active_menu)
        return;
    
    screen = window->WScreen;
    rp = &screen->RastPort;
    
    /* Validate RastPort has a valid BitMap */
    if (!rp->BitMap || !rp->BitMap->Planes[0])
    {
        DPRINTF(LOG_ERROR, "_intuition: _render_menu_items() invalid RastPort BitMap\n");
        return;
    }
    
    menu = g_active_menu;
    
    barHeight = screen->BarHeight + 1;
    
    /* Calculate menu drop-down position and size */
    menuX = screen->BarHBorder + menu->LeftEdge;
    menuY = barHeight;
    
    /* Calculate menu width/height from items */
    menuWidth = 0;
    menuHeight = 0;
    for (item = menu->FirstItem; item; item = item->NextItem)
    {
        WORD w = item->LeftEdge + item->Width;
        WORD h = item->TopEdge + item->Height;
        if (w > menuWidth) menuWidth = w;
        if (h > menuHeight) menuHeight = h;
    }
    menuWidth += 8;  /* Add some padding */
    menuHeight += 4;
    
    /* Draw menu box background */
    SetAPen(rp, 1);  /* Background color */
    RectFill(rp, menuX, menuY, menuX + menuWidth - 1, menuY + menuHeight - 1);
    
    /* Draw menu box border (3D effect) */
    SetAPen(rp, 2);  /* White/shine */
    Move(rp, menuX, menuY + menuHeight - 2);
    Draw(rp, menuX, menuY);
    Draw(rp, menuX + menuWidth - 2, menuY);
    
    SetAPen(rp, 0);  /* Black/shadow */
    Move(rp, menuX + menuWidth - 1, menuY);
    Draw(rp, menuX + menuWidth - 1, menuY + menuHeight - 1);
    Draw(rp, menuX, menuY + menuHeight - 1);
    
    /* Render menu items */
    SetDrMd(rp, JAM2);
    
    for (item = menu->FirstItem; item; item = item->NextItem)
    {
        WORD itemX = menuX + item->LeftEdge + 4;
        itemY = menuY + item->TopEdge + 2;
        
        /* Highlight selected item */
        if (item == g_active_item && (item->Flags & ITEMENABLED))
        {
            SetAPen(rp, 0);  /* Highlighted background */
            RectFill(rp, menuX + 2, itemY - 1, 
                     menuX + menuWidth - 3, itemY + item->Height - 2);
            SetAPen(rp, 1);
            SetBPen(rp, 0);
        }
        else
        {
            SetAPen(rp, 0);
            SetBPen(rp, 1);
        }
        
        /* Draw item text (if IntuiText) */
        it = (struct IntuiText *)item->ItemFill;
        if (it && it->IText)
        {
            /* Disabled items shown in gray (pen 1 on pen 1 = gray effect) */
            if (!(item->Flags & ITEMENABLED))
            {
                SetAPen(rp, 2);  /* Gray text for disabled */
                SetBPen(rp, 1);
            }
            
            Move(rp, itemX + it->LeftEdge, itemY + it->TopEdge + rp->TxBaseline);
            Text(rp, (STRPTR)it->IText, strlen((const char *)it->IText));
            
            /* Draw checkmark if checked */
            if (item->Flags & CHECKED)
            {
                Move(rp, menuX + 3, itemY + rp->TxBaseline);
                Text(rp, (STRPTR)"\x9E", 1);  /* Amiga checkmark character */
            }
            
            /* Draw command key if present */
            if (item->Flags & COMMSEQ)
            {
                char cmdStr[4];
                cmdStr[0] = 'A';  /* Amiga key symbol (simplified) */
                cmdStr[1] = '-';
                cmdStr[2] = item->Command;
                cmdStr[3] = '\0';
                Move(rp, menuX + menuWidth - 40, itemY + rp->TxBaseline);
                Text(rp, (STRPTR)cmdStr, 3);
            }
        }
    }
}

/*
 * Enter menu mode - called on MENUDOWN
 */
static void _enter_menu_mode(struct Window *window, struct Screen *screen, WORD mouseX, WORD mouseY)
{
    if (!window || !window->MenuStrip || !screen)
        return;
    
    /* Check if window has WFLG_RMBTRAP - if so, don't enter menu mode */
    if (window->Flags & WFLG_RMBTRAP)
        return;
    
    g_menu_mode = TRUE;
    g_menu_window = window;
    g_active_menu = NULL;
    g_active_item = NULL;
    g_menu_selection = MENUNULL;
    
    DPRINTF(LOG_DEBUG, "_intuition: Entering menu mode for window 0x%08lx\n", (ULONG)window);
    
    /* Render the menu bar */
    _render_menu_bar(window);
    
    /* Check if mouse is already over a menu */
    if (mouseY < screen->BarHeight + 1)
    {
        struct Menu *menu = _find_menu_at_x(window, mouseX);
        if (menu && (menu->Flags & MENUENABLED))
        {
            g_active_menu = menu;
            _render_menu_bar(window);
            _render_menu_items(window);
        }
    }
}

/*
 * Exit menu mode - called on MENUUP
 */
static void _exit_menu_mode(struct Window *window, WORD mouseX, WORD mouseY)
{
    UWORD menuCode = MENUNULL;
    
    if (!g_menu_mode)
        return;
    
    DPRINTF(LOG_DEBUG, "_intuition: Exiting menu mode, active_menu=0x%08lx active_item=0x%08lx\n",
            (ULONG)g_active_menu, (ULONG)g_active_item);
    
    /* Calculate menu selection code if we have a valid selection */
    if (g_active_menu && g_active_item && (g_active_item->Flags & ITEMENABLED))
    {
        WORD menuNum = _get_menu_index(g_menu_window, g_active_menu);
        WORD itemNum = _get_item_index(g_active_menu, g_active_item);
        menuCode = _encode_menu_selection(menuNum, itemNum, NOSUB);
        
        DPRINTF(LOG_DEBUG, "_intuition: Menu selection: menu=%d item=%d code=0x%04x\n",
                (int)menuNum, (int)itemNum, menuCode);
    }
    
    /* Post IDCMP_MENUPICK if we have a valid selection */
    if (g_menu_window && menuCode != MENUNULL)
    {
        WORD relX = mouseX - g_menu_window->LeftEdge;
        WORD relY = mouseY - g_menu_window->TopEdge;
        _post_idcmp_message(g_menu_window, IDCMP_MENUPICK, menuCode,
                           IEQUALIFIER_RBUTTON, NULL, relX, relY);
    }
    
    /* Clear menu mode state */
    g_menu_mode = FALSE;
    g_menu_window = NULL;
    g_active_menu = NULL;
    g_active_item = NULL;
    g_menu_selection = MENUNULL;
    
    /* Redraw screen to clear menu bar */
    /* For now, we'll just let the next refresh handle it */
    /* A proper implementation would restore saved screen area */
}

/*
 * Internal function to post an IDCMP message to a window
 * Returns TRUE if message was posted, FALSE if window not interested
 */
static BOOL _post_idcmp_message(struct Window *window, ULONG class, UWORD code, 
                                 UWORD qualifier, APTR iaddress, WORD mouseX, WORD mouseY)
{
    struct IntuiMessage *imsg;
    
    if (!window || !window->UserPort) {
        return FALSE;
    }
    
    /* Check if window is interested in this message class */
    if (!(window->IDCMPFlags & class)) {
        return FALSE;
    }
    
    /* Allocate IntuiMessage */
    imsg = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_PUBLIC | MEMF_CLEAR);
    if (!imsg)
    {
        LPRINTF(LOG_ERROR, "_intuition: _post_idcmp_message() out of memory\n");
        return FALSE;
    }
    
    /* Fill in the message */
    imsg->ExecMessage.mn_Node.ln_Type = NT_MESSAGE;
    imsg->ExecMessage.mn_Length = sizeof(struct IntuiMessage);
    imsg->ExecMessage.mn_ReplyPort = NULL;  /* No reply expected for IDCMP */
    
    imsg->Class = class;
    imsg->Code = code;
    imsg->Qualifier = qualifier;
    imsg->IAddress = iaddress;
    imsg->MouseX = mouseX;
    imsg->MouseY = mouseY;
    imsg->IDCMPWindow = window;
    
    /* Get current time via emucall */
    struct timeval tv;
    emucall1(EMU_CALL_GETSYSTIME, (ULONG)&tv);
    imsg->Seconds = tv.tv_secs;
    imsg->Micros = tv.tv_micro;
    
    /* Update window's mouse position */
    window->MouseX = mouseX;
    window->MouseY = mouseY;
    
    /* Post the message to the window's UserPort */
    PutMsg(window->UserPort, (struct Message *)imsg);
    
    DPRINTF(LOG_DEBUG, "_intuition: Posted IDCMP 0x%08lx to window 0x%08lx\n",
            class, (ULONG)window);
    
    return TRUE;
}

/*
 * Internal function: Process pending input events from the host
 * This should be called periodically (e.g., from WaitTOF or the scheduler)
 * 
 * The function polls for SDL events via emucalls and posts appropriate
 * IDCMP messages to windows that have requested them.
 */
/* Prevent re-entry to event processing (e.g., VBlank firing during WaitTOF) */
static volatile BOOL g_processing_events;

VOID _intuition_ProcessInputEvents(struct Screen *screen)
{
    ULONG event_type;
    ULONG mouse_pos;
    ULONG button_code;
    ULONG key_data;
    WORD mouseX, mouseY;
    struct Window *window;
    struct IntuitionBase *IBase;
    
    if (!screen)
        return;
    
    /* Prevent re-entry - if already processing events, skip */
    if (g_processing_events)
        return;
    g_processing_events = TRUE;
    
    /* Get IntuitionBase for updating global state */
    IBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    
    /* Poll for input events */
    while (1)
    {
        event_type = emucall0(EMU_CALL_INT_POLL_INPUT);
        if (event_type == 0)
            break;  /* No more events */
        
        /* Get mouse position for all events */
        mouse_pos = emucall0(EMU_CALL_INT_GET_MOUSE_POS);
        mouseX = (WORD)(mouse_pos >> 16);
        mouseY = (WORD)(mouse_pos & 0xFFFF);
        
        /* Update IntuitionBase with current mouse position and timestamp */
        if (IBase)
        {
            struct timeval tv;
            emucall1(EMU_CALL_GETSYSTIME, (ULONG)&tv);
            IBase->MouseX = mouseX;
            IBase->MouseY = mouseY;
            IBase->Seconds = tv.tv_secs;
            IBase->Micros = tv.tv_micro;
        }
        
        /* Find the window at the mouse position */
        window = _find_window_at_pos(screen, mouseX, mouseY);
        
        /* If no window under mouse but we have an active gadget, use that window */
        if (!window && g_active_window)
            window = g_active_window;
        
        /* Fallback to first window if still none found */
        if (!window)
            window = screen->FirstWindow;
        
        switch (event_type)
        {
            case 1:  /* Mouse button */
            {
                button_code = emucall0(EMU_CALL_INT_GET_MOUSE_BTN);
                UWORD code = (UWORD)(button_code & 0xFF);
                UWORD qualifier = (UWORD)((button_code >> 8) & 0xFFFF);
                
                if (window)
                {
                    /* Convert to window-relative coordinates */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    
                    /* Check for gadget hit on mouse down (SELECTDOWN) */
                    if (code == SELECTDOWN)
                    {
                        struct Gadget *gad = _find_gadget_at_pos(window, relX, relY);
                        if (gad)
                        {
                            DPRINTF(LOG_DEBUG, "_intuition: SELECTDOWN on gadget type=0x%04x\n",
                                    gad->GadgetType);
                            
                            /* Set as active gadget for tracking */
                            g_active_gadget = gad;
                            g_active_window = window;
                            
                            /* Set gadget as selected */
                            gad->Flags |= GFLG_SELECTED;
                            
                            /* Post IDCMP_GADGETDOWN if it's a GADGIMMEDIATE gadget */
                            if (gad->Activation & GACT_IMMEDIATE)
                            {
                                _post_idcmp_message(window, IDCMP_GADGETDOWN, 0,
                                                   qualifier, gad, relX, relY);
                            }
                            
                            /* TODO: Render selected state (GADGHCOMP/GADGHBOX) */
                        }
                    }
                    /* Check for gadget release on mouse up (SELECTUP) */
                    else if (code == SELECTUP)
                    {
                        if (g_active_gadget && g_active_window)
                        {
                            struct Gadget *gad = g_active_gadget;
                            struct Window *activeWin = g_active_window;
                            
                            /* Calculate relative position in the active window */
                            WORD activeRelX = mouseX - activeWin->LeftEdge;
                            WORD activeRelY = mouseY - activeWin->TopEdge;
                            
                            /* Check if still inside gadget (RELVERIFY) */
                            BOOL inside = _point_in_gadget(activeWin, gad, activeRelX, activeRelY);
                            
                            DPRINTF(LOG_DEBUG, "_intuition: SELECTUP on gadget type=0x%04x inside=%d\n",
                                    gad->GadgetType, inside);
                            
                            /* Clear selected state */
                            gad->Flags &= ~GFLG_SELECTED;
                            
                            if (inside)
                            {
                                /* Handle system gadget verification */
                                if (gad->GadgetType & GTYP_SYSGADGET)
                                {
                                    _handle_sys_gadget_verify(activeWin, gad);
                                }
                                /* Post IDCMP_GADGETUP for RELVERIFY gadgets */
                                else if (gad->Activation & GACT_RELVERIFY)
                                {
                                    _post_idcmp_message(activeWin, IDCMP_GADGETUP, 0,
                                                       qualifier, gad, activeRelX, activeRelY);
                                }
                            }
                            
                            /* TODO: Render normal state */
                            
                            /* Clear active gadget */
                            g_active_gadget = NULL;
                            g_active_window = NULL;
                        }
                    }
                    /* Right mouse button press - enter menu mode */
                    else if (code == MENUDOWN)
                    {
                        /* Find window with menu at this position (check title bar) */
                        struct Window *menuWin = NULL;
                        
                        /* Check if we're in the screen's title bar area (where menus are displayed) */
                        if (mouseY < screen->BarHeight)
                        {
                            /* Find the active window with a menu strip */
                            menuWin = screen->FirstWindow;
                            while (menuWin)
                            {
                                if (menuWin->MenuStrip && (menuWin->Flags & WFLG_MENUSTATE) == 0)
                                    break;
                                menuWin = menuWin->NextWindow;
                            }
                        }
                        
                        if (menuWin && menuWin->MenuStrip)
                        {
                            _enter_menu_mode(menuWin, screen, mouseX, mouseY);
                        }
                    }
                    /* Right mouse button release - exit menu mode and select item */
                    else if (code == MENUUP)
                    {
                        if (g_menu_mode && g_menu_window)
                        {
                            _exit_menu_mode(g_menu_window, mouseX, mouseY);
                        }
                    }
                    
                    /* Post IDCMP_MOUSEBUTTONS for general notification */
                    _post_idcmp_message(window, IDCMP_MOUSEBUTTONS, code, 
                                       qualifier, NULL, relX, relY);
                }
                break;
            }
            
            case 2:  /* Mouse move */
            {
                key_data = emucall0(EMU_CALL_INT_GET_KEY);  /* Get qualifier */
                UWORD qualifier = (UWORD)(key_data >> 16);
                
                /* Handle menu tracking when in menu mode */
                if (g_menu_mode && g_menu_window)
                {
                    BOOL needRedraw = FALSE;
                    struct Menu *oldMenu = g_active_menu;
                    
                    /* Check if mouse is in menu bar area */
                    if (mouseY < screen->BarHeight + 1)
                    {
                        struct Menu *newMenu = _find_menu_at_x(g_menu_window, mouseX);
                        if (newMenu != g_active_menu)
                        {
                            g_active_menu = newMenu;
                            g_active_item = NULL;  /* Clear item when switching menus */
                            needRedraw = TRUE;
                        }
                    }
                    /* Check if mouse is in drop-down menu area */
                    else if (g_active_menu && g_active_menu->FirstItem)
                    {
                        /* Calculate position relative to menu drop-down */
                        WORD menuTop = screen->BarHeight + 1;
                        WORD menuLeft = screen->BarHBorder + g_active_menu->LeftEdge + g_active_menu->BeatX;
                        WORD itemX = mouseX - menuLeft;
                        WORD itemY = mouseY - menuTop;
                        
                        struct MenuItem *newItem = _find_item_at_pos(g_active_menu, itemX, itemY);
                        if (newItem != g_active_item)
                        {
                            g_active_item = newItem;
                            needRedraw = TRUE;
                        }
                    }
                    else
                    {
                        /* Mouse outside menu areas */
                        if (g_active_item)
                        {
                            g_active_item = NULL;
                            needRedraw = TRUE;
                        }
                    }
                    
                    /* Redraw if selection changed */
                    if (needRedraw)
                    {
                        if (oldMenu != g_active_menu)
                        {
                            _render_menu_bar(g_menu_window);
                        }
                        if (g_active_menu)
                        {
                            _render_menu_items(g_menu_window);
                        }
                    }
                }
                
                if (window)
                {
                    /* Update window mouse position */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    window->MouseX = relX;
                    window->MouseY = relY;
                    
                    /* Post IDCMP_MOUSEMOVE if requested */
                    _post_idcmp_message(window, IDCMP_MOUSEMOVE, 0, 
                                       qualifier, NULL, relX, relY);
                }
                break;
            }
            
            case 3:  /* Key */
            {
                key_data = emucall0(EMU_CALL_INT_GET_KEY);
                UWORD rawkey = (UWORD)(key_data & 0xFFFF);
                UWORD qualifier = (UWORD)(key_data >> 16);
                
                if (window)
                {
                    /* Convert to window-relative coordinates */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    _post_idcmp_message(window, IDCMP_RAWKEY, rawkey, 
                                       qualifier, NULL, relX, relY);
                }
                break;
            }
            
            case 4:  /* Close window request (from host window manager) */
            {
                if (window)
                {
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    _post_idcmp_message(window, IDCMP_CLOSEWINDOW, 0, 
                                       0, window, relX, relY);
                }
                break;
            }
            
            case 5:  /* Quit */
            {
                /* System quit requested - post close to all windows */
                for (window = screen->FirstWindow; window; window = window->NextWindow)
                {
                    _post_idcmp_message(window, IDCMP_CLOSEWINDOW, 0, 0, window, 0, 0);
                }
                break;
            }
        }
    }
    
    /* Close IntuitionBase if we opened it */
    if (IBase)
    {
        CloseLibrary((struct Library *)IBase);
    }
    
    /* Release re-entry guard */
    g_processing_events = FALSE;
}

/*
 * VBlank hook for input processing.
 * Called from the VBlank interrupt handler to ensure input events
 * are processed even when the app doesn't call WaitTOF().
 * 
 * This function must be called from a context where interrupts are safe
 * (e.g., at the end of VBlank processing before scheduling).
 */
VOID _intuition_VBlankInputHook(void)
{
    static int counter = 0;
    counter++;
    if (counter % 50 == 0)  /* Log every second (50Hz VBlank) */
    {
        DPRINTF(LOG_DEBUG, "_intuition: VBlankInputHook called (count=%d)\n", counter);
    }
    
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (IntuitionBase)
    {
        struct Screen *screen;
        for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
        {
            _intuition_ProcessInputEvents(screen);
        }
        CloseLibrary((struct Library *)IntuitionBase);
    }
}

/*
 * ReplyIntuiMsg - Reply to an IntuiMessage and free it
 * This is a convenience function for applications
 */
VOID _intuition_ReplyIntuiMsg(struct IntuiMessage *imsg)
{
    if (imsg)
    {
        /* Just free the message - we don't track them centrally */
        FreeMem(imsg, sizeof(struct IntuiMessage));
    }
}

VOID _intuition_ModifyProp ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register UWORD flags __asm("d0"),
                                                        register UWORD horizPot __asm("d1"),
                                                        register UWORD vertPot __asm("d2"),
                                                        register UWORD horizBody __asm("d3"),
                                                        register UWORD vertBody __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_intuition: ModifyProp() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_MoveScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: MoveScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_MoveWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: MoveWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_OffGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: OffGadget() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_OffMenu ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    /*
     * OffMenu() disables a menu, menu item, or sub-item.
     * The menuNumber encodes: menu (bits 0-4), item (bits 5-10), subitem (bits 11-15).
     * For now we just log it - menu rendering isn't implemented yet.
     */
    DPRINTF (LOG_DEBUG, "_intuition: OffMenu() window=0x%08lx menuNumber=0x%04x (no-op)\n",
             (ULONG)window, (unsigned)menuNumber);
    /* No-op - menu enable/disable is visual only */
}

VOID _intuition_OnGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: OnGadget() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_OnMenu ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    /*
     * OnMenu() enables a menu, menu item, or sub-item.
     * The menuNumber encodes: menu (bits 0-4), item (bits 5-10), subitem (bits 11-15).
     * For now we just log it - menu rendering isn't implemented yet.
     */
    DPRINTF (LOG_DEBUG, "_intuition: OnMenu() window=0x%08lx menuNumber=0x%04x (no-op)\n",
             (ULONG)window, (unsigned)menuNumber);
    /* No-op - menu enable/disable is visual only */
}

struct Screen * _intuition_OpenScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewScreen * newScreen __asm("a0"))
{
    struct Screen *screen;
    ULONG display_handle;
    UWORD width, height;
    UBYTE depth;
    UBYTE i;

    LPRINTF (LOG_INFO, "_intuition: OpenScreen() newScreen=0x%08lx\n", (ULONG)newScreen);

    if (!newScreen)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenScreen() called with NULL newScreen\n");
        return NULL;
    }

    /* Get screen dimensions */
    width = newScreen->Width;
    height = newScreen->Height;
    depth = (UBYTE)newScreen->Depth;

    /* Use defaults if width/height are 0 (means use display defaults) */
    if (width == 0 || width == (UWORD)-1)
        width = 640;
    if (height == 0 || height == (UWORD)-1)
        height = 256;
    if (depth == 0)
        depth = 2;

    DPRINTF (LOG_DEBUG, "_intuition: OpenScreen() %dx%dx%d\n",
             (int)width, (int)height, (int)depth);

    /* Allocate Screen structure */
    screen = (struct Screen *)AllocMem(sizeof(struct Screen), MEMF_PUBLIC | MEMF_CLEAR);
    if (!screen)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenScreen() out of memory for Screen\n");
        return NULL;
    }

    /* Open host display via emucall */
    /* Pack width/height into d1, depth into d2, title into d3 */
    display_handle = emucall3(EMU_CALL_INT_OPEN_SCREEN,
                              ((ULONG)width << 16) | (ULONG)height,
                              (ULONG)depth,
                              (ULONG)newScreen->DefaultTitle);

    if (display_handle == 0)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenScreen() host display_open failed\n");
        FreeMem(screen, sizeof(struct Screen));
        return NULL;
    }

    /* Store display handle in ExtData (we'll use this to identify the screen) */
    screen->ExtData = (UBYTE *)display_handle;

    /* Initialize screen fields */
    screen->LeftEdge = newScreen->LeftEdge;
    screen->TopEdge = newScreen->TopEdge;
    screen->Width = width;
    screen->Height = height;
    screen->Flags = newScreen->Type;
    screen->Title = newScreen->DefaultTitle;
    screen->DefaultTitle = newScreen->DefaultTitle;
    screen->DetailPen = newScreen->DetailPen;
    screen->BlockPen = newScreen->BlockPen;

    /* Initialize the embedded BitMap */
    InitBitMap(&screen->BitMap, depth, width, height);

    /* Allocate bitplanes */
    for (i = 0; i < depth; i++)
    {
        screen->BitMap.Planes[i] = AllocRaster(width, height);
        if (!screen->BitMap.Planes[i])
        {
            /* Cleanup on failure */
            LPRINTF (LOG_ERROR, "_intuition: OpenScreen() out of memory for plane %d\n", (int)i);
            while (i > 0)
            {
                i--;
                FreeRaster(screen->BitMap.Planes[i], width, height);
            }
            emucall1(EMU_CALL_INT_CLOSE_SCREEN, display_handle);
            FreeMem(screen, sizeof(struct Screen));
            return NULL;
        }
    }

    /* Initialize the embedded RastPort */
    InitRastPort(&screen->RastPort);
    screen->RastPort.BitMap = &screen->BitMap;

    /* Tell the host where the screen's bitmap lives so it can auto-refresh
     * from planar RAM at VBlank time.
     * Pack bpr and depth into single parameter: (bpr << 16) | depth
     */
    ULONG bpr_depth = ((ULONG)screen->BitMap.BytesPerRow << 16) | (ULONG)depth;
    emucall3(EMU_CALL_INT_SET_SCREEN_BITMAP, display_handle,
             (ULONG)&screen->BitMap.Planes[0], bpr_depth);

    /* Initialize ViewPort (minimal) */
    screen->ViewPort.DWidth = width;
    screen->ViewPort.DHeight = height;
    screen->ViewPort.RasInfo = NULL;

    /* Set bar heights (simplified) */
    screen->BarHeight = 10;
    screen->BarVBorder = 1;
    screen->BarHBorder = 5;
    screen->WBorTop = 11;
    screen->WBorLeft = 4;
    screen->WBorRight = 4;
    screen->WBorBottom = 2;

    /* Clear the screen to color 0 */
    SetRast(&screen->RastPort, 0);

    /* Initialize Layer_Info for this screen */
    InitLayers(&screen->LayerInfo);
    screen->LayerInfo.top_layer = NULL;

    /* Link screen into IntuitionBase screen list (at front) */
    screen->NextScreen = IntuitionBase->FirstScreen;
    IntuitionBase->FirstScreen = screen;
    
    /* If this is the first screen, make it the active screen */
    if (!IntuitionBase->ActiveScreen)
    {
        IntuitionBase->ActiveScreen = screen;
    }

    DPRINTF (LOG_DEBUG, "_intuition: OpenScreen() -> 0x%08lx, display_handle=0x%08lx\n",
             (ULONG)screen, display_handle);

    return screen;
}

/*
 * System Gadget constants
 * These define the standard sizes for Amiga window gadgets
 */
#define SYS_GADGET_WIDTH   18  /* Width of close/depth gadgets */
#define SYS_GADGET_HEIGHT  10  /* Height of system gadgets (based on title bar) */

/*
 * Create a system gadget for a window
 * Returns the allocated gadget or NULL on failure
 */
static struct Gadget * _create_sys_gadget(struct Window *window, UWORD type,
                                           WORD left, WORD top, WORD width, WORD height)
{
    struct Gadget *gad;
    
    gad = (struct Gadget *)AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad)
        return NULL;
    
    gad->LeftEdge = left;
    gad->TopEdge = top;
    gad->Width = width;
    gad->Height = height;
    gad->Flags = GFLG_GADGHCOMP;  /* Complement on select */
    gad->Activation = GACT_RELVERIFY | GACT_TOPBORDER;
    gad->GadgetType = GTYP_SYSGADGET | type | GTYP_BOOLGADGET;
    gad->GadgetRender = NULL;
    gad->SelectRender = NULL;
    gad->GadgetText = NULL;
    gad->SpecialInfo = NULL;
    gad->GadgetID = type;
    gad->UserData = (APTR)window;  /* Back-link to window */
    
    return gad;
}

/*
 * Create system gadgets for a window based on its flags
 * Links them into the window's gadget list
 */
static void _create_window_sys_gadgets(struct Window *window)
{
    struct Gadget *gad;
    struct Gadget **lastPtr = &window->FirstGadget;
    WORD gadWidth = SYS_GADGET_WIDTH;
    WORD gadHeight = window->BorderTop > 0 ? window->BorderTop - 1 : SYS_GADGET_HEIGHT;
    WORD rightX = window->Width - window->BorderRight - gadWidth;
    
    /* Skip to end of existing gadget list */
    while (*lastPtr)
        lastPtr = &(*lastPtr)->NextGadget;
    
    /* Create Close gadget (top-left) if requested */
    if (window->Flags & WFLG_CLOSEGADGET)
    {
        gad = _create_sys_gadget(window, GTYP_CLOSE, 
                                  window->BorderLeft, 0, 
                                  gadWidth, gadHeight);
        if (gad)
        {
            *lastPtr = gad;
            lastPtr = &gad->NextGadget;
            DPRINTF(LOG_DEBUG, "_intuition: Created CLOSE gadget at (%d,%d) %dx%d\n",
                    (int)gad->LeftEdge, (int)gad->TopEdge, (int)gad->Width, (int)gad->Height);
        }
    }
    
    /* Create Depth gadget (top-right) if requested */
    if (window->Flags & WFLG_DEPTHGADGET)
    {
        gad = _create_sys_gadget(window, GTYP_WDEPTH,
                                  rightX, 0,
                                  gadWidth, gadHeight);
        if (gad)
        {
            *lastPtr = gad;
            lastPtr = &gad->NextGadget;
            DPRINTF(LOG_DEBUG, "_intuition: Created DEPTH gadget at (%d,%d) %dx%d\n",
                    (int)gad->LeftEdge, (int)gad->TopEdge, (int)gad->Width, (int)gad->Height);
            rightX -= gadWidth;  /* Next gadget goes to the left */
        }
    }
    
    /* Drag bar is handled via WFLG_DRAGBAR flag, not as a separate gadget */
    /* It uses the entire title bar area not covered by other gadgets */
}

/*
 * Render system gadgets for a window
 * Draws the window frame, title bar, and gadget imagery
 */
static void _render_window_frame(struct Window *window)
{
    struct RastPort *rp;
    WORD x0, y0, x1, y1;
    WORD titleBarBottom;
    struct Gadget *gad;
    UBYTE detPen, blkPen, shiPen, shaPen;
    
    if (!window || !window->RPort)
        return;
    
    rp = window->RPort;
    
    /* Get pens - use window pens or defaults */
    detPen = window->DetailPen;
    blkPen = window->BlockPen;
    shiPen = 2;   /* Standard shine pen (white) */
    shaPen = 1;   /* Standard shadow pen (black) */
    
    /* Window interior bounds */
    x0 = 0;
    y0 = 0;
    x1 = window->Width - 1;
    y1 = window->Height - 1;
    titleBarBottom = window->BorderTop - 1;
    
    /* Skip if borderless */
    if (window->Flags & WFLG_BORDERLESS)
        return;
    
    /* Draw outer border (3D effect) */
    /* Top-left bright edge */
    SetAPen(rp, shiPen);
    Move(rp, x0, y1 - 1);
    Draw(rp, x0, y0);
    Draw(rp, x1 - 1, y0);
    
    /* Bottom-right dark edge */
    SetAPen(rp, shaPen);
    Move(rp, x1, y0 + 1);
    Draw(rp, x1, y1);
    Draw(rp, x0 + 1, y1);
    
    /* Fill title bar background if we have one */
    if (window->BorderTop > 1)
    {
        /* Fill title bar with block pen */
        SetAPen(rp, (window->Flags & WFLG_WINDOWACTIVE) ? blkPen : detPen);
        RectFill(rp, x0 + 1, y0 + 1, x1 - 1, titleBarBottom);
        
        /* Draw title bar bottom line */
        SetAPen(rp, shaPen);
        Move(rp, x0, titleBarBottom);
        Draw(rp, x1, titleBarBottom);
        
        /* Draw title text if present */
        if (window->Title)
        {
            WORD textX = window->BorderLeft;
            WORD textY = 1;  /* One pixel from top */
            
            /* Adjust for close gadget */
            if (window->Flags & WFLG_CLOSEGADGET)
                textX += SYS_GADGET_WIDTH + 2;
            
            /* Draw title */
            SetAPen(rp, (window->Flags & WFLG_WINDOWACTIVE) ? detPen : blkPen);
            SetBPen(rp, (window->Flags & WFLG_WINDOWACTIVE) ? blkPen : detPen);
            Move(rp, textX, textY + rp->TxBaseline);
            Text(rp, (STRPTR)window->Title, strlen((char *)window->Title));
        }
    }
    
    /* Render system gadgets */
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        UWORD sysType;
        WORD gx0, gy0, gx1, gy1;
        
        /* Skip non-system gadgets */
        if (!(gad->GadgetType & GTYP_SYSGADGET))
            continue;
        
        sysType = gad->GadgetType & GTYP_SYSTYPEMASK;
        gx0 = gad->LeftEdge;
        gy0 = gad->TopEdge;
        gx1 = gx0 + gad->Width - 1;
        gy1 = gy0 + gad->Height - 1;
        
        /* Draw gadget frame */
        SetAPen(rp, shiPen);
        Move(rp, gx0, gy1 - 1);
        Draw(rp, gx0, gy0);
        Draw(rp, gx1 - 1, gy0);
        SetAPen(rp, shaPen);
        Move(rp, gx1, gy0);
        Draw(rp, gx1, gy1);
        Draw(rp, gx0, gy1);
        
        /* Draw gadget-specific imagery */
        switch (sysType)
        {
            case GTYP_CLOSE:
            {
                /* Draw a small filled square in center (close icon) */
                WORD cx = (gx0 + gx1) / 2;
                WORD cy = (gy0 + gy1) / 2;
                SetAPen(rp, shaPen);
                RectFill(rp, cx - 2, cy - 2, cx + 2, cy + 2);
                SetAPen(rp, shiPen);
                RectFill(rp, cx - 1, cy - 1, cx + 1, cy + 1);
                break;
            }
            
            case GTYP_WDEPTH:
            {
                /* Draw overlapping rectangles (depth icon) */
                WORD cx = (gx0 + gx1) / 2;
                WORD cy = (gy0 + gy1) / 2;
                /* Back rectangle */
                SetAPen(rp, shaPen);
                Move(rp, cx - 3, cy + 2);
                Draw(rp, cx - 3, cy - 2);
                Draw(rp, cx + 1, cy - 2);
                Draw(rp, cx + 1, cy + 2);
                Draw(rp, cx - 3, cy + 2);
                /* Front rectangle */
                SetAPen(rp, shiPen);
                Move(rp, cx - 1, cy + 3);
                Draw(rp, cx - 1, cy);
                Draw(rp, cx + 3, cy);
                Draw(rp, cx + 3, cy + 3);
                Draw(rp, cx - 1, cy + 3);
                break;
            }
            
            case GTYP_WDRAGGING:
                /* Drag gadget has no special imagery - just the frame */
                break;
        }
    }
}

struct Window * _intuition_OpenWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"))
{
    struct Window *window;
    struct Screen *screen;
    WORD width, height;
    ULONG rootless_mode;

    LPRINTF (LOG_INFO, "_intuition: OpenWindow() newWindow=0x%08lx\n", (ULONG)newWindow);

    if (!newWindow)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenWindow() called with NULL newWindow\n");
        return NULL;
    }

    /* Debug dump of NewWindow structure for KP2 investigation */
    LPRINTF (LOG_INFO, "_intuition: OpenWindow() NewWindow dump:\n");
    U_hexdump(LOG_INFO, (void *)newWindow, 48);
    LPRINTF (LOG_INFO, "  LeftEdge=%d TopEdge=%d Width=%d Height=%d\n",
             (int)newWindow->LeftEdge, (int)newWindow->TopEdge,
             (int)newWindow->Width, (int)newWindow->Height);
    LPRINTF (LOG_INFO, "  DetailPen=%d BlockPen=%d\n",
             (int)newWindow->DetailPen, (int)newWindow->BlockPen);
    LPRINTF (LOG_INFO, "  IDCMPFlags=0x%08lx Flags=0x%08lx\n",
             (ULONG)newWindow->IDCMPFlags, (ULONG)newWindow->Flags);
    LPRINTF (LOG_INFO, "  FirstGadget=0x%08lx CheckMark=0x%08lx\n",
             (ULONG)newWindow->FirstGadget, (ULONG)newWindow->CheckMark);
    LPRINTF (LOG_INFO, "  Title=0x%08lx Screen=0x%08lx BitMap=0x%08lx\n",
             (ULONG)newWindow->Title, (ULONG)newWindow->Screen, (ULONG)newWindow->BitMap);
    LPRINTF (LOG_INFO, "  MinWidth=%d MinHeight=%d MaxWidth=%u MaxHeight=%u Type=%u\n",
             (int)newWindow->MinWidth, (int)newWindow->MinHeight,
             (unsigned)newWindow->MaxWidth, (unsigned)newWindow->MaxHeight,
             (unsigned)newWindow->Type);
    if (newWindow->Title)
    {
        LPRINTF (LOG_INFO, "  Title string: '%s'\n", (char *)newWindow->Title);
    }

    /* Get the target screen */
    screen = newWindow->Screen;
    if (!screen)
    {
        /* No screen specified - use the Workbench screen (open it if needed) */
        DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() no screen specified, using Workbench\n");
        
        /* Try to find existing Workbench screen */
        screen = IntuitionBase->FirstScreen;
        while (screen)
        {
            if (screen->Flags & WBENCHSCREEN)
                break;
            screen = screen->NextScreen;
        }
        
        /* If no Workbench screen, open one */
        if (!screen)
        {
            LPRINTF (LOG_INFO, "_intuition: OpenWindow() opening Workbench screen (called from OpenWindow)\n");
            if (!_intuition_OpenWorkBench(IntuitionBase))
            {
                LPRINTF (LOG_ERROR, "_intuition: OpenWindow() failed to open Workbench screen\n");
                return NULL;
            }
            /* Find the newly created Workbench screen */
            screen = IntuitionBase->FirstScreen;
            while (screen)
            {
                if (screen->Flags & WBENCHSCREEN)
                    break;
                screen = screen->NextScreen;
            }
        }
        
        if (!screen)
        {
            LPRINTF (LOG_ERROR, "_intuition: OpenWindow() Workbench screen not found after creation\n");
            return NULL;
        }
    }

    /* Calculate window dimensions */
    width = newWindow->Width;
    height = newWindow->Height;

    /* Apply defaults if dimensions are zero */
    if (width == 0)
        width = screen->Width - newWindow->LeftEdge;
    if (height == 0)
        height = screen->Height - newWindow->TopEdge;

    DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() %dx%d at (%d,%d) flags=0x%08lx\n",
             (int)width, (int)height, (int)newWindow->LeftEdge, (int)newWindow->TopEdge,
             (ULONG)newWindow->Flags);

    /* Allocate Window structure */
    window = (struct Window *)AllocMem(sizeof(struct Window), MEMF_PUBLIC | MEMF_CLEAR);
    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenWindow() out of memory for Window\n");
        return NULL;
    }

    /* Initialize window fields */
    window->LeftEdge = newWindow->LeftEdge;
    window->TopEdge = newWindow->TopEdge;
    window->Width = width;
    window->Height = height;
    window->MinWidth = newWindow->MinWidth ? newWindow->MinWidth : width;
    window->MinHeight = newWindow->MinHeight ? newWindow->MinHeight : height;
    window->MaxWidth = newWindow->MaxWidth ? newWindow->MaxWidth : (UWORD)-1;
    window->MaxHeight = newWindow->MaxHeight ? newWindow->MaxHeight : (UWORD)-1;
    window->Flags = newWindow->Flags;
    window->IDCMPFlags = newWindow->IDCMPFlags;
    window->Title = newWindow->Title;
    window->WScreen = screen;
    window->DetailPen = newWindow->DetailPen;
    window->BlockPen = newWindow->BlockPen;
    window->FirstGadget = newWindow->FirstGadget;
    window->CheckMark = newWindow->CheckMark;

    /* Set up border dimensions based on flags */
    if (newWindow->Flags & WFLG_BORDERLESS)
    {
        window->BorderLeft = 0;
        window->BorderTop = 0;
        window->BorderRight = 0;
        window->BorderBottom = 0;
    }
    else
    {
        /* Use the Screen's WBorXXX fields per NDK/AROS pattern */
        window->BorderLeft = screen->WBorLeft;
        window->BorderRight = screen->WBorRight;
        window->BorderBottom = screen->WBorBottom;
        
        /* Top border depends on whether we have a title bar */
        if ((newWindow->Flags & (WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_DEPTHGADGET)) || newWindow->Title)
        {
            /* Title bar: WBorTop + font height + 1 (per AROS pattern)
             * For now, use WBorTop which already includes space for title (11 pixels)
             * TODO: Use actual font height when fonts are supported:
             *   window->BorderTop = screen->WBorTop + GfxBase->DefaultFont->tf_YSize + 1;
             */
            window->BorderTop = screen->WBorTop;
        }
        else
        {
            /* No title bar, just use basic border */
            window->BorderTop = screen->WBorBottom;
        }
    }

    /* Create a Layer for this window so graphics operations use proper coordinate offsets */
    {
        struct Layer *layer;
        LONG x0 = newWindow->LeftEdge;
        LONG y0 = newWindow->TopEdge;
        LONG x1 = newWindow->LeftEdge + width - 1;
        LONG y1 = newWindow->TopEdge + height - 1;
        
        layer = CreateUpfrontLayer(&screen->LayerInfo, &screen->BitMap,
                                   x0, y0, x1, y1, LAYERSIMPLE, NULL);
        if (layer)
        {
            window->WLayer = layer;
            window->RPort = layer->rp;
            DPRINTF(LOG_DEBUG, "_intuition: OpenWindow() created layer=0x%08lx rp=0x%08lx bounds=[%ld,%ld]-[%ld,%ld]\n",
                    (ULONG)layer, (ULONG)layer->rp, x0, y0, x1, y1);
        }
        else
        {
            /* Fallback to screen's RastPort if layer creation fails */
            DPRINTF(LOG_WARNING, "_intuition: OpenWindow() layer creation failed, using screen RastPort\n");
            window->RPort = &screen->RastPort;
            window->WLayer = NULL;
        }
    }

    /* Check if rootless mode is enabled */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);

    if (rootless_mode)
    {
        /* Phase 15: Create a separate host window for this Amiga window */
        ULONG window_handle;
        ULONG screen_handle = (ULONG)screen->ExtData;

        window_handle = emucall4(EMU_CALL_INT_OPEN_WINDOW,
                                 screen_handle,
                                 ((ULONG)(WORD)newWindow->LeftEdge << 16) | ((ULONG)(WORD)newWindow->TopEdge & 0xFFFF),
                                 ((ULONG)width << 16) | ((ULONG)height & 0xFFFF),
                                 (ULONG)newWindow->Title);

        if (window_handle == 0)
        {
            LPRINTF (LOG_WARNING, "_intuition: OpenWindow() rootless window creation failed, continuing without\n");
        }

        /* Store the host window handle in UserData */
        window->UserData = (APTR)window_handle;

        DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() rootless window_handle=0x%08lx\n", window_handle);
    }
    else
    {
        window->UserData = NULL;
    }

    /* Create IDCMP message port if IDCMP flags are set */
    if (newWindow->IDCMPFlags)
    {
        window->UserPort = CreateMsgPort();
        if (!window->UserPort)
        {
            LPRINTF (LOG_ERROR, "_intuition: OpenWindow() failed to create IDCMP port\n");
            if (window->UserData)
            {
                emucall1(EMU_CALL_INT_CLOSE_WINDOW, (ULONG)window->UserData);
            }
            FreeMem(window, sizeof(struct Window));
            return NULL;
        }
    }

    /* Link window into screen's window list */
    window->NextWindow = screen->FirstWindow;
    screen->FirstWindow = window;

    /* Activate window if requested */
    if (newWindow->Flags & WFLG_ACTIVATE)
    {
        window->Flags |= WFLG_WINDOWACTIVE;
        /* TODO: Deactivate previous active window */
    }

    /* Create system gadgets based on window flags */
    _create_window_sys_gadgets(window);
    
    /* Render the window frame and gadgets (in non-rootless mode) */
    if (!rootless_mode)
    {
        _render_window_frame(window);
    }

    DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() -> 0x%08lx\n", (ULONG)window);

    return window;
}

ULONG _intuition_OpenWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *wbscreen;
    struct NewScreen ns;
    
    LPRINTF (LOG_INFO, "_intuition: OpenWorkBench() called, FirstScreen=0x%08lx\n", 
             (ULONG)IntuitionBase->FirstScreen);
    
    /* Check if Workbench screen already exists */
    wbscreen = IntuitionBase->FirstScreen;
    while (wbscreen)
    {
        if (wbscreen->Flags & WBENCHSCREEN)
        {
            DPRINTF (LOG_DEBUG, "_intuition: OpenWorkBench() - Workbench already open at 0x%08lx\n", (ULONG)wbscreen);
            return (ULONG)wbscreen;
        }
        wbscreen = wbscreen->NextScreen;
    }
    
    /* Create a new Workbench screen */
    memset(&ns, 0, sizeof(ns));
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 640;
    ns.Height = 256;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.Type = WBENCHSCREEN;
    ns.DefaultTitle = (UBYTE *)"Workbench Screen";
    
    wbscreen = _intuition_OpenScreen(IntuitionBase, &ns);
    
    if (wbscreen)
    {
        LPRINTF (LOG_INFO, "_intuition: OpenWorkBench() - opened at 0x%08lx, Width=%d Height=%d\n", 
                 (ULONG)wbscreen, (int)wbscreen->Width, (int)wbscreen->Height);
        LPRINTF (LOG_INFO, "_intuition: OpenWorkBench() FirstScreen=0x%08lx, FirstScreen->Width=%d Height=%d\n",
                 (ULONG)IntuitionBase->FirstScreen,
                 IntuitionBase->FirstScreen ? (int)IntuitionBase->FirstScreen->Width : -1,
                 IntuitionBase->FirstScreen ? (int)IntuitionBase->FirstScreen->Height : -1);
        /* Dump raw bytes at screen structure offsets 8-16 to verify layout */
        UBYTE *p = (UBYTE *)wbscreen;
        LPRINTF (LOG_INFO, "_intuition: Screen raw bytes at offset 8-15: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
        /* Also dump IntuitionBase offsets 56-64 to check FirstScreen pointer location */
        UBYTE *ib = (UBYTE *)IntuitionBase;
        LPRINTF (LOG_INFO, "_intuition: IntuitionBase raw at offset 56-63: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 ib[56], ib[57], ib[58], ib[59], ib[60], ib[61], ib[62], ib[63]);
        
        /* Print IntuitionBase address (what a6 should be after this call) */
        LPRINTF (LOG_INFO, "_intuition: OpenWorkBench() returning, IntuitionBase=0x%08lx\n", (ULONG)IntuitionBase);
        
        return (ULONG)wbscreen;
    }
    
    LPRINTF (LOG_ERROR, "_intuition: OpenWorkBench() failed to create screen\n");
    return 0;
}

VOID _intuition_PrintIText ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register const struct IntuiText * iText __asm("a1"),
                                                        register WORD left __asm("d0"),
                                                        register WORD top __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: PrintIText() rp=0x%08lx iText=0x%08lx at %d,%d\n",
             (ULONG)rp, (ULONG)iText, (int)left, (int)top);
    
    if (!rp || !iText)
        return;
    
    /* Walk the IntuiText chain and render each text */
    while (iText)
    {
        if (iText->IText)
        {
            BYTE oldAPen = rp->FgPen;
            BYTE oldBPen = rp->BgPen;
            BYTE oldDrMd = rp->DrawMode;
            
            /* Set colors and drawmode from IntuiText */
            SetAPen(rp, iText->FrontPen);
            SetBPen(rp, iText->BackPen);
            SetDrMd(rp, iText->DrawMode);
            
            /* Calculate position */
            WORD x = left + iText->LeftEdge;
            WORD y = top + iText->TopEdge;
            
            DPRINTF (LOG_DEBUG, "_intuition: PrintIText() text='%s' leftEdge=%d topEdge=%d -> x=%d y=%d\n",
                     (const char *)iText->IText, (int)iText->LeftEdge, (int)iText->TopEdge, (int)x, (int)y);
            
            /* If a font is specified, try to use it */
            /* For now, just use the rastport's current font */
            
            /* Move to position and render text */
            Move(rp, x, y + 6);  /* +6 for baseline adjustment with 8-pixel font */
            
            /* Calculate text length and render */
            STRPTR txt = iText->IText;
            UWORD len = 0;
            while (txt[len]) len++;
            
            if (len > 0)
            {
                Text(rp, txt, len);
            }
            
            /* Restore original colors/mode */
            SetAPen(rp, oldAPen);
            SetBPen(rp, oldBPen);
            SetDrMd(rp, oldDrMd);
        }
        
        iText = iText->NextText;
    }
}

VOID _intuition_RefreshGadgets ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_intuition: RefreshGadgets() stub - no-op for now\n");
    /* TODO: Implement gadget refresh when gadget system is implemented */
}

UWORD _intuition_RemoveGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: RemoveGadget() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _intuition_ReportMouse ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register BOOL flag __asm("d0"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ReportMouse() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_Request ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"),
                                                        register struct Window * window __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: Request() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _intuition_ScreenToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    /*
     * ScreenToBack() moves a screen to the back of the screen depth list.
     * In lxa with a single screen model, this is a no-op.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ScreenToBack() screen=0x%08lx (no-op)\n", (ULONG)screen);
    /* No-op: single screen model */
}

VOID _intuition_ScreenToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    /*
     * ScreenToFront() moves a screen to the front of the screen depth list.
     * In lxa with a single screen model, this is a no-op.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ScreenToFront() screen=0x%08lx (no-op)\n", (ULONG)screen);
    /* No-op: single screen model */
}

BOOL _intuition_SetDMRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Requester * requester __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetDMRequest() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _intuition_SetMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Menu * menu __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: SetMenuStrip() window=0x%08lx menu=0x%08lx\n",
             (ULONG)window, (ULONG)menu);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: SetMenuStrip() called with NULL window\n");
        return FALSE;
    }

    /* Store the menu pointer for the window
     * NOTE: Actual menu rendering/interaction not yet implemented
     */
    window->MenuStrip = menu;

    return TRUE;
}

VOID _intuition_SetPointer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD * pointer __asm("a1"),
                                                        register WORD height __asm("d0"),
                                                        register WORD width __asm("d1"),
                                                        register WORD xOffset __asm("d2"),
                                                        register WORD yOffset __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetPointer() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_SetWindowTitles ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register CONST_STRPTR windowTitle __asm("a1"),
                                                        register CONST_STRPTR screenTitle __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetWindowTitles() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ShowTitle ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register BOOL showIt __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ShowTitle() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_SizeWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SizeWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

struct View * _intuition_ViewAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: ViewAddress() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

struct ViewPort * _intuition_ViewPortAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Window * window __asm("a0"))
{
    /*
     * ViewPortAddress() returns a pointer to the ViewPort associated with
     * the window's screen. This is used for graphics functions that need
     * to work with the screen's color map and display settings.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ViewPortAddress() window=0x%08lx\n", (ULONG)window);
    
    if (!window || !window->WScreen) {
        DPRINTF (LOG_WARNING, "_intuition: ViewPortAddress() - invalid window or no screen\n");
        return NULL;
    }
    
    /* Return the ViewPort from the window's screen */
    return &window->WScreen->ViewPort;
}

VOID _intuition_WindowToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: WindowToBack() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_WindowToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: WindowToFront() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_WindowLimits ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register LONG widthMin __asm("d0"),
                                                        register LONG heightMin __asm("d1"),
                                                        register ULONG widthMax __asm("d2"),
                                                        register ULONG heightMax __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: WindowLimits() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

struct Preferences  * _intuition_SetPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Preferences * preferences __asm("a0"),
                                                        register LONG size __asm("d0"),
                                                        register BOOL inform __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetPrefs() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

LONG _intuition_IntuiTextLength ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct IntuiText * iText __asm("a0"))
{
    /*
     * IntuiTextLength() returns the pixel width of an IntuiText string.
     * This is used for layout calculations before rendering.
     */
    DPRINTF (LOG_DEBUG, "_intuition: IntuiTextLength() iText=0x%08lx\n", (ULONG)iText);
    
    if (!iText || !iText->IText) {
        return 0;
    }
    
    /* Count string length */
    const char *s = (const char *)iText->IText;
    int len = 0;
    while (s[len]) len++;
    
    /* Default to 8 pixels per char (Topaz 8) - ITextFont is a TextAttr, not TextFont */
    UWORD char_width = 8;
    
    /* Could look up font from TextAttr if needed, but for now use default */
    (void)iText->ITextFont;  /* Unused - would need to open font to get metrics */
    
    return (LONG)(len * char_width);
}

BOOL _intuition_WBenchToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_intuition: WBenchToBack() stub called - no-op.\n");
    return TRUE;
}

BOOL _intuition_WBenchToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *wbscreen;
    
    DPRINTF (LOG_DEBUG, "_intuition: WBenchToFront() called.\n");
    
    /* First, open the Workbench screen if it doesn't exist.
     * According to AmigaOS behavior, WBenchToFront should open the
     * Workbench if not already open (similar to OpenWorkBench but
     * also brings it to front).
     */
    wbscreen = IntuitionBase->FirstScreen;
    while (wbscreen)
    {
        if (wbscreen->Flags & WBENCHSCREEN)
            break;
        wbscreen = wbscreen->NextScreen;
    }
    
    if (!wbscreen)
    {
        /* Workbench not open - open it */
        DPRINTF (LOG_DEBUG, "_intuition: WBenchToFront() - Workbench not open, opening it.\n");
        if (!_intuition_OpenWorkBench(IntuitionBase))
        {
            DPRINTF (LOG_ERROR, "_intuition: WBenchToFront() - failed to open Workbench.\n");
            return FALSE;
        }
    }
    
    /* TODO: Bring Workbench to front (screen depth ordering) */
    /* For now, since we typically only have one screen, this is a no-op */
    
    return TRUE;
}

BOOL _intuition_AutoRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct IntuiText * body __asm("a1"),
                                                        register const struct IntuiText * posText __asm("a2"),
                                                        register const struct IntuiText * negText __asm("a3"),
                                                        register ULONG pFlag __asm("d0"),
                                                        register ULONG nFlag __asm("d1"),
                                                        register UWORD width __asm("d2"),
                                                        register UWORD height __asm("d3"))
{
    /* AutoRequest() displays a modal requester with Yes/No buttons.
     * For now, we just log the request and return TRUE (positive response).
     * A full implementation would create a window and handle user input. */

    DPRINTF (LOG_DEBUG, "_intuition: AutoRequest() window=0x%08lx pFlag=0x%08lx nFlag=0x%08lx %dx%d\n",
             (ULONG)window, pFlag, nFlag, (int)width, (int)height);

    /* Print the body text chain to debug output */
    const struct IntuiText *it = body;
    while (it)
    {
        if (it->IText)
        {
            DPRINTF (LOG_DEBUG, "_intuition: AutoRequest body: %s\n", (char *)it->IText);
        }
        it = it->NextText;
    }

    /* Print button texts if available */
    if (posText && posText->IText)
    {
        DPRINTF (LOG_DEBUG, "_intuition: AutoRequest positive: %s\n", (char *)posText->IText);
    }
    if (negText && negText->IText)
    {
        DPRINTF (LOG_DEBUG, "_intuition: AutoRequest negative: %s\n", (char *)negText->IText);
    }

    /* Return TRUE (positive response) by default */
    return TRUE;
}

/*
 * BeginRefresh - Begin refresh cycle for a WFLG_SIMPLE_REFRESH window
 *
 * This is called in response to an IDCMP_REFRESHWINDOW message.
 * It sets up the layer for optimized refresh drawing (only damaged areas).
 */
VOID _intuition_BeginRefresh ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                               register struct Window * window __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_intuition: BeginRefresh() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return;

    struct Layer *layer = window->WLayer;
    if (!layer)
    {
        DPRINTF(LOG_DEBUG, "_intuition: BeginRefresh() window has no layer\n");
        return;
    }

    /* Call the layers.library BeginUpdate which sets up clipping
     * to only allow drawing in damaged regions */
    BeginUpdate(layer);

    /* Optionally: Clear the damaged areas to the background color
     * For WFLG_SIMPLE_REFRESH windows, the app is responsible for redrawing */
}

struct Window * _intuition_BuildSysRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct IntuiText * body __asm("a1"),
                                                        register const struct IntuiText * posText __asm("a2"),
                                                        register const struct IntuiText * negText __asm("a3"),
                                                        register ULONG flags __asm("d0"),
                                                        register UWORD width __asm("d1"),
                                                        register UWORD height __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_intuition: BuildSysRequest() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

/*
 * EndRefresh - End refresh cycle for a WFLG_SIMPLE_REFRESH window
 *
 * This is called after the application has redrawn damaged areas.
 * If 'complete' is TRUE, the damage list is cleared. Otherwise,
 * more IDCMP_REFRESHWINDOW messages may follow.
 */
VOID _intuition_EndRefresh ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                             register struct Window * window __asm("a0"),
                             register LONG complete __asm("d0"))
{
    DPRINTF(LOG_DEBUG, "_intuition: EndRefresh() window=0x%08lx complete=%ld\n",
            (ULONG)window, complete);

    if (!window)
        return;

    struct Layer *layer = window->WLayer;
    if (!layer)
    {
        DPRINTF(LOG_DEBUG, "_intuition: EndRefresh() window has no layer\n");
        return;
    }

    /* Call the layers.library EndUpdate to restore normal clipping.
     * If complete is TRUE (non-zero), clear the damage list. */
    EndUpdate(layer, complete ? TRUE : FALSE);

    /* Clear the LAYERREFRESH flag if we're done */
    if (complete && layer)
    {
        layer->Flags &= ~LAYERREFRESH;
    }
}

VOID _intuition_FreeSysRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: FreeSysRequest() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_MakeScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: MakeScreen() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _intuition_RemakeDisplay ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: RemakeDisplay() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _intuition_RethinkDisplay ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: RethinkDisplay() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

APTR _intuition_AllocRemember ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Remember ** rememberKey __asm("a0"),
                                                        register ULONG size __asm("d0"),
                                                        register ULONG flags __asm("d1"))
{
    struct Remember *rem;
    APTR mem;
    
    DPRINTF (LOG_DEBUG, "_intuition: AllocRemember() rememberKey=0x%08lx, size=%ld, flags=0x%08lx\n",
             (ULONG)rememberKey, size, flags);
    
    if (!rememberKey)
        return NULL;
    
    /* Allocate the memory */
    mem = AllocMem(size, flags);
    if (!mem)
        return NULL;
    
    /* Allocate a Remember node to track this allocation */
    rem = AllocMem(sizeof(struct Remember), MEMF_CLEAR | MEMF_PUBLIC);
    if (!rem) {
        FreeMem(mem, size);
        return NULL;
    }
    
    /* Set up the Remember node */
    rem->Memory = mem;
    rem->RememberSize = size;
    
    /* Link it into the list */
    rem->NextRemember = *rememberKey;
    *rememberKey = rem;
    
    DPRINTF (LOG_DEBUG, "_intuition: AllocRemember() -> mem=0x%08lx\n", (ULONG)mem);
    
    return mem;
}

VOID _intuition_private0 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private0() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_FreeRemember ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Remember ** rememberKey __asm("a0"),
                                                        register BOOL reallyForget __asm("d0"))
{
    struct Remember *rem, *next;
    
    DPRINTF (LOG_DEBUG, "_intuition: FreeRemember() rememberKey=0x%08lx, reallyForget=%d\n",
             (ULONG)rememberKey, reallyForget);
    
    if (!rememberKey)
        return;
    
    rem = *rememberKey;
    
    while (rem) {
        next = rem->NextRemember;
        
        if (reallyForget && rem->Memory) {
            /* Free the allocated memory */
            FreeMem(rem->Memory, rem->RememberSize);
        }
        
        /* Free the Remember node itself */
        FreeMem(rem, sizeof(struct Remember));
        
        rem = next;
    }
    
    *rememberKey = NULL;
}

ULONG _intuition_LockIBase ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG dontknow __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: LockIBase() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _intuition_UnlockIBase ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG ibLock __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: UnlockIBase() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_GetScreenData ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR buffer __asm("a0"),
                                                        register UWORD size __asm("d0"),
                                                        register UWORD type __asm("d1"),
                                                        register const struct Screen * screen __asm("a1"))
{
    /*
     * GetScreenData() copies data from a screen into a buffer.
     * If 'screen' is NULL, it uses the default screen based on 'type'.
     * type: WBENCHSCREEN (1) = Workbench screen, CUSTOMSCREEN (15) = specific screen.
     * Returns TRUE on success, FALSE on failure.
     */
    DPRINTF (LOG_DEBUG, "_intuition: GetScreenData() buffer=0x%08lx size=%u type=%u screen=0x%08lx\n",
             (ULONG)buffer, (unsigned)size, (unsigned)type, (ULONG)screen);
    
    if (!buffer || size == 0) {
        return FALSE;
    }
    
    const struct Screen *src = screen;
    
    /* If screen is NULL, get screen based on type */
    if (!src) {
        if (type == 1) {  /* WBENCHSCREEN */
            /* Use FirstScreen if available, otherwise let app open its own */
            src = IntuitionBase->FirstScreen;
        }
    }
    
    if (!src) {
        DPRINTF (LOG_WARNING, "_intuition: GetScreenData() - no screen available\n");
        return FALSE;
    }
    
    /* Copy screen data to buffer (limited by size) */
    UWORD copy_size = size < sizeof(struct Screen) ? size : sizeof(struct Screen);
    const char *sp = (const char *)src;
    char *dp = (char *)buffer;
    for (UWORD i = 0; i < copy_size; i++) {
        dp[i] = sp[i];
    }
    
    return TRUE;
}

VOID _intuition_RefreshGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register WORD numGad __asm("d0"))
{
    /*
     * RefreshGList() redraws a list of gadgets.
     * For now, this is a stub - gadget rendering needs proper implementation.
     * TODO: Implement gadget rendering
     */
    DPRINTF (LOG_DEBUG, "_intuition: RefreshGList() gadgets=0x%08lx window=0x%08lx req=0x%08lx numGad=%d (stub)\n",
             (ULONG)gadgets, (ULONG)window, (ULONG)requester, numGad);
    /* TODO: Actually render the gadgets */
}

UWORD _intuition_AddGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register UWORD position __asm("d0"),
                                                        register WORD numGad __asm("d1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_intuition: AddGList() window=0x%08lx gadget=0x%08lx pos=%d numGad=%d req=0x%08lx\n",
             (ULONG)window, (ULONG)gadget, position, numGad, (ULONG)requester);

    if (!window || !gadget)
        return 0;

    /* Count the gadgets to add (or use numGad if specified) */
    WORD count = 0;
    struct Gadget *gad = gadget;
    struct Gadget *last = gadget;
    
    while (gad && (numGad == -1 || count < numGad)) {
        count++;
        last = gad;
        gad = gad->NextGadget;
    }
    
    /* Insert at the specified position in window's gadget list */
    if (position == 0 || position == (UWORD)~0 || !window->FirstGadget) {
        /* Insert at beginning or first position */
        last->NextGadget = window->FirstGadget;
        window->FirstGadget = gadget;
    } else {
        /* Find the insertion point */
        struct Gadget *prev = window->FirstGadget;
        UWORD pos = 1;
        
        while (prev->NextGadget && pos < position) {
            prev = prev->NextGadget;
            pos++;
        }
        
        /* Insert after prev */
        last->NextGadget = prev->NextGadget;
        prev->NextGadget = gadget;
    }
    
    return (UWORD)count;
}

UWORD _intuition_RemoveGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * remPtr __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register WORD numGad __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: RemoveGList() window=0x%08lx gadget=0x%08lx numGad=%d\n",
             (ULONG)remPtr, (ULONG)gadget, numGad);

    if (!remPtr || !gadget)
        return 0;

    /* Find the gadget in the window's list */
    struct Gadget *prev = NULL;
    struct Gadget *curr = remPtr->FirstGadget;
    
    while (curr && curr != gadget) {
        prev = curr;
        curr = curr->NextGadget;
    }
    
    if (!curr)
        return 0;  /* Not found */

    /* Count gadgets to remove */
    UWORD count = 0;
    struct Gadget *last = gadget;
    
    while (last && (numGad == -1 || count < (UWORD)numGad)) {
        count++;
        if (!last->NextGadget || (numGad != -1 && count >= (UWORD)numGad))
            break;
        last = last->NextGadget;
    }
    
    /* Remove the gadgets from the list */
    if (prev)
        prev->NextGadget = last->NextGadget;
    else
        remPtr->FirstGadget = last->NextGadget;
    
    return count;
}

VOID _intuition_ActivateWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ActivateWindow() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return;

    /* Set this window as the active window */
    IntuitionBase->ActiveWindow = window;
    
    /* Set the WFLG_WINDOWACTIVE flag */
    window->Flags |= WFLG_WINDOWACTIVE;
}

VOID _intuition_RefreshWindowFrame ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: RefreshWindowFrame() window=0x%08lx\n", (ULONG)window);
    
    if (window)
    {
        _render_window_frame(window);
    }
}

BOOL _intuition_ActivateGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: ActivateGadget() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _intuition_NewModifyProp ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register UWORD flags __asm("d0"),
                                                        register UWORD horizPot __asm("d1"),
                                                        register UWORD vertPot __asm("d2"),
                                                        register UWORD horizBody __asm("d3"),
                                                        register UWORD vertBody __asm("d4"),
                                                        register WORD numGad __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_intuition: NewModifyProp() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_QueryOverscan ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG displayID __asm("a0"),
                                                        register struct Rectangle * rect __asm("a1"),
                                                        register WORD oScanType __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: QueryOverscan() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _intuition_MoveWindowInFrontOf ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Window * behindWindow __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: MoveWindowInFrontOf() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ChangeWindowBox ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD left __asm("d0"),
                                                        register WORD top __asm("d1"),
                                                        register WORD width __asm("d2"),
                                                        register WORD height __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: ChangeWindowBox() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Hook * _intuition_SetEditHook ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Hook * hook __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetEditHook() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

LONG _intuition_SetMouseQueue ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD queueLength __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetMouseQueue() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _intuition_ZipWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ZipWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Screen * _intuition_LockPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"))
{
    struct Screen *screen;
    
    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() name='%s'\n", name ? (char *)name : "(null/default)");
    
    /* If name is NULL or "Workbench", return the default public screen (FirstScreen) */
    if (!name || strcmp((const char *)name, "Workbench") == 0)
    {
        screen = IntuitionBase->FirstScreen;
        if (screen)
        {
            /* Increment a visitor count (we don't actually track this, but apps expect it to work) */
            DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() returning FirstScreen=0x%08lx\n", (ULONG)screen);
            return screen;
        }
    }
    
    /* Named public screens not supported yet */
    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() screen not found\n");
    return NULL;
}

VOID _intuition_UnlockPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"),
                                                        register struct Screen * screen __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: UnlockPubScreen() name='%s', screen=0x%08lx\n",
             name ? (char *)name : "(null)", (ULONG)screen);
    /* In a full implementation, we'd decrement a visitor count */
    /* For now, this is a no-op since we don't track screen locks */
}

struct List * _intuition_LockPubScreenList ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: LockPubScreenList() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _intuition_UnlockPubScreenList ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: UnlockPubScreenList() unimplemented STUB called.\n");
    assert(FALSE);
}

STRPTR _intuition_NextPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Screen * screen __asm("a0"),
                                                        register STRPTR namebuf __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: NextPubScreen() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _intuition_SetDefaultPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetDefaultPubScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

UWORD _intuition_SetPubScreenModes ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register UWORD modes __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetPubScreenModes() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

UWORD _intuition_PubScreenStatus ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register UWORD statusFlags __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: PubScreenStatus() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

struct RastPort	* _intuition_ObtainGIRPort ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct GadgetInfo * gInfo __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ObtainGIRPort() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _intuition_ReleaseGIRPort ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ReleaseGIRPort() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_GadgetMouse ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct GadgetInfo * gInfo __asm("a1"),
                                                        register WORD * mousePoint __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: GadgetMouse() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private1 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private1() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_GetDefaultPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register STRPTR nameBuffer __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetDefaultPubScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_EasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct EasyStruct * easyStruct __asm("a1"),
                                                        register ULONG * idcmpPtr __asm("a2"),
                                                        register const APTR args __asm("a3"))
{
    /* EasyRequestArgs() displays a requester with formatted text.
     * For now, we log the request and return 1 (first/rightmost gadget).
     * A full implementation would create a window and handle user input.
     *
     * Return values:
     *  0 = rightmost gadget (usually negative/cancel)
     *  1 = leftmost gadget (usually positive/ok)
     *  n = nth gadget from the right
     * -1 = IDCMP message received (if idcmpPtr != NULL)
     */

    DPRINTF (LOG_DEBUG, "_intuition: EasyRequestArgs() window=0x%08lx easyStruct=0x%08lx\n",
             (ULONG)window, (ULONG)easyStruct);

    if (easyStruct)
    {
        if (easyStruct->es_Title)
        {
            DPRINTF (LOG_DEBUG, "_intuition: EasyRequest title: %s\n", (char *)easyStruct->es_Title);
        }
        if (easyStruct->es_TextFormat)
        {
            DPRINTF (LOG_DEBUG, "_intuition: EasyRequest text: %s\n", (char *)easyStruct->es_TextFormat);
        }
        if (easyStruct->es_GadgetFormat)
        {
            DPRINTF (LOG_DEBUG, "_intuition: EasyRequest gadgets: %s\n", (char *)easyStruct->es_GadgetFormat);
        }
    }

    /* Return 1 (positive/first gadget response) by default */
    return 1;
}

struct Window * _intuition_BuildEasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct EasyStruct * easyStruct __asm("a1"),
                                                        register ULONG idcmp __asm("d0"),
                                                        register const APTR args __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_intuition: BuildEasyRequestArgs() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

LONG _intuition_SysReqHandler ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register ULONG * idcmpPtr __asm("a1"),
                                                        register BOOL waitInput __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SysReqHandler() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

struct Window * _intuition_OpenWindowTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    struct NewWindow nw;
    struct TagItem *tstate;
    struct TagItem *tag;
    
    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() called, newWindow=0x%08lx, tagList=0x%08lx\n",
            (ULONG)newWindow, (ULONG)tagList);
    
    /* Start with defaults from NewWindow if provided, else use sensible defaults */
    if (newWindow)
    {
        /* Copy the NewWindow structure */
        nw = *newWindow;
    }
    else
    {
        /* Initialize with defaults */
        memset(&nw, 0, sizeof(nw));
        nw.Width = 200;
        nw.Height = 100;
        nw.DetailPen = 0;
        nw.BlockPen = 1;
        nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET;
        nw.Type = WBENCHSCREEN;  /* Default to opening on Workbench */
    }
    
    /* Process tags to override NewWindow fields */
    if (tagList)
    {
        tstate = (struct TagItem *)tagList;
        while ((tag = NextTagItem(&tstate)))
        {
            switch (tag->ti_Tag)
            {
                case WA_Left:
                    nw.LeftEdge = (WORD)tag->ti_Data;
                    break;
                case WA_Top:
                    nw.TopEdge = (WORD)tag->ti_Data;
                    break;
                case WA_Width:
                    nw.Width = (WORD)tag->ti_Data;
                    break;
                case WA_Height:
                    nw.Height = (WORD)tag->ti_Data;
                    break;
                case WA_DetailPen:
                    nw.DetailPen = (UBYTE)tag->ti_Data;
                    break;
                case WA_BlockPen:
                    nw.BlockPen = (UBYTE)tag->ti_Data;
                    break;
                case WA_IDCMP:
                    nw.IDCMPFlags = (ULONG)tag->ti_Data;
                    break;
                case WA_Flags:
                    nw.Flags = (ULONG)tag->ti_Data;
                    break;
                case WA_Gadgets:
                    nw.FirstGadget = (struct Gadget *)tag->ti_Data;
                    break;
                case WA_Title:
                    nw.Title = (UBYTE *)tag->ti_Data;
                    break;
                case WA_CustomScreen:
                    nw.Screen = (struct Screen *)tag->ti_Data;
                    nw.Type = CUSTOMSCREEN;
                    break;
                case WA_MinWidth:
                    nw.MinWidth = (WORD)tag->ti_Data;
                    break;
                case WA_MinHeight:
                    nw.MinHeight = (WORD)tag->ti_Data;
                    break;
                case WA_MaxWidth:
                    nw.MaxWidth = (UWORD)tag->ti_Data;
                    break;
                case WA_MaxHeight:
                    nw.MaxHeight = (UWORD)tag->ti_Data;
                    break;
                /* Boolean flags - set or clear flags based on ti_Data */
                case WA_SizeGadget:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_SIZEGADGET;
                    else
                        nw.Flags &= ~WFLG_SIZEGADGET;
                    break;
                case WA_DragBar:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_DRAGBAR;
                    else
                        nw.Flags &= ~WFLG_DRAGBAR;
                    break;
                case WA_DepthGadget:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_DEPTHGADGET;
                    else
                        nw.Flags &= ~WFLG_DEPTHGADGET;
                    break;
                case WA_CloseGadget:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_CLOSEGADGET;
                    else
                        nw.Flags &= ~WFLG_CLOSEGADGET;
                    break;
                case WA_Backdrop:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_BACKDROP;
                    else
                        nw.Flags &= ~WFLG_BACKDROP;
                    break;
                case WA_ReportMouse:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_REPORTMOUSE;
                    else
                        nw.Flags &= ~WFLG_REPORTMOUSE;
                    break;
                case WA_NoCareRefresh:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_NOCAREREFRESH;
                    else
                        nw.Flags &= ~WFLG_NOCAREREFRESH;
                    break;
                case WA_Borderless:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_BORDERLESS;
                    else
                        nw.Flags &= ~WFLG_BORDERLESS;
                    break;
                case WA_Activate:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_ACTIVATE;
                    else
                        nw.Flags &= ~WFLG_ACTIVATE;
                    break;
                case WA_RMBTrap:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_RMBTRAP;
                    else
                        nw.Flags &= ~WFLG_RMBTRAP;
                    break;
                case WA_SimpleRefresh:
                    if (tag->ti_Data)
                    {
                        nw.Flags &= ~(WFLG_SMART_REFRESH | WFLG_SUPER_BITMAP);
                        nw.Flags |= WFLG_SIMPLE_REFRESH;
                    }
                    break;
                case WA_SmartRefresh:
                    if (tag->ti_Data)
                    {
                        nw.Flags &= ~(WFLG_SIMPLE_REFRESH | WFLG_SUPER_BITMAP);
                        nw.Flags |= WFLG_SMART_REFRESH;
                    }
                    break;
                case WA_GimmeZeroZero:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_GIMMEZEROZERO;
                    else
                        nw.Flags &= ~WFLG_GIMMEZEROZERO;
                    break;
                /* Tags we recognize but don't fully implement yet */
                case WA_PubScreenName:
                case WA_PubScreen:
                case WA_PubScreenFallBack:
                case WA_InnerWidth:
                case WA_InnerHeight:
                case WA_Zoom:
                case WA_MouseQueue:
                case WA_BackFill:
                case WA_RptQueue:
                case WA_AutoAdjust:
                case WA_ScreenTitle:
                case WA_Checkmark:
                case WA_SuperBitMap:
                case WA_MenuHelp:
                case WA_NewLookMenus:
                case WA_NotifyDepth:
                case WA_Pointer:
                case WA_BusyPointer:
                case WA_PointerDelay:
                case WA_TabletMessages:
                case WA_HelpGroup:
                case WA_HelpGroupWindow:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() ignoring tag 0x%08lx (not yet implemented)\n",
                            tag->ti_Tag);
                    break;
                default:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() unknown tag 0x%08lx\n",
                            tag->ti_Tag);
                    break;
            }
        }
    }
    
    /* Call our existing OpenWindow with the assembled NewWindow */
    return _intuition_OpenWindow(IntuitionBase, &nw);
}

struct Screen * _intuition_OpenScreenTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewScreen * newScreen __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    struct NewScreen ns;
    struct TagItem *tstate;
    struct TagItem *tag;
    
    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() called, newScreen=0x%08lx, tagList=0x%08lx\n",
            (ULONG)newScreen, (ULONG)tagList);
    
    /* Start with defaults from NewScreen if provided, else use sensible defaults */
    if (newScreen)
    {
        /* Copy the NewScreen structure */
        ns = *newScreen;
    }
    else
    {
        /* Initialize with defaults */
        memset(&ns, 0, sizeof(ns));
        ns.Width = 640;
        ns.Height = 256;
        ns.Depth = 2;
        ns.Type = CUSTOMSCREEN;
        ns.DetailPen = 0;
        ns.BlockPen = 1;
    }
    
    /* Process tags to override NewScreen fields */
    if (tagList)
    {
        tstate = (struct TagItem *)tagList;
        while ((tag = NextTagItem(&tstate)))
        {
            switch (tag->ti_Tag)
            {
                case SA_Left:
                    ns.LeftEdge = (WORD)tag->ti_Data;
                    break;
                case SA_Top:
                    ns.TopEdge = (WORD)tag->ti_Data;
                    break;
                case SA_Width:
                    ns.Width = (WORD)tag->ti_Data;
                    break;
                case SA_Height:
                    ns.Height = (WORD)tag->ti_Data;
                    break;
                case SA_Depth:
                    ns.Depth = (WORD)tag->ti_Data;
                    break;
                case SA_DetailPen:
                    ns.DetailPen = (UBYTE)tag->ti_Data;
                    break;
                case SA_BlockPen:
                    ns.BlockPen = (UBYTE)tag->ti_Data;
                    break;
                case SA_Title:
                    ns.DefaultTitle = (UBYTE *)tag->ti_Data;
                    break;
                case SA_Type:
                    ns.Type = (UWORD)tag->ti_Data;
                    break;
                case SA_Behind:
                    if (tag->ti_Data)
                        ns.Type |= SCREENBEHIND;
                    break;
                case SA_Quiet:
                    if (tag->ti_Data)
                        ns.Type |= SCREENQUIET;
                    break;
                case SA_ShowTitle:
                    if (tag->ti_Data)
                        ns.Type |= SHOWTITLE;
                    break;
                case SA_AutoScroll:
                    if (tag->ti_Data)
                        ns.Type |= AUTOSCROLL;
                    break;
                case SA_BitMap:
                    ns.CustomBitMap = (struct BitMap *)tag->ti_Data;
                    if (tag->ti_Data)
                        ns.Type |= CUSTOMBITMAP;
                    break;
                case SA_Font:
                    ns.Font = (struct TextAttr *)tag->ti_Data;
                    break;
                /* Tags we recognize but don't fully implement yet */
                case SA_DisplayID:
                case SA_DClip:
                case SA_Overscan:
                case SA_Colors:
                case SA_Colors32:
                case SA_Pens:
                case SA_PubName:
                case SA_PubSig:
                case SA_PubTask:
                case SA_SysFont:
                case SA_ErrorCode:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() ignoring tag 0x%08lx (not yet implemented)\n",
                            tag->ti_Tag);
                    break;
                default:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() unknown tag 0x%08lx\n",
                            tag->ti_Tag);
                    break;
            }
        }
    }
    
    /* Call our existing OpenScreen with the assembled NewScreen */
    return _intuition_OpenScreen(IntuitionBase, &ns);
}

VOID _intuition_DrawImageState ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"),
                                                        register ULONG state __asm("d2"),
                                                        register const struct DrawInfo * drawInfo __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: DrawImageState() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_PointInImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG point __asm("d0"),
                                                        register struct Image * image __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: PointInImage() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _intuition_EraseImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: EraseImage() unimplemented STUB called.\n");
    assert(FALSE);
}

APTR _intuition_NewObjectA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"),
                                                        register CONST_STRPTR classID __asm("a1"),
                                                        register const struct TagItem * tagList __asm("a2"))
{
    /*
     * NewObjectA() creates a new BOOPSI object.
     * For now, return NULL to indicate failure.
     * TODO: Implement BOOPSI object system
     */
    DPRINTF (LOG_DEBUG, "_intuition: NewObjectA() classPtr=0x%08lx classID='%s' (stub, returning NULL)\n",
             (ULONG)classPtr, classID ? (const char*)classID : "(null)");
    return NULL;
}

VOID _intuition_DisposeObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR object __asm("a0"))
{
    /*
     * DisposeObject() disposes of a BOOPSI object.
     * For now, this is a no-op stub.
     * TODO: Implement BOOPSI object system
     */
    DPRINTF (LOG_DEBUG, "_intuition: DisposeObject() object=0x%08lx (stub)\n", (ULONG)object);
}

ULONG _intuition_SetAttrsA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR object __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    /*
     * SetAttrsA() sets attributes on a BOOPSI object.
     * For now, return 0 (no attrs changed).
     * TODO: Implement BOOPSI object system
     */
    DPRINTF (LOG_DEBUG, "_intuition: SetAttrsA() object=0x%08lx (stub)\n", (ULONG)object);
    return 0;
}

ULONG _intuition_GetAttr ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG attrID __asm("d0"),
                                                        register APTR object __asm("a0"),
                                                        register ULONG * storagePtr __asm("a1"))
{
    /*
     * GetAttr() gets an attribute from a BOOPSI object.
     * For now, return FALSE (attribute not found).
     * TODO: Implement BOOPSI object system
     */
    DPRINTF (LOG_DEBUG, "_intuition: GetAttr() attrID=0x%08lx object=0x%08lx (stub)\n",
             attrID, (ULONG)object);
    return FALSE;
}

ULONG _intuition_SetGadgetAttrsA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register const struct TagItem * tagList __asm("a3"))
{
    /*
     * SetGadgetAttrsA() sets attributes on a BOOPSI gadget.
     * For now, return 0 (no attrs changed).
     * TODO: Implement BOOPSI gadget system
     */
    DPRINTF (LOG_DEBUG, "_intuition: SetGadgetAttrsA() gadget=0x%08lx window=0x%08lx (stub)\n",
             (ULONG)gadget, (ULONG)window);
    return 0;
}

APTR _intuition_NextObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR objectPtrPtr __asm("a0"))
{
    /*
     * NextObject() gets the next object from an iteration state.
     * For now, return NULL (end of list).
     * TODO: Implement BOOPSI object iteration
     */
    DPRINTF (LOG_DEBUG, "_intuition: NextObject() objectPtrPtr=0x%08lx (stub)\n", (ULONG)objectPtrPtr);
    return NULL;
}

VOID _intuition_private2 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private2() unimplemented STUB called.\n");
    assert(FALSE);
}

struct IClass * _intuition_MakeClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR classID __asm("a0"),
                                                        register CONST_STRPTR superClassID __asm("a1"),
                                                        register const struct IClass * superClassPtr __asm("a2"),
                                                        register UWORD instanceSize __asm("d0"),
                                                        register ULONG flags __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: MakeClass() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _intuition_AddClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: AddClass() unimplemented STUB called.\n");
    assert(FALSE);
}

struct DrawInfo * _intuition_GetScreenDrawInfo ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: GetScreenDrawInfo() screen=0x%08lx\n", (ULONG)screen);

    if (!screen)
        return NULL;

    /*
     * GetScreenDrawInfo returns information about the screen's drawing pens
     * and font. We allocate a DrawInfo structure and populate it with
     * the screen's attributes.
     */
    
    /* For now, return a static DrawInfo with default pens.
     * TODO: Properly allocate and track DrawInfo per screen.
     */
    static UWORD defaultPens[NUMDRIPENS + 1] = {
        0,    /* DETAILPEN */
        1,    /* BLOCKPEN */
        1,    /* TEXTPEN */
        2,    /* SHINEPEN */
        1,    /* SHADOWPEN */
        3,    /* FILLPEN */
        1,    /* FILLTEXTPEN */
        0,    /* BACKGROUNDPEN */
        2,    /* HIGHLIGHTTEXTPEN */
        1,    /* BARDETAILPEN - V39 */
        2,    /* BARBLOCKPEN - V39 */
        1,    /* BARTRIMPEN - V39 */
        (UWORD)~0  /* Terminator */
    };
    
    static struct DrawInfo drawInfo = {
        .dri_Version = 2,           /* V39 compatible */
        .dri_NumPens = NUMDRIPENS,
        .dri_Pens = defaultPens,
        .dri_Font = NULL,           /* Will be set below */
        .dri_Depth = 2,             /* 4 colors */
        .dri_Resolution = { 44, 44 },
        .dri_Flags = DRIF_NEWLOOK,
        .dri_CheckMark = NULL,
        .dri_AmigaKey = NULL,
        .dri_Reserved = { 0 }
    };
    
    /* Set font from screen */
    drawInfo.dri_Font = screen->RastPort.Font;
    
    /* Set depth from screen bitmap if available */
    if (screen->RastPort.BitMap) {
        drawInfo.dri_Depth = screen->RastPort.BitMap->Depth;
    }
    
    return &drawInfo;
}

VOID _intuition_FreeScreenDrawInfo ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register struct DrawInfo * drawInfo __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: FreeScreenDrawInfo() screen=0x%08lx drawInfo=0x%08lx\n",
             (ULONG)screen, (ULONG)drawInfo);
    /*
     * FreeScreenDrawInfo releases a DrawInfo obtained from GetScreenDrawInfo.
     * Since we're using a static DrawInfo for now, this is a no-op.
     * TODO: Free allocated DrawInfo when we properly allocate them.
     */
}

BOOL _intuition_ResetMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Menu * menu __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ResetMenuStrip() window=0x%08lx menu=0x%08lx\n",
             (ULONG)window, (ULONG)menu);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: ResetMenuStrip() called with NULL window\n");
        return TRUE;  /* Always returns TRUE per RKRM */
    }

    /* ResetMenuStrip is a "fast" SetMenuStrip - it just re-attaches the menu
     * without recalculating internal values. Use only when the menu was
     * previously attached via SetMenuStrip and only CHECKED/ITEMENABLED
     * flags have changed.
     */
    window->MenuStrip = menu;

    return TRUE;
}

VOID _intuition_RemoveClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: RemoveClass() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_FreeClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: FreeClass() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _intuition_private3 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private3() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private4 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private4() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private5 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private5() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private6 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private6() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private7 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private7() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private8 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private8() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private9 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private9() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private10 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private10() unimplemented STUB called.\n");
    assert(FALSE);
}

struct ScreenBuffer * _intuition_AllocScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct BitMap * bm __asm("a1"),
                                                        register ULONG flags __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: AllocScreenBuffer() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _intuition_FreeScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct ScreenBuffer * sb __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: FreeScreenBuffer() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_ChangeScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct ScreenBuffer * sb __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: ChangeScreenBuffer() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _intuition_ScreenDepth ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register ULONG flags __asm("d0"),
                                                        register APTR reserved __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: ScreenDepth() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ScreenPosition ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register ULONG flags __asm("d0"),
                                                        register LONG x1 __asm("d1"),
                                                        register LONG y1 __asm("d2"),
                                                        register LONG x2 __asm("d3"),
                                                        register LONG y2 __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_intuition: ScreenPosition() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ScrollWindowRaster ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * win __asm("a1"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"),
                                                        register WORD xMin __asm("d2"),
                                                        register WORD yMin __asm("d3"),
                                                        register WORD xMax __asm("d4"),
                                                        register WORD yMax __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_intuition: ScrollWindowRaster() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_LendMenus ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * fromwindow __asm("a0"),
                                                        register struct Window * towindow __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: LendMenus() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_DoGadgetMethodA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gad __asm("a0"),
                                                        register struct Window * win __asm("a1"),
                                                        register struct Requester * req __asm("a2"),
                                                        register Msg message __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_intuition: DoGadgetMethodA() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _intuition_SetWindowPointerA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * win __asm("a0"),
                                                        register const struct TagItem * taglist __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetWindowPointerA() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_TimedDisplayAlert ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG alertNumber __asm("d0"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register UWORD height __asm("d1"),
                                                        register ULONG time __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: TimedDisplayAlert() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _intuition_HelpControl ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * win __asm("a0"),
                                                        register ULONG flags __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: HelpControl() unimplemented STUB called.\n");
    assert(FALSE);
}


struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_intuition_FuncTab [];
extern struct MyDataInit __g_lxa_intuition_DataTab;
extern struct InitTable  __g_lxa_intuition_InitTab;
extern APTR              __g_lxa_intuition_EndResident;

static struct Resident __aligned ROMTag =
{                                 //                               offset
    RTC_MATCHWORD,                // UWORD rt_MatchWord;                0
    &ROMTag,                      // struct Resident *rt_MatchTag;      2
    &__g_lxa_intuition_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;                    7
    VERSION,                      // UBYTE rt_Version;                  8
    NT_LIBRARY,                   // UBYTE rt_Type;                     9
    0,                            // BYTE  rt_Pri;                     10
    &_g_intuition_ExLibName[0],     // char  *rt_Name;                   14
    &_g_intuition_ExLibID[0],       // char  *rt_IdString;               18
    &__g_lxa_intuition_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_intuition_EndResident;
struct Resident *__lxa_intuition_ROMTag = &ROMTag;

struct InitTable __g_lxa_intuition_InitTab =
{
    (ULONG)               sizeof(struct IntuitionBase),        // LibBaseSize
    (APTR              *) &__g_lxa_intuition_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_intuition_DataTab,     // DataTable
    (APTR)                __g_lxa_intuition_InitLib       // InitLibFn
};

APTR __g_lxa_intuition_FuncTab [] =
{
    __g_lxa_intuition_OpenLib,
    __g_lxa_intuition_CloseLib,
    __g_lxa_intuition_ExpungeLib,
    __g_lxa_intuition_ExtFuncLib,
    _intuition_OpenIntuition, // offset = -30
    _intuition_Intuition, // offset = -36
    _intuition_AddGadget, // offset = -42
    _intuition_ClearDMRequest, // offset = -48
    _intuition_ClearMenuStrip, // offset = -54
    _intuition_ClearPointer, // offset = -60
    _intuition_CloseScreen, // offset = -66
    _intuition_CloseWindow, // offset = -72
    _intuition_CloseWorkBench, // offset = -78
    _intuition_CurrentTime, // offset = -84
    _intuition_DisplayAlert, // offset = -90
    _intuition_DisplayBeep, // offset = -96
    _intuition_DoubleClick, // offset = -102
    _intuition_DrawBorder, // offset = -108
    _intuition_DrawImage, // offset = -114
    _intuition_EndRequest, // offset = -120
    _intuition_GetDefPrefs, // offset = -126
    _intuition_GetPrefs, // offset = -132
    _intuition_InitRequester, // offset = -138
    _intuition_ItemAddress, // offset = -144
    _intuition_ModifyIDCMP, // offset = -150
    _intuition_ModifyProp, // offset = -156
    _intuition_MoveScreen, // offset = -162
    _intuition_MoveWindow, // offset = -168
    _intuition_OffGadget, // offset = -174
    _intuition_OffMenu, // offset = -180
    _intuition_OnGadget, // offset = -186
    _intuition_OnMenu, // offset = -192
    _intuition_OpenScreen, // offset = -198
    _intuition_OpenWindow, // offset = -204
    _intuition_OpenWorkBench, // offset = -210
    _intuition_PrintIText, // offset = -216
    _intuition_RefreshGadgets, // offset = -222
    _intuition_RemoveGadget, // offset = -228
    _intuition_ReportMouse, // offset = -234
    _intuition_Request, // offset = -240
    _intuition_ScreenToBack, // offset = -246
    _intuition_ScreenToFront, // offset = -252
    _intuition_SetDMRequest, // offset = -258
    _intuition_SetMenuStrip, // offset = -264
    _intuition_SetPointer, // offset = -270
    _intuition_SetWindowTitles, // offset = -276
    _intuition_ShowTitle, // offset = -282
    _intuition_SizeWindow, // offset = -288
    _intuition_ViewAddress, // offset = -294
    _intuition_ViewPortAddress, // offset = -300
    _intuition_WindowToBack, // offset = -306
    _intuition_WindowToFront, // offset = -312
    _intuition_WindowLimits, // offset = -318
    _intuition_SetPrefs, // offset = -324
    _intuition_IntuiTextLength, // offset = -330
    _intuition_WBenchToBack, // offset = -336
    _intuition_WBenchToFront, // offset = -342
    _intuition_AutoRequest, // offset = -348
    _intuition_BeginRefresh, // offset = -354
    _intuition_BuildSysRequest, // offset = -360
    _intuition_EndRefresh, // offset = -366
    _intuition_FreeSysRequest, // offset = -372
    _intuition_MakeScreen, // offset = -378
    _intuition_RemakeDisplay, // offset = -384
    _intuition_RethinkDisplay, // offset = -390
    _intuition_AllocRemember, // offset = -396
    _intuition_private0, // offset = -402
    _intuition_FreeRemember, // offset = -408
    _intuition_LockIBase, // offset = -414
    _intuition_UnlockIBase, // offset = -420
    _intuition_GetScreenData, // offset = -426
    _intuition_RefreshGList, // offset = -432
    _intuition_AddGList, // offset = -438
    _intuition_RemoveGList, // offset = -444
    _intuition_ActivateWindow, // offset = -450
    _intuition_RefreshWindowFrame, // offset = -456
    _intuition_ActivateGadget, // offset = -462
    _intuition_NewModifyProp, // offset = -468
    _intuition_QueryOverscan, // offset = -474
    _intuition_MoveWindowInFrontOf, // offset = -480
    _intuition_ChangeWindowBox, // offset = -486
    _intuition_SetEditHook, // offset = -492
    _intuition_SetMouseQueue, // offset = -498
    _intuition_ZipWindow, // offset = -504
    _intuition_LockPubScreen, // offset = -510
    _intuition_UnlockPubScreen, // offset = -516
    _intuition_LockPubScreenList, // offset = -522
    _intuition_UnlockPubScreenList, // offset = -528
    _intuition_NextPubScreen, // offset = -534
    _intuition_SetDefaultPubScreen, // offset = -540
    _intuition_SetPubScreenModes, // offset = -546
    _intuition_PubScreenStatus, // offset = -552
    _intuition_ObtainGIRPort, // offset = -558
    _intuition_ReleaseGIRPort, // offset = -564
    _intuition_GadgetMouse, // offset = -570
    _intuition_private1, // offset = -576
    _intuition_GetDefaultPubScreen, // offset = -582
    _intuition_EasyRequestArgs, // offset = -588
    _intuition_BuildEasyRequestArgs, // offset = -594
    _intuition_SysReqHandler, // offset = -600
    _intuition_OpenWindowTagList, // offset = -606
    _intuition_OpenScreenTagList, // offset = -612
    _intuition_DrawImageState, // offset = -618
    _intuition_PointInImage, // offset = -624
    _intuition_EraseImage, // offset = -630
    _intuition_NewObjectA, // offset = -636
    _intuition_DisposeObject, // offset = -642
    _intuition_SetAttrsA, // offset = -648
    _intuition_GetAttr, // offset = -654
    _intuition_SetGadgetAttrsA, // offset = -660
    _intuition_NextObject, // offset = -666
    _intuition_private2, // offset = -672
    _intuition_MakeClass, // offset = -678
    _intuition_AddClass, // offset = -684
    _intuition_GetScreenDrawInfo, // offset = -690
    _intuition_FreeScreenDrawInfo, // offset = -696
    _intuition_ResetMenuStrip, // offset = -702
    _intuition_RemoveClass, // offset = -708
    _intuition_FreeClass, // offset = -714
    _intuition_private3, // offset = -720
    _intuition_private4, // offset = -726
    _intuition_private5, // offset = -732
    _intuition_private6, // offset = -738
    _intuition_private7, // offset = -744
    _intuition_private8, // offset = -750
    _intuition_private9, // offset = -756
    _intuition_private10, // offset = -762
    _intuition_AllocScreenBuffer, // offset = -768
    _intuition_FreeScreenBuffer, // offset = -774
    _intuition_ChangeScreenBuffer, // offset = -780
    _intuition_ScreenDepth, // offset = -786
    _intuition_ScreenPosition, // offset = -792
    _intuition_ScrollWindowRaster, // offset = -798
    _intuition_LendMenus, // offset = -804
    _intuition_DoGadgetMethodA, // offset = -810
    _intuition_SetWindowPointerA, // offset = -816
    _intuition_TimedDisplayAlert, // offset = -822
    _intuition_HelpControl, // offset = -828
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_intuition_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_intuition_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_intuition_ExLibID[0],
    (ULONG) 0
};

