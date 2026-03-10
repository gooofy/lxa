/*
 * Test: console/keymap_unit
 *
 * Verifies console.device keymap commands and CONU_LIBRARY handling.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <devices/inputevent.h>
#include <devices/keymap.h>
#include <libraries/keymap.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct ExecBase *SysBase;

#define RAWKEY_A_CODE 0x20
#define LO_KEYS 0x40
#define HI_KEYS 0x38
#define LO_BITS (LO_KEYS / 8)
#define HI_BITS (HI_KEYS / 8)

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

static struct KeyMap original_map;
static struct KeyMap verify_map;
static struct KeyMap custom_map;
static ULONG custom_lo_map[LO_KEYS];
static ULONG custom_hi_map[HI_KEYS];
static UBYTE custom_lo_types[LO_KEYS];
static UBYTE custom_hi_types[HI_KEYS];
static UBYTE custom_lo_caps[LO_BITS];
static UBYTE custom_lo_repeat[LO_BITS];
static UBYTE custom_hi_caps[HI_BITS];
static UBYTE custom_hi_repeat[HI_BITS];
static char convert_buf[16];
static char read_buf[4];
static struct NewWindow nw = {0};

static BOOL ask_keymap(struct IOStdReq *req, struct KeyMap *map)
{
    req->io_Command = CD_ASKKEYMAP;
    req->io_Data = (APTR)map;
    req->io_Length = sizeof(*map);
    DoIO((struct IORequest *)req);
    return req->io_Error == 0 && req->io_Actual == sizeof(*map);
}

static BOOL ask_default_keymap(struct IOStdReq *req, struct KeyMap *map)
{
    req->io_Command = CD_ASKDEFAULTKEYMAP;
    req->io_Data = (APTR)map;
    req->io_Length = sizeof(*map);
    DoIO((struct IORequest *)req);
    return req->io_Error == 0 && req->io_Actual == sizeof(*map);
}

static BOOL set_keymap(struct IOStdReq *req, struct KeyMap *map)
{
    req->io_Command = CD_SETKEYMAP;
    req->io_Data = (APTR)map;
    req->io_Length = sizeof(*map);
    DoIO((struct IORequest *)req);
    return req->io_Error == 0 && req->io_Actual == sizeof(*map);
}

int main(void)
{
    struct MsgPort *lib_port = NULL;
    struct IOStdReq *lib_req = NULL;
    struct MsgPort *console_port = NULL;
    struct IOStdReq *console_req = NULL;
    struct Window *window = NULL;
    LONG result;

    print("keymap_unit: testing console.device keymap commands\n");

    lib_port = CreateMsgPort();
    if (!lib_port) {
        print("FAIL: could not create library-mode port\n");
        return 1;
    }

    lib_req = (struct IOStdReq *)CreateIORequest(lib_port, sizeof(struct IOStdReq));
    if (!lib_req) {
        print("FAIL: could not create library-mode request\n");
        DeleteMsgPort(lib_port);
        return 1;
    }

    result = OpenDevice((STRPTR)"console.device", CONU_LIBRARY, (struct IORequest *)lib_req, 0);
    if (result != 0) {
        print("FAIL: CONU_LIBRARY open failed, error=");
        print_num(result);
        print("\n");
        DeleteIORequest((struct IORequest *)lib_req);
        DeleteMsgPort(lib_port);
        return 1;
    }

    if (lib_req->io_Unit != NULL || lib_req->io_Device == NULL) {
        print("FAIL: CONU_LIBRARY did not return library-only handle\n");
        CloseDevice((struct IORequest *)lib_req);
        DeleteIORequest((struct IORequest *)lib_req);
        DeleteMsgPort(lib_port);
        return 1;
    }

    print("OK: CONU_LIBRARY open works\n");

    if (!ask_default_keymap(lib_req, &original_map)) {
        print("FAIL: CD_ASKDEFAULTKEYMAP failed\n");
        CloseDevice((struct IORequest *)lib_req);
        DeleteIORequest((struct IORequest *)lib_req);
        DeleteMsgPort(lib_port);
        return 1;
    }

    CloseDevice((struct IORequest *)lib_req);
    DeleteIORequest((struct IORequest *)lib_req);
    DeleteMsgPort(lib_port);

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: could not open intuition.library\n");
        return 1;
    }

    nw.LeftEdge = 0;
    nw.TopEdge = 0;
    nw.Width = 320;
    nw.Height = 120;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_RAWKEY;
    nw.Flags = WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_CLOSEGADGET;
    nw.Title = (UBYTE *)"Console Keymap Test";
    nw.Type = WBENCHSCREEN;

    window = OpenWindow(&nw);
    if (!window) {
        print("FAIL: could not open test window\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

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
        print("FAIL: could not open console unit, error=");
        print_num(result);
        print("\n");
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    CopyMem(original_map.km_LoKeyMapTypes, custom_lo_types, sizeof(custom_lo_types));
    CopyMem(original_map.km_LoKeyMap, custom_lo_map, sizeof(custom_lo_map));
    CopyMem(original_map.km_LoCapsable, custom_lo_caps, sizeof(custom_lo_caps));
    CopyMem(original_map.km_LoRepeatable, custom_lo_repeat, sizeof(custom_lo_repeat));
    CopyMem(original_map.km_HiKeyMapTypes, custom_hi_types, sizeof(custom_hi_types));
    CopyMem(original_map.km_HiKeyMap, custom_hi_map, sizeof(custom_hi_map));
    CopyMem(original_map.km_HiCapsable, custom_hi_caps, sizeof(custom_hi_caps));
    CopyMem(original_map.km_HiRepeatable, custom_hi_repeat, sizeof(custom_hi_repeat));

    custom_lo_map[RAWKEY_A_CODE] = 'z' | ('Z' << 8) | ('z' << 16) | ('Z' << 24);

    custom_map.km_LoKeyMapTypes = custom_lo_types;
    custom_map.km_LoKeyMap = custom_lo_map;
    custom_map.km_LoCapsable = custom_lo_caps;
    custom_map.km_LoRepeatable = custom_lo_repeat;
    custom_map.km_HiKeyMapTypes = custom_hi_types;
    custom_map.km_HiKeyMap = custom_hi_map;
    custom_map.km_HiCapsable = custom_hi_caps;
    custom_map.km_HiRepeatable = custom_hi_repeat;

    if (!set_keymap(console_req, &custom_map)) {
        print("FAIL: CD_SETKEYMAP failed\n");
        CloseDevice((struct IORequest *)console_req);
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    if (!ask_keymap(console_req, &verify_map)) {
        print("FAIL: CD_ASKKEYMAP after set failed\n");
        CloseDevice((struct IORequest *)console_req);
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    if (verify_map.km_LoKeyMap != custom_lo_map || verify_map.km_LoKeyMapTypes != custom_lo_types) {
        print("FAIL: CD_SETKEYMAP did not install custom unit keymap\n");
        CloseDevice((struct IORequest *)console_req);
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    print("OK: CD_ASKKEYMAP/CD_SETKEYMAP round-trip succeeded\n");
    print("Waiting for remapped input\n");

    console_req->io_Command = CMD_READ;
    console_req->io_Data = (APTR)read_buf;
    console_req->io_Length = 1;
    DoIO((struct IORequest *)console_req);

    if (console_req->io_Error != 0 || console_req->io_Actual != 1 || read_buf[0] != 'z') {
        print("FAIL: remapped CMD_READ returned '");
        if (console_req->io_Actual > 0) {
            Write(Output(), (CONST APTR)read_buf, 1);
        } else {
            print("?");
        }
        print("'\n");
        CloseDevice((struct IORequest *)console_req);
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    print("PASS: keymap_unit all tests passed\n");

    CloseDevice((struct IORequest *)console_req);
    DeleteIORequest((struct IORequest *)console_req);
    DeleteMsgPort(console_port);
    CloseWindow(window);
    CloseLibrary((struct Library *)IntuitionBase);

    return 0;
}
