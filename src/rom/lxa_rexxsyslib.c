/*
 * lxa rexxsyslib.library implementation
 *
 * Provides the ARexx System Library API.
 * This is a stub implementation - ARexx operations will fail gracefully
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

#include <rexx/rxslib.h>
#include <rexx/storage.h>

#include "util.h"

#define VERSION    39
#define REVISION   1
#define EXLIBNAME  "rexxsyslib"
#define EXLIBVER   " 39.1 (2025/06/23)"

char __aligned _g_rexxsyslib_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_rexxsyslib_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_rexxsyslib_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_rexxsyslib_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/*
 * Library init/open/close/expunge
 */

struct RxsLib * __g_lxa_rexxsyslib_InitLib ( register struct RxsLib    *rexxbase __asm("a6"),
                                             register BPTR              seglist __asm("a0"),
                                             register struct ExecBase  *sysb __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: InitLib() called\n");
    rexxbase->rl_SegList = (LONG)seglist;
    return rexxbase;
}

BPTR __g_lxa_rexxsyslib_ExpungeLib ( register struct RxsLib *rexxbase __asm("a6") )
{
    return 0;
}

struct RxsLib * __g_lxa_rexxsyslib_OpenLib ( register struct RxsLib *rexxbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: OpenLib() called, rexxbase=0x%08lx\n", (ULONG)rexxbase);
    rexxbase->rl_Node.lib_OpenCnt++;
    rexxbase->rl_Node.lib_Flags &= ~LIBF_DELEXP;
    return rexxbase;
}

BPTR __g_lxa_rexxsyslib_CloseLib ( register struct RxsLib *rexxbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: CloseLib() called, rexxbase=0x%08lx\n", (ULONG)rexxbase);
    rexxbase->rl_Node.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_rexxsyslib_ExtFuncLib ( void )
{
    return 0;
}

/* Reserved function slots (bias 30-120, 16 slots) */
ULONG _rexxsyslib_Reserved ( void )
{
    return 0;
}

/*
 * Rexx Functions (V33+)
 * Starting at bias 126
 */

/* CreateArgstring - Create an argstring */
UBYTE * _rexxsyslib_CreateArgstring ( register struct RxsLib *RexxBase __asm("a6"),
                                      register STRPTR string __asm("a0"),
                                      register ULONG length __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: CreateArgstring() string=%s length=%ld (stub, returns NULL)\n",
             STRORNULL(string), length);
    /* Return NULL - we don't support ARexx */
    return NULL;
}

/* DeleteArgstring - Delete an argstring */
void _rexxsyslib_DeleteArgstring ( register struct RxsLib *RexxBase __asm("a6"),
                                   register UBYTE *argstring __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: DeleteArgstring() argstring=0x%08lx (stub)\n", (ULONG)argstring);
}

/* LengthArgstring - Get length of an argstring */
ULONG _rexxsyslib_LengthArgstring ( register struct RxsLib *RexxBase __asm("a6"),
                                    register UBYTE *argstring __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: LengthArgstring() argstring=0x%08lx (stub, returns 0)\n", (ULONG)argstring);
    return 0;
}

/* CreateRexxMsg - Create a REXX message packet */
struct RexxMsg * _rexxsyslib_CreateRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                             register struct MsgPort *port __asm("a0"),
                                             register STRPTR extension __asm("a1"),
                                             register STRPTR host __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: CreateRexxMsg() port=0x%08lx extension=%s host=%s (stub, returns NULL)\n",
             (ULONG)port, STRORNULL(extension), STRORNULL(host));
    return NULL;
}

/* DeleteRexxMsg - Delete a REXX message packet */
void _rexxsyslib_DeleteRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                 register struct RexxMsg *packet __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: DeleteRexxMsg() packet=0x%08lx (stub)\n", (ULONG)packet);
}

/* ClearRexxMsg - Clear REXX message argument slots */
void _rexxsyslib_ClearRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                register struct RexxMsg *msgptr __asm("a0"),
                                register ULONG count __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: ClearRexxMsg() msgptr=0x%08lx count=%ld (stub)\n",
             (ULONG)msgptr, count);
}

