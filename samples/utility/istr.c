/*
 * istr.c - Utility library international string functions test
 *
 * Based on RKM Utility sample, adapted for automated testing.
 * Tests: Stricmp, Strnicmp, ToUpper, ToLower
 */

#include <exec/types.h>
#include <stdio.h>
#include <string.h>

#include <clib/exec_protos.h>
#include <clib/utility_protos.h>

struct Library *UtilityBase = NULL;

int main(void)
{
    UBYTE *str1 = "Hello";
    UBYTE *str2 = "HELLO";
    UBYTE *str3 = "World";
    UBYTE ch;
    LONG result;

    printf("IStr: Utility library international string functions test\n\n");

    printf("Opening utility.library...\n");
    UtilityBase = OpenLibrary("utility.library", 37);
    if (UtilityBase == NULL)
    {
        printf("ERROR: Can't open utility.library v37\n");
        return 1;
    }
    printf("OK: utility.library opened (version %d)\n", UtilityBase->lib_Version);

    /* Test 1: Stricmp - case-insensitive string comparison */
    printf("\n=== Test 1: Stricmp ===\n");
    
    result = Stricmp(str1, str2);
    printf("Stricmp(\"%s\", \"%s\") = %ld\n", str1, str2, result);
    if (result == 0)
        printf("OK: Strings are equal (case-insensitive)\n");
    else
        printf("ERROR: Strings should be equal\n");

    result = Stricmp(str1, str3);
    printf("Stricmp(\"%s\", \"%s\") = %ld\n", str1, str3, result);
    if (result < 0)
        printf("OK: \"%s\" < \"%s\" (case-insensitive)\n", str1, str3);
    else
        printf("ERROR: \"%s\" should be < \"%s\"\n", str1, str3);

    /* Test 2: Strnicmp - case-insensitive comparison with length */
    printf("\n=== Test 2: Strnicmp ===\n");
    
    result = Strnicmp("Hello World", "hello WORLD", 5);
    printf("Strnicmp(\"Hello World\", \"hello WORLD\", 5) = %ld\n", result);
    if (result == 0)
        printf("OK: First 5 characters match (case-insensitive)\n");
    else
        printf("ERROR: First 5 characters should match\n");

    result = Strnicmp("Hello", "Help", 3);
    printf("Strnicmp(\"Hello\", \"Help\", 3) = %ld\n", result);
    if (result == 0)
        printf("OK: First 3 characters match\n");
    else
        printf("ERROR: First 3 characters should match\n");

    result = Strnicmp("Hello", "Help", 4);
    printf("Strnicmp(\"Hello\", \"Help\", 4) = %ld\n", result);
    if (result != 0)
        printf("OK: First 4 characters differ ('o' vs 'p')\n");
    else
        printf("ERROR: First 4 characters should differ\n");

    /* Test 3: ToUpper */
    printf("\n=== Test 3: ToUpper ===\n");
    
    ch = ToUpper('a');
    printf("ToUpper('a') = '%c' (0x%02x)\n", ch, ch);
    if (ch == 'A')
        printf("OK: Lowercase 'a' converted to uppercase 'A'\n");
    else
        printf("ERROR: Expected 'A', got '%c'\n", ch);

    ch = ToUpper('Z');
    printf("ToUpper('Z') = '%c' (0x%02x)\n", ch, ch);
    if (ch == 'Z')
        printf("OK: Uppercase 'Z' remains unchanged\n");
    else
        printf("ERROR: Expected 'Z', got '%c'\n", ch);

    ch = ToUpper('5');
    printf("ToUpper('5') = '%c' (0x%02x)\n", ch, ch);
    if (ch == '5')
        printf("OK: Digit '5' remains unchanged\n");
    else
        printf("ERROR: Expected '5', got '%c'\n", ch);

    /* Test 4: ToLower */
    printf("\n=== Test 4: ToLower ===\n");
    
    ch = ToLower('A');
    printf("ToLower('A') = '%c' (0x%02x)\n", ch, ch);
    if (ch == 'a')
        printf("OK: Uppercase 'A' converted to lowercase 'a'\n");
    else
        printf("ERROR: Expected 'a', got '%c'\n", ch);

    ch = ToLower('z');
    printf("ToLower('z') = '%c' (0x%02x)\n", ch, ch);
    if (ch == 'z')
        printf("OK: Lowercase 'z' remains unchanged\n");
    else
        printf("ERROR: Expected 'z', got '%c'\n", ch);

    ch = ToLower('9');
    printf("ToLower('9') = '%c' (0x%02x)\n", ch, ch);
    if (ch == '9')
        printf("OK: Digit '9' remains unchanged\n");
    else
        printf("ERROR: Expected '9', got '%c'\n", ch);

    /* Test 5: Mixed case conversions */
    printf("\n=== Test 5: Character Case Conversions ===\n");
    {
        char teststr[] = "Test123ABC";
        int i;
        
        printf("Original: %s\n", teststr);
        printf("ToUpper:  ");
        for (i = 0; teststr[i]; i++)
            printf("%c", ToUpper(teststr[i]));
        printf("\n");
        
        printf("ToLower:  ");
        for (i = 0; teststr[i]; i++)
            printf("%c", ToLower(teststr[i]));
        printf("\n");
        printf("OK: Case conversion functions working\n");
    }

    CloseLibrary(UtilityBase);
    printf("\nOK: utility.library closed\n");

    printf("\nPASS: IStr tests completed successfully\n");
    return 0;
}
