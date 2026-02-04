/*
 * lxa commodities.library implementation
 *
 * Provides the Commodities Exchange API.
 * This is a stub implementation - commodities operations will fail gracefully
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

#include <utility/tagitem.h>

#include <libraries/commodities.h>

#include "util.h"

#define VERSION    39
#define REVISION   1
#define EXLIBNAME  "commodities"
#define EXLIBVER   " 39.1 (2025/06/23)"

char __aligned _g_commodities_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_commodities_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_commodities_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_commodities_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* CommoditiesBase structure */
struct CommoditiesBase
{
    struct Library lib;
    BPTR           SegList;
};

/*
 * Library init/open/close/expunge
 */

struct CommoditiesBase * __g_lxa_commodities_InitLib ( register struct CommoditiesBase *cxb __asm("d0"),
                                                       register BPTR                   seglist __asm("a0"),
                                                       register struct ExecBase        *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_commodities: InitLib() called\n");
    cxb->SegList = seglist;
    return cxb;
}

BPTR __g_lxa_commodities_ExpungeLib ( register struct CommoditiesBase *cxb __asm("a6") )
{
    return 0;
}

struct CommoditiesBase * __g_lxa_commodities_OpenLib ( register struct CommoditiesBase *cxb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_commodities: OpenLib() called, cxb=0x%08lx\n", (ULONG)cxb);
    cxb->lib.lib_OpenCnt++;
    cxb->lib.lib_Flags &= ~LIBF_DELEXP;
    return cxb;
}

BPTR __g_lxa_commodities_CloseLib ( register struct CommoditiesBase *cxb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CloseLib() called, cxb=0x%08lx\n", (ULONG)cxb);
    cxb->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_commodities_ExtFuncLib ( void )
{
    return 0;
}

/*
 * Commodities Functions (V36+)
 */

/* CreateCxObj - Create a commodities object */
CxObj * _commodities_CreateCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                   register ULONG type __asm("d0"),
                                   register LONG  arg1 __asm("a0"),
                                   register LONG  arg2 __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CreateCxObj() type=%ld arg1=%ld arg2=%ld (stub, returns NULL)\n",
             type, arg1, arg2);
    /* Return NULL - we don't support commodities objects */
    return NULL;
}

/* CxBroker - Create a broker object */
CxObj * _commodities_CxBroker ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register struct NewBroker *nb __asm("a0"),
                                register LONG *error __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CxBroker() nb=0x%08lx (stub, returns NULL)\n", (ULONG)nb);
    if (nb)
    {
        DPRINTF (LOG_DEBUG, "_commodities:   nb_Name=%s nb_Title=%s\n",
                 STRORNULL(nb->nb_Name), STRORNULL(nb->nb_Title));
    }
    /* Set error to CBERR_SYSERR and return NULL */
    if (error)
    {
        *error = CBERR_SYSERR;
    }
    return NULL;
}

/* ActivateCxObj - Activate or deactivate a commodities object */
LONG _commodities_ActivateCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                  register CxObj *co __asm("a0"),
                                  register LONG flag __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: ActivateCxObj() co=0x%08lx flag=%ld (stub)\n", (ULONG)co, flag);
    return 0;
}

/* DeleteCxObj - Delete a commodities object */
void _commodities_DeleteCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *co __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: DeleteCxObj() co=0x%08lx (stub)\n", (ULONG)co);
    /* Nothing to do - we never allocate anything */
}

/* DeleteCxObjAll - Delete a commodities object and all attached objects */
void _commodities_DeleteCxObjAll ( register struct CommoditiesBase *CxBase __asm("a6"),
                                   register CxObj *co __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: DeleteCxObjAll() co=0x%08lx (stub)\n", (ULONG)co);
    /* Nothing to do - we never allocate anything */
}

/* CxObjType - Get the type of a commodities object */
ULONG _commodities_CxObjType ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register CxObj *co __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CxObjType() co=0x%08lx (stub, returns CX_INVALID)\n", (ULONG)co);
    return CX_INVALID;
}

