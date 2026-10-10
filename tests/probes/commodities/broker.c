/*
 * Probe: commodities.library brokers - CxBroker() (names, NBU_UNIQUE /
 * NBU_NOTIFY, error codes, nb_Version), ActivateCxObj() on brokers,
 * SetCxObjPri(), the private CopyBrokerList(),
 * FreeBrokerList() and BrokerCommand(), and the CXM_COMMAND messages a
 * broker's port receives.  Output compared with AmigaOS 3.1.
 */
#include <string.h>
#include "cxprobe.h"

static struct NewBroker nb;

static void set_nb(const char *name, WORD unique, WORD flags, BYTE pri, struct MsgPort *port)
{
    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)name;
    nb.nb_Title = (STRPTR)"Probe title";
    nb.nb_Descr = (STRPTR)"Probe description";
    nb.nb_Unique = unique;
    nb.nb_Flags = flags;
    nb.nb_Pri = pri;
    nb.nb_Port = port;
    nb.nb_ReservedChannel = 0;
}

static int is_probe(const char *s)
{
    return s[0] == 'P' && s[1] == 'r' && s[2] == 'o' && s[3] == 'b' && s[4] == 'e';
}

static void list_brokers(const char *label, struct MsgPort *port)
{
    struct List l;
    struct BrokerCopy *bc;
    LONG n, mine = 0;

    NewList(&l);
    n = CopyBrokerList(&l);
    for (bc = (struct BrokerCopy *)l.lh_Head; bc->bc_Node.ln_Succ;
         bc = (struct BrokerCopy *)bc->bc_Node.ln_Succ) {
        if (!is_probe(bc->bc_Name))
            continue;
        mine++;
        probe_s(label);
        probe_s(": \"");
        probe_s(bc->bc_Name);
        probe_s("\" title=\"");
        probe_s(bc->bc_Title);
        probe_s("\" descr=\"");
        probe_s(bc->bc_Descr);
        probe_s("\" flags=");
        probe_hex((UWORD)bc->bc_Flags, 4);
        probe_s(" nodename=");
        probe_s(bc->bc_Node.ln_Name == bc->bc_Name ? "bc_Name" : "other");
        probe_s(" task=");
        probe_s(bc->bc_Task == FindTask(NULL) ? "me" : (bc->bc_Task ? "other" : "NULL"));
        probe_s(" port=");
        probe_s(bc->bc_Port == port ? "mine" : (bc->bc_Port ? "other" : "NULL"));
        probe_ch('\n');
    }
    probe_s(label);
    probe_s(": probe brokers = ");
    probe_dec(mine);
    probe_s(", count >= mine: ");
    probe_s(n >= mine ? "yes" : "no");
    probe_ch('\n');
    FreeBrokerList(&l);
    probe_s(label);
    probe_s(": list empty after FreeBrokerList: ");
    probe_s(l.lh_TailPred == (struct Node *)&l ? "yes" : "no");
    probe_ch('\n');
}

