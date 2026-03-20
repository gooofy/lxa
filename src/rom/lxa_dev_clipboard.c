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
#include <utility/hooks.h>
#include <clib/exec_protos.h>
#include <clib/utility_protos.h>
#include <inline/exec.h>
#include <inline/utility.h>

#include <devices/clipboard.h>

#include "util.h"

/* Clipboard device commands are defined in devices/clipboard.h */

#define VERSION    37
#define REVISION   1
#define EXDEVNAME  "clipboard"
#define EXDEVVER   " 37.1 (2025/02/02)"

char __aligned _g_clipboard_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_clipboard_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_clipboard_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_clipboard_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;
extern struct UtilityBase *UtilityBase;

/* Maximum clipboard size (256KB should be enough for text) */
#define MAX_CLIP_SIZE 262144

/* Clipboard unit structure - one clipboard per unit */
struct ClipboardUnit {
    struct ClipboardUnitPartial cu_Partial;
    ULONG   cu_OpenCnt;
    UBYTE  *cu_Data;
    ULONG   cu_DataSize;
    ULONG   cu_DataAllocated;
    LONG    cu_DataClipID;
    UBYTE  *cu_WriteData;
    ULONG   cu_WriteSize;
    ULONG   cu_WriteAllocated;
    LONG    cu_WriteClipID;
    LONG    cu_CurrentClipID;
    LONG    cu_NextClipID;
    struct List cu_ChangeHooks;
    BOOL    cu_PostActive;
    struct MsgPort *cu_SatisfyPort;
    struct SatisfyMsg cu_SatisfyMsg;
    BOOL    cu_SatisfySent;
    struct IOClipReq *cu_PendingRead;
};

struct ClipboardHookNode {
    struct Node chn_Node;
    struct Hook *chn_Hook;
};

/* Clipboard device base */
struct ClipboardBase {
    struct Device  cb_Device;
    BPTR           cb_SegList;
    struct ClipboardUnit *cb_PrimaryUnit;
};

#define CLIPBOARD_READ_PENDING ((LONG)1)

static void clipboard_reset_staged_write(struct ClipboardUnit *clip_unit)
{
    if (clip_unit->cu_WriteData)
    {
        FreeMem(clip_unit->cu_WriteData, clip_unit->cu_WriteAllocated);
        clip_unit->cu_WriteData = NULL;
    }

    clip_unit->cu_WriteSize = 0;
    clip_unit->cu_WriteAllocated = 0;
    clip_unit->cu_WriteClipID = 0;
}

static void clipboard_free_hooks(struct ClipboardUnit *clip_unit)
{
    struct ClipboardHookNode *node;
    struct ClipboardHookNode *next;

    node = (struct ClipboardHookNode *)clip_unit->cu_ChangeHooks.lh_Head;
    while (node && node->chn_Node.ln_Succ)
    {
        next = (struct ClipboardHookNode *)node->chn_Node.ln_Succ;
        Remove(&node->chn_Node);
        FreeMem(node, sizeof(*node));
        node = next;
    }
}

static void clipboard_free_unit(struct ClipboardUnit *clip_unit)
{
    if (!clip_unit)
    {
        return;
    }

    if (clip_unit->cu_Data)
    {
        FreeMem(clip_unit->cu_Data, clip_unit->cu_DataAllocated);
        clip_unit->cu_Data = NULL;
    }

    clipboard_reset_staged_write(clip_unit);
    clipboard_free_hooks(clip_unit);
    FreeMem(clip_unit, sizeof(*clip_unit));
}

static struct ClipboardUnit *clipboard_alloc_unit(ULONG unit)
{
    struct ClipboardUnit *clip_unit;

    clip_unit = (struct ClipboardUnit *)AllocMem(sizeof(*clip_unit), MEMF_CLEAR | MEMF_PUBLIC);
    if (!clip_unit)
    {
        return NULL;
    }

    clip_unit->cu_Partial.cu_Node.ln_Type = NT_UNKNOWN;
    clip_unit->cu_Partial.cu_UnitNum = unit;
    clip_unit->cu_CurrentClipID = 0;
    clip_unit->cu_DataClipID = 0;
    clip_unit->cu_WriteClipID = 0;
    clip_unit->cu_NextClipID = 1;
    NEWLIST(&clip_unit->cu_ChangeHooks);

    return clip_unit;
}

