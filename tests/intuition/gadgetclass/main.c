
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

int main(void)
{
    struct Library *IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!IntuitionBase) {
        printf("Failed to open intuition.library\n");
        return 1;
    }

    printf("Testing GadgetClass...\n");
    
    /* Test gadgetclass */
    printf("Creating gadgetclass object...\n");
    Object *gadget = NewObject(NULL, (CONST_STRPTR)"gadgetclass",
        GA_Left, 10,
        GA_Top, 20,
        GA_Width, 100,
        GA_Height, 30,
        GA_ID, 1,
        TAG_END);
    if (!gadget) {
        printf("Failed to create gadgetclass object\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("gadgetclass object created at 0x\n");
    
    /* Verify attributes */
    ULONG value = 0;
    if (GetAttr(GA_ID, gadget, &value)) {
        printf("GA_ID = %u\n", value);
    }
    
    DisposeObject(gadget);
    printf("gadgetclass object disposed\n");
    
    /* Test buttongclass */
    printf("\nCreating buttongclass object...\n");
    Object *button = NewObject(NULL, (CONST_STRPTR)"buttongclass",
        GA_Left, 50,
        GA_Top, 50,
        GA_Width, 80,
        GA_Height, 20,
        GA_ID, 2,
        GA_RelVerify, TRUE,
        TAG_END);
    if (!button) {
        printf("Failed to create buttongclass object\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("buttongclass object created at 0x\n");
    
    DisposeObject(button);
    printf("buttongclass object disposed\n");
    
    /* Test propgclass */
    printf("\nCreating propgclass object...\n");
    Object *prop = NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_Left, 100,
        GA_Top, 100,
        GA_Width, 20,
        GA_Height, 100,
        GA_ID, 3,
        TAG_END);
    if (!prop) {
        printf("Failed to create propgclass object\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("propgclass object created at 0x\n");
    
    DisposeObject(prop);
    printf("propgclass object disposed\n");
    
    /* Test strgclass */
    printf("\nCreating strgclass object...\n");
    Object *string = NewObject(NULL, (CONST_STRPTR)"strgclass",
        GA_Left, 150,
        GA_Top, 150,
        GA_Width, 200,
        GA_Height, 15,
        GA_ID, 4,
        TAG_END);
    if (!string) {
        printf("Failed to create strgclass object\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("strgclass object created at 0x\n");
    
    DisposeObject(string);
    printf("strgclass object disposed\n");
    
    printf("\nAll gadget class tests passed!\n");
    CloseLibrary(IntuitionBase);
    return 0;
}
