#!/usr/bin/env python
# -*- coding: utf-8 -*-

import codecs
import sys

INPUTFN = "exec_mini.h"
OUT_STUBS_FN = "exec_stubs.c"
OUT_FUNCTABLE_FN = "exec_fns.c"

LIBS = { 'EXEC_BASE_NAME': ('_exec', 'struct ExecBase *')  }

MACROS = {

    #   LPX - functions that take X arguments.
    #
    #   Modifiers (variations are possible):
    #   NR - no return (void),
    #   A4, A5 - "a4" or "a5" is used as one of the arguments,
    #   NB - base will be given explicitly by user (see cia.resource).
    #   FP - one of the parameters has type "pointer to function".
    #   FR - the return type is a "pointer to function".
    #
    #   "bt" arguments are not used - they are provided for backward compatibility
    #   only.


    #            num_args,    rt, num_fp,    fr
    #define LP0(offs, rt, name, bt, bn)
    'LP0'    : (        0,  True,      0, False),
    #define LP0FR(offs, rt, name, bt, bn, fpr)
    #define LP0NR(offs, name, bt, bn)
    'LP0NR'  : (        0, False,      0, False),
    #define LP1(offs, rt, name, t1, v1, r1, bt, bn)
    'LP1'    : (        1,  True,      0, False),
    #define LP1FP(offs, rt, name, t1, v1, r1, bt, bn, fpt)
    'LP1FP'  : (        1,  True,      1, False),
    #define LP1FR(offs, rt, name, t1, v1, r1, bt, bn, fpr)
    #define LP1FPFR(offs, rt, name, t1, v1, r1, bt, bn, fpt, fpr)
    #define LP1NR(offs, name, t1, v1, r1, bt, bn)
    'LP1NR'  : (        1, False,      0, False),
    #define LP1A5(offs, rt, name, t1, v1, r1, bt, bn)
    #define LP1NRA5(offs, name, t1, v1, r1, bt, bn)
    #define LP1A5FP(offs, rt, name, t1, v1, r1, bt, bn, fpt)
    'LP1A5FP': (        1,  True,      1, False),
    #define LP1NRFP(offs, name, t1, v1, r1, bt, bn, fpt)
    #define LP2(offs, rt, name, t1, v1, r1, t2, v2, r2, bt, bn)
    'LP2'    : (        2,  True,      0, False),
    #define LP2NR(offs, name, t1, v1, r1, t2, v2, r2, bt, bn)
    'LP2NR'  : (        2, False,      0, False),
    #define LP2NB(offs, rt, name, t1, v1, r1, t2, v2, r2)
    #define LP2FP(offs, rt, name, t1, v1, r1, t2, v2, r2, bt, bn, fpt)
    'LP2FP'  : (        2,  True,      1, False),
    #define LP2FPFR(offs, rt, name, t1, v1, r1, t2, v2, r2, bt, bn, fpt, fpr)
    #define LP2NRFP(offs, name, t1, v1, r1, t2, v2, r2, bt, bn, fpt)
    #define LP3(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, bt, bn)
    'LP3'    : (        3,  True,      0, False),
    #define LP3NR(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, bt, bn)
    'LP3NR'  : (        3, False,      0, False),
    #define LP3NB(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3)
    #define LP3NRNB(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3)
    #define LP3FP(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, bt, bn, fpt)
    'LP3FP'  : (        3,  True,      1, False),
    #define LP3FP2(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, bt, bn, fpt1, fpt2)
    #define LP3FP3(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, bt, bn, fpt1, fpt2, fpt3)
    #define LP3NRFP(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, bt, bn, fpt)
    #define LP3NRFP2(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, bt, bn, fpt1, fpt2)
    #define LP3NRFP3(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, bt, bn, fpt1, fpt2, fpt3)
    #define LP4(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, bt, bn)
    'LP4'    : (        4,  True,      0, False),
    #define LP4NR(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, bt, bn)
    'LP4NR'  : (        4, False,      0, False),
    #define LP4NRFP3(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, bt, bn, fpt1, fpt2, fpt3)
    #define LP4FP(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, bt, bn, fpt)
    'LP4FP'  : (        4,  True,      1, False),
    #define LP4FP4(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, bt, bn, fpt1, fpt2, fpt3, fpt4)
    #define LP5(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, bt, bn)
    'LP5'    : (        5,  True,      0, False),
    #define LP5NR(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, bt, bn)
    'LP5NR'  : (        5, False,      0, False),
    #define LP5NRA4(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, bt, bn)
    #define LP5NRA5(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, bt, bn)
    #define LP5FP(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, bt, bn, fpt)
    'LP5FP'  : (        5,  True,      1, False),
    #define LP5A4(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, bt, bn)
    #define LP5A4FP(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, bt, bn, fpt)
    #define LP6(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, bt, bn)
    #define LP6a(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, bt, bn)
    #define LP6NR(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, bt, bn)
    'LP6NR'  : (        6, False,      0, False),
    #define LP6A4(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, bt, bn)
    #define LP6NRA4(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, bt, bn)
    #define LP6FP(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, bt, bn, fpt)
    'LP6FP'  : (        6,  True,      1, False),
    #define LP6A4FP(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, bt, bn, fpt)
    #define LP7(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, bt, bn)
    #define LP7NR(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, bt, bn)
    'LP7NR'  : (        7, False,      0, False),
    #define LP7NRFP6(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, bt, bn, fpt1, fpt2, fpt3, fpt4, fpt5, fpt6)
    #define LP7A4(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, bt, bn)
    #define LP7A4FP(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, bt, bn, fpt)
    #define LP7NRA4(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, bt, bn)
    #define LP8(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, bt, bn)
    #define LP8NR(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, bt, bn)
    'LP8NR'  : (        8, False,      0, False),
    #define LP8A4(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, bt, bn)
    #define LP8NRA4(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, bt, bn)
    #define LP9(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, t9, v9, r9, bt, bn)
    #define LP9NR(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, t9, v9, r9, bt, bn)
    'LP9NR'  : (        9, False,      0, False),
    #define LP9A4(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, t9, v9, r9, bt, bn)
    #define LP9NRA4(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, t9, v9, r9, bt, bn)
    #define LP10(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, t9, v9, r9, t10, v10, r10, bt, bn)
    #define LP10NR(offs, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, t9, v9, r9, t10, v10, r10, bt, bn)
    'LP10NR' : (       10, False,      0, False),
    #define LP10A4(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, t9, v9, r9, t10, v10, r10, bt, bn)
    #define LP11(offs, rt, name, t1, v1, r1, t2, v2, r2, t3, v3, r3, t4, v4, r4, t5, v5, r5, t6, v6, r6, t7, v7, r7, t8, v8, r8, t9, v9, r9, t10, v10, r10, t11, v11, r11, bt, bn)
}


