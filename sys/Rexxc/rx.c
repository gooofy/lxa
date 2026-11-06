/*
 * RX - run an ARexx program (Phase 250)
 *
 *   RX "string program" | <program> [arguments]
 *
 * A command line starting with a quote is a string program; otherwise
 * the first word names the program file and the rest is its argument.
 * The program inherits RX's input and output.  A non-zero return code is
 * reported as "Command returned rc[/error: text]" and becomes RX's own.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <rexx/errors.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>

struct RxsLib *RexxSysBase = NULL;

#include "rexx_common.h"

static const char version[] = "$VER: RX 36.21 (01.11.2026)";

int main(void)
{
    struct MsgPort *reply;
    struct RexxMsg *rm;
    char *line = (char *)GetArgStr();
    LONG len, start = 0, rc;
    ULONG flags = 0;

    (void)version;
    if (!line)
        line = (char *)"";
    len = lxa_rexx_strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == ' ' || line[len - 1] == '\t'))
        len--;
    while (start < len && (line[start] == ' ' || line[start] == '\t'))
        start++;
    if (start >= len) {
        PutStr((STRPTR)"Usage: RX <program> [arguments]\n");
        return RETURN_WARN;
    }
    if (line[start] == '"' || line[start] == '\'') {
        /* a string program: the text up to the closing quote */
        char q = line[start++];
        LONG end = start;
        while (end < len && line[end] != q)
            end++;
        len = end;
        flags = RXFF_STRING;
    }

    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 36);
    if (!RexxSysBase)
        return RETURN_FAIL;
    reply = CreateMsgPort();
    rm = reply ? CreateRexxMsg(reply, (STRPTR)"REXX", (STRPTR)"REXX") : NULL;
    if (!rm || !(rm->rm_Args[0] = (STRPTR)CreateArgstring((STRPTR)line + start, len - start))) {
        if (rm)
            DeleteRexxMsg(rm);
        if (reply)
            DeleteMsgPort(reply);
        CloseLibrary((struct Library *)RexxSysBase);
        return RETURN_FAIL;
    }
    rm->rm_Action = RXCOMM | flags;
    rm->rm_Stdin = (LONG)Input();
    rm->rm_Stdout = (LONG)Output();

    if (!lxa_rexx_send(rm, reply)) {
        PutStr((STRPTR)"RX: ARexx is not active (start SYS:System/RexxMast)\n");
        rc = RETURN_FAIL;
    } else {
        rc = rm->rm_Result1;
        if (rc) {
            Printf((STRPTR)"Command returned %ld", rc);
            if (rm->rm_Result2)
                Printf((STRPTR)"/%ld: %s", rm->rm_Result2, (LONG)lxa_rexx_error_text(rm->rm_Result2));
            PutStr((STRPTR)"\n");
        }
    }
    DeleteArgstring((UBYTE *)rm->rm_Args[0]);
    DeleteRexxMsg(rm);
    DeleteMsgPort(reply);
    CloseLibrary((struct Library *)RexxSysBase);
    return rc;
}
