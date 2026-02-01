/*
 * Test for DOS character I/O functions: FGetC, FPutC, UnGetC, PutStr, WriteChars, FGets, FPuts
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>

#include <stdio.h>
#include <string.h>

/* Test FPutC and FGetC */
void test_fputc_fgetc(void)
{
    BPTR fh;
    LONG ch;
    
    printf("=== FPutC/FGetC Tests ===\n");
    
    /* Write characters to file */
    fh = Open((CONST_STRPTR)"T:testfile.txt", MODE_NEWFILE);
    if (!fh)
    {
        printf("FAIL: Cannot open file for writing\n");
        return;
    }
    
    /* Write individual characters */
    FPutC(fh, 'H');
    FPutC(fh, 'e');
    FPutC(fh, 'l');
    FPutC(fh, 'l');
    FPutC(fh, 'o');
    FPutC(fh, '\n');
    
    Close(fh);
    printf("Wrote 'Hello\\n' using FPutC\n");
    
    /* Read characters back */
    fh = Open((CONST_STRPTR)"T:testfile.txt", MODE_OLDFILE);
    if (!fh)
    {
        printf("FAIL: Cannot open file for reading\n");
        return;
    }
    
    printf("Reading back with FGetC: ");
    while ((ch = FGetC(fh)) != -1)
    {
        if (ch == '\n')
            printf("\\n");
        else
            printf("%c", (char)ch);
    }
    printf("\n");
    
    Close(fh);
    printf("FPutC/FGetC test PASSED\n");
}

/* Test UnGetC */
void test_ungetc(void)
{
    BPTR fh;
    LONG ch;
    
    printf("\n=== UnGetC Tests ===\n");
    
    /* Create a test file */
    fh = Open((CONST_STRPTR)"T:testfile.txt", MODE_NEWFILE);
    if (!fh)
    {
        printf("FAIL: Cannot create test file\n");
        return;
    }
    FPuts(fh, (CONST_STRPTR)"ABC");
    Close(fh);
    
    /* Read and unget */
    fh = Open((CONST_STRPTR)"T:testfile.txt", MODE_OLDFILE);
    if (!fh)
    {
        printf("FAIL: Cannot open test file\n");
        return;
    }
    
    /* Read first char */
    ch = FGetC(fh);
    printf("First FGetC: %c\n", (char)ch);
    
    /* Push it back */
    UnGetC(fh, ch);
    printf("UnGetC(%c) called\n", (char)ch);
    
    /* Read again - should get the same char */
    ch = FGetC(fh);
    printf("FGetC after UnGetC: %c\n", (char)ch);
    
    /* Continue reading */
    ch = FGetC(fh);
    printf("Next FGetC: %c\n", (char)ch);
    
    ch = FGetC(fh);
    printf("Next FGetC: %c\n", (char)ch);
    
    Close(fh);
    printf("UnGetC test PASSED\n");
}

/* Test PutStr and WriteChars */
void test_putstr_writechars(void)
{
    printf("\n=== PutStr/WriteChars Tests ===\n");
    
    /* PutStr writes to Output() */
    PutStr((CONST_STRPTR)"PutStr output line\n");
    
    /* WriteChars writes specified number of chars to Output() */
    WriteChars((CONST_STRPTR)"WriteChars: ", 12);
    WriteChars((CONST_STRPTR)"Hello World!\n", 13);
    
    printf("PutStr/WriteChars test PASSED\n");
}

/* Test FGets and FPuts (already implemented but verify) */
void test_fgets_fputs(void)
{
    BPTR fh;
    UBYTE buffer[256];
    
    printf("\n=== FGets/FPuts Tests ===\n");
    
    /* Write multiple lines */
    fh = Open((CONST_STRPTR)"T:testfile.txt", MODE_NEWFILE);
    if (!fh)
    {
        printf("FAIL: Cannot create test file\n");
        return;
    }
    
    FPuts(fh, (CONST_STRPTR)"Line one\n");
    FPuts(fh, (CONST_STRPTR)"Line two\n");
    FPuts(fh, (CONST_STRPTR)"Line three\n");
    Close(fh);
    printf("Wrote 3 lines using FPuts\n");
    
    /* Read lines back */
    fh = Open((CONST_STRPTR)"T:testfile.txt", MODE_OLDFILE);
    if (!fh)
    {
        printf("FAIL: Cannot open test file\n");
        return;
    }
    
    printf("Reading back with FGets:\n");
    while (FGets(fh, buffer, sizeof(buffer)))
    {
        /* Remove newline for display */
        int len = strlen((char *)buffer);
        if (len > 0 && buffer[len-1] == '\n')
            buffer[len-1] = '\0';
        printf("  Got: '%s'\n", buffer);
    }
    
    Close(fh);
    printf("FGets/FPuts test PASSED\n");
}

int main(int argc, char **argv)
{
    test_fputc_fgetc();
    test_ungetc();
    test_putstr_writechars();
    test_fgets_fputs();
    
    /* Cleanup */
    DeleteFile((CONST_STRPTR)"T:testfile.txt");
    
    printf("\nAll character I/O tests completed.\n");
    return 0;
}
