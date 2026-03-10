/*
 * Test for input.device handler ordering, event dispatch, and qualifier state.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <proto/input.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Device *InputBase;

static UWORD g_first_count = 0;
static UWORD g_second_count = 0;
static UWORD g_first_last_class = 0;
static UWORD g_second_last_class = 0;
static UWORD g_first_last_code = 0;
static UWORD g_second_last_code = 0;
static UWORD g_first_last_qual = 0;
static UWORD g_second_last_qual = 0;
static WORD g_first_last_x = 0;
static WORD g_first_last_y = 0;
static WORD g_second_last_x = 0;
static WORD g_second_last_y = 0;

static void print(const char *s)
{
    BPTR out = Output();
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
        while (j > 0) {
            buf[i++] = temp[--j];
        }
    }
    buf[i] = '\0';
    print(buf);
}

static void test_ok(const char *msg)
{
    print("OK: ");
    print(msg);
    print("\n");
}

static void test_fail(const char *msg)
{
    print("FAIL: ");
    print(msg);
    print("\n");
}

static struct InputEvent *first_handler(register struct InputEvent *events __asm("a0"),
                                        register APTR data __asm("a1"))
{
    struct InputEvent *event = events;
    UWORD mark = (UWORD)(ULONG)data;

    if (event) {
        g_first_count++;
        g_first_last_class = event->ie_Class;
        g_first_last_code = event->ie_Code;
        g_first_last_qual = event->ie_Qualifier | mark;
        g_first_last_x = event->ie_X;
        g_first_last_y = event->ie_Y;
        event->ie_Qualifier |= mark;
    }

    return events;
}

static struct InputEvent *second_handler(register struct InputEvent *events __asm("a0"),
                                         register APTR data __asm("a1"))
{
    struct InputEvent *event = events;
    UWORD mark = (UWORD)(ULONG)data;

    if (event) {
        g_second_count++;
        g_second_last_class = event->ie_Class;
        g_second_last_code = event->ie_Code;
        g_second_last_qual = event->ie_Qualifier | mark;
        g_second_last_x = event->ie_X;
        g_second_last_y = event->ie_Y;
        event->ie_Qualifier |= mark;
    }

    return events;
}

static void reset_counters(void)
{
    g_first_count = 0;
    g_second_count = 0;
    g_first_last_class = 0;
    g_second_last_class = 0;
    g_first_last_code = 0;
    g_second_last_code = 0;
    g_first_last_qual = 0;
    g_second_last_qual = 0;
    g_first_last_x = 0;
    g_first_last_y = 0;
    g_second_last_x = 0;
    g_second_last_y = 0;
}

int main(void)
{
    struct MsgPort *input_port;
    struct IOStdReq *input_req;
    struct Interrupt first_interrupt;
    struct Interrupt second_interrupt;
    struct InputEvent event;
    struct InputEvent add_events[2];
    struct TimeRequest *time_req;
    UBYTE mouse_value;
    LONG error;

    print("Testing input.device\n");

    input_port = CreateMsgPort();
    if (!input_port) {
        test_fail("CreateMsgPort failed");
        return 1;
    }

    input_req = (struct IOStdReq *)CreateIORequest(input_port, sizeof(struct IOStdReq));
    if (!input_req) {
        test_fail("CreateIORequest failed");
        DeleteMsgPort(input_port);
        return 1;
    }

    error = OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)input_req, 0);
    if (error != 0) {
        test_fail("OpenDevice(input.device) failed");
        DeleteIORequest((struct IORequest *)input_req);
        DeleteMsgPort(input_port);
        return 1;
    }
    InputBase = input_req->io_Device;
    test_ok("input.device opened");

    if (input_req->io_Device == InputBase && InputBase != NULL) {
        test_ok("OpenDevice stored InputBase");
    } else {
        test_fail("OpenDevice did not store InputBase");
        CloseDevice((struct IORequest *)input_req);
        DeleteIORequest((struct IORequest *)input_req);
        DeleteMsgPort(input_port);
        return 1;
    }

    first_interrupt.is_Node.ln_Type = NT_INTERRUPT;
    first_interrupt.is_Node.ln_Pri = 10;
    first_interrupt.is_Node.ln_Name = (STRPTR)"input-first";
    first_interrupt.is_Data = (APTR)0x0100;
    first_interrupt.is_Code = (VOID (*)())first_handler;

    second_interrupt.is_Node.ln_Type = NT_INTERRUPT;
    second_interrupt.is_Node.ln_Pri = 5;
    second_interrupt.is_Node.ln_Name = (STRPTR)"input-second";
    second_interrupt.is_Data = (APTR)0x0200;
    second_interrupt.is_Code = (VOID (*)())second_handler;

    input_req->io_Command = IND_ADDHANDLER;
    input_req->io_Data = &second_interrupt;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);
    if (input_req->io_Error == 0) {
        test_ok("IND_ADDHANDLER adds lower priority handler");
    } else {
        test_fail("IND_ADDHANDLER lower priority failed");
        CloseDevice((struct IORequest *)input_req);
        DeleteIORequest((struct IORequest *)input_req);
        DeleteMsgPort(input_port);
        return 1;
    }

    input_req->io_Command = IND_ADDHANDLER;
    input_req->io_Data = &first_interrupt;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);
    if (input_req->io_Error == 0) {
        test_ok("IND_ADDHANDLER adds higher priority handler");
    } else {
        test_fail("IND_ADDHANDLER higher priority failed");
        CloseDevice((struct IORequest *)input_req);
        DeleteIORequest((struct IORequest *)input_req);
        DeleteMsgPort(input_port);
        return 1;
    }

    reset_counters();
    event.ie_NextEvent = NULL;
    event.ie_Class = IECLASS_RAWKEY;
    event.ie_SubClass = 0;
    event.ie_Code = 0x20;
    event.ie_Qualifier = IEQUALIFIER_LSHIFT;
    event.ie_X = 12;
    event.ie_Y = 34;

    input_req->io_Command = IND_WRITEEVENT;
    input_req->io_Data = &event;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);

    if (input_req->io_Error == 0) {
        test_ok("IND_WRITEEVENT succeeded");
    } else {
        test_fail("IND_WRITEEVENT failed");
    }

    if (g_first_count == 1 && g_second_count == 1) {
        test_ok("Both handlers received rawkey event");
    } else {
        test_fail("Handlers did not receive rawkey event");
    }

    if (g_first_last_class == IECLASS_RAWKEY && g_second_last_class == IECLASS_RAWKEY) {
        test_ok("Rawkey class delivered");
    } else {
        test_fail("Wrong class delivered to handlers");
    }

    if ((g_second_last_qual & 0x0300) == 0x0300) {
        test_ok("Handler chain preserves priority ordering and mutations");
    } else {
        test_fail("Handler ordering/mutation mismatch");
    }

    if (PeekQualifier() == IEQUALIFIER_LSHIFT) {
        test_ok("PeekQualifier tracks rawkey qualifier state");
    } else {
        test_fail("PeekQualifier rawkey state mismatch");
    }

    if (g_first_last_x == 12 && g_first_last_y == 34 && g_second_last_x == 12 && g_second_last_y == 34) {
        test_ok("Event coordinates preserved");
    } else {
        test_fail("Event coordinates mismatch");
    }

    reset_counters();
    add_events[0].ie_NextEvent = NULL;
    add_events[0].ie_Class = IECLASS_RAWKEY;
    add_events[0].ie_SubClass = 0;
    add_events[0].ie_Code = 0x60;
    add_events[0].ie_Qualifier = IEQUALIFIER_LSHIFT;
    add_events[0].ie_X = 0;
    add_events[0].ie_Y = 0;

    add_events[1].ie_NextEvent = NULL;
    add_events[1].ie_Class = IECLASS_POINTERPOS;
    add_events[1].ie_SubClass = 0;
    add_events[1].ie_Code = 0;
    add_events[1].ie_Qualifier = IEQUALIFIER_LEFTBUTTON;
    add_events[1].ie_X = 77;
    add_events[1].ie_Y = 88;

    input_req->io_Command = IND_ADDEVENT;
    input_req->io_Data = add_events;
    input_req->io_Length = sizeof(add_events);
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);

    if (input_req->io_Error == 0) {
        test_ok("IND_ADDEVENT succeeded");
    } else {
        test_fail("IND_ADDEVENT failed");
    }

    if (g_first_count == 1 && g_second_count == 1) {
        test_ok("IND_ADDEVENT filters unsupported classes");
    } else {
        test_fail("IND_ADDEVENT class filtering mismatch");
    }

    mouse_value = 1;
    input_req->io_Command = IND_SETMPORT;
    input_req->io_Data = &mouse_value;
    input_req->io_Length = 1;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);
    if (input_req->io_Error == 0) {
        test_ok("IND_SETMPORT accepted byte payload");
    } else {
        test_fail("IND_SETMPORT failed");
    }

    mouse_value = 2;
    input_req->io_Command = IND_SETMTYPE;
    input_req->io_Data = &mouse_value;
    input_req->io_Length = 1;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);
    if (input_req->io_Error == 0) {
        test_ok("IND_SETMTYPE accepted byte payload");
    } else {
        test_fail("IND_SETMTYPE failed");
    }

    time_req = (struct TimeRequest *)input_req;
    time_req->tr_node.io_Command = IND_SETTHRESH;
    time_req->tr_node.io_Flags = IOF_QUICK;
    time_req->tr_time.tv_secs = 0;
    time_req->tr_time.tv_micro = 250000;
    DoIO((struct IORequest *)time_req);
    if (time_req->tr_node.io_Error == 0) {
        test_ok("IND_SETTHRESH accepted timeval");
    } else {
        test_fail("IND_SETTHRESH failed");
    }

    time_req->tr_node.io_Command = IND_SETPERIOD;
    time_req->tr_node.io_Flags = IOF_QUICK;
    time_req->tr_time.tv_secs = 0;
    time_req->tr_time.tv_micro = 30000;
    DoIO((struct IORequest *)time_req);
    if (time_req->tr_node.io_Error == 0) {
        test_ok("IND_SETPERIOD accepted timeval");
    } else {
        test_fail("IND_SETPERIOD failed");
    }

    input_req->io_Command = IND_SETMPORT;
    input_req->io_Data = NULL;
    input_req->io_Length = 1;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);
    if (input_req->io_Error == IOERR_BADADDRESS) {
        test_ok("IND_SETMPORT rejects NULL data");
    } else {
        test_fail("IND_SETMPORT NULL-data error mismatch");
    }

    input_req->io_Command = IND_ADDEVENT;
    input_req->io_Data = add_events;
    input_req->io_Length = sizeof(struct InputEvent) + 1;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);
    if (input_req->io_Error == IOERR_BADLENGTH) {
        test_ok("IND_ADDEVENT rejects bad length");
    } else {
        test_fail("IND_ADDEVENT bad-length error mismatch");
    }

    input_req->io_Command = IND_REMHANDLER;
    input_req->io_Data = &first_interrupt;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);
    if (input_req->io_Error == 0) {
        test_ok("IND_REMHANDLER removed first handler");
    } else {
        test_fail("IND_REMHANDLER first handler failed");
    }

    input_req->io_Command = IND_REMHANDLER;
    input_req->io_Data = &second_interrupt;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);
    if (input_req->io_Error == 0) {
        test_ok("IND_REMHANDLER removed second handler");
    } else {
        test_fail("IND_REMHANDLER second handler failed");
    }

    reset_counters();
    event.ie_Class = IECLASS_RAWMOUSE;
    event.ie_Code = IECODE_LBUTTON;
    event.ie_Qualifier = IEQUALIFIER_LEFTBUTTON;
    event.ie_X = 9;
    event.ie_Y = 11;

    input_req->io_Command = IND_WRITEEVENT;
    input_req->io_Data = &event;
    input_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)input_req);

    if (g_first_count == 0 && g_second_count == 0) {
        test_ok("Removed handlers stop receiving events");
    } else {
        test_fail("Removed handlers still received events");
    }

    if (PeekQualifier() == IEQUALIFIER_LEFTBUTTON) {
        test_ok("PeekQualifier tracks mouse qualifier state");
    } else {
        test_fail("PeekQualifier mouse state mismatch");
    }

    CloseDevice((struct IORequest *)input_req);
    DeleteIORequest((struct IORequest *)input_req);
    DeleteMsgPort(input_port);

    print("PASS: input.device tests passed\n");
    return 0;
}
