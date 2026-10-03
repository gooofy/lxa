/*
 * Probe (Phase 222f): iffparse.library context handling - PropChunk(s)/
 * FindProp, CollectionChunk(s)/FindCollection, StopChunk(s), StopOnExit,
 * EntryHandler/ExitHandler (return codes, positions, precedence),
 * FindPropContext, local context items (AllocLocalItem, LocalItemData,
 * StoreLocalItem, StoreItemInContext, FindLocalItem, SetLocalItemPurge,
 * FreeLocalItem) and GoodID/GoodType/IDtoStr.
 */
#include "iffprobe.h"

static struct MemStream ms;
static struct IFFHandle *g_iff;

/* LIST ILBM { PROP ILBM { BMHD CRNG } FORM ILBM { BMHD CRNG CRNG BODY }
 *             FORM ILBM { CRNG BODY } FORM 8SVX { BODY } } */
static void build(void)
{
    ms_reset(&ms);
    ms_id(&ms, ID_LIST); ms_id(&ms, 146); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_PROP); ms_id(&ms, 24); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BMHD); ms_id(&ms, 2); ms_str(&ms, "L1", 2);
    ms_id(&ms, ID_CRNG); ms_id(&ms, 2); ms_str(&ms, "c1", 2);
    ms_id(&ms, ID_FORM); ms_id(&ms, 48); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BMHD); ms_id(&ms, 4); ms_str(&ms, "F1F1", 4);
    ms_id(&ms, ID_CRNG); ms_id(&ms, 2); ms_str(&ms, "c2", 2);
    ms_id(&ms, ID_CRNG); ms_id(&ms, 3); ms_str(&ms, "c3x", 3); ms_pad(&ms);
    ms_id(&ms, ID_BODY); ms_id(&ms, 2); ms_str(&ms, "b1", 2);
    ms_id(&ms, ID_FORM); ms_id(&ms, 24); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_CRNG); ms_id(&ms, 2); ms_str(&ms, "c4", 2);
    ms_id(&ms, ID_BODY); ms_id(&ms, 2); ms_str(&ms, "b2", 2);
    ms_id(&ms, ID_FORM); ms_id(&ms, 14); ms_id(&ms, ID_8SVX);
    ms_id(&ms, ID_BODY); ms_id(&ms, 1); ms_str(&ms, "s", 1); ms_pad(&ms);
}

static struct IFFHandle *ropen(void)
{
    struct IFFHandle *iff = AllocIFF();
    ms_rewind(&ms);
    ms.quiet = TRUE;
    iff->iff_Stream = (ULONG)&ms;
    InitIFF(iff, IFFF_RSEEK, &ip_streamhook);
    g_iff = iff;
    return iff;
}

static void pdata(const UBYTE *p, LONG n)
{
    LONG i;
    probe_ch('"');
    for (i = 0; i < n && i < 16; i++)
        probe_ch((char)p[i]);
    probe_ch('"');
}

static void show_prop(struct IFFHandle *iff, ULONG type, ULONG id)
{
    struct StoredProperty *sp = FindProp(iff, type, id);
    probe_s("    FindProp ");
    ip_id(type);
    probe_ch('.');
    ip_id(id);
    probe_s(" = ");
    if (sp) {
        probe_dec(sp->sp_Size);
        probe_ch(' ');
        pdata(sp->sp_Data, sp->sp_Size);
    } else
        probe_s("NULL");
    probe_ch('\n');
}

static void show_coll(struct IFFHandle *iff, ULONG type, ULONG id)
{
    struct CollectionItem *ci = FindCollection(iff, type, id);
    int n = 0;
    probe_s("    FindCollection ");
    ip_id(type);
    probe_ch('.');
    ip_id(id);
    probe_s(" =");
    if (!ci)
        probe_s(" NULL");
    while (ci && n++ < 10) {
        probe_ch(' ');
        probe_dec(ci->ci_Size);
        probe_ch(':');
        pdata(ci->ci_Data, ci->ci_Size);
        ci = ci->ci_Next;
    }
    probe_ch('\n');
}

static void show_ctx(struct IFFHandle *iff)
{
    probe_s("    FindPropContext = ");
    ip_cn(FindPropContext(iff));
    probe_ch('\n');
}

