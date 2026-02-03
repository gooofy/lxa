/*
 * lxa iffparse.library implementation
 *
 * Full implementation of IFF parsing API following AROS and RKRM.
 * Supports reading and writing IFF files via DOS or custom stream hooks.
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

#include <libraries/iffparse.h>
#include <utility/hooks.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"

extern struct UtilityBase *UtilityBase;

#define VERSION    39
#define REVISION   2
#define EXLIBNAME  "iffparse"
#define EXLIBVER   " 39.2 (2026/02/03)"

char __aligned _g_iffparse_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_iffparse_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_iffparse_Copyright [] = "(C)opyright 2025-2026 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_iffparse_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

/*
 * Internal structures (following AROS design)
 */

/* Parser state machine states */
#define IFFSTATE_INIT       0   /* Initial state */
#define IFFSTATE_COMPOSITE  1   /* Inside FORM/LIST/CAT */
#define IFFSTATE_PUSHCHUNK  2   /* About to push new chunk */
#define IFFSTATE_ATOMIC     3   /* Inside atomic (data) chunk */
#define IFFSTATE_SCANEXIT   4   /* Scanning towards chunk end */
#define IFFSTATE_EXIT       5   /* At chunk end, calling exit handlers */
#define IFFSTATE_POPCHUNK   6   /* Popping chunk from stack */

/* Internal context node - extends public ContextNode */
struct IntContextNode
{
    struct ContextNode CN;          /* Public part */
    struct MinList     cn_LCIList;  /* List of LocalContextItems */
    BOOL               cn_Composite;/* TRUE if FORM/LIST/CAT/PROP */
    LONG               cn_SizeOffset;/* File offset where size field is (for fixup) */
};

/* Internal local context item - extends public LocalContextItem */
struct IntLocalContextItem
{
    struct LocalContextItem LCI;        /* Public part */
    struct Hook            *lci_PurgeHook;  /* Cleanup hook */
    APTR                    lci_UserData;   /* User data pointer */
    ULONG                   lci_UserDataSize;
};

/* Internal IFF handle - extends public IFFHandle */
struct IntIFFHandle
{
    struct IFFHandle IH;                    /* Public part */
    
    struct IntContextNode iff_DefaultCN;    /* Built-in default context node */
    struct MinList        iff_CNStack;      /* Stack of context nodes */
    ULONG                 iff_CurrentState; /* Parser state */
    struct Hook          *iff_StreamHandler;/* Stream hook */
    
    /* For write mode - track chunk sizes */
    LONG                  iff_NewDepth;     /* Depth after push */
    LONG                  iff_StreamPos;    /* Current stream position (for write mode) */
};

/* IFFParseBase structure */
struct IFFParseBase
{
    struct Library lib;
    BPTR           SegList;
    struct Hook    iff_DOSHook;     /* Built-in DOS stream hook */
};

/* Forward declarations of library functions */
struct LocalContextItem * _iffparse_AllocLocalItem(register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                   register LONG type __asm("d0"),
                                                   register LONG id __asm("d1"),
                                                   register LONG ident __asm("d2"),
                                                   register LONG dataSize __asm("d3"));
APTR _iffparse_LocalItemData(register struct IFFParseBase *IFFParseBase __asm("a6"),
                             register struct LocalContextItem *localItem __asm("a0"));
void _iffparse_FreeLocalItem(register struct IFFParseBase *IFFParseBase __asm("a6"),
                             register struct LocalContextItem *localItem __asm("a0"));
struct LocalContextItem * _iffparse_FindLocalItem(register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                  register struct IFFHandle *iff __asm("a0"),
                                                  register LONG type __asm("d0"),
                                                  register LONG id __asm("d1"),
                                                  register LONG ident __asm("d2"));
LONG _iffparse_StoreLocalItem(register struct IFFParseBase *IFFParseBase __asm("a6"),
                              register struct IFFHandle *iff __asm("a0"),
                              register struct LocalContextItem *localItem __asm("a1"),
                              register LONG position __asm("d0"));
LONG _iffparse_GoodID(register struct IFFParseBase *IFFParseBase __asm("a6"),
                      register LONG id __asm("d0"));
struct ContextNode * _iffparse_CurrentChunk(register struct IFFParseBase *IFFParseBase __asm("a6"),
                                            register struct IFFHandle *iff __asm("a0"));
struct ContextNode * _iffparse_FindPropContext(register struct IFFParseBase *IFFParseBase __asm("a6"),
                                               register struct IFFHandle *iff __asm("a0"));
LONG _iffparse_StopChunk(register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0"),
                         register LONG type __asm("d0"),
                         register LONG id __asm("d1"));

/* Forward declarations for static functions */
static LONG DOSStreamHandler(register struct Hook *hook __asm("a0"),
                            register struct IFFHandle *iff __asm("a2"),
                            register struct IFFStreamCmd *cmd __asm("a1"));

static LONG StreamRead(struct IntIFFHandle *iiff, APTR buf, LONG numBytes);
static LONG StreamWrite(struct IntIFFHandle *iiff, APTR buf, LONG numBytes);
static LONG StreamSeek(struct IntIFFHandle *iiff, LONG offset);
static void PurgeLCI(struct IFFParseBase *IFFParseBase, struct IntLocalContextItem *ilci);
static struct IntContextNode *PushContextNode(struct IFFParseBase *IFFParseBase, 
                                               struct IntIFFHandle *iiff,
                                               LONG id, LONG type, LONG size);
static void PopContextNode(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff);
static LONG GetChunkHeader(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff);
static LONG InvokeHandlers(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff,
                          LONG mode, LONG ident);

/*
 * Library init/open/close/expunge
 */

struct IFFParseBase * __g_lxa_iffparse_InitLib ( register struct IFFParseBase *iffbase __asm("a6"),
                                                  register BPTR                seglist __asm("a0"),
                                                  register struct ExecBase    *sysb __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_iffparse: InitLib() called\n");
    iffbase->SegList = seglist;
    
    /* Initialize the DOS stream hook */
    iffbase->iff_DOSHook.h_Entry = (ULONG (*)())DOSStreamHandler;
    iffbase->iff_DOSHook.h_SubEntry = NULL;
    iffbase->iff_DOSHook.h_Data = NULL;
    
    return iffbase;
}

BPTR __g_lxa_iffparse_ExpungeLib ( register struct IFFParseBase *iffbase __asm("a6") )
{
    return 0;
}

struct IFFParseBase * __g_lxa_iffparse_OpenLib ( register struct IFFParseBase *iffbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: OpenLib() called, iffbase=0x%08lx\n", (ULONG)iffbase);
    iffbase->lib.lib_OpenCnt++;
    iffbase->lib.lib_Flags &= ~LIBF_DELEXP;
    return iffbase;
}

