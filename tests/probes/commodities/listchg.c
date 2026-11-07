/*
 * Probe (Phase 251): who gets BrokerCommand(NULL, id) and when
 * commodities.library tells the controller (Exchange) that the broker
 * list changed (CXCMD_LIST_CHG).  Exchange only refreshes its list on
 * that message.
 */
#include "cxprobe.h"

static struct MsgPort *pA, *pB, *pC;

static CxObj *broker(const char *name, struct MsgPort *port, BYTE flags)
{
    struct NewBroker nb;
    LONG err;
    CxObj *co;

    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)name;
    nb.nb_Title = (STRPTR)name;
    nb.nb_Descr = (STRPTR)"listchg probe";
    nb.nb_Unique = NBU_UNIQUE;
    nb.nb_Flags = flags;
    nb.nb_Pri = 0;
    nb.nb_Port = port;
    nb.nb_ReservedChannel = 0;
    co = CxBroker(&nb, &err);
    if (!co)
        P_LONG("CxBroker error", err);
    return co;
}

static void all(const char *what)
{
    P_SECTION(what);
    drain("Exchange", pA);
    drain("Other", pB);
    drain("Third", pC);
}

int main(void)
{
    CxObj *other, *exch, *third, *filter;

    if (!cx_open())
        return 0;
    pA = CreateMsgPort();
    pB = CreateMsgPort();
    pC = CreateMsgPort();

    P_SECTION("no brokers");
    P_LONG("BrokerCommand(NULL, LIST_CHG)", BrokerCommand(NULL, CXCMD_LIST_CHG));

    other = broker("Other", pB, COF_SHOW_HIDE);
    all("CxBroker Other");
    P_LONG("BrokerCommand(NULL, LIST_CHG) without Exchange", BrokerCommand(NULL, CXCMD_LIST_CHG));
    all("after that");

    exch = broker("Exchange", pA, 0);
    all("CxBroker Exchange");
    P_LONG("BrokerCommand(NULL, LIST_CHG)", BrokerCommand(NULL, CXCMD_LIST_CHG));
    all("after BrokerCommand(NULL, LIST_CHG)");
    P_LONG("BrokerCommand(NULL, ENABLE)", BrokerCommand(NULL, CXCMD_ENABLE));
    all("after BrokerCommand(NULL, ENABLE)");
    P_LONG("BrokerCommand(\"Other\", LIST_CHG)", BrokerCommand((STRPTR)"Other", CXCMD_LIST_CHG));
    all("after BrokerCommand(Other, LIST_CHG)");

    P_LONG("ActivateCxObj(Other, TRUE)", ActivateCxObj(other, TRUE));
    all("ActivateCxObj(Other, TRUE)");
    P_LONG("ActivateCxObj(Other, TRUE) again", ActivateCxObj(other, TRUE));
    all("ActivateCxObj(Other, TRUE) again");
    P_LONG("ActivateCxObj(Other, FALSE)", ActivateCxObj(other, FALSE));
    all("ActivateCxObj(Other, FALSE)");

    filter = CxFilter((STRPTR)"rawkey f9");
    AttachCxObj(other, filter);
    all("CxFilter + AttachCxObj");
    P_LONG("ActivateCxObj(filter, FALSE)", ActivateCxObj(filter, FALSE));
    all("ActivateCxObj(filter, FALSE)");

    SetCxObjPri(other, 5);
    all("SetCxObjPri(Other, 5)");

    third = broker("Third", pC, COF_SHOW_HIDE);
    all("CxBroker Third");
    DeleteCxObj(third);
    all("DeleteCxObj(Third)");
    DeleteCxObjAll(other);
    all("DeleteCxObjAll(Other)");
    DeleteCxObj(exch);
    all("DeleteCxObj(Exchange)");

    DeleteMsgPort(pA);
    DeleteMsgPort(pB);
    DeleteMsgPort(pC);
    CloseLibrary(CxBase);
    return 0;
}
