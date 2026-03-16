/*
 * lxa gameport.device implementation
 *
 * Provides a hosted gameport.device surface with controller-type,
 * trigger, request queuing, and expunge semantics.
 */

#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/gameport.h>
#include <devices/inputevent.h>
#include <devices/newstyle.h>

#include "util.h"

#define VERSION    37
#define REVISION   1
#define EXDEVNAME  "gameport"
#define EXDEVVER   " 37.1 (2025/02/02)"

#define GAMEPORT_UNIT_COUNT 2

char __aligned _g_gameport_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_gameport_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_gameport_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_gameport_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;
extern BOOL _input_device_add_handler(struct Interrupt *handler);
extern void _input_device_remove_handler(struct Interrupt *handler);

struct GameportUnit
{
    struct Unit gu_Unit;
    struct GameportUnit *gu_Next;
    UWORD gu_UnitNum;
    struct GamePortTrigger gu_Trigger;
    struct IOStdReq *gu_PendingRead;
    UWORD gu_LastQualifier;
    WORD gu_LastX;
    WORD gu_LastY;
};

struct GameportBase
{
    struct Device gb_Device;
    BPTR gb_SegList;
    struct List gb_PendingReads;
    struct GameportUnit *gb_Units;
    UBYTE gb_ControllerTypes[GAMEPORT_UNIT_COUNT];
    UWORD gb_OpenCounts[GAMEPORT_UNIT_COUNT];
    struct Interrupt gb_InputHandler;
    ULONG gb_TickCounter;
};

static const UWORD gameport_supported_commands[] =
{
    CMD_CLEAR,
    GPD_ASKCTYPE,
    GPD_SETCTYPE,
    GPD_ASKTRIGGER,
    GPD_SETTRIGGER,
    GPD_READEVENT,
    NSCMD_DEVICEQUERY,
    0
};