static struct ClipboardUnit *clipboard_get_unit(struct ClipboardBase *clipbase,
                                                ULONG unit,
                                                BOOL create)
{
    if (unit != PRIMARY_CLIP)
    {
        return NULL;
    }

    if (!clipbase->cb_PrimaryUnit && create)
    {
        clipbase->cb_PrimaryUnit = clipboard_alloc_unit(unit);
    }

    return clipbase->cb_PrimaryUnit;
}

static LONG clipboard_reserve_buffer(UBYTE **buffer,
                                     ULONG *allocated,
                                     ULONG required)
{
    UBYTE *newbuf;
    ULONG newsize;

    if (required <= *allocated)
    {
        return 0;
    }

    newsize = (required + 4095UL) & ~4095UL;
    newbuf = (UBYTE *)AllocMem(newsize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!newbuf)
    {
        return IOERR_NOCMD;
    }

    if (*buffer && *allocated)
    {
        CopyMem(*buffer, newbuf, *allocated);
        FreeMem(*buffer, *allocated);
    }

    *buffer = newbuf;
    *allocated = newsize;
    return 0;
}

static struct ClipboardHookNode *clipboard_find_hook(struct ClipboardUnit *clip_unit,
                                                     struct Hook *hook)
{
    struct ClipboardHookNode *node;

    node = (struct ClipboardHookNode *)clip_unit->cu_ChangeHooks.lh_Head;
    while (node && node->chn_Node.ln_Succ)
    {
        if (node->chn_Hook == hook)
        {
            return node;
        }
        node = (struct ClipboardHookNode *)node->chn_Node.ln_Succ;
    }

    return NULL;
}

static void clipboard_notify_changehooks(struct ClipboardUnit *clip_unit,
                                         LONG change_cmd,
                                         LONG clip_id)
{
    struct ClipboardHookNode *node;
    struct ClipboardHookNode *next;
    struct ClipHookMsg msg;

    msg.chm_Type = 0;
    msg.chm_ChangeCmd = change_cmd;
    msg.chm_ClipID = clip_id;

    node = (struct ClipboardHookNode *)clip_unit->cu_ChangeHooks.lh_Head;
    while (node && node->chn_Node.ln_Succ)
    {
        next = (struct ClipboardHookNode *)node->chn_Node.ln_Succ;
        if (node->chn_Hook)
        {
            CallHookPkt(node->chn_Hook, &clip_unit->cu_Partial, &msg);
        }
        node = next;
    }
}

static LONG clipboard_send_satisfy(struct ClipboardUnit *clip_unit)
{
    if (!clip_unit->cu_PostActive || !clip_unit->cu_SatisfyPort)
    {
        return IOERR_BADADDRESS;
    }

    if (!clip_unit->cu_SatisfySent)
    {
        clip_unit->cu_SatisfyMsg.sm_Msg.mn_ReplyPort = NULL;
        clip_unit->cu_SatisfyMsg.sm_Msg.mn_Length = sizeof(struct SatisfyMsg);
        clip_unit->cu_SatisfyMsg.sm_Unit = (UWORD)clip_unit->cu_Partial.cu_UnitNum;
        clip_unit->cu_SatisfyMsg.sm_ClipID = clip_unit->cu_CurrentClipID;
        PutMsg(clip_unit->cu_SatisfyPort, &clip_unit->cu_SatisfyMsg.sm_Msg);
        clip_unit->cu_SatisfySent = TRUE;
    }

    return 0;
}

