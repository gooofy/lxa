/*
 * lxa iffparse.library implementation
 *
 * Reads and writes EA IFF 85 streams through DOS, clipboard or client
 * stream hooks.  The behaviour (stream-hook call pattern, context-stack
 * bookkeeping, error codes, handler and local-context-item semantics) is
 * matched black-box against AmigaOS 3.1 by the conformance probes in
 * tests/probes/iffparse/ (roadmap Phase 222f).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include <inline/alib.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <devices/clipboard.h>

#include <libraries/iffparse.h>
#include <utility/hooks.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"

extern struct UtilityBase *UtilityBase;

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "iffparse"
#define EXLIBVER   " 40.1 (9.2.93)\r\n"

char __aligned _g_iffparse_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_iffparse_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_iffparse_Copyright [] = "(C)opyright 2025-2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_iffparse_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

/* iff_Flags bit set by OpenIFF() (observed on AmigaOS 3.1) */
#define IFFF_OPENED         0x00010000L

/* size of the scratch buffer used to skip data on streams without seek */
#define SKIP_CHUNK          512

/*
 * Internal structures
 */

/* context node: public part followed by the local context items */
struct IntContextNode
{
    struct ContextNode CN;
    struct MinList     cn_Items;
};

/* local context item: public part, purge hook, user data follows */
struct IntLocalContextItem
{
    struct LocalContextItem LCI;
    struct Hook            *lci_Purge;
    ULONG                   lci_DataSize;
};

/* data of an entry/exit handler item */
struct HandlerData
{
    struct Hook *hd_Hook;
    APTR         hd_Object;
};

/* data of a collection item ('coll') stored in one context */
struct CollectionData
{
    struct CollectionItem *cd_First;  /* newest item (FindCollection) */
    struct CollectionItem *cd_Outer;  /* first item of the enclosing scope */
};

/* one buffered write (streams without random seek) */
struct WriteSeg
{
    struct MinNode ws_Node;
    LONG           ws_Size;
    /* data follows */
};

struct IntIFFHandle
{
    struct IFFHandle      IH;
    struct MinList        iff_Stack;     /* head = current chunk */
    struct IntContextNode iff_Default;   /* outermost (root) context */
    struct Hook          *iff_Hook;
    BOOL                  iff_TopDone;   /* the one top-level chunk was entered */
    BOOL                  iff_PopPending;/* ParseIFF() returned EOC for the top */
    struct MinList        iff_Segs;      /* buffered writes */
    LONG                  iff_BufLen;
};

struct IFFParseBase
{
    struct Library lib;
    BPTR           SegList;
    struct Hook    iff_DOSHook;
    struct Hook    iff_ClipHook;
    struct Hook    iff_PropHook;
    struct Hook    iff_CollHook;
    struct Hook    iff_StopHook;
    struct Hook    iff_StopExitHook;
    struct Hook    iff_CollPurgeHook;
};

struct IntClipboardHandle
{
    struct ClipboardHandle cbh_Public;
    LONG                   cbh_Position;
    LONG                   cbh_ClipID;
};

#define IIFF(iff)   ((struct IntIFFHandle *)(iff))
#define ICN(cn)     ((struct IntContextNode *)(cn))
#define ILCI(lci)   ((struct IntLocalContextItem *)(lci))
#define LCIDATA(i)  ((APTR)((UBYTE *)(i) + sizeof(struct IntLocalContextItem)))

/* forward declarations of library functions used internally */
struct LocalContextItem *_iffparse_AllocLocalItem(register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                  register LONG type __asm("d0"),
                                                  register LONG id __asm("d1"),
                                                  register LONG ident __asm("d2"),
                                                  register LONG dataSize __asm("d3"));
void _iffparse_FreeLocalItem(register struct IFFParseBase *IFFParseBase __asm("a6"),
                             register struct LocalContextItem *localItem __asm("a0"));
struct LocalContextItem *_iffparse_FindLocalItem(register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                 register struct IFFHandle *iff __asm("a0"),
                                                 register LONG type __asm("d0"),
                                                 register LONG id __asm("d1"),
                                                 register LONG ident __asm("d2"));
LONG _iffparse_StoreLocalItem(register struct IFFParseBase *IFFParseBase __asm("a6"),
                              register struct IFFHandle *iff __asm("a0"),
                              register struct LocalContextItem *localItem __asm("a1"),
                              register LONG position __asm("d0"));
struct ContextNode *_iffparse_FindPropContext(register struct IFFParseBase *IFFParseBase __asm("a6"),
                                              register struct IFFHandle *iff __asm("a0"));
LONG _iffparse_GoodID(register struct IFFParseBase *IFFParseBase __asm("a6"),
                      register LONG id __asm("d0"));
LONG _iffparse_GoodType(register struct IFFParseBase *IFFParseBase __asm("a6"),
                        register LONG type __asm("d0"));
LONG _iffparse_EntryHandler(register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG type __asm("d0"),
                            register LONG id __asm("d1"),
                            register LONG position __asm("d2"),
                            register struct Hook *handler __asm("a1"),
                            register APTR object __asm("a2"));
LONG _iffparse_ExitHandler(register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1"),
                           register LONG position __asm("d2"),
                           register struct Hook *handler __asm("a1"),
                           register APTR object __asm("a2"));

static LONG DOSStreamHandler(register struct Hook *hook __asm("a0"),
                             register struct IFFHandle *iff __asm("a2"),
                             register struct IFFStreamCmd *cmd __asm("a1"));
static LONG ClipboardStreamHandler(register struct Hook *hook __asm("a0"),
                                   register struct IFFHandle *iff __asm("a2"),
                                   register struct IFFStreamCmd *cmd __asm("a1"));
static LONG PropHandler(register struct Hook *hook __asm("a0"),
                        register struct IFFHandle *iff __asm("a2"),
                        register LONG *cmd __asm("a1"));
static LONG CollHandler(register struct Hook *hook __asm("a0"),
                        register struct IFFHandle *iff __asm("a2"),
                        register LONG *cmd __asm("a1"));
static LONG StopHandler(register struct Hook *hook __asm("a0"),
                        register APTR obj __asm("a2"),
                        register LONG *cmd __asm("a1"));
static LONG StopExitHandler(register struct Hook *hook __asm("a0"),
                            register APTR obj __asm("a2"),
                            register LONG *cmd __asm("a1"));
static LONG CollPurge(register struct Hook *hook __asm("a0"),
                      register struct LocalContextItem *lci __asm("a2"),
                      register LONG *cmd __asm("a1"));

/*
 * Library init/open/close/expunge
 */

static void init_hook(struct Hook *h, APTR entry)
{
    h->h_Entry = (ULONG (*)())entry;
    h->h_SubEntry = NULL;
    h->h_Data = NULL;
}

struct IFFParseBase * __g_lxa_iffparse_InitLib ( register struct IFFParseBase *iffbase __asm("d0"),
                                                  register BPTR                seglist __asm("a0"),
                                                  register struct ExecBase    *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_iffparse: InitLib() called\n");
    iffbase->SegList = seglist;

    init_hook(&iffbase->iff_DOSHook, DOSStreamHandler);
    init_hook(&iffbase->iff_ClipHook, ClipboardStreamHandler);
    init_hook(&iffbase->iff_PropHook, PropHandler);
    init_hook(&iffbase->iff_CollHook, CollHandler);
    init_hook(&iffbase->iff_StopHook, StopHandler);
    init_hook(&iffbase->iff_StopExitHook, StopExitHandler);
    init_hook(&iffbase->iff_CollPurgeHook, CollPurge);

    return iffbase;
}

