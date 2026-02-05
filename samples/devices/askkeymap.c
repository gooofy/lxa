/* askkeymap.c - Adapted from RKM Devices Manual AskKeymap sample
 * Demonstrates the CD_ASKKEYMAP command of console.device:
 * - Opening console.device with CONU_LIBRARY
 * - Sending CD_ASKKEYMAP command
 * - Reading and displaying keymap data
 *
 * Modified for automated testing (auto-exits with verification).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/libraries.h>

#include <devices/console.h>
#include <devices/keymap.h>
#include <devices/conunit.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

int main(void)
{
    struct MsgPort *ConsoleMP;
    struct IOStdReq *ConsoleIO;
    struct KeyMap *keymap;
    int i, j;
    UBYTE *prt;
    int errors = 0;

    printf("AskKeymap: CD_ASKKEYMAP command test\n\n");

    /* Create the message port */
    ConsoleMP = CreateMsgPort();
    if (!ConsoleMP) {
        printf("ERROR: Could not create message port\n");
        return 1;
    }

    /* Create the IORequest */
    ConsoleIO = (struct IOStdReq *)CreateIORequest(ConsoleMP, sizeof(struct IOStdReq));
    if (!ConsoleIO) {
        printf("ERROR: Could not create IORequest\n");
        DeleteMsgPort(ConsoleMP);
        return 1;
    }

    /* Open the console device with CONU_LIBRARY (no window needed) */
    if (OpenDevice("console.device", CONU_LIBRARY, (struct IORequest *)ConsoleIO, 0L)) {
        printf("ERROR: console.device did not open\n");
        DeleteIORequest((struct IORequest *)ConsoleIO);
        DeleteMsgPort(ConsoleMP);
        return 1;
    }

    printf("OK: console.device opened with CONU_LIBRARY\n");

    /* Allocate memory for the keymap */
    keymap = (struct KeyMap *)AllocMem(sizeof(struct KeyMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!keymap) {
        printf("ERROR: Could not allocate memory for keymap\n");
        CloseDevice((struct IORequest *)ConsoleIO);
        DeleteIORequest((struct IORequest *)ConsoleIO);
        DeleteMsgPort(ConsoleMP);
        return 1;
    }

    printf("OK: Allocated KeyMap structure (%ld bytes)\n", (long)sizeof(struct KeyMap));

    /* Send CD_ASKKEYMAP command */
    ConsoleIO->io_Length = sizeof(struct KeyMap);
    ConsoleIO->io_Data = (APTR)keymap;
    ConsoleIO->io_Command = CD_ASKKEYMAP;

    if (DoIO((struct IORequest *)ConsoleIO)) {
        printf("ERROR: CD_ASKKEYMAP failed, error=%d\n", ConsoleIO->io_Error);
        errors++;
    } else {
        printf("OK: CD_ASKKEYMAP succeeded\n");

        /* Verify we got valid keymap pointers */
        if (!keymap->km_LoKeyMapTypes) {
            printf("ERROR: km_LoKeyMapTypes is NULL\n");
            errors++;
        } else {
            printf("OK: km_LoKeyMapTypes = 0x%08lx\n", (unsigned long)keymap->km_LoKeyMapTypes);
        }

        if (!keymap->km_LoKeyMap) {
            printf("ERROR: km_LoKeyMap is NULL\n");
            errors++;
        } else {
            printf("OK: km_LoKeyMap = 0x%08lx\n", (unsigned long)keymap->km_LoKeyMap);
        }

        if (!keymap->km_LoCapsable) {
            printf("ERROR: km_LoCapsable is NULL\n");
            errors++;
        } else {
            printf("OK: km_LoCapsable = 0x%08lx\n", (unsigned long)keymap->km_LoCapsable);
        }

        if (!keymap->km_LoRepeatable) {
            printf("ERROR: km_LoRepeatable is NULL\n");
            errors++;
        } else {
            printf("OK: km_LoRepeatable = 0x%08lx\n", (unsigned long)keymap->km_LoRepeatable);
        }

        if (!keymap->km_HiKeyMapTypes) {
            printf("ERROR: km_HiKeyMapTypes is NULL\n");
            errors++;
        } else {
            printf("OK: km_HiKeyMapTypes = 0x%08lx\n", (unsigned long)keymap->km_HiKeyMapTypes);
        }

        if (!keymap->km_HiKeyMap) {
            printf("ERROR: km_HiKeyMap is NULL\n");
            errors++;
        } else {
            printf("OK: km_HiKeyMap = 0x%08lx\n", (unsigned long)keymap->km_HiKeyMap);
        }

        if (!keymap->km_HiCapsable) {
            printf("ERROR: km_HiCapsable is NULL\n");
            errors++;
        } else {
            printf("OK: km_HiCapsable = 0x%08lx\n", (unsigned long)keymap->km_HiCapsable);
        }

        if (!keymap->km_HiRepeatable) {
            printf("ERROR: km_HiRepeatable is NULL\n");
            errors++;
        } else {
            printf("OK: km_HiRepeatable = 0x%08lx\n", (unsigned long)keymap->km_HiRepeatable);
        }

        /* If all pointers are valid, print top row of keyboard */
        if (errors == 0) {
            printf("\nResult of CD_ASKKEYMAP for top row of keyboard:\n\n");
            printf("\tShift\n");
            printf("\tAlt\tAlt\tShift\tNo Qualifier\n");
            prt = (UBYTE *)keymap->km_LoKeyMap;
            for (j = 0; j < 14; j++) {
                for (i = 0; i < 4; i++) {
                    if (*prt >= 0x20 && *prt < 0x7F) {
                        printf("\t%c", *prt);
                    } else {
                        printf("\t0x%02x", *prt);
                    }
                    prt++;
                }
                printf("\n");
            }
        }
    }

    /* Clean up */
    FreeMem(keymap, sizeof(struct KeyMap));
    CloseDevice((struct IORequest *)ConsoleIO);
    DeleteIORequest((struct IORequest *)ConsoleIO);
    DeleteMsgPort(ConsoleMP);

    printf("\n");
    if (errors == 0) {
        printf("AskKeymap: All tests passed!\n");
        return 0;
    } else {
        printf("AskKeymap: FAILED with %d errors\n", errors);
        return 1;
    }
}