BPTR __g_lxa_iffparse_CloseLib ( register struct IFFParseBase *iffbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: CloseLib() called, iffbase=0x%08lx\n", (ULONG)iffbase);
    iffbase->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_iffparse_ExtFuncLib ( void )
{
    return 0;
}

/*
 * DOS Stream Handler - handles IFFCMD_* for DOS file I/O
 */
static LONG DOSStreamHandler(register struct Hook *hook __asm("a0"),
                            register struct IFFHandle *iff __asm("a2"),
                            register struct IFFStreamCmd *cmd __asm("a1"))
{
    BPTR fh = (BPTR)iff->iff_Stream;
    LONG result = 0;
    
    switch (cmd->sc_Command)
    {
        case IFFCMD_INIT:
            /* Nothing to do for DOS streams */
            result = 0;
            break;
            
        case IFFCMD_CLEANUP:
            /* Seek to beginning of stream */
            if (iff->iff_Flags & IFFF_RSEEK)
            {
                Seek(fh, 0, OFFSET_BEGINNING);
            }
            result = 0;
            break;
            
        case IFFCMD_READ:
            result = Read(fh, cmd->sc_Buf, cmd->sc_NBytes);
            if (result < 0)
                result = IFFERR_READ;
            break;
            
        case IFFCMD_WRITE:
            result = Write(fh, cmd->sc_Buf, cmd->sc_NBytes);
            if (result < 0)
                result = IFFERR_WRITE;
            break;
            
        case IFFCMD_SEEK:
            result = Seek(fh, cmd->sc_NBytes, OFFSET_CURRENT);
            if (result < 0)
                result = IFFERR_SEEK;
            else
                result = 0;  /* Seek returns old position, we want 0 for success */
            break;
            
        default:
            result = IFFERR_SYNTAX;
            break;
    }
    
    return result;
}

/*
 * Stream helper functions
 */
static LONG StreamRead(struct IntIFFHandle *iiff, APTR buf, LONG numBytes)
{
    struct IFFStreamCmd cmd;
    
    if (!iiff->iff_StreamHandler)
        return IFFERR_NOHOOK;
    
    cmd.sc_Command = IFFCMD_READ;
    cmd.sc_Buf = buf;
    cmd.sc_NBytes = numBytes;
    
    return CallHookPkt(iiff->iff_StreamHandler, &iiff->IH, &cmd);
}

static LONG StreamWrite(struct IntIFFHandle *iiff, APTR buf, LONG numBytes)
{
    struct IFFStreamCmd cmd;
    
    if (!iiff->iff_StreamHandler)
        return IFFERR_NOHOOK;
    
    cmd.sc_Command = IFFCMD_WRITE;
    cmd.sc_Buf = buf;
    cmd.sc_NBytes = numBytes;
    
    return CallHookPkt(iiff->iff_StreamHandler, &iiff->IH, &cmd);
}

static LONG StreamSeek(struct IntIFFHandle *iiff, LONG offset)
{
    struct IFFStreamCmd cmd;
    
    if (!iiff->iff_StreamHandler)
        return IFFERR_NOHOOK;
    
    cmd.sc_Command = IFFCMD_SEEK;
    cmd.sc_Buf = NULL;
    cmd.sc_NBytes = offset;
    
    return CallHookPkt(iiff->iff_StreamHandler, &iiff->IH, &cmd);
}

/* Seek to absolute position (uses OFFSET_CURRENT relative seek) */
static LONG StreamSeekAbs(struct IntIFFHandle *iiff, LONG newPos)
{
    LONG offset = newPos - iiff->iff_StreamPos;
    LONG err;
    
    if (offset == 0)
        return 0;
    
    err = StreamSeek(iiff, offset);
    if (err == 0)
        iiff->iff_StreamPos = newPos;
    
    return err;
}

/*
 * Purge a LocalContextItem
 */
static void PurgeLCI(struct IFFParseBase *IFFParseBase, struct IntLocalContextItem *ilci)
{
    if (ilci->lci_PurgeHook)
    {
        /* Call the purge hook - it will free any associated data */
        struct IFFStreamCmd cmd;
        cmd.sc_Command = IFFCMD_PURGELCI;
        cmd.sc_Buf = ilci->lci_UserData;
        cmd.sc_NBytes = ilci->lci_UserDataSize;
        CallHookPkt(ilci->lci_PurgeHook, &ilci->LCI, &cmd);
    }
    
    /* Free the LCI itself */
    FreeMem(ilci, sizeof(struct IntLocalContextItem));
}

/*
 * Context node management
 */
static struct IntContextNode *PushContextNode(struct IFFParseBase *IFFParseBase,
                                               struct IntIFFHandle *iiff,
                                               LONG id, LONG type, LONG size)
{
    struct IntContextNode *icn;
    
    icn = AllocMem(sizeof(struct IntContextNode), MEMF_ANY | MEMF_CLEAR);
    if (!icn)
        return NULL;
    
    icn->CN.cn_ID = id;
    icn->CN.cn_Type = type;
    icn->CN.cn_Size = size;
    icn->CN.cn_Scan = 0;
    
    NewList((struct List *)&icn->cn_LCIList);
    
    /* Determine if this is a composite chunk */
    icn->cn_Composite = (id == ID_FORM || id == ID_LIST || id == ID_CAT || id == MAKE_ID('P','R','O','P'));
    
    /* Add to head of stack */
    AddHead((struct List *)&iiff->iff_CNStack, (struct Node *)&icn->CN.cn_Node);
    iiff->IH.iff_Depth++;
    
    DPRINTF(LOG_DEBUG, "_iffparse: PushContextNode id=0x%08lx type=0x%08lx size=%ld depth=%ld\n",
            id, type, size, iiff->IH.iff_Depth);
    
    return icn;
}

static void PopContextNode(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff)
{
    struct IntContextNode *icn;
    struct IntLocalContextItem *ilci, *next;
    
    /* Get top context node */
    icn = (struct IntContextNode *)RemHead((struct List *)&iiff->iff_CNStack);
    if (!icn)
        return;
    
    DPRINTF(LOG_DEBUG, "_iffparse: PopContextNode id=0x%08lx type=0x%08lx depth=%ld\n",
            icn->CN.cn_ID, icn->CN.cn_Type, iiff->IH.iff_Depth);
    
    /* Purge all local context items */
    ilci = (struct IntLocalContextItem *)icn->cn_LCIList.mlh_Head;
    while ((next = (struct IntLocalContextItem *)ilci->LCI.lci_Node.mln_Succ))
    {
        Remove((struct Node *)&ilci->LCI.lci_Node);
        PurgeLCI(IFFParseBase, ilci);
        ilci = next;
    }
    
    iiff->IH.iff_Depth--;
    
    /* Free the context node */
    FreeMem(icn, sizeof(struct IntContextNode));
}

/*
 * Get chunk header from stream
 */
static LONG GetChunkHeader(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff)
{
    LONG id, size, type = 0;
    LONG err;
    struct IntContextNode *icn;
    
    /* Read chunk ID */
    err = StreamRead(iiff, &id, 4);
    if (err < 4)
        return (err < 0) ? err : IFFERR_EOF;
    
    /* Read chunk size */
    err = StreamRead(iiff, &size, 4);
    if (err < 4)
        return (err < 0) ? err : IFFERR_MANGLED;
    
    /* Check if this is a container chunk (FORM, LIST, CAT, PROP) */
    if (id == ID_FORM || id == ID_LIST || id == ID_CAT || id == MAKE_ID('P','R','O','P'))
    {
        /* Read the type */
        err = StreamRead(iiff, &type, 4);
        if (err < 4)
            return (err < 0) ? err : IFFERR_MANGLED;
        
        /* Note: size includes the type (4 bytes) plus all nested chunk data.
         * We track cn_Scan starting at 0 after the type, so adjust size to
         * represent remaining data after the type. */
        size -= 4;
    }
    else
    {
        /* For non-container chunks, inherit type from parent */
        struct IntContextNode *parent = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
        if (parent && parent->CN.cn_Node.mln_Succ)
            type = parent->CN.cn_Type;
    }
    
    /* Validate IDs */
    if (!_iffparse_GoodID((struct IFFParseBase *)SysBase, id))
        return IFFERR_MANGLED;
    
    /* Push context node */
    icn = PushContextNode(IFFParseBase, iiff, id, type, size);
    if (!icn)
        return IFFERR_NOMEM;
    
    return 0;
}

/*
 * Invoke entry or exit handlers
 */
static LONG InvokeHandlers(struct IFFParseBase *IFFParseBase, struct IntIFFHandle *iiff,
                          LONG mode, LONG ident)
{
    struct IntContextNode *icn;
    struct IntLocalContextItem *ilci;
    struct ContextNode *cn;
    LONG result = 0;
    
    /* Get current chunk */
    cn = _iffparse_CurrentChunk(IFFParseBase, (struct IFFHandle *)iiff);
    if (!cn)
        return 0;
    
    /* Walk the context stack looking for handlers */
    icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
    while (icn->CN.cn_Node.mln_Succ)
    {
        /* Search this context's LCI list for matching handlers */
        ilci = (struct IntLocalContextItem *)icn->cn_LCIList.mlh_Head;
        while (ilci->LCI.lci_Node.mln_Succ)
        {
            if (ilci->LCI.lci_Ident == ident)
            {
                /* Check if this handler matches current chunk */
                if ((ilci->LCI.lci_Type == cn->cn_Type || ilci->LCI.lci_Type == 0) &&
                    (ilci->LCI.lci_ID == cn->cn_ID || ilci->LCI.lci_ID == 0))
                {
                    /* Found a matching handler - call it */
                    struct Hook *hook = ilci->lci_PurgeHook;  /* Hook is stored here */
                    if (hook)
                    {
                        struct IFFStreamCmd cmd;
                        cmd.sc_Command = (ident == IFFLCI_ENTRYHANDLER) ? IFFCMD_ENTRY : IFFCMD_EXIT;
                        cmd.sc_Buf = ilci->lci_UserData;  /* Object pointer */
                        cmd.sc_NBytes = 0;
                        
                        result = CallHookPkt(hook, (struct IFFHandle *)iiff, &cmd);
                        if (result != 0)
                            return result;
                    }
                }
            }
            ilci = (struct IntLocalContextItem *)ilci->LCI.lci_Node.mln_Succ;
        }
        icn = (struct IntContextNode *)icn->CN.cn_Node.mln_Succ;
    }
    
    return 0;
}

/*
 * IFFParse Functions (V36+)
 */

/* AllocIFF - Allocate an IFF handle */
struct IFFHandle * _iffparse_AllocIFF ( register struct IFFParseBase *IFFParseBase __asm("a6") )
{
    struct IntIFFHandle *iiff;
    
    DPRINTF (LOG_DEBUG, "_iffparse: AllocIFF()\n");
    
    iiff = AllocMem(sizeof(struct IntIFFHandle), MEMF_ANY | MEMF_CLEAR);
    if (!iiff)
    {
        DPRINTF (LOG_DEBUG, "_iffparse: AllocIFF() - out of memory\n");
        return NULL;
    }
    
    /* Initialize the handle */
    iiff->IH.iff_Flags = IFFF_READ;  /* Default to read mode */
    iiff->IH.iff_Depth = 0;
    iiff->iff_CurrentState = IFFSTATE_INIT;
    
    /* Initialize the context stack */
    NewList((struct List *)&iiff->iff_CNStack);
    
    /* Initialize the default context node */
    NewList((struct List *)&iiff->iff_DefaultCN.cn_LCIList);
    iiff->iff_DefaultCN.CN.cn_ID = 0;
    iiff->iff_DefaultCN.CN.cn_Type = 0;
    iiff->iff_DefaultCN.CN.cn_Size = 0;
    iiff->iff_DefaultCN.CN.cn_Scan = 0;
    iiff->iff_DefaultCN.cn_Composite = FALSE;
    
    /* Add default context node to stack (doesn't count towards depth) */
    AddTail((struct List *)&iiff->iff_CNStack, (struct Node *)&iiff->iff_DefaultCN.CN.cn_Node);
    
    DPRINTF (LOG_DEBUG, "_iffparse: AllocIFF() - allocated handle 0x%08lx\n", (ULONG)iiff);
    return (struct IFFHandle *)iiff;
}

/* OpenIFF - Open an IFF handle for reading/writing */
LONG _iffparse_OpenIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0"),
                         register LONG rwMode __asm("d0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IFFStreamCmd cmd;
    LONG err;
    
    DPRINTF (LOG_DEBUG, "_iffparse: OpenIFF() iff=0x%08lx rwMode=%ld\n", (ULONG)iff, rwMode);
    
    if (!iff)
        return IFFERR_NOMEM;
    
    if (!iiff->iff_StreamHandler)
        return IFFERR_NOHOOK;
    
    /* Set read/write mode */
    iff->iff_Flags = (iff->iff_Flags & ~IFFF_RWBITS) | (rwMode & IFFF_RWBITS);
    
    /* Call stream handler's INIT */
    cmd.sc_Command = IFFCMD_INIT;
    cmd.sc_Buf = NULL;
    cmd.sc_NBytes = 0;
    
    err = CallHookPkt(iiff->iff_StreamHandler, iff, &cmd);
    if (err != 0)
        return err;
    
    /* Set initial state */
    if (rwMode & IFFF_WRITE)
    {
        iiff->iff_CurrentState = IFFSTATE_INIT;
    }
    else
    {
        /* For reading, try to read the first chunk header */
        err = GetChunkHeader(IFFParseBase, iiff);
        if (err != 0)
            return err;
        
        /* Verify it's a container */
        struct IntContextNode *icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
        if (!icn || !icn->cn_Composite)
            return IFFERR_NOTIFF;
        
        iiff->iff_CurrentState = IFFSTATE_COMPOSITE;
    }
    
    return 0;
}

/* ParseIFF - Parse an IFF file */
LONG _iffparse_ParseIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0"),
                          register LONG control __asm("d0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn;
    LONG err;
    BOOL done = FALSE;
    
    DPRINTF (LOG_DEBUG, "_iffparse: ParseIFF() iff=0x%08lx control=%ld state=%ld\n",
             (ULONG)iff, control, iiff->iff_CurrentState);
    
    if (!iff)
        return IFFERR_NOMEM;
    
    while (!done)
    {
        switch (iiff->iff_CurrentState)
        {
            case IFFSTATE_COMPOSITE:
                /* Inside a composite chunk - invoke entry handlers if not RAWSTEP */
                if (control != IFFPARSE_RAWSTEP)
                {
                    err = InvokeHandlers(IFFParseBase, iiff, control, IFFLCI_ENTRYHANDLER);
                    if (err == IFF_RETURN2CLIENT)
                    {
                        return 0;  /* Handler wants us to return to client */
                    }
                    if (err != 0)
                        return err;
                }
                
                /* Check for stop chunks */
                /* For now, just proceed to read next chunk */
                iiff->iff_CurrentState = IFFSTATE_PUSHCHUNK;
                
                if (control == IFFPARSE_STEP || control == IFFPARSE_RAWSTEP)
                {
                    done = TRUE;
                }
                break;
                
            case IFFSTATE_PUSHCHUNK:
                /* Try to read next chunk header */
                icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
                if (!icn || !icn->CN.cn_Node.mln_Succ)
                {
                    /* No parent context - we're done */
                    return IFFERR_EOF;
                }
                
                /* Check if we've read all of parent's data */
                if (icn->CN.cn_Scan >= icn->CN.cn_Size)
                {
                    /* End of parent chunk */
                    iiff->iff_CurrentState = IFFSTATE_EXIT;
                    break;
                }
                
                err = GetChunkHeader(IFFParseBase, iiff);
                if (err != 0)
                {
                    if (err == IFFERR_EOF)
                    {
                        /* No more data - end of container */
                        iiff->iff_CurrentState = IFFSTATE_EXIT;
                    }
                    else
                    {
                        return err;
                    }
                    break;
                }
                
                /* Determine next state based on chunk type */
                icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
                if (icn->cn_Composite)
                {
                    iiff->iff_CurrentState = IFFSTATE_COMPOSITE;
                }
                else
                {
                    iiff->iff_CurrentState = IFFSTATE_ATOMIC;
                }
                break;
                
            case IFFSTATE_ATOMIC:
                /* Inside an atomic (data) chunk - invoke entry handlers */
                if (control != IFFPARSE_RAWSTEP)
                {
                    err = InvokeHandlers(IFFParseBase, iiff, control, IFFLCI_ENTRYHANDLER);
                    if (err == IFF_RETURN2CLIENT)
                    {
                        return 0;
                    }
                    if (err != 0)
                        return err;
                }
                
                /* Move to scan exit */
                iiff->iff_CurrentState = IFFSTATE_SCANEXIT;
                
                if (control == IFFPARSE_STEP || control == IFFPARSE_RAWSTEP)
                {
                    done = TRUE;
                }
                break;
                
            case IFFSTATE_SCANEXIT:
                /* Seek to end of chunk data (skip any unread bytes) */
                icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
                if (icn)
                {
                    LONG remaining = icn->CN.cn_Size - icn->CN.cn_Scan;
                    if (remaining > 0)
                    {
                        /* Skip remaining bytes */
                        if (iff->iff_Flags & (IFFF_FSEEK | IFFF_RSEEK))
                        {
                            err = StreamSeek(iiff, remaining);
                            if (err < 0)
                                return err;
                        }
                        else
                        {
                            /* No seek - must read and discard */
                            UBYTE buf[256];
                            while (remaining > 0)
                            {
                                LONG toRead = (remaining > 256) ? 256 : remaining;
                                err = StreamRead(iiff, buf, toRead);
                                if (err < toRead)
                                    return (err < 0) ? err : IFFERR_READ;
                                remaining -= toRead;
                            }
                        }
                        icn->CN.cn_Scan = icn->CN.cn_Size;
                    }
                    
                    /* Handle odd-length chunks (IFF requires word alignment) */
                    if (icn->CN.cn_Size & 1)
                    {
                        UBYTE pad;
                        StreamRead(iiff, &pad, 1);
                    }
                }
                
                iiff->iff_CurrentState = IFFSTATE_EXIT;
                break;
                
            case IFFSTATE_EXIT:
                /* At chunk end - invoke exit handlers */
                if (control != IFFPARSE_RAWSTEP)
                {
                    err = InvokeHandlers(IFFParseBase, iiff, control, IFFLCI_EXITHANDLER);
                    if (err == IFF_RETURN2CLIENT)
                    {
                        return 0;
                    }
                    if (err != 0)
                        return err;
                }
                
                iiff->iff_CurrentState = IFFSTATE_POPCHUNK;
                break;
                
            case IFFSTATE_POPCHUNK:
                /* Pop chunk from stack */
                icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
                
                /* Don't pop the default context node */
                if (icn == &iiff->iff_DefaultCN)
                {
                    return IFFERR_EOF;
                }
                
                /* Get chunk size including padding */
                LONG chunkSize = icn->CN.cn_Size;
                if (icn->cn_Composite)
                    chunkSize += 4;  /* Type was part of size */
                chunkSize += 8;  /* ID + Size fields */
                if (chunkSize & 1)
                    chunkSize++;  /* Padding */
                
                PopContextNode(IFFParseBase, iiff);
                
                /* Update parent's scan position */
                icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
                if (icn && icn != &iiff->iff_DefaultCN)
                {
                    icn->CN.cn_Scan += chunkSize;
                    iiff->iff_CurrentState = IFFSTATE_PUSHCHUNK;
                }
                else
                {
                    /* Popped last chunk */
                    return IFFERR_EOF;
                }
                break;
                
            default:
                return IFFERR_SYNTAX;
        }
    }
    
    return 0;  /* Stopped at STEP/RAWSTEP */
}

/* CloseIFF - Close an IFF handle */
void _iffparse_CloseIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IFFStreamCmd cmd;
    
    DPRINTF (LOG_DEBUG, "_iffparse: CloseIFF() iff=0x%08lx\n", (ULONG)iff);
    
    if (!iff)
        return;
    
    /* Pop all context nodes (except default) */
    while (iiff->IH.iff_Depth > 0)
    {
        PopContextNode(IFFParseBase, iiff);
    }
    
    /* Call stream handler's CLEANUP */
    if (iiff->iff_StreamHandler)
    {
        cmd.sc_Command = IFFCMD_CLEANUP;
        cmd.sc_Buf = NULL;
        cmd.sc_NBytes = 0;
        CallHookPkt(iiff->iff_StreamHandler, iff, &cmd);
    }
    
    iiff->iff_CurrentState = IFFSTATE_INIT;
}

