/*
 * lxa clipboard.device implementation
 *
 * Provides clipboard (copy/paste) services for AmigaOS applications.
 * Clips are kept in memory, one clipboard per unit; any unit number can
 * be opened (as on AmigaOS 3.1, where unit N is stored in CLIPS:N).
 *
 * The clip ID and hold-off rules follow AmigaOS 3.1 as observed on the
 * reference machine (tests/probes/clipboard/clipboard.c, Phase 222f):
 *
 *  - Every unit has a read ID (the last ID handed out) and a write ID
 *    (the ID of the last clip written or posted), both 1 for a new unit.
 *    CMD_WRITE or CBD_POST with io_ClipID 0, and CMD_UPDATE without a
 *    write, take the next ID (read ID + 1) at once, even when they then
 *    have to wait.
 *  - A request whose io_ClipID is below the write ID is obsolete
 *    (CBERR_OBSOLETEID).
 *  - A read that starts with io_ClipID 0 counts as an unfinished read;
 *    any read that reaches the end of the clip (io_ClipID becomes -1)
 *    ends one.  While the count is not zero, the first CMD_WRITE of a new
 *    clip, a CMD_UPDATE without a write and CBD_POST are held off.
 *  - While a clip is being written (first CMD_WRITE until CMD_UPDATE),
 *    reads that are not obsolete are held off.  A read of a pending post
 *    sends the SatisfyMsg and waits for the clip.
 *  - A clip is 8 + the big-endian long at offset 4 bytes long (the IFF
 *    chunk size, no padding); bytes beyond the stored data read as zeros.
 *
 * On AmigaOS 3.1 a held request blocks inside BeginIO(); here it is
 * queued (not replied) until it can be served, and AbortIO() ends it.
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

/* Maximum stored clip size */
#define MAX_CLIP_SIZE (1024UL * 1024UL)

/* private io_Flags bit: the read started with io_ClipID 0 (counts as a reader) */
#define CLIPF_COUNTED 0x40

/* Clipboard unit structure - one clipboard per unit */
struct ClipboardUnit {
    struct ClipboardUnitPartial cu_Partial;   /* cu_Node links the unit list */
    UBYTE  *cu_Data;                /* committed clip */
    ULONG   cu_DataSize;
    ULONG   cu_DataAllocated;
    UBYTE  *cu_WriteData;           /* clip being written */
    ULONG   cu_WriteSize;
    ULONG   cu_WriteAllocated;
    LONG    cu_ReadID;              /* last ID handed out */
    LONG    cu_WriteID;             /* ID of the last write / post */
    LONG    cu_Readers;             /* unfinished reads (may go negative, as on 3.1) */
    BOOL    cu_Writing;             /* a write is in progress */
    struct List cu_ChangeHooks;
    BOOL    cu_PostActive;
    LONG    cu_PostID;
    struct MsgPort *cu_SatisfyPort;
    struct SatisfyMsg cu_SatisfyMsg;
    BOOL    cu_SatisfySent;
    struct List cu_Held;            /* requests waiting (not yet replied) */
};

struct ClipboardHookNode {
    struct Node chn_Node;
    struct Hook *chn_Hook;
};

/* Clipboard device base */
struct ClipboardBase {
    struct Device  cb_Device;
    BPTR           cb_SegList;
    struct List    cb_Units;
};

/* result of serving a request */
#define CLIP_DONE 0
#define CLIP_HOLD 1

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
    if (clip_unit->cu_Data)
        FreeMem(clip_unit->cu_Data, clip_unit->cu_DataAllocated);
    if (clip_unit->cu_WriteData)
        FreeMem(clip_unit->cu_WriteData, clip_unit->cu_WriteAllocated);
    clipboard_free_hooks(clip_unit);
    FreeMem(clip_unit, sizeof(*clip_unit));
}

static struct ClipboardUnit *clipboard_get_unit(struct ClipboardBase *clipbase, ULONG unit)
{
    struct ClipboardUnit *clip_unit;

    for (clip_unit = (struct ClipboardUnit *)clipbase->cb_Units.lh_Head;
         clip_unit->cu_Partial.cu_Node.ln_Succ;
         clip_unit = (struct ClipboardUnit *)clip_unit->cu_Partial.cu_Node.ln_Succ)
    {
        if (clip_unit->cu_Partial.cu_UnitNum == unit)
            return clip_unit;
    }

