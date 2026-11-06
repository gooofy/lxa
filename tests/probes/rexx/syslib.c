/*
 * Probe: rexxsyslib.library without RexxMast - the library base,
 * argstrings (CreateArgstring/DeleteArgstring/LengthArgstring), RexxMsg
 * packets (CreateRexxMsg/DeleteRexxMsg/FillRexxMsg/ClearRexxMsg/IsRexxMsg)
 * and LockRexxBase/UnlockRexxBase.  Output compared with AmigaOS 3.1.
 */
#include "rexxprobe.h"

static void base_info(void)
{
    struct RxsLib *rl = RexxSysBase;
    struct Node *n;
    int i;

    P_SECTION("base");
    P_LONG("lib_Version", rl->rl_Node.lib_Version);
    P_STR("ln_Name", rl->rl_Node.lib_Node.ln_Name);
    P_LONG("ln_Pri", rl->rl_Node.lib_Node.ln_Pri);
    P_LONG("lib_PosSize", rl->rl_Node.lib_PosSize);
    P_LONG("lib_NegSize", rl->rl_Node.lib_NegSize);
    P_HEX("rl_Flags", rl->rl_Flags);
    P_BOOL("rl_SysBase == SysBase", rl->rl_SysBase == (APTR)SysBase);
    P_BOOL("rl_DOSBase == DOSBase", rl->rl_DOSBase == (APTR)DOSBase);
    P_NULL("rl_NIL", (void *)rl->rl_NIL);
    P_LONG("rl_Chunk", rl->rl_Chunk);
    P_LONG("rl_MaxNest", rl->rl_MaxNest);
    P_STR("rl_NULL", rl->rl_NULL ? (char *)rl->rl_NULL->ns_Buff : NULL);
    P_STR("rl_FALSE", rl->rl_FALSE ? (char *)rl->rl_FALSE->ns_Buff : NULL);
    P_STR("rl_TRUE", rl->rl_TRUE ? (char *)rl->rl_TRUE->ns_Buff : NULL);
    P_STR("rl_REXX", rl->rl_REXX ? (char *)rl->rl_REXX->ns_Buff : NULL);
    P_STR("rl_COMMAND", rl->rl_COMMAND ? (char *)rl->rl_COMMAND->ns_Buff : NULL);
    P_STR("rl_STDIN", rl->rl_STDIN ? (char *)rl->rl_STDIN->ns_Buff : NULL);
    P_STR("rl_STDOUT", rl->rl_STDOUT ? (char *)rl->rl_STDOUT->ns_Buff : NULL);
    P_STR("rl_STDERR", rl->rl_STDERR ? (char *)rl->rl_STDERR->ns_Buff : NULL);
    if (rl->rl_TRUE) {
        P_LONG("rl_TRUE ns_Ivalue", rl->rl_TRUE->ns_Ivalue);
        P_LONG("rl_TRUE ns_Length", rl->rl_TRUE->ns_Length);
        P_HEX("rl_TRUE ns_Flags", rl->rl_TRUE->ns_Flags);
        P_HEX("rl_TRUE ns_Hash", rl->rl_TRUE->ns_Hash);
    }
    if (rl->rl_REXX) {
        P_LONG("rl_REXX ns_Length", rl->rl_REXX->ns_Length);
        P_HEX("rl_REXX ns_Flags", rl->rl_REXX->ns_Flags);
        P_HEX("rl_REXX ns_Hash", rl->rl_REXX->ns_Hash);
    }
    P_STR("rl_TaskName", (char *)rl->rl_TaskName);
    P_LONG("rl_TaskPri", rl->rl_TaskPri);
    P_LONG("rl_StackSize", rl->rl_StackSize);
    P_STR("rl_RexxDir", (char *)rl->rl_RexxDir);
    P_NULL("rl_CTABLE", rl->rl_CTABLE);
    if (rl->rl_CTABLE) {
        P_HEX("rl_CTABLE hash", probe_hash(rl->rl_CTABLE, 256));
        for (i = 0; i < 16; i++) {
            static char lbl[] = "rl_CTABLE row x";
            lbl[14] = "0123456789abcdef"[i];
            P_BYTES(lbl, rl->rl_CTABLE + i * 16, 16);
        }
    }
    P_STR("rl_RexxPort ln_Name", rl->rl_RexxPort.mp_Node.ln_Name);
    P_LONG("rl_RexxPort ln_Type", rl->rl_RexxPort.mp_Node.ln_Type);
    P_BOOL("FindPort(REXX) == &rl_RexxPort", find_port("REXX") == &rl->rl_RexxPort);
    P_NULL("FindPort(REXX)", find_port("REXX"));
    P_LONG("rl_ReadLock", rl->rl_ReadLock);
    P_NULL("rl_TraceFH", (void *)rl->rl_TraceFH);
    P_LONG("rl_NumTask", rl->rl_NumTask);
    P_LONG("rl_NumLib", rl->rl_NumLib);
    for (n = rl->rl_LibList.lh_Head, i = 0; n->ln_Succ && i < 8; n = n->ln_Succ, i++) {
        probe_s("rl_LibList: ");
        probe_s(n->ln_Name);
        probe_s(" pri=");
        probe_dec(n->ln_Pri);
        probe_s(" type=");
        probe_dec(n->ln_Type);
        probe_ch('\n');
    }
    P_LONG("rl_NumClip", rl->rl_NumClip);
    P_LONG("rl_NumMsg", rl->rl_NumMsg);
    P_LONG("rl_NumPgm", rl->rl_NumPgm);
    P_LONG("rl_TraceCnt", rl->rl_TraceCnt);
}

