/*
 * lxa arp.library implementation
 *
 * Provides the ARP (AmigaDOS Replacement Project) API as a stub library.
 * arp.library was a predecessor to the dos.library enhancements in
 * AmigaOS 2.0+, used by very old Amiga applications (KickPascal, etc.).
 * All functions return safe failure values so that applications which
 * open arp.library but don't strictly require it can continue to run.
 *
 * Function biases and register conventions from VBCC inline headers.
 * Biases -30 through -222 are reserved/private slots (33 entries).
 * Public functions start at bias -228 (Printf).
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

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "arp"
#define EXLIBVER   " 40.1 (2026/03/22)"

char __aligned _g_arp_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_arp_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_arp_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_arp_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/*
 * Library init/open/close/expunge
 */

struct Library * __g_lxa_arp_InitLib ( register struct Library   *libbase __asm("d0"),
                                      register BPTR              seglist __asm("a0"),
                                      register struct ExecBase  *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_arp: InitLib() called\n");
    return libbase;
}

BPTR __g_lxa_arp_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_arp_OpenLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_arp: OpenLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_arp_CloseLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_arp: CloseLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_arp_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_arp", "ExtFuncLib");
    return 0;
}

/* Reserved / private function stub */
ULONG _arp_Reserved ( void )
{
    DPRINTF (LOG_DEBUG, "_arp: Reserved/Private function called (stub)\n");
    return 0;
}

/*
 * Public API stubs
 *
 * arp.library function biases (from VBCC inline arp_protos.h):
 *
 * Biases -30 to -222: Reserved/private (33 slots)
 *
 * Bias -228: Printf           (a0,a1)
 * Bias -234: FPrintf          (d0,a0,a1)
 * Bias -240: Puts             (a1)
 * Bias -246: ReadLine         (a0)
 * Bias -252: GADS             (a0,d0,a1,a2,a3)
 * Bias -258: Atol             (a0)
 * Bias -264: EscapeString     (a0)
 * Bias -270: CheckAbort       (a1)
 * Bias -276: CheckBreak       (d1,a1)
 * Bias -282: Getenv           (a0,a1,d0)
 * Bias -288: Setenv           (a0,a1)
 * Bias -294: FileRequest      (a0)
 * Bias -300: CloseWindowSafely(a0,a1)
 * Bias -306: CreatePort       (a0,d0)
 * Bias -312: DeletePort       (a1)
 * Bias -318: SendPacket       (d0,a0,a1)
 * Bias -324: InitStdPacket    (d0,a0,a1,a2)
 * Bias -330: PathName         (d0,a0,d1)
 * Bias -336: Assign           (a0,a1)
 * Bias -342: DosAllocMem      (d0)
 * Bias -348: DosFreeMem       (a1)
 * Bias -354: BtoCStr          (a0,d0,d1)
 * Bias -360: CtoBStr          (a0,d0,d1)
 * Bias -366: GetDevInfo       (a2)
 * Bias -372: FreeTaskResList  ()
 * Bias -378: ArpExit          (d0,d2)
 * Bias -384: ArpAlloc         (d0)
 * Bias -390: ArpAllocMem      (d0,d1)
 * Bias -396: ArpOpen          (d1,d2)
 * Bias -402: ArpDupLock       (d1)
 * Bias -408: ArpLock          (d1,d2)
 * Bias -414: RListAlloc       (a0,d0)
 * Bias -420: FindCLI          (d0)
 * Bias -426: QSort            (a0,d0,d1,a1)
 * Bias -432: PatternMatch     (a0,a1)
 * Bias -438: FindFirst        (d0,a0)
 * Bias -444: FindNext         (a0)
 * Bias -450: FreeAnchorChain  (a0)
 * Bias -456: CompareLock      (d0,d1)
 * Bias -462: FindTaskResList  ()
 * Bias -468: CreateTaskResList()
 * Bias -474: FreeResList      (a1)
 * Bias -480: FreeTrackedItem  (a1)
 * Bias -486: (private)
 * Bias -492: GetAccess        (a1)
 * Bias -498: FreeAccess       (a1)
 * Bias -504: FreeDAList       (a1)
 * Bias -510: AddDANode        (a0,a1,d0,d1)
 * Bias -516: AddDADevs        (a0,d0)
 * Bias -522: Strcmp            (a0,a1)
 * Bias -528: Strncmp          (a0,a1,d0)
 * Bias -534: (private)
 * Bias -540: SyncRun          (a0,a1,d0,d1)
 * Bias -546: ASyncRun         (a0,a1,a2)
 * Bias -552: LoadPrg          (d1)
 * Bias -558: PreParse         (a0,a1)
 * Bias -564: StamptoStr       (a0)
 * Bias -570: StrtoStamp       (a0)
 * Bias -576: ObtainResidentPrg(a0)
 * Bias -582: AddResidentPrg   (d1,a0)
 * Bias -588: RemResidentPrg   (a0)
 * Bias -594: UnLoadPrg        (d1)
 * Bias -600: LMult            (d0,d1)
 * Bias -606: LDiv             (d0,d1)
 * Bias -612: LMod             (d0,d1)
 * Bias -618: CheckSumPrg      (d0)
 * Bias -624: TackOn           (a0,a1)
 * Bias -630: BaseName         (a0)
 * Bias -636: ReleaseResidentPrg (d1)
 * Bias -642: SPrintf          (d0,a0,a1)
 * Bias -648: GetKeywordIndex  (a0,a1)
 * Bias -654: ArpOpenLibrary   (a1,d0)
 * Bias -660: ArpAllocFreq     ()
 */

