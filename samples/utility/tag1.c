/* Tag1.c - Utility library TagItem sample */
#include <exec/types.h>
#include <exec/libraries.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/utility_protos.h>
#include <stdio.h>

struct Library *UtilityBase;

#define MY_TAG (0x12345678)

int main(void)
{
    struct TagItem tags[] = {
        {MY_TAG,   1},
        {TAG_DONE, 0}
    };
    struct TagItem *ti;

    UtilityBase = OpenLibrary((CONST_STRPTR)"utility.library", 36);
    if (!UtilityBase) return 1;

    printf("Searching for MY_TAG in tag list...\n");
    ti = FindTagItem(MY_TAG, tags);
    if (ti) {
        printf("Found it! Data: %u\n", (unsigned int)ti->ti_Data);
    } else {
        printf("Not found.\n");
    }

    CloseLibrary(UtilityBase);
    return 0;
}