    clip_unit = (struct ClipboardUnit *)AllocMem(sizeof(*clip_unit), MEMF_CLEAR | MEMF_PUBLIC);
    if (!clip_unit)
        return NULL;

    clip_unit->cu_Partial.cu_Node.ln_Type = NT_UNKNOWN;
    clip_unit->cu_Partial.cu_UnitNum = unit;
    /* a new unit reports clip ID 1 for reading and writing; the first
     * write gets ID 2 (AmigaOS 3.1) */
    clip_unit->cu_ReadID = 1;
    clip_unit->cu_WriteID = 1;
    NEWLIST(&clip_unit->cu_ChangeHooks);
    NEWLIST(&clip_unit->cu_Held);
    AddTail(&clipbase->cb_Units, &clip_unit->cu_Partial.cu_Node);

    return clip_unit;
}

static LONG clipboard_reserve_buffer(UBYTE **buffer,
                                     ULONG *allocated,
                                     ULONG required)
{
    UBYTE *newbuf;
    ULONG newsize;

    if (required <= *allocated)
        return 0;

    newsize = (required + 4095UL) & ~4095UL;
    newbuf = (UBYTE *)AllocMem(newsize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!newbuf)
        return IOERR_NOCMD;

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
            return node;
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
            CallHookPkt(node->chn_Hook, &clip_unit->cu_Partial, &msg);
        node = next;
    }
}

static void clipboard_send_satisfy(struct ClipboardUnit *clip_unit)
{
    if (clip_unit->cu_SatisfySent || !clip_unit->cu_SatisfyPort)
        return;

    /* 3.1 sends the message with mn_Length 0 and no reply port */
    clip_unit->cu_SatisfyMsg.sm_Msg.mn_ReplyPort = NULL;
    clip_unit->cu_SatisfyMsg.sm_Msg.mn_Length = 0;
    clip_unit->cu_SatisfyMsg.sm_Unit = (UWORD)clip_unit->cu_Partial.cu_UnitNum;
    clip_unit->cu_SatisfyMsg.sm_ClipID = clip_unit->cu_PostID;
    PutMsg(clip_unit->cu_SatisfyPort, &clip_unit->cu_SatisfyMsg.sm_Msg);
    clip_unit->cu_SatisfySent = TRUE;
    /* 3.1 calls the change hooks (CBD_POST) when it asks for the data */
    clipboard_notify_changehooks(clip_unit, CBD_POST, clip_unit->cu_PostID);
}

static void clipboard_reply_request(struct IORequest *ioreq)
{
    ioreq->io_Flags &= ~CLIPF_COUNTED;
    if (!(ioreq->io_Flags & IOF_QUICK))
        ReplyMsg(&ioreq->io_Message);
}

/* The readable size of the committed clip: 8 + the long at offset 4. */
static ULONG clipboard_clip_size(struct ClipboardUnit *clip_unit)
{
    UBYTE h[8];
    ULONG len;
    ULONG i;

    if (clip_unit->cu_DataSize == 0)
        return 0;

    for (i = 0; i < 8; i++)
        h[i] = i < clip_unit->cu_DataSize ? clip_unit->cu_Data[i] : 0;

    len = ((ULONG)h[4] << 24) | ((ULONG)h[5] << 16) | ((ULONG)h[6] << 8) | (ULONG)h[7];
    if (len > 0x7ffffff0UL)
        len = 0x7ffffff0UL;
    return len + 8;
}

/* start a new clip (the first write) - or tell that it must wait */
static LONG clipboard_begin_write(struct ClipboardUnit *clip_unit, struct IOClipReq *clipreq)
{
    if (clipreq->io_ClipID < clip_unit->cu_WriteID)
    {
        clipreq->io_Actual = 0;
        clipreq->io_Error = CBERR_OBSOLETEID;
        return CLIP_DONE;
    }

    if (clip_unit->cu_Readers != 0)
        return CLIP_HOLD;

    clip_unit->cu_Writing = TRUE;
    clip_unit->cu_WriteID = clipreq->io_ClipID;
    clip_unit->cu_WriteSize = 0;
    return -1;      /* go ahead */
}

