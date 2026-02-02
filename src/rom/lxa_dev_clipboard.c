/*
 * lxa clipboard.device implementation
 *
 * Provides clipboard (copy/paste) services for AmigaOS applications.
 * Implements a simple in-memory clipboard with support for multiple units.
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

#include <devices/clipboard.h>

#include "util.h"

/* Clipboard device commands are defined in devices/clipboard.h */

#define VERSION    37
#define REVISION   1
#define EXDEVNAME  "clipboard"
#define EXDEVVER   " 37.1 (2025/02/02)"

char __aligned _g_clipboard_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_clipboard_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_clipboard_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_clipboard_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

/* Maximum clipboard size (256KB should be enough for text) */
#define MAX_CLIP_SIZE 262144

/* Clipboard unit structure - one clipboard per unit */
struct ClipboardUnit {
    struct ClipboardUnitPartial cu_Partial;
    UBYTE  *cu_Data;           /* Clipboard data buffer */
    ULONG   cu_DataSize;       /* Current data size */
    ULONG   cu_DataAllocated;  /* Allocated buffer size */
    LONG    cu_ClipID;         /* Current clip identifier */
    LONG    cu_PostID;         /* ID for next post */
};

/* Clipboard device base */
struct ClipboardBase {
    struct Device  cb_Device;
    BPTR           cb_SegList;
};

/*
 * Device Init
 */
static struct Library * __g_lxa_clipboard_InitDev  ( register struct Library    *dev     __asm("d0"),
                                                      register BPTR              seglist __asm("a0"),
                                                      register struct ExecBase  *sysb    __asm("a6"))
{
    struct ClipboardBase *clipbase = (struct ClipboardBase *)dev;
    
    DPRINTF (LOG_DEBUG, "_clipboard: InitDev() called\n");
    
    clipbase->cb_SegList = seglist;
    
    return dev;
}

/*
 * Device Open
 */
static void __g_lxa_clipboard_Open ( register struct Library   *dev   __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"),
                                      register ULONG             unit  __asm("d0"),
                                      register ULONG             flags __asm("d1"))
{
    struct ClipboardBase *clipbase = (struct ClipboardBase *)dev;
    struct ClipboardUnit *clip_unit;
    
    DPRINTF (LOG_DEBUG, "_clipboard: Open() called, unit=%lu flags=0x%08lx\n", unit, flags);
    
    ioreq->io_Error = 0;
    
    /* For now, only support PRIMARY_CLIP (unit 0) */
    if (unit != PRIMARY_CLIP) {
        DPRINTF (LOG_ERROR, "_clipboard: Open() invalid unit %lu\n", unit);
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }
    
    /* Allocate unit structure */
    clip_unit = (struct ClipboardUnit *)AllocMem(sizeof(struct ClipboardUnit), MEMF_CLEAR | MEMF_PUBLIC);
    if (!clip_unit) {
        DPRINTF (LOG_ERROR, "_clipboard: Open() out of memory for unit\n");
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }
    
    /* Initialize unit */
    clip_unit->cu_Partial.cu_Node.ln_Type = NT_UNKNOWN;
    clip_unit->cu_Partial.cu_UnitNum = unit;
    clip_unit->cu_Data = NULL;
    clip_unit->cu_DataSize = 0;
    clip_unit->cu_DataAllocated = 0;
    clip_unit->cu_ClipID = 0;
    clip_unit->cu_PostID = 1;
    
    ioreq->io_Unit = (struct Unit *)&clip_unit->cu_Partial;
    ioreq->io_Device = (struct Device *)clipbase;
    
    /* Update device open count */
    clipbase->cb_Device.dd_Library.lib_OpenCnt++;
    clipbase->cb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
    
    DPRINTF (LOG_DEBUG, "_clipboard: Open() successful, unit=0x%08lx\n", (ULONG)clip_unit);
}

/*
 * Device Close
 */
