/*
 * cxprobe.h - common declarations for the commodities.library probes.
 *
 * The private entries (CopyBrokerList -186, FreeBrokerList -192,
 * BrokerCommand -198) and struct BrokerCopy are not
 * in the NDK; Exchange and Scout use them (layout from Scout's
 * commodities_private.h).
 */
#ifndef CXPROBE_H
#define CXPROBE_H

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/ports.h>
#include <exec/libraries.h>
#include <devices/inputevent.h>
#include <libraries/commodities.h>
#include <clib/exec_protos.h>
#include <clib/commodities_protos.h>
#include <inline/exec.h>
#include <inline/commodities.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *CxBase;

struct BrokerCopy
{
    struct Node     bc_Node;
    char            bc_Name[CBD_NAMELEN];
    char            bc_Title[CBD_TITLELEN];
    char            bc_Descr[CBD_DESCRLEN];
    struct Task    *bc_Task;
    struct MsgPort *bc_Port;
    ULONG           bc_Dummy2;
    WORD            bc_Flags;
};

#define CopyBrokerList(list) \
    LP1(0xba, LONG, CopyBrokerList, struct List *, list, a0, , CxBase)
#define FreeBrokerList(list) \
    LP1NR(0xc0, FreeBrokerList, struct List *, list, a0, , CxBase)
#define BrokerCommand(name, id) \
    LP2(0xc6, LONG, BrokerCommand, STRPTR, name, a0, LONG, id, d0, , CxBase)

static int cx_open(void)
{
    CxBase = OpenLibrary((STRPTR)"commodities.library", 37);
    if (!CxBase) {
        P_STR("commodities.library", "missing");
        return 0;
    }
    return 1;
}

/* print every message waiting at port, reply it */
static void drain(const char *label, struct MsgPort *port)
{
    struct Message *m;
    int n = 0;
    while ((m = GetMsg(port)) != NULL) {
        CxMsg *cxm = (CxMsg *)m;
        struct InputEvent *ie = (struct InputEvent *)CxMsgData(cxm);
        probe_s(label);
        probe_s(" msg ");
        probe_dec(n++);
        probe_s(": type=");
        probe_hex(CxMsgType(cxm), 2);
        probe_s(" id=");
        probe_dec(CxMsgID(cxm));
        if (CxMsgType(cxm) == CXM_IEVENT && ie) {
            probe_s(" class=");
            probe_hex(ie->ie_Class, 2);
            probe_s(" code=");
            probe_hex(ie->ie_Code, 4);
            probe_s(" qual=");
            probe_hex(ie->ie_Qualifier, 4);
        } else {
            probe_s(ie ? " data=non-NULL" : " data=NULL");
        }
        probe_ch('\n');
        ReplyMsg(m);
    }
    if (!n) {
        probe_s(label);
        probe_s(" no messages\n");
    }
}

#endif
