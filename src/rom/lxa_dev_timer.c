/*
 * lxa timer.device implementation
 *
 * Provides timing services for AmigaOS applications using the host system clock via emucalls.
 *
 * Phase 45: Async I/O & Timer Completion
 * - TR_ADDREQUEST now properly queues requests with host-side timer
 * - AbortIO can cancel pending timer requests
 * - Supports all timer units (MICROHZ, VBLANK, ECLOCK, WAITUNTIL, WAITECLOCK)
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
#define REVISION   2
#define EXDEVNAME  "timer"
#define EXDEVVER   " 40.2 (2025/02/03)"

char __aligned _g_timer_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_timer_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_timer_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_timer_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

/*
 * Timer request node - used to track pending timer requests
 * This is stored in a list maintained by TimerBase
 */
struct TimerRequest
{
    struct MinNode tr_Node;            /* Node for linking in the timer list */
    struct timerequest *tr_Request;    /* The actual timer request */
    ULONG tr_WakeTime_Secs;            /* Absolute wake time (seconds) */
    ULONG tr_WakeTime_Micro;           /* Absolute wake time (microseconds) */
};

/* Timer device base */
struct TimerBase
{
    struct Device  tb_Device;
    BPTR           tb_SegList;
    struct timeval tb_SystemTime;      /* Current system time */
    struct MinList tb_TimerList;       /* List of pending timer requests */
    ULONG          tb_EClock;          /* E-Clock frequency (709379 Hz for PAL) */
};

/* Timer unit structure (one per open) */
struct TimerUnit
{
    struct Unit    tu_Unit;
    ULONG          tu_UnitNum;         /* Unit number (MICROHZ, VBLANK, etc.) */
};

/*
 * Helper: Get current host time via emucall
 * EMU_CALL_GETSYSTIME takes pointer to struct timeval and fills it
 */
static void get_current_time(struct timeval *tv)
{
    /* Use emucall to get time from host - pass pointer to timeval struct */
    emucall1(EMU_CALL_GETSYSTIME, (ULONG)tv);
}

/*
 * Helper: Add timer request to host-side timer queue
 * Returns TRUE on success
 */
static BOOL timer_add_request(struct timerequest *tr, ULONG secs, ULONG micros)
{
    return emucall3(EMU_CALL_TIMER_ADD, (ULONG)tr, secs, micros) != 0;
}

/*
 * Helper: Remove timer request from host-side timer queue
 * Returns TRUE if request was found and removed
 */
static BOOL timer_remove_request(struct timerequest *tr)
{
    return emucall1(EMU_CALL_TIMER_REMOVE, (ULONG)tr) != 0;
}

/*
 * Helper: Get next expired timer IORequest from host
 * Returns the m68k IORequest pointer, or NULL if none available
 */
static struct timerequest *timer_get_expired(void)
{
    return (struct timerequest *)emucall0(EMU_CALL_TIMER_GET_EXPIRED);
}

/*
 * Helper: Check for expired timers on host side
 * This marks any timers whose wake time has passed.
 */
static void timer_check_expired(void)
{
    ULONG result = emucall0(EMU_CALL_TIMER_CHECK);
    DPRINTF(LOG_DEBUG, "_timer: timer_check_expired() emucall returned %lu\n", result);
}

/*
 * VBlank Hook for Timer Device
 * 
 * Called from the VBlank interrupt handler to process expired timer requests.
 * This function:
 * 1. Checks for expired timers on the host side
 * 2. For each expired timer, calls ReplyMsg() to wake up the waiting task
 * 
 * This must be called from the m68k side (not host) so that ReplyMsg() properly
 * signals the task through the Exec scheduler.
 */
VOID _timer_VBlankHook(void)
{
    struct timerequest *tr;
    
    /* First, mark any newly expired timers */
    DPRINTF(LOG_DEBUG, "_timer: VBlankHook: calling timer_check_expired()\n");
    timer_check_expired();
    
    /* Process all expired timers */
    while ((tr = timer_get_expired()) != NULL)
    {
        DPRINTF(LOG_DEBUG, "_timer: VBlankHook: completing request 0x%08lx\n", (ULONG)tr);
        
        /* Set error to 0 (success) */
        tr->tr_node.io_Error = 0;
        
        /* Reply to the message - this wakes up the waiting task */
        ReplyMsg(&tr->tr_node.io_Message);
    }
}

