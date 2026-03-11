#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/configvars.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/expansion_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/expansion.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct ExpansionBase *ExpansionBase;

static LONG errors;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
    {
        len++;
    }

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG value)
{
    char buf[16];
    int i = 0;
    BOOL negative = FALSE;

    if (value < 0)
    {
        negative = TRUE;
        value = -value;
    }

    if (value == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        while (value > 0 && i < (int)sizeof(buf) - 1)
        {
            buf[i++] = '0' + (value % 10);
            value /= 10;
        }
    }

    if (negative)
    {
        buf[i++] = '-';
    }

    while (i-- > 0)
    {
        Write(Output(), &buf[i], 1);
    }
}

static void pass(const char *msg)
{
    print("OK: ");
    print(msg);
    print("\n");
}

static void fail(const char *msg)
{
    print("FAIL: ");
    print(msg);
    print("\n");
    errors++;
}

static void expect_true(BOOL condition, const char *msg)
{
    if (condition)
    {
        pass(msg);
    }
    else
    {
        fail(msg);
    }
}

static void expect_ulong(ULONG actual, ULONG expected, const char *msg)
{
    if (actual == expected)
    {
        pass(msg);
    }
    else
    {
        fail(msg);
    }
}

static void expect_ptr(APTR actual, APTR expected, const char *msg)
{
    if (actual == expected)
    {
        pass(msg);
    }
    else
    {
        fail(msg);
    }
}

static BOOL expect_bstr(BSTR bstr, const char *expected, BOOL expect_terminator, const char *msg)
{
    const UBYTE *bytes = (const UBYTE *)BADDR(bstr);
    ULONG expected_len = 0;
    ULONG index;

    while (expected[expected_len] != 0)
    {
        expected_len++;
    }

    if (bytes == NULL)
    {
        fail(msg);
        return FALSE;
    }

    if (bytes[0] != expected_len + (expect_terminator ? 1 : 0))
    {
        fail(msg);
        return FALSE;
    }

    for (index = 0; index < expected_len; index++)
    {
        if (bytes[index + 1] != (UBYTE)expected[index])
        {
            fail(msg);
            return FALSE;
        }
    }

    if (expect_terminator && bytes[expected_len + 1] != 0)
    {
        fail(msg);
        return FALSE;
    }

    pass(msg);
    return TRUE;
}

static struct BootNode *first_boot_node(void)
{
    struct Node *node = ExpansionBase->MountList.lh_Head;

    if (node == NULL || node->ln_Succ == NULL)
    {
        return NULL;
    }

    return (struct BootNode *)node;
}

static void clear_mount_list(void)
{
    struct BootNode *node = first_boot_node();

    while (node != NULL)
    {
        struct BootNode *next = (struct BootNode *)node->bn_Node.ln_Succ;

        Remove((struct Node *)node);
        FreeMem(node, sizeof(struct BootNode));

        if (next == NULL || next->bn_Node.ln_Succ == NULL)
        {
            node = NULL;
        }
        else
        {
            node = next;
        }
    }
}

static void init_current_binding(struct CurrentBinding *binding,
                                 struct ConfigDev *config_dev,
                                 STRPTR file_name,
                                 STRPTR product_string,
                                 STRPTR *tool_types)
{
    binding->cb_ConfigDev = config_dev;
    binding->cb_FileName = file_name;
    binding->cb_ProductString = product_string;
    binding->cb_ToolTypes = tool_types;
}

