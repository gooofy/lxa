/*
 * lxa iffparse.library implementation
 *
 * Provides the IFF parsing API.
 * This is a stub implementation - IFF parsing operations will fail gracefully
 * but apps should handle this.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>

#include <libraries/iffparse.h>
#include <utility/hooks.h>

#include "util.h"

#define VERSION    39
#define REVISION   1
#define EXLIBNAME  "iffparse"
#define EXLIBVER   " 39.1 (2025/06/23)"

char __aligned _g_iffparse_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_iffparse_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_iffparse_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_iffparse_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* IFFParseBase structure */
struct IFFParseBase
{
    struct Library lib;
    BPTR           SegList;
};

/*
 * Library init/open/close/expunge
 */

struct IFFParseBase * __g_lxa_iffparse_InitLib ( register struct IFFParseBase *iffbase __asm("a6"),
                                                  register BPTR                seglist __asm("a0"),
                                                  register struct ExecBase    *sysb __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_iffparse: InitLib() called\n");
    iffbase->SegList = seglist;
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
 * IFFParse Functions (V36+)
 */

/* AllocIFF - Allocate an IFF handle */
struct IFFHandle * _iffparse_AllocIFF ( register struct IFFParseBase *IFFParseBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: AllocIFF() (stub, returns NULL)\n");
    /* Return NULL - we don't support IFF parsing */
    return NULL;
}

/* OpenIFF - Open an IFF handle for reading/writing */
LONG _iffparse_OpenIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0"),
                         register LONG rwMode __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: OpenIFF() iff=0x%08lx rwMode=%ld (stub, returns IFFERR_NOMEM)\n",
             (ULONG)iff, rwMode);
    return IFFERR_NOMEM;
}

/* ParseIFF - Parse an IFF file */
LONG _iffparse_ParseIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0"),
                          register LONG control __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: ParseIFF() iff=0x%08lx control=%ld (stub, returns IFFERR_EOF)\n",
             (ULONG)iff, control);
    return IFFERR_EOF;
}

/* CloseIFF - Close an IFF handle */
void _iffparse_CloseIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: CloseIFF() iff=0x%08lx (stub)\n", (ULONG)iff);
}

/* FreeIFF - Free an IFF handle */
void _iffparse_FreeIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: FreeIFF() iff=0x%08lx (stub)\n", (ULONG)iff);
}

/* ReadChunkBytes - Read bytes from current chunk */
LONG _iffparse_ReadChunkBytes ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                register struct IFFHandle *iff __asm("a0"),
                                register APTR buf __asm("a1"),
                                register LONG numBytes __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: ReadChunkBytes() iff=0x%08lx numBytes=%ld (stub, returns -1)\n",
             (ULONG)iff, numBytes);
    return -1;
}

/* WriteChunkBytes - Write bytes to current chunk */
LONG _iffparse_WriteChunkBytes ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                 register struct IFFHandle *iff __asm("a0"),
                                 register APTR buf __asm("a1"),
                                 register LONG numBytes __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: WriteChunkBytes() iff=0x%08lx numBytes=%ld (stub, returns -1)\n",
             (ULONG)iff, numBytes);
    return -1;
}

/* ReadChunkRecords - Read records from current chunk */
LONG _iffparse_ReadChunkRecords ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                  register struct IFFHandle *iff __asm("a0"),
                                  register APTR buf __asm("a1"),
                                  register LONG bytesPerRecord __asm("d0"),
                                  register LONG numRecords __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: ReadChunkRecords() iff=0x%08lx (stub, returns -1)\n", (ULONG)iff);
    return -1;
}

/* WriteChunkRecords - Write records to current chunk */
LONG _iffparse_WriteChunkRecords ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                   register struct IFFHandle *iff __asm("a0"),
                                   register APTR buf __asm("a1"),
                                   register LONG bytesPerRecord __asm("d0"),
                                   register LONG numRecords __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: WriteChunkRecords() iff=0x%08lx (stub, returns -1)\n", (ULONG)iff);
    return -1;
}

/* PushChunk - Push a new chunk onto the context stack */
LONG _iffparse_PushChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1"),
                           register LONG size __asm("d2") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: PushChunk() iff=0x%08lx type=0x%08lx id=0x%08lx (stub, returns IFFERR_NOMEM)\n",
             (ULONG)iff, type, id);
    return IFFERR_NOMEM;
}