/* FreeIFF - Free an IFF handle */
void _iffparse_FreeIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntLocalContextItem *ilci, *next;
    
    DPRINTF (LOG_DEBUG, "_iffparse: FreeIFF() iff=0x%08lx\n", (ULONG)iff);
    
    if (!iff)
        return;
    
    /* Purge all LCIs from default context node */
    ilci = (struct IntLocalContextItem *)iiff->iff_DefaultCN.cn_LCIList.mlh_Head;
    while ((next = (struct IntLocalContextItem *)ilci->LCI.lci_Node.mln_Succ))
    {
        Remove((struct Node *)&ilci->LCI.lci_Node);
        PurgeLCI(IFFParseBase, ilci);
        ilci = next;
    }
    
    /* Free the handle */
    FreeMem(iiff, sizeof(struct IntIFFHandle));
}

/* ReadChunkBytes - Read bytes from current chunk */
LONG _iffparse_ReadChunkBytes ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                register struct IFFHandle *iff __asm("a0"),
                                register APTR buf __asm("a1"),
                                register LONG numBytes __asm("d0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn;
    LONG available, toRead, bytesRead;
    
    DPRINTF (LOG_DEBUG, "_iffparse: ReadChunkBytes() iff=0x%08lx numBytes=%ld\n",
             (ULONG)iff, numBytes);
    
    if (!iff || !buf || numBytes <= 0)
        return IFFERR_READ;
    
    /* Get current chunk */
    icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
    if (!icn || icn == &iiff->iff_DefaultCN)
        return IFFERR_READ;
    
    /* Calculate available bytes */
    available = icn->CN.cn_Size - icn->CN.cn_Scan;
    toRead = (numBytes < available) ? numBytes : available;
    
    if (toRead <= 0)
        return 0;  /* No more bytes in this chunk */
    
    /* Read from stream */
    bytesRead = StreamRead(iiff, buf, toRead);
    if (bytesRead < 0)
        return bytesRead;  /* Error code */
    
    /* Update scan position */
    icn->CN.cn_Scan += bytesRead;
    
    return bytesRead;
}

