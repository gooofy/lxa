/*
 * lxa timer.device implementation
 *
 * Provides timing services for AmigaOS applications using the host system clock via emucalls.
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

#include <devices/timer.h>

#include "util.h"

/* Timer device commands are defined in devices/timer.h */

#define VERSION    40
#define REVISION   1
#define EXDEVNAME  "timer"
#define EXDEVVER   " 40.1 (2025/02/02)"

char __aligned _g_timer_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_timer_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_timer_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_timer_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

/* Timer device base */
struct TimerBase {
    struct Device  tb_Device;
    BPTR           tb_SegList;
    struct timeval tb_SystemTime;  /* Current system time */
};

/* Timer unit structure (one per open) */
struct TimerUnit {
    struct Unit    tu_Unit;
    ULONG          tu_EClock;      /* E-Clock frequency */
};

/*
 * Helper: Get current host time via emucall
 * EMU_CALL_GETSYSTIME takes pointer to struct timeval and fills it
 */
static void get_current_time(struct timeval *tv) {
    /* Use emucall to get time from host - pass pointer to timeval struct */
    emucall1(EMU_CALL_GETSYSTIME, (ULONG)tv);
}

/*
 * Device Init
 */
static struct Library * __g_lxa_timer_InitDev  ( register struct Library    *dev     __asm("d0"),
                                                 register BPTR              seglist __asm("a0"),
                                                 register struct ExecBase  *sysb    __asm("a6"))
{
    struct TimerBase *timerbase = (struct TimerBase *)dev;
    
    DPRINTF (LOG_DEBUG, "_timer: InitDev() called\n");
    
    timerbase->tb_SegList = seglist;
    
    /* Initialize system time */
    get_current_time(&timerbase->tb_SystemTime);
    
    DPRINTF (LOG_DEBUG, "_timer: InitDev() system time: %lu.%06lu\n",
             timerbase->tb_SystemTime.tv_secs, timerbase->tb_SystemTime.tv_micro);
    
    return dev;
}

/*
 * Device Open
 */
static void __g_lxa_timer_Open ( register struct Library   *dev   __asm("a6"),
                                 register struct IORequest *ioreq __asm("a1"),
                                 register ULONG             unit  __asm("d0"),
                                 register ULONG             flags __asm("d1"))
{
    struct TimerBase *timerbase = (struct TimerBase *)dev;
    struct TimerUnit *timer_unit;
    
    DPRINTF (LOG_DEBUG, "_timer: Open() called, unit=%lu flags=0x%08lx\n", unit, flags);
    
    ioreq->io_Error = 0;
    
    /* Validate unit */
    if (unit > UNIT_WAITECLOCK) {
        DPRINTF (LOG_ERROR, "_timer: Open() invalid unit %lu\n", unit);
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }
    
    /* Allocate unit structure */
    timer_unit = AllocMem(sizeof(struct TimerUnit), MEMF_CLEAR | MEMF_PUBLIC);
    if (!timer_unit) {
        DPRINTF (LOG_ERROR, "_timer: Open() out of memory\n");
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }
    
    /* Initialize unit */
    timer_unit->tu_Unit.unit_OpenCnt = 1;
    timer_unit->tu_EClock = 709379;  /* PAL E-Clock frequency (709379 Hz) */
    
    ioreq->io_Unit = (struct Unit *)timer_unit;
    ioreq->io_Device = (struct Device *)timerbase;
    
    /* Update device open count */
    timerbase->tb_Device.dd_Library.lib_OpenCnt++;
    timerbase->tb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
    
    DPRINTF (LOG_DEBUG, "_timer: Open() successful, unit=0x%08lx\n", (ULONG)timer_unit);
}

/*
 * Device Close
 */
static BPTR __g_lxa_timer_Close( register struct Library   *dev   __asm("a6"),
                                 register struct IORequest *ioreq __asm("a1"))
{
    struct TimerBase *timerbase = (struct TimerBase *)dev;
    struct TimerUnit *timer_unit = (struct TimerUnit *)ioreq->io_Unit;
    
    DPRINTF (LOG_DEBUG, "_timer: Close() called\n");
    
    if (timer_unit) {
        FreeMem(timer_unit, sizeof(struct TimerUnit));
        ioreq->io_Unit = NULL;
    }
    
    timerbase->tb_Device.dd_Library.lib_OpenCnt--;
    
    return 0;
}

/*
 * Device Expunge
 */
static BPTR __g_lxa_timer_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_timer: Expunge() called (stub)\n");
    return 0;
}

/*
 * Device BeginIO - Execute a timer request
 */
