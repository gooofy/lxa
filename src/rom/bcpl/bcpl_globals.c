/*
 * bcpl_globals.c - the system global vector of lxa's BCPL support
 *
 * BCPL code calls the system through its global vector: entry G of the
 * vector a2 points at is called with "movea.l G(a2),a4; jsr (a5)".  Every
 * system global that lxa implements in C has an entry stub (bcpl_entry.S)
 * that comes here with the global's offset and the routine's frame (its
 * arguments: args[0] = d1, args[1] = d2, ...).  Pointers in BCPL are
 * mostly BPTRs (machine address / 4); strings are BSTRs (length byte,
 * characters).
 *
 * The offsets and their meaning are the BCPL interface of AmigaDOS as
 * documented for 1.x programs and observed on AmigaOS 3.1
 * (tests/probes/dos/bcpl.c, tests/probes/dos/entryregs.c).
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/rdargs.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <devices/timer.h>
#include <intuition/intuition.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>

#include <stddef.h>

#include "../util.h"
#include "lxa_bcpl.h"

extern struct ExecBase      *SysBase;
extern struct DosLibrary    *DOSBase;
extern struct IntuitionBase *IntuitionBase;

_Static_assert(offsetof(struct Process, pr_ReturnAddr) == LXA_BCPL_PR_RETURNADDR, "pr_ReturnAddr");

typedef LONG (*bcpl_global)(LONG *args, ULONG *gv);

/* slot index (into lxa_bcpl_stubs) and global vector index of an offset */
#define SLOT(off)       (((off) + LXA_BCPL_NEG_BYTES) / 4)
#define GVI(off)        ((off) / 4)

/* the offsets lxa gives special meaning to */
#define OFF_STOP        0x008
#define OFF_LONGCALL    0x048
#define OFF_REMPROCESS  0x088
#define OFF_WRCH        0x0e0
#define OFF_WRITEI      0x114
#define OFF_WRITEHEX    0x11c
#define OFF_WRITEOCT    0x120
#define OFF_WRITES      0x124
#define OFF_WRITET      0x0d4
#define OFF_DOSBASE     0x204

#define BP(x)           ((APTR)((ULONG)(x) << 2))
#define BSTR_LEN(b)     (((UBYTE *)BP(b))[0])
#define BSTR_TXT(b)     ((UBYTE *)BP(b) + 1)

/* ------------------------------------------------------------------ helpers */

/* A BSTR as a C string in buf (at most size - 1 characters) */
static STRPTR cstr(BSTR b, char *buf, LONG size)
{
    LONG n = 0, len;

    if (b) {
        len = BSTR_LEN(b);
        for (; n < len && n < size - 1; n++)
            buf[n] = BSTR_TXT(b)[n];
    }
    buf[n] = 0;
    return (STRPTR)buf;
}

static UBYTE up(UBYTE c)
{
    return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}

static struct Process *self(void)
{
    return (struct Process *)FindTask(NULL);
}

static const bcpl_global g_globals[LXA_BCPL_SLOTS];

static APTR stub_of(LONG off)
{
    return (APTR)(lxa_bcpl_stubs + SLOT(off) * LXA_BCPL_STUB_BYTES);
}

/*
 * Call global 'off' of the vector the program uses, so that a program
 * that replaced e.g. wrch sees writef's output.  lxa's own globals are
 * called directly.
 */
static LONG call_global(ULONG *gv, LONG *args, LONG off, LONG x, LONG y)
{
    APTR fn = (APTR)gv[GVI(off)];
    LONG *frame = args + LXA_BCPL_CALLBACK_GAP;

    if (!fn || fn == stub_of(off)) {
        frame[0] = x;
        frame[1] = y;
        frame[2] = 0;
        frame[3] = 0;
        return g_globals[SLOT(off)](frame, gv);
    }
    return lxa_bcpl_enter(fn, gv, frame, NULL, x, y, 0, 0);
}

static void put_ch(ULONG *gv, LONG *args, LONG c)
{
    call_global(gv, args, OFF_WRCH, c, 0);
}

/* number output: decimal pads with blanks and is signed, hex/octal pad
 * with zeros and print the bit pattern */
static void put_number(ULONG *gv, LONG *args, LONG value, LONG width, ULONG base)
{
    char digits[34];
    LONG n = 0;
    BOOL minus = base == 10 && value < 0;
    ULONG u = minus ? 0u - (ULONG)value : (ULONG)value;

    do {
        ULONG d = u % base;
        digits[n++] = d < 10 ? '0' + d : 'A' + d - 10;
        u /= base;
    } while (u);

    for (width -= n + (minus ? 1 : 0); width > 0; width--)
        put_ch(gv, args, base == 10 ? ' ' : '0');
    if (minus)
        put_ch(gv, args, '-');
    while (n > 0)
        put_ch(gv, args, digits[--n]);
}

/* field width digit of writef: 0-9, A-Z */
static LONG width_digit(UBYTE c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = up(c);
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 0;
}

/* -------------------------------------------------------- the dos wrappers */
/* The globals at negative offsets take the dos.library arguments as they are */

