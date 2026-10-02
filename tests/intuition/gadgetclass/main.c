/*
 * Test: intuition/gadgetclass
 * Creates the public gadget classes and reports which attributes OM_GET
 * (GetAttr()) supports.  Validated against AmigaOS 3.1: the output lists
 * GetAttr()'s result for every attribute, so the set of gettable attributes
 * is compared with the reference.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *IntuitionBase;

static void show(Object *obj, ULONG attr, const char *name, BOOL is_string)
{
    ULONG value = 0;

    if (GetAttr(attr, obj, &value)) {
        if (is_string)
            printf("  GetAttr(%s) = '%s'\n", name, value ? (char *)value : "(null)");
        else
            printf("  GetAttr(%s) = %lu\n", name, value);
    } else {
        printf("  GetAttr(%s) not supported\n", name);
    }
}

#define SHOW(obj, attr) show(obj, attr, #attr, FALSE)
#define SHOWSTR(obj, attr) show(obj, attr, #attr, TRUE)

int main(void)
{
    Object *obj;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!IntuitionBase) {
        printf("Failed to open intuition.library\n");
        return 1;
    }

    printf("Testing GadgetClass...\n");

    printf("Creating gadgetclass object...\n");
    obj = NewObject(NULL, (CONST_STRPTR)"gadgetclass",
        GA_Left, 10,
        GA_Top, 20,
        GA_Width, 100,
        GA_Height, 30,
        GA_ID, 1,
        GA_UserData, 1234,
        TAG_END);
    if (!obj) {
        printf("Failed to create gadgetclass object\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("gadgetclass object created\n");
    SHOW(obj, GA_Left);
    SHOW(obj, GA_Top);
    SHOW(obj, GA_Width);
    SHOW(obj, GA_Height);
    SHOW(obj, GA_ID);
    SHOW(obj, GA_UserData);
    SHOW(obj, GA_Disabled);
    SHOW(obj, GA_Selected);
    DisposeObject(obj);
    printf("gadgetclass object disposed\n");

    printf("\nCreating buttongclass object...\n");
    obj = NewObject(NULL, (CONST_STRPTR)"buttongclass",
        GA_Left, 50,
        GA_Top, 50,
        GA_Width, 80,
        GA_Height, 20,
        GA_ID, 2,
        GA_RelVerify, TRUE,
        TAG_END);
    if (!obj) {
        printf("Failed to create buttongclass object\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("buttongclass object created\n");
    SHOW(obj, GA_ID);
    SHOW(obj, GA_Disabled);
    SHOW(obj, GA_Selected);
    DisposeObject(obj);
    printf("buttongclass object disposed\n");

    printf("\nCreating propgclass object...\n");
    obj = NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_Left, 100,
        GA_Top, 100,
        GA_Width, 20,
        GA_Height, 100,
        GA_ID, 3,
        PGA_Total, 100,
        PGA_Top, 25,
        PGA_Visible, 10,
        TAG_END);
    if (!obj) {
        printf("Failed to create propgclass object\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("propgclass object created\n");
    SHOW(obj, GA_ID);
    SHOW(obj, PGA_Top);
    SHOW(obj, PGA_Visible);
    SHOW(obj, PGA_Total);
    SHOW(obj, PGA_Freedom);
    SHOW(obj, GA_Disabled);
    DisposeObject(obj);
    printf("propgclass object disposed\n");

    printf("\nCreating strgclass object...\n");
    obj = NewObject(NULL, (CONST_STRPTR)"strgclass",
        GA_Left, 150,
        GA_Top, 150,
        GA_Width, 200,
        GA_Height, 15,
        GA_ID, 4,
        STRINGA_MaxChars, 20,
        STRINGA_TextVal, (ULONG)"hello",
        TAG_END);
    if (!obj) {
        printf("Failed to create strgclass object\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("strgclass object created\n");
    SHOW(obj, GA_ID);
    SHOWSTR(obj, STRINGA_TextVal);
    SHOW(obj, STRINGA_LongVal);
    SHOW(obj, STRINGA_MaxChars);
    SHOW(obj, STRINGA_BufferPos);
    SHOW(obj, GA_Disabled);
    DisposeObject(obj);
    printf("strgclass object disposed\n");

    printf("\nAll gadget class tests passed!\n");
    CloseLibrary(IntuitionBase);
    return 0;
}