int main(void)
{
    struct CurrentBinding binding_in;
    struct CurrentBinding binding_out;
    struct CurrentBinding partial_binding;
    struct ConfigDev config_dev;
    STRPTR tool_types[2];
    ULONG parm_packet[4 + 20];
    struct DeviceNode *device_a;
    struct DeviceNode *device_b;
    struct DeviceNode *device_c;
    struct DeviceNode *device_dup;
    struct FileSysStartupMsg *startup;
    struct DosEnvec *environ;
    struct BootNode *boot_node;
    ULONG returned_size;

    print("Testing expansion.library DOS/binding helpers\n");

    ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    if (ExpansionBase == NULL)
    {
        print("FAIL: OpenLibrary(expansion.library) failed\n");
        return 20;
    }

    tool_types[0] = (STRPTR)"QUIET";
    tool_types[1] = NULL;
    init_current_binding(&binding_in, &config_dev, (STRPTR)"L:expansion.device", (STRPTR)"Test Product", tool_types);

    ObtainConfigBinding();
    SetCurrentBinding(&binding_in, sizeof(binding_in));
    returned_size = GetCurrentBinding(&binding_out, sizeof(binding_out));
    ReleaseConfigBinding();

    expect_ulong(returned_size, sizeof(struct CurrentBinding), "GetCurrentBinding reports the full structure size");
    expect_ptr(binding_out.cb_ConfigDev, &config_dev, "SetCurrentBinding stores the ConfigDev pointer");
    expect_ptr(binding_out.cb_FileName, binding_in.cb_FileName, "SetCurrentBinding stores the file name pointer");
    expect_ptr(binding_out.cb_ProductString, binding_in.cb_ProductString, "SetCurrentBinding stores the product string pointer");
    expect_ptr(binding_out.cb_ToolTypes, binding_in.cb_ToolTypes, "SetCurrentBinding stores the tool types pointer");

    init_current_binding(&partial_binding, &config_dev, (STRPTR)"partial", (STRPTR)0x12345678, (STRPTR *)0x11223344);
    SetCurrentBinding(&partial_binding, sizeof(APTR) * 2);
    binding_out.cb_ConfigDev = (struct ConfigDev *)0xdeadbeef;
    binding_out.cb_FileName = (STRPTR)0xdeadbeef;
    binding_out.cb_ProductString = (STRPTR)0xdeadbeef;
    binding_out.cb_ToolTypes = (STRPTR *)0xdeadbeef;
    returned_size = GetCurrentBinding(&binding_out, sizeof(binding_out));
    expect_ulong(returned_size, sizeof(struct CurrentBinding), "GetCurrentBinding still reports full size after partial SetCurrentBinding");
    expect_ptr(binding_out.cb_ConfigDev, &config_dev, "Partial SetCurrentBinding preserves copied ConfigDev field");
    expect_ptr(binding_out.cb_FileName, partial_binding.cb_FileName, "Partial SetCurrentBinding preserves copied file name field");
    expect_ptr(binding_out.cb_ProductString, NULL, "Partial SetCurrentBinding clears uncopied product string field");
    expect_ptr(binding_out.cb_ToolTypes, NULL, "Partial SetCurrentBinding clears uncopied tool types field");

    expect_ptr(MakeDosNode(NULL), NULL, "MakeDosNode rejects NULL parameter packets");

    parm_packet[0] = (ULONG)"DH0";
    parm_packet[1] = (ULONG)"trackdisk.device";
    parm_packet[2] = 3;
    parm_packet[3] = 0x55;
    parm_packet[4 + DE_TABLESIZE] = DE_BOOTBLOCKS;
    parm_packet[4 + DE_SIZEBLOCK] = 128;
    parm_packet[4 + DE_SECORG] = 0;
    parm_packet[4 + DE_NUMHEADS] = 2;
    parm_packet[4 + DE_SECSPERBLK] = 1;
    parm_packet[4 + DE_BLKSPERTRACK] = 11;
    parm_packet[4 + DE_RESERVEDBLKS] = 2;
    parm_packet[4 + DE_PREFAC] = 0;
    parm_packet[4 + DE_INTERLEAVE] = 0;
    parm_packet[4 + DE_LOWCYL] = 0;
    parm_packet[4 + DE_UPPERCYL] = 79;
    parm_packet[4 + DE_NUMBUFFERS] = 30;
    parm_packet[4 + DE_BUFMEMTYPE] = MEMF_PUBLIC;
    parm_packet[4 + DE_MAXTRANSFER] = 0x1fe00;
    parm_packet[4 + DE_MASK] = 0x7ffffffe;
    parm_packet[4 + DE_BOOTPRI] = 5;
    parm_packet[4 + DE_DOSTYPE] = ID_DOS_DISK;
    parm_packet[4 + DE_BAUD] = 0;
    parm_packet[4 + DE_CONTROL] = 0;
    parm_packet[4 + DE_BOOTBLOCKS] = 2;

    device_a = MakeDosNode((APTR)parm_packet);
    expect_true(device_a != NULL, "MakeDosNode allocates a DeviceNode for valid packets");

    if (device_a != NULL)
    {
        startup = (struct FileSysStartupMsg *)BADDR(device_a->dn_Startup);
        environ = (struct DosEnvec *)BADDR(startup->fssm_Environ);

        expect_ulong(device_a->dn_Type, DLT_DEVICE, "MakeDosNode tags DeviceNode entries as DLT_DEVICE");
        expect_ulong(device_a->dn_StackSize, 4000, "MakeDosNode sets the default stack size");
        expect_ulong(device_a->dn_Priority, 10, "MakeDosNode sets the default startup priority");
        expect_bstr(device_a->dn_Name, "DH0", FALSE, "MakeDosNode stores the DOS device name as a BSTR");
        expect_ulong(startup->fssm_Unit, 3, "MakeDosNode copies the startup unit number");
        expect_ulong(startup->fssm_Flags, 0x55, "MakeDosNode copies the startup flags");
        expect_bstr(startup->fssm_Device, "trackdisk.device", TRUE, "MakeDosNode stores the exec device name as a NULL-terminated BSTR");
        expect_ulong(environ->de_TableSize, DE_BOOTBLOCKS, "MakeDosNode copies the environment table size");
        expect_ulong(environ->de_NumBuffers, 30, "MakeDosNode copies the environment payload");
        expect_ulong(environ->de_BootPri, 5, "MakeDosNode preserves boot priority in the copied environment");
    }

    device_b = MakeDosNode((APTR)parm_packet);
    device_c = MakeDosNode((APTR)parm_packet);
    device_dup = MakeDosNode((APTR)parm_packet);
    expect_true(device_b != NULL && device_c != NULL && device_dup != NULL, "MakeDosNode can allocate multiple independent device nodes");

    if (device_b != NULL)
    {
        ((UBYTE *)BADDR(device_b->dn_Name))[1] = 'H';
        ((UBYTE *)BADDR(device_b->dn_Name))[2] = '1';
    }

    if (device_c != NULL)
    {
        ((UBYTE *)BADDR(device_c->dn_Name))[1] = 'D';
        ((UBYTE *)BADDR(device_c->dn_Name))[2] = 'F';
        ((UBYTE *)BADDR(device_c->dn_Name))[3] = '0';
    }

    expect_true(AddDosNode(0, ADNF_STARTPROC, NULL) == FALSE, "AddDosNode rejects NULL device nodes");
    expect_true(AddDosNode(5, 0, device_a) == TRUE, "AddDosNode accepts the first device node");
    expect_true(AddDosNode(20, ADNF_STARTPROC, device_b) == TRUE, "AddDosNode accepts higher-priority device nodes");
    expect_true(AddDosNode(-10, 0, device_c) == TRUE, "AddDosNode accepts lower-priority device nodes");
    expect_true(AddDosNode(1, 0, device_dup) == FALSE, "AddDosNode rejects duplicate DOS names case-insensitively");

    boot_node = first_boot_node();
    expect_true(boot_node != NULL, "AddDosNode appends BootNodes to ExpansionBase->MountList");
    if (boot_node != NULL)
    {
        expect_ptr(boot_node->bn_DeviceNode, device_b, "AddDosNode orders BootNodes by descending boot priority");
        expect_ulong(boot_node->bn_Node.ln_Type, NT_BOOTNODE, "AddDosNode marks mount-list entries as NT_BOOTNODE");
        expect_ulong(boot_node->bn_Flags, ADNF_STARTPROC, "AddDosNode stores BootNode flags");
        boot_node = (struct BootNode *)boot_node->bn_Node.ln_Succ;
        expect_ptr(boot_node->bn_DeviceNode, device_a, "AddDosNode keeps lower-priority entries after higher-priority ones");
        boot_node = (struct BootNode *)boot_node->bn_Node.ln_Succ;
        expect_ptr(boot_node->bn_DeviceNode, device_c, "AddDosNode places the lowest-priority entry at the tail");
    }

    clear_mount_list();
    FreeVec(device_dup);
    FreeVec(device_c);
    FreeVec(device_b);
    FreeVec(device_a);

    CloseLibrary((struct Library *)ExpansionBase);

    if (errors == 0)
    {
        print("PASS: expansion DOS/binding helper tests passed\n");
        return 0;
    }

    print("FAIL: ");
    print_num(errors);
    print(" expansion DOS/binding helper test(s) failed\n");
    return 20;
}