static void argstrings(void)
{
    static const LONG lens[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 100 };
    static char text[101];
    UBYTE *s;
    int i;

    P_SECTION("argstrings");
    for (i = 0; i < 100; i++)
        text[i] = (char)('a' + i % 26);
    for (i = 0; i < (int)(sizeof(lens) / sizeof(lens[0])); i++) {
        s = CreateArgstring((STRPTR)text, lens[i]);
        probe_s("CreateArgstring(");
        probe_dec(lens[i]);
        probe_s(") ra_Size=");
        probe_dec(s ? RARG(s)->ra_Size : -1);
        probe_s(" ra_Length=");
        probe_dec(s ? RARG(s)->ra_Length : -1);
        probe_s(" Length=");
        probe_dec(s ? LengthArgstring(s) : -1);
        probe_s(" flags=");
        probe_hex(s ? RARG(s)->ra_Flags : 0, 2);
        probe_s(" hash=");
        probe_hex(s ? RARG(s)->ra_Hash : 0, 2);
        probe_s(" nul=");
        probe_dec(s ? s[lens[i]] : -1);
        probe_ch('\n');
        if (s)
            DeleteArgstring(s);
    }
    s = CreateArgstring((STRPTR)"hello", 5);
    P_ARGSTR("hello", s);
    DeleteArgstring(s);
    s = CreateArgstring((STRPTR)"123", 3);
    P_ARGSTR("123", s);
    DeleteArgstring(s);
    s = CreateArgstring((STRPTR)"-42", 3);
    P_ARGSTR("-42", s);
    DeleteArgstring(s);
    s = CreateArgstring((STRPTR)"1.5", 3);
    P_ARGSTR("1.5", s);
    DeleteArgstring(s);
    s = CreateArgstring((STRPTR)"ab\0cd", 5);
    probe_s("embedded nul: len=");
    probe_dec(LengthArgstring(s));
    probe_s(" bytes:");
    P_BYTES("", s, 6);
    DeleteArgstring(s);
    s = CreateArgstring((STRPTR)"\xff\xfe", 2);
    P_LONG("hash ff fe", RARG(s)->ra_Hash);
    DeleteArgstring(s);
}