/* -228: Printf */
LONG _arp_Printf ( register struct Library *ArpBase __asm("a6"),
                   register STRPTR string __asm("a0"),
                   register APTR stream __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: Printf() (stub, returns 0)\n");
    return 0;
}

/* -234: FPrintf */
LONG _arp_FPrintf ( register struct Library *ArpBase __asm("a6"),
                    register BPTR file __asm("d0"),
                    register STRPTR string __asm("a0"),
                    register APTR stream __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: FPrintf() (stub, returns 0)\n");
    return 0;
}

/* -240: Puts */
LONG _arp_Puts ( register struct Library *ArpBase __asm("a6"),
                 register STRPTR string __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: Puts() string=%s (stub, returns 0)\n", STRORNULL(string));
    return 0;
}

/* -246: ReadLine */
LONG _arp_ReadLine ( register struct Library *ArpBase __asm("a6"),
                     register STRPTR buffer __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: ReadLine() (stub, returns -1)\n");
    if (buffer)
        buffer[0] = '\0';
    return -1;
}

/* -252: GADS */
LONG _arp_GADS ( register struct Library *ArpBase __asm("a6"),
                 register STRPTR line __asm("a0"),
                 register LONG len __asm("d0"),
                 register STRPTR help __asm("a1"),
                 register STRPTR *args __asm("a2"),
                 register STRPTR tmplate __asm("a3") )
{
    DPRINTF (LOG_DEBUG, "_arp: GADS() (stub, returns -1)\n");
    return -1;
}

/* -258: Atol */
LONG _arp_Atol ( register struct Library *ArpBase __asm("a6"),
                 register STRPTR string __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: Atol() string=%s (stub, returns 0)\n", STRORNULL(string));
    return 0;
}

/* -264: EscapeString */
ULONG _arp_EscapeString ( register struct Library *ArpBase __asm("a6"),
                          register STRPTR string __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: EscapeString() (stub, returns 0)\n");
    return 0;
}

/* -270: CheckAbort */
LONG _arp_CheckAbort ( register struct Library *ArpBase __asm("a6"),
                       register APTR func __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: CheckAbort() (stub, returns 0)\n");
    return 0;
}

/* -276: CheckBreak */
LONG _arp_CheckBreak ( register struct Library *ArpBase __asm("a6"),
                       register LONG masks __asm("d1"),
                       register APTR func __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: CheckBreak() masks=0x%lx (stub, returns 0)\n", masks);
    return 0;
}

/* -282: Getenv */
STRPTR _arp_Getenv ( register struct Library *ArpBase __asm("a6"),
                     register STRPTR name __asm("a0"),
                     register STRPTR buffer __asm("a1"),
                     register LONG size __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: Getenv() name=%s (stub, returns NULL)\n", STRORNULL(name));
    if (buffer && size > 0)
        buffer[0] = '\0';
    return NULL;
}

