/*
 * Test for trackdisk.device - basic device open/close and commands
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <exec/errors.h>
#include <devices/trackdisk.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

#define NEWLIST(l) ((l)->lh_Head = (struct Node *)&(l)->lh_Tail, \
                    (l)->lh_Tail = NULL, \
                    (l)->lh_TailPred = (struct Node *)&(l)->lh_Head)

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;
static volatile LONG change_irq_count = 0;

#define TRACK_BYTES (80 * 2 * 11 * 512)

static BOOL create_adf(const char *name, UBYTE fill)
{
    BPTR fh;
    UBYTE buffer[512];
    LONG written;
    LONG blocks;
    LONG i;

    for (i = 0; i < 512; i++)
        buffer[i] = fill;

    fh = Open((CONST_STRPTR)name, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    blocks = TRACK_BYTES / 512;
    for (i = 0; i < blocks; i++)
    {
        written = Write(fh, buffer, 512);
        if (written != 512)
        {
            Close(fh);
            return FALSE;
        }
    }

    Close(fh);
    return TRUE;
}

static BOOL write_pattern(const char *name, ULONG offset, ULONG length, UBYTE base)
{
    BPTR fh;
    UBYTE buffer[512];
    ULONG i;

    if (length > sizeof(buffer))
        return FALSE;

    for (i = 0; i < length; i++)
        buffer[i] = (UBYTE)(base + i);

    fh = Open((CONST_STRPTR)name, MODE_READWRITE);
    if (!fh)
        return FALSE;

    if (Seek(fh, offset, OFFSET_BEGINNING) < 0)
    {
        Close(fh);
        return FALSE;
    }

    if (Write(fh, buffer, length) != (LONG)length)
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

static BOOL verify_pattern(const UBYTE *buffer, ULONG length, UBYTE base)
{
    ULONG i;

    for (i = 0; i < length; i++)
    {
        if (buffer[i] != (UBYTE)(base + i))
            return FALSE;
    }

    return TRUE;
}

static void change_irq_handler(void)
{
    change_irq_count++;
}

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;
    BOOL neg = FALSE;

    if (num < 0) {
        neg = TRUE;
        num = -num;
    }
    if (num == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (num > 0) {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }
        while (j > 0) buf[i++] = temp[--j];
    }
    if (neg) Write(out, "-", 1);
    buf[i] = '\0';
    print(buf);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

int main(void)
{
    struct MsgPort *port;
    struct IOExtTD *req;
    struct Interrupt change_irq;
    struct MsgPort *write_port;
    struct IOExtTD *write_req;
    struct MsgPort *missing_port;
    struct IOExtTD *missing_req;
    LONG error;
    UBYTE sector[TD_SECTOR];
    UBYTE trackbuf[TD_SECTOR * NUMSECS];
    ULONG i;

    out = Output();
    print("Testing trackdisk.device\n\n");

    /* Create message port */
    port = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    if (!port) {
        print("FAIL: Cannot allocate message port\n");
        return 20;
    }
    port->mp_Node.ln_Type = NT_MSGPORT;
    port->mp_Flags = PA_IGNORE;
    port->mp_SigTask = FindTask(NULL);
    NEWLIST(&port->mp_MsgList);

    /* Create IO request */
    req = (struct IOExtTD *)AllocMem(sizeof(struct IOExtTD), MEMF_PUBLIC | MEMF_CLEAR);
    if (!req) {
        print("FAIL: Cannot allocate IO request\n");
        FreeMem(port, sizeof(struct MsgPort));
        return 20;
    }
    req->iotd_Req.io_Message.mn_ReplyPort = port;
    req->iotd_Req.io_Message.mn_Length = sizeof(struct IOExtTD);

    if (!create_adf("DF0:trackdisk_test.adf", 0xE5)) {
        print("FAIL: Cannot create DF0:trackdisk_test.adf\n");
        FreeMem(req, sizeof(struct IOExtTD));
        FreeMem(port, sizeof(struct MsgPort));
        return 20;
    }

    if (!create_adf("DF1:trackdisk_test.adf", 0x33)) {
        print("FAIL: Cannot create DF1:trackdisk_test.adf\n");
        DeleteFile("DF0:trackdisk_test.adf");
        FreeMem(req, sizeof(struct IOExtTD));
        FreeMem(port, sizeof(struct MsgPort));
        return 20;
    }

    /* Test: Open unit 0 */
    error = OpenDevice("trackdisk.device", 0, (struct IORequest *)req, 0);
    if (error == 0)
        test_ok("OpenDevice unit 0");
    else {
        test_fail_msg("OpenDevice unit 0");
        print("    error="); print_num(error); print("\n");
        FreeMem(req, sizeof(struct IOExtTD));
        FreeMem(port, sizeof(struct MsgPort));
        return 20;
    }

    /* Test: TD_CHANGENUM */
    req->iotd_Req.io_Command = TD_CHANGENUM;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_CHANGENUM");
    else
        test_fail_msg("TD_CHANGENUM");

    /* Test: TD_CHANGESTATE */
    req->iotd_Req.io_Command = TD_CHANGESTATE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_CHANGESTATE");
    else
        test_fail_msg("TD_CHANGESTATE");

    /* Test: TD_PROTSTATUS */
    req->iotd_Req.io_Command = TD_PROTSTATUS;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_PROTSTATUS");
    else
        test_fail_msg("TD_PROTSTATUS");

    /* Test: TD_GETDRIVETYPE */
    req->iotd_Req.io_Command = TD_GETDRIVETYPE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_GETDRIVETYPE");
    else
        test_fail_msg("TD_GETDRIVETYPE");

    /* Test: TD_GETNUMTRACKS */
    req->iotd_Req.io_Command = TD_GETNUMTRACKS;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0) {
        test_ok("TD_GETNUMTRACKS");
        if (req->iotd_Req.io_Actual == 160) /* 80 cylinders * 2 heads */
            test_ok("TD_GETNUMTRACKS value=160");
        else {
            test_fail_msg("TD_GETNUMTRACKS value");
            print("    got: "); print_num(req->iotd_Req.io_Actual); print(", expected: 160\n");
        }
    } else
        test_fail_msg("TD_GETNUMTRACKS");

    /* Test: TD_MOTOR off */
    req->iotd_Req.io_Command = TD_MOTOR;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Length = 0;  /* motor off */
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_MOTOR off");
    else
        test_fail_msg("TD_MOTOR off");

    /* Test: CMD_RESET */
    req->iotd_Req.io_Command = CMD_RESET;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("CMD_RESET");
    else
        test_fail_msg("CMD_RESET");

    /* Test: CMD_FLUSH */
    req->iotd_Req.io_Command = CMD_FLUSH;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("CMD_FLUSH");
    else
        test_fail_msg("CMD_FLUSH");

    for (i = 0; i < TD_SECTOR; i++)
        sector[i] = 0;

    req->iotd_Req.io_Command = CMD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && sector[0] == 0xE5 && sector[TD_SECTOR - 1] == 0xE5)
        test_ok("CMD_READ sector 0");
    else
        test_fail_msg("CMD_READ sector 0");

    if (write_pattern("DF0:trackdisk_test.adf", TD_SECTOR, TD_SECTOR, 0x10)) {
        req->iotd_Req.io_Command = ETD_READ;
        req->iotd_Req.io_Flags = IOF_QUICK;
        req->iotd_Req.io_Data = sector;
        req->iotd_Req.io_Length = TD_SECTOR;
        req->iotd_Req.io_Offset = TD_SECTOR;
        req->iotd_Count = 1;
        req->iotd_SecLabel = NULL;
        DoIO((struct IORequest *)req);
        if (req->iotd_Req.io_Error == 0 && verify_pattern(sector, TD_SECTOR, 0x10))
            test_ok("ETD_READ sector 1");
        else
            test_fail_msg("ETD_READ sector 1");
    } else {
        test_fail_msg("Prepare ETD_READ sector 1");
    }

    req->iotd_Req.io_Command = TD_RAWREAD;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = trackbuf;
    req->iotd_Req.io_Length = TD_SECTOR * 2;
    req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 &&
        trackbuf[0] == 0xE5 &&
        verify_pattern(trackbuf + TD_SECTOR, TD_SECTOR, 0x10))
        test_ok("TD_RAWREAD track 0");
    else
        test_fail_msg("TD_RAWREAD track 0");

    req->iotd_Req.io_Command = ETD_RAWREAD;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 0;
    req->iotd_Count = 2;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_DiskChanged)
        test_ok("ETD_RAWREAD detects stale change count");
    else
        test_fail_msg("ETD_RAWREAD detects stale change count");

    req->iotd_Req.io_Command = TD_RAWREAD;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 160;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_SeekError)
        test_ok("TD_RAWREAD rejects invalid track");
    else
        test_fail_msg("TD_RAWREAD rejects invalid track");

    for (i = 0; i < TD_SECTOR * 2; i++)
        trackbuf[i] = (UBYTE)(0x60 + (i % TD_SECTOR));

    req->iotd_Req.io_Command = TD_RAWWRITE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = trackbuf;
    req->iotd_Req.io_Length = TD_SECTOR * 2;
    req->iotd_Req.io_Offset = 1;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_RAWWRITE track 1");
    else
        test_fail_msg("TD_RAWWRITE track 1");

    req->iotd_Req.io_Command = CMD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = TD_SECTOR * NUMSECS;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && verify_pattern(sector, TD_SECTOR, 0x60))
        test_ok("TD_RAWWRITE persisted track data");
    else
        test_fail_msg("TD_RAWWRITE persisted track data");

    for (i = 0; i < TD_SECTOR; i++)
        sector[i] = (UBYTE)i;

    req->iotd_Req.io_Command = CMD_WRITE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("CMD_WRITE sector 0");
    else
        test_fail_msg("CMD_WRITE sector 0");

    for (i = 0; i < TD_SECTOR; i++)
        sector[i] = 0;

    req->iotd_Req.io_Command = CMD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && verify_pattern(sector, TD_SECTOR, 0x00))
        test_ok("CMD_WRITE persisted data");
    else
        test_fail_msg("CMD_WRITE persisted data");

    for (i = 0; i < TD_SECTOR; i++)
        sector[i] = (UBYTE)(0x40 + i);

    req->iotd_Req.io_Command = ETD_WRITE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = TD_SECTOR * 2;
    req->iotd_Count = 1;
    req->iotd_SecLabel = NULL;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("ETD_WRITE sector 2");
    else
        test_fail_msg("ETD_WRITE sector 2");

    for (i = 0; i < TD_SECTOR; i++)
        sector[i] = 0;

    req->iotd_Req.io_Command = CMD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = TD_SECTOR * 2;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && verify_pattern(sector, TD_SECTOR, 0x40))
        test_ok("ETD_WRITE persisted data");
    else
        test_fail_msg("ETD_WRITE persisted data");

    req->iotd_Req.io_Command = ETD_WRITE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = TD_SECTOR * 3;
    req->iotd_Count = 2;
    req->iotd_SecLabel = NULL;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_DiskChanged)
        test_ok("ETD_WRITE detects stale change count");
    else
        test_fail_msg("ETD_WRITE detects stale change count");

    req->iotd_Req.io_Command = ETD_WRITE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = TD_SECTOR * 3;
    req->iotd_Count = 1;
    req->iotd_SecLabel = sector;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_NoSecHdr)
        test_ok("ETD_WRITE rejects sector labels");
    else
        test_fail_msg("ETD_WRITE rejects sector labels");
    req->iotd_SecLabel = NULL;

    for (i = 0; i < TD_SECTOR * NUMSECS; i++)
        trackbuf[i] = (UBYTE)(0xC0 + (i % TD_SECTOR));

    req->iotd_Req.io_Command = TD_FORMAT;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = trackbuf;
    req->iotd_Req.io_Length = TD_SECTOR * NUMSECS;
    req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_FORMAT track 0");
    else
        test_fail_msg("TD_FORMAT track 0");

    for (i = 0; i < TD_SECTOR; i++)
        sector[i] = 0;

    req->iotd_Req.io_Command = CMD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && verify_pattern(sector, TD_SECTOR, 0xC0))
        test_ok("TD_FORMAT rewrote track data");
    else
        test_fail_msg("TD_FORMAT rewrote track data");

    req->iotd_Req.io_Command = TD_FORMAT;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_TooFewSecs)
        test_ok("TD_FORMAT rejects partial track");
    else
        test_fail_msg("TD_FORMAT rejects partial track");

    req->iotd_Req.io_Command = TD_SEEK;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Offset = TD_SECTOR * NUMSECS;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_SEEK track 1");
    else
        test_fail_msg("TD_SEEK track 1");

    req->iotd_Req.io_Command = ETD_SEEK;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Offset = TD_SECTOR * NUMSECS * 2;
    req->iotd_Count = 1;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("ETD_SEEK track 2");
    else
        test_fail_msg("ETD_SEEK track 2");

    req->iotd_Req.io_Command = CMD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = 100;
    req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_BadSecPreamble)
        test_ok("CMD_READ rejects short sector length");
    else
        test_fail_msg("CMD_READ rejects short sector length");

    req->iotd_Req.io_Command = ETD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 0;
    req->iotd_Count = 2;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_DiskChanged)
        test_ok("ETD_READ detects stale change count");
    else
        test_fail_msg("ETD_READ detects stale change count");

    req->iotd_Req.io_Command = ETD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = sector;
    req->iotd_Req.io_Length = TD_SECTOR;
    req->iotd_Req.io_Offset = 0;
    req->iotd_Count = 1;
    req->iotd_SecLabel = sector;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_NoSecHdr)
        test_ok("ETD_READ rejects sector labels");
    else
        test_fail_msg("ETD_READ rejects sector labels");
    req->iotd_SecLabel = NULL;

    if (SetProtection("DF1:trackdisk_test.adf", FIBF_WRITE))
        test_ok("SetProtection write-protected image");
    else
        test_fail_msg("SetProtection write-protected image");

    write_port = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    write_req = (struct IOExtTD *)AllocMem(sizeof(struct IOExtTD), MEMF_PUBLIC | MEMF_CLEAR);
    if (write_port && write_req) {
        write_port->mp_Node.ln_Type = NT_MSGPORT;
        write_port->mp_Flags = PA_IGNORE;
        write_port->mp_SigTask = FindTask(NULL);
        NEWLIST(&write_port->mp_MsgList);
        write_req->iotd_Req.io_Message.mn_ReplyPort = write_port;
        write_req->iotd_Req.io_Message.mn_Length = sizeof(struct IOExtTD);
        error = OpenDevice("trackdisk.device", 1, (struct IORequest *)write_req, 0);
        if (error == 0) {
            for (i = 0; i < TD_SECTOR; i++)
                sector[i] = (UBYTE)(0x80 + i);
            write_req->iotd_Req.io_Command = CMD_WRITE;
            write_req->iotd_Req.io_Flags = IOF_QUICK;
            write_req->iotd_Req.io_Data = sector;
            write_req->iotd_Req.io_Length = TD_SECTOR;
            write_req->iotd_Req.io_Offset = 0;
            DoIO((struct IORequest *)write_req);
            if (write_req->iotd_Req.io_Error == TDERR_WriteProt)
                test_ok("CMD_WRITE write-protected image");
            else
                test_fail_msg("CMD_WRITE write-protected image");
            CloseDevice((struct IORequest *)write_req);
        } else {
            test_fail_msg("OpenDevice unit 1 write-protected handle");
        }
    } else {
        test_fail_msg("Allocate write-protected request");
    }

    if (write_req) FreeMem(write_req, sizeof(struct IOExtTD));
    if (write_port) FreeMem(write_port, sizeof(struct MsgPort));

    missing_port = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    missing_req = (struct IOExtTD *)AllocMem(sizeof(struct IOExtTD), MEMF_PUBLIC | MEMF_CLEAR);
    if (missing_port && missing_req) {
        missing_port->mp_Node.ln_Type = NT_MSGPORT;
        missing_port->mp_Flags = PA_IGNORE;
        missing_port->mp_SigTask = FindTask(NULL);
        NEWLIST(&missing_port->mp_MsgList);
        missing_req->iotd_Req.io_Message.mn_ReplyPort = missing_port;
        missing_req->iotd_Req.io_Message.mn_Length = sizeof(struct IOExtTD);
        error = OpenDevice("trackdisk.device", 2, (struct IORequest *)missing_req, 0);
        if (error == 0) {
            missing_req->iotd_Req.io_Command = CMD_READ;
            missing_req->iotd_Req.io_Flags = IOF_QUICK;
            missing_req->iotd_Req.io_Data = sector;
            missing_req->iotd_Req.io_Length = TD_SECTOR;
            missing_req->iotd_Req.io_Offset = 0;
            DoIO((struct IORequest *)missing_req);
            if (missing_req->iotd_Req.io_Error == TDERR_DiskChanged)
                test_ok("CMD_READ missing disk image");
            else
                test_fail_msg("CMD_READ missing disk image");
            CloseDevice((struct IORequest *)missing_req);
        } else {
            test_fail_msg("OpenDevice unit 2");
        }
    } else {
        test_fail_msg("Allocate missing-disk request");
    }

    if (missing_req) FreeMem(missing_req, sizeof(struct IOExtTD));
    if (missing_port) FreeMem(missing_port, sizeof(struct MsgPort));

    /* Test: TD_ADDCHANGEINT / TD_REMCHANGEINT lifecycle */
    change_irq.is_Node.ln_Type = NT_INTERRUPT;
    change_irq.is_Node.ln_Pri = 0;
    change_irq.is_Node.ln_Name = (char *)"trackdisk-change";
    change_irq.is_Data = NULL;
    change_irq.is_Code = (VOID (*)())change_irq_handler;

    req->iotd_Req.io_Command = TD_ADDCHANGEINT;
    req->iotd_Req.io_Flags = 0;
    req->iotd_Req.io_Length = sizeof(struct Interrupt);
    req->iotd_Req.io_Data = &change_irq;
    SendIO((struct IORequest *)req);

    if (CheckIO((struct IORequest *)req) == NULL)
        test_ok("TD_ADDCHANGEINT remains pending");
    else
        test_fail_msg("TD_ADDCHANGEINT remains pending");

    req->iotd_Req.io_Command = TD_REMCHANGEINT;
    req->iotd_Req.io_Flags = 0;
    req->iotd_Req.io_Length = sizeof(struct Interrupt);
    req->iotd_Req.io_Data = &change_irq;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("TD_REMCHANGEINT completes held request");
    else
        test_fail_msg("TD_REMCHANGEINT completes held request");

    if (CheckIO((struct IORequest *)req) == (struct IORequest *)req)
        test_ok("TD_REMCHANGEINT request reports completion");
    else
        test_fail_msg("TD_REMCHANGEINT request reports completion");

    req->iotd_Req.io_Command = TD_ADDCHANGEINT;
    req->iotd_Req.io_Flags = 0;
    req->iotd_Req.io_Length = sizeof(struct Interrupt);
    req->iotd_Req.io_Data = &change_irq;
    SendIO((struct IORequest *)req);

    if (CheckIO((struct IORequest *)req) == NULL)
        test_ok("TD_ADDCHANGEINT stays abortable while pending");
    else
        test_fail_msg("TD_ADDCHANGEINT stays abortable while pending");

    AbortIO((struct IORequest *)req);
    WaitIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == IOERR_ABORTED)
        test_ok("AbortIO cancels TD_ADDCHANGEINT");
    else
        test_fail_msg("AbortIO cancels TD_ADDCHANGEINT");

    /* Test: Close device */
    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice");

    /* Test: Open unit 1 */
    error = OpenDevice("trackdisk.device", 1, (struct IORequest *)req, 0);
    if (error == 0) {
        test_ok("OpenDevice unit 1");
        CloseDevice((struct IORequest *)req);
        test_ok("CloseDevice unit 1");
    } else {
        test_fail_msg("OpenDevice unit 1");
    }

    /* Test: Invalid unit (4) should fail */
    error = OpenDevice("trackdisk.device", 4, (struct IORequest *)req, 0);
    if (error != 0)
        test_ok("OpenDevice unit 4 rejected (expected)");
    else {
        test_fail_msg("OpenDevice unit 4 should have failed");
        CloseDevice((struct IORequest *)req);
    }

    /* Cleanup */
    DeleteFile("DF0:trackdisk_test.adf");
    DeleteFile("DF1:trackdisk_test.adf");
    FreeMem(req, sizeof(struct IOExtTD));
    FreeMem(port, sizeof(struct MsgPort));
    test_ok("Cleanup complete");

    print("\n");
    if (test_fail == 0) {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    } else {
        print("FAIL: ");
        print_num(test_fail);
        print(" of ");
        print_num(test_pass + test_fail);
        print(" tests failed\n");
        return 20;
    }
}