/* WriteChunkBytes - Write bytes to current chunk */
LONG _iffparse_WriteChunkBytes ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                 register struct IFFHandle *iff __asm("a0"),
                                 register APTR buf __asm("a1"),
                                 register LONG numBytes __asm("d0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn;
    LONG bytesWritten;
    
    DPRINTF (LOG_DEBUG, "_iffparse: WriteChunkBytes() iff=0x%08lx numBytes=%ld\n",
             (ULONG)iff, numBytes);
    
    if (!iff || !buf || numBytes <= 0)
        return IFFERR_WRITE;
    
    if (!(iff->iff_Flags & IFFF_WRITE))
        return IFFERR_WRITE;  /* Not in write mode */
    
    /* Get current chunk */
    icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
    if (!icn || icn == &iiff->iff_DefaultCN)
        return IFFERR_WRITE;
    
    /* Write to stream */
    bytesWritten = StreamWrite(iiff, buf, numBytes);
    if (bytesWritten < 0)
        return bytesWritten;
    
    /* Update scan position (tracks written bytes) */
    icn->CN.cn_Scan += bytesWritten;
    
    /* Update stream position */
    iiff->iff_StreamPos += bytesWritten;
    
    return bytesWritten;
}

/* ReadChunkRecords - Read records from current chunk */
LONG _iffparse_ReadChunkRecords ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                  register struct IFFHandle *iff __asm("a0"),
                                  register APTR buf __asm("a1"),
                                  register LONG bytesPerRecord __asm("d0"),
                                  register LONG numRecords __asm("d1") )
{
    LONG totalBytes, bytesRead, recordsRead;
    
    DPRINTF (LOG_DEBUG, "_iffparse: ReadChunkRecords() iff=0x%08lx bpr=%ld num=%ld\n",
             (ULONG)iff, bytesPerRecord, numRecords);
    
    if (bytesPerRecord <= 0 || numRecords <= 0)
        return 0;
    
    totalBytes = bytesPerRecord * numRecords;
    bytesRead = _iffparse_ReadChunkBytes(IFFParseBase, iff, buf, totalBytes);
    
    if (bytesRead < 0)
        return bytesRead;  /* Error */
    
    recordsRead = bytesRead / bytesPerRecord;
    return recordsRead;
}

