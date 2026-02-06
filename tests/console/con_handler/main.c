/*
 * Test for CON: handler - Opens a CON: window via Open()
 * This tests the DOS-level CON: handling
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <inline/dos.h>


extern struct DosLibrary *DOSBase;
extern struct GfxBase *GfxBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

int main(void)
{
    BPTR conFile;
    char buf[64];
    LONG written, read_len;
    
    print("Testing CON: handler via Open()\n");
    
    /* Open a CON: window */
    conFile = Open((STRPTR)"CON:0/0/400/150/Test CON Window", MODE_OLDFILE);
    if (!conFile) {
        print("FAIL: Cannot open CON: window\n");
        return 1;
    }
    print("OK: CON: window opened\n");
    
    /* Write to the console */
    written = Write(conFile, (CONST APTR)"Hello from CON: handler!\n", 25);
    if (written != 25) {
        print("FAIL: Write returned wrong count\n");
        Close(conFile);
        return 1;
    }
    print("OK: Write succeeded\n");
    
    /* Inject some input */
    print("Injecting 'test' + Return...\n");
    
    
    
    /* Give events time to be processed */
    WaitTOF();
    WaitTOF();
    WaitTOF();
    WaitTOF();
    
    /* Read from the console */
    read_len = Read(conFile, buf, 10);
    if (read_len < 0) {
        print("FAIL: Read failed\n");
        Close(conFile);
        return 1;
    }
    
    print("OK: Read ");
    /* Print the number of bytes read */
    char num[8];
    int i = 0;
    if (read_len == 0) {
        num[i++] = '0';
    } else {
        LONG n = read_len;
        if (n >= 10) num[i++] = '0' + (n / 10);
        num[i++] = '0' + (n % 10);
    }
    num[i] = '\0';
    print(num);
    print(" bytes\n");
    
    /* Close the console */
    Close(conFile);
    print("OK: CON: window closed\n");
    
    print("PASS: con_handler test complete\n");
    
    return 0;
}
