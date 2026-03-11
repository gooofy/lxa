#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/inputevent.h>
#include <devices/keyboard.h>
#include <libraries/keymap.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXDEVNAME  "keyboard"
#define EXDEVVER   " 40.1 (2026/03/11)"

#define KBD_MAX_KEYS          256
#define KBD_MATRIX_BYTES      (KBD_MAX_KEYS / 8)
#define KBD_EVENT_BUFFER_SIZE 128
#define RAWKEY_RESET_WARNING  0x78

char __aligned _g_keyboard_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_keyboard_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_keyboard_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_keyboard_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

struct KeyboardEvent
{
    UBYTE ke_Code;
    UBYTE ke_Pad;
    UWORD ke_Qualifier;
};

struct KeyboardUnit
{
    struct Unit     ku_Unit;
    struct KeyboardUnit *ku_Next;
    ULONG           ku_ReadSeq;
    struct IOStdReq *ku_PendingRead;
};

struct KeyboardBase
{
    struct Device     kb_Device;
    BPTR              kb_SegList;
    struct KeyboardUnit *kb_Units;
    struct List       kb_ResetHandlers;
    struct KeyboardEvent kb_Events[KBD_EVENT_BUFFER_SIZE];
    ULONG             kb_WriteSeq;
    UBYTE             kb_Matrix[KBD_MATRIX_BYTES];
    BOOL              kb_ResetPhase;
    UWORD             kb_PendingResetAcks;
};

static struct KeyboardBase *g_keyboard_base = NULL;

static BOOL keyboard_is_numeric_pad(UWORD code)
{
    code &= ~IECODE_UP_PREFIX;

    switch (code)
    {
        case 0x0F:
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
        case 0x43:
        case 0x4A:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
            return TRUE;

        default:
            return FALSE;
    }
}

static void keyboard_matrix_set(struct KeyboardBase *keyboardbase, UWORD code)
{
    UWORD key = code & ~IECODE_UP_PREFIX;

    if (!keyboardbase || key >= KBD_MAX_KEYS)
    {
        return;
    }

    if (code & IECODE_UP_PREFIX)
    {
        keyboardbase->kb_Matrix[key >> 3] &= (UBYTE)~(1U << (key & 7));
    }
    else
    {
        keyboardbase->kb_Matrix[key >> 3] |= (UBYTE)(1U << (key & 7));
    }
}

static BOOL keyboard_reset_handler_is_linked(struct KeyboardBase *keyboardbase,
                                             struct Interrupt *handler)
{
    struct Node *node;

    if (!keyboardbase || !handler)
    {
        return FALSE;
    }