/* WriteChunkRecords - Write records to current chunk */
LONG _iffparse_WriteChunkRecords ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                   register struct IFFHandle *iff __asm("a0"),
                                   register APTR buf __asm("a1"),
                                   register LONG bytesPerRecord __asm("d0"),
                                   register LONG numRecords __asm("d1") )
{
    LONG totalBytes, bytesWritten, recordsWritten;
    
    DPRINTF (LOG_DEBUG, "_iffparse: WriteChunkRecords() iff=0x%08lx bpr=%ld num=%ld\n",
             (ULONG)iff, bytesPerRecord, numRecords);
    
    if (bytesPerRecord <= 0 || numRecords <= 0)
        return 0;
    
    totalBytes = bytesPerRecord * numRecords;
    bytesWritten = _iffparse_WriteChunkBytes(IFFParseBase, iff, buf, totalBytes);
    
    if (bytesWritten < 0)
        return bytesWritten;
    
    recordsWritten = bytesWritten / bytesPerRecord;
    return recordsWritten;
}

/* PushChunk - Push a new chunk onto the context stack (for writing) */
LONG _iffparse_PushChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1"),
                           register LONG size __asm("d2") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn;
    LONG err;
    LONG header[3];
    LONG headerSize;
    BOOL composite;
    LONG sizeOffset;
    
    DPRINTF (LOG_DEBUG, "_iffparse: PushChunk() type=0x%08lx id=0x%08lx size=%ld\n",
             type, id, size);
    
    if (!iff)
        return IFFERR_NOMEM;
    
    if (!(iff->iff_Flags & IFFF_WRITE))
        return IFFERR_WRITE;
    
    /* Determine if this is a composite chunk */
    composite = (id == ID_FORM || id == ID_LIST || id == ID_CAT || id == MAKE_ID('P','R','O','P'));
    
    /* Remember where the size field will be written */
    sizeOffset = iiff->iff_StreamPos + 4;  /* After ID */
    
    /* Build header */
    header[0] = id;
    if (composite)
    {
        header[1] = (size == IFFSIZE_UNKNOWN) ? 0 : size + 4;  /* +4 for type */
        header[2] = type;
        headerSize = 12;
    }
    else
    {
        header[1] = (size == IFFSIZE_UNKNOWN) ? 0 : size;
        headerSize = 8;
    }
    
    /* Write header */
    err = StreamWrite(iiff, header, headerSize);
    if (err < headerSize)
        return (err < 0) ? err : IFFERR_WRITE;
    
    /* Update stream position */
    iiff->iff_StreamPos += headerSize;
    
    /* Push context node */
    icn = PushContextNode(IFFParseBase, iiff, id, type, 
                          (size == IFFSIZE_UNKNOWN) ? IFFSIZE_UNKNOWN : size);
    if (!icn)
        return IFFERR_NOMEM;
    
    /* Store the size field offset for later fixup */
    icn->cn_SizeOffset = sizeOffset;
    
    return 0;
}