int main(void)
{
    struct MsgPort *port, *port2;
    CxObj *b1, *b2, *b3, *b4;
    LONG err;

    if (!cx_open())
        return 20;
    port = CreateMsgPort();
    port2 = CreateMsgPort();

    P_SECTION("library");
    P_LONG("lib_Version >= 37", CxBase->lib_Version >= 37);

    P_SECTION("CxBroker");
    set_nb("ProbeA", NBU_UNIQUE | NBU_NOTIFY, COF_SHOW_HIDE, 0, port);
    err = -99;
    b1 = CxBroker(&nb, &err);
    P_NULL("ProbeA", b1);
    P_LONG("error", err);
    P_LONG("CxObjType", CxObjType(b1));
    P_LONG("CxObjError", CxObjError(b1));
    drain("after create", port);

    err = -99;
    b2 = CxBroker(&nb, &err);
    P_NULL("ProbeA again (UNIQUE|NOTIFY)", b2);
    P_LONG("error", err);
    drain("ProbeA port", port);

    set_nb("ProbeA", NBU_UNIQUE, 0, 0, port2);
    err = -99;
    b2 = CxBroker(&nb, &err);
    P_NULL("ProbeA again (UNIQUE)", b2);
    P_LONG("error", err);
    drain("ProbeA port", port);

    set_nb("PROBEA", NBU_UNIQUE | NBU_NOTIFY, 0, 0, port2);
    err = -99;
    b2 = CxBroker(&nb, &err);
    P_NULL("PROBEA (UNIQUE|NOTIFY, other case)", b2);
    P_LONG("error", err);
    drain("ProbeA port", port);
    if (b2)
        DeleteCxObj(b2);
    drain("ProbeA port after delete", port);

    set_nb("ProbeA", NBU_DUPLICATE, 0, 0, port2);
    err = -99;
    b2 = CxBroker(&nb, &err);
    P_NULL("ProbeA again (DUPLICATE)", b2);
    P_LONG("error", err);
    drain("ProbeA port", port);

    set_nb("ProbeB", NBU_UNIQUE, 0, 0, port2);
    b3 = CxBroker(&nb, NULL);
    P_NULL("ProbeB, error NULL", b3);

    set_nb("ProbeC with a name longer than 24 characters", NBU_UNIQUE, 0, 5, NULL);
    nb.nb_Title = (STRPTR)"A title that is longer than the forty characters a copy holds";
    err = -99;
    b4 = CxBroker(&nb, &err);
    P_NULL("ProbeC long, pri 5, no port", b4);
    P_LONG("error", err);

    P_SECTION("CxBroker nb_Version");
    {
        static const BYTE vers[] = { 0, 4, 6, 127 };
        int i;
        for (i = 0; i < 4; i++) {
            CxObj *b;
            set_nb("ProbeV", NBU_UNIQUE, 0, 0, NULL);
            nb.nb_Version = vers[i];
            err = -99;
            b = CxBroker(&nb, &err);
            probe_s("nb_Version ");
            probe_dec(vers[i]);
            probe_s(b ? ": non-NULL" : ": NULL");
            probe_s(" error ");
            probe_dec(err);
            probe_ch('\n');
            if (b)
                DeleteCxObj(b);
        }
    }

    P_SECTION("broker list");
    list_brokers("list", port);

    P_SECTION("ActivateCxObj");
    P_LONG("ActivateCxObj(b1, TRUE)", ActivateCxObj(b1, TRUE));
    P_LONG("ActivateCxObj(b1, TRUE)", ActivateCxObj(b1, TRUE));
    P_LONG("ActivateCxObj(b1, FALSE)", ActivateCxObj(b1, FALSE));
    P_LONG("ActivateCxObj(b1, FALSE)", ActivateCxObj(b1, FALSE));
    P_LONG("ActivateCxObj(b1, 5)", ActivateCxObj(b1, 5));
    P_LONG("ActivateCxObj(NULL, TRUE)", ActivateCxObj(NULL, TRUE));
    drain("ProbeA port", port);
    list_brokers("after activate", port);

    P_SECTION("SetCxObjPri");
    P_LONG("SetCxObjPri(b3, 10)", SetCxObjPri(b3, 10));
    P_LONG("SetCxObjPri(b3, -3)", SetCxObjPri(b3, -3));
    P_LONG("SetCxObjPri(b3, 0)", SetCxObjPri(b3, 0));
    P_LONG("SetCxObjPri(NULL, 1)", SetCxObjPri(NULL, 1));
    list_brokers("after pri", port);

    P_SECTION("BrokerCommand");
    P_LONG("BrokerCommand(ProbeA, CXCMD_DISABLE)", BrokerCommand((STRPTR)"ProbeA", CXCMD_DISABLE));
    drain("ProbeA port", port);
    P_LONG("BrokerCommand(ProbeA, CXCMD_APPEAR)", BrokerCommand((STRPTR)"ProbeA", CXCMD_APPEAR));
    P_LONG("BrokerCommand(ProbeA, 1234)", BrokerCommand((STRPTR)"ProbeA", 1234));
    drain("ProbeA port", port);
    P_LONG("BrokerCommand(probea, CXCMD_ENABLE)", BrokerCommand((STRPTR)"probea", CXCMD_ENABLE));
    drain("ProbeA port", port);
    P_LONG("BrokerCommand(NoSuchBroker, CXCMD_ENABLE)", BrokerCommand((STRPTR)"NoSuchBroker", CXCMD_ENABLE));
    {
        /* broker without a port */
        char name[CBD_NAMELEN];
        struct List l;
        struct BrokerCopy *bc;
        name[0] = 0;
        NewList(&l);
        CopyBrokerList(&l);
        for (bc = (struct BrokerCopy *)l.lh_Head; bc->bc_Node.ln_Succ;
             bc = (struct BrokerCopy *)bc->bc_Node.ln_Succ)
            if (bc->bc_Name[0] == 'P' && bc->bc_Name[5] == 'C')
                strcpy(name, bc->bc_Name);
        FreeBrokerList(&l);
        P_LONG("BrokerCommand(ProbeC..., CXCMD_APPEAR) (no port)", BrokerCommand((STRPTR)name, CXCMD_APPEAR));
    }
    drain("ProbeB port", port2);

    P_SECTION("DeleteCxObj");
    DeleteCxObj(b2);
    DeleteCxObj(b3);
    DeleteCxObj(b4);
    drain("ProbeA port", port);
    list_brokers("after delete", port);
    DeleteCxObj(b1);
    list_brokers("after delete all", port);
    DeleteCxObj(NULL);
    P_SECTION("done");

    DeleteMsgPort(port2);
    DeleteMsgPort(port);
    CloseLibrary(CxBase);
    return 0;
}
