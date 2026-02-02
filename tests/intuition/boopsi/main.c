
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>
#include <stdio.h>

int main(void)
{
    struct Library *IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!IntuitionBase) {
        printf("Failed to open intuition.library\n");
        return 1;
    }

    printf("Creating Test Class...\n");
    Class *myClass = MakeClass((CONST_STRPTR)"test.class", (CONST_STRPTR)"rootclass", NULL, 0, 0);
    if (!myClass) {
        printf("MakeClass failed\n");
        return 1;
    }

    printf("Creating Object...\n");
    Object *obj = NewObject(myClass, NULL, TAG_END);
    if (!obj) {
        printf("NewObject failed\n");
        FreeClass(myClass);
        return 1;
    }

    printf("Object created at\n");

    printf("Disposing Object...\n");
    DisposeObject(obj);

    printf("Freeing Class...\n");
    FreeClass(myClass);

    printf("Done.\n");
    CloseLibrary(IntuitionBase);
    return 0;
}
