/*
 * lxa rexxsyslib.library (Phase 250)
 *
 * The ARexx systems library as on AmigaOS 3.1 (V36): the library base
 * with its static strings, character attribute table and the REXX public
 * port, argstrings, RexxMsg packets and the base lock.  The resident
 * process (SYS:System/RexxMast) activates rl_RexxPort and keeps the
 * Library List, the Clip List and the task count in this base; the
 * interpreter is SYS:System/ARexx (Regina with lxa's ARexx glue).
 *
 * Behaviour follows the reference (tests/probes/rexx/syslib.c); the
 * private vectors of the 3.1 library are used by its own RexxMast only.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/semaphores.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <rexx/rxslib.h>
#include <rexx/storage.h>

#include "util.h"

#define VERSION    36
#define REVISION   21
#define EXLIBNAME  "rexxsyslib"
#define EXLIBVER   " 36.21 (01.11.2026)"

char __aligned _g_rexxsyslib_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_rexxsyslib_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_rexxsyslib_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_rexxsyslib_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

struct ExecBase   *SysBase = NULL;
struct DosLibrary *DOSBase = NULL;

/* the 3.1 base ends before rl_UtilityBase (lib_PosSize 252) */
#define RXS_POSSIZE ((ULONG)&((struct RxsLib *)0)->rl_UtilityBase)

/* LockRexxBase() serialises with this semaphore; rl_ReadLock counts */
static struct SignalSemaphore g_rexx_sem;

/*
 * The static strings of the base (struct NexxStr, a RexxArg without the
 * size).  Flags and hash as on 3.1: numbers are NSF_SOURCE|NSF_BINARY|
 * NSF_NUMBER|NSF_STRING (0x9a), words NSF_SOURCE|NSF_NOTNUM|NSF_STRING
 * (0x86); the hash is the byte sum.
 */
struct StaticNexx
{
    struct NexxStr ns;
};

#define NEXX(iv, len, flags, hash, s) { { (iv), (len), (flags), (hash), s } }

static struct StaticNexx g_nexx_null    = NEXX(0, 0, 0x86, 0x00, "");
static struct StaticNexx g_nexx_false   = NEXX(0, 1, 0x9a, 0x30, "0");
static struct StaticNexx g_nexx_true    = NEXX(1, 1, 0x9a, 0x31, "1");
static struct StaticNexx g_nexx_rexx    = NEXX(0, 4, 0x86, 0x47, "REXX");
static struct StaticNexx g_nexx_command  = NEXX(0, 7, 0x86, 0x05, "COMMAND");
static struct StaticNexx g_nexx_stdin   = NEXX(0, 5, 0x86, 0x7a, "STDIN");
static struct StaticNexx g_nexx_stdout  = NEXX(0, 6, 0x86, 0xd9, "STDOUT");
static struct StaticNexx g_nexx_stderr  = NEXX(0, 6, 0x86, 0xd5, "STDERR");

static char g_task_name[] = "ARexx";
static char g_rexx_dir[]  = "REXX:";

/*
 * The character attribute table (rl_CTABLE) of the reference: one byte
 * per character code.
 */
static const UBYTE g_ctable[256] =
{
    /* 0x00 */ 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    /* 0x10 */ 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    /* 0x20 */ 0x01,0x08,0x00,0x08,0x08,0x10,0x10,0x00,0x20,0x20,0x10,0x10,0x20,0x10,0x08,0x10,
    /* 0x30 */ 0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x20,0x20,0x10,0x10,0x10,0x08,
    /* 0x40 */ 0x08,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,
    /* 0x50 */ 0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x00,0x00,0x00,0x10,0x08,
    /* 0x60 */ 0x00,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,
    /* 0x70 */ 0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,0x00,0x10,0x00,0x10,0x01,
    /* 0x80 */ 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    /* 0x90 */ 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    /* 0xa0 */ 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0xb0 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0xc0 */ 0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,
    /* 0xd0 */ 0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x00,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,
    /* 0xe0 */ 0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,
    /* 0xf0 */ 0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x00,0x84,0x84,0x84,0x84,0x84,0x84,0x84,0x84,
};

