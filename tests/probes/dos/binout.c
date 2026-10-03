/*
 * Probe (Phase 222g): Write() passes every byte value through to the
 * output stream unchanged, NUL included.  Also a regression check for the
 * suite-ref harness, which must compare program output binary-safely
 * (it used to cut lxa's output at the first NUL).
 */
#include <exec/types.h>
#include <dos/dos.h>
#include "probe.h"

int main(void)
{
    static UBYTE bytes[256];
    int i;
    for (i = 0; i < 256; i++)
        bytes[i] = (UBYTE)i;
    probe_s("before\n");
    probe_flush();
    Write(Output(), bytes, 4);          /* 00 01 02 03 */
    Write(Output(), "|nul\n", 5);
    Write(Output(), bytes + 0x7f, 0x81); /* 7f..ff */
    Write(Output(), "\nafter\n", 7);
    return 0;
}