static LONG g_Open(LONG *a, ULONG *gv)           { (void)gv; return Open((STRPTR)a[0], a[1]); }
static LONG g_Close(LONG *a, ULONG *gv)          { (void)gv; return Close(a[0]); }
static LONG g_Read(LONG *a, ULONG *gv)           { (void)gv; return Read(a[0], (APTR)a[1], a[2]); }
static LONG g_Write(LONG *a, ULONG *gv)          { (void)gv; return Write(a[0], (APTR)a[1], a[2]); }
static LONG g_Seek(LONG *a, ULONG *gv)           { (void)gv; return Seek(a[0], a[1], a[2]); }
static LONG g_DeleteFile(LONG *a, ULONG *gv)     { (void)gv; return DeleteFile((STRPTR)a[0]); }
static LONG g_Rename(LONG *a, ULONG *gv)         { (void)gv; return Rename((STRPTR)a[0], (STRPTR)a[1]); }
static LONG g_Lock(LONG *a, ULONG *gv)           { (void)gv; return Lock((STRPTR)a[0], a[1]); }
static LONG g_UnLock(LONG *a, ULONG *gv)         { (void)gv; UnLock(a[0]); return 0; }
static LONG g_DupLock(LONG *a, ULONG *gv)        { (void)gv; return DupLock(a[0]); }
static LONG g_Examine(LONG *a, ULONG *gv)        { (void)gv; return Examine(a[0], (struct FileInfoBlock *)a[1]); }
static LONG g_ExNext(LONG *a, ULONG *gv)         { (void)gv; return ExNext(a[0], (struct FileInfoBlock *)a[1]); }
static LONG g_Info(LONG *a, ULONG *gv)           { (void)gv; return Info(a[0], (struct InfoData *)a[1]); }
static LONG g_CreateDir(LONG *a, ULONG *gv)      { (void)gv; return CreateDir((STRPTR)a[0]); }
static LONG g_CurrentDir(LONG *a, ULONG *gv)     { (void)gv; return CurrentDir(a[0]); }
static LONG g_IoErr(LONG *a, ULONG *gv)          { (void)a; (void)gv; return IoErr(); }
static LONG g_CreateProc(LONG *a, ULONG *gv)     { (void)gv; return (LONG)CreateProc((STRPTR)a[0], a[1], a[2], a[3]); }
static LONG g_LoadSeg(LONG *a, ULONG *gv)        { (void)gv; return LoadSeg((STRPTR)a[0]); }
static LONG g_UnLoadSeg(LONG *a, ULONG *gv)      { (void)gv; UnLoadSeg(a[0]); return 0; }
static LONG g_SetComment(LONG *a, ULONG *gv)     { (void)gv; return SetComment((STRPTR)a[0], (STRPTR)a[1]); }
static LONG g_SetProtection(LONG *a, ULONG *gv)  { (void)gv; return SetProtection((STRPTR)a[0], a[1]); }
static LONG g_DateStamp(LONG *a, ULONG *gv)      { (void)gv; return (LONG)DateStamp((struct DateStamp *)a[0]); }
static LONG g_IsInteractive(LONG *a, ULONG *gv)  { (void)gv; return IsInteractive(a[0]); }
static LONG g_Execute(LONG *a, ULONG *gv)        { (void)gv; return Execute((STRPTR)a[0], a[1], a[2]); }
static LONG g_ParentDir(LONG *a, ULONG *gv)      { (void)gv; return ParentDir(a[0]); }
static LONG g_Delay(LONG *a, ULONG *gv)          { (void)gv; Delay(a[0]); return 0; }
static LONG g_WaitForChar(LONG *a, ULONG *gv)    { (void)gv; return WaitForChar(a[0], a[1]); }
static LONG g_SelectInput(LONG *a, ULONG *gv)    { (void)gv; return SelectInput(a[0]); }
static LONG g_SelectOutput(LONG *a, ULONG *gv)   { (void)gv; return SelectOutput(a[0]); }
static LONG g_Input(LONG *a, ULONG *gv)          { (void)a; (void)gv; return Input(); }
static LONG g_Output(LONG *a, ULONG *gv)         { (void)a; (void)gv; return Output(); }

/* NoReqLoadSeg(name): LoadSeg() without volume requesters */
static LONG g_NoReqLoadSeg(LONG *a, ULONG *gv)
{
    struct Process *me = self();
    char name[256];
    APTR win;
    BPTR seg;

    (void)gv;
    if (!a[0]) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return 0;
    }
    win = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    seg = LoadSeg(cstr(a[0], name, sizeof(name)));
    me->pr_WindowPtr = win;
    return seg;
}

/* --------------------------------------------------------------- strings */

/* toCStr(bstr): the BSTR becomes a C string in place */
static LONG g_toCStr(LONG *a, ULONG *gv)
{
    UBYTE *s = BP(a[0]);
    LONG i, len = s[0];

    (void)gv;
    for (i = 0; i < len; i++)
        s[i] = s[i + 1];
    s[len] = 0;
    return (LONG)s;
}

/* toBSTR(cstring, bstr) */
static LONG g_toBSTR(LONG *a, ULONG *gv)
{
    const UBYTE *src = (const UBYTE *)a[0];
    UBYTE *dst = BP(a[1]);
    LONG n = 0;

    (void)gv;
    while (src && src[n] && n < 255) {
        dst[n + 1] = src[n];
        n++;
    }
    dst[0] = n;
    return 0;
}

/* packstring(vector, bstr): one character per long -> bytes; returns the
 * upper bound of the packed string in longs */
static LONG g_packstring(LONG *a, ULONG *gv)
{
    LONG *v = BP(a[0]);
    UBYTE *s = BP(a[1]);
    LONG i, len = v[0] & 0xff;

    (void)gv;
    for (i = 0; i <= len; i++)
        s[i] = v[i];
    return len / 4;
}

/* unpackstring(bstr, vector) */
static LONG g_unpackstring(LONG *a, ULONG *gv)
{
    UBYTE *s = BP(a[0]);
    LONG *v = BP(a[1]);
    LONG i, len = s[0];

    (void)gv;
    for (i = 0; i <= len; i++)
        v[i] = s[i];
    return 0;
}

/* strcpy(src, dst), BSTRs */
static LONG g_strcpy(LONG *a, ULONG *gv)
{
    UBYTE *src = BP(a[0]), *dst = BP(a[1]);
    LONG i, len = src[0];

    (void)gv;
    for (i = 0; i <= len; i++)
        dst[i] = src[i];
    return 0;
}

static LONG g_toupper(LONG *a, ULONG *gv)
{
    (void)gv;
    return (a[0] & ~0xff) | up(a[0]);
}

/* charicmp(c1, c2): difference ignoring case */
static LONG g_charicmp(LONG *a, ULONG *gv)
{
    (void)gv;
    return (LONG)up(a[0]) - (LONG)up(a[1]);
}

/* stricmp(s1, s2), BSTRs: difference of the first differing characters,
 * otherwise -1/0/1 by length */
static LONG g_stricmp(LONG *a, ULONG *gv)
{
    UBYTE *s1 = BP(a[0]), *s2 = BP(a[1]);
    LONG i, l1 = s1[0], l2 = s2[0];

    (void)gv;
    for (i = 1; i <= l1 && i <= l2; i++) {
        LONG d = (LONG)up(s1[i]) - (LONG)up(s2[i]);
        if (d)
            return d;
    }
    if (l1 == l2)
        return 0;
    return l1 < l2 ? -1 : 1;
}

/* ---------------------------------------------------------------- memory */

static LONG g_getbyte(LONG *a, ULONG *gv)  { (void)gv; return ((UBYTE *)BP(a[0]))[a[1]]; }
static LONG g_putbyte(LONG *a, ULONG *gv)  { (void)gv; ((UBYTE *)BP(a[0]))[a[1]] = a[2]; return 0; }
static LONG g_getword(LONG *a, ULONG *gv)  { (void)gv; return ((UWORD *)BP(a[0]))[a[1]]; }
static LONG g_putword(LONG *a, ULONG *gv)  { (void)gv; ((UWORD *)BP(a[0]))[a[1]] = a[2]; return 0; }

