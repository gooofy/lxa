/*
 * Shell Alias Test
 * Tests ALIAS command functionality
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <string.h>
#include <stdio.h>

extern struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    /* Execute the shell script */
    LONG rc = Execute("SYS:System/Shell script.txt", (BPTR)0, (BPTR)0);
    
    if (rc != 0) {
        printf("Script execution failed with rc=%d\n", rc);
        return 20;
    }
    
    return 0;
}