/* -288: Setenv */
BOOL _arp_Setenv ( register struct Library *ArpBase __asm("a6"),
                   register STRPTR varname __asm("a0"),
                   register STRPTR value __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: Setenv() varname=%s (stub, returns FALSE)\n", STRORNULL(varname));
    return FALSE;
}

/* -294: FileRequest */
STRPTR _arp_FileRequest ( register struct Library *ArpBase __asm("a6"),
                          register APTR filerequester __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: FileRequest() freq=0x%08lx (stub, returns NULL)\n", (ULONG)filerequester);
    return NULL;
}

/* -300: CloseWindowSafely */
void _arp_CloseWindowSafely ( register struct Library *ArpBase __asm("a6"),
                              register APTR window __asm("a0"),
                              register APTR furtherwindows __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: CloseWindowSafely() window=0x%08lx (stub)\n", (ULONG)window);
}

/* -306: CreatePort */
struct MsgPort * _arp_CreatePort ( register struct Library *ArpBase __asm("a6"),
                                   register STRPTR name __asm("a0"),
                                   register LONG pri __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: CreatePort() name=%s pri=%ld (stub, returns NULL)\n",
             STRORNULL(name), pri);
    return NULL;
}

/* -312: DeletePort */
void _arp_DeletePort ( register struct Library *ArpBase __asm("a6"),
                       register struct MsgPort *port __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: DeletePort() port=0x%08lx (stub)\n", (ULONG)port);
}

/* -318: SendPacket */
LONG _arp_SendPacket ( register struct Library *ArpBase __asm("a6"),
                       register LONG action __asm("d0"),
                       register LONG *args __asm("a0"),
                       register struct MsgPort *handler __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: SendPacket() action=%ld (stub, returns 0)\n", action);
    return 0;
}

/* -324: InitStdPacket */
void _arp_InitStdPacket ( register struct Library *ArpBase __asm("a6"),
                          register LONG action __asm("d0"),
                          register LONG *args __asm("a0"),
                          register APTR packet __asm("a1"),
                          register struct MsgPort *replyport __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_arp: InitStdPacket() action=%ld (stub)\n", action);
}

/* -330: PathName */
ULONG _arp_PathName ( register struct Library *ArpBase __asm("a6"),
                      register BPTR lock __asm("d0"),
                      register STRPTR buffer __asm("a0"),
                      register LONG componentcount __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: PathName() (stub, returns 0)\n");
    if (buffer)
        buffer[0] = '\0';
    return 0;
}

/* -336: Assign */
ULONG _arp_Assign ( register struct Library *ArpBase __asm("a6"),
                    register STRPTR logical __asm("a0"),
                    register STRPTR physical __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: Assign() logical=%s physical=%s (stub, returns 0)\n",
             STRORNULL(logical), STRORNULL(physical));
    return 0;
}

/* -342: DosAllocMem */
APTR _arp_DosAllocMem ( register struct Library *ArpBase __asm("a6"),
                        register LONG size __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: DosAllocMem() size=%ld (stub, returns NULL)\n", size);
    return NULL;
}

/* -348: DosFreeMem */
void _arp_DosFreeMem ( register struct Library *ArpBase __asm("a6"),
                       register APTR dosblock __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: DosFreeMem() dosblock=0x%08lx (stub)\n", (ULONG)dosblock);
}

/* -354: BtoCStr */
ULONG _arp_BtoCStr ( register struct Library *ArpBase __asm("a6"),
                     register STRPTR cstr __asm("a0"),
                     register BPTR bstr __asm("d0"),
                     register LONG maxlength __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: BtoCStr() (stub, returns 0)\n");
    if (cstr && maxlength > 0)
        cstr[0] = '\0';
    return 0;
}

/* -360: CtoBStr */
ULONG _arp_CtoBStr ( register struct Library *ArpBase __asm("a6"),
                     register STRPTR cstr __asm("a0"),
                     register BPTR bstr __asm("d0"),
                     register LONG maxlength __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: CtoBStr() (stub, returns 0)\n");
    return 0;
}

