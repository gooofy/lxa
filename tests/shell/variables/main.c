/*
 * Shell Variables Test
 * Exercises the DOS variable APIs and emits the strings expected by the
 * shell driver test.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <string.h>
#include <stdio.h>

extern struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    char buffer[256];
    LONG len;

    printf("Testing Shell Variables\n");
    printf("=======================\n");

    if (!SetVar((STRPTR)"TESTVAR", (STRPTR)"Hello", -1, GVF_LOCAL_ONLY | LV_VAR)) {
        PrintFault(IoErr(), (STRPTR)"SET");
        return 20;
    }

    if (!SetVar((STRPTR)"MYENV", (STRPTR)"GlobalValue", -1, GVF_GLOBAL_ONLY | LV_VAR)) {
        PrintFault(IoErr(), (STRPTR)"SETENV");
        return 20;
    }

    len = GetVar((STRPTR)"MYENV", (STRPTR)buffer, sizeof(buffer), GVF_GLOBAL_ONLY | LV_VAR);
    if (len < 0) {
        PrintFault(IoErr(), (STRPTR)"GETENV");
        return 20;
    }

    buffer[len] = '\0';
    printf("%s\n", buffer);

    if (!DeleteVar((STRPTR)"MYENV", GVF_GLOBAL_ONLY | LV_VAR)) {
        PrintFault(IoErr(), (STRPTR)"UNSETENV");
        return 20;
    }

    printf("Variable tests done\n");
    return 0;
}