/* CxObjError - Get error state of a commodities object */
LONG _commodities_CxObjError ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register CxObj *co __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CxObjError() co=0x%08lx (stub, returns COERR_ISNULL)\n", (ULONG)co);
    return COERR_ISNULL;
}

/* ClearCxObjError - Clear error state of a commodities object */
void _commodities_ClearCxObjError ( register struct CommoditiesBase *CxBase __asm("a6"),
                                    register CxObj *co __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: ClearCxObjError() co=0x%08lx (stub)\n", (ULONG)co);
}

/* SetCxObjPri - Set priority of a commodities object */
LONG _commodities_SetCxObjPri ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *co __asm("a0"),
                                register LONG pri __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: SetCxObjPri() co=0x%08lx pri=%ld (stub)\n", (ULONG)co, pri);
    return 0;
}

/* AttachCxObj - Attach a commodities object to another */
void _commodities_AttachCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *headObj __asm("a0"),
                                register CxObj *co __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: AttachCxObj() headObj=0x%08lx co=0x%08lx (stub)\n",
             (ULONG)headObj, (ULONG)co);
}

/* EnqueueCxObj - Enqueue a commodities object */
void _commodities_EnqueueCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                 register CxObj *headObj __asm("a0"),
                                 register CxObj *co __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: EnqueueCxObj() headObj=0x%08lx co=0x%08lx (stub)\n",
             (ULONG)headObj, (ULONG)co);
}

/* InsertCxObj - Insert a commodities object */
void _commodities_InsertCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *headObj __asm("a0"),
                                register CxObj *co __asm("a1"),
                                register CxObj *pred __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_commodities: InsertCxObj() headObj=0x%08lx co=0x%08lx pred=0x%08lx (stub)\n",
             (ULONG)headObj, (ULONG)co, (ULONG)pred);
}

/* RemoveCxObj - Remove a commodities object */
void _commodities_RemoveCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *co __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: RemoveCxObj() co=0x%08lx (stub)\n", (ULONG)co);
}

/* Private function slot */
ULONG _commodities_Private1 ( register struct CommoditiesBase *CxBase __asm("a6") )
{
    return 0;
}

/* SetTranslate - Set translation for a translator object */
void _commodities_SetTranslate ( register struct CommoditiesBase *CxBase __asm("a6"),
                                 register CxObj *translator __asm("a0"),
                                 register struct InputEvent *events __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: SetTranslate() translator=0x%08lx events=0x%08lx (stub)\n",
             (ULONG)translator, (ULONG)events);
}

/* SetFilter - Set filter text for a filter object */
void _commodities_SetFilter ( register struct CommoditiesBase *CxBase __asm("a6"),
                              register CxObj *filter __asm("a0"),
                              register STRPTR text __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: SetFilter() filter=0x%08lx text=%s (stub)\n",
             (ULONG)filter, STRORNULL(text));
}

/* SetFilterIX - Set filter IX for a filter object */
void _commodities_SetFilterIX ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *filter __asm("a0"),
                                register IX *ix __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: SetFilterIX() filter=0x%08lx ix=0x%08lx (stub)\n",
             (ULONG)filter, (ULONG)ix);
}

/* ParseIX - Parse an input expression description */
LONG _commodities_ParseIX ( register struct CommoditiesBase *CxBase __asm("a6"),
                            register STRPTR description __asm("a0"),
                            register IX *ix __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: ParseIX() description=%s ix=0x%08lx (stub, returns -1)\n",
             STRORNULL(description), (ULONG)ix);
    /* Return -1 to indicate failure */
    return -1;
}

/* CxMsgType - Get type of a commodities message */
ULONG _commodities_CxMsgType ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register CxMsg *cxm __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CxMsgType() cxm=0x%08lx (stub, returns 0)\n", (ULONG)cxm);
    return 0;
}

/* CxMsgData - Get data pointer from a commodities message */
APTR _commodities_CxMsgData ( register struct CommoditiesBase *CxBase __asm("a6"),
                              register CxMsg *cxm __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CxMsgData() cxm=0x%08lx (stub, returns NULL)\n", (ULONG)cxm);
    return NULL;
}