static void rexxmsgs(void)
{
    struct MsgPort *port = CreateMsgPort();
    struct RexxMsg *rm, *rm2;
    struct Message plain;
    static char rexxname[] = "REXX";
    LONG r;
    int i;

    P_SECTION("rexxmsg");
    rm = CreateRexxMsg(port, (STRPTR)"ext", (STRPTR)"HOST");
    P_NULL("CreateRexxMsg", rm);
    if (!rm)
        return;
    P_LONG("ln_Type", rm->rm_Node.mn_Node.ln_Type);
    P_LONG("ln_Pri", rm->rm_Node.mn_Node.ln_Pri);
    P_STR("ln_Name", rm->rm_Node.mn_Node.ln_Name);
    P_BOOL("ln_Name == rl_REXX buffer",
           RexxSysBase->rl_REXX && rm->rm_Node.mn_Node.ln_Name == (char *)RexxSysBase->rl_REXX->ns_Buff);
    P_LONG("mn_Length", rm->rm_Node.mn_Length);
    P_BOOL("mn_ReplyPort == port", rm->rm_Node.mn_ReplyPort == port);
    P_NULL("rm_TaskBlock", rm->rm_TaskBlock);
    P_BOOL("rm_LibBase == RexxSysBase", rm->rm_LibBase == (APTR)RexxSysBase);
    P_NULL("rm_LibBase", rm->rm_LibBase);
    P_HEX("rm_Action", rm->rm_Action);
    P_LONG("rm_Result1", rm->rm_Result1);
    P_LONG("rm_Result2", rm->rm_Result2);
    for (i = 0; i < 16; i++)
        if (rm->rm_Args[i])
            P_LONG("rm_Args non-NULL", i);
    P_NULL("rm_PassPort", rm->rm_PassPort);
    P_STR("rm_CommAddr", (char *)rm->rm_CommAddr);
    P_STR("rm_FileExt", (char *)rm->rm_FileExt);
    P_LONG("rm_Stdin", rm->rm_Stdin);
    P_LONG("rm_Stdout", rm->rm_Stdout);
    P_LONG("rm_avail", rm->rm_avail);
    P_BOOL("IsRexxMsg(rm)", IsRexxMsg(rm));

    rm2 = CreateRexxMsg(port, NULL, NULL);
    P_NULL("CreateRexxMsg(NULL ext/host)", rm2);
    if (rm2) {
        P_STR("  rm_CommAddr", (char *)rm2->rm_CommAddr);
        P_STR("  rm_FileExt", (char *)rm2->rm_FileExt);
        DeleteRexxMsg(rm2);
    }

    /* IsRexxMsg looks at the node name */
    plain.mn_Node.ln_Name = NULL;
    plain.mn_Node.ln_Type = NT_MESSAGE;
    P_BOOL("IsRexxMsg(plain, name NULL)", IsRexxMsg((struct RexxMsg *)&plain));
    plain.mn_Node.ln_Name = rexxname;
    P_BOOL("IsRexxMsg(plain, own \"REXX\")", IsRexxMsg((struct RexxMsg *)&plain));
    plain.mn_Node.ln_Name = (char *)"rexx";
    P_BOOL("IsRexxMsg(plain, \"rexx\")", IsRexxMsg((struct RexxMsg *)&plain));
    plain.mn_Node.ln_Name = rm->rm_Node.mn_Node.ln_Name;
    P_BOOL("IsRexxMsg(plain, msg's name)", IsRexxMsg((struct RexxMsg *)&plain));

    /* FillRexxMsg: strings and integers (mask bit set) */
    rm->rm_Args[0] = (STRPTR)"first";
    rm->rm_Args[1] = (STRPTR)-42;
    rm->rm_Args[2] = (STRPTR)"third";
    rm->rm_Args[3] = (STRPTR)123456;
    rm->rm_Args[4] = (STRPTR)0;
    r = FillRexxMsg(rm, 5, (1 << 1) | (1 << 3) | (1 << 4));
    P_LONG("FillRexxMsg(5)", r);
    P_ARGSTR("  arg0", (UBYTE *)rm->rm_Args[0]);
    P_ARGSTR("  arg1", (UBYTE *)rm->rm_Args[1]);
    P_ARGSTR("  arg2", (UBYTE *)rm->rm_Args[2]);
    P_ARGSTR("  arg3", (UBYTE *)rm->rm_Args[3]);
    P_ARGSTR("  arg4", (UBYTE *)rm->rm_Args[4]);
    P_NULL("  arg5", rm->rm_Args[5]);
    ClearRexxMsg(rm, 5);
    for (i = 0; i < 6; i++) {
        probe_s("  after ClearRexxMsg arg");
        probe_dec(i);
        probe_s(rm->rm_Args[i] ? " = non-NULL\n" : " = NULL\n");
    }

    /* a NULL string slot is skipped by FillRexxMsg (string mode) */
    rm->rm_Args[0] = (STRPTR)"x";
    rm->rm_Args[1] = NULL;
    rm->rm_Args[2] = (STRPTR)"z";
    r = FillRexxMsg(rm, 3, 0);
    P_LONG("FillRexxMsg(NULL slot)", r);
    P_ARGSTR("  arg0", (UBYTE *)rm->rm_Args[0]);
    P_NULL("  arg1", rm->rm_Args[1]);
    P_ARGSTR("  arg2", (UBYTE *)rm->rm_Args[2]);
    ClearRexxMsg(rm, 3);

    /* FillRexxMsg(0) */
    r = FillRexxMsg(rm, 0, 0);
    P_LONG("FillRexxMsg(0)", r);

    /* ClearRexxMsg of NULL slots */
    for (i = 0; i < 16; i++)
        rm->rm_Args[i] = NULL;
    ClearRexxMsg(rm, 16);
    P_LONG("ClearRexxMsg(16 NULL) ok", 1);

    DeleteRexxMsg(rm);
    DeleteMsgPort(port);
}

static void locks(void)
{
    P_SECTION("locks");
    P_LONG("rl_ReadLock before", RexxSysBase->rl_ReadLock);
    LockRexxBase(0);
    P_LONG("rl_ReadLock locked", RexxSysBase->rl_ReadLock);
    LockRexxBase(0);
    P_LONG("rl_ReadLock locked twice", RexxSysBase->rl_ReadLock);
    UnlockRexxBase(0);
    UnlockRexxBase(0);
    P_LONG("rl_ReadLock after", RexxSysBase->rl_ReadLock);
}

int main(void)
{
    struct Library *v45;

    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    P_NULL("OpenLibrary(rexxsyslib, 0)", RexxSysBase);
    if (!RexxSysBase)
        return 0;
    v45 = OpenLibrary((STRPTR)"rexxsyslib.library", 45);
    P_NULL("OpenLibrary(rexxsyslib, 45)", v45);
    if (v45)
        CloseLibrary(v45);
    v45 = OpenLibrary((STRPTR)"rexxsyslib.library", 36);
    P_NULL("OpenLibrary(rexxsyslib, 36)", v45);
    if (v45)
        CloseLibrary(v45);

    base_info();
    argstrings();
    rexxmsgs();
    locks();

    CloseLibrary((struct Library *)RexxSysBase);
    return 0;
}