static void clipboard_reply_request(struct IORequest *ioreq)
{
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static LONG clipboard_service_read(struct ClipboardUnit *clip_unit,
                                   struct IOClipReq *clipreq)
{
    ULONG offset;
    ULONG available;
    ULONG toread;
    LONG requested_id;

    requested_id = clipreq->io_ClipID;
    if (requested_id == 0)
    {
        requested_id = clip_unit->cu_CurrentClipID;
        clipreq->io_ClipID = requested_id;
    }

    clipreq->io_Actual = 0;

    if (requested_id == 0)
    {
        clipreq->io_Offset = 0;
        return 0;
    }

    if (requested_id != clip_unit->cu_CurrentClipID)
    {
        return CBERR_OBSOLETEID;
    }

    if (clip_unit->cu_DataClipID != requested_id)
    {
        LONG error = clipboard_send_satisfy(clip_unit);
        if (error != 0)
        {
            return error;
        }

        clip_unit->cu_PendingRead = clipreq;
        clipreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
        clipreq->io_Message.mn_Node.ln_Succ = NULL;
        clipreq->io_Message.mn_Node.ln_Pred = NULL;
        clipreq->io_Flags &= ~IOF_QUICK;
        return CLIPBOARD_READ_PENDING;
    }

    offset = clipreq->io_Offset;
    if (offset > clip_unit->cu_DataSize)
    {
        offset = clip_unit->cu_DataSize;
    }

    available = clip_unit->cu_DataSize - offset;
    toread = (clipreq->io_Length < available) ? clipreq->io_Length : available;

    if (toread > 0 && clipreq->io_Data)
    {
        CopyMem(clip_unit->cu_Data + offset, clipreq->io_Data, toread);
    }

    clipreq->io_Actual = toread;
    clipreq->io_Offset = offset + toread;
    if (toread != clipreq->io_Length)
    {
        clipreq->io_Offset = clip_unit->cu_DataSize + 1;
    }

    return 0;
}

static void clipboard_complete_pending_read(struct ClipboardUnit *clip_unit)
{
    struct IOClipReq *pending;
    LONG error;

    pending = clip_unit->cu_PendingRead;
    if (!pending)
    {
        return;
    }

    clip_unit->cu_PendingRead = NULL;
    error = clipboard_service_read(clip_unit, pending);
    if (error == CLIPBOARD_READ_PENDING)
    {
        clip_unit->cu_PendingRead = pending;
        return;
    }

    pending->io_Error = error;
    ReplyMsg(&pending->io_Message);
}

static BPTR clipboard_expunge_if_possible(struct ClipboardBase *clipbase)
{
    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)clipbase->cb_Device.dd_Library.lib_Node.ln_Name) ==
        &clipbase->cb_Device.dd_Library.lib_Node)
    {
        Remove(&clipbase->cb_Device.dd_Library.lib_Node);
        DPRINTF(LOG_DEBUG, "_clipboard: Expunge() removed device from DeviceList\n");
    }

    if (clipbase->cb_Device.dd_Library.lib_OpenCnt != 0)
    {
        clipbase->cb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        DPRINTF(LOG_DEBUG, "_clipboard: Expunge() deferred, open count=%u\n",
                (unsigned int)clipbase->cb_Device.dd_Library.lib_OpenCnt);
        return 0;
    }

    clipbase->cb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
    clipboard_free_unit(clipbase->cb_PrimaryUnit);
    clipbase->cb_PrimaryUnit = NULL;

    DPRINTF(LOG_DEBUG, "_clipboard: Expunge() finalizing removal\n");
    return clipbase->cb_SegList;
}

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
    clipbase->cb_PrimaryUnit = NULL;
    
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
    
    clip_unit = clipboard_get_unit(clipbase, unit, TRUE);
    if (!clip_unit) {
        DPRINTF (LOG_ERROR, "_clipboard: Open() out of memory for unit\n");
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }
    clip_unit->cu_OpenCnt++;
    
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
        if (clip_unit->cu_OpenCnt > 0) {
            clip_unit->cu_OpenCnt--;
        }
        ioreq->io_Unit = NULL;
    }

    if (clipbase->cb_Device.dd_Library.lib_OpenCnt > 0) {
        clipbase->cb_Device.dd_Library.lib_OpenCnt--;
    }

    if (clipbase->cb_Device.dd_Library.lib_OpenCnt == 0 &&
        (clipbase->cb_Device.dd_Library.lib_Flags & LIBF_DELEXP))
    {
        return clipboard_expunge_if_possible(clipbase);
    }
    
    return 0;
}

/*
 * Device Expunge
 */
