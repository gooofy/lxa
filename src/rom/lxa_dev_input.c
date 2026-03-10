#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/inputevent.h>
#include <devices/input.h>
#include <devices/timer.h>

#include "util.h"

#define VERSION    40
#define REVISION   3
#define EXDEVNAME  "input"
#define EXDEVVER   " 40.3 (2026/03/10)"

#define QUALIFIER_KEY_MASK (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | \
                            IEQUALIFIER_CAPSLOCK | IEQUALIFIER_CONTROL | \
                            IEQUALIFIER_LALT | IEQUALIFIER_RALT | \
                            IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND | \
                            IEQUALIFIER_NUMERICPAD)

#define QUALIFIER_MOUSE_MASK (IEQUALIFIER_MIDBUTTON | IEQUALIFIER_RBUTTON | \
                              IEQUALIFIER_LEFTBUTTON)

#define QUALIFIER_STATE_MASK (QUALIFIER_KEY_MASK | QUALIFIER_MOUSE_MASK)

char __aligned _g_input_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_input_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_input_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_input_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

struct InputBase
{
    struct Device  ib_Device;
    BPTR           ib_SegList;
    struct List    ib_HandlerList;
    struct timeval ib_KeyRepeatThreshold;
    struct timeval ib_KeyRepeatPeriod;
    UWORD          ib_ActQualifier;
    UWORD          ib_MousePort;
    UBYTE          ib_MouseType;
};

static struct InputBase *g_input_base = NULL;

static void input_get_systime(struct timeval *tv)
{
    if (!tv)
    {
        return;
    }

    emucall1(EMU_CALL_GETSYSTIME, (ULONG)tv);
}

static void input_init_defaults(struct InputBase *inputbase)
{
    if (!inputbase)
    {
        return;
    }

    NEWLIST(&inputbase->ib_HandlerList);
    inputbase->ib_KeyRepeatThreshold.tv_secs = 0;
    inputbase->ib_KeyRepeatThreshold.tv_micro = 500000;
    inputbase->ib_KeyRepeatPeriod.tv_secs = 0;
    inputbase->ib_KeyRepeatPeriod.tv_micro = 40000;
    inputbase->ib_ActQualifier = 0;
    inputbase->ib_MousePort = 0;
    inputbase->ib_MouseType = 0;
}

static void input_update_qualifier(struct InputBase *inputbase,
                                   const struct InputEvent *event)
{
    UWORD qualifier_bits;

    if (!inputbase || !event)
    {
        return;
    }

    qualifier_bits = event->ie_Qualifier & QUALIFIER_STATE_MASK;

    switch (event->ie_Class)
    {
        case IECLASS_RAWKEY:
            inputbase->ib_ActQualifier &= ~QUALIFIER_KEY_MASK;
            inputbase->ib_ActQualifier |= qualifier_bits & QUALIFIER_KEY_MASK;
            inputbase->ib_ActQualifier &= ~QUALIFIER_MOUSE_MASK;
            inputbase->ib_ActQualifier |= qualifier_bits & QUALIFIER_MOUSE_MASK;
            break;

        case IECLASS_RAWMOUSE:
        case IECLASS_POINTERPOS:
            inputbase->ib_ActQualifier &= ~QUALIFIER_MOUSE_MASK;
            inputbase->ib_ActQualifier |= qualifier_bits & QUALIFIER_MOUSE_MASK;
            inputbase->ib_ActQualifier &= ~QUALIFIER_KEY_MASK;
            inputbase->ib_ActQualifier |= qualifier_bits & QUALIFIER_KEY_MASK;
            break;

        default:
            inputbase->ib_ActQualifier = qualifier_bits;
            break;
    }
}

static struct InputEvent *input_call_handler(struct Interrupt *handler,
                                             struct InputEvent *events)
{
    typedef struct InputEvent *(*InputHandlerFn)(register struct InputEvent *events __asm("a0"),
                                                 register APTR data __asm("a1"));
    InputHandlerFn entry;

    if (!handler || !handler->is_Code)
    {
        return events;
    }

    entry = (InputHandlerFn)handler->is_Code;
    return entry(events, handler->is_Data);
}

static struct InputEvent *input_forward_handlers(struct InputBase *inputbase,
                                                 struct InputEvent *events)
{
    struct Interrupt *handler;

    if (!inputbase)
    {
        return events;
    }