static BPTR __g_lxa_clipboard_Close( register struct Library   *dev   __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"))
{
    struct ClipboardBase *clipbase = (struct ClipboardBase *)dev;
    struct ClipboardUnit *clip_unit = (struct ClipboardUnit *)ioreq->io_Unit;
    
    DPRINTF (LOG_DEBUG, "_clipboard: Close() called\n");
    
    if (clip_unit) {
        /* Free clipboard data buffer if allocated */
        if (clip_unit->cu_Data) {
            FreeMem(clip_unit->cu_Data, clip_unit->cu_DataAllocated);
        }
        
        FreeMem(clip_unit, sizeof(struct ClipboardUnit));
        ioreq->io_Unit = NULL;
    }
    
    clipbase->cb_Device.dd_Library.lib_OpenCnt--;
    
    return 0;
}

/*
 * Device Expunge
 */
static BPTR __g_lxa_clipboard_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_clipboard: Expunge() called (stub)\n");
    return 0;
}

/*
 * Device BeginIO - Execute a clipboard request
 */
static BPTR __g_lxa_clipboard_BeginIO ( register struct Library   *dev   __asm("a6"),
                                         register struct IORequest *ioreq __asm("a1"))
{
    struct IOClipReq *clipreq = (struct IOClipReq *)ioreq;
    struct ClipboardUnit *clip_unit = (struct ClipboardUnit *)ioreq->io_Unit;
    UWORD command = ioreq->io_Command;
    
    DPRINTF (LOG_DEBUG, "_clipboard: BeginIO() called, command=%u\n", command);
    
    ioreq->io_Error = 0;
    clipreq->io_Actual = 0;
    
    if (!clip_unit) {
        DPRINTF (LOG_ERROR, "_clipboard: BeginIO() NULL unit\n");
        ioreq->io_Error = IOERR_OPENFAIL;
        goto done;
    }
    
    switch (command) {
        case CMD_READ: {
            /* Read data from clipboard */
            ULONG offset = clipreq->io_Offset;
            ULONG length = clipreq->io_Length;
            STRPTR buffer = clipreq->io_Data;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_READ offset=%lu length=%lu clipID=%ld\n",
                     offset, length, clipreq->io_ClipID);
            
            /* Check if ClipID matches current clipboard */
            if (clipreq->io_ClipID != clip_unit->cu_ClipID) {
                DPRINTF (LOG_DEBUG, "_clipboard: CMD_READ obsolete ID (wanted %ld, have %ld)\n",
                         clipreq->io_ClipID, clip_unit->cu_ClipID);
                ioreq->io_Error = CBERR_OBSOLETEID;
                break;
            }
            
            /* Validate offset */
            if (offset > clip_unit->cu_DataSize) {
                offset = clip_unit->cu_DataSize;
            }
            
            /* Calculate bytes to read */
            ULONG available = clip_unit->cu_DataSize - offset;
            ULONG toread = (length < available) ? length : available;
            
            /* Copy data */
            if (toread > 0 && buffer && clip_unit->cu_Data) {
                CopyMem(clip_unit->cu_Data + offset, buffer, toread);
            }
            
            clipreq->io_Actual = toread;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_READ returned %lu bytes\n", toread);
            break;
        }
        
        case CMD_WRITE: {
            /* Write data to clipboard */
            ULONG offset = clipreq->io_Offset;
            ULONG length = clipreq->io_Length;
            STRPTR buffer = clipreq->io_Data;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_WRITE offset=%lu length=%lu\n", offset, length);
            
            /* Calculate required size */
            ULONG required = offset + length;
            
            if (required > MAX_CLIP_SIZE) {
                DPRINTF (LOG_ERROR, "_clipboard: CMD_WRITE size %lu exceeds max %lu\n",
                         required, (ULONG)MAX_CLIP_SIZE);
                ioreq->io_Error = IOERR_BADLENGTH;
                break;
            }
            
            /* Allocate or expand buffer if needed */
            if (required > clip_unit->cu_DataAllocated) {
                ULONG newsize = (required + 4095) & ~4095;  /* Round up to 4K */
                UBYTE *newbuf = AllocMem(newsize, MEMF_PUBLIC);
                
                if (!newbuf) {
                    DPRINTF (LOG_ERROR, "_clipboard: CMD_WRITE out of memory\n");
                    ioreq->io_Error = IOERR_NOCMD;  /* Best error code available */
                    break;
                }
                
                /* Copy existing data if any */
                if (clip_unit->cu_Data && clip_unit->cu_DataSize > 0) {
                    CopyMem(clip_unit->cu_Data, newbuf, clip_unit->cu_DataSize);
                }
                
                /* Free old buffer */
                if (clip_unit->cu_Data) {
                    FreeMem(clip_unit->cu_Data, clip_unit->cu_DataAllocated);
                }
                
                clip_unit->cu_Data = newbuf;
                clip_unit->cu_DataAllocated = newsize;
            }
            
            /* Copy data into clipboard */
            if (length > 0 && buffer) {
                CopyMem(buffer, clip_unit->cu_Data + offset, length);
            }
            
            /* Update size */
            if (required > clip_unit->cu_DataSize) {
                clip_unit->cu_DataSize = required;
            }
            
            clipreq->io_Actual = length;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_WRITE wrote %lu bytes, total size now %lu\n",
                     length, clip_unit->cu_DataSize);
            break;
        }
        
        case CMD_UPDATE: {
            /* Finish write transaction - assign new ClipID */
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_UPDATE\n");
            
            clip_unit->cu_ClipID = clip_unit->cu_PostID++;
            clipreq->io_ClipID = clip_unit->cu_ClipID;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_UPDATE new ClipID=%ld\n", clip_unit->cu_ClipID);
            break;
        }
        
        case CBD_CURRENTREADID: {
            /* Get current read ID */
            clipreq->io_ClipID = clip_unit->cu_ClipID;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CBD_CURRENTREADID -> %ld\n", clip_unit->cu_ClipID);
            break;
        }
        
        case CBD_CURRENTWRITEID: {
            /* Get current write ID (next ID to be assigned) */
            clipreq->io_ClipID = clip_unit->cu_PostID;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CBD_CURRENTWRITEID -> %ld\n", clip_unit->cu_PostID);
            break;
        }
        
        case CBD_POST:
        case CBD_CHANGEHOOK:
            /* Post notification and change hooks not implemented yet */
            DPRINTF (LOG_DEBUG, "_clipboard: command %u (stub)\n", command);
            ioreq->io_Error = 0;
            break;
        
        default:
            DPRINTF (LOG_ERROR, "_clipboard: BeginIO() unknown command %u\n", command);
            ioreq->io_Error = IOERR_NOCMD;
            break;
    }
    