/* -366: GetDevInfo */
APTR _arp_GetDevInfo ( register struct Library *ArpBase __asm("a6"),
                       register APTR devnode __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_arp: GetDevInfo() (stub, returns NULL)\n");
    return NULL;
}

/* -372: FreeTaskResList */
BOOL _arp_FreeTaskResList ( register struct Library *ArpBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_arp: FreeTaskResList() (stub, returns FALSE)\n");
    return FALSE;
}

/* -378: ArpExit */
void _arp_ArpExit ( register struct Library *ArpBase __asm("a6"),
                    register LONG rc __asm("d0"),
                    register LONG result2 __asm("d2") )
{
    DPRINTF (LOG_DEBUG, "_arp: ArpExit() rc=%ld result2=%ld (stub)\n", rc, result2);
}

/* -384: ArpAlloc */
APTR _arp_ArpAlloc ( register struct Library *ArpBase __asm("a6"),
                     register LONG size __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: ArpAlloc() size=%ld (stub, returns NULL)\n", size);
    return NULL;
}

/* -390: ArpAllocMem */
APTR _arp_ArpAllocMem ( register struct Library *ArpBase __asm("a6"),
                        register LONG size __asm("d0"),
                        register LONG requirements __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: ArpAllocMem() size=%ld reqs=0x%lx (stub, returns NULL)\n", size, requirements);
    return NULL;
}

/* -396: ArpOpen */
BPTR _arp_ArpOpen ( register struct Library *ArpBase __asm("a6"),
                    register STRPTR name __asm("d1"),
                    register LONG mode __asm("d2") )
{
    DPRINTF (LOG_DEBUG, "_arp: ArpOpen() name=%s mode=%ld (stub, returns 0)\n",
             STRORNULL((char *)(ULONG)name), mode);
    return 0;
}

/* -402: ArpDupLock */
BPTR _arp_ArpDupLock ( register struct Library *ArpBase __asm("a6"),
                       register BPTR lock __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: ArpDupLock() (stub, returns 0)\n");
    return 0;
}

/* -408: ArpLock */
BPTR _arp_ArpLock ( register struct Library *ArpBase __asm("a6"),
                    register STRPTR name __asm("d1"),
                    register LONG mode __asm("d2") )
{
    DPRINTF (LOG_DEBUG, "_arp: ArpLock() name=%s mode=%ld (stub, returns 0)\n",
             STRORNULL((char *)(ULONG)name), mode);
    return 0;
}

/* -414: RListAlloc */
void _arp_RListAlloc ( register struct Library *ArpBase __asm("a6"),
                       register APTR reslist __asm("a0"),
                       register LONG size __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: RListAlloc() (stub)\n");
}

/* -420: FindCLI */
APTR _arp_FindCLI ( register struct Library *ArpBase __asm("a6"),
                    register LONG clinum __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: FindCLI() clinum=%ld (stub, returns NULL)\n", clinum);
    return NULL;
}

/* -426: QSort */
BOOL _arp_QSort ( register struct Library *ArpBase __asm("a6"),
                  register APTR base __asm("a0"),
                  register LONG rsize __asm("d0"),
                  register LONG bsize __asm("d1"),
                  register APTR comp __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: QSort() (stub, returns FALSE)\n");
    return FALSE;
}

/* -432: PatternMatch */
BOOL _arp_PatternMatch ( register struct Library *ArpBase __asm("a6"),
                         register STRPTR pattern __asm("a0"),
                         register STRPTR string __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: PatternMatch() pattern=%s string=%s (stub, returns FALSE)\n",
             STRORNULL(pattern), STRORNULL(string));
    return FALSE;
}

/* -438: FindFirst */
LONG _arp_FindFirst ( register struct Library *ArpBase __asm("a6"),
                      register STRPTR pattern __asm("d0"),
                      register APTR anchorpath __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: FindFirst() (stub, returns -1)\n");
    return -1;
}

/* -444: FindNext */
LONG _arp_FindNext ( register struct Library *ArpBase __asm("a6"),
                     register APTR anchorpath __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: FindNext() (stub, returns -1)\n");
    return -1;
}

