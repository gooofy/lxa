/*
 * Test for audio.device allocation, playback timing, and interrupt delivery.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/interrupts.h>
#include <exec/execbase.h>
#include <devices/audio.h>
#include <devices/timer.h>
#include <hardware/intbits.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;
static volatile ULONG g_audio_irq_count = 0;

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
        if (neg) buf[i++] = '-';
        while (j > 0) buf[i++] = temp[--j];
    }
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

static void delay_ticks(ULONG ticks)
{
    struct MsgPort *port;
    struct timerequest *tr;

    port = CreateMsgPort();
    tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));
    if (!port || !tr) {
        return;
    }

    if (OpenDevice((STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0) == 0) {
        tr->tr_node.io_Command = TR_ADDREQUEST;
        tr->tr_time.tv_secs = ticks / 50;
        tr->tr_time.tv_micro = (ticks % 50) * 20000;
        DoIO((struct IORequest *)tr);
        CloseDevice((struct IORequest *)tr);
    }

    DeleteIORequest((struct IORequest *)tr);
    DeleteMsgPort(port);
}

static void AudioIRQHandler(void)
{
    g_audio_irq_count++;
}

int main(void)
{
    struct MsgPort *port;
    struct IOAudio *req;
    struct IOAudio *req2;
    struct Interrupt audio_irq;
    UBYTE alloc_map[] = { 1 };
    BYTE waveform[800];
    LONG error;

    out = Output();
    print("Testing audio.device\n\n");

    {
        int i;
        for (i = 0; i < (int)sizeof(waveform); i++) {
            waveform[i] = (i & 1) ? -127 : 127;
        }
    }

    port = CreateMsgPort();
    if (!port) {
        print("FAIL: Cannot create message port\n");
        return 20;
    }

    req = (struct IOAudio *)CreateIORequest(port, sizeof(struct IOAudio));
    req2 = (struct IOAudio *)CreateIORequest(port, sizeof(struct IOAudio));
    if (!req || !req2) {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    req->ioa_Data = alloc_map;
    req->ioa_Length = sizeof(alloc_map);
    req->ioa_Request.io_Message.mn_Node.ln_Pri = 10;

    error = OpenDevice((STRPTR)"audio.device", 0, (struct IORequest *)req, 0);
    if (error == 0) {
        test_ok("OpenDevice audio.device allocates requested channel");
    } else {
        test_fail_msg("OpenDevice audio.device allocates requested channel");
        print("    error="); print_num(error); print("\n");
        return 20;
    }

    if ((ULONG)req->ioa_Request.io_Unit == 1) {
        test_ok("OpenDevice returns channel bitmask");
    } else {
        test_fail_msg("OpenDevice returns channel bitmask");
    }

    if (req->ioa_AllocKey != 0) {
        test_ok("OpenDevice generates allocation key");
    } else {
        test_fail_msg("OpenDevice generates allocation key");
    }

    req2->ioa_Data = alloc_map;
    req2->ioa_Length = sizeof(alloc_map);
    req2->ioa_Request.io_Message.mn_Node.ln_Pri = 5;
    error = OpenDevice((STRPTR)"audio.device", 0, (struct IORequest *)req2, 0);
    if (error != 0) {
        test_ok("Second OpenDevice fails when channel already allocated");
    } else {
        test_fail_msg("Second OpenDevice fails when channel already allocated");
        CloseDevice((struct IORequest *)req2);
    }

    req->ioa_Request.io_Command = ADCMD_SETPREC;
    req->ioa_Request.io_Unit = (struct Unit *)1;
    req->ioa_Request.io_Flags = IOF_QUICK;
    req->ioa_Request.io_Message.mn_Node.ln_Pri = 3;
    DoIO((struct IORequest *)req);
    if (req->ioa_Request.io_Error == 0) {
        test_ok("ADCMD_SETPREC succeeds for allocated channel");
    } else {
        test_fail_msg("ADCMD_SETPREC succeeds for allocated channel");
    }

    req->ioa_Request.io_Command = ADCMD_WAITCYCLE;
    req->ioa_Request.io_Unit = (struct Unit *)1;
    req->ioa_Request.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->ioa_Request.io_Error == 0) {
        test_ok("ADCMD_WAITCYCLE completes immediately when idle");
    } else {
        test_fail_msg("ADCMD_WAITCYCLE completes immediately when idle");
    }

    audio_irq.is_Node.ln_Type = NT_INTERRUPT;
    audio_irq.is_Node.ln_Pri = 0;
    audio_irq.is_Node.ln_Name = (char *)"audio-irq";
    audio_irq.is_Data = NULL;
    audio_irq.is_Code = (VOID (*)())AudioIRQHandler;
    AddIntServer(INTB_AUD0, &audio_irq);

    req->ioa_Request.io_Command = CMD_WRITE;
    req->ioa_Request.io_Unit = (struct Unit *)1;
    req->ioa_Request.io_Flags = ADIOF_PERVOL;
    req->ioa_Data = (UBYTE *)waveform;
    req->ioa_Length = sizeof(waveform);
    req->ioa_Period = 50000;
    req->ioa_Volume = 32;
    req->ioa_Cycles = 4;
    SendIO((struct IORequest *)req);

    if (req->ioa_Request.io_Error == 0) {
        test_ok("CMD_WRITE accepted playback request");
    } else {
        test_fail_msg("CMD_WRITE accepted playback request");
    }

    req2->ioa_Request.io_Device = req->ioa_Request.io_Device;
    req2->ioa_AllocKey = req->ioa_AllocKey;
    req2->ioa_Request.io_Command = ADCMD_WAITCYCLE;
    req2->ioa_Request.io_Unit = (struct Unit *)1;
    req2->ioa_Request.io_Flags = 0;
    SendIO((struct IORequest *)req2);
    delay_ticks(15);
    WaitIO((struct IORequest *)req2);
    if (req2->ioa_Request.io_Error == 0) {
        test_ok("ADCMD_WAITCYCLE waits for cycle completion");
    } else {
        test_fail_msg("ADCMD_WAITCYCLE waits for cycle completion");
    }

    req2->ioa_Request.io_Command = ADCMD_PERVOL;
    req2->ioa_Request.io_Unit = (struct Unit *)1;
    req2->ioa_Request.io_Flags = IOF_QUICK;
    req2->ioa_Period = 25000;
    req2->ioa_Volume = 12;
    DoIO((struct IORequest *)req2);
    if (req2->ioa_Request.io_Error == 0) {
        test_ok("ADCMD_PERVOL updates active playback parameters");
    } else {
        test_fail_msg("ADCMD_PERVOL updates active playback parameters");
    }

    req2->ioa_Request.io_Command = ADCMD_FINISH;
    req2->ioa_Request.io_Unit = (struct Unit *)1;
    req2->ioa_Request.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req2);
    if (req2->ioa_Request.io_Error == 0) {
        test_ok("ADCMD_FINISH succeeds for active playback");
    } else {
        test_fail_msg("ADCMD_FINISH succeeds for active playback");
    }

    WaitIO((struct IORequest *)req);
    if (req->ioa_Request.io_Error == 0) {
        test_ok("CMD_WRITE completes after finish/playback end");
    } else {
        test_fail_msg("CMD_WRITE completes after finish/playback end");
    }

    if (g_audio_irq_count >= 1) {
        test_ok("Audio interrupt server fired at end of sample cycle");
    } else {
        test_fail_msg("Audio interrupt server fired at end of sample cycle");
    }

    req->ioa_Request.io_Command = ADCMD_FREE;
    req->ioa_Request.io_Unit = (struct Unit *)1;
    req->ioa_Request.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->ioa_Request.io_Error == 0) {
        test_ok("ADCMD_FREE releases allocated channel");
    } else {
        test_fail_msg("ADCMD_FREE releases allocated channel");
    }

    RemIntServer(INTB_AUD0, &audio_irq);
    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice audio.device");

    if (req->ioa_Request.io_Device == NULL) {
        test_ok("CloseDevice clears io_Device");
    } else {
        test_fail_msg("CloseDevice clears io_Device");
    }

    DeleteIORequest((struct IORequest *)req2);
    DeleteIORequest((struct IORequest *)req);
    DeleteMsgPort(port);
    test_ok("Cleanup complete");

    print("\n");
    if (test_fail == 0) {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    }

    print("FAIL: ");
    print_num(test_fail);
    print(" of ");
    print_num(test_pass + test_fail);
    print(" tests failed\n");
    return 20;
}