done:
    /* Mark request as done if not QUICK */
    if (!(ioreq->io_Flags & IOF_QUICK)) {
        ReplyMsg(&ioreq->io_Message);
    }
    
    return 0;
}

/*
 * Device AbortIO - Abort a clipboard request
 */
static ULONG __g_lxa_clipboard_AbortIO ( register struct Library   *dev   __asm("a6"),
                                          register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_clipboard: AbortIO() called (stub)\n");
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

extern APTR              __g_lxa_clipboard_FuncTab [];
extern struct MyDataInit __g_lxa_clipboard_DataTab;
extern struct InitTable  __g_lxa_clipboard_InitTab;
extern APTR              __g_lxa_clipboard_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                   // UWORD rt_MatchWord;           0
    &ROMTag,                         // struct Resident *rt_MatchTag; 2
    &__g_lxa_clipboard_EndResident,  // APTR  rt_EndSkip;             6
    RTF_AUTOINIT,                    // UBYTE rt_Flags;               10
    VERSION,                         // UBYTE rt_Version;             11
    NT_DEVICE,                       // UBYTE rt_Type;                12
    0,                               // BYTE  rt_Pri;                 13
    &_g_clipboard_ExDevName[0],      // char  *rt_Name;               14
    &_g_clipboard_ExDevID[0],        // char  *rt_IdString;           18
    &__g_lxa_clipboard_InitTab       // APTR  rt_Init;                22
};

APTR __g_lxa_clipboard_EndResident;
struct Resident *__lxa_clipboard_ROMTag = &ROMTag;

struct InitTable __g_lxa_clipboard_InitTab =
{
    (ULONG)               sizeof(struct ClipboardBase),  // DataSize
    (APTR              *) &__g_lxa_clipboard_FuncTab[0], // FunctionTable
    (APTR)                &__g_lxa_clipboard_DataTab,    // DataTable
    (APTR)                __g_lxa_clipboard_InitDev      // InitLibFn
};

APTR __g_lxa_clipboard_FuncTab [] =
{
    __g_lxa_clipboard_Open,
    __g_lxa_clipboard_Close,
    __g_lxa_clipboard_Expunge,
    0, /* Reserved */
    __g_lxa_clipboard_BeginIO,
    __g_lxa_clipboard_AbortIO,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_clipboard_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,    ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_clipboard_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library, lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library, lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library, lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_clipboard_ExDevID[0],
    (ULONG) 0
};