static void clipboard_commit(struct ClipboardUnit *clip_unit)
{
    UBYTE *old = clip_unit->cu_Data;
    ULONG old_alloc = clip_unit->cu_DataAllocated;

    clip_unit->cu_Data = clip_unit->cu_WriteData;
    clip_unit->cu_DataSize = clip_unit->cu_WriteSize;
    clip_unit->cu_DataAllocated = clip_unit->cu_WriteAllocated;
    clip_unit->cu_WriteData = old;
    clip_unit->cu_WriteAllocated = old_alloc;
    clip_unit->cu_WriteSize = 0;
    clip_unit->cu_Writing = FALSE;

    /* the post is satisfied (or superseded) */
    if (clip_unit->cu_PostActive && clip_unit->cu_WriteID >= clip_unit->cu_PostID)
    {
        clip_unit->cu_PostActive = FALSE;
        clip_unit->cu_SatisfyPort = NULL;
        clip_unit->cu_SatisfySent = FALSE;
    }

    clipboard_notify_changehooks(clip_unit, CMD_UPDATE, clip_unit->cu_WriteID);
}

static LONG clipboard_read(struct ClipboardUnit *clip_unit, struct IOClipReq *clipreq)
{
    ULONG offset;
    ULONG size;
    ULONG toread;

    if (clipreq->io_ClipID < clip_unit->cu_WriteID)
    {
        clipreq->io_Actual = 0;
        clipreq->io_Error = CBERR_OBSOLETEID;
        return CLIP_DONE;
    }

    if (clip_unit->cu_Writing)
        return CLIP_HOLD;

    if (clip_unit->cu_PostActive && clipreq->io_ClipID >= clip_unit->cu_PostID)
    {
        clipboard_send_satisfy(clip_unit);
        return CLIP_HOLD;
    }

    if (clipreq->io_Flags & CLIPF_COUNTED)
    {
        clipreq->io_Flags &= ~CLIPF_COUNTED;
        clip_unit->cu_Readers++;
    }

    size = clipboard_clip_size(clip_unit);
    offset = clipreq->io_Offset;
    if (offset >= size)
    {
        /* past the end: this read is finished */
        clipreq->io_Actual = 0;
        clipreq->io_ClipID = -1;
        clip_unit->cu_Readers--;
        return CLIP_DONE;
    }

    toread = size - offset;
    if (clipreq->io_Length < toread)
        toread = clipreq->io_Length;

    if (toread > 0 && clipreq->io_Data)
    {
        ULONG stored = 0;

        if (offset < clip_unit->cu_DataSize)
        {
            stored = clip_unit->cu_DataSize - offset;
            if (stored > toread)
                stored = toread;
            CopyMem(clip_unit->cu_Data + offset, clipreq->io_Data, stored);
        }
        if (stored < toread)
            memset((UBYTE *)clipreq->io_Data + stored, 0, toread - stored);
    }

    clipreq->io_Actual = toread;
    clipreq->io_Offset = offset + toread;
    return CLIP_DONE;
}

static LONG clipboard_write(struct ClipboardUnit *clip_unit, struct IOClipReq *clipreq)
{
    ULONG offset = clipreq->io_Offset;
    ULONG length = clipreq->io_Length;
    ULONG required;
    LONG error;

    if (!clip_unit->cu_Writing)
    {
        LONG r = clipboard_begin_write(clip_unit, clipreq);
        if (r != -1)
            return r;
    }

    clipreq->io_Actual = 0;
    required = offset + length;
    if (required > MAX_CLIP_SIZE || required < offset)
    {
        clipreq->io_Error = IOERR_BADLENGTH;
        return CLIP_DONE;
    }

    error = clipboard_reserve_buffer(&clip_unit->cu_WriteData,
                                     &clip_unit->cu_WriteAllocated,
                                     required);
    if (error != 0)
    {
        clipreq->io_Error = error;
        return CLIP_DONE;
    }

    /* a gap beyond the current end reads as zeros */
    if (offset > clip_unit->cu_WriteSize)
        memset(clip_unit->cu_WriteData + clip_unit->cu_WriteSize, 0,
               offset - clip_unit->cu_WriteSize);

    if (length > 0 && clipreq->io_Data)
        CopyMem(clipreq->io_Data, clip_unit->cu_WriteData + offset, length);
    else if (length > 0)
        memset(clip_unit->cu_WriteData + offset, 0, length);

    if (required > clip_unit->cu_WriteSize)
        clip_unit->cu_WriteSize = required;

    clipreq->io_Actual = length;
    clipreq->io_Offset = offset + length;
    return CLIP_DONE;
}