static BPTR __g_lxa_clipboard_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF(LOG_DEBUG, "_clipboard: Expunge() called\n");
    return clipboard_expunge_if_possible((struct ClipboardBase *)dev);
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
    LONG error = 0;
    
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
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_READ offset=%lu length=%lu clipID=%ld\n",
                     clipreq->io_Offset, clipreq->io_Length, clipreq->io_ClipID);

            error = clipboard_service_read(clip_unit, clipreq);
            if (error == CLIPBOARD_READ_PENDING)
            {
                DPRINTF(LOG_DEBUG, "_clipboard: CMD_READ waiting for posted clip %ld\n",
                        clipreq->io_ClipID);
                return 0;
            }

            ioreq->io_Error = error;

            DPRINTF (LOG_DEBUG, "_clipboard: CMD_READ returned %lu bytes error=%ld\n",
                     clipreq->io_Actual, error);
            break;
        }
        
        case CMD_WRITE: {
            ULONG offset = clipreq->io_Offset;
            ULONG length = clipreq->io_Length;
            STRPTR buffer = clipreq->io_Data;
            LONG write_id = clipreq->io_ClipID;
            ULONG required;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_WRITE offset=%lu length=%lu clipID=%ld\n",
                     offset, length, write_id);

            if (write_id == 0) {
                if (clip_unit->cu_WriteClipID == 0) {
                    clipboard_reset_staged_write(clip_unit);
                    clip_unit->cu_WriteClipID = clip_unit->cu_NextClipID++;
                    clipreq->io_ClipID = clip_unit->cu_WriteClipID;
                } else {
                    clipreq->io_ClipID = write_id = clip_unit->cu_WriteClipID;
                }
            } else if (clip_unit->cu_WriteClipID == 0) {
                clip_unit->cu_WriteClipID = write_id;
            } else if (clip_unit->cu_WriteClipID != write_id) {
                ioreq->io_Error = CBERR_OBSOLETEID;
                break;
            }

            required = offset + length;
            
            if (required > MAX_CLIP_SIZE) {
                DPRINTF (LOG_ERROR, "_clipboard: CMD_WRITE size %lu exceeds max %lu\n",
                         required, (ULONG)MAX_CLIP_SIZE);
                ioreq->io_Error = IOERR_BADLENGTH;
                break;
            }

            error = clipboard_reserve_buffer(&clip_unit->cu_WriteData,
                                             &clip_unit->cu_WriteAllocated,
                                             required);
            if (error != 0) {
                DPRINTF (LOG_ERROR, "_clipboard: CMD_WRITE out of memory\n");
                ioreq->io_Error = error;
                break;
            }

            if (offset > clip_unit->cu_WriteSize) {
                memset(clip_unit->cu_WriteData + clip_unit->cu_WriteSize,
                       0,
                       offset - clip_unit->cu_WriteSize);
            }

            if (length > 0 && buffer) {
                CopyMem(buffer, clip_unit->cu_WriteData + offset, length);
            }

            if (required > clip_unit->cu_WriteSize) {
                clip_unit->cu_WriteSize = required;
            }

            clipreq->io_Actual = length;
            clipreq->io_Offset = offset + length;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_WRITE wrote %lu bytes, staged size now %lu clipID=%ld\n",
                     length, clip_unit->cu_WriteSize, clipreq->io_ClipID);
            break;
        }
        
        case CMD_UPDATE: {
            DPRINTF (LOG_DEBUG, "_clipboard: CMD_UPDATE clipID=%ld\n", clipreq->io_ClipID);

            if (clip_unit->cu_WriteClipID == 0 ||
                (clipreq->io_ClipID != 0 && clipreq->io_ClipID != clip_unit->cu_WriteClipID)) {
                ioreq->io_Error = CBERR_OBSOLETEID;
                break;
            }

            if (clip_unit->cu_Data) {
                FreeMem(clip_unit->cu_Data, clip_unit->cu_DataAllocated);
            }

            clip_unit->cu_Data = clip_unit->cu_WriteData;
            clip_unit->cu_DataSize = clip_unit->cu_WriteSize;
            clip_unit->cu_DataAllocated = clip_unit->cu_WriteAllocated;
            clip_unit->cu_DataClipID = clip_unit->cu_WriteClipID;
            clip_unit->cu_CurrentClipID = clip_unit->cu_WriteClipID;

            clip_unit->cu_WriteData = NULL;
            clip_unit->cu_WriteSize = 0;
            clip_unit->cu_WriteAllocated = 0;
            clipreq->io_ClipID = clip_unit->cu_WriteClipID;
            clip_unit->cu_WriteClipID = 0;
            clip_unit->cu_PostActive = FALSE;
            clip_unit->cu_SatisfyPort = NULL;
            clip_unit->cu_SatisfySent = FALSE;

            clipboard_notify_changehooks(clip_unit, CMD_UPDATE, clipreq->io_ClipID);
            clipboard_complete_pending_read(clip_unit);

            DPRINTF (LOG_DEBUG, "_clipboard: CMD_UPDATE committed ClipID=%ld size=%lu\n",
                     clipreq->io_ClipID, clip_unit->cu_DataSize);
            break;
        }
        
        case CBD_CURRENTREADID: {
            clipreq->io_ClipID = clip_unit->cu_CurrentClipID;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CBD_CURRENTREADID -> %ld\n", clip_unit->cu_CurrentClipID);
            break;
        }
        
        case CBD_CURRENTWRITEID: {
            clipreq->io_ClipID = clip_unit->cu_CurrentClipID;
            
            DPRINTF (LOG_DEBUG, "_clipboard: CBD_CURRENTWRITEID -> %ld\n", clip_unit->cu_CurrentClipID);
            break;
        }
        
        case CBD_POST: {
            struct MsgPort *port = (struct MsgPort *)clipreq->io_Data;

            if (!port) {
                ioreq->io_Error = IOERR_BADADDRESS;
                break;
            }

            clip_unit->cu_PostActive = TRUE;
            clip_unit->cu_SatisfyPort = port;
            clip_unit->cu_SatisfySent = FALSE;
            clip_unit->cu_CurrentClipID = clip_unit->cu_NextClipID++;
            clipreq->io_ClipID = clip_unit->cu_CurrentClipID;

            if (clip_unit->cu_PendingRead) {
                clip_unit->cu_PendingRead->io_Error = CBERR_OBSOLETEID;
                ReplyMsg(&clip_unit->cu_PendingRead->io_Message);
                clip_unit->cu_PendingRead = NULL;
            }

            clipboard_notify_changehooks(clip_unit, CBD_POST, clipreq->io_ClipID);
            DPRINTF(LOG_DEBUG, "_clipboard: CBD_POST registered ClipID=%ld\n", clipreq->io_ClipID);
            break;
        }

        case CBD_CHANGEHOOK: {
            struct Hook *hook = (struct Hook *)clipreq->io_Data;
            struct ClipboardHookNode *node;

            if (!hook) {
                ioreq->io_Error = IOERR_BADADDRESS;
                break;
            }

            node = clipboard_find_hook(clip_unit, hook);
            if (clipreq->io_Length == 0) {
                if (node) {
                    Remove(&node->chn_Node);
                    FreeMem(node, sizeof(*node));
                }
            } else {
                if (!node) {
                    node = (struct ClipboardHookNode *)AllocMem(sizeof(*node), MEMF_PUBLIC | MEMF_CLEAR);
                    if (!node) {
                        ioreq->io_Error = IOERR_NOCMD;
                        break;
                    }
                    node->chn_Hook = hook;
                    AddTail(&clip_unit->cu_ChangeHooks, &node->chn_Node);
                }
            }

            DPRINTF(LOG_DEBUG, "_clipboard: CBD_CHANGEHOOK %s hook=0x%08lx\n",
                    clipreq->io_Length == 0 ? "removed" : "installed",
                    (ULONG)hook);
            break;
        }

        case CMD_RESET:
        case CMD_CLEAR:
        case CMD_STOP:
        case CMD_START:
        case CMD_FLUSH:
            /* Standard device commands - no-op for clipboard */
            DPRINTF (LOG_DEBUG, "_clipboard: BeginIO() standard command %u (no-op)\n", command);
            break;
        
        default:
            DPRINTF (LOG_ERROR, "_clipboard: BeginIO() unknown command %u\n", command);
            ioreq->io_Error = IOERR_NOCMD;
            break;
    }
    
done:
    clipboard_reply_request(ioreq);
    
    return 0;
}

/*
 * Device AbortIO - Abort a clipboard request
 */
static ULONG __g_lxa_clipboard_AbortIO ( register struct Library   *dev   __asm("a6"),
                                           register struct IORequest *ioreq __asm("a1"))
{
    struct ClipboardUnit *clip_unit = (struct ClipboardUnit *)ioreq->io_Unit;

    (void)dev;

    DPRINTF(LOG_DEBUG, "_clipboard: AbortIO() called, ioreq=0x%08lx unit=0x%08lx\n",
            (ULONG)ioreq,
            (ULONG)clip_unit);

    if (clip_unit && clip_unit->cu_PendingRead == (struct IOClipReq *)ioreq)
    {
        clip_unit->cu_PendingRead = NULL;
        ioreq->io_Error = IOERR_ABORTED;
        ((struct IOClipReq *)ioreq)->io_Actual = 0;
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
