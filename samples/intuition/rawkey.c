/*
 * rawkey.c - Raw Keyboard Input Sample
 *
 * This is an RKM-style sample demonstrating raw keyboard input handling.
 * It creates a window that:
 *   - Receives RAWKEY events from the keyboard
 *   - Uses RawKeyConvert() to translate raw keycodes to ASCII
 *   - Shows qualifier keys (Shift, Ctrl, Alt, etc.)
 *   - Handles key up and key down events
 *   - Demonstrates the console.device keymapping functionality
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/console_protos.h>
#include <stdio.h>

/* A buffer is created for RawKeyConvert() to put its output. */
#define BUFSIZE (16)

/* our function prototypes */
LONG deadKeyConvert(struct IntuiMessage *msg, UBYTE *kbuffer,
                    LONG kbsize, struct KeyMap *kmap, struct InputEvent *ievent);
VOID print_qualifiers(ULONG qual);
BOOL doKeys(struct IntuiMessage *msg, struct InputEvent *ievent,
            UBYTE **buffer, ULONG *bufsize);
VOID process_window(struct Window *win, struct InputEvent *ievent,
                    UBYTE **buffer, ULONG *bufsize);

struct Library *IntuitionBase, *ConsoleDevice;

/* main() - set-up everything used by program. */
VOID main(int argc, char **argv)
{
    struct Window *win;
    struct IOStdReq ioreq;
    struct InputEvent *ievent;
    UBYTE *buffer;
    ULONG bufsize = BUFSIZE;

    printf("RawKey: Starting raw keyboard input demonstration\n");

    if (IntuitionBase = OpenLibrary("intuition.library", 37))
    {
        /* Open the console device just to do keymapping. (unit -1 means any unit) */
        if (0 == OpenDevice("console.device", -1, (struct IORequest *)&ioreq, 0))
        {
            ConsoleDevice = (struct Library *)ioreq.io_Device;
            printf("RawKey: Opened console.device for keymapping\n");

            /* Allocate the initial character buffer used by RawKeyConvert() */
            if (buffer = AllocMem(bufsize, MEMF_CLEAR))
            {
                if (ievent = AllocMem(sizeof(struct InputEvent), MEMF_CLEAR))
                {
                    if (win = OpenWindowTags(NULL,
                            WA_Width, 300,
                            WA_Height, 50,
                            WA_Flags, WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | 
                                      WFLG_ACTIVATE | WFLG_DRAGBAR,
                            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY,
                            WA_Title, "Raw Key Example",
                            TAG_END))
                    {
                        printf("RawKey: Window opened successfully\n");
                        printf("RawKey: Press keyboard keys to see ASCII conversion from rawkey\n");
                        printf("RawKey: Unprintable characters will be shown as ?\n");
                        printf("RawKey: Click close gadget to exit\n");
                        
                        process_window(win, ievent, &buffer, &bufsize);
                        
                        CloseWindow(win);
                        printf("RawKey: Window closed\n");
                    }
                    else
                    {
                        printf("RawKey: Failed to open window\n");
                    }
                    FreeMem(ievent, sizeof(struct InputEvent));
                }
                else
                {
                    printf("RawKey: Failed to allocate InputEvent\n");
                }
                /* Buffer can be freed elsewhere in the program so test first. */
                if (buffer != NULL)
                    FreeMem(buffer, bufsize);
            }
            else
            {
                printf("RawKey: Failed to allocate buffer\n");
            }
            CloseDevice((struct IORequest *)&ioreq);
        }
        else
        {
            printf("RawKey: Failed to open console.device\n");
        }
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("RawKey: Failed to open intuition.library\n");
    }
    printf("RawKey: Done\n");
}

/* Convert RAWKEYs into VANILLAKEYs, also shows special keys like HELP, Cursor Keys,
** FKeys, etc.  It returns:
**   -2 if not a RAWKEY event.
**   -1 if not enough room in the buffer, try again with a bigger buffer.
**   otherwise, returns the number of characters placed in the buffer.
*/
LONG deadKeyConvert(struct IntuiMessage *msg, UBYTE *kbuffer,
    LONG kbsize, struct KeyMap *kmap, struct InputEvent *ievent)
{
    if (msg->Class != IDCMP_RAWKEY) return(-2);
    ievent->ie_Class = IECLASS_RAWKEY;
    ievent->ie_Code = msg->Code;
    ievent->ie_Qualifier = msg->Qualifier;
    ievent->ie_position.ie_addr = *((APTR*)msg->IAddress);

    return(RawKeyConvert(ievent, kbuffer, kbsize, kmap));
}