static void gameport_reply_request(struct IORequest *ioreq)
{
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static struct GameportUnit *gameport_find_unit(struct GameportBase *gameportbase,
                                               UWORD unit_num)
{
    struct GameportUnit *unit;

    if (!gameportbase)
    {
        return NULL;
    }

    for (unit = gameportbase->gb_Units; unit; unit = unit->gu_Next)
    {
        if (unit->gu_UnitNum == unit_num)
        {
            return unit;
        }
    }

    return NULL;
}

static void gameport_remove_unit(struct GameportBase *gameportbase,
                                 struct GameportUnit *gameportunit)
{
    struct GameportUnit **link;

    if (!gameportbase || !gameportunit)
    {
        return;
    }

    for (link = &gameportbase->gb_Units; *link; link = &(*link)->gu_Next)
    {
        if (*link == gameportunit)
        {
            *link = gameportunit->gu_Next;
            gameportunit->gu_Next = NULL;
            return;
        }
    }
}

static BOOL gameport_is_supported_ctype(LONG ctype)
{
    return ctype == GPCT_NOCONTROLLER ||
           ctype == GPCT_MOUSE ||
           ctype == GPCT_RELJOYSTICK ||
           ctype == GPCT_ABSJOYSTICK;
}

static void gameport_update_unit_state(struct GameportUnit *gameportunit,
                                       UWORD code,
                                       UWORD qualifier,
                                       WORD x,
                                       WORD y)
{
    if (!gameportunit)
    {
        return;
    }

    gameportunit->gu_LastQualifier = qualifier;
    if (code == IECODE_NOBUTTON || x != 0 || y != 0)
    {
        gameportunit->gu_LastX = x;
        gameportunit->gu_LastY = y;
    }
}

static BOOL gameport_trigger_matches(struct GameportBase *gameportbase,
                                     const struct GameportUnit *gameportunit,
                                     UWORD code,
                                     WORD x,
                                     WORD y)
{
    BOOL down;
    BOOL up;
    BOOL motion;
    BOOL triggered = FALSE;

    if (!gameportbase || !gameportunit)
    {
        return FALSE;
    }

    down = (code == IECODE_LBUTTON || code == IECODE_RBUTTON || code == IECODE_MBUTTON);
    up = ((code & IECODE_UP_PREFIX) != 0) &&
         (((code & ~IECODE_UP_PREFIX) == IECODE_LBUTTON) ||
          ((code & ~IECODE_UP_PREFIX) == IECODE_RBUTTON) ||
          ((code & ~IECODE_UP_PREFIX) == IECODE_MBUTTON));
    motion = (code == IECODE_NOBUTTON);

    if ((down && (gameportunit->gu_Trigger.gpt_Keys & GPTF_DOWNKEYS)) ||
        (up && (gameportunit->gu_Trigger.gpt_Keys & GPTF_UPKEYS)))
    {
        triggered = TRUE;
    }

    if (motion)
    {
        WORD dx = x - gameportunit->gu_LastX;
        WORD dy = y - gameportunit->gu_LastY;

        if (dx < 0)
            dx = -dx;
        if (dy < 0)
            dy = -dy;

        if ((gameportunit->gu_Trigger.gpt_XDelta != 0 && dx >= (WORD)gameportunit->gu_Trigger.gpt_XDelta) ||
            (gameportunit->gu_Trigger.gpt_YDelta != 0 && dy >= (WORD)gameportunit->gu_Trigger.gpt_YDelta))
        {
            triggered = TRUE;
        }
    }

    if (gameportunit->gu_Trigger.gpt_Timeout != 0 &&
        gameportbase->gb_TickCounter >= gameportunit->gu_Trigger.gpt_Timeout)
    {
        triggered = TRUE;
    }

    return triggered;
}

static void gameport_deliver_event(struct GameportBase *gameportbase,
                                   UWORD unit_num,
                                   UWORD code,
                                   UWORD qualifier,
                                   WORD x,
                                   WORD y)
{
    struct GameportUnit *gameportunit;
    struct IOStdReq *pending;
    struct InputEvent *event;

    if (!gameportbase)
    {
        return;
    }

    gameportunit = gameport_find_unit(gameportbase, unit_num);
    if (!gameportunit)
    {
        return;
    }

    pending = gameportunit->gu_PendingRead;
    if (pending && gameport_trigger_matches(gameportbase, gameportunit, code, x, y))
    {
        if (!pending->io_Data)
        {
            pending->io_Error = IOERR_BADADDRESS;
            pending->io_Actual = 0;
        }
        else if (pending->io_Length < sizeof(struct InputEvent))
        {
            pending->io_Error = IOERR_BADLENGTH;
            pending->io_Actual = 0;
        }
        else
        {
            event = (struct InputEvent *)pending->io_Data;
            memset(event, 0, sizeof(*event));
            event->ie_Class = IECLASS_RAWMOUSE;
            event->ie_SubClass = unit_num;
            event->ie_Code = code;
            event->ie_Qualifier = qualifier;
            event->ie_X = x;
            event->ie_Y = y;
            event->ie_NextEvent = NULL;
            event->ie_TimeStamp.tv_secs = gameportbase->gb_TickCounter;
            event->ie_TimeStamp.tv_micro = 0;
            pending->io_Error = 0;
            pending->io_Actual = sizeof(struct InputEvent);
        }

        Remove((struct Node *)pending);
        gameportunit->gu_PendingRead = NULL;
        gameport_reply_request((struct IORequest *)pending);
        gameportbase->gb_TickCounter = 0;
    }

    gameport_update_unit_state(gameportunit, code, qualifier, x, y);
}

static struct InputEvent *gameport_input_handler(register struct InputEvent *events __asm("a0"),
                                                 register APTR data __asm("a1"))
{
    struct GameportBase *gameportbase = (struct GameportBase *)data;
    struct InputEvent *event;

    if (!gameportbase)
    {
        return events;
    }

    gameportbase->gb_TickCounter++;

    for (event = events; event; event = event->ie_NextEvent)
    {
        if (event->ie_Class == IECLASS_RAWMOUSE || event->ie_Class == IECLASS_POINTERPOS)
        {
            UWORD code = (event->ie_Class == IECLASS_POINTERPOS) ? IECODE_NOBUTTON : event->ie_Code;
            gameport_deliver_event(gameportbase, 0, code, event->ie_Qualifier, event->ie_X, event->ie_Y);
        }
    }

    return events;
}

static BPTR gameport_expunge_if_possible(struct GameportBase *gameportbase)
{
    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)gameportbase->gb_Device.dd_Library.lib_Node.ln_Name) ==
        &gameportbase->gb_Device.dd_Library.lib_Node)
    {
        Remove(&gameportbase->gb_Device.dd_Library.lib_Node);
        DPRINTF(LOG_DEBUG, "_gameport: Expunge() removed device from DeviceList\n");
    }

    if (gameportbase->gb_Device.dd_Library.lib_OpenCnt != 0)
    {
        gameportbase->gb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        DPRINTF(LOG_DEBUG, "_gameport: Expunge() deferred, open count=%u\n",
                (unsigned int)gameportbase->gb_Device.dd_Library.lib_OpenCnt);
        return 0;
    }

    gameportbase->gb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
    _input_device_remove_handler(&gameportbase->gb_InputHandler);

    DPRINTF(LOG_DEBUG, "_gameport: Expunge() finalizing removal\n");

    return gameportbase->gb_SegList;
}

