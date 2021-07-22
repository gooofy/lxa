import codecs
import re

# #define AllocMem(___byteSize, ___requirements) LP2(0xc6, APTR, AllocMem , ULONG, ___byteSize, d0, ULONG, ___requirements, d1,, EXEC_BASE_NAME)

plp2 = re.compile (r"^#define ([A-Za-z_]+)\(([_a-zA-Z]+),\s*([_a-zA-Z]+)\)")

buf = ""

SYM_NONE   = 0
SYM_EOF    = 1
SYM_PRAGMA = 2
SYM_ERR    = 999

sym = SYM_NONE
sym_str = ""

def isIdChar(ch):
    if ((ch>='a') and (ch<='z')) or ((ch>='A') and (ch<='Z')) or (ch=='_'):
        return True
    return False

def nextch():
    global ch, buf_pos, buf_len, buf, buf_eol
    if buf_pos >= buf_len:
        buf_eol = True
        return
    ch = buf[buf_pos]
    buf_pos += 1

def nextsym():
    global ch, buf_eol, sym, sym_str

    while (not buf_eol) and ch.isspace():
        nextch()

    if buf_eol:
        sym = SYM_EOF
        return

    if (ch=='#'):
        nextch();
        sym_str = ""
        while isIdChar(ch):
            sym_str = sym_str + ch
            nextch()
        sym = SYM_PRAGMA

    else:
        sym = SYM_ERR

def scanner_init():
    global buf_pos, buf_eol, buf_len
    buf_pos = 0
    buf_eol = False
    buf_len = len(buf)
    nextch()
    nextsym()

with codecs.open ("exec.h", 'r', 'utf8') as inf:

    for line in inf:

        line = line.strip()
        l = len(line)

        if (l==0) or (line[l-1]!='\\'):
            buf = buf + line
            print (buf)

            scanner_init()
            while sym != SYM_ERR and sym != SYM_EOF:
                print(sym)
                nextsym()

            # m = plp2.match(buf)
            # if m:
            #     fn = m.group(1)
            #     pn1 = m.group(2)
            #     pn2 = m.group(3)
            #     print ("***MATCH***, fn=%s, pn1=%s, pn2=%s" % (fn, pn1, pn2))

            buf = ""
        else:
            buf = buf + line[:l-1]


