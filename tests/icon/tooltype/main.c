/*
 * Test: icon/tooltype
 * Tests icon.library ToolType functions: FindToolType, MatchToolValue, BumpRevision
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <workbench/workbench.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/icon_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/icon.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
struct Library *IconBase;

/* Helper macros for cleaner code */
#define STR(s) ((CONST_STRPTR)(s))
#define STRARRAY(a) ((CONST_STRPTR *)(a))

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static BOOL str_equal(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        if (*s1++ != *s2++)
            return FALSE;
    }
    return *s1 == *s2;
}

int main(void)
{
    int errors = 0;
    UBYTE *result;
    BOOL match;

    print("Testing icon.library ToolType functions...\n");

    /* Open icon.library */
    IconBase = (struct Library *)OpenLibrary(STR("icon.library"), 0);
    if (!IconBase)
    {
        print("FAIL: Could not open icon.library\n");
        return 20;
    }
    print("OK: Opened icon.library\n");

    /* Set up test ToolTypes array */
    CONST_STRPTR toolTypes[] = {
        STR("PUBSCREEN=Workbench"),
        STR("DONOTWAIT"),
        STR("TOOLPRI=5"),
        STR("FILETYPE=TEXT|BINARY|SOURCE"),
        STR("(This is a comment)"),
        STR("  SPACED=value with spaces  "),
        STR("CX_POPUP=YES"),
        NULL
    };

    /*
     * Test 1: FindToolType - exact match (boolean)
     */
    print("\nTest 1: FindToolType() exact match (boolean)...\n");
    result = FindToolType(STRARRAY(toolTypes), STR("DONOTWAIT"));
    if (result != NULL)
    {
        print("OK: Found DONOTWAIT\n");
        if (result[0] == '\0')
        {
            print("OK: Returns empty string for boolean\n");
        }
        else
        {
            print("FAIL: Should return empty string for boolean\n");
            errors++;
        }
    }
    else
    {
        print("FAIL: DONOTWAIT not found\n");
        errors++;
    }

    /*
     * Test 2: FindToolType - key=value
     */
    print("\nTest 2: FindToolType() key=value...\n");
    result = FindToolType(STRARRAY(toolTypes), STR("PUBSCREEN"));
    if (result != NULL)
    {
        print("OK: Found PUBSCREEN, value='");
        print((const char *)result);
        print("'\n");
        
        if (str_equal((const char *)result, "Workbench"))
        {
            /* Value correct - no message needed */
        }
        else
        {
            print("FAIL: Value should be 'Workbench'\n");
            errors++;
        }
    }
    else
    {
        print("FAIL: PUBSCREEN not found\n");
        errors++;
    }

    /*
     * Test 3: FindToolType - case insensitive
     */
    print("\nTest 3: FindToolType() case insensitive...\n");
    result = FindToolType(STRARRAY(toolTypes), STR("pubscreen"));
    if (result != NULL)
    {
        print("OK: Found pubscreen (lowercase)\n");
    }
    else
    {
        print("FAIL: pubscreen not found (case insensitive should work)\n");
        errors++;
    }

    result = FindToolType(STRARRAY(toolTypes), STR("PubScreen"));
    if (result != NULL)
    {
        print("OK: Found PubScreen (mixed case)\n");
    }
    else
    {
        print("FAIL: PubScreen not found\n");
        errors++;
    }

    /*
     * Test 4: FindToolType - not found
     */
    print("\nTest 4: FindToolType() not found...\n");
    result = FindToolType(STRARRAY(toolTypes), STR("NONEXISTENT"));
    if (result == NULL)
    {
        print("OK: NONEXISTENT correctly not found\n");
    }
    else
    {
        print("FAIL: NONEXISTENT should not be found\n");
        errors++;
    }

    /*
     * Test 5: FindToolType - skips comments
     */
    print("\nTest 5: FindToolType() skips comments...\n");
    result = FindToolType(STRARRAY(toolTypes), STR("(This"));
    if (result == NULL)
    {
        print("OK: Comment line correctly skipped\n");
    }
    else
    {
        print("FAIL: Should not find text in comment\n");
        errors++;
    }

    /*
     * Test 6: FindToolType - handles leading spaces
     */
    print("\nTest 6: FindToolType() handles leading spaces...\n");
    result = FindToolType(STRARRAY(toolTypes), STR("SPACED"));
    if (result != NULL)
    {
        print("OK: Found SPACED despite leading spaces\n");
    }
    else
    {
        print("FAIL: SPACED not found (should handle leading spaces)\n");
        errors++;
    }

    /*
     * Test 7: FindToolType - NULL parameters
     */
    print("\nTest 7: FindToolType() NULL parameters...\n");
    result = FindToolType(NULL, STR("TEST"));
    if (result == NULL)
    {
        print("OK: NULL toolTypes returns NULL\n");
    }
    else
    {
        print("FAIL: NULL toolTypes should return NULL\n");
        errors++;
    }

    result = FindToolType(STRARRAY(toolTypes), NULL);
    if (result == NULL)
    {
        print("OK: NULL typeName returns NULL\n");
    }
    else
    {
        print("FAIL: NULL typeName should return NULL\n");
        errors++;
    }

    /*
     * Test 8: MatchToolValue - single value
     */
    print("\nTest 8: MatchToolValue() single value...\n");
    match = MatchToolValue(STR("TEXT"), STR("TEXT"));
    if (match)
    {
        print("OK: TEXT matches TEXT\n");
    }
    else
    {
        print("FAIL: TEXT should match TEXT\n");
        errors++;
    }

    /*
     * Test 9: MatchToolValue - pipe-separated values
     */
    print("\nTest 9: MatchToolValue() pipe-separated...\n");
    match = MatchToolValue(STR("TEXT|BINARY|SOURCE"), STR("BINARY"));
    if (match)
    {
        print("OK: Found BINARY in TEXT|BINARY|SOURCE\n");
    }
    else
    {
        print("FAIL: BINARY should match in TEXT|BINARY|SOURCE\n");
        errors++;
    }

    match = MatchToolValue(STR("TEXT|BINARY|SOURCE"), STR("SOURCE"));
    if (match)
    {
        print("OK: Found SOURCE in TEXT|BINARY|SOURCE\n");
    }
    else
    {
        print("FAIL: SOURCE should match in TEXT|BINARY|SOURCE\n");
        errors++;
    }

    match = MatchToolValue(STR("TEXT|BINARY|SOURCE"), STR("TEXT"));
    if (match)
    {
        print("OK: Found TEXT in TEXT|BINARY|SOURCE\n");
    }
    else
    {
        print("FAIL: TEXT should match in TEXT|BINARY|SOURCE\n");
        errors++;
    }

    /*
     * Test 10: MatchToolValue - case insensitive
     */
    print("\nTest 10: MatchToolValue() case insensitive...\n");
    match = MatchToolValue(STR("TEXT|BINARY"), STR("text"));
    if (match)
    {
        print("OK: text matches TEXT (case insensitive)\n");
    }
    else
    {
        print("FAIL: text should match TEXT\n");
        errors++;
    }

    /*
     * Test 11: MatchToolValue - not found
     */
    print("\nTest 11: MatchToolValue() not found...\n");
    match = MatchToolValue(STR("TEXT|BINARY"), STR("HEXDUMP"));
    if (!match)
    {
        print("OK: HEXDUMP correctly not found in TEXT|BINARY\n");
    }
    else
    {
        print("FAIL: HEXDUMP should not match TEXT|BINARY\n");
        errors++;
    }

    /*
     * Test 12: MatchToolValue - NULL parameters
     */
    print("\nTest 12: MatchToolValue() NULL parameters...\n");
    match = MatchToolValue(NULL, STR("TEXT"));
    if (!match)
    {
        print("OK: NULL typeString returns FALSE\n");
    }
    else
    {
        print("FAIL: NULL typeString should return FALSE\n");
        errors++;
    }

    match = MatchToolValue(STR("TEXT"), NULL);
    if (!match)
    {
        print("OK: NULL value returns FALSE\n");
    }
    else
    {
        print("FAIL: NULL value should return FALSE\n");
        errors++;
    }

    /*
     * Test 13: BumpRevision - new copy
     */
    print("\nTest 13: BumpRevision() new copy...\n");
    {
        UBYTE newname[64];
        STRPTR res = BumpRevision(newname, STR("MyFile"));
        if (res)
        {
            print("OK: BumpRevision returned '");
            print((const char *)newname);
            print("'\n");
            
            if (str_equal((const char *)newname, "copy_of_MyFile"))
            {
                print("OK: Result is correct\n");
            }
            else
            {
                print("FAIL: Expected 'copy_of_MyFile'\n");
                errors++;
            }
        }
        else
        {
            print("FAIL: BumpRevision returned NULL\n");
            errors++;
        }
    }

    /*
     * Test 14: BumpRevision - first numbered copy
     */
    print("\nTest 14: BumpRevision() first numbered...\n");
    {
        UBYTE newname[64];
        STRPTR res = BumpRevision(newname, STR("copy_of_MyFile"));
        if (res)
        {
            print("OK: BumpRevision returned '");
            print((const char *)newname);
            print("'\n");
            
            if (str_equal((const char *)newname, "copy_2_of_MyFile"))
            {
                print("OK: Result is correct\n");
            }
            else
            {
                print("FAIL: Expected 'copy_2_of_MyFile'\n");
                errors++;
            }
        }
        else
        {
            print("FAIL: BumpRevision returned NULL\n");
            errors++;
        }
    }

    /*
     * Test 15: BumpRevision - increment number
     */
    print("\nTest 15: BumpRevision() increment number...\n");
    {
        UBYTE newname[64];
        STRPTR res = BumpRevision(newname, STR("copy_5_of_MyFile"));
        if (res)
        {
            print("OK: BumpRevision returned '");
            print((const char *)newname);
            print("'\n");
            
            if (str_equal((const char *)newname, "copy_6_of_MyFile"))
            {
                print("OK: Result is correct\n");
            }
            else
            {
                print("FAIL: Expected 'copy_6_of_MyFile'\n");
                errors++;
            }
        }
        else
        {
            print("FAIL: BumpRevision returned NULL\n");
            errors++;
        }
    }

    /*
     * Test 16: GetDefDiskObject
     */
    print("\nTest 16: GetDefDiskObject()...\n");
    {
        struct DiskObject *dobj = GetDefDiskObject(WBPROJECT);
        if (dobj)
        {
            print("OK: GetDefDiskObject returned non-NULL\n");
            
            if (dobj->do_Magic == WB_DISKMAGIC)
            {
                print("OK: Magic number is correct\n");
            }
            else
            {
                print("FAIL: Magic number is wrong\n");
                errors++;
            }
            
            if (dobj->do_Type == WBPROJECT)
            {
                print("OK: Type is WBPROJECT\n");
            }
            else
            {
                print("FAIL: Type should be WBPROJECT\n");
                errors++;
            }
            
            FreeDiskObject(dobj);
            print("OK: FreeDiskObject completed\n");
        }
        else
        {
            print("FAIL: GetDefDiskObject returned NULL\n");
            errors++;
        }
    }

    /*
     * Test 17: FreeDiskObject with NULL
     */
    print("\nTest 17: FreeDiskObject() NULL...\n");
    FreeDiskObject(NULL);
    print("OK: FreeDiskObject(NULL) didn't crash\n");

    /* Close library */
    CloseLibrary(IconBase);
    print("OK: Closed icon.library\n");

    /* Final result */
    if (errors == 0)
    {
        print("\nPASS: icon/tooltype all tests passed\n");
        return 0;
    }
    else
    {
        print("\nFAIL: icon/tooltype had errors\n");
        return 20;
    }
}
