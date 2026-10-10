/*
 * Probe: commodities.library objects - CreateCxObj() of every type,
 * CxObjType(), CxObjError(), ClearCxObjError(), ActivateCxObj(),
 * SetCxObjPri(), SetFilter(), SetFilterIX(), SetTranslate(), and the
 * error bits of AttachCxObj()/EnqueueCxObj()/InsertCxObj() with a NULL
 * object.  Output compared with AmigaOS 3.1.
 */
#include "cxprobe.h"

static void show(const char *label, CxObj *co)
{
    probe_s(label);
    if (!co) {
        probe_s(": NULL\n");
        return;
    }
    probe_s(": type=");
    probe_dec(CxObjType(co));
    probe_s(" error=");
    probe_dec(CxObjError(co));
    probe_ch('\n');
}

static void custom_fn(CxMsg *msg, CxObj *co)
{
    (void)msg;
    (void)co;
}

int main(void)
{
    struct MsgPort *port;
    CxObj *filt, *filt0, *filtbad, *tf, *send, *sig, *trans, *dbg, *cust, *zero, *inv;
    static IX ix;

    if (!cx_open())
        return 20;
    port = CreateMsgPort();

    P_SECTION("CreateCxObj");
    filt = CxFilter("rawkey control a");
    show("CxFilter(\"rawkey control a\")", filt);
    filt0 = CxFilter(NULL);
    show("CxFilter(NULL)", filt0);
    filtbad = CxFilter("this is not a filter");
    show("CxFilter(bad)", filtbad);
    tf = CreateCxObj(CX_TYPEFILTER, CXM_IEVENT, 0);
    show("CX_TYPEFILTER", tf);
    send = CxSender(port, 5);
    show("CxSender", send);
    sig = CxSignal(FindTask(NULL), SIGBREAKB_CTRL_F);
    show("CxSignal", sig);
    trans = CxTranslate(NULL);
    show("CxTranslate(NULL)", trans);
    dbg = CxDebug(7);
    show("CxDebug", dbg);
    cust = CxCustom(custom_fn, 9);
    show("CxCustom", cust);
    zero = CreateCxObj(CX_ZERO, 0, 0);
    show("CX_ZERO", zero);
    inv = CreateCxObj(CX_INVALID, 0, 0);
    show("CX_INVALID", inv);
    P_LONG("CxObjType(NULL)", CxObjType(NULL));
    P_LONG("CxObjError(NULL)", CxObjError(NULL));

    P_SECTION("ActivateCxObj on a filter");
    P_LONG("ActivateCxObj(filt, FALSE)", ActivateCxObj(filt, FALSE));
    P_LONG("ActivateCxObj(filt, FALSE)", ActivateCxObj(filt, FALSE));
    P_LONG("ActivateCxObj(filt, TRUE)", ActivateCxObj(filt, TRUE));
    P_LONG("ActivateCxObj(filt, TRUE)", ActivateCxObj(filt, TRUE));

    P_SECTION("SetCxObjPri");
    P_LONG("SetCxObjPri(send, 5)", SetCxObjPri(send, 5));
    P_LONG("SetCxObjPri(send, -7)", SetCxObjPri(send, -7));
    P_LONG("SetCxObjPri(send, 127)", SetCxObjPri(send, 127));
    P_LONG("SetCxObjPri(send, 0)", SetCxObjPri(send, 0));

    P_SECTION("SetFilter");
    SetFilter(filt, "rawkey ctrl b");
    show("SetFilter(filt, good)", filt);
    SetFilter(filt, "garbage words here");
    show("SetFilter(filt, bad)", filt);
    SetFilter(filt, "rawkey ctrl c");
    show("SetFilter(filt, good again)", filt);
    SetFilter(filt, "garbage words here");
    ClearCxObjError(filt);
    show("ClearCxObjError(filt)", filt);
    SetFilter(filtbad, "lalt x");
    show("SetFilter(filtbad, good)", filtbad);
    SetFilter(filt0, NULL);
    show("SetFilter(filt0, NULL)", filt0);
    SetFilter(send, "ctrl a");
    show("SetFilter(sender, ...)", send);
    ClearCxObjError(send);
    ClearCxObjError(NULL);

    P_SECTION("SetFilterIX");
    ix.ix_Version = IX_VERSION;
    ix.ix_Class = IECLASS_RAWKEY;
    ix.ix_Code = 0x20;
    ix.ix_CodeMask = 0xffff;
    ix.ix_Qualifier = IEQUALIFIER_CONTROL;
    ix.ix_QualMask = IEQUALIFIER_CONTROL;
    ix.ix_QualSame = 0;
    SetFilterIX(filt0, &ix);
    show("SetFilterIX(filt0, v2)", filt0);
    ix.ix_Version = 1;
    SetFilterIX(filt0, &ix);
    show("SetFilterIX(filt0, v1)", filt0);
    ix.ix_Version = 3;
    SetFilterIX(filt0, &ix);
    show("SetFilterIX(filt0, v3)", filt0);
    ix.ix_Version = IX_VERSION;
    SetFilterIX(filt0, &ix);
    show("SetFilterIX(filt0, v2 again)", filt0);
    SetFilterIX(trans, &ix);
    show("SetFilterIX(translator)", trans);

    P_SECTION("SetTranslate");
    ClearCxObjError(trans);
    SetTranslate(trans, NULL);
    show("SetTranslate(trans, NULL)", trans);
    SetTranslate(filt, NULL);
    show("SetTranslate(filter, NULL)", filt);
    ClearCxObjError(filt);

    P_SECTION("attach NULL");
    AttachCxObj(filt, NULL);
    show("AttachCxObj(filt, NULL)", filt);
    ClearCxObjError(filt);
    EnqueueCxObj(filt, NULL);
    show("EnqueueCxObj(filt, NULL)", filt);
    ClearCxObjError(filt);
    InsertCxObj(filt, NULL, NULL);
    show("InsertCxObj(filt, NULL, NULL)", filt);
    ClearCxObjError(filt);

    P_SECTION("attach");
    AttachCxObj(filt, send);
    EnqueueCxObj(filt, sig);
    InsertCxObj(filt, trans, send);
    InsertCxObj(filt, dbg, NULL);
    show("filt after attaching 4", filt);
    RemoveCxObj(sig);
    RemoveCxObj(sig);
    show("filt after RemoveCxObj twice", filt);
    show("removed sig", sig);
    RemoveCxObj(NULL);
    DeleteCxObj(sig);
    DeleteCxObjAll(filt);
    DeleteCxObjAll(NULL);

    DeleteCxObj(filt0);
    DeleteCxObj(filtbad);
    DeleteCxObj(tf);
    DeleteCxObj(cust);
    DeleteCxObj(zero);
    DeleteCxObj(inv);
    P_SECTION("done");

    DeleteMsgPort(port);
    CloseLibrary(CxBase);
    return 0;
}