    for (node = GETHEAD(&keyboardbase->kb_ResetHandlers); node; node = GETSUCC(node))
    {
        if (node == (struct Node *)handler)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static struct KeyboardUnit *keyboard_get_unit(struct IORequest *ioreq)
{
    if (!ioreq)
    {
        return NULL;
    }

    return (struct KeyboardUnit *)ioreq->io_Unit;
}

static void keyboard_clamp_read_seq(struct KeyboardBase *keyboardbase,
                                    struct KeyboardUnit *unit)
{
    ULONG oldest_seq;

    if (!keyboardbase || !unit)
    {
        return;
    }

    oldest_seq = (keyboardbase->kb_WriteSeq > KBD_EVENT_BUFFER_SIZE)
        ? (keyboardbase->kb_WriteSeq - KBD_EVENT_BUFFER_SIZE)
        : 0;

    if (unit->ku_ReadSeq < oldest_seq)
    {
        unit->ku_ReadSeq = oldest_seq;
    }
}

static LONG keyboard_fill_read(struct KeyboardBase *keyboardbase,
                               struct IOStdReq *io)
{
    struct KeyboardUnit *unit = keyboard_get_unit((struct IORequest *)io);
    struct InputEvent *events;
    ULONG available;
    ULONG max_events;
    ULONG count;
    ULONG idx;

    if (!keyboardbase || !io || !unit)
    {
        return IOERR_BADADDRESS;
    }

    if (!io->io_Data)
    {
        return IOERR_BADADDRESS;
    }

    max_events = io->io_Length / sizeof(struct InputEvent);
    if (max_events == 0)
    {
        return IOERR_BADLENGTH;
    }

    keyboard_clamp_read_seq(keyboardbase, unit);
    available = keyboardbase->kb_WriteSeq - unit->ku_ReadSeq;
    if (available == 0)
    {
        io->io_Actual = 0;
        return 1;
    }

    count = (available < max_events) ? available : max_events;
    events = (struct InputEvent *)io->io_Data;

    for (idx = 0; idx < count; idx++)
    {
        struct KeyboardEvent *src = &keyboardbase->kb_Events[(unit->ku_ReadSeq + idx) % KBD_EVENT_BUFFER_SIZE];

        memset(&events[idx], 0, sizeof(struct InputEvent));
        events[idx].ie_Class = IECLASS_RAWKEY;
        events[idx].ie_Code = src->ke_Code;
        events[idx].ie_Qualifier = src->ke_Qualifier;
        events[idx].ie_NextEvent = (idx + 1 < count) ? &events[idx + 1] : NULL;
    }

    unit->ku_ReadSeq += count;
    io->io_Actual = count * sizeof(struct InputEvent);
    return 0;
}

static void keyboard_reply_request(struct IORequest *ioreq)
{
    if (ioreq && !(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static void keyboard_complete_pending_reads(struct KeyboardBase *keyboardbase)
{
    struct KeyboardUnit *unit;

    if (!keyboardbase)
    {
        return;
    }

    for (unit = keyboardbase->kb_Units; unit; unit = unit->ku_Next)
    {
        struct IOStdReq *pending = unit->ku_PendingRead;
        LONG result;

        if (!pending)
        {
            continue;
        }

        result = keyboard_fill_read(keyboardbase, pending);
        if (result != 0)
        {
            if (result < 0)
            {
                pending->io_Error = result;
                pending->io_Actual = 0;
                unit->ku_PendingRead = NULL;
                keyboard_reply_request((struct IORequest *)pending);
            }
            continue;
        }

        pending->io_Error = 0;
        unit->ku_PendingRead = NULL;
        keyboard_reply_request((struct IORequest *)pending);
    }
}

static void keyboard_call_reset_handlers(struct KeyboardBase *keyboardbase)
{
    struct Interrupt *handler;
    struct Interrupt *next;

    if (!keyboardbase)
    {
        return;
    }

    for (handler = (struct Interrupt *)GETHEAD(&keyboardbase->kb_ResetHandlers);
         handler;
         handler = next)
    {
        typedef VOID (*ResetHandlerFn)(register APTR data __asm("a1"));
        ResetHandlerFn entry = (ResetHandlerFn)handler->is_Code;

        next = (struct Interrupt *)GETSUCC(handler);
        if (entry)
        {
            entry(handler->is_Data);
        }
    }
}

static void keyboard_trigger_reset(struct KeyboardBase *keyboardbase)
{
    struct Node *node;
    UWORD count = 0;

    if (!keyboardbase || keyboardbase->kb_ResetPhase)
    {
        return;
    }

    for (node = GETHEAD(&keyboardbase->kb_ResetHandlers); node; node = GETSUCC(node))
    {
        count++;
    }

    keyboardbase->kb_ResetPhase = TRUE;
    keyboardbase->kb_PendingResetAcks = count;

    if (count == 0)
    {
        keyboardbase->kb_ResetPhase = FALSE;
        return;
    }

    keyboard_call_reset_handlers(keyboardbase);
}

void _keyboard_device_record_event(UWORD rawkey, UWORD qualifier)
{
    struct KeyboardBase *keyboardbase = g_keyboard_base;
    struct KeyboardEvent *event;
    ULONG write_index;

    if (!keyboardbase)
    {
        return;
    }

    write_index = keyboardbase->kb_WriteSeq % KBD_EVENT_BUFFER_SIZE;
    event = &keyboardbase->kb_Events[write_index];
    event->ke_Code = (UBYTE)rawkey;
    event->ke_Qualifier = qualifier;
    if (keyboard_is_numeric_pad(rawkey))
    {
        event->ke_Qualifier |= IEQUALIFIER_NUMERICPAD;
    }

    keyboard_matrix_set(keyboardbase, rawkey);
    keyboardbase->kb_WriteSeq++;
    keyboard_complete_pending_reads(keyboardbase);

    if ((rawkey & ~IECODE_UP_PREFIX) == RAWKEY_RESET_WARNING && !(rawkey & IECODE_UP_PREFIX))
    {
        keyboard_trigger_reset(keyboardbase);
    }
}

static struct Library * __g_lxa_keyboard_InitDev(register struct Library *dev __asm("d0"),
                                                 register BPTR seglist __asm("a0"),
                                                 register struct ExecBase *sysb __asm("a6"))
{
    struct KeyboardBase *keyboardbase = (struct KeyboardBase *)dev;

    (void)sysb;

    keyboardbase->kb_SegList = seglist;
    keyboardbase->kb_Units = NULL;
    NEWLIST(&keyboardbase->kb_ResetHandlers);
    keyboardbase->kb_WriteSeq = 0;
    memset(keyboardbase->kb_Matrix, 0, sizeof(keyboardbase->kb_Matrix));
    keyboardbase->kb_ResetPhase = FALSE;
    keyboardbase->kb_PendingResetAcks = 0;
    g_keyboard_base = keyboardbase;

    return dev;
}

static void __g_lxa_keyboard_Open(register struct Library *dev __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"),
                                  register ULONG unitn __asm("d0"),
                                  register ULONG flags __asm("d1"))
{
    struct KeyboardBase *keyboardbase = (struct KeyboardBase *)dev;
    struct KeyboardUnit *unit;

    (void)unitn;
    (void)flags;

    unit = AllocVec(sizeof(struct KeyboardUnit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!unit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        ioreq->io_Device = NULL;
        ioreq->io_Unit = NULL;
        return;
    }

    unit->ku_ReadSeq = keyboardbase->kb_WriteSeq;
    unit->ku_PendingRead = NULL;
    unit->ku_Next = keyboardbase->kb_Units;
    keyboardbase->kb_Units = unit;

    ioreq->io_Error = 0;
    ioreq->io_Device = (struct Device *)dev;
    ioreq->io_Unit = (struct Unit *)unit;

    dev->lib_OpenCnt++;
    dev->lib_Flags &= ~LIBF_DELEXP;
}

static BPTR __g_lxa_keyboard_Close(register struct Library *dev __asm("a6"),
                                   register struct IORequest *ioreq __asm("a1"))
{
    struct KeyboardUnit *unit = keyboard_get_unit(ioreq);
    struct KeyboardBase *keyboardbase = (struct KeyboardBase *)dev;

    if (unit)
    {
        struct KeyboardUnit **link = &keyboardbase->kb_Units;

        while (*link && *link != unit)
        {
            link = &(*link)->ku_Next;
        }

        if (*link == unit)
        {
            *link = unit->ku_Next;
        }

        if (unit->ku_PendingRead == (struct IOStdReq *)ioreq)
        {
            unit->ku_PendingRead = NULL;
        }
        FreeVec(unit);
        ioreq->io_Unit = NULL;
    }

    if (dev->lib_OpenCnt > 0)
    {
        dev->lib_OpenCnt--;
    }

    return 0;
}

static BPTR __g_lxa_keyboard_Expunge(register struct Library *dev __asm("a6"))
{
    (void)dev;
    return 0;
}

static BPTR __g_lxa_keyboard_BeginIO(register struct Library *dev __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    struct KeyboardBase *keyboardbase = (struct KeyboardBase *)dev;
    struct IOStdReq *io = (struct IOStdReq *)ioreq;
    struct KeyboardUnit *unit = keyboard_get_unit(ioreq);
    LONG result = 0;

    ioreq->io_Error = 0;
    io->io_Actual = 0;

    switch (ioreq->io_Command)
    {
        case CMD_CLEAR:
            if (!unit)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            unit->ku_ReadSeq = keyboardbase->kb_WriteSeq;
            break;

        case KBD_READEVENT:
            result = keyboard_fill_read(keyboardbase, io);
            if (result > 0)
            {
                if (!unit)
                {
                    result = IOERR_BADADDRESS;
                    break;
                }
                unit->ku_PendingRead = io;
                ioreq->io_Flags &= ~IOF_QUICK;
                return 0;
            }
            break;

        case KBD_READMATRIX:
            if (!io->io_Data)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            io->io_Actual = (io->io_Length < KBD_MATRIX_BYTES) ? io->io_Length : KBD_MATRIX_BYTES;
            CopyMem(keyboardbase->kb_Matrix, io->io_Data, io->io_Actual);
            break;

        case KBD_ADDRESETHANDLER:
            if (!io->io_Data)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            if (!keyboard_reset_handler_is_linked(keyboardbase, (struct Interrupt *)io->io_Data))
            {
                ((struct Node *)io->io_Data)->ln_Succ = NULL;
                ((struct Node *)io->io_Data)->ln_Pred = NULL;
                Enqueue(&keyboardbase->kb_ResetHandlers, (struct Node *)io->io_Data);
            }
            break;

        case KBD_REMRESETHANDLER:
            if (!io->io_Data)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            if (keyboard_reset_handler_is_linked(keyboardbase, (struct Interrupt *)io->io_Data))
            {
                Remove((struct Node *)io->io_Data);
                ((struct Node *)io->io_Data)->ln_Succ = NULL;
                ((struct Node *)io->io_Data)->ln_Pred = NULL;
            }
            break;

        case KBD_RESETHANDLERDONE:
            if (!keyboardbase->kb_ResetPhase || keyboardbase->kb_PendingResetAcks == 0)
            {
                result = IOERR_NOCMD;
                break;
            }
            keyboardbase->kb_PendingResetAcks--;
            if (keyboardbase->kb_PendingResetAcks == 0)
            {
                keyboardbase->kb_ResetPhase = FALSE;
            }
            break;

        default:
            result = IOERR_NOCMD;
            break;
    }

    ioreq->io_Error = result;
    keyboard_reply_request(ioreq);
    return 0;
}

static ULONG __g_lxa_keyboard_AbortIO(register struct Library *dev __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"))
{
    struct KeyboardUnit *unit = keyboard_get_unit(ioreq);

    (void)dev;

    if (unit && unit->ku_PendingRead == (struct IOStdReq *)ioreq)
    {
        unit->ku_PendingRead = NULL;
        ioreq->io_Error = IOERR_ABORTED;
        ((struct IOStdReq *)ioreq)->io_Actual = 0;
        ReplyMsg(&ioreq->io_Message);
        return 0;
    }

    return (ULONG)-1;
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

extern APTR              __g_lxa_keyboard_FuncTab [];
extern struct MyDataInit __g_lxa_keyboard_DataTab;
extern struct InitTable  __g_lxa_keyboard_InitTab;
extern APTR              __g_lxa_keyboard_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_keyboard_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_keyboard_ExDevName[0],
    &_g_keyboard_ExDevID[0],
    &__g_lxa_keyboard_InitTab
};

APTR __g_lxa_keyboard_EndResident;
struct Resident *__lxa_keyboard_ROMTag = &ROMTag;

struct InitTable __g_lxa_keyboard_InitTab =
{
    (ULONG)sizeof(struct KeyboardBase),
    (APTR *)&__g_lxa_keyboard_FuncTab[0],
    (APTR)&__g_lxa_keyboard_DataTab,
    (APTR)__g_lxa_keyboard_InitDev
};

APTR __g_lxa_keyboard_FuncTab [] =
{
    __g_lxa_keyboard_Open,
    __g_lxa_keyboard_Close,
    __g_lxa_keyboard_Expunge,
    NULL,
    __g_lxa_keyboard_BeginIO,
    __g_lxa_keyboard_AbortIO,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_keyboard_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE)(ULONG)OFFSET(Node,    ln_Name), (ULONG)&_g_keyboard_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_keyboard_ExDevID[0],
    (ULONG)0
};
