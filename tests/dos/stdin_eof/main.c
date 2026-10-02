/*
 * stdin_eof - console input with no input available (Phase 201)
 *
 * Under liblxa the host stdin is detached from the emulated console: with
 * no injected input, WaitForChar() times out and Read() returns end-of-file
 * instead of blocking on whatever stdin the test harness inherited.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>
#include <stdio.h>

extern struct DosLibrary *DOSBase;

int main(void)
{
    char buf[16];
    LONG ready, n;

    ready = WaitForChar(Input(), 100000);   /* 0.1 s */
    printf("WaitForChar=%ld\n", ready);

    n = Read(Input(), buf, sizeof(buf));
    printf("Read=%ld\n", n);

    if (ready == 0 && n == 0)
        printf("PASS: stdin_eof\n");
    else
        printf("FAIL: stdin_eof\n");
    return 0;
}