/* getlong(index, address) / putlong(index, address, value): the long at
 * address + 4 * index; putlong returns the old value */
static LONG g_getlong(LONG *a, ULONG *gv)
{
    (void)gv;
    return *(LONG *)((UBYTE *)a[1] + (a[0] << 2));
}

static LONG g_putlong(LONG *a, ULONG *gv)
{
    LONG *p = (LONG *)((UBYTE *)a[1] + (a[0] << 2));
    LONG old = *p;

    (void)gv;
    *p = a[2];
    return old;
}

/* packlw(address, n): n bytes, most significant first, into a long */
static LONG g_packlw(LONG *a, ULONG *gv)
{
    const UBYTE *p = (const UBYTE *)a[0];
    ULONG v = 0;
    LONG n = a[1] & 0xff;

    (void)gv;
    if (!n)
        n = 256;
    while (n--)
        v = (v << 8) | *p++;
    return v;
}

/* unpacklw(address, n, value): the top n bytes of value to address */
static LONG g_unpacklw(LONG *a, ULONG *gv)
{
    UBYTE *p = (UBYTE *)a[0];
    ULONG v = a[2];
    LONG n = a[1] & 0xff;

    (void)gv;
    if (!n)
        n = 256;
    while (n--) {
        v = (v << 8) | (v >> 24);
        *p++ = v;
    }
    return 0;
}

/* clearvec(v, upb) / copyvec(src, dst, upb): upb + 1 longs */
static LONG g_clearvec(LONG *a, ULONG *gv)
{
    LONG *v = BP(a[0]);
    LONG i;

    (void)gv;
    for (i = 0; i <= a[1]; i++)
        v[i] = 0;
    return 0;
}

static LONG g_copyvec(LONG *a, ULONG *gv)
{
    LONG *src = BP(a[0]), *dst = BP(a[1]);
    LONG i;

    (void)gv;
    for (i = 0; i <= a[2]; i++)
        dst[i] = src[i];
    return 0;
}

/* getvec(upb): a vector of upb + 1 longs, as a BPTR */
static LONG g_getvec(LONG *a, ULONG *gv)
{
    APTR mem = AllocVec((a[0] + 1) << 2, MEMF_PUBLIC);

    if (!mem) {
        if (gv)
            SetIoErr(ERROR_NO_FREE_STORE);
        return 0;
    }
    return MKBADDR(mem);
}

static LONG g_freevec(LONG *a, ULONG *gv)
{
    (void)gv;
    if (a[0])
        FreeVec(BP(a[0]));
    return 0;
}

/* allocMem(upb, requirements): cleared, with four longs to spare */
static LONG g_allocMem(LONG *a, ULONG *gv)
{
    (void)gv;
    return MKBADDR(AllocVec((a[0] + 4) << 2, a[1] | MEMF_CLEAR));
}

/* ------------------------------------------------------------- arithmetic */

static LONG g_multiply(LONG *a, ULONG *gv)  { (void)gv; return a[0] * a[1]; }
static LONG g_divide(LONG *a, ULONG *gv)    { (void)gv; return a[0] / a[1]; }
static LONG g_mod(LONG *a, ULONG *gv)       { (void)gv; return a[0] % a[1]; }

/* level(): the caller's frame */
static LONG g_level(LONG *a, ULONG *gv)
{
    (void)gv;
    return a[-3];
}

/* --------------------------------------------------------- character I/O */

static LONG g_rdch(LONG *a, ULONG *gv)    { (void)a; (void)gv; return FGetC(Input()); }
static LONG g_unrdch(LONG *a, ULONG *gv)  { (void)a; (void)gv; return UnGetC(Input(), -1); }
static LONG g_wrch(LONG *a, ULONG *gv)    { (void)gv; return FPutC(Output(), a[0]); }

static LONG g_newline(LONG *a, ULONG *gv)
{
    put_ch(gv, a, '\n');
    return 0;
}

/* writet(s, width): the string, padded with blanks to width */
static LONG g_writet(LONG *a, ULONG *gv)
{
    UBYTE *s = BP(a[0]);
    LONG i, len = s ? s[0] : 0;

    for (i = 1; i <= len; i++)
        put_ch(gv, a, s[i]);
    for (i = a[1] - len; i > 0; i--)
        put_ch(gv, a, ' ');
    return 0;
}

static LONG g_writes(LONG *a, ULONG *gv)
{
    return call_global(gv, a, OFF_WRITET, a[0], 0);
}

static LONG g_writei(LONG *a, ULONG *gv)    { put_number(gv, a, a[0], a[1], 10); return 0; }
static LONG g_writen(LONG *a, ULONG *gv)    { put_number(gv, a, a[0], 0, 10); return 0; }
static LONG g_writehex(LONG *a, ULONG *gv)  { put_number(gv, a, a[0], a[1], 16); return 0; }
static LONG g_writeoct(LONG *a, ULONG *gv)  { put_number(gv, a, a[0], a[1], 8); return 0; }

/*
 * writef(format, arg1, ...): %S string, %Tn string in a field of n,
 * %C character, %In/%N number, %Xn hex, %On octal (n a digit 0-9/A-Z),
 * %% a percent sign; another character after % is written in upper case.
 */
static LONG g_writef(LONG *a, ULONG *gv)
{
    const UBYTE *f = BP(a[0]);
    LONG len = f[0], i = 1, next = 1;

    while (i <= len) {
        UBYTE c = f[i++], k;
        LONG w = 0;

        if (c != '%' || i > len) {
            put_ch(gv, a, c);
            continue;
        }
        c = f[i++];
        k = up(c);
        if ((k == 'T' || k == 'I' || k == 'X' || k == 'O') && i <= len)
            w = width_digit(f[i++]);
        switch (k) {
        case 'S': call_global(gv, a, OFF_WRITES, a[next++], 0); break;
        case 'T': call_global(gv, a, OFF_WRITET, a[next++], w); break;
        case 'C': put_ch(gv, a, a[next++]); break;
        case 'I': call_global(gv, a, OFF_WRITEI, a[next++], w); break;
        case 'N': call_global(gv, a, OFF_WRITEI, a[next++], 0); break;
        case 'X': call_global(gv, a, OFF_WRITEHEX, a[next++], w); break;
        case 'O': call_global(gv, a, OFF_WRITEOCT, a[next++], w); break;
        case '%': put_ch(gv, a, '%'); break;
        default:  put_ch(gv, a, k); break;
        }
    }
    return 0;
}