static BPTR __g_lxa_timer_BeginIO ( register struct Library   *dev   __asm("a6"),
                                    register struct IORequest *ioreq __asm("a1"))
{
    struct TimerBase *timerbase = (struct TimerBase *)dev;
    struct timerequest *tr = (struct timerequest *)ioreq;
    UWORD command = ioreq->io_Command;
    
    DPRINTF (LOG_DEBUG, "_timer: BeginIO() called, command=%u\n", command);
    
    ioreq->io_Error = 0;
    
    switch (command) {
        case TR_GETSYSTIME: {
            /* Get current system time */
            get_current_time(&tr->tr_time);
            
            DPRINTF (LOG_DEBUG, "_timer: TR_GETSYSTIME -> %lu.%06lu\n",
                     tr->tr_time.tv_secs, tr->tr_time.tv_micro);
            
            ioreq->io_Error = 0;
            break;
        }
        
        case TR_SETSYSTIME: {
            /* Set system time (for now, just log it) */
            DPRINTF (LOG_DEBUG, "_timer: TR_SETSYSTIME %lu.%06lu (ignored)\n",
                     tr->tr_time.tv_secs, tr->tr_time.tv_micro);
            
            timerbase->tb_SystemTime = tr->tr_time;
            ioreq->io_Error = 0;
            break;
        }
        
        case TR_ADDREQUEST: {
            /* Add a timer request (delay) */
            DPRINTF (LOG_DEBUG, "_timer: TR_ADDREQUEST delay=%lu.%06lu\n",
                     tr->tr_time.tv_secs, tr->tr_time.tv_micro);
            
            /* For now, implement as immediate completion (no actual delay) */
            /* TODO: Implement asynchronous timer queue with proper delays */
            
            ioreq->io_Error = 0;
            break;
        }
        
        case CMD_READ: {
            /* Read elapsed time since device was opened */
            struct timeval current_time;
            get_current_time(&current_time);
            
            tr->tr_time.tv_secs = current_time.tv_secs;
            tr->tr_time.tv_micro = current_time.tv_micro;
            
            DPRINTF (LOG_DEBUG, "_timer: CMD_READ -> %lu.%06lu\n",
                     tr->tr_time.tv_secs, tr->tr_time.tv_micro);
            
            ioreq->io_Error = 0;
            break;
        }
        
        case CMD_RESET:
        case CMD_CLEAR:
        case CMD_UPDATE:
        case CMD_FLUSH:
            /* These commands are no-ops for timer */
            DPRINTF (LOG_DEBUG, "_timer: command %u (no-op)\n", command);
            ioreq->io_Error = 0;
            break;
        
        default:
            DPRINTF (LOG_ERROR, "_timer: BeginIO() unknown command %u\n", command);
            ioreq->io_Error = IOERR_NOCMD;
            break;
    }
    
    /* Mark request as done if not QUICK */
    if (!(ioreq->io_Flags & IOF_QUICK)) {
        ReplyMsg(&ioreq->io_Message);
    }
    
    return 0;
}

/*
 * Device AbortIO - Abort a timer request
 */
static ULONG __g_lxa_timer_AbortIO ( register struct Library   *dev   __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_timer: AbortIO() called (stub)\n");
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

extern APTR              __g_lxa_timer_FuncTab [];
extern struct MyDataInit __g_lxa_timer_DataTab;
extern struct InitTable  __g_lxa_timer_InitTab;
extern APTR              __g_lxa_timer_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                // UWORD rt_MatchWord;           0
    &ROMTag,                      // struct Resident *rt_MatchTag; 2
    &__g_lxa_timer_EndResident,   // APTR  rt_EndSkip;             6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;               10
    VERSION,                      // UBYTE rt_Version;             11
    NT_DEVICE,                    // UBYTE rt_Type;                12
    0,                            // BYTE  rt_Pri;                 13
    &_g_timer_ExDevName[0],       // char  *rt_Name;               14
    &_g_timer_ExDevID[0],         // char  *rt_IdString;           18
    &__g_lxa_timer_InitTab        // APTR  rt_Init;                22
};

APTR __g_lxa_timer_EndResident;
struct Resident *__lxa_timer_ROMTag = &ROMTag;

struct InitTable __g_lxa_timer_InitTab =
{
    (ULONG)               sizeof(struct TimerBase),     // DataSize
    (APTR              *) &__g_lxa_timer_FuncTab[0],   // FunctionTable
    (APTR)                &__g_lxa_timer_DataTab,      // DataTable
    (APTR)                __g_lxa_timer_InitDev        // InitLibFn
};

APTR __g_lxa_timer_FuncTab [] =
{
    __g_lxa_timer_Open,
    __g_lxa_timer_Close,
    __g_lxa_timer_Expunge,
    0, /* Reserved */
    __g_lxa_timer_BeginIO,
    __g_lxa_timer_AbortIO,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_timer_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,    ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_timer_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library, lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library, lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library, lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_timer_ExDevID[0],
    (ULONG) 0
};
