#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/console.h>

#include "util.h"
#include "mem_map.h"

#define VERSION    40
#define REVISION   1
#define EXDEVNAME  "console"
#define EXDEVVER   " 40.1 (2022/03/07)"

char __aligned _g_console_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_console_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_console_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_console_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase      *SysBase;

static struct Library * __saveds __g_lxa_console_InitDev  ( register struct Library    *dev     __asm("d0"),
                                                          register BPTR               seglist __asm("a0"),
                                                          register struct ExecBase   *SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_console: WARNING: InitDev() unimplemented STUB called.\n");
    return dev;
}

static void __saveds __g_lxa_console_Open ( register struct Library   *dev   __asm("a6"),
                                            register struct IORequest *ioreq __asm("a1"),
                                            register ULONG            unitn  __asm("d0"),
                                            register ULONG            flags  __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_console: WARNING: Open() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds __g_lxa_console_Close( register struct Library   *dev   __asm("a6"),
                                          register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_console: WARNING: Close() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static BPTR __saveds __g_lxa_console_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_console: WARNING: Expunge() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static BPTR __saveds __g_lxa_console_BeginIO ( register struct Library   *dev   __asm("a6"),
                                             register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_console: WARNING: BeginIO() unimplemented STUB called, ioreq->io_Command=%d\n", ioreq->io_Command);
    assert(FALSE);
    return 0;
}

static ULONG __saveds __g_lxa_console_AbortIO ( register struct Library   *dev   __asm("a6"),
                                              register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_console: ERROR: AbortIO() unimplemented STUB called.\n");
    assert(FALSE);
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