/* FillRexxMsg - Fill REXX message argument slots */
BOOL _rexxsyslib_FillRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                               register struct RexxMsg *msgptr __asm("a0"),
                               register ULONG count __asm("d0"),
                               register ULONG mask __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: FillRexxMsg() msgptr=0x%08lx count=%ld mask=0x%lx (stub, returns FALSE)\n",
             (ULONG)msgptr, count, mask);
    return FALSE;
}

/* IsRexxMsg - Check if message is a REXX message */
BOOL _rexxsyslib_IsRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                             register struct RexxMsg *msgptr __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: IsRexxMsg() msgptr=0x%08lx (stub, returns FALSE)\n", (ULONG)msgptr);
    return FALSE;
}

/* LockRexxBase - Lock the REXX library base (V36+) */
void _rexxsyslib_LockRexxBase ( register struct RxsLib *RexxBase __asm("a6"),
                                register ULONG resource __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: LockRexxBase() resource=%ld (stub)\n", resource);
}

/* UnlockRexxBase - Unlock the REXX library base (V36+) */
void _rexxsyslib_UnlockRexxBase ( register struct RxsLib *RexxBase __asm("a6"),
                                  register ULONG resource __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: UnlockRexxBase() resource=%ld (stub)\n", resource);
}

/* CreateRexxHostPort - Create a REXX host port (V45+) */
struct MsgPort * _rexxsyslib_CreateRexxHostPort ( register struct RxsLib *RexxBase __asm("a6"),
                                                  register STRPTR basename __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: CreateRexxHostPort() basename=%s (stub, returns NULL)\n",
             STRORNULL(basename));
    return NULL;
}

/* DeleteRexxHostPort - Delete a REXX host port (V45+) */
void _rexxsyslib_DeleteRexxHostPort ( register struct RxsLib *RexxBase __asm("a6"),
                                      register struct MsgPort *port __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: DeleteRexxHostPort() port=0x%08lx (stub)\n", (ULONG)port);
}

/* GetRexxVarFromMsg - Get variable from REXX message (V45+) */
LONG _rexxsyslib_GetRexxVarFromMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                     register STRPTR var __asm("a0"),
                                     register struct RexxMsg *msgptr __asm("a2"),
                                     register STRPTR value __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: GetRexxVarFromMsg() var=%s msgptr=0x%08lx (stub, returns -1)\n",
             STRORNULL(var), (ULONG)msgptr);
    return -1;
}

/* SetRexxVarFromMsg - Set variable in REXX message (V45+) */
LONG _rexxsyslib_SetRexxVarFromMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                     register STRPTR var __asm("a0"),
                                     register struct RexxMsg *msgptr __asm("a2"),
                                     register STRPTR value __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: SetRexxVarFromMsg() var=%s msgptr=0x%08lx value=%s (stub, returns -1)\n",
             STRORNULL(var), (ULONG)msgptr, STRORNULL(value));
    return -1;
}

/* LaunchRexxScript - Launch a REXX script (V45+) */
struct RexxMsg * _rexxsyslib_LaunchRexxScript ( register struct RxsLib *RexxBase __asm("a6"),
                                                register STRPTR script __asm("a0"),
                                                register struct MsgPort *replyport __asm("a1"),
                                                register STRPTR extension __asm("a2"),
                                                register BPTR input __asm("d1"),
                                                register BPTR output __asm("d2") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: LaunchRexxScript() script=%s (stub, returns NULL)\n",
             STRORNULL(script));
    return NULL;
}

/* FreeRexxMsg - Free a REXX message (V45+) */
void _rexxsyslib_FreeRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                               register struct RexxMsg *msgptr __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: FreeRexxMsg() msgptr=0x%08lx (stub)\n", (ULONG)msgptr);
}

/* GetRexxBufferFromMsg - Get buffer from REXX message (V47+) */
LONG _rexxsyslib_GetRexxBufferFromMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                        register STRPTR var __asm("a0"),
                                        register struct RexxMsg *msgptr __asm("a2"),
                                        register STRPTR buffer __asm("a1"),
                                        register ULONG buffer_size __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_rexxsyslib: GetRexxBufferFromMsg() var=%s msgptr=0x%08lx (stub, returns -1)\n",
             STRORNULL(var), (ULONG)msgptr);
    return -1;
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

