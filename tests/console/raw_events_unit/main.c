/*
 * Test: console/raw_events_unit
 *
 * Verifies console.device raw event selection and read-stream reports.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct ExecBase *SysBase;

#define CSI "\x9b"

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++) {
        len++;
    }

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    int i = 0;

    if (n < 0) {
        print("-");
        n = -n;
    }

    if (n == 0) {
        print("0");
        return;
    }

    while (n > 0 && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    while (i > 0) {
        char c[2] = { buf[--i], '\0' };
        print(c);
    }
}

static LONG con_write(struct IOStdReq *req, const char *data, LONG len)
{
    req->io_Command = CMD_WRITE;
    req->io_Data = (APTR)data;
    req->io_Length = len;
    DoIO((struct IORequest *)req);
    return req->io_Actual;
}

static LONG con_puts(struct IOStdReq *req, const char *str)
{
    LONG len = 0;
    const char *p = str;

    while (*p++) {
        len++;
    }

    return con_write(req, str, len);
}

static void con_clear(struct IOStdReq *req)
{
    req->io_Command = CMD_CLEAR;
    DoIO((struct IORequest *)req);
}

static LONG con_read(struct IOStdReq *req, char *buf, LONG len)
{
    req->io_Command = CMD_READ;
    req->io_Data = (APTR)buf;
    req->io_Length = len;
    DoIO((struct IORequest *)req);
    return req->io_Actual;
}

static BOOL starts_with_bytes(const char *buf, LONG len, const char *prefix, LONG prefix_len)
{
    LONG i;

    if (len < prefix_len) {
        return FALSE;
    }

    for (i = 0; i < prefix_len; i++) {
        if ((UBYTE)buf[i] != (UBYTE)prefix[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL contains_char(const char *buf, LONG len, char c)
{
    LONG i;

    for (i = 0; i < len; i++) {
        if (buf[i] == c) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL expect_special_sequence(struct IOStdReq *req,
                                    char *buf,
                                    const char *label,
                                    const char *prefix,
                                    LONG prefix_len)
{
    LONG result = con_read(req, buf, 128);

    if (!starts_with_bytes(buf, result, prefix, prefix_len)) {
        print("FAIL: ");
        print(label);
        print(" mismatch, len=");
        print_num(result);
        print("\n");
        return FALSE;
    }

    return TRUE;
}

static BOOL expect_rawkey_or_special_sequence(struct IOStdReq *req,
                                              char *buf,
                                              const char *label,
                                              const char *raw_prefix,
                                              LONG raw_prefix_len,
                                              const char *special_prefix,
                                              LONG special_prefix_len)
{
    LONG result = con_read(req, buf, 128);

    if (!starts_with_bytes(buf, result, raw_prefix, raw_prefix_len) &&
        !starts_with_bytes(buf, result, special_prefix, special_prefix_len)) {
        print("FAIL: ");
        print(label);
        print(" mismatch\n");
        return FALSE;
    }

    return TRUE;
}

static BOOL expect_raw_report(struct IOStdReq *req,
                              char *buf,
                              const char *label,
                              const char *prefix,
                              LONG prefix_len)
{
    LONG result;
    int attempts;

    for (attempts = 0; attempts < 8; attempts++) {
        result = con_read(req, buf, 128);

        if (starts_with_bytes(buf, result, prefix, prefix_len) && contains_char(buf, result, '|')) {
            return TRUE;
        }
    }

    if (!starts_with_bytes(buf, result, prefix, prefix_len) || !contains_char(buf, result, '|')) {
        print("FAIL: ");
        print(label);
        print(" mismatch\n");
        return FALSE;
    }

    return TRUE;
}

int main(void)
{
    struct Window *window = NULL;
    struct MsgPort *console_port = NULL;
    struct IOStdReq *console_req = NULL;
    struct Gadget button = {0};
    struct Border border = {0};
    WORD border_data[] = { 0,0, 39,0, 39,11, 0,11, 0,0 };
    struct NewWindow nw = {0};
    char read_buf[128];
    LONG result;
    const char rawkey_enable[] = CSI "1{";
    const char rawkey_disable[] = CSI "1}";
    const char all_events_enable[] = CSI "1;2;7;8;12{";
    const char all_events_disable[] = CSI "1;2;7;8;12}";
    const char raw_special_prefix[] = { (char)0x9B, 'A', '~' };
    const char rawkey_prefix[] = { (char)0x9B, '1', ';', '0', ';' };
    const char rawmouse_prefix[] = { (char)0x9B, '2', ';', '0', ';' };
    const char gadgetdown_prefix[] = { (char)0x9B, '7', ';', '0', ';' };
    const char gadgetup_prefix[] = { (char)0x9B, '8', ';', '0', ';' };
    const char size_prefix[] = { (char)0x9B, '1', '2', ';', '0', ';', '0', ';', '0', ';', '0', ';', '0', ';' };

    print("raw_events_unit: testing console raw input reports\n");

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: could not open intuition.library\n");
        return 1;
    }

    border.LeftEdge = -1;
    border.TopEdge = -1;
    border.FrontPen = 1;
    border.BackPen = 0;
    border.DrawMode = JAM1;
    border.Count = 5;
    border.XY = border_data;
    border.NextBorder = NULL;

    button.NextGadget = NULL;
    button.LeftEdge = 70;
    button.TopEdge = 20;
    button.Width = 40;
    button.Height = 12;
    button.Flags = GFLG_GADGHCOMP;
    button.Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
    button.GadgetType = GTYP_BOOLGADGET;
    button.GadgetRender = &border;
    button.SelectRender = NULL;
    button.GadgetText = NULL;
    button.MutualExclude = 0;
    button.SpecialInfo = NULL;
    button.GadgetID = 7;
    button.UserData = NULL;

    nw.LeftEdge = 0;
    nw.TopEdge = 0;
    nw.Width = 320;
    nw.Height = 140;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_RAWKEY | IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_ACTIVEWINDOW |
                    IDCMP_INACTIVEWINDOW | IDCMP_CHANGEWINDOW | IDCMP_REFRESHWINDOW |
                    IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE;
    nw.Flags = WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_CLOSEGADGET |
               WFLG_SIZEGADGET | WFLG_DEPTHGADGET;
    nw.FirstGadget = &button;
    nw.Title = (UBYTE *)"Console Raw Event Test";
    nw.Type = WBENCHSCREEN;

    window = OpenWindow(&nw);
    if (!window) {
        print("FAIL: could not open test window\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    ActivateWindow(window);
    WaitTOF();

    console_port = CreateMsgPort();
    if (!console_port) {
        print("FAIL: could not create console port\n");
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    console_req = (struct IOStdReq *)CreateIORequest(console_port, sizeof(struct IOStdReq));
    if (!console_req) {
        print("FAIL: could not create console request\n");
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    console_req->io_Data = (APTR)window;
    console_req->io_Length = sizeof(struct Window);
    result = OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)console_req, 0);
    if (result != 0) {
        print("FAIL: could not open console.device, error=");
        print_num(result);
        print("\n");
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    con_clear(console_req);
    print("Waiting for special key report\n");
    if (!expect_special_sequence(console_req, read_buf, "special key report",
                                 (const char[]){ (char)0x9B, 'A' }, 2)) {
        goto fail;
    }
    print("OK: special key report returned expected sequence\n");

    con_puts(console_req, rawkey_enable);
    con_clear(console_req);
    print("Waiting for raw special-key report\n");
    if (!expect_rawkey_or_special_sequence(console_req, read_buf, "raw special key report",
                                           rawkey_prefix, sizeof(rawkey_prefix),
                                           raw_special_prefix, sizeof(raw_special_prefix))) {
        goto fail;
    }
    print("OK: raw special key report returned compatible sequence\n");

    print("Waiting for RAWKEY report\n");
    if (!expect_raw_report(console_req, read_buf, "raw key report",
                           rawkey_prefix, sizeof(rawkey_prefix))) {
        goto fail;
    }
    print("OK: raw key report returned expected prefix\n");

    con_puts(console_req, all_events_enable);
    con_clear(console_req);

    print("Waiting for raw mouse-button report\n");
    if (!expect_raw_report(console_req, read_buf, "raw mouse button report",
                           rawmouse_prefix, sizeof(rawmouse_prefix))) {
        goto fail;
    }
    print("OK: raw mouse-button report returned expected prefix\n");

    con_puts(console_req, CSI "2}");
    con_clear(console_req);

    print("Waiting for raw gadget-down report\n");
    if (!expect_raw_report(console_req, read_buf, "raw gadget-down report",
                           gadgetdown_prefix, sizeof(gadgetdown_prefix))) {
        goto fail;
    }
    print("OK: raw gadget-down report returned expected prefix\n");

    con_puts(console_req, CSI "7}");
    con_clear(console_req);

    con_puts(console_req, all_events_disable);
    con_puts(console_req, rawkey_disable);

    print("Waiting for size-window raw event report\n");
    con_puts(console_req, CSI "12{");
    SizeWindow(window, 16, 8);
    if (!expect_raw_report(console_req, read_buf, "size-window report",
                           size_prefix, sizeof(size_prefix))) {
        goto fail;
    }
    print("OK: size-window raw event report returned expected prefix\n");

    con_puts(console_req, CSI "12}");
    con_clear(console_req);
    print("Waiting for ASCII after raw reset\n");
    result = con_read(console_req, read_buf, sizeof(read_buf));
    if (result < 1 || read_buf[0] != 'a') {
        print("FAIL: raw event reset did not restore ASCII stream\n");
        goto fail;
    }
    print("OK: raw event reset restored ASCII stream\n");

    print("PASS: raw_events_unit all tests passed\n");

    CloseDevice((struct IORequest *)console_req);
    DeleteIORequest((struct IORequest *)console_req);
    DeleteMsgPort(console_port);
    CloseWindow(window);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;

fail:
    CloseDevice((struct IORequest *)console_req);
    DeleteIORequest((struct IORequest *)console_req);
    DeleteMsgPort(console_port);
    CloseWindow(window);
    CloseLibrary((struct Library *)IntuitionBase);
    return 1;
}
