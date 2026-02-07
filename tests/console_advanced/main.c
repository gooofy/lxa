/*
 * Test: console_advanced
 *
 * Tests advanced console.device features:
 * 1. CONU_CHARMAP mode support
 * 2. RawKeyConvert function
 * 3. CSI 'r' (DECSTBM) scrolling region
 * 4. CSI '{' (Amiga) scrolling region
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <devices/keymap.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/console_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG val)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    BOOL neg = FALSE;
    
    if (val < 0) {
        neg = TRUE;
        val = -val;
    }
    if (val == 0) {
        *--p = '0';
    } else {
        while (val > 0) {
            *--p = '0' + (val % 10);
            val /= 10;
        }
    }
    if (neg) *--p = '-';
    print(p);
}

int main(void)
{
    struct Library *ConsoleDevice = NULL;
    struct IOStdReq *consoleReq = NULL;
    struct MsgPort *consolePort = NULL;
    LONG error = 0;
    
    print("console_advanced test: Testing advanced console features\n");
    
    /* Test 1: Open console.device with CONU_STANDARD */
    print("\n1. Testing CONU_STANDARD mode...\n");
    
    consolePort = CreateMsgPort();
    if (!consolePort) {
        print("ERROR: Failed to create message port\n");
        return 20;
    }
    
    consoleReq = (struct IOStdReq *)CreateIORequest(consolePort, sizeof(struct IOStdReq));
    if (!consoleReq) {
        print("ERROR: Failed to create IO request\n");
        DeleteMsgPort(consolePort);
        return 20;
    }
    
    /* Open with CONU_STANDARD (unit 0) */
    consoleReq->io_Data = NULL;  /* No window for this test */
    error = OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)consoleReq, 0);
    if (error) {
        print("ERROR: Failed to open console.device with CONU_STANDARD, error=");
        print_num(error);
        print("\n");
        DeleteIORequest((struct IORequest *)consoleReq);
        DeleteMsgPort(consolePort);
        return 20;
    }
    
    ConsoleDevice = (struct Library *)consoleReq->io_Device;
    print("OK: Opened console.device with CONU_STANDARD\n");
    
    CloseDevice((struct IORequest *)consoleReq);
    
    /* Test 2: Open console.device with CONU_CHARMAP */
    print("\n2. Testing CONU_CHARMAP mode...\n");
    
    consoleReq->io_Data = NULL;  /* No window for this test */
    error = OpenDevice((STRPTR)"console.device", CONU_CHARMAP, (struct IORequest *)consoleReq, 0);
    if (error) {
        print("ERROR: Failed to open console.device with CONU_CHARMAP, error=");
        print_num(error);
        print("\n");
        DeleteIORequest((struct IORequest *)consoleReq);
        DeleteMsgPort(consolePort);
        return 20;
    }
    
    print("OK: Opened console.device with CONU_CHARMAP\n");
    
    CloseDevice((struct IORequest *)consoleReq);
    
    /* Test 3: RawKeyConvert function */
    print("\n3. Testing RawKeyConvert...\n");
    
    /* Reopen console device to use RawKeyConvert */
    error = OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)consoleReq, 0);
    if (error) {
        print("ERROR: Failed to reopen console.device\n");
        DeleteIORequest((struct IORequest *)consoleReq);
        DeleteMsgPort(consolePort);
        return 20;
    }
    
    ConsoleDevice = (struct Library *)consoleReq->io_Device;
    
    /* Create a fake InputEvent for rawkey 0x20 ('a' key) */
    struct InputEvent ie;
    ie.ie_NextEvent = NULL;
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_SubClass = 0;
    ie.ie_Code = 0x20;  /* Rawkey for 'a' */
    ie.ie_Qualifier = 0;  /* No qualifiers */
    ie.ie_X = 0;
    ie.ie_Y = 0;
    ie.ie_TimeStamp.tv_secs = 0;
    ie.ie_TimeStamp.tv_micro = 0;
    
    char buffer[16];
    LONG result;
    
    /* Call RawKeyConvert via library vector at -48 (LVO)
     * console.device bias=42: CDInputHandler=-42, RawKeyConvert=-48
     * RawKeyConvert(events, buffer, length, keyMap) (a0/a1,d1/a2)
     * Pass NULL for keyMap to use default keymap */
    {
        register LONG _result __asm("d0");
        register struct Library *_a6 __asm("a6") = ConsoleDevice;
        register struct InputEvent *_a0 __asm("a0") = &ie;
        register STRPTR _a1 __asm("a1") = (STRPTR)buffer;
        register LONG _d1 __asm("d1") = sizeof(buffer);
        register struct KeyMap *_a2 __asm("a2") = NULL;
        
        __asm volatile (
            "jsr %1@(-48)"
            : "=r" (_result)
            : "a" (_a6), "r" (_a0), "r" (_a1), "r" (_d1), "r" (_a2)
            : "cc", "memory"
        );
        
        result = _result;
    }
    
    if (result > 0) {
        print("OK: RawKeyConvert returned ");
        print_num(result);
        print(" character(s): '");
        for (LONG i = 0; i < result; i++) {
            char c = buffer[i];
            Write(Output(), &c, 1);
        }
        print("'\n");
    } else if (result == 0) {
        print("OK: RawKeyConvert returned 0 (no conversion)\n");
    } else {
        print("ERROR: RawKeyConvert returned ");
        print_num(result);
        print("\n");
        CloseDevice((struct IORequest *)consoleReq);
        DeleteIORequest((struct IORequest *)consoleReq);
        DeleteMsgPort(consolePort);
        return 20;
    }
    
    CloseDevice((struct IORequest *)consoleReq);
    
    /* Test 4: Scrolling region CSI sequences (write-only test) */
    print("\n4. Testing scrolling region CSI sequences...\n");
    print("   (Testing CSI 'r' (DECSTBM) and CSI '{' (Amiga) - write tests only)\n");
    
    /* Reopen console device */
    error = OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)consoleReq, 0);
    if (error) {
        print("ERROR: Failed to reopen console.device\n");
        DeleteIORequest((struct IORequest *)consoleReq);
        DeleteMsgPort(consolePort);
        return 20;
    }
    
    /* Write DECSTBM sequence: CSI 5;20 r (set scrolling region to lines 5-20) */
    const char decstbm[] = "\x9B" "5;20r";  /* CSI 5;20 r */
    consoleReq->io_Command = CMD_WRITE;
    consoleReq->io_Data = (APTR)decstbm;
    consoleReq->io_Length = sizeof(decstbm) - 1;
    DoIO((struct IORequest *)consoleReq);
    
    print("OK: Sent CSI 5;20 r (DECSTBM)\n");
    
    /* Write Amiga scrolling region sequence: CSI 15 { (set scrolling region to 15 lines) */
    const char amiga_scroll[] = "\x9B" "15{";  /* CSI 15 { */
    consoleReq->io_Command = CMD_WRITE;
    consoleReq->io_Data = (APTR)amiga_scroll;
    consoleReq->io_Length = sizeof(amiga_scroll) - 1;
    DoIO((struct IORequest *)consoleReq);
    
    print("OK: Sent CSI 15 { (Amiga scrolling region)\n");
    
    CloseDevice((struct IORequest *)consoleReq);
    
    /* Cleanup */
    DeleteIORequest((struct IORequest *)consoleReq);
    DeleteMsgPort(consolePort);
    
    print("\nPASS: All console_advanced tests passed!\n");
    
    return 0;
}