/* readn(): a decimal number from the input; IoErr() is -1 without digits */
static LONG g_readn(LONG *a, ULONG *gv)
{
    BPTR in = Input();
    LONG c, value = 0;
    BOOL minus = FALSE, digits = FALSE;

    (void)a; (void)gv;
    do
        c = FGetC(in);
    while (c == ' ' || c == '\t' || c == '\n');
    if (c == '-' || c == '+') {
        minus = c == '-';
        c = FGetC(in);
    }
    while (c >= '0' && c <= '9') {
        value = value * 10 + (c - '0');
        digits = TRUE;
        c = FGetC(in);
    }
    UnGetC(in, -1);
    SetIoErr(digits ? 0 : -1);
    return minus ? -value : value;
}

/* readinput(buffer, bytes) / writeoutput(buffer, bytes): byte addresses */
static LONG g_readinput(LONG *a, ULONG *gv)
{
    (void)gv;
    return Read(Input(), (APTR)a[0], a[1]);
}

static LONG g_writeoutput(LONG *a, ULONG *gv)
{
    (void)gv;
    return Write(Output(), (APTR)a[0], a[1]);
}

/* the same with a BPTR and a size in longs */
static LONG g_readinput_long(LONG *a, ULONG *gv)
{
    (void)gv;
    return Read(Input(), BP(a[0]), a[1] << 2) >> 2;
}

static LONG g_writeoutput_long(LONG *a, ULONG *gv)
{
    (void)gv;
    return FWrite(Output(), BP(a[0]), 4, a[1]);
}

static LONG g_findinput(LONG *a, ULONG *gv)
{
    char name[256];

    (void)gv;
    return Open(cstr(a[0], name, sizeof(name)), MODE_OLDFILE);
}

static LONG g_findoutput(LONG *a, ULONG *gv)
{
    char name[256];

    (void)gv;
    return Open(cstr(a[0], name, sizeof(name)), MODE_NEWFILE);
}

static LONG g_endread(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    Close(Input());
    SelectInput(0);
    return 0;
}

static LONG g_endwrite(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    Close(Output());
    SelectOutput(0);
    return 0;
}

/* -------------------------------------------------- arguments and items */

/* the modifiers of template item 'n' contain one of 'flags'? */
static BOOL item_has(CONST_STRPTR t, LONG n, const char *flags)
{
    BOOL mods = FALSE;

    for (; *t && n > 0; t++)
        if (*t == ',')
            n--;
    for (; *t && *t != ','; t++) {
        const char *f;
        if (*t == '/') {
            mods = TRUE;
            continue;
        }
        if (!mods)
            continue;
        for (f = flags; *f; f++)
            if (up(*t) == (UBYTE)*f)
                return TRUE;
    }
    return FALSE;
}

/*
 * rdargs(template, vector, size): parses the command line like ReadArgs();
 * switches and numbers are values, every string is a BSTR stored in the
 * vector behind the argument slots.  Returns the number of longs used, 0
 * on an error.
 */
static LONG g_rdargs(LONG *a, ULONG *gv)
{
    char tmpl[256];
    LONG *vec = BP(a[1]);
    LONG size = a[2], args = 1, used, i;
    struct RDArgs *rd;

    (void)gv;
    cstr(a[0], tmpl, sizeof(tmpl));
    for (i = 0; tmpl[i]; i++)
        if (tmpl[i] == ',')
            args++;
    for (i = 0; i < size; i++)
        vec[i] = 0;

    rd = ReadArgs((STRPTR)tmpl, vec, NULL);
    if (!rd)
        return 0;

    used = args;
    for (i = 0; i < args; i++) {
        const char *s;
        UBYTE *b;
        LONG len, k;

        if (!vec[i] || item_has((STRPTR)tmpl, i, "ST"))
            continue;
        if (item_has((STRPTR)tmpl, i, "N")) {
            vec[i] = *(LONG *)vec[i];
            continue;
        }
        if (item_has((STRPTR)tmpl, i, "M")) {
            vec[i] = 0;
            continue;
        }
        s = (const char *)vec[i];
        len = strlen(s);
        if (len + 1 > (size - used) * 4) {
            FreeArgs(rd);
            SetIoErr(ERROR_NO_FREE_STORE);
            return 0;
        }
        b = (UBYTE *)&vec[used];
        b[0] = len;
        for (k = 0; k < len; k++)
            b[k + 1] = s[k];
        vec[i] = MKBADDR(b);
        used += (len + 4) / 4;
    }
    FreeArgs(rd);
    SetIoErr(0);
    return used;
}

/* rditem(buffer, upb): the next item of the command line as a BSTR */
static LONG g_rditem(LONG *a, ULONG *gv)
{
    UBYTE *b = BP(a[0]);
    LONG r;

    (void)gv;
    b[1] = 0;
    r = ReadItem((STRPTR)b + 1, (a[1] << 2) - 1, NULL);
    b[0] = strlen((const char *)b + 1);
    return r;
}

/* subString / findarg(template, keyword) */
static LONG g_findarg(LONG *a, ULONG *gv)
{
    char tmpl[256], key[256];

    (void)gv;
    return FindArg((STRPTR)cstr(a[0], tmpl, sizeof(tmpl)), (STRPTR)cstr(a[1], key, sizeof(key)));
}

/* extractDeviceName(buffer, separator, name, position) */
static LONG g_splitname(LONG *a, ULONG *gv)
{
    char name[256];
    UBYTE *b = BP(a[0]);
    LONG pos;

    (void)gv;
    pos = SplitName((STRPTR)cstr(a[2], name, sizeof(name)), a[1], (STRPTR)b + 1, a[3] - 1, 30);
    b[0] = strlen((const char *)b + 1);
    return pos + 1;
}

/* -------------------------------------------------------- files and locks */

static LONG g_load(LONG *a, ULONG *gv)
{
    char name[256];

    (void)gv;
    return LoadSeg(cstr(a[0], name, sizeof(name)));
}

static LONG g_deleteObject(LONG *a, ULONG *gv)
{
    char name[256];

    (void)gv;
    return DeleteFile(cstr(a[0], name, sizeof(name)));
}