    for (handler = (struct Interrupt *)GETHEAD(&inputbase->ib_HandlerList);
         handler;
         handler = (struct Interrupt *)GETSUCC(handler))
    {
        events = input_call_handler(handler, events);
        if (!events)
        {
            break;
        }
    }

    return events;
}

static BOOL input_handler_is_linked(struct InputBase *inputbase,
                                    struct Interrupt *handler)
{
    struct Node *node;

    if (!inputbase || !handler)
    {
        return FALSE;
    }

    for (node = GETHEAD(&inputbase->ib_HandlerList); node; node = GETSUCC(node))
    {
        if (node == (struct Node *)handler)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void input_prepare_event(struct InputBase *inputbase,
                                struct InputEvent *event)
{
    if (!inputbase || !event)
    {
        return;
    }

    input_update_qualifier(inputbase, event);
    event->ie_Qualifier = (event->ie_Qualifier & ~QUALIFIER_STATE_MASK) |
                          inputbase->ib_ActQualifier;
    event->ie_NextEvent = NULL;
    input_get_systime(&event->ie_TimeStamp);
}

static LONG input_dispatch_single_event(struct InputBase *inputbase,
                                        struct InputEvent *event)
{
    if (!inputbase || !event)
    {
        return IOERR_BADADDRESS;
    }

    input_prepare_event(inputbase, event);
    input_forward_handlers(inputbase, event);
    return 0;
}

static LONG input_dispatch_event_chain(struct InputBase *inputbase,
                                       struct InputEvent *events,
                                       ULONG event_count,
                                       BOOL filter_classes)
{
    ULONG idx;

    if (!inputbase || !events)
    {
        return IOERR_BADADDRESS;
    }

    for (idx = 0; idx < event_count; idx++)
    {
        struct InputEvent *event = &events[idx];

        if (filter_classes && event->ie_Class != IECLASS_RAWKEY && event->ie_Class != IECLASS_RAWMOUSE)
        {
            continue;
        }

        input_prepare_event(inputbase, event);
        input_forward_handlers(inputbase, event);
    }

    return 0;
}

void _input_device_dispatch_event(struct InputEvent *event)
{
    if (!g_input_base || !event)
    {
        return;
    }

    input_dispatch_single_event(g_input_base, event);
}

UWORD __g_lxa_input_PeekQualifier(register struct Library *dev __asm("a6"))
{
    struct InputBase *inputbase = (struct InputBase *)dev;

    if (!inputbase)
    {
        return 0;
    }

    return inputbase->ib_ActQualifier;
}

static struct Library * __g_lxa_input_InitDev(register struct Library *dev __asm("d0"),
                                              register BPTR seglist __asm("a0"),
                                              register struct ExecBase *sysb __asm("a6"))
{
    struct InputBase *inputbase = (struct InputBase *)dev;

    (void)sysb;

    inputbase->ib_SegList = seglist;
    input_init_defaults(inputbase);
    g_input_base = inputbase;

    return dev;
}

static void __g_lxa_input_Open(register struct Library *dev __asm("a6"),
                               register struct IORequest *ioreq __asm("a1"),
                               register ULONG unitn __asm("d0"),
                               register ULONG flags __asm("d1"))
{
    (void)unitn;
    (void)flags;

    ioreq->io_Error = 0;
    ioreq->io_Device = (struct Device *)dev;
    ioreq->io_Unit = NULL;

    dev->lib_OpenCnt++;
    dev->lib_Flags &= ~LIBF_DELEXP;
}

static BPTR __g_lxa_input_Close(register struct Library *dev __asm("a6"),
                                register struct IORequest *ioreq __asm("a1"))
{
    (void)ioreq;

    if (dev->lib_OpenCnt > 0)
    {
        dev->lib_OpenCnt--;
    }

    return 0;
}

static BPTR __g_lxa_input_Expunge(register struct Library *dev __asm("a6"))
{
    (void)dev;
    return 0;
}

static BPTR __g_lxa_input_BeginIO(register struct Library *dev __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"))
{
    struct InputBase *inputbase = (struct InputBase *)dev;
    struct IOStdReq *io = (struct IOStdReq *)ioreq;
    LONG error = 0;

    ioreq->io_Error = 0;
    io->io_Actual = 0;

    switch (ioreq->io_Command)
    {
        case IND_ADDHANDLER:
        {
            struct Interrupt *handler = (struct Interrupt *)io->io_Data;

            if (!handler)
            {
                error = IOERR_BADADDRESS;
                break;
            }

            if (input_handler_is_linked(inputbase, handler))
            {
                break;
            }

            ((struct Node *)handler)->ln_Succ = NULL;
            ((struct Node *)handler)->ln_Pred = NULL;
            Enqueue(&inputbase->ib_HandlerList, (struct Node *)handler);
            break;
        }

        case IND_REMHANDLER:
        {
            struct Interrupt *handler = (struct Interrupt *)io->io_Data;

            if (!handler)
            {
                error = IOERR_BADADDRESS;
                break;
            }

            if (input_handler_is_linked(inputbase, handler))
            {
                Remove((struct Node *)handler);
                ((struct Node *)handler)->ln_Succ = NULL;
                ((struct Node *)handler)->ln_Pred = NULL;
            }
            break;
        }

        case IND_SETTHRESH:
            ((struct timerequest *)ioreq)->tr_time.tv_micro %= 1000000;
            inputbase->ib_KeyRepeatThreshold = ((struct timerequest *)ioreq)->tr_time;
            break;

        case IND_SETPERIOD:
            ((struct timerequest *)ioreq)->tr_time.tv_micro %= 1000000;
            inputbase->ib_KeyRepeatPeriod = ((struct timerequest *)ioreq)->tr_time;
            break;

        case IND_SETMPORT:
            if (!io->io_Data)
            {
                error = IOERR_BADADDRESS;
                break;
            }
            if (io->io_Length != 1)
            {
                error = IOERR_BADLENGTH;
                break;
            }
            inputbase->ib_MousePort = *((UBYTE *)io->io_Data);
            break;

        case IND_SETMTYPE:
            if (!io->io_Data)
            {
                error = IOERR_BADADDRESS;
                break;
            }
            if (io->io_Length != 1)
            {
                error = IOERR_BADLENGTH;
                break;
            }
            inputbase->ib_MouseType = *((UBYTE *)io->io_Data);
            break;

        case IND_WRITEEVENT:
            error = input_dispatch_single_event(inputbase, (struct InputEvent *)io->io_Data);
            break;

        case IND_ADDEVENT:
        {
            ULONG event_count;

            if (!io->io_Data)
            {
                error = IOERR_BADADDRESS;
                break;
            }
            if (io->io_Length == 0 || (io->io_Length % sizeof(struct InputEvent)) != 0)
            {
                error = IOERR_BADLENGTH;
                break;
            }

            event_count = io->io_Length / sizeof(struct InputEvent);
            error = input_dispatch_event_chain(inputbase,
                                               (struct InputEvent *)io->io_Data,
                                               event_count,
                                               TRUE);
            break;
        }

        default:
            error = IOERR_NOCMD;
            break;
    }

    ioreq->io_Error = error;
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }

    return 0;
}

static ULONG __g_lxa_input_AbortIO(register struct Library *dev __asm("a6"),
                                   register struct IORequest *ioreq __asm("a1"))
{
    (void)dev;
    (void)ioreq;
    return 0;
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

extern APTR              __g_lxa_input_FuncTab [];
extern struct MyDataInit __g_lxa_input_DataTab;
extern struct InitTable  __g_lxa_input_InitTab;
extern APTR              __g_lxa_input_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_input_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_input_ExDevName[0],
    &_g_input_ExDevID[0],
    &__g_lxa_input_InitTab
};

APTR __g_lxa_input_EndResident;
struct Resident *__lxa_input_ROMTag = &ROMTag;

struct InitTable __g_lxa_input_InitTab =
{
    (ULONG)sizeof(struct InputBase),
    (APTR *)&__g_lxa_input_FuncTab[0],
    (APTR)&__g_lxa_input_DataTab,
    (APTR)__g_lxa_input_InitDev
};

APTR __g_lxa_input_FuncTab [] =
{
    __g_lxa_input_Open,
    __g_lxa_input_Close,
    __g_lxa_input_Expunge,
    NULL,
    __g_lxa_input_BeginIO,
    __g_lxa_input_AbortIO,
    __g_lxa_input_PeekQualifier,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_input_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE)(ULONG)OFFSET(Node,    ln_Name), (ULONG)&_g_input_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_input_ExDevID[0],
    (ULONG)0
};