BPTR __g_lxa_iffparse_ExpungeLib ( register struct IFFParseBase *iffbase __asm("a6") )
{
    return 0;
}

struct IFFParseBase * __g_lxa_iffparse_OpenLib ( register struct IFFParseBase *iffbase __asm("a6") )
{
    iffbase->lib.lib_OpenCnt++;
    iffbase->lib.lib_Flags &= ~LIBF_DELEXP;
    return iffbase;
}

BPTR __g_lxa_iffparse_CloseLib ( register struct IFFParseBase *iffbase __asm("a6") )
{
    iffbase->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_iffparse_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_iffparse", "ExtFuncLib");
    return 0;
}

/*
 * Built-in stream handlers.  Like every stream hook they return 0 for
 * success and non-zero for failure (InitIFF autodoc).
 */
static LONG DOSStreamHandler(register struct Hook *hook __asm("a0"),
                             register struct IFFHandle *iff __asm("a2"),
                             register struct IFFStreamCmd *cmd __asm("a1"))
{
    BPTR fh = (BPTR)iff->iff_Stream;

    switch (cmd->sc_Command)
    {
        case IFFCMD_READ:
            return Read(fh, cmd->sc_Buf, cmd->sc_NBytes) == cmd->sc_NBytes ? 0 : IFFERR_READ;
        case IFFCMD_WRITE:
            return Write(fh, cmd->sc_Buf, cmd->sc_NBytes) == cmd->sc_NBytes ? 0 : IFFERR_WRITE;
        case IFFCMD_SEEK:
            return Seek(fh, cmd->sc_NBytes, OFFSET_CURRENT) == -1 ? IFFERR_SEEK : 0;
        default:
            return 0;
    }
}

static LONG ClipboardStreamHandler(register struct Hook *hook __asm("a0"),
                                   register struct IFFHandle *iff __asm("a2"),
                                   register struct IFFStreamCmd *cmd __asm("a1"))
{
    struct IntClipboardHandle *clip = (struct IntClipboardHandle *)iff->iff_Stream;
    struct IOClipReq *req;

    if (!clip)
        return IFFERR_NOHOOK;

    req = &clip->cbh_Public.cbh_Req;

    switch (cmd->sc_Command)
    {
        case IFFCMD_INIT:
            /* ClipID 0: a write starts a new clip, a read gets the current one */
            clip->cbh_ClipID = 0;
            clip->cbh_Position = 0;
            return 0;

        case IFFCMD_CLEANUP:
            if (iff->iff_Flags & IFFF_WRITE)
            {
                req->io_Command = CMD_UPDATE;
                req->io_ClipID = clip->cbh_ClipID;
                req->io_Offset = clip->cbh_Position;
                req->io_Length = 0;
                req->io_Data = NULL;
                DoIO((struct IORequest *)req);
            }
            else if (clip->cbh_ClipID != -1)
            {
                /* reading past the end tells the device the read is over */
                UBYTE buf[32];
                int guard = 0;
                do
                {
                    req->io_Command = CMD_READ;
                    req->io_Data = (STRPTR)buf;
                    req->io_Length = sizeof(buf);
                    req->io_Offset = clip->cbh_Position;
                    req->io_ClipID = clip->cbh_ClipID;
                    DoIO((struct IORequest *)req);
                    clip->cbh_Position += req->io_Actual;
                } while (req->io_Error == 0 && req->io_Actual != 0 && ++guard < 100000);
            }
            clip->cbh_Position = 0;
            clip->cbh_ClipID = 0;
            return 0;

        case IFFCMD_READ:
            req->io_Command = CMD_READ;
            req->io_Data = (STRPTR)cmd->sc_Buf;
            req->io_Length = cmd->sc_NBytes;
            req->io_Offset = clip->cbh_Position;
            req->io_ClipID = clip->cbh_ClipID;
            DoIO((struct IORequest *)req);
            clip->cbh_ClipID = req->io_ClipID;
            clip->cbh_Position += req->io_Actual;
            if (req->io_Error != 0 || req->io_Actual != (ULONG)cmd->sc_NBytes)
                return IFFERR_READ;
            return 0;

        case IFFCMD_WRITE:
            req->io_Command = CMD_WRITE;
            req->io_Data = (STRPTR)cmd->sc_Buf;
            req->io_Length = cmd->sc_NBytes;
            req->io_Offset = clip->cbh_Position;
            req->io_ClipID = clip->cbh_ClipID;
            DoIO((struct IORequest *)req);
            clip->cbh_ClipID = req->io_ClipID;
            clip->cbh_Position += req->io_Actual;
            if (req->io_Error != 0 || req->io_Actual != (ULONG)cmd->sc_NBytes)
                return IFFERR_WRITE;
            return 0;

        case IFFCMD_SEEK:
            if (clip->cbh_Position + cmd->sc_NBytes < 0)
                return IFFERR_SEEK;
            clip->cbh_Position += cmd->sc_NBytes;
            return 0;

        default:
            return 0;
    }
}

/*
 * Stream access
 */

static LONG stream_cmd(struct IntIFFHandle *iiff, LONG command, APTR buf, LONG n)
{
    struct IFFStreamCmd cmd;

    if (!iiff->iff_Hook)
        return IFFERR_NOHOOK;

    cmd.sc_Command = command;
    cmd.sc_Buf = buf;
    cmd.sc_NBytes = n;
    return (LONG)CallHookPkt(iiff->iff_Hook, &iiff->IH, &cmd);
}

static BOOL is_buffered(struct IntIFFHandle *iiff)
{
    return (iiff->IH.iff_Flags & IFFF_WRITE) && !(iiff->IH.iff_Flags & IFFF_RSEEK);
}

static void free_segs(struct IntIFFHandle *iiff)
{
    struct WriteSeg *ws;

    while ((ws = (struct WriteSeg *)RemHead((struct List *)&iiff->iff_Segs)) != NULL)
        FreeMem(ws, sizeof(struct WriteSeg) + ws->ws_Size);
    iiff->iff_BufLen = 0;
}

/* write n bytes to the stream, or to the write buffer (0 or IFFERR_WRITE) */
static LONG stream_write(struct IntIFFHandle *iiff, APTR buf, LONG n)
{
    if (is_buffered(iiff))
    {
        struct WriteSeg *ws;

        if (n < 0)
            return IFFERR_WRITE;
        ws = AllocMem(sizeof(struct WriteSeg) + n, MEMF_ANY);
        if (!ws)
            return IFFERR_NOMEM;
        ws->ws_Size = n;
        CopyMem(buf, (UBYTE *)ws + sizeof(struct WriteSeg), n);
        AddTail((struct List *)&iiff->iff_Segs, (struct Node *)&ws->ws_Node);
        iiff->iff_BufLen += n;
        return 0;
    }

    return stream_cmd(iiff, IFFCMD_WRITE, buf, n) ? IFFERR_WRITE : 0;
}

/* overwrite 4 bytes at a buffer offset (size fix-up of a buffered chunk) */
static void patch_segs(struct IntIFFHandle *iiff, LONG offset, LONG value)
{
    struct WriteSeg *ws;
    LONG pos = 0;
    int i;

    for (ws = (struct WriteSeg *)iiff->iff_Segs.mlh_Head;
         ws->ws_Node.mln_Succ;
         ws = (struct WriteSeg *)ws->ws_Node.mln_Succ)
    {
        UBYTE *d = (UBYTE *)ws + sizeof(struct WriteSeg);
        for (i = 0; i < 4; i++)
        {
            LONG at = offset + i - pos;
            if (at >= 0 && at < ws->ws_Size)
                d[at] = (UBYTE)(value >> (24 - i * 8));
        }
        pos += ws->ws_Size;
    }
}

