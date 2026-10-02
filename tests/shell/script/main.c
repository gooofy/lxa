/*
 * Shell Script Test
 * Runs a plain ECHO script as an AmigaDOS command file (C:EXECUTE), the way a user
 * script runs on AmigaOS.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

int main(void)
{
    /* Execute() returns DOSTRUE once the shell ran the command; the script
     * output goes to our output stream */
    LONG rc = Execute((CONST_STRPTR)"EXECUTE SYS:Tests/Shell/Script/script.txt", 0, Output());

    if (rc != DOSTRUE) {
        PutStr((CONST_STRPTR)"Script execution failed\n");
        return 20;
    }

    return 0;
}