/* CxMsgID - Get ID from a commodities message */
LONG _commodities_CxMsgID ( register struct CommoditiesBase *CxBase __asm("a6"),
                            register CxMsg *cxm __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CxMsgID() cxm=0x%08lx (stub, returns 0)\n", (ULONG)cxm);
    return 0;
}

/* DivertCxMsg - Divert a commodities message */
void _commodities_DivertCxMsg ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxMsg *cxm __asm("a0"),
                                register CxObj *headObj __asm("a1"),
                                register CxObj *returnObj __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_commodities: DivertCxMsg() cxm=0x%08lx headObj=0x%08lx returnObj=0x%08lx (stub)\n",
             (ULONG)cxm, (ULONG)headObj, (ULONG)returnObj);
}

/* RouteCxMsg - Route a commodities message */
void _commodities_RouteCxMsg ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register CxMsg *cxm __asm("a0"),
                               register CxObj *co __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: RouteCxMsg() cxm=0x%08lx co=0x%08lx (stub)\n",
             (ULONG)cxm, (ULONG)co);
}

/* DisposeCxMsg - Dispose of a commodities message */
void _commodities_DisposeCxMsg ( register struct CommoditiesBase *CxBase __asm("a6"),
                                 register CxMsg *cxm __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: DisposeCxMsg() cxm=0x%08lx (stub)\n", (ULONG)cxm);
}

/* InvertKeyMap - Invert a keymap translation */
BOOL _commodities_InvertKeyMap ( register struct CommoditiesBase *CxBase __asm("a6"),
                                 register ULONG ansiCode __asm("d0"),
                                 register struct InputEvent *event __asm("a0"),
                                 register struct KeyMap *km __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: InvertKeyMap() ansiCode=0x%lx event=0x%08lx km=0x%08lx (stub, returns FALSE)\n",
             ansiCode, (ULONG)event, (ULONG)km);
    return FALSE;
}

/* AddIEvents - Add input events to the input stream */
void _commodities_AddIEvents ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register struct InputEvent *events __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_commodities: AddIEvents() events=0x%08lx (stub)\n", (ULONG)events);
}

/* Private function slots */
ULONG _commodities_Private2 ( register struct CommoditiesBase *CxBase __asm("a6") )
{
    return 0;
}

ULONG _commodities_Private3 ( register struct CommoditiesBase *CxBase __asm("a6") )
{
    return 0;
}

ULONG _commodities_Private4 ( register struct CommoditiesBase *CxBase __asm("a6") )
{
    return 0;
}

/* MatchIX - Match an input event against an IX (V38+) */
BOOL _commodities_MatchIX ( register struct CommoditiesBase *CxBase __asm("a6"),
                            register struct InputEvent *event __asm("a0"),
                            register IX *ix __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_commodities: MatchIX() event=0x%08lx ix=0x%08lx (stub, returns FALSE)\n",
             (ULONG)event, (ULONG)ix);
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

extern APTR              __g_lxa_commodities_FuncTab [];
extern struct MyDataInit __g_lxa_commodities_DataTab;
extern struct InitTable  __g_lxa_commodities_InitTab;
extern APTR              __g_lxa_commodities_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                         // UWORD rt_MatchWord
    &ROMTag,                               // struct Resident *rt_MatchTag
    &__g_lxa_commodities_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                          // UBYTE rt_Flags
    VERSION,                               // UBYTE rt_Version
    NT_LIBRARY,                            // UBYTE rt_Type
    0,                                     // BYTE  rt_Pri
    &_g_commodities_ExLibName[0],          // char  *rt_Name
    &_g_commodities_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_commodities_InitTab           // APTR  rt_Init
};

APTR __g_lxa_commodities_EndResident;
struct Resident *__lxa_commodities_ROMTag = &ROMTag;

struct InitTable __g_lxa_commodities_InitTab =
{
    (ULONG)               sizeof(struct CommoditiesBase),
    (APTR              *) &__g_lxa_commodities_FuncTab[0],
    (APTR)                &__g_lxa_commodities_DataTab,
    (APTR)                __g_lxa_commodities_InitLib
};