/* write the buffered segments, one hook call each */
static LONG flush_segs(struct IntIFFHandle *iiff)
{
    struct WriteSeg *ws;
    LONG err = 0;

    for (ws = (struct WriteSeg *)iiff->iff_Segs.mlh_Head;
         ws->ws_Node.mln_Succ;
         ws = (struct WriteSeg *)ws->ws_Node.mln_Succ)
    {
        if (stream_cmd(iiff, IFFCMD_WRITE, (UBYTE *)ws + sizeof(struct WriteSeg), ws->ws_Size))
        {
            err = IFFERR_WRITE;
            break;
        }
    }
    free_segs(iiff);
    return err;
}

/*
 * Context stack
 */

static BOOL is_composite(LONG id)
{
    return id == ID_FORM || id == ID_LIST || id == ID_CAT || id == ID_PROP;
}

static struct IntContextNode *top_node(struct IntIFFHandle *iiff)
{
    struct IntContextNode *icn = (struct IntContextNode *)iiff->iff_Stack.mlh_Head;
    return icn->CN.cn_Node.mln_Succ ? icn : NULL;
}

static struct IntContextNode *parent_node(struct IntContextNode *icn)
{
    struct IntContextNode *p = (struct IntContextNode *)icn->CN.cn_Node.mln_Succ;
    return (p && p->CN.cn_Node.mln_Succ) ? p : NULL;
}

static void purge_item(struct IFFParseBase *IFFParseBase, struct IntLocalContextItem *ilci)
{
    if (ilci->lci_Purge)
    {
        LONG cmd = IFFCMD_PURGELCI;
        CallHookPkt(ilci->lci_Purge, &ilci->LCI, &cmd);
    }
    else
        _iffparse_FreeLocalItem(IFFParseBase, &ilci->LCI);
}

static void purge_items(struct IFFParseBase *IFFParseBase, struct IntContextNode *icn)
{
    struct IntLocalContextItem *ilci;

    while ((ilci = (struct IntLocalContextItem *)RemHead((struct List *)&icn->cn_Items)) != NULL)
        purge_item(IFFParseBase, ilci);
}

static struct IntContextNode *push_node(struct IntIFFHandle *iiff, LONG id, LONG type, LONG size)
{
    struct IntContextNode *icn = AllocMem(sizeof(struct IntContextNode), MEMF_ANY | MEMF_CLEAR);

    if (!icn)
        return NULL;
    icn->CN.cn_ID = id;
    icn->CN.cn_Type = type;
    icn->CN.cn_Size = size;
    icn->CN.cn_Scan = 0;
    NewList((struct List *)&icn->cn_Items);
    AddHead((struct List *)&iiff->iff_Stack, (struct Node *)&icn->CN.cn_Node);
    iiff->IH.iff_Depth++;
    return icn;
}

static void free_top(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff)
{
    struct IntContextNode *icn = top_node(iiff);

    if (!icn)
        return;
    Remove((struct Node *)&icn->CN.cn_Node);
    iiff->IH.iff_Depth--;
    purge_items(IFFParseBase, icn);
    FreeMem(icn, sizeof(struct IntContextNode));
}

/* store an item in a context, replacing (purging) one with the same keys */
static void store_in(struct IFFParseBase *IFFParseBase, struct IntContextNode *icn,
                     struct LocalContextItem *lci)
{
    struct IntLocalContextItem *old;

    for (old = (struct IntLocalContextItem *)icn->cn_Items.mlh_Head;
         old->LCI.lci_Node.mln_Succ;
         old = (struct IntLocalContextItem *)old->LCI.lci_Node.mln_Succ)
    {
        if (old->LCI.lci_Type == lci->lci_Type && old->LCI.lci_ID == lci->lci_ID &&
            old->LCI.lci_Ident == lci->lci_Ident)
        {
            Remove((struct Node *)&old->LCI.lci_Node);
            purge_item(IFFParseBase, old);
            break;
        }
    }
    AddHead((struct List *)&icn->cn_Items, (struct Node *)&lci->lci_Node);
}

static struct LocalContextItem *find_in(struct IntContextNode *icn, LONG type, LONG id, LONG ident)
{
    struct IntLocalContextItem *ilci;

    for (ilci = (struct IntLocalContextItem *)icn->cn_Items.mlh_Head;
         ilci->LCI.lci_Node.mln_Succ;
         ilci = (struct IntLocalContextItem *)ilci->LCI.lci_Node.mln_Succ)
    {
        if (ilci->LCI.lci_Type == (ULONG)type && ilci->LCI.lci_ID == (ULONG)id &&
            ilci->LCI.lci_Ident == (ULONG)ident)
            return &ilci->LCI;
    }
    return NULL;
}

/*
 * Chunk data transfer, limited to the chunk's size (unless unknown)
 */

static LONG chunk_read(struct IntIFFHandle *iiff, struct IntContextNode *icn, APTR buf, LONG n)
{
    LONG avail = icn->CN.cn_Size - icn->CN.cn_Scan;

    if (n > avail)
        n = avail;
    if (n <= 0)
        return 0;
    if (stream_cmd(iiff, IFFCMD_READ, buf, n))
        return IFFERR_READ;
    icn->CN.cn_Scan += n;
    return n;
}

static LONG chunk_write(struct IntIFFHandle *iiff, struct IntContextNode *icn, APTR buf, LONG n)
{
    LONG err;

    if (icn->CN.cn_Size != IFFSIZE_UNKNOWN)
    {
        LONG avail = icn->CN.cn_Size - icn->CN.cn_Scan;
        if (n > avail)
            n = avail;
        if (n < 0)
            n = 0;
    }
    if (n == 0)
        return 0;
    err = stream_write(iiff, buf, n);
    if (err)
        return err;
    icn->CN.cn_Scan += n;
    return n;
}

/*
 * Reading: push the next chunk from the stream.  The header is read as
 * part of the parent chunk; composite chunks then read their type as
 * part of themselves (so a short chunk yields IFFERR_READ).
 */
static LONG push_read(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff)
{
    struct IntContextNode *parent = top_node(iiff);
    struct IntContextNode *icn;
    LONG hdr[2];
    LONG id, size, n;

    if (parent)
    {
        n = chunk_read(iiff, parent, hdr, 8);
        if (n < 0)
            return n;
        if (n != 8)
            return IFFERR_READ;
    }
    else if (stream_cmd(iiff, IFFCMD_READ, hdr, 8))
        return IFFERR_READ;

    id = hdr[0];
    size = hdr[1];

    if (size < 0)
        return IFFERR_MANGLED;
    if (parent && size > parent->CN.cn_Size - parent->CN.cn_Scan)
        return IFFERR_MANGLED;
    if (!_iffparse_GoodID(IFFParseBase, id))
        return IFFERR_SYNTAX;

    /* every chunk starts with its parent's type; FORM/LIST/CAT/PROP then
     * read their own over it */
    icn = push_node(iiff, id, parent ? parent->CN.cn_Type : 0, size);
    if (!icn)
        return IFFERR_NOMEM;

    if (!parent)
    {
        if (id != ID_FORM && id != ID_LIST && id != ID_CAT)
            return IFFERR_NOTIFF;
    }
    else if (id == ID_PROP)
    {
        if (parent->CN.cn_ID != ID_LIST)
            return IFFERR_SYNTAX;
    }
    else if (!is_composite(id))
    {
        if (parent->CN.cn_ID != ID_FORM && parent->CN.cn_ID != ID_PROP)
            return IFFERR_SYNTAX;
    }

    if (is_composite(id))
    {
        if (size & 1)
            return IFFERR_MANGLED;
        n = chunk_read(iiff, icn, &icn->CN.cn_Type, 4);
        if (n < 0)
            return n;
        if (n != 4)
            return IFFERR_READ;
        if (!_iffparse_GoodType(IFFParseBase, icn->CN.cn_Type))
            return IFFERR_MANGLED;
    }

    return 0;
}

