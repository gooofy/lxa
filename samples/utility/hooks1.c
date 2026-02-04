/* Hooks1.c - Adapted from RKM sample
 * Demonstrates Hook callback system from utility.library
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <utility/hooks.h>
#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/utility_protos.h>

#include <stdio.h>

extern struct Library *SysBase;
struct Library *UtilityBase;

/* Hook entry point - converts register calling convention to C
 * This function receives parameters in registers A0, A1, A2 and
 * calls the h_SubEntry function with those parameters on the stack
 */
ULONG hookEntry(register struct Hook *h __asm("a0"),
                register APTR object __asm("a2"),
                register APTR message __asm("a1"))
{
    /* Get the C function pointer from h_SubEntry and call it */
    typedef ULONG (*SubEntry)(struct Hook *, APTR, APTR);
    SubEntry func = (SubEntry)h->h_SubEntry;
    return func(h, object, message);
}

/* Initialize a Hook structure */
VOID InitHook(struct Hook *h, ULONG (*func)(), VOID *data)
{
    /* Make sure a pointer was passed */
    if (h)
    {
        /* Fill in the Hook fields */
        h->h_Entry = (ULONG (*)())hookEntry;
        h->h_SubEntry = func;
        h->h_Data = data;
    }
}

/* This function only prints out a message indicating that we are
 * inside the callback function.
 */
ULONG MyFunction(struct Hook *h, VOID *o, VOID *msg)
{
    printf("Hooks1: Inside MyFunction() - Hook callback working!\n");
    return 1;
}

int main(int argc, char **argv)
{
    struct Hook h;

    printf("Hooks1: Starting Hook callback example\n");

    /* Open the utility library */
    if (UtilityBase = OpenLibrary("utility.library", 36))
    {
        /* Initialize the callback Hook */
        InitHook(&h, MyFunction, NULL);

        /* Use the utility library function to invoke the Hook */
        printf("Hooks1: Calling CallHookPkt()...\n");
        CallHookPkt(&h, NULL, NULL);
        printf("Hooks1: CallHookPkt() returned\n");

        /* Close utility library now that we're done with it */
        CloseLibrary(UtilityBase);
        
        printf("Hooks1: Test completed successfully\n");
        return RETURN_OK;
    }
    else
    {
        printf("Hooks1: ERROR - Couldn't open utility.library\n");
        return RETURN_FAIL;
    }
}
