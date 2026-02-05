/* EasyRequest.c - Adapted from RKM sample
 * Demonstrates EasyRequest() with variable substitution
 * Modified for automated testing (returns automatically in headless mode)
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase = NULL;

/* EasyStruct with multiple lines and variable substitution */
struct EasyStruct simpleES =
{
    sizeof(struct EasyStruct),
    0,
    "Simple Requester",
    "This is a simple request",
    "OK",
};

/* EasyStruct with text variable substitution */
struct EasyStruct textVarES =
{
    sizeof(struct EasyStruct),
    0,
    "Text Variable Test",
    "Message with variable: %s",
    "OK",
};

/* EasyStruct with numeric variable substitution */
struct EasyStruct numVarES =
{
    sizeof(struct EasyStruct),
    0,
    "Number Variable Test",
    "The answer is %ld",
    "OK",
};

/* EasyStruct with multiple buttons */
struct EasyStruct multiButtonES =
{
    sizeof(struct EasyStruct),
    0,
    "Multi-Button Test",
    "Choose an option:",
    "Yes|Maybe|No",
};

/* EasyStruct with combined features (like RKM example) */
struct EasyStruct combinedES =
{
    sizeof(struct EasyStruct),
    0,
    "Combined Features",
    "Text for the request\nSecond line of %s text\nThird line of text",
    "Yes|%ld|No",
};

int main(int argc, char **argv)
{
    LONG answer;
    LONG testNumber = 3125794;
    
    printf("EasyRequest: Demonstrating EasyRequestArgs()\n\n");
    
    /* Open Intuition Library */
    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
    {
        printf("EasyRequest: ERROR - Can't open intuition.library v37\n");
        return 20;
    }
    printf("EasyRequest: Opened intuition.library v%d\n", IntuitionBase->lib_Version);
    
    /* Test 1: Simple request with no variables */
    printf("\nEasyRequest: Test 1 - Simple request...\n");
    printf("  Title: '%s'\n", (char *)simpleES.es_Title);
    printf("  Text: '%s'\n", (char *)simpleES.es_TextFormat);
    printf("  Gadgets: '%s'\n", (char *)simpleES.es_GadgetFormat);
    answer = EasyRequest(NULL, &simpleES, NULL, NULL);
    printf("  Result: %ld\n", answer);
    
    /* Test 2: Request with string variable */
    printf("\nEasyRequest: Test 2 - String variable...\n");
    printf("  Title: '%s'\n", (char *)textVarES.es_Title);
    printf("  Text format: '%s'\n", (char *)textVarES.es_TextFormat);
    printf("  Gadgets: '%s'\n", (char *)textVarES.es_GadgetFormat);
    printf("  Variable: 'Hello World'\n");
    answer = EasyRequest(NULL, &textVarES, NULL, "Hello World");
    printf("  Result: %ld\n", answer);
    
    /* Test 3: Request with numeric variable */
    printf("\nEasyRequest: Test 3 - Numeric variable...\n");
    printf("  Title: '%s'\n", (char *)numVarES.es_Title);
    printf("  Text format: '%s'\n", (char *)numVarES.es_TextFormat);
    printf("  Gadgets: '%s'\n", (char *)numVarES.es_GadgetFormat);
    printf("  Variable: 42\n");
    answer = EasyRequest(NULL, &numVarES, NULL, 42L);
    printf("  Result: %ld\n", answer);
    
    /* Test 4: Request with multiple buttons */
    printf("\nEasyRequest: Test 4 - Multiple buttons...\n");
    printf("  Title: '%s'\n", (char *)multiButtonES.es_Title);
    printf("  Text: '%s'\n", (char *)multiButtonES.es_TextFormat);
    printf("  Gadgets: '%s' (3 buttons)\n", (char *)multiButtonES.es_GadgetFormat);
    answer = EasyRequest(NULL, &multiButtonES, NULL, NULL);
    printf("  Result: %ld (note: 0=rightmost, 1=leftmost, 2=middle)\n", answer);
    
    /* Test 5: Combined features like RKM example */
    printf("\nEasyRequest: Test 5 - Combined features...\n");
    printf("  Title: '%s'\n", (char *)combinedES.es_Title);
    printf("  Text format: '%s'\n", (char *)combinedES.es_TextFormat);
    printf("  Gadgets: '%s'\n", (char *)combinedES.es_GadgetFormat);
    printf("  String var: '(Variable)', Number var: %ld\n", testNumber);
    answer = EasyRequest(NULL, &combinedES, NULL, "(Variable)", testNumber);
    printf("  Result: %ld\n", answer);
    
    /* Verify EasyStruct structure size */
    printf("\nEasyRequest: Verifying EasyStruct...\n");
    printf("  sizeof(struct EasyStruct): %ld\n", (LONG)sizeof(struct EasyStruct));
    printf("  simpleES.es_StructSize: %ld\n", (LONG)simpleES.es_StructSize);
    if (simpleES.es_StructSize == sizeof(struct EasyStruct))
    {
        printf("  Structure size: CORRECT\n");
    }
    else
    {
        printf("  Structure size: MISMATCH!\n");
    }
    
    /* Clean up */
    printf("\nEasyRequest: Closing library...\n");
    CloseLibrary(IntuitionBase);
    
    printf("\nEasyRequest: Demo complete.\n");
    return 0;
}