/* Reading: skip the rest of the current chunk (and its pad byte) and pop it */
static LONG pop_read(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff)
{
    struct IntContextNode *icn = top_node(iiff);
    struct IntContextNode *parent;
    LONG size, skip;

    if (!icn)
        return IFFERR_EOF;

    size = icn->CN.cn_Size + (icn->CN.cn_Size & 1);
    skip = size - icn->CN.cn_Scan;
    if (skip > 0)
    {
        if (iiff->IH.iff_Flags & (IFFF_FSEEK | IFFF_RSEEK))
        {
            if (stream_cmd(iiff, IFFCMD_SEEK, NULL, skip))
                return IFFERR_SEEK;
        }
        else
        {
            UBYTE *buf = AllocMem(SKIP_CHUNK, MEMF_ANY);
            if (!buf)
                return IFFERR_NOMEM;
            while (skip > 0)
            {
                LONG n = skip > SKIP_CHUNK ? SKIP_CHUNK : skip;
                if (stream_cmd(iiff, IFFCMD_READ, buf, n))
                {
                    FreeMem(buf, SKIP_CHUNK);
                    return IFFERR_READ;
                }
                skip -= n;
            }
            FreeMem(buf, SKIP_CHUNK);
        }
    }

    parent = parent_node(icn);
    free_top(IFFParseBase, iiff);
    if (parent)
        parent->CN.cn_Scan += size;
    return 0;
}

/* Writing: finish the current chunk (pad byte, size fix-up) and pop it */
static LONG pop_write(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff)
{
    struct IntContextNode *icn = top_node(iiff);
    struct IntContextNode *parent;
    LONG scan, pad, err, rc = 0;

    if (!icn)
        return IFFERR_EOF;

    scan = icn->CN.cn_Scan;
    if (icn->CN.cn_Size != IFFSIZE_UNKNOWN && scan != icn->CN.cn_Size)
        return IFFERR_MANGLED;

    pad = scan & 1;
    if (pad)
    {
        UBYTE zero = 0;
        err = stream_write(iiff, &zero, 1);
        if (err)
            return err;
    }

    parent = parent_node(icn);

    if (icn->CN.cn_Size == IFFSIZE_UNKNOWN)
    {
        if (is_buffered(iiff))
            patch_segs(iiff, iiff->iff_BufLen - (scan + pad + 4), scan);
        else
        {
            err = 0;
            if (stream_cmd(iiff, IFFCMD_SEEK, NULL, -(scan + pad + 4)))
                err = IFFERR_SEEK;
            else if (stream_cmd(iiff, IFFCMD_WRITE, &scan, 4))
                err = IFFERR_WRITE;
            else if (stream_cmd(iiff, IFFCMD_SEEK, NULL, scan + pad))
                err = IFFERR_SEEK;
            if (err)
            {
                free_top(IFFParseBase, iiff);
                return err;
            }
        }
    }

    free_top(IFFParseBase, iiff);

    if (parent)
    {
        parent->CN.cn_Scan += scan + pad;
        if (parent->CN.cn_Size != IFFSIZE_UNKNOWN && parent->CN.cn_Scan > parent->CN.cn_Size)
            rc = IFFERR_MANGLED;
    }
    else if (is_buffered(iiff))
    {
        err = flush_segs(iiff);
        if (err)
            rc = err;
    }

    return rc;
}

/* find and call the entry or exit handler for the current chunk */
static LONG call_handler(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff,
                         struct IntContextNode *icn, LONG ident, LONG command)
{
    struct LocalContextItem *lci;
    struct HandlerData *hd;

    lci = _iffparse_FindLocalItem(IFFParseBase, &iiff->IH, icn->CN.cn_Type, icn->CN.cn_ID, ident);
    if (!lci)
        return 0;
    hd = (struct HandlerData *)LCIDATA(lci);
    if (!hd->hd_Hook)
        return 0;
    return (LONG)CallHookPkt(hd->hd_Hook, hd->hd_Object, &command);
}

/*
 * Built-in handlers (PropChunk, CollectionChunk, StopChunk, StopOnExit)
 */

static LONG PropHandler(register struct Hook *hook __asm("a0"),
                        register struct IFFHandle *iff __asm("a2"),
                        register LONG *cmd __asm("a1"))
{
    struct IFFParseBase *IFFParseBase =
        (struct IFFParseBase *)((UBYTE *)hook - (ULONG)&((struct IFFParseBase *)0)->iff_PropHook);
    struct IntIFFHandle *iiff = IIFF(iff);
    struct IntContextNode *icn = top_node(iiff);
    struct IntContextNode *scope;
    struct LocalContextItem *lci;
    struct StoredProperty *sp;
    LONG n;

    if (!icn)
        return IFFERR_NOSCOPE;
    scope = ICN(_iffparse_FindPropContext(IFFParseBase, iff));
    if (!scope)
        return IFFERR_NOSCOPE;

    lci = _iffparse_AllocLocalItem(IFFParseBase, icn->CN.cn_Type, icn->CN.cn_ID, IFFLCI_PROP,
                                   sizeof(struct StoredProperty) + icn->CN.cn_Size);
    if (!lci)
        return IFFERR_NOMEM;
    sp = (struct StoredProperty *)LCIDATA(lci);
    sp->sp_Size = icn->CN.cn_Size;
    sp->sp_Data = (APTR)(sp + 1);

    n = chunk_read(iiff, icn, sp->sp_Data, icn->CN.cn_Size);
    if (n < 0 || n != icn->CN.cn_Size)
    {
        _iffparse_FreeLocalItem(IFFParseBase, lci);
        return n < 0 ? n : IFFERR_READ;
    }

    store_in(IFFParseBase, scope, lci);
    return 0;
}

static LONG CollHandler(register struct Hook *hook __asm("a0"),
                        register struct IFFHandle *iff __asm("a2"),
                        register LONG *cmd __asm("a1"))
{
    struct IFFParseBase *IFFParseBase =
        (struct IFFParseBase *)((UBYTE *)hook - (ULONG)&((struct IFFParseBase *)0)->iff_CollHook);
    struct IntIFFHandle *iiff = IIFF(iff);
    struct IntContextNode *icn = top_node(iiff);
    struct IntContextNode *scope;
    struct LocalContextItem *lci;
    struct CollectionData *cd;
    struct CollectionItem *ci;
    LONG n, size;

    if (!icn)
        return IFFERR_NOSCOPE;
    scope = ICN(_iffparse_FindPropContext(IFFParseBase, iff));
    if (!scope)
        return IFFERR_NOSCOPE;

    size = icn->CN.cn_Size;
    ci = AllocMem(sizeof(struct CollectionItem) + size, MEMF_ANY | MEMF_CLEAR);
    if (!ci)
        return IFFERR_NOMEM;
    ci->ci_Size = size;
    ci->ci_Data = (APTR)(ci + 1);

    n = chunk_read(iiff, icn, ci->ci_Data, size);
    if (n < 0 || n != size)
    {
        FreeMem(ci, sizeof(struct CollectionItem) + size);
        return n < 0 ? n : IFFERR_READ;
    }

    lci = find_in(scope, icn->CN.cn_Type, icn->CN.cn_ID, IFFLCI_COLLECTION);
    if (!lci)
    {
        struct LocalContextItem *outer =
            _iffparse_FindLocalItem(IFFParseBase, iff, icn->CN.cn_Type, icn->CN.cn_ID, IFFLCI_COLLECTION);

        lci = _iffparse_AllocLocalItem(IFFParseBase, icn->CN.cn_Type, icn->CN.cn_ID,
                                       IFFLCI_COLLECTION, sizeof(struct CollectionData));
        if (!lci)
        {
            FreeMem(ci, sizeof(struct CollectionItem) + size);
            return IFFERR_NOMEM;
        }
        cd = (struct CollectionData *)LCIDATA(lci);
        cd->cd_Outer = outer ? ((struct CollectionData *)LCIDATA(outer))->cd_First : NULL;
        cd->cd_First = cd->cd_Outer;
        ILCI(lci)->lci_Purge = &IFFParseBase->iff_CollPurgeHook;
        store_in(IFFParseBase, scope, lci);
    }

    cd = (struct CollectionData *)LCIDATA(lci);
    ci->ci_Next = cd->cd_First;
    cd->cd_First = ci;
    return 0;
}