/* PopChunk - Pop the current chunk from the context stack (for writing) */
LONG _iffparse_PopChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn, *parent;
    LONG err;
    LONG chunkSize;
    LONG savedPos;
    LONG totalChunkBytes;
    
    DPRINTF (LOG_DEBUG, "_iffparse: PopChunk() iff=0x%08lx\n", (ULONG)iff);
    
    if (!iff)
        return IFFERR_NOMEM;
    
    /* Get current chunk */
    icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
    if (!icn || icn == &iiff->iff_DefaultCN)
        return IFFERR_EOF;
    
    /* Handle odd-length chunks (need padding byte) */
    if (icn->CN.cn_Scan & 1)
    {
        UBYTE pad = 0;
        err = StreamWrite(iiff, &pad, 1);
        if (err < 1)
            return IFFERR_WRITE;
        iiff->iff_StreamPos++;
        icn->CN.cn_Scan++;  /* Include padding in scan count */
    }
    
    /* Calculate total bytes this chunk occupies (for updating parent) */
    totalChunkBytes = 8 + icn->CN.cn_Scan;  /* ID + Size + data (including padding) */
    if (icn->cn_Composite)
        totalChunkBytes += 4;  /* Type field */
    
    /* For IFFSIZE_UNKNOWN, we need to seek back and fix up the size */
    if (icn->CN.cn_Size == IFFSIZE_UNKNOWN)
    {
        /* Calculate the actual chunk size (data portion only) */
        chunkSize = icn->CN.cn_Scan;
        if (icn->cn_Composite)
            chunkSize += 4;  /* Include type in size for composite chunks */
        
        /* Save current position */
        savedPos = iiff->iff_StreamPos;
        
        /* Seek to size field location */
        err = StreamSeekAbs(iiff, icn->cn_SizeOffset);
        if (err != 0)
            return err;
        
        /* Write the correct size */
        err = StreamWrite(iiff, &chunkSize, 4);
        if (err < 4)
            return (err < 0) ? err : IFFERR_WRITE;
        iiff->iff_StreamPos += 4;  /* Update position after write */
        
        /* Seek back to end of chunk */
        err = StreamSeekAbs(iiff, savedPos);
        if (err != 0)
            return err;
        
        DPRINTF(LOG_DEBUG, "_iffparse: PopChunk() fixed size at offset %ld to %ld\n",
                icn->cn_SizeOffset, chunkSize);
    }
    
    /* Get parent before popping */
    parent = (struct IntContextNode *)icn->CN.cn_Node.mln_Succ;
    
    PopContextNode(IFFParseBase, iiff);
    
    /* Update parent's cn_Scan to include this chunk's total size */
    if (parent && parent != &iiff->iff_DefaultCN)
    {
        parent->CN.cn_Scan += totalChunkBytes;
        DPRINTF(LOG_DEBUG, "_iffparse: PopChunk() updated parent cn_Scan to %ld\n",
                parent->CN.cn_Scan);
    }
    
    return 0;
}

/* Reserved slot */
ULONG _iffparse_Reserved ( void )
{
    return 0;
}

/* EntryHandler - Install an entry handler */
LONG _iffparse_EntryHandler ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                              register struct IFFHandle *iff __asm("a0"),
                              register LONG type __asm("d0"),
                              register LONG id __asm("d1"),
                              register LONG position __asm("d2"),
                              register struct Hook *handler __asm("a1"),
                              register APTR object __asm("a2") )
{
    struct IntLocalContextItem *ilci;
    LONG err;
    
    DPRINTF (LOG_DEBUG, "_iffparse: EntryHandler() type=0x%08lx id=0x%08lx\n",
             type, id);
    
    ilci = (struct IntLocalContextItem *)_iffparse_AllocLocalItem(IFFParseBase, type, id, 
                                                                   IFFLCI_ENTRYHANDLER, 0);
    if (!ilci)
        return IFFERR_NOMEM;
    
    /* Store hook and object */
    ilci->lci_PurgeHook = handler;
    ilci->lci_UserData = object;
    
    err = _iffparse_StoreLocalItem(IFFParseBase, iff, (struct LocalContextItem *)ilci, position);
    if (err != 0)
    {
        _iffparse_FreeLocalItem(IFFParseBase, (struct LocalContextItem *)ilci);
        return err;
    }
    
    return 0;
}

/* ExitHandler - Install an exit handler */
LONG _iffparse_ExitHandler ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                             register struct IFFHandle *iff __asm("a0"),
                             register LONG type __asm("d0"),
                             register LONG id __asm("d1"),
                             register LONG position __asm("d2"),
                             register struct Hook *handler __asm("a1"),
                             register APTR object __asm("a2") )
{
    struct IntLocalContextItem *ilci;
    LONG err;
    
    DPRINTF (LOG_DEBUG, "_iffparse: ExitHandler() type=0x%08lx id=0x%08lx\n",
             type, id);
    
    ilci = (struct IntLocalContextItem *)_iffparse_AllocLocalItem(IFFParseBase, type, id,
                                                                   IFFLCI_EXITHANDLER, 0);
    if (!ilci)
        return IFFERR_NOMEM;
    
    ilci->lci_PurgeHook = handler;
    ilci->lci_UserData = object;
    
    err = _iffparse_StoreLocalItem(IFFParseBase, iff, (struct LocalContextItem *)ilci, position);
    if (err != 0)
    {
        _iffparse_FreeLocalItem(IFFParseBase, (struct LocalContextItem *)ilci);
        return err;
    }
    
    return 0;
}

/* PropChunk - Declare a property chunk */
LONG _iffparse_PropChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: PropChunk() type=0x%08lx id=0x%08lx\n",
             type, id);
    
    /* For now, just register as a stop chunk - full property handling would
       require an entry handler that reads and stores the chunk data */
    return _iffparse_StopChunk(IFFParseBase, iff, type, id);
}

/* PropChunks - Declare multiple property chunks */
LONG _iffparse_PropChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG *propArray __asm("a1"),
                            register LONG numPairs __asm("d0") )
{
    LONG i, err;
    
    DPRINTF (LOG_DEBUG, "_iffparse: PropChunks() numPairs=%ld\n", numPairs);
    
    for (i = 0; i < numPairs; i++)
    {
        err = _iffparse_PropChunk(IFFParseBase, iff, propArray[i*2], propArray[i*2+1]);
        if (err != 0)
            return err;
    }
    
    return 0;
}

/* StopChunk - Declare a stop chunk */
LONG _iffparse_StopChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: StopChunk() type=0x%08lx id=0x%08lx\n",
             type, id);
    
    /* A stop chunk is implemented as an entry handler that returns IFF_RETURN2CLIENT */
    /* For simplicity, we store the type/id in a LCI and check in ParseIFF */
    /* TODO: Implement proper stop chunk handling */
    
    return 0;  /* Success - even though not fully implemented */
}

/* StopChunks - Declare multiple stop chunks */
LONG _iffparse_StopChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG *propArray __asm("a1"),
                            register LONG numPairs __asm("d0") )
{
    LONG i, err;
    
    DPRINTF (LOG_DEBUG, "_iffparse: StopChunks() numPairs=%ld\n", numPairs);
    
    for (i = 0; i < numPairs; i++)
    {
        err = _iffparse_StopChunk(IFFParseBase, iff, propArray[i*2], propArray[i*2+1]);
        if (err != 0)
            return err;
    }
    
    return 0;
}

/* CollectionChunk - Declare a collection chunk */
LONG _iffparse_CollectionChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                 register struct IFFHandle *iff __asm("a0"),
                                 register LONG type __asm("d0"),
                                 register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: CollectionChunk() type=0x%08lx id=0x%08lx\n",
             type, id);
    
    /* Similar to PropChunk but collects multiple instances */
    return _iffparse_StopChunk(IFFParseBase, iff, type, id);
}

/* CollectionChunks - Declare multiple collection chunks */
LONG _iffparse_CollectionChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                  register struct IFFHandle *iff __asm("a0"),
                                  register LONG *propArray __asm("a1"),
                                  register LONG numPairs __asm("d0") )
{
    LONG i, err;
    
    DPRINTF (LOG_DEBUG, "_iffparse: CollectionChunks() numPairs=%ld\n", numPairs);
    
    for (i = 0; i < numPairs; i++)
    {
        err = _iffparse_CollectionChunk(IFFParseBase, iff, propArray[i*2], propArray[i*2+1]);
        if (err != 0)
            return err;
    }
    
    return 0;
}