# #define AllocMem(___byteSize, ___requirements) LP2(0xc6, APTR, AllocMem , ULONG, ___byteSize, d0, ULONG, ___requirements, d1,, EXEC_BASE_NAME)

buf = ""

SYM_NONE     = 0
SYM_EOF      = 1
SYM_PRAGMA   = 2
SYM_IDENT    = 3
SYM_LPAREN   = 4
SYM_RPAREN   = 5
SYM_COMMA    = 6
SYM_ERR      = 7
SYM_NUM      = 8
SYM_ASTERISK = 9

SYM_NAMES = [ "SYM_NONE", "SYM_EOF", "SYM_PRAGMA", "SYM_IDENT", "SYM_LPAREN", "SYM_RPAREN", "SYM_COMMA", "SYM_ERR", "SYM_NUM", "SYM_ASTERISK" ]

sym = SYM_NONE
sym_str = ""
sym_num = 0

def __error (msg):
    print ("\n\n***ERROR: %s ***" % msg)
    sys.exit(42)

def isIdChar(ch):
    if ((ch>='a') and (ch<='z')) or ((ch>='0') and (ch<='9')) or ((ch>='A') and (ch<='Z')) or (ch=='_'):
        return True
    return False

def isIdStartChar(ch):
    if ((ch>='a') and (ch<='z')) or ((ch>='A') and (ch<='Z')) or (ch=='_'):
        return True
    return False

def isDigit(ch):
    if (ch>='0') and (ch<='9'):
        return True
    return False

def isHexDigit(ch):
    if ((ch>='0') and (ch<='9')) or ((ch>='a') and (ch<='f')) or ((ch>='A') and (ch<='F')):
        return True
    return False

def nextch():
    global ch, buf_pos, buf_len, buf, buf_eol
    if buf_pos >= buf_len:
        buf_eol = True
        return
    ch = buf[buf_pos]
    buf_pos += 1

    sys.stdout.write(ch)

