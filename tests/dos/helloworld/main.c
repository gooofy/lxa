#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

int main(void)
{
    BPTR out = Output();
    Write(out, (CONST APTR) "Hello, World!\n", 14);
    return 0;
}
