/*
 * Test for timer.device core commands and library entry points.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <exec/errors.h>
#include <exec/libraries.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/timer_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Library *TimerBase;
static int g_failures = 0;

#define NEWLIST(l) ((l)->lh_Head = (struct Node *)&(l)->lh_Tail, \
                    (l)->lh_Tail = NULL, \
                    (l)->lh_TailPred = (struct Node *)&(l)->lh_Head)

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(ULONG num)
{
    char buf[16];
    int i = 0;
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (num > 0) {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }
        while (j > 0) {
            buf[i++] = temp[--j];
        }
    }
    buf[i] = '\0';
    print(buf);
}

int main(void)
{
    struct MsgPort *timerPort;
    struct timerequest *timerReq;
    struct timerequest *waitUntilReq;
    struct timerequest *reopenReq;
    struct Device *timerDevice;
    struct Node *deviceNode;
    LONG error;
    struct timeval tv_a;
    struct timeval tv_b;
    struct timeval tv_set;
    struct timeval tv_orig;
    struct timeval tv_now;
    struct timeval tv_quick;
    struct timeval tv_lib;
    struct timeval tv_cmp_hi;
    struct timeval tv_cmp_lo;
    struct timeval tv_add;
    struct timeval tv_sub;
    struct EClockVal eclock_1;
    struct EClockVal eclock_2;
    ULONG eclock_freq_1;
    ULONG eclock_freq_2;
    BOOL eclock_advanced = FALSE;
    int eclock_attempt;
    
    print("Testing timer.device\n");
    
    /* Create message port for timer replies */
    /* a signalling port: WaitIO() on a request that completes from the
     * timer interrupt needs the reply signal (a PA_IGNORE port hangs on
     * AmigaOS 3.1) */
    timerPort = CreateMsgPort();
    if (!timerPort) {
        print("FAIL: Cannot allocate message port\n");
        return 1;
    }
    
    print("OK: Message port created\n");
    
    /* Create timer request */
    timerReq = (struct timerequest *)AllocMem(sizeof(struct timerequest), MEMF_PUBLIC | MEMF_CLEAR);
    if (!timerReq) {
        print("FAIL: Cannot allocate IO request\n");
        DeleteMsgPort(timerPort);
        return 1;
    }
    
    timerReq->tr_node.io_Message.mn_ReplyPort = timerPort;
    timerReq->tr_node.io_Message.mn_Length = sizeof(struct timerequest);
    
    print("OK: IO request created\n");
    
    /* Open timer.device */
    error = OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timerReq, 0);
    if (error != 0) {
        print("FAIL: Cannot open timer.device, error=");
        print_num((ULONG)error);
        print("\n");
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }
    print("OK: timer.device opened\n");
    TimerBase = (struct Library *)timerReq->tr_node.io_Device;

    waitUntilReq = (struct timerequest *)AllocMem(sizeof(struct timerequest), MEMF_PUBLIC | MEMF_CLEAR);
    if (!waitUntilReq) {
        print("FAIL: Cannot allocate wait-until IO request\n");
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    waitUntilReq->tr_node.io_Message.mn_ReplyPort = timerPort;
    waitUntilReq->tr_node.io_Message.mn_Length = sizeof(struct timerequest);

    error = OpenDevice((STRPTR)TIMERNAME, UNIT_WAITUNTIL, (struct IORequest *)waitUntilReq, 0);
    if (error != 0) {
        print("FAIL: Cannot open timer.device wait-until unit, error=");
        print_num((ULONG)error);
        print("\n");
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }
    print("OK: wait-until unit opened\n");

    /* Test TR_GETSYSTIME */
    timerReq->tr_node.io_Command = TR_GETSYSTIME;
    timerReq->tr_node.io_Flags = IOF_QUICK;
    
    DoIO((struct IORequest *)timerReq);
    
    if (timerReq->tr_node.io_Error != 0) {
        print("FAIL: TR_GETSYSTIME failed, error=");
        print_num((ULONG)timerReq->tr_node.io_Error);
        print("\n");
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    print("OK: TR_GETSYSTIME succeeded\n");

    tv_now = timerReq->tr_time;
    if (tv_now.tv_micro < 1000000) {
        print("OK: TR_GETSYSTIME returned normalized timeval\n");
    } else {
        print("FAIL: TR_GETSYSTIME returned unnormalized timeval\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    GetSysTime(&tv_lib);
    if (tv_lib.tv_micro < 1000000) {
        print("OK: GetSysTime returned normalized timeval\n");
    } else {
        print("FAIL: GetSysTime returned unnormalized timeval\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    tv_cmp_hi = tv_lib;
    tv_cmp_lo = tv_now;
    if (CmpTime(&tv_cmp_hi, &tv_cmp_lo) == 0 || CmpTime(&tv_cmp_hi, &tv_cmp_lo) == -1 || CmpTime(&tv_cmp_hi, &tv_cmp_lo) == 1) {
        print("OK: CmpTime callable through timer base\n");
    } else {
        print("FAIL: CmpTime returned invalid result\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    tv_add.tv_secs = 1;
    tv_add.tv_micro = 900000;
    tv_a.tv_secs = 2;
    tv_a.tv_micro = 200000;
    AddTime(&tv_add, &tv_a);
    if (tv_add.tv_secs == 4 && tv_add.tv_micro == 100000) {
        print("OK: AddTime normalized carry\n");
    } else {
        print("FAIL: AddTime produced unexpected result\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    tv_sub.tv_secs = 5;
    tv_sub.tv_micro = 100000;
    tv_b.tv_secs = 1;
    tv_b.tv_micro = 200000;
    SubTime(&tv_sub, &tv_b);
    if (tv_sub.tv_secs == 3 && tv_sub.tv_micro == 900000) {
        print("OK: SubTime handled borrow\n");
    } else {
        print("FAIL: SubTime produced unexpected result\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    tv_a.tv_secs = 10;
    tv_a.tv_micro = 0;
    tv_b.tv_secs = 9;
    tv_b.tv_micro = 999999;
    if (CmpTime(&tv_a, &tv_b) == -1 &&
        CmpTime(&tv_b, &tv_a) == 1 &&
        CmpTime(&tv_a, &tv_a) == 0) {
        print("OK: CmpTime matches timer.device ordering\n");
    } else {
        print("FAIL: CmpTime ordering is incorrect\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    GetSysTime(&tv_orig);
    tv_set.tv_secs = 1000;
    tv_set.tv_micro = 250000;
    timerReq->tr_node.io_Command = TR_SETSYSTIME;
    timerReq->tr_node.io_Flags = IOF_QUICK;
    timerReq->tr_time = tv_set;
    DoIO((struct IORequest *)timerReq);
    if (timerReq->tr_node.io_Error != 0) {
        print("FAIL: TR_SETSYSTIME failed\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }
    if (timerReq->tr_time.tv_secs == 0 && timerReq->tr_time.tv_micro == 0) {
        print("OK: TR_SETSYSTIME zeroed request time\n");
    } else {
        print("FAIL: TR_SETSYSTIME did not zero request time\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    timerReq->tr_node.io_Command = TR_GETSYSTIME;
    timerReq->tr_node.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)timerReq);
    if (timerReq->tr_time.tv_secs < tv_set.tv_secs || timerReq->tr_time.tv_secs > tv_set.tv_secs + 1) {
        print("FAIL: TR_GETSYSTIME did not follow TR_SETSYSTIME\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }
    print("OK: TR_SETSYSTIME updated system time\n");

    GetSysTime(&tv_lib);
    if (tv_lib.tv_secs < tv_set.tv_secs || tv_lib.tv_secs > tv_set.tv_secs + 1) {
        print("FAIL: GetSysTime did not follow TR_SETSYSTIME\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }
    print("OK: GetSysTime follows TR_SETSYSTIME\n");

    eclock_freq_1 = ReadEClock(&eclock_1);
    eclock_freq_2 = eclock_freq_1;
    eclock_2 = eclock_1;

    timerReq->tr_node.io_Command = TR_GETSYSTIME;
    timerReq->tr_node.io_Flags = IOF_QUICK;

    for (eclock_attempt = 0; eclock_attempt < 5; eclock_attempt++) {
        DoIO((struct IORequest *)timerReq);
        if (timerReq->tr_node.io_Error != 0) {
            print("FAIL: TR_GETSYSTIME failed during ReadEClock retry\n");
            CloseDevice((struct IORequest *)waitUntilReq);
            FreeMem(waitUntilReq, sizeof(struct timerequest));
            CloseDevice((struct IORequest *)timerReq);
            FreeMem(timerReq, sizeof(struct timerequest));
            DeleteMsgPort(timerPort);
            return 1;
        }
        eclock_freq_2 = ReadEClock(&eclock_2);
        if (eclock_2.ev_hi > eclock_1.ev_hi ||
            (eclock_2.ev_hi == eclock_1.ev_hi && eclock_2.ev_lo > eclock_1.ev_lo)) {
            eclock_advanced = TRUE;
            break;
        }
    }

    if (eclock_freq_1 != 0 && eclock_freq_1 == eclock_freq_2) {
        print("OK: ReadEClock returned stable frequency\n");
    } else {
        print("FAIL: ReadEClock returned invalid frequency\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    if (eclock_advanced) {
        print("OK: ReadEClock advanced over time\n");
    } else {
        print("FAIL: ReadEClock did not advance\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    print("OK: Preparing UNIT_WAITUNTIL immediate test\n");
    tv_quick = tv_lib;
    tv_a.tv_secs = 0;
    tv_a.tv_micro = 1;
    SubTime(&tv_quick, &tv_a);

    print("OK: Calling UNIT_WAITUNTIL DoIO\n");
    waitUntilReq->tr_node.io_Command = TR_ADDREQUEST;
    waitUntilReq->tr_node.io_Flags = IOF_QUICK;
    waitUntilReq->tr_time = tv_quick;
    DoIO((struct IORequest *)waitUntilReq);
    print("OK: UNIT_WAITUNTIL DoIO returned\n");

    if (waitUntilReq->tr_node.io_Error == 0 &&
        waitUntilReq->tr_time.tv_secs == 0 &&
        waitUntilReq->tr_time.tv_micro == 0) {
        print("OK: UNIT_WAITUNTIL completes immediately for past times\n");
    } else {
        print("FAIL: UNIT_WAITUNTIL immediate path failed\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    /* restore the system time saved before TR_SETSYSTIME */
    timerReq->tr_node.io_Command = TR_SETSYSTIME;
    timerReq->tr_node.io_Flags = IOF_QUICK;
    timerReq->tr_time = tv_orig;
    DoIO((struct IORequest *)timerReq);
    GetSysTime(&tv_lib);
    if (timerReq->tr_node.io_Error == 0 && tv_lib.tv_secs >= tv_orig.tv_secs) {
        print("OK: TR_SETSYSTIME restored the original time\n");
    } else {
        print("FAIL: TR_SETSYSTIME could not restore the original time\n");
        g_failures++;
    }

    /* timer.device only knows the TR_* commands */
    timerReq->tr_node.io_Command = CMD_READ;
    timerReq->tr_node.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)timerReq);
    if (timerReq->tr_node.io_Error == IOERR_NOCMD) {
        print("OK: CMD_READ is not a timer.device command\n");
    } else {
        print("FAIL: CMD_READ did not return IOERR_NOCMD\n");
        g_failures++;
    }

    timerReq->tr_node.io_Command = CMD_RESET;
    timerReq->tr_node.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)timerReq);
    if (timerReq->tr_node.io_Error == IOERR_NOCMD) {
        print("OK: CMD_RESET is not a timer.device command\n");
    } else {
        print("FAIL: CMD_RESET did not return IOERR_NOCMD\n");
        g_failures++;
    }

    timerDevice = timerReq->tr_node.io_Device;
    deviceNode = FindName(&SysBase->DeviceList, (STRPTR)TIMERNAME);
    if (deviceNode == &timerDevice->dd_Library.lib_Node) {
        print("OK: timer.device present in DeviceList before Expunge\n");
    } else {
        print("FAIL: timer.device missing from DeviceList before Expunge\n");
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        FreeMem(timerReq, sizeof(struct timerequest));
        DeleteMsgPort(timerPort);
        return 1;
    }

    /* timer.device is a permanent ROM device: RemDevice() is ignored */
    RemDevice(timerDevice);
    if ((timerDevice->dd_Library.lib_Flags & LIBF_DELEXP) == 0 &&
        FindName(&SysBase->DeviceList, (STRPTR)TIMERNAME) == &timerDevice->dd_Library.lib_Node) {
        print("OK: RemDevice() leaves timer.device in place\n");
    } else {
        print("FAIL: RemDevice() changed timer.device\n");
        g_failures++;
    }

    reopenReq = (struct timerequest *)AllocMem(sizeof(struct timerequest), MEMF_PUBLIC | MEMF_CLEAR);
    if (!reopenReq) {
        print("FAIL: Cannot allocate reopen IO request\n");
        return 1;
    }
    reopenReq->tr_node.io_Message.mn_ReplyPort = timerPort;
    reopenReq->tr_node.io_Message.mn_Length = sizeof(struct timerequest);
    error = OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)reopenReq, 0);
    if (error == 0 && reopenReq->tr_node.io_Device == timerDevice) {
        print("OK: OpenDevice() still works after RemDevice()\n");
        CloseDevice((struct IORequest *)reopenReq);
    } else {
        print("FAIL: OpenDevice() failed after RemDevice()\n");
        g_failures++;
    }
    FreeMem(reopenReq, sizeof(struct timerequest));

    CloseDevice((struct IORequest *)waitUntilReq);
    CloseDevice((struct IORequest *)timerReq);
    if (FindName(&SysBase->DeviceList, (STRPTR)TIMERNAME) == &timerDevice->dd_Library.lib_Node &&
        timerReq->tr_node.io_Device == (struct Device *)-1) {
        print("OK: closing leaves timer.device in place and invalidates the request\n");
    } else {
        print("FAIL: closing timer.device misbehaved\n");
        g_failures++;
    }

    /* Cleanup */
    FreeMem(waitUntilReq, sizeof(struct timerequest));
    FreeMem(timerReq, sizeof(struct timerequest));
    DeleteMsgPort(timerPort);
    print("OK: Cleanup complete\n");

    if (g_failures) {
        print("FAIL: timer.device test had failures\n");
        return 1;
    }
    print("PASS: timer.device test complete\n");

    return 0;
}
