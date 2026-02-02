/*
 * Test for timer.device - TR_GETSYSTIME command
 * This tests basic timer.device opening and time retrieval
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

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
    LONG error;
    
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
    error = OpenDevice((STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *)timerReq, 0);
    if (error != 0) {
        print("FAIL: Cannot open timer.device, error=");
        print_num((ULONG)error);
        print("\n");
        FreeMem(timerReq, sizeof(struct timerequest));
        FreeMem(timerPort, sizeof(struct MsgPort));
        return 1;
    }
    print("OK: timer.device opened\n");
    
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
    
    /* Close timer.device */
    CloseDevice((struct IORequest *)timerReq);
    print("OK: timer.device closed\n");
    
    /* Cleanup */
    FreeMem(timerReq, sizeof(struct timerequest));
    FreeMem(timerPort, sizeof(struct MsgPort));
    print("OK: Cleanup complete\n");
    
    print("PASS: timer.device test complete\n");
    
    return 0;
}
