/*
 * Probe (Phase 222g): InitResident() with an RTF_AUTOINIT library resident -
 * which Library fields come from the Resident tag (rt_Name, rt_Version,
 * rt_IdString, rt_Type, rt_Pri) and which only from the data table, and
 * whether the library is added to SysBase->LibList.  Phase 237: a resident
 * of a type exec does not keep a list for (Fish JukeBox: 0xFD modules) is
 * still made and returned.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/nodes.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

__asm__(
    "    .text\n"
    "    .globl _ir_open\n"
    "_ir_open:\n"
    "    addq.w #1,32(a6)\n"
    "    move.l a6,d0\n"
    "    rts\n"
    "    .globl _ir_init\n"
    "_ir_init:\n"                       /* d0 = library, a0 = segList */
    "    move.l d0,a1\n"
    "    move.l 10(a1),_ir_seen_name\n"
    "    move.l 24(a1),_ir_seen_id\n"
    "    move.w 20(a1),_ir_seen_ver\n"
    "    move.b 9(a1),_ir_seen_pri\n"
    "    move.b 14(a1),_ir_seen_flags\n"
    "    rts\n"
    "    .globl _ir_null\n"
    "_ir_null:\n"
    "    moveq #0,d0\n"
    "    rts\n");

extern void ir_open(void);
extern void ir_null(void);
extern void ir_init(void);
char *ir_seen_name = (char *)1, *ir_seen_id = (char *)1;
UWORD ir_seen_ver = 0xffff;
BYTE ir_seen_pri = 99;
UBYTE ir_seen_flags = 0xff;

static APTR functab[] = { (APTR)ir_open, (APTR)ir_null, (APTR)ir_null, (APTR)ir_null, (APTR)-1 };

static char name_a[] = "probeinita.library";
static char name_b[] = "probeinitb.library";
static char id_a[] = "probeinita 7.1 (1.1.24)\r\n";
static char id_b[] = "probeinitb 7.1 (1.1.24)\r\n";
static char name_c[] = "probeinitc.library";
static char id_c[] = "probeinitc 7.1 (1.1.24)\r\n";
static char data_name[] = "dataname.library";
static char data_id[] = "data id";

/* data table: name, version 9, revision 3, IdString */
static const struct {
    UBYTE c1; UBYTE o1; ULONG v1;
    UWORD c2; UWORD o2; UWORD v2;
    UWORD c3; UWORD o3; UWORD v3;
    UBYTE c4; UBYTE o4; ULONG v4;
    ULONG end;
} datatab = {
    0x80, 10, (ULONG)data_name,                 /* ln_Name */
    0xd000, 20, 9,                              /* lib_Version (INITWORD) */
    0xd000, 22, 3,                              /* lib_Revision */
    0x80, 24, (ULONG)data_id,                   /* lib_IdString */
    0
};

struct IrInitTable { ULONG size; APTR *functions; APTR data; APTR init; };

static struct IrInitTable init_a = { sizeof(struct Library), functab, NULL, NULL };
static struct IrInitTable init_b = { sizeof(struct Library), functab, (APTR)&datatab, NULL };
static struct IrInitTable init_c = { sizeof(struct Library), functab, (APTR)&datatab, (APTR)ir_init };

static struct Resident res_a = {
    RTC_MATCHWORD, &res_a, &res_a + 1, RTF_AUTOINIT, 7, NT_LIBRARY, 3, name_a, id_a, &init_a
};
static struct Resident res_b = {
    RTC_MATCHWORD, &res_b, &res_b + 1, RTF_AUTOINIT, 7, NT_LIBRARY, -2, name_b, id_b, &init_b
};

static struct Resident res_c = {
    RTC_MATCHWORD, &res_c, &res_c + 1, RTF_AUTOINIT, 7, NT_LIBRARY, 5, name_c, id_c, &init_c
};