/* -450: FreeAnchorChain */
void _arp_FreeAnchorChain ( register struct Library *ArpBase __asm("a6"),
                            register APTR anchorpath __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: FreeAnchorChain() (stub)\n");
}

/* -456: CompareLock */
ULONG _arp_CompareLock ( register struct Library *ArpBase __asm("a6"),
                         register BPTR lock1 __asm("d0"),
                         register BPTR lock2 __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: CompareLock() (stub, returns 0)\n");
    return 0;
}

/* -462: FindTaskResList */
APTR _arp_FindTaskResList ( register struct Library *ArpBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_arp: FindTaskResList() (stub, returns NULL)\n");
    return NULL;
}

/* -468: CreateTaskResList */
APTR _arp_CreateTaskResList ( register struct Library *ArpBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_arp: CreateTaskResList() (stub, returns NULL)\n");
    return NULL;
}

/* -474: FreeResList */
void _arp_FreeResList ( register struct Library *ArpBase __asm("a6"),
                        register APTR freelist __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: FreeResList() (stub)\n");
}

/* -480: FreeTrackedItem */
void _arp_FreeTrackedItem ( register struct Library *ArpBase __asm("a6"),
                            register APTR item __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: FreeTrackedItem() (stub)\n");
}

/* -492: GetAccess */
APTR _arp_GetAccess ( register struct Library *ArpBase __asm("a6"),
                      register APTR tracker __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: GetAccess() (stub, returns NULL)\n");
    return NULL;
}

/* -498: FreeAccess */
void _arp_FreeAccess ( register struct Library *ArpBase __asm("a6"),
                       register APTR tracker __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: FreeAccess() (stub)\n");
}

/* -504: FreeDAList */
void _arp_FreeDAList ( register struct Library *ArpBase __asm("a6"),
                       register APTR danode __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: FreeDAList() (stub)\n");
}

/* -510: AddDANode */
APTR _arp_AddDANode ( register struct Library *ArpBase __asm("a6"),
                      register STRPTR data __asm("a0"),
                      register APTR dalist __asm("a1"),
                      register LONG length __asm("d0"),
                      register LONG id __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: AddDANode() (stub, returns NULL)\n");
    return NULL;
}

/* -516: AddDADevs */
ULONG _arp_AddDADevs ( register struct Library *ArpBase __asm("a6"),
                       register APTR dalist __asm("a0"),
                       register LONG select __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: AddDADevs() (stub, returns 0)\n");
    return 0;
}

/* -522: Strcmp */
LONG _arp_Strcmp ( register struct Library *ArpBase __asm("a6"),
                  register STRPTR s1 __asm("a0"),
                  register STRPTR s2 __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: Strcmp() (stub, returns 0)\n");
    return 0;
}

/* -528: Strncmp */
LONG _arp_Strncmp ( register struct Library *ArpBase __asm("a6"),
                    register STRPTR s1 __asm("a0"),
                    register STRPTR s2 __asm("a1"),
                    register LONG count __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: Strncmp() (stub, returns 0)\n");
    return 0;
}

/* -540: SyncRun */
LONG _arp_SyncRun ( register struct Library *ArpBase __asm("a6"),
                    register STRPTR name __asm("a0"),
                    register STRPTR command __asm("a1"),
                    register BPTR input __asm("d0"),
                    register BPTR output __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: SyncRun() name=%s (stub, returns -1)\n", STRORNULL(name));
    return -1;
}

/* -546: ASyncRun */
LONG _arp_ASyncRun ( register struct Library *ArpBase __asm("a6"),
                     register STRPTR name __asm("a0"),
                     register STRPTR command __asm("a1"),
                     register APTR pcb __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_arp: ASyncRun() name=%s (stub, returns -1)\n", STRORNULL(name));
    return -1;
}

/* -552: LoadPrg */
BPTR _arp_LoadPrg ( register struct Library *ArpBase __asm("a6"),
                    register STRPTR name __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: LoadPrg() (stub, returns 0)\n");
    return 0;
}

/* -558: PreParse */
BOOL _arp_PreParse ( register struct Library *ArpBase __asm("a6"),
                     register STRPTR source __asm("a0"),
                     register STRPTR dest __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: PreParse() (stub, returns FALSE)\n");
    return FALSE;
}