/* PopChunk - Pop the current chunk from the context stack */
LONG _iffparse_PopChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                          register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: PopChunk() iff=0x%08lx (stub, returns IFFERR_EOF)\n", (ULONG)iff);
    return IFFERR_EOF;
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
    DPRINTF (LOG_DEBUG, "_iffparse: EntryHandler() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
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
    DPRINTF (LOG_DEBUG, "_iffparse: ExitHandler() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* PropChunk - Declare a property chunk */
LONG _iffparse_PropChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: PropChunk() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* PropChunks - Declare multiple property chunks */
LONG _iffparse_PropChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG *propArray __asm("a1"),
                            register LONG numPairs __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: PropChunks() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* StopChunk - Declare a stop chunk */
LONG _iffparse_StopChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                           register struct IFFHandle *iff __asm("a0"),
                           register LONG type __asm("d0"),
                           register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: StopChunk() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* StopChunks - Declare multiple stop chunks */
LONG _iffparse_StopChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG *propArray __asm("a1"),
                            register LONG numPairs __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: StopChunks() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* CollectionChunk - Declare a collection chunk */
LONG _iffparse_CollectionChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                 register struct IFFHandle *iff __asm("a0"),
                                 register LONG type __asm("d0"),
                                 register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: CollectionChunk() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* CollectionChunks - Declare multiple collection chunks */
LONG _iffparse_CollectionChunks ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                  register struct IFFHandle *iff __asm("a0"),
                                  register LONG *propArray __asm("a1"),
                                  register LONG numPairs __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: CollectionChunks() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* StopOnExit - Declare a stop-on-exit chunk */
LONG _iffparse_StopOnExit ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                            register struct IFFHandle *iff __asm("a0"),
                            register LONG type __asm("d0"),
                            register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: StopOnExit() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* FindProp - Find a stored property */
struct StoredProperty * _iffparse_FindProp ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                             register struct IFFHandle *iff __asm("a0"),
                                             register LONG type __asm("d0"),
                                             register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: FindProp() iff=0x%08lx (stub, returns NULL)\n", (ULONG)iff);
    return NULL;
}

/* FindCollection - Find a collection */
struct CollectionItem * _iffparse_FindCollection ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                   register struct IFFHandle *iff __asm("a0"),
                                                   register LONG type __asm("d0"),
                                                   register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: FindCollection() iff=0x%08lx (stub, returns NULL)\n", (ULONG)iff);
    return NULL;
}

/* FindPropContext - Find property context */
struct ContextNode * _iffparse_FindPropContext ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                 register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: FindPropContext() iff=0x%08lx (stub, returns NULL)\n", (ULONG)iff);
    return NULL;
}

/* CurrentChunk - Get current chunk context */
struct ContextNode * _iffparse_CurrentChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                              register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: CurrentChunk() iff=0x%08lx (stub, returns NULL)\n", (ULONG)iff);
    return NULL;
}

/* ParentChunk - Get parent chunk context */
struct ContextNode * _iffparse_ParentChunk ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                             register struct ContextNode *contextNode __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: ParentChunk() contextNode=0x%08lx (stub, returns NULL)\n", (ULONG)contextNode);
    return NULL;
}

/* AllocLocalItem - Allocate a local context item */
struct LocalContextItem * _iffparse_AllocLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                     register LONG type __asm("d0"),
                                                     register LONG id __asm("d1"),
                                                     register LONG ident __asm("d2"),
                                                     register LONG dataSize __asm("d3") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: AllocLocalItem() (stub, returns NULL)\n");
    return NULL;
}

/* LocalItemData - Get data pointer from local item */
APTR _iffparse_LocalItemData ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct LocalContextItem *localItem __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: LocalItemData() localItem=0x%08lx (stub, returns NULL)\n", (ULONG)localItem);
    return NULL;
}

/* SetLocalItemPurge - Set purge hook for local item */
void _iffparse_SetLocalItemPurge ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                   register struct LocalContextItem *localItem __asm("a0"),
                                   register struct Hook *purgeHook __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: SetLocalItemPurge() (stub)\n");
}

/* FreeLocalItem - Free a local context item */
void _iffparse_FreeLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct LocalContextItem *localItem __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: FreeLocalItem() localItem=0x%08lx (stub)\n", (ULONG)localItem);
}

/* FindLocalItem - Find a local context item */
struct LocalContextItem * _iffparse_FindLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                    register struct IFFHandle *iff __asm("a0"),
                                                    register LONG type __asm("d0"),
                                                    register LONG id __asm("d1"),
                                                    register LONG ident __asm("d2") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: FindLocalItem() iff=0x%08lx (stub, returns NULL)\n", (ULONG)iff);
    return NULL;
}

/* StoreLocalItem - Store a local context item */
LONG _iffparse_StoreLocalItem ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                register struct IFFHandle *iff __asm("a0"),
                                register struct LocalContextItem *localItem __asm("a1"),
                                register LONG position __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: StoreLocalItem() iff=0x%08lx (stub, returns IFFERR_NOMEM)\n", (ULONG)iff);
    return IFFERR_NOMEM;
}