/*
 * Device Init
 */
static struct Library * __g_lxa_gameport_InitDev  ( register struct Library    *dev     __asm("d0"),
                                                     register BPTR              seglist __asm("a0"),
                                                     register struct ExecBase  *sysb    __asm("a6"))
{
    struct GameportBase *gameportbase = (struct GameportBase *)dev;
    ULONG unit;

    (void)sysb;

    DPRINTF(LOG_DEBUG, "_gameport: InitDev() called\n");

    gameportbase->gb_SegList = seglist;
    NEWLIST(&gameportbase->gb_PendingReads);
    gameportbase->gb_Units = NULL;
    gameportbase->gb_TickCounter = 0;

    gameportbase->gb_InputHandler.is_Node.ln_Type = NT_INTERRUPT;
    gameportbase->gb_InputHandler.is_Node.ln_Pri = 0;
    gameportbase->gb_InputHandler.is_Node.ln_Name = (char *)"gameport.device input";
    gameportbase->gb_InputHandler.is_Data = gameportbase;
    gameportbase->gb_InputHandler.is_Code = (VOID (*)())gameport_input_handler;

    _input_device_add_handler(&gameportbase->gb_InputHandler);

    for (unit = 0; unit < GAMEPORT_UNIT_COUNT; unit++)
    {
        gameportbase->gb_ControllerTypes[unit] = GPCT_NOCONTROLLER;
        gameportbase->gb_OpenCounts[unit] = 0;
    }

    return dev;
}

/*
 * Device Open
 */
