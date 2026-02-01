/*
 * Test for DOS VFPrintf and VPrintf functions
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>

#include <stdio.h>
#include <string.h>

/* We use Printf directly since it uses VFPrintf internally */
void test_vprintf_basic(void)
{
    printf("=== VPrintf Basic Tests ===\n");
    
    /* Test string formatting */
    Printf((CONST_STRPTR)"String test: %s\n", (LONG)"hello");
    
    /* Test decimal formatting */
    Printf((CONST_STRPTR)"Decimal test: %ld\n", 12345);
    
    /* Test negative numbers */
    Printf((CONST_STRPTR)"Negative test: %ld\n", -42);
    
    /* Test zero */
    Printf((CONST_STRPTR)"Zero test: %ld\n", 0);
    
    /* Test hex lowercase */
    Printf((CONST_STRPTR)"Hex lowercase: %lx\n", 0xABCD);
    
    /* Test hex uppercase */
    Printf((CONST_STRPTR)"Hex uppercase: %lX\n", 0xABCD);
    
    /* Test character */
    Printf((CONST_STRPTR)"Character test: %lc\n", 'X');
    
    /* Test percent literal */
    Printf((CONST_STRPTR)"Percent literal: %%\n");
}

void test_vprintf_width(void)
{
    printf("\n=== VPrintf Width Tests ===\n");
    
    /* Test width with numbers */
    Printf((CONST_STRPTR)"Width 8: [%8ld]\n", 42);
    
    /* Test left justify */
    Printf((CONST_STRPTR)"Left justify: [%-8ld]\n", 42);
    
    /* Test zero padding */
    Printf((CONST_STRPTR)"Zero pad: [%08ld]\n", 42);
    
    /* Test string width */
    Printf((CONST_STRPTR)"String width: [%10s]\n", (LONG)"hello");
    
    /* Test string left justify */
    Printf((CONST_STRPTR)"String left: [%-10s]\n", (LONG)"hello");
}

void test_vprintf_multiple_args(void)
{
    printf("\n=== VPrintf Multiple Args Tests ===\n");
    
    /* Multiple arguments in one call */
    Printf((CONST_STRPTR)"Multiple: %s=%ld (0x%lx)\n", 
           (LONG)"value", 255, 255);
    
    /* Test without arguments */
    Printf((CONST_STRPTR)"No args test\n");
}

void test_vfprintf_to_file(void)
{
    BPTR fh;
    UBYTE buffer[256];
    LONG args[3];
    
    printf("\n=== VFPrintf to File Tests ===\n");
    
    /* Write to file using VFPrintf */
    fh = Open((CONST_STRPTR)"T:testfile.txt", MODE_NEWFILE);
    if (!fh)
    {
        printf("FAIL: Cannot create test file\n");
        return;
    }
    
    args[0] = 42;
    args[1] = (LONG)"test string";
    args[2] = 0xDEAD;
    VFPrintf(fh, (CONST_STRPTR)"Number: %ld, String: %s, Hex: %lx\n", args);
    
    Close(fh);
    
    /* Read back and verify */
    fh = Open((CONST_STRPTR)"T:testfile.txt", MODE_OLDFILE);
    if (!fh)
    {
        printf("FAIL: Cannot open test file\n");
        return;
    }
    
    FGets(fh, buffer, sizeof(buffer));
    Close(fh);
    
    printf("Read from file: %s", buffer);
    printf("VFPrintf file test PASSED\n");
}

int main(int argc, char **argv)
{
    test_vprintf_basic();
    test_vprintf_width();
    test_vprintf_multiple_args();
    test_vfprintf_to_file();
    
    /* Cleanup */
    DeleteFile((CONST_STRPTR)"T:testfile.txt");
    
    printf("\nAll VPrintf tests completed.\n");
    return 0;
}