/*
 * Helper: Add two timevals: result = a + b
 */
static void timeval_add(struct timeval *result, struct timeval *a, struct timeval *b)
{
    result->tv_secs = a->tv_secs + b->tv_secs;
    result->tv_micro = a->tv_micro + b->tv_micro;
    
    /* Handle microsecond overflow */
    if (result->tv_micro >= 1000000)
    {
        result->tv_secs++;
        result->tv_micro -= 1000000;
    }
}

/*
 * Helper: Subtract two timevals: result = a - b
 * Returns TRUE if result is positive (a > b)
 */
static BOOL timeval_sub(struct timeval *result, struct timeval *a, struct timeval *b)
{
    if (a->tv_secs > b->tv_secs || 
        (a->tv_secs == b->tv_secs && a->tv_micro >= b->tv_micro))
    {
        result->tv_secs = a->tv_secs - b->tv_secs;
        if (a->tv_micro >= b->tv_micro)
        {
            result->tv_micro = a->tv_micro - b->tv_micro;
        }
        else
        {
            result->tv_secs--;
            result->tv_micro = 1000000 + a->tv_micro - b->tv_micro;
        }
        return TRUE;
    }
    else
    {
        /* b > a, result is negative */
        result->tv_secs = 0;
        result->tv_micro = 0;
        return FALSE;
    }
}

/*
 * Helper: Compare two timevals
 * Returns: -1 if a < b, 0 if a == b, 1 if a > b
 */