def nextsym():
    global ch, buf_eol, sym, sym_str, sym_num

    while (not buf_eol) and ch.isspace():
        nextch()

    if buf_eol:
        sym = SYM_EOF
        return

    if (ch=='#'):
        nextch();
        sym_str = ""
        while not buf_eol and isIdChar(ch):
            sym_str = sym_str + ch
            nextch()
        sym = SYM_PRAGMA

    elif isIdStartChar(ch):
        sym_str = ch
        nextch();
        while not buf_eol and isIdChar(ch):
            sym_str = sym_str + ch
            nextch()
        sym = SYM_IDENT

    elif ch=='(':
        sym = SYM_LPAREN
        nextch()
    elif ch==')':
        sym = SYM_RPAREN
        nextch()
    elif ch==',':
        sym = SYM_COMMA
        nextch()
    elif ch=='*':
        sym = SYM_ASTERISK
        nextch()
    elif isDigit(ch):
        sym_num = ord(ch)-ord('0')
        nextch()
        if ch=='x':
            nextch()
            sym_num = 0
            while not buf_eol and isHexDigit(ch):
                if (ch>='0') and (ch<='9'):
                    sym_num = sym_num * 16 + (ord(ch)-ord('0'))
                elif (ch>='a') and (ch<='f'):
                    sym_num = sym_num * 16 + (ord(ch)-ord('a')+10)
                elif (ch>='A') and (ch<='F'):
                    sym_num = sym_num * 16 + (ord(ch)-ord('A')+10)
                else:
                    break
                nextch()
        else:
            while not buf_eol and isDigit(ch):
                sym_num = sym_num * 10 + (ch-'0')
                nextch()
        sym = SYM_NUM
    else:
        sym = SYM_ERR

    sys.stdout.write("[%s]" % SYM_NAMES[sym])

def scanner_init():
    global buf_pos, buf_eol, buf_len
    buf_pos = 0
    buf_eol = False
    buf_len = len(buf)
    nextch()
    nextsym()

#define AllocMem(___byteSize, ___requirements) LP2(0xc6, APTR, AllocMem , ULONG, ___byteSize, d0, ULONG, ___requirements, d1,, EXEC_BASE_NAME)
#define [SYM_PRAGMA]AllocMem([SYM_IDENT]_[SYM_LPAREN]__byteSize,[SYM_IDENT] [SYM_COMMA]___requirements)[SYM_IDENT] [SYM_RPAREN]LP2([SYM_IDENT]0[SYM_LPAREN]xc6,[SYM_NUM] [SYM_COMMA]APTR,[SYM_IDENT] [SYM_COMMA]AllocMem [SYM_IDENT], [SYM_COMMA]ULONG,[SYM_IDENT] [SYM_COMMA]___byteSize,[SYM_IDENT] [SYM_COMMA]d0,[SYM_IDENT] [SYM_COMMA]ULONG,[SYM_IDENT] [SYM_COMMA]___requirements,[SYM_IDENT] [SYM_COMMA]d1,[SYM_IDENT],[SYM_COMMA] [SYM_COMMA]EXEC_BASE_NAME)[SYM_IDENT][SYM_RPAREN]

def parse_type():

    global sym, sym_num, sym_str

    t = ""
    while True:
        if sym == SYM_IDENT:
            if not t:
                t = sym_str
            else:
                t = t + " " + sym_str
        elif sym == SYM_ASTERISK:
            if not t:
                t = "*"
            else:
                t = t + " *"
        elif sym == SYM_LPAREN:
            nextsym()
            st = parse_type()
            if sym != SYM_RPAREN:
                __error ("SYNTAX ERROR IN TYPE!")
                sys.exit(23)
            nextsym()
            if not t:
                t = "(" + st + ")"
            else:
                t = t + " (" + st + ")"
        else:
            return t
        nextsym()


def parse_arg_triple():

    global sym, sym_num, sym_str

    t = parse_type()
    if not t:
        __error ("type expected.")
        return None

    if sym != SYM_COMMA:
        __error (", expected.")
        return None
    nextsym()

    if sym != SYM_IDENT:
        __error ("arg identifier expected.")
        return None
    arg_name = sym_str
    nextsym()

    if sym != SYM_COMMA:
        __error (", expected.")
        return None
    nextsym()

    if sym != SYM_IDENT:
        __error ("register identifier expected.")
        return None
    reg_name = sym_str
    nextsym()

    return t, arg_name, reg_name