/* -564: StamptoStr */
BOOL _arp_StamptoStr ( register struct Library *ArpBase __asm("a6"),
                       register APTR datetime __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: StamptoStr() (stub, returns FALSE)\n");
    return FALSE;
}

/* -570: StrtoStamp */
BOOL _arp_StrtoStamp ( register struct Library *ArpBase __asm("a6"),
                       register APTR datetime __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: StrtoStamp() (stub, returns FALSE)\n");
    return FALSE;
}

/* -576: ObtainResidentPrg */
APTR _arp_ObtainResidentPrg ( register struct Library *ArpBase __asm("a6"),
                              register STRPTR name __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: ObtainResidentPrg() name=%s (stub, returns NULL)\n", STRORNULL(name));
    return NULL;
}

/* -582: AddResidentPrg */
APTR _arp_AddResidentPrg ( register struct Library *ArpBase __asm("a6"),
                           register BPTR segment __asm("d1"),
                           register STRPTR name __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: AddResidentPrg() name=%s (stub, returns NULL)\n", STRORNULL(name));
    return NULL;
}

/* -588: RemResidentPrg */
LONG _arp_RemResidentPrg ( register struct Library *ArpBase __asm("a6"),
                           register STRPTR name __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: RemResidentPrg() name=%s (stub, returns -1)\n", STRORNULL(name));
    return -1;
}

/* -594: UnLoadPrg */
void _arp_UnLoadPrg ( register struct Library *ArpBase __asm("a6"),
                      register BPTR segment __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: UnLoadPrg() (stub)\n");
}

/* -600: LMult */
LONG _arp_LMult ( register struct Library *ArpBase __asm("a6"),
                  register LONG a __asm("d0"),
                  register LONG b __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: LMult() %ld * %ld (stub, returns 0)\n", a, b);
    return 0;
}

/* -606: LDiv */
LONG _arp_LDiv ( register struct Library *ArpBase __asm("a6"),
                 register LONG a __asm("d0"),
                 register LONG b __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: LDiv() %ld / %ld (stub, returns 0)\n", a, b);
    return 0;
}

/* -612: LMod */
LONG _arp_LMod ( register struct Library *ArpBase __asm("a6"),
                 register LONG a __asm("d0"),
                 register LONG b __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: LMod() %ld %% %ld (stub, returns 0)\n", a, b);
    return 0;
}

/* -618: CheckSumPrg */
ULONG _arp_CheckSumPrg ( register struct Library *ArpBase __asm("a6"),
                          register APTR residentnode __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: CheckSumPrg() (stub, returns 0)\n");
    return 0;
}

/* -624: TackOn */
void _arp_TackOn ( register struct Library *ArpBase __asm("a6"),
                   register STRPTR pathname __asm("a0"),
                   register STRPTR filename __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: TackOn() pathname=%s filename=%s (stub)\n",
             STRORNULL(pathname), STRORNULL(filename));
}

/* -630: BaseName */
STRPTR _arp_BaseName ( register struct Library *ArpBase __asm("a6"),
                       register STRPTR name __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_arp: BaseName() name=%s (stub, returns name)\n", STRORNULL(name));
    return name;
}

/* -636: ReleaseResidentPrg */
APTR _arp_ReleaseResidentPrg ( register struct Library *ArpBase __asm("a6"),
                               register BPTR segment __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_arp: ReleaseResidentPrg() (stub, returns NULL)\n");
    return NULL;
}

/* -642: SPrintf */
LONG _arp_SPrintf ( register struct Library *ArpBase __asm("a6"),
                    register STRPTR buffer __asm("d0"),
                    register STRPTR format __asm("a0"),
                    register APTR args __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: SPrintf() (stub, returns 0)\n");
    return 0;
}

/* -648: GetKeywordIndex */
LONG _arp_GetKeywordIndex ( register struct Library *ArpBase __asm("a6"),
                            register STRPTR keyword __asm("a0"),
                            register STRPTR tmplate __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_arp: GetKeywordIndex() keyword=%s (stub, returns -1)\n", STRORNULL(keyword));
    return -1;
}