static LONG clipboard_update(struct ClipboardUnit *clip_unit, struct IOClipReq *clipreq)
{
    if (!clip_unit->cu_Writing)
    {
        /* CMD_UPDATE without a write: an empty clip */
        LONG r = clipboard_begin_write(clip_unit, clipreq);
        if (r != -1)
            return r;
    }

    clipboard_commit(clip_unit);
    return CLIP_DONE;
}

static LONG clipboard_post(struct ClipboardUnit *clip_unit, struct IOClipReq *clipreq)
{
    if (clip_unit->cu_Readers != 0)
        return CLIP_HOLD;

    clip_unit->cu_PostActive = TRUE;
    clip_unit->cu_PostID = clipreq->io_ClipID;
    clip_unit->cu_WriteID = clipreq->io_ClipID;
    clip_unit->cu_SatisfyPort = (struct MsgPort *)clipreq->io_Data;
    clip_unit->cu_SatisfySent = FALSE;

    clipboard_notify_changehooks(clip_unit, CBD_POST, clipreq->io_ClipID);
    return CLIP_DONE;
}

static LONG clipboard_serve(struct ClipboardUnit *clip_unit, struct IOClipReq *clipreq)
{
    switch (clipreq->io_Command)
    {
        case CMD_READ:   return clipboard_read(clip_unit, clipreq);
        case CMD_WRITE:  return clipboard_write(clip_unit, clipreq);
        case CMD_UPDATE: return clipboard_update(clip_unit, clipreq);
        case CBD_POST:   return clipboard_post(clip_unit, clipreq);
    }
    return CLIP_DONE;
}

/* serve held requests until none can make progress */
static void clipboard_run_held(struct ClipboardUnit *clip_unit)
{
    BOOL progress = TRUE;

    while (progress)
    {
        struct IOClipReq *req;

        progress = FALSE;
        for (req = (struct IOClipReq *)clip_unit->cu_Held.lh_Head;
             req->io_Message.mn_Node.ln_Succ;
             req = (struct IOClipReq *)req->io_Message.mn_Node.ln_Succ)
        {
            if (clipboard_serve(clip_unit, req) == CLIP_DONE)
            {
                Remove(&req->io_Message.mn_Node);
                clipboard_reply_request((struct IORequest *)req);
                progress = TRUE;
                break;
            }
        }
    }
}

/*
 * AmigaOS 3.1 behaviour (verified on the reference, Phase 220): an
 * Expunge() while the device is open only sets LIBF_DELEXP and leaves the
 * device in the DeviceList (a later OpenDevice() clears the flag again);
 * closing the last opener does not expunge.  Only an Expunge() with no
 * opener removes the device.
 */
static BPTR clipboard_expunge_if_possible(struct ClipboardBase *clipbase)
{
    struct ClipboardUnit *clip_unit;

    if (clipbase->cb_Device.dd_Library.lib_OpenCnt != 0)
    {
        clipbase->cb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        DPRINTF(LOG_DEBUG, "_clipboard: Expunge() deferred, open count=%u\n",
                (unsigned int)clipbase->cb_Device.dd_Library.lib_OpenCnt);
        return 0;
    }

    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)clipbase->cb_Device.dd_Library.lib_Node.ln_Name) ==
        &clipbase->cb_Device.dd_Library.lib_Node)
    {
        Remove(&clipbase->cb_Device.dd_Library.lib_Node);
        DPRINTF(LOG_DEBUG, "_clipboard: Expunge() removed device from DeviceList\n");
    }

    clipbase->cb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
    while ((clip_unit = (struct ClipboardUnit *)RemHead(&clipbase->cb_Units)) != NULL)
        clipboard_free_unit(clip_unit);

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
    NEWLIST(&clipbase->cb_Units);

    return dev;
}

/*
 * Device Open - every unit number opens its own clipboard (AmigaOS 3.1)
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

    clip_unit = clipboard_get_unit(clipbase, unit);
    if (!clip_unit) {
        DPRINTF (LOG_ERROR, "_clipboard: Open() out of memory for unit\n");
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    ioreq->io_Unit = (struct Unit *)&clip_unit->cu_Partial;
    ioreq->io_Device = (struct Device *)clipbase;

    clipbase->cb_Device.dd_Library.lib_OpenCnt++;
    clipbase->cb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
}

/*
 * Device Close
 */
