/*
 * Test for timer.device core commands and library entry points.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/timer_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Library *TimerBase;

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
    LONG error;
    struct timeval tv_a;
    struct timeval tv_b;
    struct timeval tv_set;
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
    
    print("Testing timer.device\n");
    
    /* Create message port for timer replies */
    timerPort = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    if (!timerPort) {
        print("FAIL: Cannot allocate message port\n");
        return 1;
    }
    
    timerPort->mp_Node.ln_Type = NT_MSGPORT;
    timerPort->mp_Flags = PA_IGNORE;
    timerPort->mp_SigTask = FindTask(NULL);
    NEWLIST(&timerPort->mp_MsgList);
    
    print("OK: Message port created\n");
    
    /* Create timer request */
    timerReq = (struct timerequest *)AllocMem(sizeof(struct timerequest), MEMF_PUBLIC | MEMF_CLEAR);
    if (!timerReq) {
        print("FAIL: Cannot allocate IO request\n");
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
        return 1;
    }
    print("OK: timer.device opened\n");
    TimerBase = (struct Library *)timerReq->tr_node.io_Device;

    waitUntilReq = (struct timerequest *)AllocMem(sizeof(struct timerequest), MEMF_PUBLIC | MEMF_CLEAR);
    if (!waitUntilReq) {
        print("FAIL: Cannot allocate wait-until IO request\n");
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
        return 1;
    }

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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
        return 1;
    }
    print("OK: GetSysTime follows TR_SETSYSTIME\n");

    eclock_freq_1 = ReadEClock(&eclock_1);
    GetSysTime(&tv_a);
    GetSysTime(&tv_b);
    eclock_freq_2 = ReadEClock(&eclock_2);

    if (eclock_freq_1 != 0 && eclock_freq_1 == eclock_freq_2) {
        print("OK: ReadEClock returned stable frequency\n");
    } else {
        print("FAIL: ReadEClock returned invalid frequency\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        FreeMem(timerPort, sizeof(struct MsgPort));
        return 1;
    }

    if (eclock_2.ev_hi > eclock_1.ev_hi ||
        (eclock_2.ev_hi == eclock_1.ev_hi && eclock_2.ev_lo > eclock_1.ev_lo)) {
        print("OK: ReadEClock advanced over time\n");
    } else {
        print("FAIL: ReadEClock did not advance\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        FreeMem(timerPort, sizeof(struct MsgPort));
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
        FreeMem(timerPort, sizeof(struct MsgPort));
        return 1;
    }

    /* Test CMD_READ */
    timerReq->tr_node.io_Command = CMD_READ;
    timerReq->tr_node.io_Flags = IOF_QUICK;
    
    DoIO((struct IORequest *)timerReq);
    
    if (timerReq->tr_node.io_Error != 0) {
        print("FAIL: CMD_READ failed\n");
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        FreeMem(timerPort, sizeof(struct MsgPort));
        return 1;
    }
    
    print("OK: CMD_READ succeeded\n");

    if (timerReq->tr_time.tv_micro < 1000000) {
        print("OK: CMD_READ returned normalized timeval\n");
    } else {
        print("FAIL: CMD_READ returned unnormalized timeval\n");
        CloseDevice((struct IORequest *)waitUntilReq);
        FreeMem(waitUntilReq, sizeof(struct timerequest));
        CloseDevice((struct IORequest *)timerReq);
        FreeMem(timerReq, sizeof(struct timerequest));
        FreeMem(timerPort, sizeof(struct MsgPort));
        return 1;
    }

    /* Close timer.device */
    CloseDevice((struct IORequest *)waitUntilReq);
    print("OK: wait-until unit closed\n");

    CloseDevice((struct IORequest *)timerReq);
    print("OK: timer.device closed\n");

    /* Cleanup */
    FreeMem(waitUntilReq, sizeof(struct timerequest));
    FreeMem(timerReq, sizeof(struct timerequest));
    FreeMem(timerPort, sizeof(struct MsgPort));
    print("OK: Cleanup complete\n");
    
    print("PASS: timer.device test complete\n");
    
    return 0;
}