extern APTR              __g_lxa_rexxsyslib_FuncTab [];
extern struct MyDataInit __g_lxa_rexxsyslib_DataTab;
extern struct InitTable  __g_lxa_rexxsyslib_InitTab;
extern APTR              __g_lxa_rexxsyslib_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_rexxsyslib_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_rexxsyslib_ExLibName[0],          // char  *rt_Name
    &_g_rexxsyslib_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_rexxsyslib_InitTab           // APTR  rt_Init
};

APTR __g_lxa_rexxsyslib_EndResident;
struct Resident *__lxa_rexxsyslib_ROMTag = &ROMTag;

struct InitTable __g_lxa_rexxsyslib_InitTab =
{
    (ULONG)               sizeof(struct RxsLib),
    (APTR              *) &__g_lxa_rexxsyslib_FuncTab[0],
    (APTR)                &__g_lxa_rexxsyslib_DataTab,
    (APTR)                __g_lxa_rexxsyslib_InitLib
};

/* Function table - from rexxsyslib_lib.fd
 * Standard library functions at -6 through -24
 * 16 reserved slots from -30 to -120
 * First real function at -126
 *
 * Bias 30-120: Reserved (16 slots)
 * Bias 126: CreateArgstring       (a0,d0)
 * Bias 132: DeleteArgstring       (a0)
 * Bias 138: LengthArgstring       (a0)
 * Bias 144: CreateRexxMsg         (a0/a1,d0)
 * Bias 150: DeleteRexxMsg         (a0)
 * Bias 156: ClearRexxMsg          (a0,d0)
 * Bias 162: FillRexxMsg           (a0,d0/d1)
 * Bias 168: IsRexxMsg             (a0)
 * ... private ...
 * Bias 450: LockRexxBase          (d0)
 * Bias 456: UnlockRexxBase        (d0)
 * ... private ...
 * Bias 480: CreateRexxHostPort    (a0)
 * Bias 486: DeleteRexxHostPort    (a0)
 * Bias 492: GetRexxVarFromMsg     (a0/a2,a1)
 * Bias 498: SetRexxVarFromMsg     (a0/a2,a1)
 * Bias 504: LaunchRexxScript      (a0/a1/a2,d1/d2)
 * Bias 510: FreeRexxMsg           (a0)
 * Bias 516: GetRexxBufferFromMsg  (a0/a2,a1,d0)
 */