/*
 * Library init/open/close/expunge
 */

struct RxsLib * __g_lxa_rexxsyslib_InitLib ( register struct RxsLib    *rexxbase __asm("d0"),
                                             register BPTR              seglist __asm("a0"),
                                             register struct ExecBase  *sysb __asm("a6"))
{
    struct RxsLib *rl = rexxbase;

    SysBase = sysb;
    DOSBase = (struct DosLibrary *)OpenLibrary((CONST_STRPTR)"dos.library", 36);
    if (!DOSBase)
        return NULL;

    InitSemaphore(&g_rexx_sem);

    rl->rl_Flags      = 0;
    rl->rl_Shadow     = 0;
    rl->rl_SysBase    = sysb;
    rl->rl_DOSBase    = DOSBase;
    rl->rl_IeeeDPBase = NULL;
    rl->rl_SegList    = (LONG)seglist;
    rl->rl_NIL        = (LONG)Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    rl->rl_Chunk      = RXSCHUNK;
    rl->rl_MaxNest    = RXSNEST;
    rl->rl_NULL       = &g_nexx_null.ns;
    rl->rl_FALSE      = &g_nexx_false.ns;
    rl->rl_TRUE       = &g_nexx_true.ns;
    rl->rl_REXX       = &g_nexx_rexx.ns;
    rl->rl_COMMAND    = &g_nexx_command.ns;
    rl->rl_STDIN      = &g_nexx_stdin.ns;
    rl->rl_STDOUT     = &g_nexx_stdout.ns;
    rl->rl_STDERR     = &g_nexx_stderr.ns;
    rl->rl_Version    = (STRPTR)&_g_rexxsyslib_ExLibID[0];
    rl->rl_TaskName   = (STRPTR)g_task_name;
    rl->rl_TaskPri    = RXSTPRI;
    rl->rl_TaskSeg    = 0;
    rl->rl_StackSize  = RXSSTACK;
    rl->rl_RexxDir    = (STRPTR)g_rexx_dir;
    rl->rl_CTABLE     = (STRPTR)g_ctable;
    rl->rl_Notice     = (STRPTR)&_g_rexxsyslib_Copyright[0];

    /* the REXX port stays inactive (no name, not public) until RexxMast
     * takes it over */
    rl->rl_RexxPort.mp_Node.ln_Type = 0;
    rl->rl_RexxPort.mp_Node.ln_Name = NULL;
    rl->rl_RexxPort.mp_Flags = PA_IGNORE;
    rl->rl_RexxPort.mp_SigBit = 0;
    rl->rl_RexxPort.mp_SigTask = NULL;
    NEWLIST(&rl->rl_RexxPort.mp_MsgList);

    rl->rl_ReadLock = 0;
    rl->rl_TraceFH  = 0;
    NEWLIST(&rl->rl_TaskList);
    rl->rl_NumTask  = 0;
    NEWLIST(&rl->rl_LibList);
    rl->rl_NumLib   = 0;
    NEWLIST(&rl->rl_ClipList);
    rl->rl_NumClip  = 0;
    NEWLIST(&rl->rl_MsgList);
    rl->rl_NumMsg   = 0;
    NEWLIST(&rl->rl_PgmList);
    rl->rl_NumPgm   = 0;
    rl->rl_TraceCnt = 0;
    rl->rl_avail    = 0;

    return rl;
}

BPTR __g_lxa_rexxsyslib_ExpungeLib ( register struct RxsLib *rexxbase __asm("a6") )
{
    /* the base carries the REXX port and the global lists: it stays */
    rexxbase->rl_Node.lib_Flags |= LIBF_DELEXP;
    return 0;
}

struct RxsLib * __g_lxa_rexxsyslib_OpenLib ( register struct RxsLib *rexxbase __asm("a6") )
{
    rexxbase->rl_Node.lib_OpenCnt++;
    rexxbase->rl_Node.lib_Flags &= ~LIBF_DELEXP;
    return rexxbase;
}