static LONG StopHandler(register struct Hook *hook __asm("a0"),
                        register APTR obj __asm("a2"),
                        register LONG *cmd __asm("a1"))
{
    return IFF_RETURN2CLIENT;
}

static LONG StopExitHandler(register struct Hook *hook __asm("a0"),
                            register APTR obj __asm("a2"),
                            register LONG *cmd __asm("a1"))
{
    return IFFERR_EOC;
}

/* purge vector of a collection item: free this context's items */
static LONG CollPurge(register struct Hook *hook __asm("a0"),
                      register struct LocalContextItem *lci __asm("a2"),
                      register LONG *cmd __asm("a1"))
{
    struct IFFParseBase *IFFParseBase =
        (struct IFFParseBase *)((UBYTE *)hook - (ULONG)&((struct IFFParseBase *)0)->iff_CollPurgeHook);
    struct CollectionData *cd = (struct CollectionData *)LCIDATA(lci);
    struct CollectionItem *ci = cd->cd_First;

    while (ci && ci != cd->cd_Outer)
    {
        struct CollectionItem *next = ci->ci_Next;
        FreeMem(ci, sizeof(struct CollectionItem) + ci->ci_Size);
        ci = next;
    }
    _iffparse_FreeLocalItem(IFFParseBase, lci);
    return 0;
}

/*
 * IFFParse functions
 */

struct IFFHandle * _iffparse_AllocIFF ( register struct IFFParseBase *IFFParseBase __asm("a6") )
{
    struct IntIFFHandle *iiff = AllocMem(sizeof(struct IntIFFHandle), MEMF_ANY | MEMF_CLEAR);

    if (!iiff)
        return NULL;

    NewList((struct List *)&iiff->iff_Stack);
    NewList((struct List *)&iiff->iff_Default.cn_Items);
    NewList((struct List *)&iiff->iff_Segs);
    return &iiff->IH;
}

LONG _iffparse_OpenIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0"),
                         register LONG rwMode __asm("d0") )
{
    struct IntIFFHandle *iiff = IIFF(iff);

    if (!iff)
        return IFFERR_NOMEM;

    iff->iff_Flags = (iff->iff_Flags & ~IFFF_RWBITS) | (rwMode & IFFF_RWBITS) | IFFF_OPENED;
    iiff->iff_TopDone = FALSE;
    iiff->iff_PopPending = FALSE;
    free_segs(iiff);

    /* reading starts with the first ParseIFF(): nothing is read here */
    return stream_cmd(iiff, IFFCMD_INIT, NULL, 0);
}

LONG _iffparse_ParseIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0"),
                          register LONG control __asm("d0") )
{
    struct IntIFFHandle *iiff = IIFF(iff);
    struct IntContextNode *icn;
    LONG err;

    if (!iff)
        return IFFERR_NOMEM;

    for (;;)
    {
        BOOL entry;

        if (iiff->iff_PopPending)
        {
            err = pop_read(IFFParseBase, iiff);
            if (err)
                return err;
            iiff->iff_PopPending = FALSE;
            if (iff->iff_Depth == 0)
                return IFFERR_EOF;
        }

        icn = top_node(iiff);
        if (!icn)
        {
            if (iiff->iff_TopDone)
                return IFFERR_EOF;
            iiff->iff_TopDone = TRUE;
            err = push_read(IFFParseBase, iiff);
            if (err)
                return err;
            entry = TRUE;
        }
        else if (!is_composite(icn->CN.cn_ID))
            entry = FALSE;
        else if (icn->CN.cn_Scan > icn->CN.cn_Size)
            return IFFERR_MANGLED;
        else if (icn->CN.cn_Scan == icn->CN.cn_Size)
            entry = FALSE;
        else
        {
            err = push_read(IFFParseBase, iiff);
            if (err)
                return err;
            entry = TRUE;
        }

        icn = top_node(iiff);
        if (entry)
        {
            if (control != IFFPARSE_RAWSTEP)
            {
                err = call_handler(IFFParseBase, iiff, icn, IFFLCI_ENTRYHANDLER, IFFCMD_ENTRY);
                if (err == IFF_RETURN2CLIENT)
                    return 0;
                if (err)
                    return err;
            }
            if (control != IFFPARSE_SCAN)
                return 0;
        }
        else
        {
            err = 0;
            if (control != IFFPARSE_RAWSTEP)
                err = call_handler(IFFParseBase, iiff, icn, IFFLCI_EXITHANDLER, IFFCMD_EXIT);
            iiff->iff_PopPending = TRUE;
            if (err == IFF_RETURN2CLIENT)
                return 0;
            if (err)
                return err;
            if (control != IFFPARSE_SCAN)
                return IFFERR_EOC;
        }
    }
}

void _iffparse_CloseIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0") )
{
    struct IntIFFHandle *iiff = IIFF(iff);

    if (!iff)
        return;

    while (iff->iff_Depth > 0)
    {
        LONG depth = iff->iff_Depth;
        if (iff->iff_Flags & IFFF_WRITE)
            pop_write(IFFParseBase, iiff);
        else
            pop_read(IFFParseBase, iiff);
        if (iff->iff_Depth == depth)
            free_top(IFFParseBase, iiff);       /* could not finish it: drop it */
    }

    free_segs(iiff);
    iiff->iff_PopPending = FALSE;
    stream_cmd(iiff, IFFCMD_CLEANUP, NULL, 0);
}

void _iffparse_FreeIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0") )
{
    struct IntIFFHandle *iiff = IIFF(iff);

    if (!iff)
        return;

    while (iff->iff_Depth > 0)
        free_top(IFFParseBase, iiff);
    purge_items(IFFParseBase, &iiff->iff_Default);
    free_segs(iiff);
    FreeMem(iiff, sizeof(struct IntIFFHandle));
}

LONG _iffparse_ReadChunkBytes ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                register struct IFFHandle *iff __asm("a0"),
                                register APTR buf __asm("a1"),
                                register LONG numBytes __asm("d0") )
{
    struct IntContextNode *icn = top_node(IIFF(iff));

    if (!icn)
        return IFFERR_EOF;
    return chunk_read(IIFF(iff), icn, buf, numBytes);
}

LONG _iffparse_WriteChunkBytes ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                 register struct IFFHandle *iff __asm("a0"),
                                 register APTR buf __asm("a1"),
                                 register LONG numBytes __asm("d0") )
{
    struct IntContextNode *icn = top_node(IIFF(iff));

    if (!icn)
        return IFFERR_EOF;
    return chunk_write(IIFF(iff), icn, buf, numBytes);
}