static char name_d[] = "probeinitd.module";
static char id_d[] = "probeinitd 7.1 (1.1.24)\r\n";
static struct IrInitTable init_d = { sizeof(struct Library) + 8, functab, NULL, NULL };
static struct Resident res_d = {
    RTC_MATCHWORD, &res_d, &res_d + 1, RTF_AUTOINIT, 7, 0xfd, 0, name_d, id_d, &init_d
};

static int in_list(struct List *l, struct Node *x)
{
    struct Node *n;
    for (n = l->lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (n == x)
            return 1;
    return 0;
}

static void pstr(const char *s)
{
    if (!s) {
        probe_s("NULL");
        return;
    }
    probe_ch('"');
    while (*s) {
        UBYTE c = *s++;
        if (c == '\r')
            probe_s("\\r");
        else if (c == '\n')
            probe_s("\\n");
        else
            probe_ch(c);
    }
    probe_ch('"');
}

static void show(const char *label, struct Resident *r)
{
    struct Library *lib = (struct Library *)InitResident(r, 0);
    struct Node *n;

    P_SECTION(label);
    probe_s("result ");
    probe_s(lib ? "library" : "NULL");
    probe_ch('\n');
    if (!lib)
        return;
    probe_s("ln_Type ");
    probe_dec(lib->lib_Node.ln_Type);
    probe_s(" ln_Pri ");
    probe_dec(lib->lib_Node.ln_Pri);
    probe_s(" ln_Name ");
    pstr(lib->lib_Node.ln_Name);
    probe_ch('\n');
    probe_s("lib_Version ");
    probe_dec(lib->lib_Version);
    probe_s(" lib_Revision ");
    probe_dec(lib->lib_Revision);
    probe_s(" lib_IdString ");
    pstr(lib->lib_IdString);
    probe_ch('\n');
    probe_s("lib_OpenCnt ");
    probe_dec(lib->lib_OpenCnt);
    probe_s(" lib_Flags ");
    probe_dec(lib->lib_Flags);
    probe_ch('\n');
    Forbid();
    for (n = SysBase->LibList.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (n == (struct Node *)lib)
            break;
    probe_s("in LibList ");
    probe_s(n->ln_Succ ? "yes" : "no");
    probe_ch('\n');
    if (n->ln_Succ)
        Remove(n);
    Permit();
}

int main(void)
{
    show("no data table", &res_a);
    show("data table sets name, version, revision, IdString", &res_b);
    show("data table and init function", &res_c);
    probe_s("seen by the init function: ln_Name ");
    pstr(ir_seen_name);
    probe_s(" lib_IdString ");
    pstr(ir_seen_id);
    probe_s(" lib_Version ");
    probe_dec(ir_seen_ver);
    probe_s(" ln_Pri ");
    probe_dec(ir_seen_pri);
    probe_s(" lib_Flags ");
    probe_dec(ir_seen_flags);
    probe_ch('\n');
    {
        struct Library *lib = (struct Library *)InitResident(&res_d, 0);
        P_SECTION("type 0xfd (no list)");
        probe_s("result ");
        probe_s(lib ? "base" : "NULL");
        probe_ch('\n');
        if (lib) {
            probe_s("ln_Type ");
            probe_dec(lib->lib_Node.ln_Type);
            probe_s(" ln_Name ");
            pstr(lib->lib_Node.ln_Name);
            probe_s(" lib_Version ");
            probe_dec(lib->lib_Version);
            probe_s(" lib_NegSize ");
            probe_dec(lib->lib_NegSize);
            probe_s(" lib_PosSize ");
            probe_dec(lib->lib_PosSize);
            probe_ch('\n');
            Forbid();
            probe_s("in a system list ");
            probe_s(in_list(&SysBase->LibList, &lib->lib_Node) || in_list(&SysBase->DeviceList, &lib->lib_Node) ||
                    in_list(&SysBase->ResourceList, &lib->lib_Node) ? "yes" : "no");
            Permit();
            probe_ch('\n');
        }
    }
    return 0;
}