static void __g_lxa_gameport_Open ( register struct Library   *dev   __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"),
                                     register ULONG             unit  __asm("d0"),
                                     register ULONG             flags __asm("d1"))
{
    struct GameportBase *gameportbase = (struct GameportBase *)dev;
    struct GameportUnit *gameportunit;

    (void)flags;

    DPRINTF(LOG_DEBUG, "_gameport: Open() called, unit=%lu flags=0x%08lx\n", unit, flags);

    ioreq->io_Error = 0;
    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    if (unit >= GAMEPORT_UNIT_COUNT || ioreq->io_Message.mn_Length < sizeof(struct IOStdReq))
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    gameportunit = (struct GameportUnit *)AllocMem(sizeof(*gameportunit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gameportunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    gameportunit->gu_Unit.unit_OpenCnt = 1;
    gameportunit->gu_Next = gameportbase->gb_Units;
    gameportunit->gu_UnitNum = (UWORD)unit;
    gameportunit->gu_Trigger.gpt_Keys = 0;
    gameportunit->gu_Trigger.gpt_Timeout = 0;
    gameportunit->gu_Trigger.gpt_XDelta = 0;
    gameportunit->gu_Trigger.gpt_YDelta = 0;
    gameportunit->gu_PendingRead = NULL;
    gameportunit->gu_LastQualifier = 0;
    gameportunit->gu_LastX = 0;
    gameportunit->gu_LastY = 0;

    gameportbase->gb_Units = gameportunit;

    ioreq->io_Error = 0;
    ioreq->io_Device = (struct Device *)gameportbase;
    ioreq->io_Unit = (struct Unit *)gameportunit;

    gameportbase->gb_OpenCounts[unit]++;
    gameportbase->gb_Device.dd_Library.lib_OpenCnt++;
    gameportbase->gb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
}

/*
 * Device Close
 */
static BPTR __g_lxa_gameport_Close( register struct Library   *dev   __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    struct GameportBase *gameportbase = (struct GameportBase *)dev;
    struct GameportUnit *gameportunit = (struct GameportUnit *)ioreq->io_Unit;
    UWORD unit = 0;

    DPRINTF(LOG_DEBUG, "_gameport: Close() called\n");

    if (gameportunit)
    {
        unit = gameportunit->gu_UnitNum;

        if (gameportunit->gu_PendingRead)
        {
            Remove((struct Node *)gameportunit->gu_PendingRead);
            gameportunit->gu_PendingRead = NULL;
        }

        if (unit < GAMEPORT_UNIT_COUNT && gameportbase->gb_OpenCounts[unit] > 0)
        {
            gameportbase->gb_OpenCounts[unit]--;
            if (gameportbase->gb_OpenCounts[unit] == 0)
            {
                gameportbase->gb_ControllerTypes[unit] = GPCT_NOCONTROLLER;
            }
        }

        gameport_remove_unit(gameportbase, gameportunit);
        FreeMem(gameportunit, sizeof(*gameportunit));
        ioreq->io_Unit = NULL;
    }

    ioreq->io_Device = NULL;

    if (gameportbase->gb_Device.dd_Library.lib_OpenCnt > 0)
    {
        gameportbase->gb_Device.dd_Library.lib_OpenCnt--;
    }

    if (gameportbase->gb_Device.dd_Library.lib_OpenCnt == 0 &&
        (gameportbase->gb_Device.dd_Library.lib_Flags & LIBF_DELEXP))
    {
        return gameport_expunge_if_possible(gameportbase);
    }

    return 0;
}

/*
 * Device Expunge
 */
static BPTR __g_lxa_gameport_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF(LOG_DEBUG, "_gameport: Expunge() called\n");
    return gameport_expunge_if_possible((struct GameportBase *)dev);
}

/*
 * Device BeginIO
 */
static BPTR __g_lxa_gameport_BeginIO ( register struct Library   *dev   __asm("a6"),
                                        register struct IORequest *ioreq __asm("a1"))
{
    struct GameportBase *gameportbase = (struct GameportBase *)dev;
    struct GameportUnit *gameportunit = (struct GameportUnit *)ioreq->io_Unit;
    struct IOStdReq *io = (struct IOStdReq *)ioreq;
    UWORD command = ioreq->io_Command;
    BOOL queued = FALSE;

    DPRINTF(LOG_DEBUG, "_gameport: BeginIO() called, command=%u\n", command);

    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->io_Error = 0;
    io->io_Actual = 0;

    if (!gameportunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        goto done;
    }

    switch (command)
    {
        case NSCMD_DEVICEQUERY:
        {
            struct NSDeviceQueryResult *result;

            if (io->io_Length < (LONG)sizeof(struct NSDeviceQueryResult))
            {
                ioreq->io_Error = IOERR_BADLENGTH;
                break;
            }

            if (!io->io_Data)
            {
                ioreq->io_Error = IOERR_BADADDRESS;
                break;
            }

            result = (struct NSDeviceQueryResult *)io->io_Data;
            result->nsdqr_DevQueryFormat = 0;
            result->nsdqr_SizeAvailable = sizeof(*result);
            result->nsdqr_DeviceType = NSDEVTYPE_GAMEPORT;
            result->nsdqr_DeviceSubType = 0;
            result->nsdqr_SupportedCommands = (APTR)gameport_supported_commands;
            io->io_Actual = sizeof(*result);
            break;
        }

        case CMD_CLEAR:
            gameportunit->gu_LastQualifier = 0;
            gameportunit->gu_LastX = 0;
            gameportunit->gu_LastY = 0;
            gameportbase->gb_TickCounter = 0;
            break;

        case GPD_ASKCTYPE:
            if (io->io_Length < sizeof(UBYTE))
            {
                ioreq->io_Error = IOERR_BADLENGTH;
                break;
            }

            if (!io->io_Data)
            {
                ioreq->io_Error = IOERR_BADADDRESS;
                break;
            }

            *((UBYTE *)io->io_Data) = gameportbase->gb_ControllerTypes[gameportunit->gu_UnitNum];
            io->io_Actual = sizeof(UBYTE);
            break;

        case GPD_SETCTYPE:
        {
            LONG ctype;

            if (io->io_Length != sizeof(UBYTE))
            {
                ioreq->io_Error = IOERR_BADLENGTH;
                break;
            }

            if (!io->io_Data)
            {
                ioreq->io_Error = IOERR_BADADDRESS;
                break;
            }

            ctype = *((BYTE *)io->io_Data);
            if (!gameport_is_supported_ctype(ctype))
            {
                ioreq->io_Error = GPDERR_SETCTYPE;
                break;
            }

            gameportbase->gb_ControllerTypes[gameportunit->gu_UnitNum] = (UBYTE)ctype;
            io->io_Actual = sizeof(UBYTE);
            break;
        }

        case GPD_ASKTRIGGER:
            if (io->io_Length != sizeof(struct GamePortTrigger))
            {
                ioreq->io_Error = IOERR_BADLENGTH;
                break;
            }

            if (!io->io_Data)
            {
                ioreq->io_Error = IOERR_BADADDRESS;
                break;
            }

            *((struct GamePortTrigger *)io->io_Data) = gameportunit->gu_Trigger;
            io->io_Actual = sizeof(struct GamePortTrigger);
            break;

        case GPD_SETTRIGGER:
            if (io->io_Length != sizeof(struct GamePortTrigger))
            {
                ioreq->io_Error = IOERR_BADLENGTH;
                break;
            }

            if (!io->io_Data)
            {
                ioreq->io_Error = IOERR_BADADDRESS;
                break;
            }

            gameportunit->gu_Trigger = *((struct GamePortTrigger *)io->io_Data);
            io->io_Actual = sizeof(struct GamePortTrigger);
            break;

        case GPD_READEVENT:
            if (!io->io_Data)
            {
                ioreq->io_Error = IOERR_BADADDRESS;
                break;
            }

            if (io->io_Length < sizeof(struct InputEvent))
            {
                ioreq->io_Error = IOERR_BADLENGTH;
                break;
            }

            if (gameportunit->gu_PendingRead)
            {
                ioreq->io_Error = IOERR_NOCMD;
                break;
            }

            ioreq->io_Flags &= ~IOF_QUICK;
            ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
            gameportunit->gu_PendingRead = io;
            AddTail(&gameportbase->gb_PendingReads, (struct Node *)io);
            queued = TRUE;
            break;

        default:
            ioreq->io_Error = IOERR_NOCMD;
            break;
    }

done:
    if (!queued)
    {
        gameport_reply_request(ioreq);
    }

    return 0;
}

/*
 * Device AbortIO
 */
static ULONG __g_lxa_gameport_AbortIO ( register struct Library   *dev   __asm("a6"),
                                         register struct IORequest *ioreq __asm("a1"))
{
    struct GameportUnit *gameportunit = (struct GameportUnit *)ioreq->io_Unit;

    (void)dev;

    DPRINTF(LOG_DEBUG, "_gameport: AbortIO() called\n");

    if (gameportunit && gameportunit->gu_PendingRead == (struct IOStdReq *)ioreq &&
        ioreq->io_Message.mn_Node.ln_Type == NT_MESSAGE)
    {
        Remove((struct Node *)ioreq);
        gameportunit->gu_PendingRead = NULL;
        ioreq->io_Error = IOERR_ABORTED;
        ReplyMsg(&ioreq->io_Message);
        return 0;
    }

    return (ULONG)-1;
}

/*
 * Device data tables and initialization
 */

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

extern APTR              __g_lxa_gameport_FuncTab [];
extern struct MyDataInit __g_lxa_gameport_DataTab;
extern struct InitTable  __g_lxa_gameport_InitTab;
extern APTR              __g_lxa_gameport_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_gameport_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_gameport_ExDevName[0],
    &_g_gameport_ExDevID[0],
    &__g_lxa_gameport_InitTab
};

APTR __g_lxa_gameport_EndResident;
struct Resident *__lxa_gameport_ROMTag = &ROMTag;

struct InitTable __g_lxa_gameport_InitTab =
{
    (ULONG)               sizeof(struct GameportBase),
    (APTR              *) &__g_lxa_gameport_FuncTab[0],
    (APTR)                &__g_lxa_gameport_DataTab,
    (APTR)                __g_lxa_gameport_InitDev
};

APTR __g_lxa_gameport_FuncTab [] =
{
    __g_lxa_gameport_Open,
    __g_lxa_gameport_Close,
    __g_lxa_gameport_Expunge,
    0,
    __g_lxa_gameport_BeginIO,
    __g_lxa_gameport_AbortIO,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_gameport_DataTab =
{
    INITBYTE(OFFSET(Node,    ln_Type),      NT_DEVICE),
    0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_gameport_ExDevName[0],
    INITBYTE(OFFSET(Library, lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    INITWORD(OFFSET(Library, lib_Version),  VERSION),
    INITWORD(OFFSET(Library, lib_Revision), REVISION),
    0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_gameport_ExDevID[0],
    (ULONG) 0
};
