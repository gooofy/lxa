#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/configvars.h>
#include <libraries/expansionbase.h>
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

static void expect_ptr(APTR actual, APTR expected, const char *msg)
{
    if (actual != expected)
    {
        fail(msg);
    }
    else
    {
        pass(msg);
    }
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

static struct ConfigDev *alloc_config_dev(UWORD manufacturer, UBYTE product)
{
    struct ConfigDev *config_dev = (struct ConfigDev *)AllocMem(sizeof(struct ConfigDev), MEMF_CLEAR);

    if (config_dev != NULL)
    {
        config_dev->cd_Node.ln_Type = NT_UNKNOWN;
        config_dev->cd_Rom.er_Manufacturer = manufacturer;
        config_dev->cd_Rom.er_Product = product;
    }

    return config_dev;
}

static void free_if_nonnull(APTR ptr, ULONG size)
{
    if (ptr != NULL)
    {
        FreeMem(ptr, size);
    }
}

int main(void)
{
    struct ConfigDev *board_a;
    struct ConfigDev *board_b;
    struct ConfigDev *board_c;
    struct ConfigDev *found;

    print("Testing expansion.library ConfigDev chain APIs\n");

    ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    if (ExpansionBase == NULL)
    {
        print("FAIL: OpenLibrary(expansion.library) failed\n");
        return 20;
    }

    board_a = alloc_config_dev(1000, 1);
    board_b = alloc_config_dev(2000, 2);
    board_c = alloc_config_dev(1000, 3);

    if (board_a == NULL || board_b == NULL || board_c == NULL)
    {
        print("FAIL: AllocMem for ConfigDev fixtures failed\n");
        free_if_nonnull(board_a, sizeof(struct ConfigDev));
        free_if_nonnull(board_b, sizeof(struct ConfigDev));
        free_if_nonnull(board_c, sizeof(struct ConfigDev));
        CloseLibrary((struct Library *)ExpansionBase);
        return 20;
    }

    found = FindConfigDev(NULL, -1, -1);
    expect_ptr(found, NULL, "FindConfigDev returns NULL on empty list");

    AddConfigDev(NULL);
    found = FindConfigDev(NULL, -1, -1);
    expect_ptr(found, NULL, "AddConfigDev(NULL) leaves list unchanged");

    AddConfigDev(board_a);
    AddConfigDev(board_b);
    AddConfigDev(board_c);

    found = FindConfigDev(NULL, -1, -1);
    expect_ptr(found, board_a, "FindConfigDev starts at list head");

    found = FindConfigDev(board_a, -1, -1);
    expect_ptr(found, board_b, "FindConfigDev continues after previous match");

    found = FindConfigDev(board_b, -1, -1);
    expect_ptr(found, board_c, "FindConfigDev reaches tail entry");

    found = FindConfigDev(board_c, -1, -1);
    expect_ptr(found, NULL, "FindConfigDev returns NULL after last entry");

    found = FindConfigDev(NULL, 1000, -1);
    expect_ptr(found, board_a, "FindConfigDev matches manufacturer wildcard product");

    found = FindConfigDev(board_a, 1000, -1);
    expect_ptr(found, board_c, "FindConfigDev finds later manufacturer match");

    found = FindConfigDev(NULL, -1, 2);
    expect_ptr(found, board_b, "FindConfigDev matches product wildcard manufacturer");

    found = FindConfigDev(NULL, 1000, 3);
    expect_ptr(found, board_c, "FindConfigDev matches exact manufacturer and product");

    found = FindConfigDev(NULL, 9999, -1);
    expect_ptr(found, NULL, "FindConfigDev returns NULL for missing manufacturer");

    RemConfigDev(board_b);

    found = FindConfigDev(NULL, -1, -1);
    expect_ptr(found, board_a, "RemConfigDev keeps earlier entries accessible");

    found = FindConfigDev(board_a, -1, -1);
    expect_ptr(found, board_c, "RemConfigDev unlinks removed middle entry");

    found = FindConfigDev(NULL, -1, 2);
    expect_ptr(found, NULL, "Removed entry no longer matches searches");

    RemConfigDev(NULL);
    found = FindConfigDev(NULL, 1000, 3);
    expect_ptr(found, board_c, "RemConfigDev(NULL) is a no-op");

    AddConfigDev(board_b);
    found = FindConfigDev(board_c, -1, -1);
    expect_ptr(found, board_b, "Re-added entry is appended to tail");

    RemConfigDev(board_a);
    found = FindConfigDev(NULL, -1, -1);
    expect_ptr(found, board_c, "Removing head promotes next entry");

    RemConfigDev(board_b);
    found = FindConfigDev(NULL, -1, -1);
    expect_ptr(found, board_c, "Removing tail leaves remaining entry");

    RemConfigDev(board_c);
    found = FindConfigDev(NULL, -1, -1);
    expect_ptr(found, NULL, "Removing final entry empties list");

    free_if_nonnull(board_a, sizeof(struct ConfigDev));
    free_if_nonnull(board_b, sizeof(struct ConfigDev));
    free_if_nonnull(board_c, sizeof(struct ConfigDev));

    CloseLibrary((struct Library *)ExpansionBase);

    if (errors == 0)
    {
        print("PASS: expansion ConfigDev chain tests passed\n");
        return 0;
    }

    print("FAIL: ");
    print_num(errors);
    print(" expansion ConfigDev chain test(s) failed\n");
    return 20;
}