LONG _iffparse_ReadChunkRecords ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                  register struct IFFHandle *iff __asm("a0"),
                                  register APTR buf __asm("a1"),
                                  register LONG bytesPerRecord __asm("d0"),
                                  register LONG numRecords __asm("d1") )
{
    struct IntContextNode *icn = top_node(IIFF(iff));
    LONG avail, n;

    if (!icn)
        return IFFERR_EOF;
    if (bytesPerRecord <= 0 || numRecords <= 0)
        return 0;
    avail = (icn->CN.cn_Size - icn->CN.cn_Scan) / bytesPerRecord;
    if (numRecords > avail)
        numRecords = avail;
    n = chunk_read(IIFF(iff), icn, buf, numRecords * bytesPerRecord);
    if (n < 0)
        return n;
    return n / bytesPerRecord;
}

LONG _iffparse_WriteChunkRecords ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                   register struct IFFHandle *iff __asm("a0"),
                                   register APTR buf __asm("a1"),
                                   register LONG bytesPerRecord __asm("d0"),
                                   register LONG numRecords __asm("d1") )
{
    struct IntContextNode *icn = top_node(IIFF(iff));
    LONG n;

    if (!icn)
        return IFFERR_EOF;
    if (bytesPerRecord <= 0 || numRecords <= 0)
        return 0;
    if (icn->CN.cn_Size != IFFSIZE_UNKNOWN)
    {
        LONG avail = (icn->CN.cn_Size - icn->CN.cn_Scan) / bytesPerRecord;
        if (numRecords > avail)
            numRecords = avail;
    }
    n = chunk_write(IIFF(iff), icn, buf, numRecords * bytesPerRecord);
    if (n < 0)
        return n;
    return n / bytesPerRecord;
}

LONG _iffparse_PushChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1"),
                           register LONG size __asm("d2") )
{
    struct IntIFFHandle *iiff = IIFF(iff);
    struct IntContextNode *parent, *icn;
    LONG hdr[2];
    LONG n, err;

    if (!iff)
        return IFFERR_NOMEM;

    parent = top_node(iiff);

    if (!(iff->iff_Flags & IFFF_WRITE))
    {
        /* read mode: push the next chunk of the current one */
        if (!parent)
            return IFFERR_EOF;
        return push_read(IFFParseBase, iiff);
    }

    if (!parent)
    {
        /* a stream holds exactly one top-level FORM, LIST or CAT */
        if (iiff->iff_TopDone)
            return IFFERR_EOF;
        iiff->iff_TopDone = TRUE;
        if (id != ID_FORM && id != ID_LIST && id != ID_CAT)
            return IFFERR_NOTIFF;
    }
    else
    {
        if (id == ID_PROP)
        {
            if (parent->CN.cn_ID != ID_LIST)
                return IFFERR_SYNTAX;
        }
        else if (!is_composite(id))
        {
            if (parent->CN.cn_ID != ID_FORM && parent->CN.cn_ID != ID_PROP)
                return IFFERR_SYNTAX;
            if (!_iffparse_GoodID(IFFParseBase, id))
                return IFFERR_SYNTAX;
        }
        if (id == ID_FORM && !_iffparse_GoodType(IFFParseBase, type))
            return IFFERR_NOTIFF;
    }

    hdr[0] = id;
    hdr[1] = size;
    if (parent)
    {
        n = chunk_write(iiff, parent, hdr, 8);
        if (n < 0)
            return n;
        if (n != 8)
            return IFFERR_WRITE;
    }
    else
    {
        err = stream_write(iiff, hdr, 8);
        if (err)
            return err;
    }

    icn = push_node(iiff, id, is_composite(id) ? type : (parent ? parent->CN.cn_Type : 0), size);
    if (!icn)
        return IFFERR_NOMEM;

    if (is_composite(id))
    {
        LONG t = type;
        n = chunk_write(iiff, icn, &t, 4);
        if (n < 0)
            return n;
        if (n != 4)
            return IFFERR_WRITE;
    }

    return 0;
}

LONG _iffparse_PopChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0") )
{
    if (!iff)
        return IFFERR_NOMEM;
    if (iff->iff_Flags & IFFF_WRITE)
        return pop_write(IFFParseBase, IIFF(iff));
    return pop_read(IFFParseBase, IIFF(iff));
}

ULONG _iffparse_Reserved ( void )
{
    PRIVATE_FUNCTION_ERROR("_iffparse", "Reserved");
    return 0;
}

static LONG add_handler(struct IFFParseBase *IFFParseBase, struct IFFHandle *iff,
                        LONG type, LONG id, LONG ident, LONG position,
                        struct Hook *handler, APTR object)
{
    struct LocalContextItem *lci;
    struct HandlerData *hd;
    LONG err;

    lci = _iffparse_AllocLocalItem(IFFParseBase, type, id, ident, sizeof(struct HandlerData));
    if (!lci)
        return IFFERR_NOMEM;
    hd = (struct HandlerData *)LCIDATA(lci);
    hd->hd_Hook = handler;
    hd->hd_Object = object;

    err = _iffparse_StoreLocalItem(IFFParseBase, iff, lci, position);
    if (err)
        _iffparse_FreeLocalItem(IFFParseBase, lci);
    return err;
}

LONG _iffparse_EntryHandler ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                              register struct IFFHandle *iff __asm("a0"),
                              register LONG type __asm("d0"),
                              register LONG id __asm("d1"),
                              register LONG position __asm("d2"),
                              register struct Hook *handler __asm("a1"),
                              register APTR object __asm("a2") )
{
    return add_handler(IFFParseBase, iff, type, id, IFFLCI_ENTRYHANDLER, position, handler, object);
}

LONG _iffparse_ExitHandler ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                             register struct IFFHandle *iff __asm("a0"),
                             register LONG type __asm("d0"),
                             register LONG id __asm("d1"),
                             register LONG position __asm("d2"),
                             register struct Hook *handler __asm("a1"),
                             register APTR object __asm("a2") )
{
    return add_handler(IFFParseBase, iff, type, id, IFFLCI_EXITHANDLER, position, handler, object);
}

/* PropChunk() & co. install entry/exit handlers in the root context */
LONG _iffparse_PropChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1") )
{
    return add_handler(IFFParseBase, iff, type, id, IFFLCI_ENTRYHANDLER, IFFSLI_ROOT,
                       &IFFParseBase->iff_PropHook, iff);
}

LONG _iffparse_PropChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG *propArray __asm("a1"),
                            register LONG numPairs __asm("d0") )
{
    LONG i, err;

    for (i = 0; i < numPairs; i++)
    {
        err = _iffparse_PropChunk(IFFParseBase, iff, propArray[i * 2], propArray[i * 2 + 1]);
        if (err)
            return err;
    }
    return 0;
}

LONG _iffparse_StopChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1") )
{
    return add_handler(IFFParseBase, iff, type, id, IFFLCI_ENTRYHANDLER, IFFSLI_ROOT,
                       &IFFParseBase->iff_StopHook, iff);
}

LONG _iffparse_StopChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG *propArray __asm("a1"),
                            register LONG numPairs __asm("d0") )
{
    LONG i, err;

    for (i = 0; i < numPairs; i++)
    {
        err = _iffparse_StopChunk(IFFParseBase, iff, propArray[i * 2], propArray[i * 2 + 1]);
        if (err)
            return err;
    }
    return 0;
}