APTR __g_lxa_rexxsyslib_FuncTab [] =
{
    __g_lxa_rexxsyslib_OpenLib,          // -6   Standard
    __g_lxa_rexxsyslib_CloseLib,         // -12  Standard
    __g_lxa_rexxsyslib_ExpungeLib,       // -18  Standard
    __g_lxa_rexxsyslib_ExtFuncLib,       // -24  Standard (reserved)
    /* Reserved slots -30 to -120 (16 slots) */
    _rexxsyslib_Reserved,                // -30  Reserved
    _rexxsyslib_Reserved,                // -36  Reserved
    _rexxsyslib_Reserved,                // -42  Reserved
    _rexxsyslib_Reserved,                // -48  Reserved
    _rexxsyslib_Reserved,                // -54  Reserved
    _rexxsyslib_Reserved,                // -60  Reserved
    _rexxsyslib_Reserved,                // -66  Reserved
    _rexxsyslib_Reserved,                // -72  Reserved
    _rexxsyslib_Reserved,                // -78  Reserved
    _rexxsyslib_Reserved,                // -84  Reserved
    _rexxsyslib_Reserved,                // -90  Reserved
    _rexxsyslib_Reserved,                // -96  Reserved
    _rexxsyslib_Reserved,                // -102 Reserved
    _rexxsyslib_Reserved,                // -108 Reserved
    _rexxsyslib_Reserved,                // -114 Reserved
    _rexxsyslib_Reserved,                // -120 Reserved
    /* V33 functions starting at -126 */
    _rexxsyslib_CreateArgstring,         // -126 CreateArgstring
    _rexxsyslib_DeleteArgstring,         // -132 DeleteArgstring
    _rexxsyslib_LengthArgstring,         // -138 LengthArgstring
    _rexxsyslib_CreateRexxMsg,           // -144 CreateRexxMsg
    _rexxsyslib_DeleteRexxMsg,           // -150 DeleteRexxMsg
    _rexxsyslib_ClearRexxMsg,            // -156 ClearRexxMsg
    _rexxsyslib_FillRexxMsg,             // -162 FillRexxMsg
    _rexxsyslib_IsRexxMsg,               // -168 IsRexxMsg
    /* Private slots -174 to -444 (46 slots) */
    _rexxsyslib_Reserved,                // -174 Private
    _rexxsyslib_Reserved,                // -180 Private
    _rexxsyslib_Reserved,                // -186 Private
    _rexxsyslib_Reserved,                // -192 Private
    _rexxsyslib_Reserved,                // -198 Private
    _rexxsyslib_Reserved,                // -204 Private
    _rexxsyslib_Reserved,                // -210 Private
    _rexxsyslib_Reserved,                // -216 Private
    _rexxsyslib_Reserved,                // -222 Private
    _rexxsyslib_Reserved,                // -228 Private
    _rexxsyslib_Reserved,                // -234 Private
    _rexxsyslib_Reserved,                // -240 Private
    _rexxsyslib_Reserved,                // -246 Private
    _rexxsyslib_Reserved,                // -252 Private
    _rexxsyslib_Reserved,                // -258 Private
    _rexxsyslib_Reserved,                // -264 Private
    _rexxsyslib_Reserved,                // -270 Private
    _rexxsyslib_Reserved,                // -276 Private
    _rexxsyslib_Reserved,                // -282 Private
    _rexxsyslib_Reserved,                // -288 Private
    _rexxsyslib_Reserved,                // -294 Private
    _rexxsyslib_Reserved,                // -300 Private
    _rexxsyslib_Reserved,                // -306 Private
    _rexxsyslib_Reserved,                // -312 Private
    _rexxsyslib_Reserved,                // -318 Private
    _rexxsyslib_Reserved,                // -324 Private
    _rexxsyslib_Reserved,                // -330 Private
    _rexxsyslib_Reserved,                // -336 Private
    _rexxsyslib_Reserved,                // -342 Private
    _rexxsyslib_Reserved,                // -348 Private
    _rexxsyslib_Reserved,                // -354 Private
    _rexxsyslib_Reserved,                // -360 Private
    _rexxsyslib_Reserved,                // -366 Private
    _rexxsyslib_Reserved,                // -372 Private
    _rexxsyslib_Reserved,                // -378 Private
    _rexxsyslib_Reserved,                // -384 Private
    _rexxsyslib_Reserved,                // -390 Private
    _rexxsyslib_Reserved,                // -396 Private
    _rexxsyslib_Reserved,                // -402 Private
    _rexxsyslib_Reserved,                // -408 Private
    _rexxsyslib_Reserved,                // -414 Private
    _rexxsyslib_Reserved,                // -420 Private
    _rexxsyslib_Reserved,                // -426 Private
    _rexxsyslib_Reserved,                // -432 Private
    _rexxsyslib_Reserved,                // -438 Private
    _rexxsyslib_Reserved,                // -444 Private
    /* V36+ functions at -450 */
    _rexxsyslib_LockRexxBase,            // -450 LockRexxBase
    _rexxsyslib_UnlockRexxBase,          // -456 UnlockRexxBase
    /* Private slots -462 to -474 (3 slots) */
    _rexxsyslib_Reserved,                // -462 Private
    _rexxsyslib_Reserved,                // -468 Private
    _rexxsyslib_Reserved,                // -474 Private
    /* V45+ functions at -480 */
    _rexxsyslib_CreateRexxHostPort,      // -480 CreateRexxHostPort
    _rexxsyslib_DeleteRexxHostPort,      // -486 DeleteRexxHostPort
    _rexxsyslib_GetRexxVarFromMsg,       // -492 GetRexxVarFromMsg
    _rexxsyslib_SetRexxVarFromMsg,       // -498 SetRexxVarFromMsg
    _rexxsyslib_LaunchRexxScript,        // -504 LaunchRexxScript
    _rexxsyslib_FreeRexxMsg,             // -510 FreeRexxMsg
    /* V47+ functions at -516 */
    _rexxsyslib_GetRexxBufferFromMsg,    // -516 GetRexxBufferFromMsg
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_rexxsyslib_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_rexxsyslib_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_rexxsyslib_ExLibID[0],
    (ULONG) 0
};