def parse_macro (num_args, rt, num_fp, fr):

    global sym, sym_num, sym_str

    if sym != SYM_LPAREN:
        __error ("( expected.")
        return None
    nextsym()

    if sym != SYM_NUM:
        __error ("offset expected.")
        return None
    offset = sym_num
    nextsym()

    if rt:
        if sym != SYM_COMMA:
            __error (", expected.")
            return None
        nextsym()

        return_type = parse_type()
        if not return_type:
            __error ("return type expected.")
            return None

    else:
        return_type = "void"

    if sym != SYM_COMMA:
        __error (", expected.")
        return None
    nextsym()

    if sym != SYM_IDENT:
        __error ("identifier expected. [1]")
        return None
    syscall_name = sym_str
    nextsym()

    args = []

    for i in range (num_args):

        if sym != SYM_COMMA:
            __error (", expected.")
            return None
        nextsym()

        a = parse_arg_triple()
        if not a:
            return None
        args.append(a)

    nextsym()
    if sym != SYM_COMMA:
        __error (", expected.")
        return None
    nextsym()

    if sym != SYM_IDENT:
        __error ("bn identifier expected. [1]")
        return None
    bn = sym_str
    nextsym()

    fps = []
    for i in range (num_fp):

        if sym != SYM_COMMA:
            __error (", expected.")
            return None
        nextsym()

        fp = parse_type()
        if not fp:
            return None
        fps.append(fp)

    if fr:
        if sym != SYM_COMMA:
            __error (", expected.")
            return None
        nextsym()

        frt = parse_type()
        if not frt:
            return None
    else:
        frt = None

    if sym != SYM_RPAREN:
        __error (") expected.")
        return None
    nextsym()

    return syscall_name, offset, return_type, args, bn

def parse_pragma():

    global sym, sym_num, sym_str

    nextsym() # skip #define

    if sym != SYM_IDENT:
        __error ("identifier expected. [1]")
        return None
    syscall_name = sym_str
    nextsym()

    if sym != SYM_LPAREN:
        __error ("( expected.")
        return None
    nextsym()

    args = []

    if sym == SYM_RPAREN:
        nextsym()
    else:
        while True:
            if sym != SYM_IDENT:
                __error ("identifier expected. [2]")
                return None
            args.append(sym_str)
            nextsym()

            if sym == SYM_RPAREN:
                nextsym()
                break

            if sym != SYM_COMMA:
                __error (", expected.")
                return None
            nextsym()

    print()
    print ("pragma parse result: syscall_name=%s, args=%s" % (syscall_name, repr(args)))

    if sym != SYM_IDENT:
        __error ("identifier expected. [3]")
        return None
    macro_name = sym_str
    nextsym()

    if not (macro_name in MACROS):
        __error ("unknown macro %s" % (macro_name))
        sys.exit(1)

    num_args, rt, num_fp, fr = MACROS[macro_name]

    return parse_macro (num_args, rt, num_fp, fr)

def gen_stub (syscall_name, offset, return_type, args, bn, out):

    lib_prefix, lib_type = LIBS[bn]

    out.write ("static %s __saveds %s_%s ( register %s __libBase __asm(\"a6\")" % (return_type, lib_prefix, syscall_name, lib_type))

    n = len(args)
    if not n:
        out.write (")\n")
    else:
        out.write (",\n")
        for i in range(n):
            out.write ("                                                        register %s %s  __asm(\"%s\")" % (args[i][0], args[i][1], args[i][2]))
            if i<(n-1):
                out.write (",\n")
            else:
                out.write (")\n")
    out.write ("{\n")
    out.write ("    lprintf (\"%s: %s unimplemented STUB called.\");\n" % (lib_prefix, syscall_name));
    out.write ("    assert(FALSE);\n")
    out.write ("}\n\n")

funcs = []

with codecs.open (INPUTFN, 'r', 'utf8') as inf:

    for line in inf:

        line = line.strip()
        l = len(line)

        if (l==0) or (line[l-1]!='\\'):
            buf = buf + line
            print ()
            print (buf)

            scanner_init()

            if (sym == SYM_PRAGMA) and (sym_str == "define"):
                res = parse_pragma()
                if res:
                    print ("parse result: %s" % repr(res))
                    gen_stub (res[0], res[1], res[2], res[3], res[4], sys.stdout)
                    funcs.append (res)

            while sym != SYM_ERR and sym != SYM_EOF:
                nextsym()

            buf = ""
        else:
            buf = buf + line[:l-1]

with codecs.open (OUT_STUBS_FN, 'w', 'utf8') as out:
    for f in funcs:
        gen_stub (f[0], f[1], f[2], f[3], f[4], out)

print ("%s created." % OUT_STUBS_FN)

with codecs.open (OUT_FUNCTABLE_FN, 'w', 'utf8') as out:
    for f in funcs:
        out.write("    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(%d)].vec = _exec_%s;\n" % (-f[1], f[0]))

print ("%s created." % OUT_FUNCTABLE_FN)