/* Function table - from commodities_lib.fd
 * Standard library functions at -6 through -24
 * First real function at -30
 *
 * Bias 30:  CreateCxObj      (d0/a0/a1)
 * Bias 36:  CxBroker         (a0,d0)
 * Bias 42:  ActivateCxObj    (a0,d0)
 * Bias 48:  DeleteCxObj      (a0)
 * Bias 54:  DeleteCxObjAll   (a0)
 * Bias 60:  CxObjType        (a0)
 * Bias 66:  CxObjError       (a0)
 * Bias 72:  ClearCxObjError  (a0)
 * Bias 78:  SetCxObjPri      (a0,d0)
 * Bias 84:  AttachCxObj      (a0/a1)
 * Bias 90:  EnqueueCxObj     (a0/a1)
 * Bias 96:  InsertCxObj      (a0/a1/a2)
 * Bias 102: RemoveCxObj      (a0)
 * Bias 108: Private1
 * Bias 114: SetTranslate     (a0/a1)
 * Bias 120: SetFilter        (a0/a1)
 * Bias 126: SetFilterIX      (a0/a1)
 * Bias 132: ParseIX          (a0/a1)
 * Bias 138: CxMsgType        (a0)
 * Bias 144: CxMsgData        (a0)
 * Bias 150: CxMsgID          (a0)
 * Bias 156: DivertCxMsg      (a0/a1/a2)
 * Bias 162: RouteCxMsg       (a0/a1)
 * Bias 168: DisposeCxMsg     (a0)
 * Bias 174: InvertKeyMap     (d0/a0/a1)
 * Bias 180: AddIEvents       (a0)
 * Bias 186: Private2
 * Bias 192: Private3
 * Bias 198: Private4
 * Bias 204: MatchIX          (a0/a1)
 */
APTR __g_lxa_commodities_FuncTab [] =
{
    __g_lxa_commodities_OpenLib,           // -6   Standard
    __g_lxa_commodities_CloseLib,          // -12  Standard
    __g_lxa_commodities_ExpungeLib,        // -18  Standard
    __g_lxa_commodities_ExtFuncLib,        // -24  Standard (reserved)
    _commodities_CreateCxObj,              // -30  CreateCxObj
    _commodities_CxBroker,                 // -36  CxBroker
    _commodities_ActivateCxObj,            // -42  ActivateCxObj
    _commodities_DeleteCxObj,              // -48  DeleteCxObj
    _commodities_DeleteCxObjAll,           // -54  DeleteCxObjAll
    _commodities_CxObjType,                // -60  CxObjType
    _commodities_CxObjError,               // -66  CxObjError
    _commodities_ClearCxObjError,          // -72  ClearCxObjError
    _commodities_SetCxObjPri,              // -78  SetCxObjPri
    _commodities_AttachCxObj,              // -84  AttachCxObj
    _commodities_EnqueueCxObj,             // -90  EnqueueCxObj
    _commodities_InsertCxObj,              // -96  InsertCxObj
    _commodities_RemoveCxObj,              // -102 RemoveCxObj
    _commodities_Private1,                 // -108 Private1
    _commodities_SetTranslate,             // -114 SetTranslate
    _commodities_SetFilter,                // -120 SetFilter
    _commodities_SetFilterIX,              // -126 SetFilterIX
    _commodities_ParseIX,                  // -132 ParseIX
    _commodities_CxMsgType,                // -138 CxMsgType
    _commodities_CxMsgData,                // -144 CxMsgData
    _commodities_CxMsgID,                  // -150 CxMsgID
    _commodities_DivertCxMsg,              // -156 DivertCxMsg
    _commodities_RouteCxMsg,               // -162 RouteCxMsg
    _commodities_DisposeCxMsg,             // -168 DisposeCxMsg
    _commodities_InvertKeyMap,             // -174 InvertKeyMap
    _commodities_AddIEvents,               // -180 AddIEvents
    _commodities_Private2,                 // -186 Private2
    _commodities_Private3,                 // -192 Private3
    _commodities_Private4,                 // -198 Private4
    _commodities_MatchIX,                  // -204 MatchIX
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_commodities_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_commodities_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_commodities_ExLibID[0],
    (ULONG) 0
};
