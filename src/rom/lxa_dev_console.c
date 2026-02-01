#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/console.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXDEVNAME  "console"
#define EXDEVVER   " 40.1 (2022/03/07)"

char __aligned _g_console_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_console_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_console_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_console_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase      *SysBase;

static struct Library * __g_lxa_console_InitDev  ( register struct Library    *dev     __asm("d0"),
                                                          register BPTR               seglist __asm("a0"),
                                                          register struct ExecBase   *SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_console: InitDev() called.\n");
    return dev;
}

static void __g_lxa_console_Open ( register struct Library   *dev   __asm("a6"),
                                            register struct IORequest *ioreq __asm("a1"),
                                            register ULONG            unitn  __asm("d0"),
                                            register ULONG            flags  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_console: Open() called, unitn=%ld, flags=0x%08lx\n", unitn, flags);
    
    /* Accept the open request - the console is now "open" */
    ioreq->io_Error = 0;
    ioreq->io_Device = (struct Device *)dev;
    ioreq->io_Unit = (struct Unit *)(unitn + 1);  /* Non-NULL placeholder */
    
    /* Mark device as open */
    dev->lib_OpenCnt++;
    dev->lib_Flags &= ~LIBF_DELEXP;
}

static BPTR __g_lxa_console_Close( register struct Library   *dev   __asm("a6"),
                                          register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_console: Close() called\n");
    dev->lib_OpenCnt--;
    return 0;
}

static BPTR __g_lxa_console_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_console: Expunge() called - not expunging ROM device\n");
    return 0;
}

static BPTR __g_lxa_console_BeginIO ( register struct Library   *dev   __asm("a6"),
                                             register struct IORequest *ioreq __asm("a1"))
{
    struct IOStdReq *iostd = (struct IOStdReq *)ioreq;
    
    DPRINTF (LOG_DEBUG, "_console: BeginIO() cmd=%d, len=%ld, data=0x%08lx\n", 
             ioreq->io_Command, iostd->io_Length, (ULONG)iostd->io_Data);
    
    ioreq->io_Error = 0;
    iostd->io_Actual = 0;
    
    switch (ioreq->io_Command)
    {
        case CMD_READ:
            /* Console read - for now, return EOF/no data */
            DPRINTF (LOG_DEBUG, "_console: CMD_READ len=%ld\n", iostd->io_Length);
            iostd->io_Actual = 0;  /* No data available */
            break;
            
        case CMD_WRITE:
            /* Console write - for now, just accept and ignore */
            DPRINTF (LOG_DEBUG, "_console: CMD_WRITE len=%ld\n", iostd->io_Length);
            /* In a full implementation, we would write to the window here */
            iostd->io_Actual = iostd->io_Length;  /* Pretend we wrote everything */
            break;
            
        default:
            /* Unknown or unimplemented command */
            DPRINTF (LOG_DEBUG, "_console: BeginIO unknown cmd=%d\n", ioreq->io_Command);
            ioreq->io_Error = 0;  /* Accept anyway to avoid crashes */
            break;
    }
    
    /* If quick I/O was requested and we completed, return immediately */
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        /* Need to reply to the message */
        ReplyMsg(&ioreq->io_Message);
    }
    
    return 0;
}

static ULONG __g_lxa_console_AbortIO ( register struct Library   *dev   __asm("a6"),
                                              register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_console: AbortIO() called\n");
    return 0;  /* No pending I/O to abort */
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

extern APTR              __g_lxa_console_FuncTab [];
extern struct MyDataInit __g_lxa_console_DataTab;
extern struct InitTable  __g_lxa_console_InitTab;
extern APTR              __g_lxa_console_EndResident;

static struct Resident __aligned ROMTag =
{                               //                               offset
    RTC_MATCHWORD,              // UWORD rt_MatchWord;                0
    &ROMTag,                    // struct Resident *rt_MatchTag;      2
    &__g_lxa_console_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,               // UBYTE rt_Flags;                    7
    VERSION,                    // UBYTE rt_Version;                  8
    NT_DEVICE,                  // UBYTE rt_Type;                     9
    0,                          // BYTE  rt_Pri;                     10
    &_g_console_ExDevName[0],     // char  *rt_Name;                   14
    &_g_console_ExDevID[0],       // char  *rt_IdString;               18
    &__g_lxa_console_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_console_EndResident;
struct Resident *__lxa_console_ROMTag = &ROMTag;

struct InitTable __g_lxa_console_InitTab =
{
    (ULONG)               sizeof(struct Library),     // LibBaseSize
    (APTR              *) &__g_lxa_console_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_console_DataTab,     // DataTable
    (APTR)                __g_lxa_console_InitDev       // InitLibFn
};

APTR __g_lxa_console_FuncTab [] =
{
    __g_lxa_console_Open,
    __g_lxa_console_Close,
    __g_lxa_console_Expunge,
    NULL,
    __g_lxa_console_BeginIO,
    __g_lxa_console_AbortIO,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_console_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_console_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_console_ExDevID[0],
    (ULONG) 0
};