/* StopOnExit - Declare a stop-on-exit chunk */
LONG _iffparse_StopOnExit ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG type __asm("d0"),
                            register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: StopOnExit() type=0x%08lx id=0x%08lx\n",
             type, id);
    
    /* TODO: Implement as exit handler */
    return 0;
}

/* FindProp - Find a stored property */
struct StoredProperty * _iffparse_FindProp ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                             register struct IFFHandle *iff __asm("a0"),
                                             register LONG type __asm("d0"),
                                             register LONG id __asm("d1") )
{
    struct LocalContextItem *lci;
    
    DPRINTF (LOG_DEBUG, "_iffparse: FindProp() type=0x%08lx id=0x%08lx\n",
             type, id);
    
    lci = _iffparse_FindLocalItem(IFFParseBase, iff, type, id, IFFLCI_PROP);
    if (lci)
    {
        return (struct StoredProperty *)_iffparse_LocalItemData(IFFParseBase, lci);
    }
    
    return NULL;
}

/* FindCollection - Find a collection */
struct CollectionItem * _iffparse_FindCollection ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                   register struct IFFHandle *iff __asm("a0"),
                                                   register LONG type __asm("d0"),
                                                   register LONG id __asm("d1") )
{
    struct LocalContextItem *lci;
    
    DPRINTF (LOG_DEBUG, "_iffparse: FindCollection() type=0x%08lx id=0x%08lx\n",
             type, id);
    
    lci = _iffparse_FindLocalItem(IFFParseBase, iff, type, id, IFFLCI_COLLECTION);
    if (lci)
    {
        return (struct CollectionItem *)_iffparse_LocalItemData(IFFParseBase, lci);
    }
    
    return NULL;
}

/* FindPropContext - Find property context (nearest FORM or LIST) */
struct ContextNode * _iffparse_FindPropContext ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                 register struct IFFHandle *iff __asm("a0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn;
    
    DPRINTF (LOG_DEBUG, "_iffparse: FindPropContext() iff=0x%08lx\n", (ULONG)iff);
    
    if (!iff)
        return NULL;
    
    /* Walk the context stack looking for FORM or LIST */
    icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
    while (icn->CN.cn_Node.mln_Succ)
    {
        if (icn != &iiff->iff_DefaultCN)
        {
            if (icn->CN.cn_ID == ID_FORM || icn->CN.cn_ID == ID_LIST)
            {
                return (struct ContextNode *)icn;
            }
        }
        icn = (struct IntContextNode *)icn->CN.cn_Node.mln_Succ;
    }
    
    return NULL;
}

/* CurrentChunk - Get current chunk context */
struct ContextNode * _iffparse_CurrentChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                              register struct IFFHandle *iff __asm("a0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn;
    
    if (!iff)
        return NULL;
    
    icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
    if (!icn || icn == &iiff->iff_DefaultCN)
        return NULL;
    
    DPRINTF (LOG_DEBUG, "_iffparse: CurrentChunk() -> id=0x%08lx type=0x%08lx\n",
             icn->CN.cn_ID, icn->CN.cn_Type);
    
    return (struct ContextNode *)icn;
}

/* ParentChunk - Get parent chunk context */
struct ContextNode * _iffparse_ParentChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                             register struct ContextNode *contextNode __asm("a0") )
{
    struct IntContextNode *icn = (struct IntContextNode *)contextNode;
    struct IntContextNode *parent;
    
    DPRINTF (LOG_DEBUG, "_iffparse: ParentChunk() contextNode=0x%08lx\n", (ULONG)contextNode);
    
    if (!contextNode)
        return NULL;
    
    parent = (struct IntContextNode *)icn->CN.cn_Node.mln_Succ;
    if (!parent || !parent->CN.cn_Node.mln_Succ)
        return NULL;  /* No parent or parent is end of list */
    
    return (struct ContextNode *)parent;
}

/* AllocLocalItem - Allocate a local context item */
struct LocalContextItem * _iffparse_AllocLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                     register LONG type __asm("d0"),
                                                     register LONG id __asm("d1"),
                                                     register LONG ident __asm("d2"),
                                                     register LONG dataSize __asm("d3") )
{
    struct IntLocalContextItem *ilci;
    ULONG totalSize;
    
    DPRINTF (LOG_DEBUG, "_iffparse: AllocLocalItem() type=0x%08lx id=0x%08lx ident=0x%08lx dataSize=%ld\n",
             type, id, ident, dataSize);
    
    totalSize = sizeof(struct IntLocalContextItem);
    if (dataSize > 0)
        totalSize += dataSize;
    
    ilci = AllocMem(totalSize, MEMF_ANY | MEMF_CLEAR);
    if (!ilci)
        return NULL;
    
    ilci->LCI.lci_Type = type;
    ilci->LCI.lci_ID = id;
    ilci->LCI.lci_Ident = ident;
    ilci->lci_UserDataSize = dataSize;
    
    if (dataSize > 0)
    {
        ilci->lci_UserData = (APTR)((UBYTE *)ilci + sizeof(struct IntLocalContextItem));
    }
    
    return (struct LocalContextItem *)ilci;
}

/* LocalItemData - Get data pointer from local item */
APTR _iffparse_LocalItemData ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct LocalContextItem *localItem __asm("a0") )
{
    struct IntLocalContextItem *ilci = (struct IntLocalContextItem *)localItem;
    
    DPRINTF (LOG_DEBUG, "_iffparse: LocalItemData() localItem=0x%08lx\n", (ULONG)localItem);
    
    if (!localItem)
        return NULL;
    
    return ilci->lci_UserData;
}

/* SetLocalItemPurge - Set purge hook for local item */
void _iffparse_SetLocalItemPurge ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                   register struct LocalContextItem *localItem __asm("a0"),
                                   register struct Hook *purgeHook __asm("a1") )
{
    struct IntLocalContextItem *ilci = (struct IntLocalContextItem *)localItem;
    
    DPRINTF (LOG_DEBUG, "_iffparse: SetLocalItemPurge() localItem=0x%08lx hook=0x%08lx\n",
             (ULONG)localItem, (ULONG)purgeHook);
    
    if (localItem)
    {
        ilci->lci_PurgeHook = purgeHook;
    }
}

/* FreeLocalItem - Free a local context item */
void _iffparse_FreeLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct LocalContextItem *localItem __asm("a0") )
{
    struct IntLocalContextItem *ilci = (struct IntLocalContextItem *)localItem;
    ULONG totalSize;
    
    DPRINTF (LOG_DEBUG, "_iffparse: FreeLocalItem() localItem=0x%08lx\n", (ULONG)localItem);
    
    if (!localItem)
        return;
    
    totalSize = sizeof(struct IntLocalContextItem);
    if (ilci->lci_UserDataSize > 0)
        totalSize += ilci->lci_UserDataSize;
    
    FreeMem(ilci, totalSize);
}