static LONG timeval_cmp(struct timeval *a, struct timeval *b)
{
    if (a->tv_secs < b->tv_secs) return -1;
    if (a->tv_secs > b->tv_secs) return 1;
    if (a->tv_micro < b->tv_micro) return -1;
    if (a->tv_micro > b->tv_micro) return 1;
    return 0;
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
    timerbase->tb_EClock = 709379;  /* PAL E-Clock frequency */
    
    /* Initialize the timer request list */
    timerbase->tb_TimerList.mlh_Head = (struct MinNode *)&timerbase->tb_TimerList.mlh_Tail;
    timerbase->tb_TimerList.mlh_Tail = NULL;
    timerbase->tb_TimerList.mlh_TailPred = (struct MinNode *)&timerbase->tb_TimerList.mlh_Head;
    
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
    timer_unit->tu_UnitNum = unit;
    
    ioreq->io_Unit = (struct Unit *)timer_unit;
    ioreq->io_Device = (struct Device *)timerbase;
    
    /* Update device open count */
    timerbase->tb_Device.dd_Library.lib_OpenCnt++;
    timerbase->tb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
    
    DPRINTF (LOG_DEBUG, "_timer: Open() successful, unit=0x%08lx (unitnum=%lu)\n", 
             (ULONG)timer_unit, unit);
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
    struct TimerUnit *timer_unit = (struct TimerUnit *)ioreq->io_Unit;
    struct timerequest *tr = (struct timerequest *)ioreq;
    UWORD command = ioreq->io_Command;
    BOOL async_request = FALSE;
    
    DPRINTF (LOG_DEBUG, "_timer: BeginIO() called, command=%u unit=%lu\n", 
             command, timer_unit ? timer_unit->tu_UnitNum : 0xFFFFFFFF);
    
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
            /* Add a timer request - this is an async operation */
            struct timeval current_time;
            struct timeval wake_time;
            ULONG delay_secs = tr->tr_time.tv_secs;
            ULONG delay_micro = tr->tr_time.tv_micro;
            
            DPRINTF (LOG_DEBUG, "_timer: TR_ADDREQUEST delay=%lu.%06lu flags=0x%02x\n",
                     delay_secs, delay_micro, ioreq->io_Flags);
            
            /* Get current time */
            get_current_time(&current_time);
            
            /* Handle different unit types */
            if (timer_unit) {
                switch (timer_unit->tu_UnitNum) {
                    case UNIT_VBLANK:
                        /* UNIT_VBLANK: delay is in VBlank ticks (1/50th second for PAL)
                         * Convert to actual time */
                        delay_secs = delay_secs / 50;
                        delay_micro = (delay_secs % 50) * 20000 + delay_micro;
                        if (delay_micro >= 1000000) {
                            delay_secs++;
                            delay_micro -= 1000000;
                        }
                        /* Minimum delay of 1 VBlank tick */
                        if (delay_secs == 0 && delay_micro < 20000) {
                            delay_micro = 20000;
                        }
                        break;
                        
                    case UNIT_ECLOCK:
                    case UNIT_WAITECLOCK:
                        /* UNIT_ECLOCK: delay is in E-Clock ticks (709379 Hz for PAL)
                         * Convert to microseconds */
                        {
                            ULONG total_ticks = delay_secs * timerbase->tb_EClock + delay_micro;
                            delay_secs = total_ticks / timerbase->tb_EClock;
                            delay_micro = (total_ticks % timerbase->tb_EClock) * 1000000 / timerbase->tb_EClock;
                        }
                        break;
                        
                    case UNIT_WAITUNTIL:
                        /* UNIT_WAITUNTIL: tr_time is absolute time to wait until */
                        {
                            wake_time.tv_secs = tr->tr_time.tv_secs;
                            wake_time.tv_micro = tr->tr_time.tv_micro;
                            
                            /* Calculate delay from now until wake time */
                            if (!timeval_sub(&tr->tr_time, &wake_time, &current_time)) {
                                /* Wake time already passed, complete immediately */
                                DPRINTF(LOG_DEBUG, "_timer: UNIT_WAITUNTIL - time already passed\n");
                                tr->tr_time.tv_secs = 0;
                                tr->tr_time.tv_micro = 0;
                                break;
                            }
                            delay_secs = tr->tr_time.tv_secs;
                            delay_micro = tr->tr_time.tv_micro;
                        }
                        break;
                        
                    case UNIT_MICROHZ:
                    default:
                        /* UNIT_MICROHZ: delay is in seconds + microseconds (standard) */
                        break;
                }
            }
            
            /* Check if delay is essentially zero */
            if (delay_secs == 0 && delay_micro == 0) {
                /* No delay, complete immediately */
                DPRINTF(LOG_DEBUG, "_timer: Zero delay, completing immediately\n");
                ioreq->io_Error = 0;
                break;
            }
            
            /* Add to host-side timer queue with DELAY (not absolute wake time).
             * The host will compute the wake time by adding the delay to current host time.
             * This avoids time synchronization issues between Amiga and Unix epochs.
             */
            if (timer_add_request(tr, delay_secs, delay_micro)) {
                /* Request queued successfully - mark as async */
                async_request = TRUE;
                DPRINTF(LOG_DEBUG, "_timer: TR_ADDREQUEST queued, delay %lu.%06lu\n",
                        delay_secs, delay_micro);
            } else {
                /* Failed to queue - complete with error */
                LPRINTF(LOG_ERROR, "_timer: Failed to queue TR_ADDREQUEST\n");
                ioreq->io_Error = IOERR_NOCMD;
            }
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
    
    /* For async requests, don't reply yet - wait for timer to expire */
    if (async_request) {
        /* Clear QUICK flag to indicate request is pending */
        ioreq->io_Flags &= ~IOF_QUICK;
        return 0;
    }
    
    /* Mark request as done if not QUICK */
    if (!(ioreq->io_Flags & IOF_QUICK)) {
        ReplyMsg(&ioreq->io_Message);
    }
    
    return 0;
}

/*
 * Device AbortIO - Abort a timer request
 * 
 * This function removes a pending timer request from the queue.
 * Returns 0 if the request was successfully aborted, or an error code.
 */
static ULONG __g_lxa_timer_AbortIO ( register struct Library   *dev   __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    struct timerequest *tr = (struct timerequest *)ioreq;
    
    DPRINTF (LOG_DEBUG, "_timer: AbortIO() called for request 0x%08lx\n", (ULONG)tr);
    
    /* Try to remove from host-side timer queue */
    if (timer_remove_request(tr)) {
        /* Successfully removed - mark as aborted */
        ioreq->io_Error = IOERR_ABORTED;
        
        /* Reply to the message */
        ReplyMsg(&ioreq->io_Message);
        
        DPRINTF(LOG_DEBUG, "_timer: AbortIO() successfully aborted request\n");
        return 0;
    }
    
    /* Request not found - might have already completed */
    DPRINTF(LOG_DEBUG, "_timer: AbortIO() request not found (may have completed)\n");
    return IOERR_NOCMD;
}