BPTR __g_lxa_rexxsyslib_CloseLib ( register struct RxsLib *rexxbase __asm("a6") )
{
    rexxbase->rl_Node.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_rexxsyslib_ExtFuncLib ( void )
{
    return 0;
}

/* the private vectors of the 3.1 library (its RexxMast's interface) */
ULONG _rexxsyslib_Private ( void )
{
    PRIVATE_FUNCTION_ERROR("_rexxsyslib", "Private");
    return 0;
}

/*
 * Argstrings
 */

static UBYTE *new_argstring(CONST_STRPTR string, ULONG length)
{
    ULONG size = length + 9;   /* RexxArg header (8), text, NUL */
    struct RexxArg *ra;
    UBYTE hash = 0;
    ULONG i;

    ra = (struct RexxArg *)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!ra)
        return NULL;
    ra->ra_Size = size;
    ra->ra_Length = (UWORD)length;
    ra->ra_Flags = NSF_EXT | NSF_NOTNUM | NSF_STRING;
    for (i = 0; i < length; i++)
    {
        UBYTE c = string ? (UBYTE)string[i] : 0;
        ((UBYTE *)ra->ra_Buff)[i] = c;
        hash += c;
    }
    ((UBYTE *)ra->ra_Buff)[length] = 0;
    ra->ra_Hash = hash;
    return (UBYTE *)ra->ra_Buff;
}

static void free_argstring(UBYTE *argstring)
{
    struct RexxArg *ra;

    if (!argstring)
        return;
    ra = (struct RexxArg *)(argstring - 8);
    FreeMem(ra, ra->ra_Size);
}

UBYTE * _rexxsyslib_CreateArgstring ( register struct RxsLib *RexxBase __asm("a6"),
                                      register STRPTR string __asm("a0"),
                                      register ULONG length __asm("d0") )
{
    (void)RexxBase;
    return new_argstring(string, length);
}

void _rexxsyslib_DeleteArgstring ( register struct RxsLib *RexxBase __asm("a6"),
                                   register UBYTE *argstring __asm("a0") )
{
    (void)RexxBase;
    free_argstring(argstring);
}

ULONG _rexxsyslib_LengthArgstring ( register struct RxsLib *RexxBase __asm("a6"),
                                    register UBYTE *argstring __asm("a0") )
{
    (void)RexxBase;
    if (!argstring)
        return 0;
    return ((struct RexxArg *)(argstring - 8))->ra_Length;
}

/*
 * RexxMsg packets
 */

struct RexxMsg * _rexxsyslib_CreateRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                             register struct MsgPort *port __asm("a0"),
                                             register STRPTR extension __asm("a1"),
                                             register STRPTR host __asm("d0") )
{
    struct RexxMsg *rm;

    (void)RexxBase;
    rm = (struct RexxMsg *)AllocMem(sizeof(struct RexxMsg), MEMF_PUBLIC | MEMF_CLEAR);
    if (!rm)
        return NULL;
    rm->rm_Node.mn_Length = sizeof(struct RexxMsg);
    rm->rm_Node.mn_ReplyPort = port;
    rm->rm_FileExt = extension;
    rm->rm_CommAddr = host;
    return rm;
}

void _rexxsyslib_DeleteRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                 register struct RexxMsg *packet __asm("a0") )
{
    (void)RexxBase;
    if (packet)
        FreeMem(packet, packet->rm_Node.mn_Length ? packet->rm_Node.mn_Length : sizeof(struct RexxMsg));
}

void _rexxsyslib_ClearRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                                register struct RexxMsg *msgptr __asm("a0"),
                                register ULONG count __asm("d0") )
{
    ULONG i;

    (void)RexxBase;
    if (!msgptr)
        return;
    if (count > MAXRMARG + 1)
        count = MAXRMARG + 1;
    for (i = 0; i < count; i++)
    {
        if (msgptr->rm_Args[i])
            free_argstring((UBYTE *)msgptr->rm_Args[i]);
        msgptr->rm_Args[i] = NULL;
    }
}