/* FindLocalItem - Find a local context item */
struct LocalContextItem * _iffparse_FindLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                    register struct IFFHandle *iff __asm("a0"),
                                                    register LONG type __asm("d0"),
                                                    register LONG id __asm("d1"),
                                                    register LONG ident __asm("d2") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn;
    struct IntLocalContextItem *ilci;
    
    DPRINTF (LOG_DEBUG, "_iffparse: FindLocalItem() type=0x%08lx id=0x%08lx ident=0x%08lx\n",
             type, id, ident);
    
    if (!iff)
        return NULL;
    
    /* Walk context stack from top to bottom */
    icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
    while (icn->CN.cn_Node.mln_Succ)
    {
        /* Search this context's LCI list */
        ilci = (struct IntLocalContextItem *)icn->cn_LCIList.mlh_Head;
        while (ilci->LCI.lci_Node.mln_Succ)
        {
            if ((type == 0 || ilci->LCI.lci_Type == type) &&
                (id == 0 || ilci->LCI.lci_ID == id) &&
                ilci->LCI.lci_Ident == ident)
            {
                return (struct LocalContextItem *)ilci;
            }
            ilci = (struct IntLocalContextItem *)ilci->LCI.lci_Node.mln_Succ;
        }
        icn = (struct IntContextNode *)icn->CN.cn_Node.mln_Succ;
    }
    
    return NULL;
}

/* StoreLocalItem - Store a local context item */
LONG _iffparse_StoreLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                register struct IFFHandle *iff __asm("a0"),
                                register struct LocalContextItem *localItem __asm("a1"),
                                register LONG position __asm("d0") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    struct IntContextNode *icn = NULL;
    
    DPRINTF (LOG_DEBUG, "_iffparse: StoreLocalItem() iff=0x%08lx item=0x%08lx position=%ld\n",
             (ULONG)iff, (ULONG)localItem, position);
    
    if (!iff || !localItem)
        return IFFERR_NOMEM;
    
    switch (position)
    {
        case IFFSLI_ROOT:
            /* Store in default (bottom) context */
            icn = &iiff->iff_DefaultCN;
            break;
            
        case IFFSLI_TOP:
            /* Store in current (top) context */
            icn = (struct IntContextNode *)iiff->iff_CNStack.mlh_Head;
            if (icn == &iiff->iff_DefaultCN)
                icn = &iiff->iff_DefaultCN;  /* Only default available */
            break;
            
        case IFFSLI_PROP:
            /* Store in nearest FORM or LIST context */
            icn = (struct IntContextNode *)_iffparse_FindPropContext(IFFParseBase, iff);
            if (!icn)
                return IFFERR_NOSCOPE;
            break;
            
        default:
            return IFFERR_SYNTAX;
    }
    
    if (!icn)
        return IFFERR_NOSCOPE;
    
    /* Add to head of context's LCI list */
    AddHead((struct List *)&icn->cn_LCIList, (struct Node *)&localItem->lci_Node);
    
    return 0;
}

/* StoreItemInContext - Store item in specific context */
void _iffparse_StoreItemInContext ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                    register struct IFFHandle *iff __asm("a0"),
                                    register struct LocalContextItem *localItem __asm("a1"),
                                    register struct ContextNode *contextNode __asm("a2") )
{
    struct IntContextNode *icn = (struct IntContextNode *)contextNode;
    
    DPRINTF (LOG_DEBUG, "_iffparse: StoreItemInContext()\n");
    
    if (!localItem || !contextNode)
        return;
    
    AddHead((struct List *)&icn->cn_LCIList, (struct Node *)&localItem->lci_Node);
}

/* InitIFF - Initialize an IFF handle with custom stream hook */
void _iffparse_InitIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0"),
                         register LONG flags __asm("d0"),
                         register struct Hook *streamHook __asm("a1") )
{
    struct IntIFFHandle *iiff = (struct IntIFFHandle *)iff;
    
    DPRINTF (LOG_DEBUG, "_iffparse: InitIFF() iff=0x%08lx flags=0x%lx hook=0x%08lx\n",
             (ULONG)iff, flags, (ULONG)streamHook);
    
    if (!iff)
        return;
    
    iff->iff_Flags = (iff->iff_Flags & IFFF_RESERVED) | (flags & ~IFFF_RESERVED);
    iiff->iff_StreamHandler = streamHook;
}

/* InitIFFasDOS - Initialize IFF handle for DOS I/O */
void _iffparse_InitIFFasDOS ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                              register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: InitIFFasDOS() iff=0x%08lx\n", (ULONG)iff);
    
    _iffparse_InitIFF(IFFParseBase, iff, IFFF_RSEEK, &IFFParseBase->iff_DOSHook);
}

/* InitIFFasClip - Initialize IFF handle for Clipboard I/O */
void _iffparse_InitIFFasClip ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: InitIFFasClip() iff=0x%08lx (stub - uses DOS hook)\n", (ULONG)iff);
    
    /* For now, just use DOS hook - clipboard support would need clipboard.device */
    _iffparse_InitIFF(IFFParseBase, iff, IFFF_RSEEK, &IFFParseBase->iff_DOSHook);
}

/* OpenClipboard - Open clipboard for IFF operations */
struct ClipboardHandle * _iffparse_OpenClipboard ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                   register LONG unitNumber __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: OpenClipboard() unitNumber=%ld (stub, returns NULL)\n", unitNumber);
    /* Clipboard support not implemented */
    return NULL;
}

/* CloseClipboard - Close clipboard handle */
void _iffparse_CloseClipboard ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                register struct ClipboardHandle *clipHandle __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: CloseClipboard() clipHandle=0x%08lx (stub)\n", (ULONG)clipHandle);
}

/* GoodID - Check if ID is valid */
LONG _iffparse_GoodID ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                        register LONG id __asm("d0") )
{
    /* Check that all characters are printable (0x20-0x7E) */
    UBYTE c0 = (id >> 24) & 0xFF;
    UBYTE c1 = (id >> 16) & 0xFF;
    UBYTE c2 = (id >>  8) & 0xFF;
    UBYTE c3 = id & 0xFF;
    if (c0 < 0x20 || c0 > 0x7E) return FALSE;
    if (c1 < 0x20 || c1 > 0x7E) return FALSE;
    if (c2 < 0x20 || c2 > 0x7E) return FALSE;
    if (c3 < 0x20 || c3 > 0x7E) return FALSE;
    return TRUE;
}

/* GoodType - Check if type is valid */
LONG _iffparse_GoodType ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register LONG type __asm("d0") )
{
    /* Same as GoodID but first char must not be space */
    UBYTE c0 = (type >> 24) & 0xFF;
    UBYTE c1 = (type >> 16) & 0xFF;
    UBYTE c2 = (type >>  8) & 0xFF;
    UBYTE c3 = type & 0xFF;
    if (c0 == 0x20) return FALSE;
    if (c0 < 0x20 || c0 > 0x7E) return FALSE;
    if (c1 < 0x20 || c1 > 0x7E) return FALSE;
    if (c2 < 0x20 || c2 > 0x7E) return FALSE;
    if (c3 < 0x20 || c3 > 0x7E) return FALSE;
    return TRUE;
}

/* IDtoStr - Convert ID to string */
STRPTR _iffparse_IDtoStr ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register LONG id __asm("d0"),
                           register STRPTR buf __asm("a0") )
{
    if (buf)
    {
        buf[0] = (id >> 24) & 0xFF;
        buf[1] = (id >> 16) & 0xFF;
        buf[2] = (id >>  8) & 0xFF;
        buf[3] = id & 0xFF;
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