static void show_all(struct IFFHandle *iff)
{
    show_ctx(iff);
    show_prop(iff, ID_ILBM, ID_BMHD);
    show_prop(iff, ID_8SVX, ID_BMHD);
    show_prop(iff, ID_ILBM, ID_CRNG);
    show_coll(iff, ID_ILBM, ID_CRNG);
    show_coll(iff, ID_ILBM, ID_BMHD);
}

/* ---- client handlers ---- */

struct HCtx {
    const char *name;
    LONG ret;
    LONG calls;
};

static struct Hook h_handler;

static ULONG handlerfunc(struct Hook *h, APTR obj, APTR msg)
{
    struct HCtx *hc = (struct HCtx *)obj;
    struct ContextNode *cn = CurrentChunk(g_iff);
    (void)h;
    hc->calls++;
    probe_s("    handler ");
    probe_s(hc->name);
    probe_s(" cmd=");
    probe_dec(*(LONG *)msg);
    probe_s(" top=");
    ip_cn(cn);
    probe_s(" -> ");
    probe_dec(hc->ret);
    probe_ch('\n');
    return (ULONG)hc->ret;
}

/* ---- purge hook ---- */

static struct Hook h_purge;

static ULONG purgefunc(struct Hook *h, APTR obj, APTR msg)
{
    struct LocalContextItem *lci = (struct LocalContextItem *)obj;
    (void)h;
    probe_s("    purge ");
    ip_id(lci->lci_Ident);
    probe_ch(' ');
    ip_id(lci->lci_Type);
    probe_ch('.');
    ip_id(lci->lci_ID);
    probe_s(" cmd=");
    probe_dec(*(LONG *)msg);
    probe_s(" data=");
    probe_s((char *)LocalItemData(lci));
    probe_ch('\n');
    FreeLocalItem(lci);
    return 0;
}

