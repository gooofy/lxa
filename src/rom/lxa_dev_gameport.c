/*
 * lxa gameport.device stub implementation
 *
 * Provides stub gameport.device for compatibility with applications that use joysticks/gamepads.
 * No actual gameport input is provided.
 */

#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include "util.h"

#define VERSION    37
#define REVISION   1
#define EXDEVNAME  "gameport"
#define EXDEVVER   " 37.1 (2025/02/02)"

char __aligned _g_gameport_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_gameport_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_gameport_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_gameport_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

/* Gameport device base */
struct GameportBase {
    struct Device  gb_Device;
    BPTR           gb_SegList;
};

/*
 * Device Init
 */
static struct Library * __g_lxa_gameport_InitDev  ( register struct Library    *dev     __asm("d0"),
                                                     register BPTR              seglist __asm("a0"),
                                                     register struct ExecBase  *sysb    __asm("a6"))
{
    struct GameportBase *gameportbase = (struct GameportBase *)dev;
    
    DPRINTF (LOG_DEBUG, "_gameport: InitDev() stub called\n");
    
    gameportbase->gb_SegList = seglist;
    
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
    DPRINTF (LOG_DEBUG, "_gameport: Open() stub called, unit=%lu flags=0x%08lx\n", unit, flags);
    
    /* Stub implementation - just set up the IORequest */
    ioreq->io_Error = 0;
    ioreq->io_Device = (struct Device *)dev;
    ioreq->io_Unit = NULL;  /* We don't allocate actual units */
    
    /* Update device open count */
    dev->lib_OpenCnt++;
    dev->lib_Flags &= ~LIBF_DELEXP;
}

/*
 * Device Close
 */
static BPTR __g_lxa_gameport_Close( register struct Library   *dev   __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_gameport: Close() stub called\n");
    
    /* Update device open count */
    dev->lib_OpenCnt--;
    
    return 0;
}

/*
 * Device Expunge
 */
static BPTR __g_lxa_gameport_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_gameport: Expunge() stub called\n");
    return 0;
}

/*
 * Device BeginIO
 */
static BPTR __g_lxa_gameport_BeginIO ( register struct Library   *dev   __asm("a6"),
                                        register struct IORequest *ioreq __asm("a1"))
{
    UWORD command = ioreq->io_Command;
    
    DPRINTF (LOG_DEBUG, "_gameport: BeginIO() stub called, command=%u\n", command);
    
    /* Stub implementation - return success but no data */
    ioreq->io_Error = 0;
    
    /* Mark request as done if not QUICK */
    if (!(ioreq->io_Flags & IOF_QUICK)) {
        ReplyMsg(&ioreq->io_Message);
    }
    
    return 0;
}

/*
 * Device AbortIO
 */
static ULONG __g_lxa_gameport_AbortIO ( register struct Library   *dev   __asm("a6"),
                                         register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_gameport: AbortIO() stub called\n");
    return IOERR_NOCMD;
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
    RTC_MATCHWORD,                   // UWORD rt_MatchWord;           0
    &ROMTag,                         // struct Resident *rt_MatchTag; 2
    &__g_lxa_gameport_EndResident,   // APTR  rt_EndSkip;             6
    RTF_AUTOINIT,                    // UBYTE rt_Flags;               10
    VERSION,                         // UBYTE rt_Version;             11
    NT_DEVICE,                       // UBYTE rt_Type;                12
    0,                               // BYTE  rt_Pri;                 13
    &_g_gameport_ExDevName[0],       // char  *rt_Name;               14
    &_g_gameport_ExDevID[0],         // char  *rt_IdString;           18
    &__g_lxa_gameport_InitTab        // APTR  rt_Init;                22
};

APTR __g_lxa_gameport_EndResident;
struct Resident *__lxa_gameport_ROMTag = &ROMTag;

struct InitTable __g_lxa_gameport_InitTab =
{
    (ULONG)               sizeof(struct GameportBase),     // DataSize
    (APTR              *) &__g_lxa_gameport_FuncTab[0],    // FunctionTable
    (APTR)                &__g_lxa_gameport_DataTab,       // DataTable
    (APTR)                __g_lxa_gameport_InitDev         // InitLibFn
};

APTR __g_lxa_gameport_FuncTab [] =
{
    __g_lxa_gameport_Open,
    __g_lxa_gameport_Close,
    __g_lxa_gameport_Expunge,
    0, /* Reserved */
    __g_lxa_gameport_BeginIO,
    __g_lxa_gameport_AbortIO,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_gameport_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,    ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_gameport_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library, lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library, lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library, lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_gameport_ExDevID[0],
    (ULONG) 0
};
