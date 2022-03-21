#!/bin/env python3

import re
import sys
import io

SIGPATTERN = re.compile(r"^([^(]+)\(([^)]*)\)\W*\(([^)]*)\)")

libBase   = '???'
baseType  = '???'
libName   = '???'
libPrefix = '???'

def gen_stub (f, out):

    if not f['name']:
        return

    out.write ("%s __saveds %s_%s ( register %s %s __asm(\"a6\")" % (f['returnType'], libPrefix, f['name'], baseType, libBase))

    n = len(f['args'])
    if not n:
        out.write (")\n")
    else:
        out.write (",\n")
        for i in range(n):
            out.write ("                                                        register %s __asm(\"%s\")" % (f['args'][i].strip(), f['regs'][i]))
            if i<(n-1):
                out.write (",\n")
            else:
                out.write (")\n")
    out.write ("{\n")
    out.write ("    DPRINTF (LOG_ERROR, \"%s: %s() unimplemented STUB called.\\n\");\n" % (libPrefix, f['name']));
    out.write ("    assert(FALSE);\n")
    out.write ("}\n\n")

def gen_functable (funcs, out):

    out.write ("APTR __g_lxa%s_FuncTab [] =\n" % libPrefix)
    out.write ("{\n")
    out.write ("    __g_lxa_dos_OpenLib,\n")
    out.write ("    __g_lxa_dos_CloseLib,\n")
    out.write ("    __g_lxa_dos_ExpungeLib,\n")
    out.write ("    __g_lxa_dos_ExtFuncLib,\n")

    for f in funcs:
        if not f['name']:
            out.write ("    NULL, // offset = %d\n"% f['offset'])
            continue
        out.write ("    %s_%s, // offset = %d\n" % (libPrefix, f['name'], f['offset']))

    out.write ("    (APTR) ((LONG)-1)\n")
    out.write ("};\n")

input_stream = io.TextIOWrapper(sys.stdin.buffer, encoding='latin1')

offset = -30
skip = False
idxPrivate = 0

funcs = []

for line in input_stream:

    if not line:
        continue

    line = line.strip()

    if skip:
        skip = False
        continue

    if line.startswith ('=='):

        if line.startswith ('==base '):
            libBase = line[8:]
            print ("// libBase: %s" % libBase)
        elif line.startswith ('==basetype '):
            baseType = line[11:]
            print ("// baseType: %s" % baseType)
        elif line.startswith ('==bias '):
            offset = - int(line[7:])
            # print ("// offset: %d" % offset)
        elif line.startswith ('==reserve '):
            n = int(line[10:])
            # print ("// reserve %d -> new offset %d" % (n, offset))
            for i in range(n):
                funcs.append ({'name': 'private%d'%idxPrivate, 'offset': offset, 'args':[], 'regs':[], 'returnType': 'VOID'})
                idxPrivate+=1
                offset -= 6
        elif line.startswith ('==alias'):
            skip = True
            # print ("// skip")
        elif line.startswith ('==varargs'):
            skip = True
            # print ("// varargs")
        elif line.startswith ('==libname '):
            libName = line[10:]
            print ("// libname: %s" % libName)
            libPrefix = '_' + libName.replace('.library','')
    else:
        # print(line)

        m = SIGPATTERN.match (line)
        if m:
            # print ("*** MATCH %s" % m.group(1))
            # print ("          %s" % m.group(2))
            # print ("          %s" % m.group(3))

            ri = m.group(1).rfind(' ')
            returntype = m.group(1)[:ri]
            fname = m.group(1)[ri:].strip()
            # print (returntype, ri, fname)

            args = []
            regs = []
            sargs = m.group(2).strip()
            if sargs:
                args = sargs.split(',')
                regs = m.group(3).strip().split(',')

            # if m.group(1) == "VOID":
            #     sys.stdout.write ("DECLARE SUB %s (" % fname)
            # else:
            #     sys.stdout.write ("DECLARE FUNCTION %s (" % fname)

            # first=True
            # for arg in args:

            #     if not arg:
            #         continue

            #     if first:
            #         first = False
            #     else:
            #         sys.stdout.write (", ")

            #     ti = arg.rfind(' ')
            #     argname = arg[ti:].strip()
            #     sys.stdout.write ("BYVAL %s AS %s" % (argname, arg[:ti].strip()))

            # if returntype == "VOID":
            #     sys.stdout.write (")")
            # else:
            #     sys.stdout.write (") AS %s" % returntype)

            # print (" LIB %d %s (%s)" % (offset, libBase, regs))

            f = { 'name': fname, 'returnType': returntype, 'args': args, 'regs': regs, 'offset': offset }

            # print (repr(f))

            funcs.append ( f )

            offset -= 6
        else:
            print ("// ??? %s" % line)
            assert False

for f in funcs:
    gen_stub (f, sys.stdout)

gen_functable (funcs, sys.stdout)

