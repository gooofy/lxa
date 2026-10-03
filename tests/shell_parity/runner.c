/*
 * Shell parity runner (Phase 221)
 *
 *   SYS:Tests/ShellParity/Run <script>
 *
 * Runs SYS:Tests/ShellParity/<script>.script as an AmigaDOS command file
 * (C:EXECUTE in a shell), the way a user script runs.  The same scripts run
 * on real AmigaOS 3.1 (with its WB 3.1 C: commands) and on lxa (with lxa's
 * own C: commands and shell); the output must be identical
 * (tests/shell_parity/<script>.ref.out, captured on the reference).
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    char cmd[256];

    if (argc < 2 || strlen(argv[1]) > 100) {
        PutStr((CONST_STRPTR)"usage: Run <script>\n");
        return 20;
    }
    strcpy(cmd, "Execute SYS:Tests/ShellParity/");
    strcat(cmd, argv[1]);
    strcat(cmd, ".script");

    /* the shell runs the command, then reads further commands from the
     * (empty) input: no input means EOF right away */
    if (Execute((CONST_STRPTR)cmd, 0, Output()) != DOSTRUE) {
        PutStr((CONST_STRPTR)"Execute failed\n");
        return 20;
    }
    return 0;
}
