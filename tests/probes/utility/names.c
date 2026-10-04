/*
 * Probe (Phase 222a): utility.library name spaces - AllocNamedObjectA,
 * AddNamedObject, FindNamedObject, NamedObjectName, ReleaseNamedObject,
 * AttemptRemNamedObject, RemNamedObject, FreeNamedObject, GetUniqueID.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/ports.h>
#include <utility/name.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/utility_protos.h>
#include <inline/exec.h>
#include <inline/utility.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *UtilityBase;

static struct NamedObject *mk(const char *name, LONG pri, ULONG flags, int ns, ULONG usersize)
{
    struct TagItem t[] = {
        {ANO_Priority, (ULONG)pri}, {ANO_Flags, flags}, {ANO_NameSpace, (ULONG)ns},
        {usersize ? ANO_UserSpace : TAG_IGNORE, usersize}, {TAG_DONE, 0}
    };
    return AllocNamedObjectA((STRPTR)name, t);
}

static void list_ns(const char *label, struct NamedObject *ns)
{
    struct NamedObject *o = NULL;
    int n = 0;
    probe_s(label);
    probe_s(" =");
    while ((o = FindNamedObject(ns, NULL, o)) != NULL) {
        probe_ch(' ');
        probe_s((char *)NamedObjectName(o));
        ReleaseNamedObject(o);
        if (++n > 20)
            break;
    }
    probe_ch('\n');
}

int main(void)
{
    struct NamedObject *ns, *nsnd, *nscase, *a, *b, *c, *d, *e, *f;
    ULONG id1, id2;

    UtilityBase = OpenLibrary((STRPTR)"utility.library", 39);
    if (!UtilityBase)
        return 20;

    P_SECTION("allocation");
    ns = mk("space", 0, 0, 1, 0);
    nsnd = mk("nodups", 0, NSF_NODUPS, 1, 0);
    nscase = mk("case", 0, NSF_CASE, 1, 0);
    P_NULL("namespace", ns);
    P_NULL("namespace NODUPS", nsnd);
    P_NULL("namespace CASE", nscase);
    if (!ns || !nsnd || !nscase)
        return 20;
    a = mk("alpha", 0, 0, 0, 16);
    b = mk("beta", 5, 0, 0, 0);
    c = mk("Alpha", -3, 0, 0, 0);
    d = mk("gamma", 5, 0, 0, 0);
    P_NULL("object alpha", a);
    P_NULL("alpha no_Object (UserSpace 16)", a->no_Object);
    P_NULL("beta no_Object (no UserSpace)", b->no_Object);
    P_STR("NamedObjectName(alpha)", (char *)NamedObjectName(a));
    P_NULL("NamedObjectName(NULL)", NamedObjectName(NULL));

    P_SECTION("add/find");
    P_BOOL("AddNamedObject alpha", AddNamedObject(ns, a));
    P_BOOL("AddNamedObject beta", AddNamedObject(ns, b));
    P_BOOL("AddNamedObject Alpha", AddNamedObject(ns, c));
    P_BOOL("AddNamedObject gamma", AddNamedObject(ns, d));
    list_ns("namespace order", ns);
    {
        struct NamedObject *o = FindNamedObject(ns, (STRPTR)"ALPHA", NULL);
        P_BOOL("Find ALPHA (caseless) == alpha", o == a);
        if (o) {
            struct NamedObject *o2 = FindNamedObject(ns, (STRPTR)"ALPHA", o);
            P_BOOL("Find ALPHA next == Alpha", o2 == c);
            if (o2) {
                P_NULL("Find ALPHA next next", FindNamedObject(ns, (STRPTR)"ALPHA", o2));
                ReleaseNamedObject(o2);
            }
            ReleaseNamedObject(o);
        }
        o = FindNamedObject(ns, (STRPTR)"delta", NULL);
        P_NULL("Find delta", o);
    }
    {
        /* system namespace */
        e = mk("lxaprobe-sys", 0, 0, 0, 0);
        P_BOOL("AddNamedObject(NULL ns)", AddNamedObject(NULL, e));
        f = FindNamedObject(NULL, (STRPTR)"lxaprobe-sys", NULL);
        P_BOOL("FindNamedObject(NULL ns)", f == e);
        if (f)
            ReleaseNamedObject(f);
        P_BOOL("AttemptRemNamedObject sys", AttemptRemNamedObject(e));
        FreeNamedObject(e);
    }

    P_SECTION("NODUPS / CASE");
    {
        struct NamedObject *x1 = mk("dup", 0, 0, 0, 0), *x2 = mk("DUP", 0, 0, 0, 0), *x3 = mk("dup", 0, 0, 0, 0);
        P_BOOL("nodups add dup", AddNamedObject(nsnd, x1));
        P_BOOL("nodups add DUP", AddNamedObject(nsnd, x2));
        P_BOOL("nodups add dup again", AddNamedObject(nsnd, x3));
        list_ns("nodups order", nsnd);
        P_LONG("nodups AttemptRem dup", AttemptRemNamedObject(x1));
        FreeNamedObject(x1);
        P_LONG("freed x1", 1);
        FreeNamedObject(x2);
        P_LONG("freed x2", 1);
        FreeNamedObject(x3);
        P_LONG("freed x3", 1);
    }
    {
        struct NamedObject *y1 = mk("Case", 0, 0, 0, 0), *o;
        AddNamedObject(nscase, y1);
        o = FindNamedObject(nscase, (STRPTR)"case", NULL);
        P_NULL("case-sensitive find 'case'", o);
        if (o)
            ReleaseNamedObject(o);
        o = FindNamedObject(nscase, (STRPTR)"Case", NULL);
        P_BOOL("case-sensitive find 'Case'", o == y1);
        if (o)
            ReleaseNamedObject(o);
        P_BOOL("AttemptRemNamedObject Case", AttemptRemNamedObject(y1));
        FreeNamedObject(y1);
    }

    P_SECTION("removal");
    {
        struct NamedObject *o = FindNamedObject(ns, (STRPTR)"beta", NULL);
        P_LONG("AttemptRem beta while found", AttemptRemNamedObject(b));
        if (o)
            ReleaseNamedObject(o);
        P_LONG("AttemptRem beta after release", AttemptRemNamedObject(b));
        list_ns("namespace after beta", ns);
        P_LONG("AttemptRem gamma", AttemptRemNamedObject(d));
        list_ns("namespace after gamma", ns);
        AttemptRemNamedObject(a);
        AttemptRemNamedObject(c);
        list_ns("namespace emptied", ns);
        FreeNamedObject(a);
        P_LONG("freed alpha", 1);
        FreeNamedObject(b);
        P_LONG("freed beta", 1);
        FreeNamedObject(c);
        P_LONG("freed Alpha", 1);
        FreeNamedObject(d);
        P_LONG("freed gamma", 1);
    }

    /* RemNamedObject() while the object is in use: it leaves the name
     * space at once, the message is replied when the last user releases
     * it.  The object is not freed afterwards (freeing a removed object
     * that was in use has crashed AmigaOS 3.1 in earlier probe runs). */
    P_SECTION("RemNamedObject while in use");
    {
        struct NamedObject *u = mk("inuse", 0, 0, 0, 0), *o1, *o2, *o;
        struct MsgPort *port = CreateMsgPort();
        static struct Message msg;
        struct Message *got;

        msg.mn_ReplyPort = port;
        msg.mn_Length = sizeof(msg);
        P_BOOL("add inuse", AddNamedObject(ns, u));
        o1 = FindNamedObject(ns, (STRPTR)"inuse", NULL);
        o2 = FindNamedObject(ns, (STRPTR)"inuse", NULL);
        P_BOOL("found twice", o1 == u && o2 == u);
        P_LONG("AttemptRem while used twice", AttemptRemNamedObject(u));
        RemNamedObject(u, &msg);
        P_NULL("replied after Rem", GetMsg(port));
        o = FindNamedObject(ns, (STRPTR)"inuse", NULL);
        P_NULL("Find after Rem", o);
        if (o)
            ReleaseNamedObject(o);
        list_ns("space after Rem", ns);
        P_STR("NamedObjectName after Rem", (char *)NamedObjectName(u));
        ReleaseNamedObject(u);
        P_NULL("replied after first release", GetMsg(port));
        ReleaseNamedObject(u);
        got = GetMsg(port);
        P_NULL("replied after second release", got);
        if (got)
            P_BOOL("reply ln_Name == object", got->mn_Node.ln_Name == (char *)u);
        DeleteMsgPort(port);
    }

    P_SECTION("GetUniqueID");
    id1 = GetUniqueID();
    id2 = GetUniqueID();
    P_LONG("GetUniqueID delta", (LONG)(id2 - id1));
    P_BOOL("GetUniqueID nonzero", id1 != 0 && id2 != 0);

    P_SECTION("freeing the namespaces");
    FreeNamedObject(ns);
    P_LONG("freed namespace", 1);
    FreeNamedObject(nsnd);
    P_LONG("freed namespace NODUPS", 1);
    FreeNamedObject(nscase);
    P_LONG("freed namespace CASE", 1);
    CloseLibrary(UtilityBase);
    return 0;
}