/* decimal text of a LONG; returns the length */
static ULONG long_to_text(LONG value, char *buf)
{
    char tmp[12];
    ULONG n = 0, len = 0;
    ULONG v = value < 0 ? (ULONG)-value : (ULONG)value;

    do
    {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    if (value < 0)
        buf[len++] = '-';
    while (n)
        buf[len++] = tmp[--n];
    buf[len] = 0;
    return len;
}

LONG _rexxsyslib_FillRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                               register struct RexxMsg *msgptr __asm("a0"),
                               register ULONG count __asm("d0"),
                               register ULONG mask __asm("d1") )
{
    UBYTE *made[MAXRMARG + 1];
    char num[12];
    ULONG i, len;

    (void)RexxBase;
    if (!msgptr)
        return 0;
    if (count > MAXRMARG + 1)
        count = MAXRMARG + 1;

    for (i = 0; i < count; i++)
    {
        made[i] = NULL;
        if (mask & (1UL << i))
        {
            len = long_to_text((LONG)msgptr->rm_Args[i], num);
            made[i] = new_argstring((CONST_STRPTR)num, len);
        }
        else if (msgptr->rm_Args[i])
        {
            const char *s = (const char *)msgptr->rm_Args[i];
            for (len = 0; s[len]; len++)
                ;
            made[i] = new_argstring((CONST_STRPTR)s, len);
        }
        else
            continue;

        if (!made[i])
        {
            /* out of memory: undo, the slots keep their values */
            while (i--)
                if (made[i])
                    free_argstring(made[i]);
            return 0;
        }
    }

    for (i = 0; i < count; i++)
        if (made[i])
            msgptr->rm_Args[i] = (STRPTR)made[i];
    return -1;
}

LONG _rexxsyslib_IsRexxMsg ( register struct RxsLib *RexxBase __asm("a6"),
                             register struct RexxMsg *msgptr __asm("a0") )
{
    const char *name;

    (void)RexxBase;
    if (!msgptr)
        return 0;
    name = msgptr->rm_Node.mn_Node.ln_Name;
    if (!name)
        return 0;
    return name[0] == 'R' && name[1] == 'E' && name[2] == 'X' && name[3] == 'X' && name[4] == 0 ? -1 : 0;
}

/*
 * The base lock (V36)
 */

void _rexxsyslib_LockRexxBase ( register struct RxsLib *RexxBase __asm("a6"),
                                register ULONG resource __asm("d0") )
{
    (void)resource;
    ObtainSemaphore(&g_rexx_sem);
    RexxBase->rl_ReadLock++;
}

void _rexxsyslib_UnlockRexxBase ( register struct RxsLib *RexxBase __asm("a6"),
                                  register ULONG resource __asm("d0") )
{
    (void)resource;
    if (RexxBase->rl_ReadLock)
        RexxBase->rl_ReadLock--;
    ReleaseSemaphore(&g_rexx_sem);
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
    (ULONG)               RXS_POSSIZE,
    (APTR              *) &__g_lxa_rexxsyslib_FuncTab[0],
    (APTR)                &__g_lxa_rexxsyslib_DataTab,
    (APTR)                __g_lxa_rexxsyslib_InitLib
};

/* Function table (rexxsyslib_lib.fd, V36): 16 reserved vectors, the
 * public functions from -126, private vectors and LockRexxBase /
 * UnlockRexxBase at -450/-456; 79 vectors (lib_NegSize 476). */