LONG _iffparse_CollectionChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                 register struct IFFHandle *iff __asm("a0"),
                                 register LONG type __asm("d0"),
                                 register LONG id __asm("d1") )
{
    return add_handler(IFFParseBase, iff, type, id, IFFLCI_ENTRYHANDLER, IFFSLI_ROOT,
                       &IFFParseBase->iff_CollHook, iff);
}

LONG _iffparse_CollectionChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                  register struct IFFHandle *iff __asm("a0"),
                                  register LONG *propArray __asm("a1"),
                                  register LONG numPairs __asm("d0") )
{
    LONG i, err;

    for (i = 0; i < numPairs; i++)
    {
        err = _iffparse_CollectionChunk(IFFParseBase, iff, propArray[i * 2], propArray[i * 2 + 1]);
        if (err)
            return err;
    }
    return 0;
}

LONG _iffparse_StopOnExit ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG type __asm("d0"),
                            register LONG id __asm("d1") )
{
    return add_handler(IFFParseBase, iff, type, id, IFFLCI_EXITHANDLER, IFFSLI_ROOT,
                       &IFFParseBase->iff_StopExitHook, iff);
}

struct StoredProperty * _iffparse_FindProp ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                             register struct IFFHandle *iff __asm("a0"),
                                             register LONG type __asm("d0"),
                                             register LONG id __asm("d1") )
{
    struct LocalContextItem *lci = _iffparse_FindLocalItem(IFFParseBase, iff, type, id, IFFLCI_PROP);
    return lci ? (struct StoredProperty *)LCIDATA(lci) : NULL;
}

struct CollectionItem * _iffparse_FindCollection ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                   register struct IFFHandle *iff __asm("a0"),
                                                   register LONG type __asm("d0"),
                                                   register LONG id __asm("d1") )
{
    struct LocalContextItem *lci = _iffparse_FindLocalItem(IFFParseBase, iff, type, id, IFFLCI_COLLECTION);
    return lci ? ((struct CollectionData *)LCIDATA(lci))->cd_First : NULL;
}

/* the nearest FORM or LIST enclosing the current chunk */
struct ContextNode * _iffparse_FindPropContext ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                 register struct IFFHandle *iff __asm("a0") )
{
    struct IntContextNode *icn;

    if (!iff)
        return NULL;
    icn = top_node(IIFF(iff));
    if (!icn)
        return NULL;
    for (icn = parent_node(icn); icn; icn = parent_node(icn))
    {
        if (icn->CN.cn_ID == ID_FORM || icn->CN.cn_ID == ID_LIST)
            return &icn->CN;
    }
    return NULL;
}

struct ContextNode * _iffparse_CurrentChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                              register struct IFFHandle *iff __asm("a0") )
{
    struct IntContextNode *icn;

    if (!iff)
        return NULL;
    icn = top_node(IIFF(iff));
    return icn ? &icn->CN : NULL;
}

struct ContextNode * _iffparse_ParentChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                             register struct ContextNode *contextNode __asm("a0") )
{
    struct IntContextNode *p;

    if (!contextNode)
        return NULL;
    p = parent_node(ICN(contextNode));
    return p ? &p->CN : NULL;
}

struct LocalContextItem * _iffparse_AllocLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                     register LONG type __asm("d0"),
                                                     register LONG id __asm("d1"),
                                                     register LONG ident __asm("d2"),
                                                     register LONG dataSize __asm("d3") )
{
    struct IntLocalContextItem *ilci;

    if (dataSize < 0)
        dataSize = 0;
    ilci = AllocMem(sizeof(struct IntLocalContextItem) + dataSize, MEMF_ANY | MEMF_CLEAR);
    if (!ilci)
        return NULL;

    ilci->LCI.lci_Type = type;
    ilci->LCI.lci_ID = id;
    ilci->LCI.lci_Ident = ident;
    ilci->lci_DataSize = dataSize;
    return &ilci->LCI;
}

APTR _iffparse_LocalItemData ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct LocalContextItem *localItem __asm("a0") )
{
    return localItem ? LCIDATA(localItem) : NULL;
}

void _iffparse_SetLocalItemPurge ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                   register struct LocalContextItem *localItem __asm("a0"),
                                   register struct Hook *purgeHook __asm("a1") )
{
    if (localItem)
        ILCI(localItem)->lci_Purge = purgeHook;
}

void _iffparse_FreeLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct LocalContextItem *localItem __asm("a0") )
{
    if (localItem)
        FreeMem(localItem, sizeof(struct IntLocalContextItem) + ILCI(localItem)->lci_DataSize);
}

/* search the context stack from the current chunk down to the root context */
struct LocalContextItem * _iffparse_FindLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                    register struct IFFHandle *iff __asm("a0"),
                                                    register LONG type __asm("d0"),
                                                    register LONG id __asm("d1"),
                                                    register LONG ident __asm("d2") )
{
    struct IntIFFHandle *iiff = IIFF(iff);
    struct IntContextNode *icn;
    struct LocalContextItem *lci;

    if (!iff)
        return NULL;
    for (icn = top_node(iiff); icn; icn = parent_node(icn))
    {
        lci = find_in(icn, type, id, ident);
        if (lci)
            return lci;
    }
    return find_in(&iiff->iff_Default, type, id, ident);
}

LONG _iffparse_StoreLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                register struct IFFHandle *iff __asm("a0"),
                                register struct LocalContextItem *localItem __asm("a1"),
                                register LONG position __asm("d0") )
{
    struct IntIFFHandle *iiff = IIFF(iff);
    struct IntContextNode *icn;

    if (!iff || !localItem)
        return IFFERR_NOMEM;

    switch (position)
    {
        case IFFSLI_ROOT:
            icn = &iiff->iff_Default;
            break;
        case IFFSLI_TOP:
            icn = top_node(iiff);
            if (!icn)
                icn = &iiff->iff_Default;
            break;
        case IFFSLI_PROP:
            icn = ICN(_iffparse_FindPropContext(IFFParseBase, iff));
            if (!icn)
                return IFFERR_NOSCOPE;
            break;
        default:
            /* AmigaOS 3.1 accepts other positions without storing the item */
            return 0;
    }

    store_in(IFFParseBase, icn, localItem);
    return 0;
}

void _iffparse_StoreItemInContext ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                    register struct IFFHandle *iff __asm("a0"),
                                    register struct LocalContextItem *localItem __asm("a1"),
                                    register struct ContextNode *contextNode __asm("a2") )
{
    if (!localItem || !contextNode)
        return;
    store_in(IFFParseBase, ICN(contextNode), localItem);
}

void _iffparse_InitIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0"),
                         register LONG flags __asm("d0"),
                         register struct Hook *streamHook __asm("a1") )
{
    if (!iff)
        return;
    iff->iff_Flags = flags;
    IIFF(iff)->iff_Hook = streamHook;
}

void _iffparse_InitIFFasDOS ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                              register struct IFFHandle *iff __asm("a0") )
{
    _iffparse_InitIFF(IFFParseBase, iff, IFFF_FSEEK | IFFF_RSEEK, &IFFParseBase->iff_DOSHook);
}

void _iffparse_InitIFFasClip ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct IFFHandle *iff __asm("a0") )
{
    _iffparse_InitIFF(IFFParseBase, iff, IFFF_FSEEK | IFFF_RSEEK, &IFFParseBase->iff_ClipHook);
}

struct ClipboardHandle * _iffparse_OpenClipboard ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                    register LONG unitNumber __asm("d0") )
{
    struct IntClipboardHandle *clipHandle;
    struct MsgPort *reply_port;

    clipHandle = AllocMem(sizeof(struct IntClipboardHandle), MEMF_ANY | MEMF_CLEAR);
    if (!clipHandle)
        return NULL;