/*
 * Timer utility functions (library functions)
 * These are accessed via LVO offsets from the device base
 */

/*
 * AddTime - Add two timevals: dest = dest + src
 * LVO: -42
 */
static void __g_lxa_timer_AddTime (
    register struct Library   *dev   __asm("a6"),
    register struct timeval   *dest  __asm("a0"),
    register struct timeval   *src   __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_timer: AddTime(%lu.%06lu + %lu.%06lu)\n",
            dest->tv_secs, dest->tv_micro, src->tv_secs, src->tv_micro);
    
    timeval_add(dest, dest, src);
    
    DPRINTF(LOG_DEBUG, "_timer: AddTime -> %lu.%06lu\n", dest->tv_secs, dest->tv_micro);
}

/*
 * SubTime - Subtract two timevals: dest = dest - src
 * LVO: -48
 */
static void __g_lxa_timer_SubTime (
    register struct Library   *dev   __asm("a6"),
    register struct timeval   *dest  __asm("a0"),
    register struct timeval   *src   __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_timer: SubTime(%lu.%06lu - %lu.%06lu)\n",
            dest->tv_secs, dest->tv_micro, src->tv_secs, src->tv_micro);
    
    timeval_sub(dest, dest, src);
    
    DPRINTF(LOG_DEBUG, "_timer: SubTime -> %lu.%06lu\n", dest->tv_secs, dest->tv_micro);
}

/*
 * CmpTime - Compare two timevals
 * LVO: -54
 * Returns: -1 if dest < src, 0 if equal, 1 if dest > src
 */
static LONG __g_lxa_timer_CmpTime (
    register struct Library   *dev   __asm("a6"),
    register struct timeval   *dest  __asm("a0"),
    register struct timeval   *src   __asm("a1"))
{
    LONG result = timeval_cmp(dest, src);
    
    DPRINTF(LOG_DEBUG, "_timer: CmpTime(%lu.%06lu vs %lu.%06lu) -> %ld\n",
            dest->tv_secs, dest->tv_micro, src->tv_secs, src->tv_micro, result);
    
    return result;
}

/*
 * ReadEClock - Read the E-Clock value
 * LVO: -60
 * Returns E-Clock frequency, stores current E-Clock value in dest
 */
static ULONG __g_lxa_timer_ReadEClock (
    register struct Library   *dev   __asm("a6"),
    register struct EClockVal *dest  __asm("a0"))
{
    struct TimerBase *timerbase = (struct TimerBase *)dev;
    struct timeval current;
    
    get_current_time(&current);
    
    /* Convert time to E-Clock ticks */
    /* E-Clock = 709379 Hz for PAL */
    ULONG total_ticks = current.tv_secs * timerbase->tb_EClock +
                        (current.tv_micro * timerbase->tb_EClock / 1000000);
    
    dest->ev_hi = 0;  /* High 32 bits (we don't track overflow) */
    dest->ev_lo = total_ticks;
    
    DPRINTF(LOG_DEBUG, "_timer: ReadEClock -> %lu Hz, value=%lu\n", 
            timerbase->tb_EClock, total_ticks);
    
    return timerbase->tb_EClock;
}

/*
 * GetSysTime - Get current system time
 * LVO: -66
 */
static void __g_lxa_timer_GetSysTime (
    register struct Library   *dev   __asm("a6"),
    register struct timeval   *dest  __asm("a0"))
{
    get_current_time(dest);
    
    DPRINTF(LOG_DEBUG, "_timer: GetSysTime -> %lu.%06lu\n", dest->tv_secs, dest->tv_micro);
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
    /* Timer utility functions (library functions) */
    __g_lxa_timer_AddTime,      /* -42 */
    __g_lxa_timer_SubTime,      /* -48 */
    __g_lxa_timer_CmpTime,      /* -54 */
    __g_lxa_timer_ReadEClock,   /* -60 */
    __g_lxa_timer_GetSysTime,   /* -66 */
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