/* -654: ArpOpenLibrary */
struct Library * _arp_ArpOpenLibrary ( register struct Library *ArpBase __asm("a6"),
                                      register STRPTR name __asm("a1"),
                                      register LONG vers __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_arp: ArpOpenLibrary() name=%s vers=%ld (stub, returns NULL)\n",
             STRORNULL(name), vers);
    return NULL;
}

/* -660: ArpAllocFreq */
APTR _arp_ArpAllocFreq ( register struct Library *ArpBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_arp: ArpAllocFreq() (stub, returns NULL)\n");
    return NULL;
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

extern APTR              __g_lxa_arp_FuncTab [];
extern struct MyDataInit __g_lxa_arp_DataTab;
extern struct InitTable  __g_lxa_arp_InitTab;
extern APTR              __g_lxa_arp_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_arp_EndResident,             // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_arp_ExLibName[0],                 // char  *rt_Name
    &_g_arp_ExLibID[0],                   // char  *rt_IdString
    &__g_lxa_arp_InitTab                  // APTR  rt_Init
};

APTR __g_lxa_arp_EndResident;
struct Resident *__lxa_arp_ROMTag = &ROMTag;

struct InitTable __g_lxa_arp_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_arp_FuncTab[0],
    (APTR)                &__g_lxa_arp_DataTab,
    (APTR)                __g_lxa_arp_InitLib
};

/*
 * Function table
 *
 * Standard library functions at -6 through -24
 * Reserved/private slots from -30 to -222 (33 slots)
 * Public functions from -228 to -660 (73 slots, including 2 private)
 * Total: 4 standard + 33 reserved + 73 public = 110 vectors
 */