APTR __g_lxa_rexxsyslib_FuncTab [] =
{
    __g_lxa_rexxsyslib_OpenLib,          //   -6 Open
    __g_lxa_rexxsyslib_CloseLib,         //  -12 Close
    __g_lxa_rexxsyslib_ExpungeLib,       //  -18 Expunge
    __g_lxa_rexxsyslib_ExtFuncLib,       //  -24 Reserved
    _rexxsyslib_Private,                 //  -30 Private
    _rexxsyslib_Private,                 //  -36 Private
    _rexxsyslib_Private,                 //  -42 Private
    _rexxsyslib_Private,                 //  -48 Private
    _rexxsyslib_Private,                 //  -54 Private
    _rexxsyslib_Private,                 //  -60 Private
    _rexxsyslib_Private,                 //  -66 Private
    _rexxsyslib_Private,                 //  -72 Private
    _rexxsyslib_Private,                 //  -78 Private
    _rexxsyslib_Private,                 //  -84 Private
    _rexxsyslib_Private,                 //  -90 Private
    _rexxsyslib_Private,                 //  -96 Private
    _rexxsyslib_Private,                 // -102 Private
    _rexxsyslib_Private,                 // -108 Private
    _rexxsyslib_Private,                 // -114 Private
    _rexxsyslib_Private,                 // -120 Private
    _rexxsyslib_CreateArgstring,         // -126 CreateArgstring
    _rexxsyslib_DeleteArgstring,         // -132 DeleteArgstring
    _rexxsyslib_LengthArgstring,         // -138 LengthArgstring
    _rexxsyslib_CreateRexxMsg,           // -144 CreateRexxMsg
    _rexxsyslib_DeleteRexxMsg,           // -150 DeleteRexxMsg
    _rexxsyslib_ClearRexxMsg,            // -156 ClearRexxMsg
    _rexxsyslib_FillRexxMsg,             // -162 FillRexxMsg
    _rexxsyslib_IsRexxMsg,               // -168 IsRexxMsg
    _rexxsyslib_Private,                 // -174 Private
    _rexxsyslib_Private,                 // -180 Private
    _rexxsyslib_Private,                 // -186 Private
    _rexxsyslib_Private,                 // -192 Private
    _rexxsyslib_Private,                 // -198 Private
    _rexxsyslib_Private,                 // -204 Private
    _rexxsyslib_Private,                 // -210 Private
    _rexxsyslib_Private,                 // -216 Private
    _rexxsyslib_Private,                 // -222 Private
    _rexxsyslib_Private,                 // -228 Private
    _rexxsyslib_Private,                 // -234 Private
    _rexxsyslib_Private,                 // -240 Private
    _rexxsyslib_Private,                 // -246 Private
    _rexxsyslib_Private,                 // -252 Private
    _rexxsyslib_Private,                 // -258 Private
    _rexxsyslib_Private,                 // -264 Private
    _rexxsyslib_Private,                 // -270 Private
    _rexxsyslib_Private,                 // -276 Private
    _rexxsyslib_Private,                 // -282 Private
    _rexxsyslib_Private,                 // -288 Private
    _rexxsyslib_Private,                 // -294 Private
    _rexxsyslib_Private,                 // -300 Private
    _rexxsyslib_Private,                 // -306 Private
    _rexxsyslib_Private,                 // -312 Private
    _rexxsyslib_Private,                 // -318 Private
    _rexxsyslib_Private,                 // -324 Private
    _rexxsyslib_Private,                 // -330 Private
    _rexxsyslib_Private,                 // -336 Private
    _rexxsyslib_Private,                 // -342 Private
    _rexxsyslib_Private,                 // -348 Private
    _rexxsyslib_Private,                 // -354 Private
    _rexxsyslib_Private,                 // -360 Private
    _rexxsyslib_Private,                 // -366 Private
    _rexxsyslib_Private,                 // -372 Private
    _rexxsyslib_Private,                 // -378 Private
    _rexxsyslib_Private,                 // -384 Private
    _rexxsyslib_Private,                 // -390 Private
    _rexxsyslib_Private,                 // -396 Private
    _rexxsyslib_Private,                 // -402 Private
    _rexxsyslib_Private,                 // -408 Private
    _rexxsyslib_Private,                 // -414 Private
    _rexxsyslib_Private,                 // -420 Private
    _rexxsyslib_Private,                 // -426 Private
    _rexxsyslib_Private,                 // -432 Private
    _rexxsyslib_Private,                 // -438 Private
    _rexxsyslib_Private,                 // -444 Private
    _rexxsyslib_LockRexxBase,            // -450 LockRexxBase
    _rexxsyslib_UnlockRexxBase,          // -456 UnlockRexxBase
    _rexxsyslib_Private,                 // -462 Private
    _rexxsyslib_Private,                 // -468 Private
    _rexxsyslib_Private,                 // -474 Private
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
