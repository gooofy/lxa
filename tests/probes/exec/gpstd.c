/*
 * Probe (Phase 237e): gameport.device's answer to the standard exec
 * commands CMD_CLEAR, CMD_RESET, CMD_FLUSH, CMD_STOP and CMD_START sent
 * with DoIO().  Fred Fish BattleForce (disk 205) sends CMD_CLEAR and quits
 * when DoIO() returns non-zero.  DoIO() is called from assembler with
 * known values in D0/D1; prints io_Error for D0 = $11 and for D0 = 0.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/gameport.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;
static struct IOStdReq *req;

static BYTE doio_d0(UWORD cmd, ULONG d0v)
{
    register APTR r_a1 __asm("a1") = req;
    register ULONG r_d0 __asm("d0") = d0v;
    register APTR r_a2 __asm("a2") = SysBase;
    req->io_Command = cmd;
    req->io_Error = 0x5c;
    __asm__ __volatile__ ("movem.l d2-d7/a6,-(sp)\n\tmove.l a2,a6\n\tmoveq #0x22,d1\n\t"
                          "jsr -456(a6)\n\tmovem.l (sp)+,d2-d7/a6"
                          : "+r" (r_a1), "+r" (r_d0), "+r" (r_a2) : : "d1", "a0", "cc", "memory");
    return req->io_Error;
}

int main(void)
{
    struct MsgPort *port = CreatePort(0, 0);
    static const struct { UWORD cmd; const char *name; } cmds[] = {
        { CMD_CLEAR, "CMD_CLEAR" }, { CMD_RESET, "CMD_RESET" }, { CMD_FLUSH, "CMD_FLUSH" },
        { CMD_STOP, "CMD_STOP" }, { CMD_START, "CMD_START" }
    };
    unsigned i;

    if (!port)
        return 20;
    req = (struct IOStdReq *)CreateExtIO(port, sizeof(struct IOStdReq));
    if (!req || OpenDevice((STRPTR)"gameport.device", 1, (struct IORequest *)req, 0))
        return 20;
    for (i = 0; i < 5; i++) {
        probe_s(cmds[i].name);
        probe_s(": D0=$11 -> ");
        probe_dec(doio_d0(cmds[i].cmd, 0x11));
        probe_s(", D0=0 -> ");
        probe_dec(doio_d0(cmds[i].cmd, 0));
        probe_ch('\n');
    }
    CloseDevice((struct IORequest *)req);
    DeleteExtIO((struct IORequest *)req);
    DeletePort(port);
    return 0;
}