/* StoreItemInContext - Store item in specific context */
void _iffparse_StoreItemInContext ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                    register struct IFFHandle *iff __asm("a0"),
                                    register struct LocalContextItem *localItem __asm("a1"),
                                    register struct ContextNode *contextNode __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: StoreItemInContext() (stub)\n");
}

/* InitIFF - Initialize an IFF handle with custom stream hook */
void _iffparse_InitIFF ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                         register struct IFFHandle *iff __asm("a0"),
                         register LONG flags __asm("d0"),
                         register struct Hook *streamHook __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: InitIFF() iff=0x%08lx (stub)\n", (ULONG)iff);
}

/* InitIFFasDOS - Initialize IFF handle for DOS I/O */
void _iffparse_InitIFFasDOS ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                              register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: InitIFFasDOS() iff=0x%08lx (stub)\n", (ULONG)iff);
}

/* InitIFFasClip - Initialize IFF handle for Clipboard I/O */
void _iffparse_InitIFFasClip ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                               register struct IFFHandle *iff __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: InitIFFasClip() iff=0x%08lx (stub)\n", (ULONG)iff);
}

/* OpenClipboard - Open clipboard for IFF operations */
struct ClipboardHandle * _iffparse_OpenClipboard ( register struct IFFParseBase *IFFParseBase __asm("a6"),
                                                   register LONG unitNumber __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_iffparse: OpenClipboard() unitNumber=%ld (stub, returns NULL)\n", unitNumber);
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
    DPRINTF (LOG_DEBUG, "_iffparse: GoodID() id=0x%08lx\n", id);
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
    DPRINTF (LOG_DEBUG, "_iffparse: GoodType() type=0x%08lx\n", type);
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
    DPRINTF (LOG_DEBUG, "_iffparse: IDtoStr() id=0x%08lx\n", id);
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

/* Function table - from iffparse_lib.fd
 * Standard library functions at -6 through -24
 * First real function at -30
 *
 * Bias 30:  AllocIFF           ()
 * Bias 36:  OpenIFF            (a0,d0)
 * Bias 42:  ParseIFF           (a0,d0)
 * Bias 48:  CloseIFF           (a0)
 * Bias 54:  FreeIFF            (a0)
 * Bias 60:  ReadChunkBytes     (a0/a1,d0)
 * Bias 66:  WriteChunkBytes    (a0/a1,d0)
 * Bias 72:  ReadChunkRecords   (a0/a1,d0/d1)
 * Bias 78:  WriteChunkRecords  (a0/a1,d0/d1)
 * Bias 84:  PushChunk          (a0,d0/d1/d2)
 * Bias 90:  PopChunk           (a0)
 * Bias 96:  Reserved
 * Bias 102: EntryHandler       (a0,d0/d1/d2/a1/a2)
 * Bias 108: ExitHandler        (a0,d0/d1/d2/a1/a2)
 * Bias 114: PropChunk          (a0,d0/d1)
 * Bias 120: PropChunks         (a0/a1,d0)
 * Bias 126: StopChunk          (a0,d0/d1)
 * Bias 132: StopChunks         (a0/a1,d0)
 * Bias 138: CollectionChunk    (a0,d0/d1)
 * Bias 144: CollectionChunks   (a0/a1,d0)
 * Bias 150: StopOnExit         (a0,d0/d1)
 * Bias 156: FindProp           (a0,d0/d1)
 * Bias 162: FindCollection     (a0,d0/d1)
 * Bias 168: FindPropContext    (a0)
 * Bias 174: CurrentChunk       (a0)
 * Bias 180: ParentChunk        (a0)
 * Bias 186: AllocLocalItem     (d0/d1/d2/d3)
 * Bias 192: LocalItemData      (a0)
 * Bias 198: SetLocalItemPurge  (a0/a1)
 * Bias 204: FreeLocalItem      (a0)
 * Bias 210: FindLocalItem      (a0,d0/d1/d2)
 * Bias 216: StoreLocalItem     (a0/a1,d0)
 * Bias 222: StoreItemInContext (a0/a1/a2)
 * Bias 228: InitIFF            (a0,d0/a1)
 * Bias 234: InitIFFasDOS       (a0)
 * Bias 240: InitIFFasClip      (a0)
 * Bias 246: OpenClipboard      (d0)
 * Bias 252: CloseClipboard     (a0)
 * Bias 258: GoodID             (d0)
 * Bias 264: GoodType           (d0)
 * Bias 270: IDtoStr            (d0/a0)
 */
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