static BPTR __g_lxa_clipboard_Close( register struct Library   *dev   __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"))
{
    struct ClipboardBase *clipbase = (struct ClipboardBase *)dev;

    DPRINTF (LOG_DEBUG, "_clipboard: Close() called\n");

    ioreq->io_Unit = NULL;

    if (clipbase->cb_Device.dd_Library.lib_OpenCnt > 0)
        clipbase->cb_Device.dd_Library.lib_OpenCnt--;

    /* the last Close() does not expunge, even with LIBF_DELEXP (3.1) */
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

    DPRINTF (LOG_DEBUG, "_clipboard: BeginIO() called, command=%u\n", command);

    ioreq->io_Error = 0;
    ioreq->io_Flags &= ~CLIPF_COUNTED;
    /* only CMD_READ and CMD_WRITE set io_Actual (AmigaOS 3.1) */

    if (!clip_unit) {
        ioreq->io_Error = IOERR_OPENFAIL;
        clipboard_reply_request(ioreq);
        return 0;
    }

    switch (command) {
        case CMD_READ:
        case CMD_WRITE:
        case CMD_UPDATE:
        case CBD_POST:
            if (clipreq->io_ClipID == 0)
            {
                if (command == CMD_READ)
                {
                    clipreq->io_ClipID = clip_unit->cu_ReadID;
                    ioreq->io_Flags |= CLIPF_COUNTED;
                }
                else if (command != CMD_UPDATE || !clip_unit->cu_Writing)
                {
                    clipreq->io_ClipID = ++clip_unit->cu_ReadID;
                }
            }

            Forbid();
            if (clipboard_serve(clip_unit, clipreq) == CLIP_HOLD)
            {
                /* held off: completes later */
                DPRINTF(LOG_DEBUG, "_clipboard: command %u held, clip %ld\n",
                        command, clipreq->io_ClipID);
                ioreq->io_Flags &= ~IOF_QUICK;
                ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
                AddTail(&clip_unit->cu_Held, &ioreq->io_Message.mn_Node);
                Permit();
                return 0;
            }
            clipboard_reply_request(ioreq);
            clipboard_run_held(clip_unit);
            Permit();
            return 0;

        case CBD_CURRENTREADID:
            clipreq->io_ClipID = clip_unit->cu_ReadID;
            break;

        case CBD_CURRENTWRITEID:
            clipreq->io_ClipID = clip_unit->cu_WriteID;
            break;

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
            } else if (!node) {
                node = (struct ClipboardHookNode *)AllocMem(sizeof(*node), MEMF_PUBLIC | MEMF_CLEAR);
                if (!node) {
                    ioreq->io_Error = IOERR_NOCMD;
                    break;
                }
                node->chn_Hook = hook;
                AddTail(&clip_unit->cu_ChangeHooks, &node->chn_Node);
            }
            break;
        }

        case CMD_RESET:
            /* AmigaOS 3.1 (probe clipboard/reset): BeginIO() returns at
             * once, the request is marked in progress (IOF_QUICK cleared,
             * io_Flags bit 7, NT_MESSAGE) with io_Error IOERR_NOCMD and is
             * never replied - not after AbortIO() or later traffic either,
             * so DoIO(CMD_RESET) never returns there. */
            ioreq->io_Error = IOERR_NOCMD;
            ioreq->io_Flags = (ioreq->io_Flags & ~IOF_QUICK) | 0x80;
            ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
            return 0;

        default:
            /* CMD_INVALID, CMD_CLEAR, CMD_STOP, CMD_START, CMD_FLUSH, ... (3.1) */
            ioreq->io_Error = IOERR_NOCMD;
            break;
    }

    clipboard_reply_request(ioreq);
    return 0;
}

/*
 * Device AbortIO - abort a held request
 */
static ULONG __g_lxa_clipboard_AbortIO ( register struct Library   *dev   __asm("a6"),
                                           register struct IORequest *ioreq __asm("a1"))
{
    struct ClipboardUnit *clip_unit = (struct ClipboardUnit *)ioreq->io_Unit;
    struct Node *n;

    (void)dev;

    if (!clip_unit)
        return (ULONG)-1;

    Forbid();
    for (n = clip_unit->cu_Held.lh_Head; n->ln_Succ; n = n->ln_Succ)
    {
        if (n == &ioreq->io_Message.mn_Node)
        {
            Remove(n);
            ioreq->io_Error = IOERR_ABORTED;
            ((struct IOClipReq *)ioreq)->io_Actual = 0;
            clipboard_reply_request(ioreq);
            clipboard_run_held(clip_unit);
            Permit();
            return 0;
        }
    }
    Permit();

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