APTR __g_lxa_arp_FuncTab [] =
{
    __g_lxa_arp_OpenLib,                  // -6   Open
    __g_lxa_arp_CloseLib,                 // -12  Close
    __g_lxa_arp_ExpungeLib,               // -18  Expunge
    __g_lxa_arp_ExtFuncLib,               // -24  Reserved
    /* Reserved/private slots -30 to -222 (33 slots) */
    _arp_Reserved,                        // -30  Private
    _arp_Reserved,                        // -36  Private
    _arp_Reserved,                        // -42  Private
    _arp_Reserved,                        // -48  Private
    _arp_Reserved,                        // -54  Private
    _arp_Reserved,                        // -60  Private
    _arp_Reserved,                        // -66  Private
    _arp_Reserved,                        // -72  Private
    _arp_Reserved,                        // -78  Private
    _arp_Reserved,                        // -84  Private
    _arp_Reserved,                        // -90  Private
    _arp_Reserved,                        // -96  Private
    _arp_Reserved,                        // -102 Private
    _arp_Reserved,                        // -108 Private
    _arp_Reserved,                        // -114 Private
    _arp_Reserved,                        // -120 Private
    _arp_Reserved,                        // -126 Private
    _arp_Reserved,                        // -132 Private
    _arp_Reserved,                        // -138 Private
    _arp_Reserved,                        // -144 Private
    _arp_Reserved,                        // -150 Private
    _arp_Reserved,                        // -156 Private
    _arp_Reserved,                        // -162 Private
    _arp_Reserved,                        // -168 Private
    _arp_Reserved,                        // -174 Private
    _arp_Reserved,                        // -180 Private
    _arp_Reserved,                        // -186 Private
    _arp_Reserved,                        // -192 Private
    _arp_Reserved,                        // -198 Private
    _arp_Reserved,                        // -204 Private
    _arp_Reserved,                        // -210 Private
    _arp_Reserved,                        // -216 Private
    _arp_Reserved,                        // -222 Private
    /* Public functions starting at -228 */
    _arp_Printf,                          // -228 Printf
    _arp_FPrintf,                         // -234 FPrintf
    _arp_Puts,                            // -240 Puts
    _arp_ReadLine,                        // -246 ReadLine
    _arp_GADS,                            // -252 GADS
    _arp_Atol,                            // -258 Atol
    _arp_EscapeString,                    // -264 EscapeString
    _arp_CheckAbort,                      // -270 CheckAbort
    _arp_CheckBreak,                      // -276 CheckBreak
    _arp_Getenv,                          // -282 Getenv
    _arp_Setenv,                          // -288 Setenv
    _arp_FileRequest,                     // -294 FileRequest
    _arp_CloseWindowSafely,               // -300 CloseWindowSafely
    _arp_CreatePort,                      // -306 CreatePort
    _arp_DeletePort,                      // -312 DeletePort
    _arp_SendPacket,                      // -318 SendPacket
    _arp_InitStdPacket,                   // -324 InitStdPacket
    _arp_PathName,                        // -330 PathName
    _arp_Assign,                          // -336 Assign
    _arp_DosAllocMem,                     // -342 DosAllocMem
    _arp_DosFreeMem,                      // -348 DosFreeMem
    _arp_BtoCStr,                         // -354 BtoCStr
    _arp_CtoBStr,                         // -360 CtoBStr
    _arp_GetDevInfo,                      // -366 GetDevInfo
    _arp_FreeTaskResList,                 // -372 FreeTaskResList
    _arp_ArpExit,                         // -378 ArpExit
    _arp_ArpAlloc,                        // -384 ArpAlloc
    _arp_ArpAllocMem,                     // -390 ArpAllocMem
    _arp_ArpOpen,                         // -396 ArpOpen
    _arp_ArpDupLock,                      // -402 ArpDupLock
    _arp_ArpLock,                         // -408 ArpLock
    _arp_RListAlloc,                      // -414 RListAlloc
    _arp_FindCLI,                         // -420 FindCLI
    _arp_QSort,                           // -426 QSort
    _arp_PatternMatch,                    // -432 PatternMatch
    _arp_FindFirst,                       // -438 FindFirst
    _arp_FindNext,                        // -444 FindNext
    _arp_FreeAnchorChain,                 // -450 FreeAnchorChain
    _arp_CompareLock,                     // -456 CompareLock
    _arp_FindTaskResList,                 // -462 FindTaskResList
    _arp_CreateTaskResList,               // -468 CreateTaskResList
    _arp_FreeResList,                     // -474 FreeResList
    _arp_FreeTrackedItem,                 // -480 FreeTrackedItem
    _arp_Reserved,                        // -486 Private
    _arp_GetAccess,                       // -492 GetAccess
    _arp_FreeAccess,                      // -498 FreeAccess
    _arp_FreeDAList,                      // -504 FreeDAList
    _arp_AddDANode,                       // -510 AddDANode
    _arp_AddDADevs,                       // -516 AddDADevs
    _arp_Strcmp,                          // -522 Strcmp
    _arp_Strncmp,                         // -528 Strncmp
    _arp_Reserved,                        // -534 Private
    _arp_SyncRun,                         // -540 SyncRun
    _arp_ASyncRun,                        // -546 ASyncRun
    _arp_LoadPrg,                         // -552 LoadPrg
    _arp_PreParse,                        // -558 PreParse
    _arp_StamptoStr,                      // -564 StamptoStr
    _arp_StrtoStamp,                      // -570 StrtoStamp
    _arp_ObtainResidentPrg,               // -576 ObtainResidentPrg
    _arp_AddResidentPrg,                  // -582 AddResidentPrg
    _arp_RemResidentPrg,                  // -588 RemResidentPrg
    _arp_UnLoadPrg,                       // -594 UnLoadPrg
    _arp_LMult,                           // -600 LMult
    _arp_LDiv,                            // -606 LDiv
    _arp_LMod,                            // -612 LMod
    _arp_CheckSumPrg,                     // -618 CheckSumPrg
    _arp_TackOn,                          // -624 TackOn
    _arp_BaseName,                        // -630 BaseName
    _arp_ReleaseResidentPrg,              // -636 ReleaseResidentPrg
    _arp_SPrintf,                         // -642 SPrintf
    _arp_GetKeywordIndex,                 // -648 GetKeywordIndex
    _arp_ArpOpenLibrary,                  // -654 ArpOpenLibrary
    _arp_ArpAllocFreq,                    // -660 ArpAllocFreq
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_arp_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_arp_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_arp_ExLibID[0],
    (ULONG) 0
};
