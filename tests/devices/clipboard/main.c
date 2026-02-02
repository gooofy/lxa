/*
 * Test for clipboard.device - Basic read/write operations
 * This tests clipboard data storage and retrieval
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <devices/clipboard.h>
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

static BOOL str_equal(const char *s1, const char *s2, ULONG len)
{
    for (ULONG i = 0; i < len; i++) {
        if (s1[i] != s2[i]) return FALSE;
    }
    return TRUE;
}

int main(void)
{
    struct MsgPort *clipPort;
    struct IOClipReq *clipReq;
    LONG error;
    char writeData[] = "Hello Clipboard!";
    char readData[64];
    LONG clipID;
    
    print("Testing clipboard.device\n");
    
    /* Create message port */
    clipPort = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    if (!clipPort) {
        print("FAIL: Cannot allocate message port\n");
        return 1;
    }
    
    clipPort->mp_Node.ln_Type = NT_MSGPORT;
    clipPort->mp_Flags = PA_IGNORE;
    clipPort->mp_SigTask = FindTask(NULL);
    NEWLIST(&clipPort->mp_MsgList);
    
    print("OK: Message port created\n");
    
    /* Create clipboard request */
    clipReq = (struct IOClipReq *)AllocMem(sizeof(struct IOClipReq), MEMF_PUBLIC | MEMF_CLEAR);
    if (!clipReq) {
        print("FAIL: Cannot allocate IO request\n");
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    
    clipReq->io_Message.mn_ReplyPort = clipPort;
    clipReq->io_Message.mn_Length = sizeof(struct IOClipReq);
    
    print("OK: IO request created\n");
    
    /* Open clipboard.device unit 0 (PRIMARY_CLIP) */
    error = OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)clipReq, 0);
    if (error != 0) {
        print("FAIL: Cannot open clipboard.device, error=");
        print_num((ULONG)error);
        print("\n");
        FreeMem(clipReq, sizeof(struct IOClipReq));
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    print("OK: clipboard.device opened\n");
    
    /* Get current write ID */
    clipReq->io_Command = CBD_CURRENTWRITEID;
    clipReq->io_Flags = IOF_QUICK;
    
    DoIO((struct IORequest *)clipReq);
    
    if (clipReq->io_Error != 0) {
        print("FAIL: CBD_CURRENTWRITEID failed\n");
        CloseDevice((struct IORequest *)clipReq);
        FreeMem(clipReq, sizeof(struct IOClipReq));
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    
    clipID = clipReq->io_ClipID;
    print("OK: CBD_CURRENTWRITEID returned ID=");
    print_num((ULONG)clipID);
    print("\n");
    
    /* Write data to clipboard */
    clipReq->io_Command = CMD_WRITE;
    clipReq->io_Flags = IOF_QUICK;
    clipReq->io_Data = (STRPTR)writeData;
    clipReq->io_Length = sizeof(writeData) - 1;  /* Don't include null terminator */
    clipReq->io_Offset = 0;
    
    DoIO((struct IORequest *)clipReq);
    
    if (clipReq->io_Error != 0) {
        print("FAIL: CMD_WRITE failed\n");
        CloseDevice((struct IORequest *)clipReq);
        FreeMem(clipReq, sizeof(struct IOClipReq));
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    
    print("OK: CMD_WRITE succeeded, wrote ");
    print_num(clipReq->io_Actual);
    print(" bytes\n");
    
    /* Update clipboard (finalize write) */
    clipReq->io_Command = CMD_UPDATE;
    clipReq->io_Flags = IOF_QUICK;
    
    DoIO((struct IORequest *)clipReq);
    
    if (clipReq->io_Error != 0) {
        print("FAIL: CMD_UPDATE failed\n");
        CloseDevice((struct IORequest *)clipReq);
        FreeMem(clipReq, sizeof(struct IOClipReq));
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    
    clipID = clipReq->io_ClipID;
    print("OK: CMD_UPDATE succeeded, new ClipID=");
    print_num((ULONG)clipID);
    print("\n");
    
    /* Get current read ID */
    clipReq->io_Command = CBD_CURRENTREADID;
    clipReq->io_Flags = IOF_QUICK;
    
    DoIO((struct IORequest *)clipReq);
    
    if (clipReq->io_Error != 0) {
        print("FAIL: CBD_CURRENTREADID failed\n");
        CloseDevice((struct IORequest *)clipReq);
        FreeMem(clipReq, sizeof(struct IOClipReq));
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    
    print("OK: CBD_CURRENTREADID returned ID=");
    print_num((ULONG)clipReq->io_ClipID);
    print("\n");
    
    /* Read data back from clipboard */
    for (int i = 0; i < 64; i++) readData[i] = 0;
    
    clipReq->io_Command = CMD_READ;
    clipReq->io_Flags = IOF_QUICK;
    clipReq->io_Data = (STRPTR)readData;
    clipReq->io_Length = sizeof(readData);
    clipReq->io_Offset = 0;
    clipReq->io_ClipID = clipID;
    
    DoIO((struct IORequest *)clipReq);
    
    if (clipReq->io_Error != 0) {
        print("FAIL: CMD_READ failed, error=");
        print_num((ULONG)clipReq->io_Error);
        print("\n");
        CloseDevice((struct IORequest *)clipReq);
        FreeMem(clipReq, sizeof(struct IOClipReq));
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    
    print("OK: CMD_READ succeeded, read ");
    print_num(clipReq->io_Actual);
    print(" bytes\n");
    
    /* Verify data */
    if (clipReq->io_Actual != sizeof(writeData) - 1) {
        print("FAIL: Read size mismatch\n");
        CloseDevice((struct IORequest *)clipReq);
        FreeMem(clipReq, sizeof(struct IOClipReq));
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    
    if (!str_equal(readData, writeData, clipReq->io_Actual)) {
        print("FAIL: Data mismatch\n");
        CloseDevice((struct IORequest *)clipReq);
        FreeMem(clipReq, sizeof(struct IOClipReq));
        FreeMem(clipPort, sizeof(struct MsgPort));
        return 1;
    }
    
    print("OK: Data verified\n");
    
    /* Close clipboard.device */
    CloseDevice((struct IORequest *)clipReq);
    print("OK: clipboard.device closed\n");
    
    /* Cleanup */
    FreeMem(clipReq, sizeof(struct IOClipReq));
    FreeMem(clipPort, sizeof(struct MsgPort));
    print("OK: Cleanup complete\n");
    
    print("PASS: clipboard.device test complete\n");
    
    return 0;
}