static LONG g_rename(LONG *a, ULONG *gv)
{
    char from[256], to[256];

    (void)gv;
    return Rename(cstr(a[0], from, sizeof(from)), cstr(a[1], to, sizeof(to)));
}

static LONG g_lock(LONG *a, ULONG *gv)
{
    char name[256];

    (void)gv;
    return Lock(cstr(a[0], name, sizeof(name)), SHARED_LOCK);
}

static LONG g_createDir(LONG *a, ULONG *gv)
{
    char name[256];

    (void)gv;
    return CreateDir(cstr(a[0], name, sizeof(name)));
}

static LONG g_deviceproc(LONG *a, ULONG *gv)
{
    char name[256];

    (void)gv;
    return (LONG)DeviceProc(cstr(a[0], name, sizeof(name)));
}

/* setCurrentDir(set, lock): returns the current directory, changes it
 * when set */
static LONG g_setCurrentDir(LONG *a, ULONG *gv)
{
    BPTR old = self()->pr_CurrentDir;

    (void)gv;
    if (a[0])
        CurrentDir(a[1]);
    return old;
}

/* findDevInfo(name): the DOS list entry of a device, volume or assign */
static LONG g_findDevInfo(LONG *a, ULONG *gv)
{
    char name[256];
    struct DosList *dl;

    (void)gv;
    cstr(a[0], name, sizeof(name));
    dl = LockDosList(LDF_ALL | LDF_READ);
    dl = FindDosEntry(dl, (STRPTR)name, LDF_ALL | LDF_READ);
    UnLockDosList(LDF_ALL | LDF_READ);
    return MKBADDR(dl);
}

/* addDevice(name): a new device entry */
static LONG g_addDevice(LONG *a, ULONG *gv)
{
    char name[256];
    struct DosList *dl;

    (void)gv;
    dl = MakeDosEntry((STRPTR)cstr(a[0], name, sizeof(name)), DLT_DEVICE);
    if (!dl)
        return 0;
    if (!AddDosEntry(dl)) {
        FreeDosEntry(dl);
        return 0;
    }
    return MKBADDR(dl);
}

static LONG g_datestamp(LONG *a, ULONG *gv)
{
    (void)gv;
    DateStamp((struct DateStamp *)BP(a[0]));
    return a[0];
}