static struct LocalContextItem *mkitem(ULONG type, ULONG id, ULONG ident, const char *s)
{
    struct LocalContextItem *lci = AllocLocalItem(type, id, ident, 8);
    char *d;
    int i;
    if (!lci)
        return NULL;
    d = (char *)LocalItemData(lci);
    for (i = 0; i < 7 && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
    return lci;
}

static void find(struct IFFHandle *iff, ULONG type, ULONG id, ULONG ident)
{
    struct LocalContextItem *lci = FindLocalItem(iff, type, id, ident);
    probe_s("    FindLocalItem ");
    ip_id(type);
    probe_ch('.');
    ip_id(id);
    probe_ch('.');
    ip_id(ident);
    probe_s(" = ");
    if (lci) {
        probe_s((char *)LocalItemData(lci));
        probe_s(" [");
        ip_id(lci->lci_Type);
        probe_ch('.');
        ip_id(lci->lci_ID);
        probe_ch('.');
        ip_id(lci->lci_Ident);
        probe_ch(']');
    } else
        probe_s("NULL");
    probe_ch('\n');
}

static void goodid(ULONG id)
{
    char buf[8];
    int i;
    for (i = 0; i < 8; i++)
        buf[i] = 'Z';
    probe_s("id ");
    probe_hex(id, 8);
    probe_s(" GoodID=");
    probe_dec(GoodID(id) != 0);
    probe_s(" GoodType=");
    probe_dec(GoodType(id) != 0);
    probe_s(" IDtoStr=");
    probe_dec((UBYTE *)IDtoStr(id, (STRPTR)buf) == (UBYTE *)buf);
    probe_ch(' ');
    P_BYTES("", buf, 6);
}

#define MAXEV 60

int main(void)
{
    struct IFFHandle *iff;
    LONG rc;
    int i;

    if (!ip_open())
        return 20;
    ip_inithook(&h_handler, handlerfunc, NULL);
    ip_inithook(&h_purge, purgefunc, NULL);
    build();

    P_SECTION("prop/collection/stop scan");
    iff = ropen();
    P_LONG("PropChunk", PropChunk(iff, ID_ILBM, ID_BMHD));
    P_LONG("CollectionChunk", CollectionChunk(iff, ID_ILBM, ID_CRNG));
    P_LONG("StopChunk", StopChunk(iff, ID_ILBM, ID_BODY));
    P_LONG("StopChunk", StopChunk(iff, ID_8SVX, ID_BODY));
    P_LONG("OpenIFF", OpenIFF(iff, IFFF_READ));
    show_all(iff);
    for (i = 0; i < 10; i++) {
        rc = ParseIFF(iff, IFFPARSE_SCAN);
        ip_state("scan", iff, rc);
        if (rc)
            break;
        show_all(iff);
    }
    show_all(iff);
    CloseIFF(iff);
    FreeIFF(iff);

    P_SECTION("prop/collection step");
    iff = ropen();
    {
        static LONG props[] = { ID_ILBM, ID_BMHD, ID_ILBM, ID_CRNG, ID_8SVX, ID_BODY };
        P_LONG("PropChunks(0)", PropChunks(iff, props, 0));
        P_LONG("PropChunks", PropChunks(iff, props, 1));
        P_LONG("CollectionChunks", CollectionChunks(iff, props + 2, 2));
        P_LONG("StopChunks(0)", StopChunks(iff, props, 0));
    }
    P_LONG("OpenIFF", OpenIFF(iff, IFFF_READ));
    for (i = 0; i < MAXEV; i++) {
        rc = ParseIFF(iff, IFFPARSE_STEP);
        ip_state("step", iff, rc);
        show_ctx(iff);
        show_prop(iff, ID_ILBM, ID_BMHD);
        show_coll(iff, ID_ILBM, ID_CRNG);
        show_coll(iff, ID_8SVX, ID_BODY);
        if (rc != 0 && rc != IFFERR_EOC)
            break;
    }
    CloseIFF(iff);
    FreeIFF(iff);

    P_SECTION("prop/collection rawstep");
    iff = ropen();
    PropChunk(iff, ID_ILBM, ID_BMHD);
    CollectionChunk(iff, ID_ILBM, ID_CRNG);
    OpenIFF(iff, IFFF_READ);
    for (i = 0; i < MAXEV; i++) {
        rc = ParseIFF(iff, IFFPARSE_RAWSTEP);
        if (rc != 0 && rc != IFFERR_EOC)
            break;
    }
    ip_state("raw end", iff, rc);
    CloseIFF(iff);
    FreeIFF(iff);

    P_SECTION("stoponexit");
    iff = ropen();
    P_LONG("StopOnExit", StopOnExit(iff, ID_ILBM, ID_FORM));
    P_LONG("StopOnExit", StopOnExit(iff, ID_ILBM, ID_CRNG));
    P_LONG("PropChunk", PropChunk(iff, ID_ILBM, ID_BMHD));
    OpenIFF(iff, IFFF_READ);
    for (i = 0; i < 12; i++) {
        rc = ParseIFF(iff, IFFPARSE_SCAN);
        ip_state("scan", iff, rc);
        show_prop(iff, ID_ILBM, ID_BMHD);
        if (rc != 0 && rc != IFFERR_EOC)
            break;
    }
    CloseIFF(iff);
    FreeIFF(iff);

    P_SECTION("stop on composite");
    iff = ropen();
    P_LONG("StopChunk LIST", StopChunk(iff, ID_ILBM, ID_LIST));
    P_LONG("StopChunk PROP", StopChunk(iff, ID_ILBM, ID_PROP));
    P_LONG("StopChunk FORM", StopChunk(iff, ID_8SVX, ID_FORM));
    P_LONG("StopChunk any BODY", StopChunk(iff, 0, ID_BODY));
    OpenIFF(iff, IFFF_READ);
    for (i = 0; i < 12; i++) {
        rc = ParseIFF(iff, IFFPARSE_SCAN);
        ip_state("scan", iff, rc);
        if (rc != 0)
            break;
    }
    CloseIFF(iff);
    FreeIFF(iff);

    P_SECTION("handlers");
    {
        static struct HCtx hlist = { "LIST-enter", 0, 0 };
        static struct HCtx hform = { "FORM-enter", 0, 0 };
        static struct HCtx hformx = { "FORM-exit", 0, 0 };
        static struct HCtx hbody = { "BODY-enter", IFF_RETURN2CLIENT, 0 };
        static struct HCtx hbodyx = { "BODY-exit", 0, 0 };
        static struct HCtx hcrng = { "CRNG-enter", 0, 0 };
        static struct HCtx hprop = { "PROP-enter", 0, 0 };
        static struct HCtx h8 = { "8SVX-BODY", 4711, 0 };
        iff = ropen();
        P_LONG("EntryHandler LIST", EntryHandler(iff, ID_ILBM, ID_LIST, IFFSLI_ROOT, &h_handler, &hlist));
        P_LONG("EntryHandler FORM", EntryHandler(iff, ID_ILBM, ID_FORM, IFFSLI_TOP, &h_handler, &hform));
        P_LONG("ExitHandler FORM", ExitHandler(iff, ID_ILBM, ID_FORM, IFFSLI_ROOT, &h_handler, &hformx));
        P_LONG("EntryHandler BODY", EntryHandler(iff, ID_ILBM, ID_BODY, IFFSLI_ROOT, &h_handler, &hbody));
        P_LONG("ExitHandler BODY", ExitHandler(iff, ID_ILBM, ID_BODY, IFFSLI_ROOT, &h_handler, &hbodyx));
        P_LONG("EntryHandler CRNG", EntryHandler(iff, ID_ILBM, ID_CRNG, IFFSLI_ROOT, &h_handler, &hcrng));
        P_LONG("EntryHandler PROP", EntryHandler(iff, ID_ILBM, ID_PROP, IFFSLI_ROOT, &h_handler, &hprop));
        P_LONG("EntryHandler 8SVX", EntryHandler(iff, ID_8SVX, ID_BODY, IFFSLI_ROOT, &h_handler, &h8));
        P_LONG("EntryHandler PROP pos", EntryHandler(iff, ID_ILBM, ID_DATA, IFFSLI_PROP, &h_handler, &hprop));
        P_LONG("EntryHandler bad pos", EntryHandler(iff, ID_ILBM, ID_DATA, 0, &h_handler, &hprop));
        P_LONG("EntryHandler bad pos 4", EntryHandler(iff, ID_ILBM, ID_DATA, 4, &h_handler, &hprop));
        P_LONG("OpenIFF", OpenIFF(iff, IFFF_READ));
        for (i = 0; i < 12; i++) {
            rc = ParseIFF(iff, IFFPARSE_SCAN);
            ip_state("scan", iff, rc);
            if (rc == 0 && CurrentChunk(iff)->cn_ID == ID_FORM) {
                /* a scoped handler inside the first FORM only */
                static struct HCtx hscoped = { "scoped-CRNG", 0, 0 };
                P_LONG("  EntryHandler scoped", EntryHandler(iff, ID_ILBM, ID_CRNG, IFFSLI_TOP, &h_handler, &hscoped));
                hform.ret = 0;
            }
            if (rc != 0 && rc != IFFERR_EOC)
                break;
            if (i == 0)
                hform.ret = IFF_RETURN2CLIENT;
        }
        CloseIFF(iff);
        FreeIFF(iff);
    }

    P_SECTION("handlers step");
    {
        static struct HCtx hform = { "FORM-enter", 0, 0 };
        static struct HCtx hformx = { "FORM-exit", 0, 0 };
        static struct HCtx hbodyx = { "BODY-exit", IFF_RETURN2CLIENT, 0 };
        iff = ropen();
        EntryHandler(iff, ID_ILBM, ID_FORM, IFFSLI_ROOT, &h_handler, &hform);
        ExitHandler(iff, ID_ILBM, ID_FORM, IFFSLI_ROOT, &h_handler, &hformx);
        ExitHandler(iff, ID_ILBM, ID_BODY, IFFSLI_ROOT, &h_handler, &hbodyx);
        StopChunk(iff, ID_ILBM, ID_CRNG);
        OpenIFF(iff, IFFF_READ);
        for (i = 0; i < MAXEV; i++) {
            rc = ParseIFF(iff, IFFPARSE_STEP);
            ip_state("step", iff, rc);
            if (rc != 0 && rc != IFFERR_EOC)
                break;
        }
        P_LONG("FORM-enter calls", hform.calls);
        P_LONG("FORM-exit calls", hformx.calls);
        CloseIFF(iff);
        FreeIFF(iff);
    }

    P_SECTION("handler precedence");
    {
        static struct HCtx ha = { "A-root", 0, 0 };
        static struct HCtx hb = { "B-root-later", 0, 0 };
        static struct HCtx hc = { "C-top", 0, 0 };
        iff = ropen();
        EntryHandler(iff, ID_ILBM, ID_BMHD, IFFSLI_ROOT, &h_handler, &ha);
        PropChunk(iff, ID_ILBM, ID_BMHD);
        EntryHandler(iff, ID_ILBM, ID_BMHD, IFFSLI_ROOT, &h_handler, &hb);
        CollectionChunk(iff, ID_ILBM, ID_BMHD);
        StopChunk(iff, ID_ILBM, ID_FORM);
        OpenIFF(iff, IFFF_READ);
        for (i = 0; i < 12; i++) {
            rc = ParseIFF(iff, IFFPARSE_SCAN);
            ip_state("scan", iff, rc);
            show_prop(iff, ID_ILBM, ID_BMHD);
            show_coll(iff, ID_ILBM, ID_BMHD);
            if (rc != 0)
                break;
            if (i == 0)
                P_LONG("  EntryHandler top", EntryHandler(iff, ID_ILBM, ID_BMHD, IFFSLI_TOP, &h_handler, &hc));
        }
        CloseIFF(iff);
        FreeIFF(iff);
    }

    P_SECTION("local items");
    {
        struct LocalContextItem *a, *b, *c, *d, *e;
        struct ContextNode *cn;
        iff = ropen();
        a = mkitem(ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'), "a-root");
        b = mkitem(ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'), "b-root");
        c = mkitem(ID_ILBM, ID_CRNG, MAKE_ID('t','s','t','1'), "c-top");
        d = mkitem(0, 0, MAKE_ID('t','s','t','2'), "d-zero");
        P_LONG("lci_Type", (LONG)a->lci_Type == ID_ILBM);
        P_LONG("lci_ID", (LONG)a->lci_ID == ID_BMHD);
        P_HEX("lci_Ident", a->lci_Ident);
        P_NULL("LocalItemData(NULL)", LocalItemData(NULL));
        e = AllocLocalItem(1, 2, 3, 0);
        P_NULL("AllocLocalItem size 0", e);
        P_NULL("LocalItemData size 0", e ? LocalItemData(e) : NULL);
        FreeLocalItem(e);
        FreeLocalItem(NULL);
        SetLocalItemPurge(a, &h_purge);
        SetLocalItemPurge(b, &h_purge);
        SetLocalItemPurge(c, &h_purge);
        SetLocalItemPurge(d, &h_purge);
        P_LONG("Store a ROOT", StoreLocalItem(iff, a, IFFSLI_ROOT));
        P_LONG("Store b ROOT", StoreLocalItem(iff, b, IFFSLI_ROOT));
        P_LONG("Store c TOP", StoreLocalItem(iff, c, IFFSLI_TOP));
        rc = StoreLocalItem(iff, d, IFFSLI_PROP);
        P_LONG("Store d PROP", rc);
        if (rc)
            FreeLocalItem(d);
        find(iff, ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'));
        find(iff, ID_ILBM, ID_CRNG, MAKE_ID('t','s','t','1'));
        find(iff, 0, ID_BMHD, MAKE_ID('t','s','t','1'));
        find(iff, ID_ILBM, 0, MAKE_ID('t','s','t','1'));
        find(iff, ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','2'));
        find(iff, 0, 0, MAKE_ID('t','s','t','2'));
        P_LONG("OpenIFF", OpenIFF(iff, IFFF_READ));
        rc = ParseIFF(iff, IFFPARSE_STEP);              /* LIST */
        ip_state("step", iff, rc);
        e = mkitem(ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'), "e-list");
        SetLocalItemPurge(e, &h_purge);
        P_LONG("Store e TOP", StoreLocalItem(iff, e, IFFSLI_TOP));
        find(iff, ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'));
        for (i = 0; i < 5; i++) {                       /* PROP, BMHD .. */
            rc = ParseIFF(iff, IFFPARSE_STEP);
            ip_state("step", iff, rc);
        }
        /* now at EOC of PROP? store into the current and the prop context */
        e = mkitem(ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'), "f-prop");
        SetLocalItemPurge(e, &h_purge);
        rc = StoreLocalItem(iff, e, IFFSLI_PROP);
        P_LONG("Store f PROP", rc);
        if (rc)
            FreeLocalItem(e);
        e = mkitem(ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'), "g-top");
        SetLocalItemPurge(e, &h_purge);
        P_LONG("Store g TOP", StoreLocalItem(iff, e, IFFSLI_TOP));
        cn = ParentChunk(CurrentChunk(iff));
        e = mkitem(ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'), "h-ctx");
        SetLocalItemPurge(e, &h_purge);
        StoreItemInContext(iff, e, cn);
        probe_s("StoreItemInContext into ");
        ip_cn(cn);
        probe_ch('\n');
        find(iff, ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'));
        for (i = 0; i < MAXEV; i++) {
            rc = ParseIFF(iff, IFFPARSE_STEP);
            ip_state("step", iff, rc);
            find(iff, ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'));
            if (rc != 0 && rc != IFFERR_EOC)
                break;
        }
        P_STR("CloseIFF", "->");
        CloseIFF(iff);
        find(iff, ID_ILBM, ID_BMHD, MAKE_ID('t','s','t','1'));
        P_STR("FreeIFF", "->");
        FreeIFF(iff);
        P_STR("FreeIFF", "done");
    }

    P_SECTION("item positions");
    {
        struct LocalContextItem *x;
        iff = ropen();
        StopChunk(iff, ID_ILBM, ID_FORM);
        OpenIFF(iff, IFFF_READ);
        rc = ParseIFF(iff, IFFPARSE_SCAN);
        ip_state("scan", iff, rc);
        x = mkitem(1, 2, 3, "pos0");
        SetLocalItemPurge(x, &h_purge);
        P_LONG("StoreLocalItem pos 0", StoreLocalItem(iff, x, 0));
        x = mkitem(1, 3, 3, "pos4");
        SetLocalItemPurge(x, &h_purge);
        P_LONG("StoreLocalItem pos 4", StoreLocalItem(iff, x, 4));
        x = mkitem(1, 4, 3, "pos-1");
        SetLocalItemPurge(x, &h_purge);
        P_LONG("StoreLocalItem pos -1", StoreLocalItem(iff, x, -1));
        x = mkitem(1, 5, 3, "root");
        SetLocalItemPurge(x, &h_purge);
        P_LONG("StoreLocalItem ROOT", StoreLocalItem(iff, x, IFFSLI_ROOT));
        x = mkitem(1, 5, 3, "ctx1");
        SetLocalItemPurge(x, &h_purge);
        StoreItemInContext(iff, x, CurrentChunk(iff));
        P_STR("StoreItemInContext", "ctx1");
        x = mkitem(1, 5, 3, "ctx2");
        SetLocalItemPurge(x, &h_purge);
        StoreItemInContext(iff, x, CurrentChunk(iff));
        P_STR("StoreItemInContext", "ctx2");
        find(iff, 1, 2, 3);
        find(iff, 1, 3, 3);
        find(iff, 1, 4, 3);
        find(iff, 1, 5, 3);
        rc = ParseIFF(iff, IFFPARSE_SCAN);
        ip_state("scan", iff, rc);
        find(iff, 1, 2, 3);
        find(iff, 1, 3, 3);
        find(iff, 1, 4, 3);
        find(iff, 1, 5, 3);
        CloseIFF(iff);
        P_STR("FreeIFF", "->");
        FreeIFF(iff);
    }

    P_SECTION("items before open");
    {
        struct LocalContextItem *x;
        iff = AllocIFF();
        x = mkitem(1, 2, 3, "x");
        rc = StoreLocalItem(iff, x, IFFSLI_PROP);
        P_LONG("StoreLocalItem PROP on fresh handle", rc);
        if (rc)
            FreeLocalItem(x);
        x = mkitem(1, 2, 3, "y");
        rc = StoreLocalItem(iff, x, IFFSLI_TOP);
        P_LONG("StoreLocalItem TOP on fresh handle", rc);
        if (rc)
            FreeLocalItem(x);
        find(iff, 1, 2, 3);
        P_LONG("PropChunk", PropChunk(iff, ID_ILBM, ID_BMHD));
        P_NULL("FindLocalItem enhd", FindLocalItem(iff, ID_ILBM, ID_BMHD, IFFLCI_ENTRYHANDLER));
        P_NULL("FindLocalItem exhd", FindLocalItem(iff, ID_ILBM, ID_BMHD, IFFLCI_EXITHANDLER));
        P_NULL("FindLocalItem prop", FindLocalItem(iff, ID_ILBM, ID_BMHD, IFFLCI_PROP));
        P_LONG("StopOnExit", StopOnExit(iff, ID_ILBM, ID_BODY));
        P_NULL("FindLocalItem exhd BODY", FindLocalItem(iff, ID_ILBM, ID_BODY, IFFLCI_EXITHANDLER));
        P_NULL("FindLocalItem enhd BODY", FindLocalItem(iff, ID_ILBM, ID_BODY, IFFLCI_ENTRYHANDLER));
        P_LONG("StopChunk", StopChunk(iff, ID_ILBM, ID_BODY));
        P_NULL("FindLocalItem enhd BODY", FindLocalItem(iff, ID_ILBM, ID_BODY, IFFLCI_ENTRYHANDLER));
        P_LONG("CollectionChunk", CollectionChunk(iff, ID_ILBM, ID_CMAP));
        P_NULL("FindLocalItem enhd CMAP", FindLocalItem(iff, ID_ILBM, ID_CMAP, IFFLCI_ENTRYHANDLER));
        P_NULL("FindProp", FindProp(iff, ID_ILBM, ID_BMHD));
        P_NULL("FindCollection", FindCollection(iff, ID_ILBM, ID_BMHD));
        P_NULL("FindPropContext", FindPropContext(iff));
        P_NULL("CurrentChunk", CurrentChunk(iff));
        FreeIFF(iff);
    }

    P_SECTION("ids");
    goodid(ID_FORM);
    goodid(ID_CAT);
    goodid(MAKE_ID(' ', ' ', ' ', ' '));
    goodid(MAKE_ID('A', ' ', ' ', ' '));
    goodid(MAKE_ID(' ', 'A', 'B', 'C'));
    goodid(MAKE_ID('A', 'B', ' ', 'C'));
    goodid(MAKE_ID('A', 'B', 'C', ' '));
    goodid(MAKE_ID('i', 'l', 'b', 'm'));
    goodid(MAKE_ID('I', 'L', 'b', 'M'));
    goodid(MAKE_ID('8', 'S', 'V', 'X'));
    goodid(MAKE_ID('1', '2', '3', '4'));
    goodid(MAKE_ID('A', 'B', 'C', 0x7e));
    goodid(MAKE_ID('A', 'B', 'C', 0x7f));
    goodid(MAKE_ID('A', 'B', 'C', 0x1f));
    goodid(MAKE_ID(0x7e, 'B', 'C', 'D'));
    goodid(MAKE_ID('A', 0x80, 'C', 'D'));
    goodid(MAKE_ID('A', 0xff, 'C', 'D'));
    goodid(MAKE_ID('A', '.', 'C', 'D'));
    goodid(MAKE_ID('A', ';', 'C', 'D'));
    goodid(MAKE_ID('A', '#', 'C', 'D'));
    goodid(MAKE_ID('A', ',', 'C', 'D'));
    goodid(MAKE_ID('A', '_', 'C', 'D'));
    goodid(MAKE_ID('A', '-', 'C', 'D'));
    goodid(MAKE_ID('(', 'c', ')', ' '));
    goodid(MAKE_ID('A', 'B', 'C', 0));
    goodid(MAKE_ID('A', 0, 'C', 'D'));
    goodid(0);
    goodid(0xffffffff);
    goodid(0x20202021);
    goodid(MAKE_ID('P', 'R', 'O', 'P'));
    goodid(MAKE_ID('L', 'I', 'S', '1'));
    goodid(MAKE_ID('F', 'O', 'R', '1'));
    goodid(MAKE_ID('C', 'A', 'T', '4'));
    goodid(MAKE_ID('F', 'O', 'R', '9'));
    goodid(MAKE_ID('C', 'A', 'T', 'S'));
    goodid(MAKE_ID('J', 'J', 'J', 'J'));

    CloseLibrary(IFFParseBase);
    return 0;
}
