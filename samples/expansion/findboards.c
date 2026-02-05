/*
 * findboards.c - Test expansion.library board enumeration
 *
 * Based on RKM Expansion sample, adapted for automated testing.
 * Tests: OpenLibrary(expansion.library), FindConfigDev
 *
 * In lxa (without actual expansion boards), this should find no boards
 * but successfully open the library and run FindConfigDev.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/configvars.h>

#include <clib/exec_protos.h>
#include <clib/expansion_protos.h>

#include <stdio.h>
#include <stdlib.h>

struct Library *ExpansionBase = NULL;

int main(int argc, char **argv)
{
    struct ConfigDev *myCD;
    int boardCount = 0;

    printf("FindBoards: Expansion board enumeration test\n\n");

    printf("Opening expansion.library...\n");
    ExpansionBase = OpenLibrary("expansion.library", 0L);
    if (ExpansionBase == NULL)
    {
        printf("ERROR: Can't open expansion.library\n");
        return 1;
    }
    printf("OK: expansion.library opened (version %d)\n", ExpansionBase->lib_Version);

    printf("\nSearching for expansion boards...\n");

    /*
     * FindConfigDev(oldConfigDev, manufacturer, product)
     * oldConfigDev = NULL for the top of the list
     * manufacturer = -1 for any manufacturer
     * product      = -1 for any product
     */
    myCD = NULL;
    while ((myCD = FindConfigDev(myCD, -1L, -1L)) != NULL)
    {
        boardCount++;
        printf("\n--- Board %d found at 0x%08lx ---\n", boardCount, (ULONG)myCD);

        printf("Board ID (ExpansionRom) information:\n");
        printf("  er_Manufacturer = %d (0x%04x)\n",
               myCD->cd_Rom.er_Manufacturer, myCD->cd_Rom.er_Manufacturer);
        printf("  er_Product      = %d (0x%02x)\n",
               myCD->cd_Rom.er_Product, myCD->cd_Rom.er_Product);
        printf("  er_Type         = 0x%02x\n", myCD->cd_Rom.er_Type);
        printf("  er_Flags        = 0x%02x\n", myCD->cd_Rom.er_Flags);

        printf("Configuration (ConfigDev) information:\n");
        printf("  cd_BoardAddr    = 0x%08lx\n", (ULONG)myCD->cd_BoardAddr);
        printf("  cd_BoardSize    = 0x%08lx (%ldK)\n",
               (ULONG)myCD->cd_BoardSize, ((ULONG)myCD->cd_BoardSize) / 1024);
        printf("  cd_Flags        = 0x%02x\n", myCD->cd_Flags);
    }

    printf("\nTotal expansion boards found: %d\n", boardCount);

    /* In lxa without actual hardware, we expect 0 boards */
    if (boardCount == 0)
    {
        printf("OK: No expansion boards (expected in emulator)\n");
    }
    else
    {
        printf("OK: Found %d expansion board(s)\n", boardCount);
    }

    CloseLibrary(ExpansionBase);
    printf("OK: expansion.library closed\n");

    printf("\nPASS: FindBoards tests completed successfully\n");
    return 0;
}
