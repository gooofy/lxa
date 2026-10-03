/*
 * Probe (Phase 222a): exec.library lists - AddHead, AddTail, Insert,
 * Remove, RemHead, RemTail, Enqueue, FindName, plus the list header
 * conventions (empty list, lh_TailPred) that programs rely on.
 */
#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static struct Node nodes[10];
static char names[10][4];

static void newlist(struct List *l)
{
    l->lh_Head = (struct Node *)&l->lh_Tail;
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&l->lh_Head;
    l->lh_Type = 0;
}

static int idx(struct Node *n)
{
    int i;
    if (!n)
        return -1;
    for (i = 0; i < 10; i++)
        if (n == &nodes[i])
            return i;
    return -2;
}

static void show(const char *label, struct List *l)
{
    struct Node *n;
    int k = 0, ok = 1;
    probe_s(label);
    probe_s(" =");
    for (n = l->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        probe_ch(' ');
        probe_s(n->ln_Name);
        if (n->ln_Pri) {
            probe_ch('/');
            probe_dec(n->ln_Pri);
        }
        if (n->ln_Succ->ln_Pred != n)
            ok = 0;
        if (++k > 12)
            break;
    }
    /* backwards links must agree */
    for (n = l->lh_TailPred, k = 0; n->ln_Pred; n = n->ln_Pred)
        if (++k > 12)
            break;
    probe_s(ok ? " [links ok]" : " [links BROKEN]");
    probe_ch('\n');
}

static struct Node *nd(int i, BYTE pri)
{
    nodes[i].ln_Pri = pri;
    nodes[i].ln_Type = NT_USER;
    nodes[i].ln_Name = names[i];
    return &nodes[i];
}

int main(void)
{
    struct List l;
    int i;

    for (i = 0; i < 10; i++) {
        names[i][0] = 'n';
        names[i][1] = (char)('0' + i);
        names[i][2] = 0;
    }

    P_SECTION("empty list");
    newlist(&l);
    show("empty", &l);
    P_NULL("RemHead(empty)", RemHead(&l));
    P_NULL("RemTail(empty)", RemTail(&l));
    P_NULL("FindName(empty)", FindName(&l, (STRPTR)"n0"));
    P_BOOL("IsListEmpty", l.lh_TailPred == (struct Node *)&l);

    P_SECTION("AddHead/AddTail/Insert/Remove");
    AddTail(&l, nd(0, 0));
    AddTail(&l, nd(1, 0));
    AddHead(&l, nd(2, 0));
    show("AddTail n0, AddTail n1, AddHead n2", &l);
    Insert(&l, nd(3, 0), &nodes[0]);
    show("Insert n3 after n0", &l);
    Insert(&l, nd(4, 0), NULL);
    show("Insert n4 after NULL (= head)", &l);
    Insert(&l, nd(5, 0), &nodes[1]);
    show("Insert n5 after tail n1", &l);
    Insert(&l, nd(6, 0), (struct Node *)&l.lh_Head);
    show("Insert n6 after list header", &l);
    Remove(&nodes[3]);
    show("Remove n3", &l);
    P_LONG("RemHead", idx(RemHead(&l)));
    P_LONG("RemTail", idx(RemTail(&l)));
    show("after RemHead/RemTail", &l);
    P_LONG("FindName n0", idx(FindName(&l, (STRPTR)"n0")));
    P_LONG("FindName N0 (case)", idx(FindName(&l, (STRPTR)"N0")));
    P_LONG("FindName n9", idx(FindName(&l, (STRPTR)"n9")));
    while (RemHead(&l))
        ;
    show("drained", &l);

    P_SECTION("FindName continues from a node");
    newlist(&l);
    AddTail(&l, nd(0, 0));
    AddTail(&l, nd(1, 0));
    AddTail(&l, nd(2, 0));
    nodes[2].ln_Name = names[0];         /* duplicate name "n0" */
    {
        struct Node *a = FindName(&l, (STRPTR)"n0");
        struct Node *b = a ? FindName((struct List *)a, (STRPTR)"n0") : NULL;
        struct Node *c = b ? FindName((struct List *)b, (STRPTR)"n0") : NULL;
        P_LONG("first n0", idx(a));
        P_LONG("next n0", idx(b));
        P_LONG("next next n0", idx(c));
    }
    nodes[2].ln_Name = names[2];

    P_SECTION("Enqueue");
    newlist(&l);
    Enqueue(&l, nd(0, 0));
    Enqueue(&l, nd(1, 5));
    Enqueue(&l, nd(2, -5));
    Enqueue(&l, nd(3, 5));
    Enqueue(&l, nd(4, 0));
    Enqueue(&l, nd(5, 127));
    Enqueue(&l, nd(6, -128));
    Enqueue(&l, nd(7, -5));
    Enqueue(&l, nd(8, 1));
    show("Enqueue order", &l);
    P_LONG("RemHead (highest pri)", idx(RemHead(&l)));
    P_LONG("RemTail (lowest pri)", idx(RemTail(&l)));
    show("after", &l);

    P_SECTION("node fields untouched");
    P_LONG("n0 ln_Type", nodes[0].ln_Type);
    P_LONG("n5 ln_Pri", nodes[5].ln_Pri);
    return 0;
}
