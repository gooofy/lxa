/*
 * ConBuf (Phase 222a): in which order buffered dos output (FPuts, FPutC,
 * VFPrintf, PutStr) and unbuffered Write() reach a console window.  The
 * window content is compared with AmigaOS 3.1 by
 * tests/scenarios/interactive/conbuf.yaml; nothing is flushed explicitly
 * until "READY" has been printed, so what the window shows is what the
 * console handle let through on its own (snapshot "window"), and whether
 * reading from the console shows a pending prompt (snapshot "read").
 * (Checked once on 3.1: a read does not flush a second handle on the same
 * console - Open("*") after SetConsoleTask() - only the handle read from.)
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/stdio.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

int main(void)
{
    BPTR con = Open((STRPTR)"CON:0/20/500/180/ConBuf", MODE_NEWFILE);
    BPTR out = Output();
    LONG none = 0;

    if (!con)
        return 20;

    FPuts(con, (STRPTR)"o1 ");
    Write(con, (APTR)"w1\n", 3);
    FPuts(con, (STRPTR)"o2\n");
    Write(con, (APTR)"w2\n", 3);
    FPutC(con, 'c');
    FPutC(con, '3');
    FPutC(con, '\n');
    Write(con, (APTR)"w3\n", 3);
    VFPrintf(con, (STRPTR)"v4\n", &none);
    Write(con, (APTR)"w4\n", 3);
    FPuts(con, (STRPTR)"o5 no newline ");
    Write(con, (APTR)"w5\n", 3);
    SetVBuf(con, NULL, BUF_FULL, 512);
    FPuts(con, (STRPTR)"f6\n");
    Write(con, (APTR)"w6\n", 3);
    SetVBuf(con, NULL, BUF_NONE, -1);
    FPuts(con, (STRPTR)"n7 ");
    Write(con, (APTR)"w7\n", 3);
    SetVBuf(con, NULL, BUF_LINE, 512);
    FPuts(con, (STRPTR)"l8\n");
    Write(con, (APTR)"w8\n", 3);
    FPutC(con, 'l');
    FPutC(con, '9');
    FPutC(con, '\n');
    Write(con, (APTR)"w9\n", 3);

    /* a prompt without '\n', then a read from the same handle: does the
     * read show the prompt? */
    SetVBuf(con, NULL, BUF_LINE, 512);
    FPuts(con, (STRPTR)"prompt? ");

    /* the program's own Output() (a file on the reference) */
    FPuts(out, (STRPTR)"READY\n");
    Flush(out);
    FGetC(con);
    FPuts(out, (STRPTR)"DONE\n");
    Flush(out);
    Delay(500);
    Close(con);
    return 0;
}
