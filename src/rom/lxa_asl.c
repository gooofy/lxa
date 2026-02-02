/*
 * lxa asl.library implementation
 *
 * Provides the ASL (AmigaOS Standard Library) requester API.
 * This is a stub implementation - requesters will fail but apps
 * should handle this gracefully.
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

#include <utility/tagitem.h>

#include "util.h"

#define VERSION    39
#define REVISION   1
#define EXLIBNAME  "asl"
#define EXLIBVER   " 39.1 (2025/02/02)"

char __aligned _g_asl_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_asl_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_asl_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_asl_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* AslBase structure */
struct AslBase {
    struct Library lib;
    BPTR           SegList;
};

/* Requester types */
#define ASL_FileRequest   0
#define ASL_FontRequest   1
#define ASL_ScreenModeRequest 2

/* Requester structure - minimal for stub */
struct AslRequester {
    ULONG  ar_Type;
    ULONG  ar_Size;
    /* File requester fields */
    STRPTR rf_File;
    STRPTR rf_Dir;
    STRPTR rf_Pattern;
    BPTR   rf_Lock;
};

/*
 * Library init/open/close/expunge
 */

struct AslBase * __g_lxa_asl_InitLib ( register struct AslBase *aslb __asm("a6"),
                                       register BPTR           seglist __asm("a0"),
                                       register struct ExecBase *sysb __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_asl: InitLib() called\n");
    aslb->SegList = seglist;
    return aslb;
}

BPTR __g_lxa_asl_ExpungeLib ( register struct AslBase *aslb __asm("a6") )
{
    return 0;
}

struct AslBase * __g_lxa_asl_OpenLib ( register struct AslBase *aslb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_asl: OpenLib() called, aslb=0x%08lx\n", (ULONG)aslb);
    aslb->lib.lib_OpenCnt++;
    aslb->lib.lib_Flags &= ~LIBF_DELEXP;
    return aslb;
}

BPTR __g_lxa_asl_CloseLib ( register struct AslBase *aslb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_asl: CloseLib() called, aslb=0x%08lx\n", (ULONG)aslb);
    aslb->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_asl_ExtFuncLib ( void )
{
    return 0;
}

/*
 * ASL Functions (V36+)
 */

/* AllocFileRequest - Obsolete, use AllocAslRequest instead */
APTR _asl_AllocFileRequest ( register struct AslBase *AslBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_asl: AllocFileRequest() (obsolete) -> AllocAslRequest(ASL_FileRequest)\n");
    /* Allocate a minimal file requester structure */
    struct AslRequester *req = AllocMem(sizeof(struct AslRequester), MEMF_CLEAR | MEMF_PUBLIC);
    if (req) {
        req->ar_Type = ASL_FileRequest;
        req->ar_Size = sizeof(struct AslRequester);
    }
    return req;
}

/* FreeFileRequest - Obsolete, use FreeAslRequest instead */
void _asl_FreeFileRequest ( register struct AslBase *AslBase __asm("a6"),
                            register APTR fileReq __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_asl: FreeFileRequest() fileReq=0x%08lx\n", (ULONG)fileReq);
    if (fileReq) {
        FreeMem(fileReq, sizeof(struct AslRequester));
    }
}

/* RequestFile - Obsolete, use AslRequest instead */
BOOL _asl_RequestFile ( register struct AslBase *AslBase __asm("a6"),
                        register APTR fileReq __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_asl: RequestFile() fileReq=0x%08lx (stub, returns FALSE)\n", (ULONG)fileReq);
    /* Return FALSE - we don't have a GUI for file requesters */
    return FALSE;
}

/* AllocAslRequest - Allocate an ASL requester */
APTR _asl_AllocAslRequest ( register struct AslBase *AslBase __asm("a6"),
                            register ULONG reqType __asm("d0"),
                            register struct TagItem *tagList __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_asl: AllocAslRequest() reqType=%ld\n", reqType);
    
    struct AslRequester *req = AllocMem(sizeof(struct AslRequester), MEMF_CLEAR | MEMF_PUBLIC);
    if (req) {
        req->ar_Type = reqType;
        req->ar_Size = sizeof(struct AslRequester);
        /* Process tags if provided */
        /* For stub, we just ignore tags */
    }
    return req;
}

/* FreeAslRequest - Free an ASL requester */
void _asl_FreeAslRequest ( register struct AslBase *AslBase __asm("a6"),
                           register APTR requester __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_asl: FreeAslRequest() requester=0x%08lx\n", (ULONG)requester);
    if (requester) {
        FreeMem(requester, sizeof(struct AslRequester));
    }
}

/* AslRequest - Display a requester */
BOOL _asl_AslRequest ( register struct AslBase *AslBase __asm("a6"),
                       register APTR requester __asm("a0"),
                       register struct TagItem *tagList __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_asl: AslRequest() requester=0x%08lx (stub, returns FALSE)\n", (ULONG)requester);
    /* Return FALSE - we don't have a GUI for requesters */
    return FALSE;
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

extern APTR              __g_lxa_asl_FuncTab [];
extern struct MyDataInit __g_lxa_asl_DataTab;
extern struct InitTable  __g_lxa_asl_InitTab;
extern APTR              __g_lxa_asl_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                  // UWORD rt_MatchWord
    &ROMTag,                        // struct Resident *rt_MatchTag
    &__g_lxa_asl_EndResident,       // APTR  rt_EndSkip
    RTF_AUTOINIT,                   // UBYTE rt_Flags
    VERSION,                        // UBYTE rt_Version
    NT_LIBRARY,                     // UBYTE rt_Type
    0,                              // BYTE  rt_Pri
    &_g_asl_ExLibName[0],           // char  *rt_Name
    &_g_asl_ExLibID[0],             // char  *rt_IdString
    &__g_lxa_asl_InitTab            // APTR  rt_Init
};

APTR __g_lxa_asl_EndResident;
struct Resident *__lxa_asl_ROMTag = &ROMTag;

struct InitTable __g_lxa_asl_InitTab =
{
    (ULONG)               sizeof(struct AslBase),
    (APTR              *) &__g_lxa_asl_FuncTab[0],
    (APTR)                &__g_lxa_asl_DataTab,
    (APTR)                __g_lxa_asl_InitLib
};

/* Function table - from asl_lib.fd
 * Standard library functions at -6 through -24
 * First real function at -30
 */
APTR __g_lxa_asl_FuncTab [] =
{
    __g_lxa_asl_OpenLib,           // -6   Standard
    __g_lxa_asl_CloseLib,          // -12  Standard
    __g_lxa_asl_ExpungeLib,        // -18  Standard
    __g_lxa_asl_ExtFuncLib,        // -24  Standard (reserved)
    _asl_AllocFileRequest,         // -30  AllocFileRequest (obsolete)
    _asl_FreeFileRequest,          // -36  FreeFileRequest (obsolete)
    _asl_RequestFile,              // -42  RequestFile (obsolete)
    _asl_AllocAslRequest,          // -48  AllocAslRequest
    _asl_FreeAslRequest,           // -54  FreeAslRequest
    _asl_AslRequest,               // -60  AslRequest
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_asl_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_asl_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_asl_ExLibID[0],
    (ULONG) 0
};
