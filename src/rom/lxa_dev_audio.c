/*
 * lxa audio.device stub implementation
 *
 * Provides stub audio.device for compatibility with applications that attempt to use it.
 * No actual audio playback is performed.
 */

#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/audio.h>

#include "util.h"

#define VERSION    37
#define REVISION   1
#define EXDEVNAME  "audio"
#define EXDEVVER   " 37.1 (2025/02/02)"

char __aligned _g_audio_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_audio_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_audio_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_audio_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

/* Audio device base */
struct AudioBase {
    struct Device  ab_Device;
    BPTR           ab_SegList;
};

/*
 * Device Init
 */
static struct Library * __g_lxa_audio_InitDev  ( register struct Library    *dev     __asm("d0"),
                                                  register BPTR              seglist __asm("a0"),
                                                  register struct ExecBase  *sysb    __asm("a6"))
{
    struct AudioBase *audiobase = (struct AudioBase *)dev;
    
    DPRINTF (LOG_DEBUG, "_audio: InitDev() stub called\n");
    
    audiobase->ab_SegList = seglist;
    
    return dev;
}

/*
 * Device Open
 */
static void __g_lxa_audio_Open ( register struct Library   *dev   __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"),
                                  register ULONG             unit  __asm("d0"),
                                  register ULONG             flags __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_audio: Open() stub called, unit=%lu flags=0x%08lx\n", unit, flags);
    
    /* Stub implementation - just set error to indicate no allocation */
    ioreq->io_Error = ADIOERR_NOALLOCATION;
    ioreq->io_Unit = NULL;
    ioreq->io_Device = NULL;
    
    /* Note: We don't increment open count on failure */
}

/*
 * Device Close
 */
static BPTR __g_lxa_audio_Close( register struct Library   *dev   __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_audio: Close() stub called\n");
    
    /* Nothing to do for stub */
    return 0;
}

/*
 * Device Expunge
 */
static BPTR __g_lxa_audio_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_audio: Expunge() stub called\n");
    return 0;
}

/*
 * Device BeginIO
 */
static BPTR __g_lxa_audio_BeginIO ( register struct Library   *dev   __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    UWORD command = ioreq->io_Command;
    
    DPRINTF (LOG_DEBUG, "_audio: BeginIO() stub called, command=%u\n", command);
    
    /* Stub implementation - return error for all commands */
    ioreq->io_Error = IOERR_NOCMD;
    
    /* Mark request as done if not QUICK */
    if (!(ioreq->io_Flags & IOF_QUICK)) {
        ReplyMsg(&ioreq->io_Message);
    }
    
    return 0;
}

/*
 * Device AbortIO
 */
static ULONG __g_lxa_audio_AbortIO ( register struct Library   *dev   __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_audio: AbortIO() stub called\n");
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

extern APTR              __g_lxa_audio_FuncTab [];
extern struct MyDataInit __g_lxa_audio_DataTab;
extern struct InitTable  __g_lxa_audio_InitTab;
extern APTR              __g_lxa_audio_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                // UWORD rt_MatchWord;           0
    &ROMTag,                      // struct Resident *rt_MatchTag; 2
    &__g_lxa_audio_EndResident,   // APTR  rt_EndSkip;             6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;               10
    VERSION,                      // UBYTE rt_Version;             11
    NT_DEVICE,                    // UBYTE rt_Type;                12
    0,                            // BYTE  rt_Pri;                 13
    &_g_audio_ExDevName[0],       // char  *rt_Name;               14
    &_g_audio_ExDevID[0],         // char  *rt_IdString;           18
    &__g_lxa_audio_InitTab        // APTR  rt_Init;                22
};

APTR __g_lxa_audio_EndResident;
struct Resident *__lxa_audio_ROMTag = &ROMTag;

struct InitTable __g_lxa_audio_InitTab =
{
    (ULONG)               sizeof(struct AudioBase),     // DataSize
    (APTR              *) &__g_lxa_audio_FuncTab[0],    // FunctionTable
    (APTR)                &__g_lxa_audio_DataTab,       // DataTable
    (APTR)                __g_lxa_audio_InitDev         // InitLibFn
};

APTR __g_lxa_audio_FuncTab [] =
{
    __g_lxa_audio_Open,
    __g_lxa_audio_Close,
    __g_lxa_audio_Expunge,
    0, /* Reserved */
    __g_lxa_audio_BeginIO,
    __g_lxa_audio_AbortIO,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_audio_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,    ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_audio_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library, lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library, lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library, lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_audio_ExDevID[0],
    (ULONG) 0
};
