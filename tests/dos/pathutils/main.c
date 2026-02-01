/*
 * Test for DOS path utility functions: FilePart, PathPart, AddPart
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>

#include <stdio.h>
#include <string.h>

/* Test FilePart */
void test_filepart(void)
{
    STRPTR result;
    
    printf("=== FilePart Tests ===\n");
    
    /* Simple filename */
    result = FilePart((CONST_STRPTR)"foo");
    printf("FilePart(\"foo\") = \"%s\"\n", result);
    
    /* Path with slash */
    result = FilePart((CONST_STRPTR)"dir/file");
    printf("FilePart(\"dir/file\") = \"%s\"\n", result);
    
    /* Path with multiple slashes */
    result = FilePart((CONST_STRPTR)"a/b/c/file.txt");
    printf("FilePart(\"a/b/c/file.txt\") = \"%s\"\n", result);
    
    /* Path with volume */
    result = FilePart((CONST_STRPTR)"SYS:C/dir");
    printf("FilePart(\"SYS:C/dir\") = \"%s\"\n", result);
    
    /* Just volume */
    result = FilePart((CONST_STRPTR)"SYS:");
    printf("FilePart(\"SYS:\") = \"%s\"\n", result);
    
    /* Volume and file */
    result = FilePart((CONST_STRPTR)"SYS:file");
    printf("FilePart(\"SYS:file\") = \"%s\"\n", result);
    
    /* Trailing slash */
    result = FilePart((CONST_STRPTR)"dir/subdir/");
    printf("FilePart(\"dir/subdir/\") = \"%s\"\n", result);
    
    /* Empty string */
    result = FilePart((CONST_STRPTR)"");
    printf("FilePart(\"\") = \"%s\"\n", result);
}

/* Test PathPart */
void test_pathpart(void)
{
    STRPTR result;
    
    printf("\n=== PathPart Tests ===\n");
    
    /* Simple filename - returns start (whole thing is filename) */
    result = PathPart((CONST_STRPTR)"foo");
    printf("PathPart(\"foo\") = \"%s\"\n", result);
    
    /* Path with slash - returns pointer to slash */
    result = PathPart((CONST_STRPTR)"dir/file");
    printf("PathPart(\"dir/file\") = \"%s\"\n", result);
    
    /* Multiple slashes - returns pointer to last slash */
    result = PathPart((CONST_STRPTR)"a/b/c/file.txt");
    printf("PathPart(\"a/b/c/file.txt\") = \"%s\"\n", result);
    
    /* Volume path */
    result = PathPart((CONST_STRPTR)"SYS:file");
    printf("PathPart(\"SYS:file\") = \"%s\"\n", result);
    
    /* Volume with subdirs */
    result = PathPart((CONST_STRPTR)"SYS:C/dir/file");
    printf("PathPart(\"SYS:C/dir/file\") = \"%s\"\n", result);
    
    /* Just volume */
    result = PathPart((CONST_STRPTR)"SYS:");
    printf("PathPart(\"SYS:\") = \"%s\"\n", result);
}

/* Test AddPart */
void test_addpart(void)
{
    UBYTE buffer[256];
    BOOL result;
    
    printf("\n=== AddPart Tests ===\n");
    
    /* Simple path + filename */
    strcpy((char *)buffer, "dir");
    result = AddPart(buffer, (CONST_STRPTR)"file", sizeof(buffer));
    printf("AddPart(\"dir\", \"file\") = %s, result=\"%s\"\n", 
           result ? "TRUE" : "FALSE", buffer);
    
    /* Path with trailing slash */
    strcpy((char *)buffer, "dir/");
    result = AddPart(buffer, (CONST_STRPTR)"file", sizeof(buffer));
    printf("AddPart(\"dir/\", \"file\") = %s, result=\"%s\"\n",
           result ? "TRUE" : "FALSE", buffer);
    
    /* Volume alone */
    strcpy((char *)buffer, "SYS:");
    result = AddPart(buffer, (CONST_STRPTR)"file", sizeof(buffer));
    printf("AddPart(\"SYS:\", \"file\") = %s, result=\"%s\"\n",
           result ? "TRUE" : "FALSE", buffer);
    
    /* Empty dirname */
    strcpy((char *)buffer, "");
    result = AddPart(buffer, (CONST_STRPTR)"file", sizeof(buffer));
    printf("AddPart(\"\", \"file\") = %s, result=\"%s\"\n",
           result ? "TRUE" : "FALSE", buffer);
    
    /* Nested path */
    strcpy((char *)buffer, "SYS:C/Dir");
    result = AddPart(buffer, (CONST_STRPTR)"subfile.txt", sizeof(buffer));
    printf("AddPart(\"SYS:C/Dir\", \"subfile.txt\") = %s, result=\"%s\"\n",
           result ? "TRUE" : "FALSE", buffer);
    
    /* Buffer too small */
    strcpy((char *)buffer, "path");
    result = AddPart(buffer, (CONST_STRPTR)"very_long_filename_that_wont_fit", 10);
    printf("AddPart(\"path\", \"very_long...\", 10) = %s (expected FALSE)\n",
           result ? "TRUE" : "FALSE");
}

int main(int argc, char **argv)
{
    test_filepart();
    test_pathpart();
    test_addpart();
    
    printf("\nDone.\n");
    return 0;
}