    reply_port = &clipHandle->cbh_Public.cbh_CBport;
    reply_port->mp_Node.ln_Type = NT_MSGPORT;
    reply_port->mp_Flags = PA_IGNORE;
    reply_port->mp_SigTask = FindTask(NULL);
    NewList(&reply_port->mp_MsgList);

    clipHandle->cbh_Public.cbh_SatisfyPort.mp_Node.ln_Type = NT_MSGPORT;
    clipHandle->cbh_Public.cbh_SatisfyPort.mp_Flags = PA_IGNORE;
    clipHandle->cbh_Public.cbh_SatisfyPort.mp_SigTask = FindTask(NULL);
    NewList(&clipHandle->cbh_Public.cbh_SatisfyPort.mp_MsgList);

    clipHandle->cbh_Public.cbh_Req.io_Message.mn_ReplyPort = reply_port;
    clipHandle->cbh_Public.cbh_Req.io_Message.mn_Length = sizeof(struct IOClipReq);

    if (OpenDevice((STRPTR)"clipboard.device", unitNumber,
                   (struct IORequest *)&clipHandle->cbh_Public.cbh_Req, 0) != 0)
    {
        FreeMem(clipHandle, sizeof(struct IntClipboardHandle));
        return NULL;
    }

    return &clipHandle->cbh_Public;
}

void _iffparse_CloseClipboard ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                register struct ClipboardHandle *clipHandle __asm("a0") )
{
    if (!clipHandle)
        return;
    CloseDevice((struct IORequest *)&clipHandle->cbh_Req);
    FreeMem(clipHandle, sizeof(struct IntClipboardHandle));
}

/* printable ASCII; no leading space except for the all-blank ID */
LONG _iffparse_GoodID ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                        register LONG id __asm("d0") )
{
    int i;

    for (i = 0; i < 4; i++)
    {
        UBYTE c = (UBYTE)(id >> (i * 8));
        if (c < 0x20 || c > 0x7e)
            return 0;
    }
    if (((ULONG)id >> 24) == ' ' && (ULONG)id != MAKE_ID(' ', ' ', ' ', ' '))
        return 0;
    return 1;
}

/* a GoodID of upper-case letters, digits and spaces only */
LONG _iffparse_GoodType ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register LONG type __asm("d0") )
{
    int i;

    if (!_iffparse_GoodID(IFFParseBase, type))
        return 0;
    for (i = 0; i < 4; i++)
    {
        UBYTE c = (UBYTE)(type >> (i * 8));
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' '))
            return 0;
    }
    return 1;
}

STRPTR _iffparse_IDtoStr ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register LONG id __asm("d0"),
                           register STRPTR buf __asm("a0") )
{
    if (buf)
    {
        buf[0] = (UBYTE)(id >> 24);
        buf[1] = (UBYTE)(id >> 16);
        buf[2] = (UBYTE)(id >> 8);
        buf[3] = (UBYTE)id;
        buf[4] = '\0';
    }
    return buf;
}

/*
 * Library structure definitions
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

extern APTR              __g_lxa_iffparse_FuncTab [];
extern struct MyDataInit __g_lxa_iffparse_DataTab;
extern struct InitTable  __g_lxa_iffparse_InitTab;
extern APTR              __g_lxa_iffparse_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                       // UWORD rt_MatchWord
    &ROMTag,                             // struct Resident *rt_MatchTag
    &__g_lxa_iffparse_EndResident,       // APTR  rt_EndSkip
    RTF_AUTOINIT,                        // UBYTE rt_Flags
    VERSION,                             // UBYTE rt_Version
    NT_LIBRARY,                          // UBYTE rt_Type
    0,                                   // BYTE  rt_Pri
    &_g_iffparse_ExLibName[0],           // char  *rt_Name
    &_g_iffparse_ExLibID[0],             // char  *rt_IdString
    &__g_lxa_iffparse_InitTab            // APTR  rt_Init
};

APTR __g_lxa_iffparse_EndResident;
struct Resident *__lxa_iffparse_ROMTag = &ROMTag;

struct InitTable __g_lxa_iffparse_InitTab =
{
    (ULONG)               sizeof(struct IFFParseBase),
    (APTR              *) &__g_lxa_iffparse_FuncTab[0],
    (APTR)                &__g_lxa_iffparse_DataTab,
    (APTR)                __g_lxa_iffparse_InitLib
};

APTR __g_lxa_iffparse_FuncTab [] =
{
    __g_lxa_iffparse_OpenLib,            // -6   Standard
    __g_lxa_iffparse_CloseLib,           // -12  Standard
    __g_lxa_iffparse_ExpungeLib,         // -18  Standard
    __g_lxa_iffparse_ExtFuncLib,         // -24  Standard (reserved)
    _iffparse_AllocIFF,                  // -30  AllocIFF
    _iffparse_OpenIFF,                   // -36  OpenIFF
    _iffparse_ParseIFF,                  // -42  ParseIFF
    _iffparse_CloseIFF,                  // -48  CloseIFF
    _iffparse_FreeIFF,                   // -54  FreeIFF
    _iffparse_ReadChunkBytes,            // -60  ReadChunkBytes
    _iffparse_WriteChunkBytes,           // -66  WriteChunkBytes
    _iffparse_ReadChunkRecords,          // -72  ReadChunkRecords
    _iffparse_WriteChunkRecords,         // -78  WriteChunkRecords
    _iffparse_PushChunk,                 // -84  PushChunk
    _iffparse_PopChunk,                  // -90  PopChunk
    _iffparse_Reserved,                  // -96  Reserved
    _iffparse_EntryHandler,              // -102 EntryHandler
    _iffparse_ExitHandler,               // -108 ExitHandler
    _iffparse_PropChunk,                 // -114 PropChunk
    _iffparse_PropChunks,                // -120 PropChunks
    _iffparse_StopChunk,                 // -126 StopChunk
    _iffparse_StopChunks,                // -132 StopChunks
    _iffparse_CollectionChunk,           // -138 CollectionChunk
    _iffparse_CollectionChunks,          // -144 CollectionChunks
    _iffparse_StopOnExit,                // -150 StopOnExit
    _iffparse_FindProp,                  // -156 FindProp
    _iffparse_FindCollection,            // -162 FindCollection
    _iffparse_FindPropContext,           // -168 FindPropContext
    _iffparse_CurrentChunk,              // -174 CurrentChunk
    _iffparse_ParentChunk,               // -180 ParentChunk
    _iffparse_AllocLocalItem,            // -186 AllocLocalItem
    _iffparse_LocalItemData,             // -192 LocalItemData
    _iffparse_SetLocalItemPurge,         // -198 SetLocalItemPurge
    _iffparse_FreeLocalItem,             // -204 FreeLocalItem
    _iffparse_FindLocalItem,             // -210 FindLocalItem
    _iffparse_StoreLocalItem,            // -216 StoreLocalItem
    _iffparse_StoreItemInContext,        // -222 StoreItemInContext
    _iffparse_InitIFF,                   // -228 InitIFF
    _iffparse_InitIFFasDOS,              // -234 InitIFFasDOS
    _iffparse_InitIFFasClip,             // -240 InitIFFasClip
    _iffparse_OpenClipboard,             // -246 OpenClipboard
    _iffparse_CloseClipboard,            // -252 CloseClipboard
    _iffparse_GoodID,                    // -258 GoodID
    _iffparse_GoodType,                  // -264 GoodType
    _iffparse_IDtoStr,                   // -270 IDtoStr
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_iffparse_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_iffparse_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_iffparse_ExLibID[0],
    (ULONG) 0
};
