#include <exec/types.h>

#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

//struct ExecBase          *SysBase;
extern struct DosLibrary *DOSBase;
//extern struct Library *IntuitionBase;
static BPTR            _io_stdout = 0;

int main(void)
{
    _io_stdout = Output();

    Write(_io_stdout, (CONST APTR) "Hello, World!\n", 14);

    return 0;
}