/* print_qualifiers() - print out the values found in the qualifier bits of
** the message. This will print out all of the qualifier bits set.
*/
VOID print_qualifiers(ULONG qual)
{
    BOOL first = TRUE;
    
    printf("RawKey: Qualifiers: ");
    
    if (qual & IEQUALIFIER_LSHIFT)
    {
        if (!first) printf(",");
        printf("LShift");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_RSHIFT)
    {
        if (!first) printf(",");
        printf("RShift");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_CAPSLOCK)
    {
        if (!first) printf(",");
        printf("CapsLock");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_CONTROL)
    {
        if (!first) printf(",");
        printf("Ctrl");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_LALT)
    {
        if (!first) printf(",");
        printf("LAlt");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_RALT)
    {
        if (!first) printf(",");
        printf("RAlt");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_LCOMMAND)
    {
        if (!first) printf(",");
        printf("LCmd");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_RCOMMAND)
    {
        if (!first) printf(",");
        printf("RCmd");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_NUMERICPAD)
    {
        if (!first) printf(",");
        printf("NumPad");
        first = FALSE;
    }
    if (qual & IEQUALIFIER_REPEAT)
    {
        if (!first) printf(",");
        printf("Repeat");
        first = FALSE;
    }
    
    if (first)
    {
        printf("None");
    }
    printf("\n");
}

/* doKeys() - Show what keys were pressed. */
BOOL doKeys(struct IntuiMessage *msg, struct InputEvent *ievent,
            UBYTE **buffer, ULONG *bufsize)
{
    USHORT char_pos;
    LONG numchars;
    BOOL ret_code = TRUE;
    UBYTE realc, c;

    /* deadKeyConvert() returns -1 if there was not enough space in the buffer to
    ** convert the string. Here, the routine increases the size of the buffer on the
    ** fly...Set the return code to FALSE on failure.
    */
    numchars = deadKeyConvert(msg, *buffer, *bufsize - 1, NULL, ievent);
    while ((numchars == -1) && (*buffer != NULL))
    {
        /* conversion failed, buffer too small. try to double the size of the buffer. */
        FreeMem(*buffer, *bufsize);
        *bufsize = *bufsize << 1;
        printf("RawKey: Increasing buffer size to %ld\n", *bufsize);

        if (NULL == (*buffer = AllocMem(*bufsize, MEMF_CLEAR)))
            ret_code = FALSE;
        else
            numchars = deadKeyConvert(msg, *buffer, *bufsize - 1, NULL, ievent);
    }

    /* numchars contains the number of characters placed within the buffer. */
    if (*buffer != NULL)
    {
        /* if high bit set, then this is a key up otherwise this is a key down */
        if (msg->Code & 0x80)
            printf("RawKey: Key Up:   ");
        else
            printf("RawKey: Key Down: ");

        printf("rawkey #%d maps to %ld ASCII character(s)\n", 
               0x7F & msg->Code, numchars);
        
        print_qualifiers(msg->Qualifier);
        
        for (char_pos = 0; char_pos < numchars; char_pos++)
        {
            realc = c = (*buffer)[char_pos];
            if ((c <= 0x1F) || ((c >= 0x80) && (c < 0xa0)))
                c = '?';
            printf("RawKey:   Char %d: code=%3d ($%02x) = '%c'\n", 
                   char_pos, realc, realc, c);
        }
    }
    return(ret_code);
}

/* process_window() - simple event loop. Note that the message is not replied
** to until the end of the loop so that it may be used in the doKeys() call.
*/
VOID process_window(struct Window *win, struct InputEvent *ievent,
    UBYTE **buffer, ULONG *bufsize)
{
    struct IntuiMessage *msg;
    BOOL done;
    ULONG key_count = 0;
    ULONG max_keys = 10;  /* Limit key events to avoid infinite loops in tests */

    done = FALSE;
    while (done == FALSE)
    {
        Wait((1L << win->UserPort->mp_SigBit));
        while ((done == FALSE) && (msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
        {
            switch (msg->Class)
            {
                case IDCMP_CLOSEWINDOW:
                    printf("RawKey: Close gadget clicked\n");
                    done = TRUE;
                    break;
                case IDCMP_RAWKEY:
                    key_count++;
                    if (key_count <= max_keys)
                    {
                        if (FALSE == doKeys(msg, ievent, buffer, bufsize))
                        {
                            printf("RawKey: Buffer allocation failed, exiting\n");
                            done = TRUE;
                        }
                    }
                    else if (key_count == max_keys + 1)
                    {
                        printf("RawKey: (suppressing further key output)\n");
                    }
                    break;
            }
            ReplyMsg((struct Message *)msg);
        }
    }
    printf("RawKey: Processed %ld key events\n", key_count);
}