/* settime(datestamp): set the system clock */
static LONG g_settime(LONG *a, ULONG *gv)
{
    struct DateStamp *ds = BP(a[0]);
    struct MsgPort *port;
    struct timerequest *tr = NULL;
    LONG ok = DOSFALSE;

    (void)gv;
    port = CreateMsgPort();
    if (port)
        tr = (struct timerequest *)CreateIORequest(port, sizeof(*tr));
    if (tr && !OpenDevice((STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0)) {
        tr->tr_node.io_Command = TR_SETSYSTIME;
        tr->tr_time.tv_secs = (ds->ds_Days * 24 * 60 + ds->ds_Minute) * 60
                              + ds->ds_Tick / TICKS_PER_SECOND;
        tr->tr_time.tv_micro = (ds->ds_Tick % TICKS_PER_SECOND) * (1000000 / TICKS_PER_SECOND);
        if (!DoIO((struct IORequest *)tr))
            ok = DOSTRUE;
        CloseDevice((struct IORequest *)tr);
    }
    if (tr)
        DeleteIORequest((struct IORequest *)tr);
    if (port)
        DeleteMsgPort(port);
    return ok;
}

/* ----------------------------------------------------- processes and I/O */

static LONG g_findtask(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    return (LONG)&self()->pr_MsgPort;
}

static LONG g_findSegArray(LONG *a, ULONG *gv)        { (void)a; (void)gv; return self()->pr_SegList; }
static LONG g_findConsoleHandler(LONG *a, ULONG *gv)  { (void)a; (void)gv; return (LONG)self()->pr_ConsoleTask; }
static LONG g_findFileHandler(LONG *a, ULONG *gv)     { (void)a; (void)gv; return (LONG)self()->pr_FileSystemTask; }
static LONG g_findRootNode(LONG *a, ULONG *gv)        { (void)a; (void)gv; return MKBADDR(DOSBase->dl_Root); }
static LONG g_findCLI(LONG *a, ULONG *gv)             { (void)a; (void)gv; return self()->pr_CLI; }

/* Res2(set, code): IoErr(), set first if asked */
static LONG g_Res2(LONG *a, ULONG *gv)
{
    (void)gv;
    if (a[0])
        SetIoErr(a[1]);
    return IoErr();
}

/* installSeg(seg): its globals into the caller's vector */
static LONG g_installSeg(LONG *a, ULONG *gv)
{
    lxa_bcpl_install(a[0], gv);
    return DOSTRUE;
}

/* createProcBCPL(segarray, stack in longs, priority, name) */
static LONG g_createProcBCPL(LONG *a, ULONG *gv)
{
    char name[256];

    (void)gv;
    return (LONG)lxa_bcpl_create_proc((STRPTR)cstr(a[3], name, sizeof(name)), BP(a[0]),
                                      a[1] << 2, a[2]);
}

/* setSignals(port, mask) / clearSignals(mask): the BCPL signals are the
 * task signals 12-15 */
static LONG g_setSignals(LONG *a, ULONG *gv)
{
    struct MsgPort *port = (struct MsgPort *)a[0];

    (void)gv;
    Signal(port->mp_SigTask, (a[1] << 12) & 0xf000);
    return 0;
}

static LONG g_clearSignals(LONG *a, ULONG *gv)
{
    ULONG mask = (a[0] << 12) & 0xf000;

    (void)gv;
    return (SetSignal(0, mask) & mask) ? DOSTRUE : DOSFALSE;
}

static LONG g_DOSAlert(LONG *a, ULONG *gv)
{
    (void)gv;
    Alert(a[0]);
    return 0;
}

/* taskwait(): the next packet, as a BPTR */
static LONG g_taskwait(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    return MKBADDR(WaitPkt());
}

/* putPkt(packet): send it; dp_Link is an empty message to use */
static LONG g_putPkt(LONG *a, ULONG *gv)
{
    struct DosPacket *dp = BP(a[0]);
    struct Message *msg = dp->dp_Link;

    (void)gv;
    msg->mn_Node.ln_Name = (char *)dp;
    msg->mn_Length = sizeof(*msg);
    PutMsg(dp->dp_Port, msg);
    return DOSTRUE;
}

/* sendPacket(-, port, type, -, -, arg1 ... arg7): send and wait */
static LONG g_sendPacket(LONG *a, ULONG *gv)
{
    struct StandardPacket *sp = AllocVec(sizeof(*sp), MEMF_PUBLIC | MEMF_CLEAR);
    LONG res1;

    (void)gv;
    if (!sp) {
        SetIoErr(ERROR_NO_FREE_STORE);
        return DOSFALSE;
    }
    sp->sp_Msg.mn_Node.ln_Name = (char *)&sp->sp_Pkt;
    sp->sp_Pkt.dp_Link = &sp->sp_Msg;
    sp->sp_Pkt.dp_Type = a[2];
    sp->sp_Pkt.dp_Arg1 = a[5];
    sp->sp_Pkt.dp_Arg2 = a[6];
    sp->sp_Pkt.dp_Arg3 = a[7];
    sp->sp_Pkt.dp_Arg4 = a[8];
    sp->sp_Pkt.dp_Arg5 = a[9];
    sp->sp_Pkt.dp_Arg6 = a[10];
    sp->sp_Pkt.dp_Arg7 = a[11];
    SendPkt(&sp->sp_Pkt, (struct MsgPort *)a[1], &self()->pr_MsgPort);
    WaitPkt();
    res1 = sp->sp_Pkt.dp_Res1;
    SetIoErr(sp->sp_Pkt.dp_Res2);
    FreeVec(sp);
    return res1;
}

static LONG g_returnPacket(LONG *a, ULONG *gv)
{
    (void)gv;
    if (a[0])
        ReplyPkt(BP(a[0]), a[1], a[2]);
    return 0;
}

/* setIO(ioreq, command, data, length, offset) */
static LONG g_setIO(LONG *a, ULONG *gv)
{
    struct IOStdReq *io = BP(a[0]);

    (void)gv;
    io->io_Command = a[1];
    io->io_Data = BP(a[2]);
    io->io_Length = a[3];
    io->io_Offset = a[4];
    return 0;
}

static LONG g_doIO(LONG *a, ULONG *gv)
{
    (void)gv;
    return (LONG)(BYTE)DoIO((struct IORequest *)BP(a[0]));
}

static LONG g_sendIO(LONG *a, ULONG *gv)
{
    struct IORequest *io = BP(a[0]);

    (void)gv;
    io->io_Message.mn_Node.ln_Name = NULL;      /* not a packet */
    SendIO(io);
    return 0;
}

/* openDevice(ioreq, name, unit, flags): replies go to the process port */
static LONG g_openDevice(LONG *a, ULONG *gv)
{
    struct IORequest *io = BP(a[0]);
    char name[256];

    (void)gv;
    io->io_Message.mn_ReplyPort = &self()->pr_MsgPort;
    return OpenDevice((STRPTR)cstr(a[1], name, sizeof(name)), a[2], io, a[3]) ? DOSFALSE : DOSTRUE;
}

static LONG g_closeDevice(LONG *a, ULONG *gv)
{
    (void)gv;
    CloseDevice((struct IORequest *)BP(a[0]));
    return 0;
}

/* timerIO(command, timerequest, seconds, microseconds) */
static LONG g_timerIO(LONG *a, ULONG *gv)
{
    struct timerequest *tr = BP(a[1]);

    (void)gv;
    tr->tr_node.io_Command = a[0];
    tr->tr_time.tv_secs = a[2];
    tr->tr_time.tv_micro = a[3];
    DoIO((struct IORequest *)tr);
    return DOSTRUE;
}

/* execLib(offset, d0, d1, a0, a1, a2) / libcall(base, offset, d0, d1, a0, a1) */
static LONG g_execLib(LONG *a, ULONG *gv)
{
    (void)gv;
    return lxa_bcpl_lvo(SysBase, (LONG)(WORD)a[0], &a[1]);
}

static LONG g_libcall(LONG *a, ULONG *gv)
{
    LONG regs[5];

    regs[0] = a[2];
    regs[1] = a[3];
    regs[2] = a[4];
    regs[3] = a[5];
    regs[4] = (LONG)gv;                         /* a2 stays the global vector */
    return lxa_bcpl_lvo((APTR)a[0], a[1], regs);
}

/* runCommand(seg, stack in longs, command line, length) */
static LONG g_runCommand(LONG *a, ULONG *gv)
{
    (void)gv;
    return RunCommand(a[0], (a[1] + 1) << 2, (STRPTR)BP(a[2]), a[3]);
}

/* writeErr(code, command name): the fault text of code */
static LONG g_writeErr(LONG *a, ULONG *gv)
{
    (void)gv;
    if (!a[0])
        return DOSTRUE;
    return PrintFault(a[0], NULL) ? DOSTRUE : DOSFALSE;
}

/* makesysreq(type, arg, port): ErrorReport() for IoErr() */
static LONG g_makesysreq(LONG *a, ULONG *gv)
{
    (void)gv;
    return ErrorReport(IoErr(), a[0], a[1], (struct MsgPort *)a[2]);
}

/* openWindow(left, top, width, height, title) */
static LONG g_openWindow(LONG *a, ULONG *gv)
{
    struct TagItem tags[8];

    (void)gv;
    if (!IntuitionBase)
        return 0;
    tags[0].ti_Tag = WA_Left;       tags[0].ti_Data = a[0];
    tags[1].ti_Tag = WA_Top;        tags[1].ti_Data = a[1];
    tags[2].ti_Tag = WA_Width;      tags[2].ti_Data = a[2];
    tags[3].ti_Tag = WA_Height;     tags[3].ti_Data = a[3];
    tags[4].ti_Tag = WA_Title;      tags[4].ti_Data = a[4];
    tags[5].ti_Tag = WA_PubScreen;  tags[5].ti_Data = 0;
    tags[6].ti_Tag = WA_AutoAdjust; tags[6].ti_Data = TRUE;
    tags[7].ti_Tag = TAG_END;       tags[7].ti_Data = 0;
    return (LONG)OpenWindowTagList(NULL, tags);
}

/* CliInit(packet): the CLI set-up of a NewCLI (dp_Res1 set) or Run */
static LONG g_CliInit(LONG *a, ULONG *gv)
{
    struct DosPacket *dp = BP(a[0]);

    (void)gv;
    if (!dp)
        return 0;
    return dp->dp_Res1 ? CliInitNewcli(dp) : CliInitRun(dp);
}

/* ------------------------------------------------- not implemented (yet) */

static LONG g_sysRequest(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL sysRequest", "stub: BCPL system requester (GV -0x84)");
    return 0;
}

static LONG g_makeGVarea(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL makeGVarea", "stub: BCPL makeGVarea (GV 0x34)");
    return 0;
}

static LONG g_longjump(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL longjump", "stub: BCPL longjump (GV 0x50)");
    return 0;
}

static LONG g_createco(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL createco", "stub: BCPL coroutines (GV 0x5c)");
    return 0;
}

static LONG g_deleteco(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL deleteco", "stub: BCPL coroutines (GV 0x60)");
    return 0;
}

static LONG g_callco(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL callco", "stub: BCPL coroutines (GV 0x64)");
    return 0;
}

static LONG g_cowait(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL cowait", "stub: BCPL coroutines (GV 0x68)");
    return 0;
}

static LONG g_resumeco(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL resumeco", "stub: BCPL coroutines (GV 0x6c)");
    return 0;
}

static LONG g_holdTask(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL holdTask", "stub: BCPL holdTask (GV 0xb8)");
    return 0;
}

static LONG g_systemRequest(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL systemRequest", "stub: BCPL system requester (GV 0xd0)");
    return 0;
}

static LONG g_tidyup(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL tidyup", "stub: BCPL tidyup (GV 0x150)");
    return 0;
}

static LONG g_openDevInfo(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL openDevInfo", "stub: BCPL handler start-up (GV 0x1c0)");
    return 0;
}

static LONG g_compareTime(LONG *a, ULONG *gv)
{
    (void)a; (void)gv;
    LXA_UNIMPLEMENTED("dos", "BCPL compareTime", "stub: BCPL compareTime (GV 0x1f8)");
    return 0;
}

/* ------------------------------------------------------------- the table */

static const bcpl_global g_globals[LXA_BCPL_SLOTS] = {
    [SLOT(-0x84)] = g_sysRequest,
    [SLOT(-0x80)] = g_toCStr,
    [SLOT(-0x7c)] = g_toBSTR,
    [SLOT(-0x6c)] = g_Execute,
    [SLOT(-0x68)] = g_IsInteractive,
    [SLOT(-0x64)] = g_DateStamp,
    [SLOT(-0x60)] = g_SetProtection,
    [SLOT(-0x5c)] = g_SetComment,
    [SLOT(-0x54)] = g_NoReqLoadSeg,
    [SLOT(-0x50)] = g_clearvec,
    [SLOT(-0x4c)] = g_LoadSeg,
    [SLOT(-0x48)] = g_CreateProc,
    [SLOT(-0x44)] = g_IoErr,
    [SLOT(-0x40)] = g_CurrentDir,
    [SLOT(-0x3c)] = g_CreateDir,
    [SLOT(-0x38)] = g_Info,
    [SLOT(-0x34)] = g_ExNext,
    [SLOT(-0x30)] = g_Examine,
    [SLOT(-0x2c)] = g_Lock,
    [SLOT(-0x28)] = g_Rename,
    [SLOT(-0x24)] = g_DeleteFile,
    [SLOT(-0x20)] = g_Seek,
    [SLOT(-0x18)] = g_Write,
    [SLOT(-0x0c)] = g_Read,
    [SLOT(-0x08)] = g_copyvec,
    [SLOT(-0x04)] = g_Open,
    [SLOT(0x00c)] = g_multiply,
    [SLOT(0x010)] = g_divide,
    [SLOT(0x014)] = g_mod,
    [SLOT(0x018)] = g_setIO,
    [SLOT(0x020)] = g_packlw,
    [SLOT(0x024)] = g_unpacklw,
    [SLOT(0x028)] = g_Res2,
    [SLOT(0x034)] = g_makeGVarea,
    [SLOT(0x038)] = g_findtask,
    [SLOT(0x03c)] = g_getbyte,
    [SLOT(0x040)] = g_putbyte,
    [SLOT(0x044)] = g_level,
    [SLOT(0x04c)] = g_allocMem,
    [SLOT(0x050)] = g_longjump,
    [SLOT(0x054)] = g_doIO,
    [SLOT(0x058)] = g_sendIO,
    [SLOT(0x05c)] = g_createco,
    [SLOT(0x060)] = g_deleteco,
    [SLOT(0x064)] = g_callco,
    [SLOT(0x068)] = g_cowait,
    [SLOT(0x06c)] = g_resumeco,
    [SLOT(0x070)] = g_installSeg,
    [SLOT(0x074)] = g_getvec,
    [SLOT(0x078)] = g_freevec,
    [SLOT(0x07c)] = g_openDevice,
    [SLOT(0x080)] = g_closeDevice,
    [SLOT(0x084)] = g_createProcBCPL,
    [SLOT(0x08c)] = g_ParentDir,
    [SLOT(0x090)] = g_setSignals,
    [SLOT(0x094)] = g_clearSignals,
    [SLOT(0x098)] = g_DOSAlert,
    [SLOT(0x09c)] = g_findRootNode,
    [SLOT(0x0a0)] = g_readinput,
    [SLOT(0x0a4)] = g_taskwait,
    [SLOT(0x0a8)] = g_putPkt,
    [SLOT(0x0ac)] = g_writeoutput,
    [SLOT(0x0b0)] = g_packstring,
    [SLOT(0x0b4)] = g_unpackstring,
    [SLOT(0x0b8)] = g_holdTask,
    [SLOT(0x0bc)] = g_Delay,
    [SLOT(0x0c0)] = g_sendPacket,
    [SLOT(0x0c4)] = g_returnPacket,
    [SLOT(0x0c8)] = g_openWindow,
    [SLOT(0x0cc)] = g_setCurrentDir,
    [SLOT(0x0d0)] = g_systemRequest,
    [SLOT(0x0d4)] = g_writet,
    [SLOT(0x0d8)] = g_rdch,
    [SLOT(0x0dc)] = g_unrdch,
    [SLOT(0x0e0)] = g_wrch,
    [SLOT(0x0e4)] = g_readinput_long,
    [SLOT(0x0e8)] = g_writeoutput_long,
    [SLOT(0x0ec)] = g_findinput,
    [SLOT(0x0f0)] = g_findoutput,
    [SLOT(0x0f4)] = g_SelectInput,
    [SLOT(0x0f8)] = g_SelectOutput,
    [SLOT(0x0fc)] = g_endread,
    [SLOT(0x100)] = g_endwrite,
    [SLOT(0x104)] = g_Input,
    [SLOT(0x108)] = g_Output,
    [SLOT(0x10c)] = g_readn,
    [SLOT(0x110)] = g_newline,
    [SLOT(0x114)] = g_writei,
    [SLOT(0x118)] = g_writen,
    [SLOT(0x11c)] = g_writehex,
    [SLOT(0x120)] = g_writeoct,
    [SLOT(0x124)] = g_writes,
    [SLOT(0x128)] = g_writef,
    [SLOT(0x12c)] = g_toupper,
    [SLOT(0x130)] = g_charicmp,
    [SLOT(0x134)] = g_stricmp,
    [SLOT(0x138)] = g_rdargs,
    [SLOT(0x13c)] = g_rditem,
    [SLOT(0x140)] = g_findarg,
    [SLOT(0x144)] = g_load,
    [SLOT(0x148)] = g_UnLoadSeg,
    [SLOT(0x150)] = g_tidyup,
    [SLOT(0x154)] = g_addDevice,
    [SLOT(0x158)] = g_datestamp,
    [SLOT(0x15c)] = g_WaitForChar,
    [SLOT(0x160)] = g_execLib,
    [SLOT(0x164)] = g_findSegArray,
    [SLOT(0x168)] = g_deleteObject,
    [SLOT(0x16c)] = g_rename,
    [SLOT(0x174)] = g_Close,
    [SLOT(0x178)] = g_getword,
    [SLOT(0x17c)] = g_putword,
    [SLOT(0x190)] = g_taskwait,
    [SLOT(0x194)] = g_Execute,
    [SLOT(0x198)] = g_deviceproc,
    [SLOT(0x19c)] = g_libcall,
    [SLOT(0x1a0)] = g_writeErr,
    [SLOT(0x1a4)] = g_findConsoleHandler,
    [SLOT(0x1a8)] = g_findFileHandler,
    [SLOT(0x1ac)] = g_splitname,
    [SLOT(0x1b0)] = g_lock,
    [SLOT(0x1b4)] = g_UnLock,
    [SLOT(0x1b8)] = g_getlong,
    [SLOT(0x1bc)] = g_putlong,
    [SLOT(0x1c0)] = g_openDevInfo,
    [SLOT(0x1c4)] = g_DupLock,
    [SLOT(0x1c8)] = g_makesysreq,
    [SLOT(0x1cc)] = g_strcpy,
    [SLOT(0x1e4)] = g_runCommand,
    [SLOT(0x1ec)] = g_lock,
    [SLOT(0x1f0)] = g_findDevInfo,
    [SLOT(0x1f4)] = g_createDir,
    [SLOT(0x1f8)] = g_compareTime,
    [SLOT(0x1fc)] = g_timerIO,
    [SLOT(0x200)] = g_settime,
    [SLOT(0x214)] = g_CliInit,
    [SLOT(0x218)] = g_findCLI,
};

LONG lxa_bcpl_dispatch(LONG off, ULONG *gv, LONG *args)
{
    bcpl_global fn = g_globals[SLOT(off)];

    return fn ? fn(args, gv) : 0;
}

/* ------------------------------------------------- global vector set-up */

/* the system's value for global vector offset 'off' */
static ULONG system_global(LONG off)
{
    switch (off) {
    case OFF_STOP:
    case OFF_REMPROCESS:
        return (ULONG)lxa_bcpl_g_stop;
    case OFF_LONGCALL:
        return (ULONG)lxa_bcpl_g_longcall;
    case OFF_DOSBASE:
        return (ULONG)DOSBase;
    }
    return g_globals[SLOT(off)] ? (ULONG)stub_of(off) : 0;
}

/* fill in the system globals; slot 0 (size) and 1 (start) stay */
static void install_system(ULONG *gv)
{
    LONG off, top = gv[0] < LXA_BCPL_POS_SLOTS ? (LONG)gv[0] : LXA_BCPL_POS_SLOTS;

    for (off = -LXA_BCPL_NEG_BYTES; off < 0; off += 4)
        gv[GVI(off)] = system_global(off);
    for (off = 2 * 4; off < top * 4; off += 4) {
        ULONG v = system_global(off);
        if (v)
            gv[GVI(off)] = v;
    }
}

/* the global table of a BCPL hunk, NULL for other hunks: the hunk's first
 * long is its length in longs, its last long the highest global */
static ULONG *global_table(ULONG *segment)
{
    ULONG n = segment[1];

    if (n < 2 || n * 4 + 8 > segment[-1])
        return NULL;
    return &segment[n];
}

ULONG lxa_bcpl_highest_global(BPTR seg)
{
    ULONG highest = 0;

    if (seg == 0)
        return 0;
    if (U_isSystemSegment(seg))
        return LXA_BCPL_POS_SLOTS - 1;
    for (; seg; seg = *(BPTR *)BADDR(seg)) {
        ULONG *t = global_table(BADDR(seg));
        if (t && t[0] > highest)
            highest = t[0];
    }
    return highest;
}

/* Install a segment list: the globals its BCPL hunks define, or all the
 * system globals for the system segments of pr_SegList ([1], [2]) */
void lxa_bcpl_install(BPTR seg, ULONG *gv)
{
    if (seg == 0)
        return;
    if (U_isSystemSegment(seg)) {
        install_system(gv);
        return;
    }
    for (; seg; seg = *(BPTR *)BADDR(seg)) {
        ULONG *segment = BADDR(seg);
        ULONG *t = global_table(segment);

        if (!t)
            continue;
        /* (global, offset) pairs below the highest global, up to a 0 */
        for (t--; t[0] != 0; t -= 2)
            gv[t[-1]] = (ULONG)&segment[1] + t[0];
    }
}

/* dos.library start-up: the system global vector dl_GV, the call/return
 * routines and the console segment */
BOOL lxa_bcpl_init(struct DosLibrary *dosbase)
{
    UBYTE *mem = AllocMem(LXA_BCPL_SLOTS * 4, MEMF_PUBLIC | MEMF_CLEAR);
    ULONG *gv;

    if (!mem)
        return FALSE;
    gv = (ULONG *)(mem + LXA_BCPL_NEG_BYTES);
    gv[0] = LXA_BCPL_POS_SLOTS;
    install_system(gv);
    gv[GVI(OFF_DOSBASE)] = (ULONG)dosbase;

    dosbase->dl_GV = gv;
    dosbase->dl_A5 = (LONG)lxa_bcpl_jsr;
    dosbase->dl_A6 = (LONG)lxa_bcpl_rts;
    if (dosbase->dl_Root)
        ((struct RootNode *)dosbase->dl_Root)->rn_ConsoleSegment = MKBADDR(lxa_bcpl_cliseg);
    return TRUE;
}
