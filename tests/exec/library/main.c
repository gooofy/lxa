/*
 * Test: exec/library
 *
 * Phase 73: Core Library Resource Management & Stability
 *
 * Tests:
 * 1. OpenLibrary() returns non-NULL for built-in libraries
 * 2. lib_OpenCnt increments on OpenLibrary()
 * 3. lib_OpenCnt decrements on CloseLibrary()
 * 4. Multiple OpenLibrary() calls increment correctly
 * 5. OpenLibrary() for non-existent library returns NULL
 * 6. Version check on OpenLibrary()
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/record.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#undef SetFunction

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static const char g_test_library_name[] = "phase78test.library";

static struct Library *test_init(register struct Library *lib __asm("d0"),
                                 register BPTR seglist __asm("a0"),
                                 register struct ExecBase *sys_base __asm("a6"))
{
    (void)seglist;
    (void)sys_base;

    lib->lib_Node.ln_Type = NT_LIBRARY;
    lib->lib_Node.ln_Name = (char *)g_test_library_name;
    lib->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    lib->lib_Version = 5;
    lib->lib_Revision = 1;
    return lib;
}

static struct Library *test_open(register ULONG version __asm("d0"),
                                 register struct Library *lib __asm("a6"))
{
    (void)version;
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

static BPTR test_close(register struct Library *lib __asm("a6"))
{
    lib->lib_OpenCnt--;
    return 0;
}

static BPTR test_expunge(register struct Library *lib __asm("a6"))
{
    if (lib->lib_OpenCnt == 0)
    {
        Remove(&lib->lib_Node);
        return (BPTR)1;
    }

    lib->lib_Flags |= LIBF_DELEXP;
    return 0;
}

static ULONG test_ext(void)
{
    return 0;
}

static ULONG test_old_function(void)
{
    return 0x11111111;
}

static ULONG test_new_function(void)
{
    return 0x22222222;
}

static APTR g_test_library_vectors[] = {
    (APTR)test_open,
    (APTR)test_close,
    (APTR)test_expunge,
    (APTR)test_ext,
    (APTR)test_old_function,
    (APTR)-1
};

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = FALSE;

    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    if (n == 0)
    {
        *--p = '0';
    }
    else
    {
        while (n > 0)
        {
            *--p = '0' + (n % 10);
            n /= 10;
        }
    }
    if (neg)
        *--p = '-';
    print(p);
}

/*
 * Test a single library's reference counting.
 * Returns number of errors.
 */
static int test_library_refcount(const char *name)
{
    int errors = 0;
    struct Library *lib1, *lib2;
    UWORD cnt_after_open1, cnt_after_open2, cnt_after_close1, cnt_after_close2;

    print("--- Testing ");
    print(name);
    print(" ---\n");

    /* Find the library node to read lib_OpenCnt directly */
    /* First, open it to get the base pointer */
    lib1 = OpenLibrary((CONST_STRPTR)name, 0);
    if (lib1 == NULL)
    {
        print("FAIL: OpenLibrary(\"");
        print(name);
        print("\", 0) returned NULL\n");
        return 1;
    }
    print("OK: OpenLibrary() returned non-NULL\n");

    /* Read count after first open */
    cnt_after_open1 = lib1->lib_OpenCnt;
    print("  lib_OpenCnt after 1st open: ");
    print_num(cnt_after_open1);
    print("\n");

    if (cnt_after_open1 < 1)
    {
        print("FAIL: lib_OpenCnt should be >= 1 after OpenLibrary()\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt >= 1 after OpenLibrary()\n");
    }

    /* Open again - count should increment */
    lib2 = OpenLibrary((CONST_STRPTR)name, 0);
    if (lib2 == NULL)
    {
        print("FAIL: 2nd OpenLibrary() returned NULL\n");
        errors++;
        CloseLibrary(lib1);
        return errors;
    }

    cnt_after_open2 = lib2->lib_OpenCnt;
    print("  lib_OpenCnt after 2nd open: ");
    print_num(cnt_after_open2);
    print("\n");

    if (cnt_after_open2 != cnt_after_open1 + 1)
    {
        print("FAIL: lib_OpenCnt should have incremented by 1\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt incremented correctly on 2nd open\n");
    }

    /* Verify same base returned */
    if (lib1 != lib2)
    {
        print("FAIL: 2nd OpenLibrary() returned different base pointer\n");
        errors++;
    }
    else
    {
        print("OK: Same library base returned\n");
    }

    /* Close once - count should decrement */
    CloseLibrary(lib2);
    cnt_after_close1 = lib1->lib_OpenCnt;
    print("  lib_OpenCnt after 1st close: ");
    print_num(cnt_after_close1);
    print("\n");

    if (cnt_after_close1 != cnt_after_open2 - 1)
    {
        print("FAIL: lib_OpenCnt should have decremented by 1\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt decremented correctly on close\n");
    }

    /* Close again */
    CloseLibrary(lib1);
    cnt_after_close2 = lib1->lib_OpenCnt;
    print("  lib_OpenCnt after 2nd close: ");
    print_num(cnt_after_close2);
    print("\n");

    if (cnt_after_close2 != cnt_after_close1 - 1)
    {
        print("FAIL: lib_OpenCnt should have decremented by 1 again\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt decremented correctly on 2nd close\n");
    }

    print("\n");
    return errors;
}

static int test_exec_library_helpers(void)
{
    int errors = 0;
    struct Library *lib;
    struct Library *opened;
    struct Node *found;
    APTR old_function;
    ULONG old_sum;
    UBYTE *lib_mem;
    ULONG lib_size;

    print("--- Test: Exec library helpers ---\n");

    lib = MakeLibrary(g_test_library_vectors, NULL, (ULONG (*)())test_init, sizeof(struct Library), 0);
    if (!lib)
    {
        print("FAIL: MakeLibrary returned NULL\n\n");
        return 1;
    }
    print("OK: MakeLibrary returned non-NULL\n");

    if (lib->lib_NegSize == 30)
    {
        print("OK: MakeLibrary computed 5 vectors of negative size\n");
    }
    else
    {
        print("FAIL: MakeLibrary negative size incorrect\n");
        errors++;
    }

    AddLibrary(lib);
    found = FindName(&SysBase->LibList, (CONST_STRPTR)g_test_library_name);
    if (found == &lib->lib_Node)
    {
        print("OK: AddLibrary inserted library into LibList\n");
    }
    else
    {
        print("FAIL: AddLibrary did not insert library into LibList\n");
        errors++;
    }

    opened = OpenLibrary((CONST_STRPTR)g_test_library_name, 5);
    if (opened == lib)
    {
        print("OK: OpenLibrary found custom library\n");
    }
    else
    {
        print("FAIL: OpenLibrary did not return custom library\n");
        errors++;
    }

    if (OpenLibrary((CONST_STRPTR)g_test_library_name, 6) == NULL)
    {
        print("OK: OpenLibrary rejected too-high version\n");
    }
    else
    {
        print("FAIL: OpenLibrary should reject too-high version\n");
        errors++;
    }

    old_sum = lib->lib_Sum;
    old_function = SetFunction(lib, -30, (ULONG (*)())test_new_function);
    if (old_function == (APTR)test_old_function)
    {
        print("OK: SetFunction returned previous function pointer\n");
    }
    else
    {
        print("FAIL: SetFunction returned wrong previous function pointer\n");
        errors++;
    }

    if (lib->lib_Sum != old_sum)
    {
        print("OK: SumLibrary updated checksum after SetFunction\n");
    }
    else
    {
        print("FAIL: SumLibrary did not update checksum after SetFunction\n");
        errors++;
    }

    CloseLibrary(opened);
    if (lib->lib_OpenCnt == 0)
    {
        print("OK: CloseLibrary decremented custom library open count\n");
    }
    else
    {
        print("FAIL: CloseLibrary did not decrement custom library open count\n");
        errors++;
    }

    RemLibrary(lib);
    found = FindName(&SysBase->LibList, (CONST_STRPTR)g_test_library_name);
    if (found == NULL)
    {
        print("OK: RemLibrary expunged custom library from LibList\n");
    }
    else
    {
        print("FAIL: RemLibrary did not expunge custom library\n");
        errors++;
    }

    lib_mem = (UBYTE *)lib - lib->lib_NegSize;
    lib_size = lib->lib_NegSize + lib->lib_PosSize;
    FreeMem(lib_mem, lib_size);

    CloseLibrary(NULL);
    print("OK: CloseLibrary(NULL) did not crash\n\n");
    return errors;
}

static int test_external_library_scope(void)
{
    static const char *external_libs[] = {
        "commodities.library",
        "rexxsyslib.library",
        "datatypes.library",
        "dopus.library",
        "powerpacker.library",
        "arp.library",
        "iff.library",
        "Explode.Library",
        "Icon.Library"
    };
    int errors = 0;
    ULONG i;

    print("--- Test: External library scope ---\n");

    for (i = 0; i < (sizeof(external_libs) / sizeof(external_libs[0])); i++)
    {
        const char *name = external_libs[i];
        struct Resident *resident = FindResident((CONST_STRPTR)name);
        struct Node *node = FindName(&SysBase->LibList, (CONST_STRPTR)name);

        print("Checking ");
        print(name);
        print("...\n");

        if (resident != NULL)
        {
            print("FAIL: library should not be resident in ROM\n");
            errors++;
        }
        else
        {
            print("OK: library is not resident in ROM\n");
        }

        if (node != NULL)
        {
            print("FAIL: library should not be pre-registered in LibList\n");
            errors++;
        }
        else
        {
            print("OK: library is not pre-registered in LibList\n");
        }
    }

    print("\n");
    return errors;
}

static int test_dos_library_init_state(void)
{
    int errors = 0;
    ULONG *task_array;
    struct DosInfo *dos_info;

    print("--- Test: DOS InitLib state ---\n");

    if (DOSBase == NULL)
    {
        print("FAIL: DOSBase should be available\n\n");
        return 1;
    }

    if (DOSBase->dl_Root != NULL)
    {
        print("OK: dos.library initialized RootNode\n");
    }
    else
    {
        print("FAIL: dos.library RootNode is NULL\n");
        errors++;
    }

    task_array = DOSBase->dl_Root ? (ULONG *)BADDR(DOSBase->dl_Root->rn_TaskArray) : NULL;
    if (task_array != NULL)
    {
        print("OK: dos.library initialized RootNode task array\n");
        if (task_array[0] >= 8)
        {
            print("OK: dos.library task array has expected initial capacity\n");
        }
        else
        {
            print("FAIL: dos.library task array capacity is too small\n");
            errors++;
        }
    }
    else
    {
        print("FAIL: dos.library task array is NULL\n");
        errors++;
    }

    dos_info = (DOSBase->dl_Root && DOSBase->dl_Root->rn_Info != 0)
        ? (struct DosInfo *)BADDR(DOSBase->dl_Root->rn_Info)
        : NULL;

    if (dos_info != NULL)
    {
        print("OK: dos.library initialized DosInfo\n");
    }
    else
    {
        print("FAIL: dos.library DosInfo is NULL\n");
        errors++;
    }

    if (DOSBase->dl_Root != NULL &&
        DOSBase->dl_Root->rn_CliList.mlh_Head == (struct MinNode *)&DOSBase->dl_Root->rn_CliList.mlh_Tail &&
        DOSBase->dl_Root->rn_CliList.mlh_Tail == NULL &&
        DOSBase->dl_Root->rn_CliList.mlh_TailPred == (struct MinNode *)&DOSBase->dl_Root->rn_CliList.mlh_Head)
    {
        print("OK: dos.library initialized an empty CLI list\n");
    }
    else
    {
        print("FAIL: dos.library CLI list is not initialized like NEWLIST\n");
        errors++;
    }

    if (dos_info != NULL &&
        dos_info->di_DevLock.ss_Link.ln_Type == NT_SIGNALSEM &&
        dos_info->di_EntryLock.ss_Link.ln_Type == NT_SIGNALSEM &&
        dos_info->di_DeleteLock.ss_Link.ln_Type == NT_SIGNALSEM)
    {
        print("OK: dos.library initialized DosInfo semaphores\n");
    }
    else
    {
        print("FAIL: dos.library DosInfo semaphore state is not initialized\n");
        errors++;
    }

    if (DOSBase->dl_Errors == NULL &&
        DOSBase->dl_TimeReq == NULL &&
        DOSBase->dl_UtilityBase == NULL &&
        DOSBase->dl_IntuitionBase == NULL)
    {
        print("OK: dos.library private startup pointers stay cleared\n");
    }
    else
    {
        print("FAIL: dos.library private startup pointers should be cleared\n");
        errors++;
    }

    print("\n");
    return errors;
}

static int test_dos_lockrecord_stub_closed(void)
{
    int errors = 0;
    BPTR fh;
    BOOL ok;

    print("--- Test: DOS LockRecord entry point ---\n");

    fh = Open((CONST_STRPTR)"library_lockrecord_test.dat", MODE_NEWFILE);
    if (fh == 0)
    {
        print("FAIL: Open() for LockRecord probe failed\n\n");
        return 1;
    }

    ok = LockRecord(fh, 0, 1, REC_SHARED_IMMED, 0);
    if (ok)
    {
        print("OK: LockRecord() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: LockRecord() unexpectedly failed\n");
        errors++;
    }

    Close(fh);
    DeleteFile((CONST_STRPTR)"library_lockrecord_test.dat");

    print("\n");
    return errors;
}

static int test_dos_lockrecords_stub_closed(void)
{
    int errors = 0;
    BPTR fh;
    BOOL ok;
    struct RecordLock records[2];

    print("--- Test: DOS LockRecords entry point ---\n");

    fh = Open((CONST_STRPTR)"library_lockrecords_test.dat", MODE_NEWFILE);
    if (fh == 0)
    {
        print("FAIL: Open() for LockRecords probe failed\n\n");
        return 1;
    }

    records[0].rec_FH = fh;
    records[0].rec_Offset = 0;
    records[0].rec_Length = 1;
    records[0].rec_Mode = REC_SHARED_IMMED;
    records[1].rec_FH = 0;
    records[1].rec_Offset = 0;
    records[1].rec_Length = 0;
    records[1].rec_Mode = 0;

    ok = LockRecords(records, 0);
    if (ok)
    {
        print("OK: LockRecords() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: LockRecords() unexpectedly failed\n");
        errors++;
    }

    Close(fh);
    DeleteFile((CONST_STRPTR)"library_lockrecords_test.dat");

    print("\n");
    return errors;
}

static int test_dos_unlockrecord_stub_closed(void)
{
    int errors = 0;
    BPTR fh;
    BOOL ok;

    print("--- Test: DOS UnLockRecord entry point ---\n");

    fh = Open((CONST_STRPTR)"library_unlockrecord_test.dat", MODE_NEWFILE);
    if (fh == 0)
    {
        print("FAIL: Open() for UnLockRecord probe failed\n\n");
        return 1;
    }

    if (!LockRecord(fh, 0, 1, REC_SHARED_IMMED, 0))
    {
        print("FAIL: LockRecord() setup for UnLockRecord probe failed\n");
        errors++;
    }
    else
    {
        ok = UnLockRecord(fh, 0, 1);
        if (ok)
        {
            print("OK: UnLockRecord() no longer behaves like a stub\n");
        }
        else
        {
            print("FAIL: UnLockRecord() unexpectedly failed\n");
            errors++;
        }
    }

    Close(fh);
    DeleteFile((CONST_STRPTR)"library_unlockrecord_test.dat");

    print("\n");
    return errors;
}

static int test_dos_unlockrecords_stub_closed(void)
{
    int errors = 0;
    BPTR fh;
    BOOL ok;
    struct RecordLock records[2];

    print("--- Test: DOS UnLockRecords entry point ---\n");

    fh = Open((CONST_STRPTR)"library_unlockrecords_test.dat", MODE_NEWFILE);
    if (fh == 0)
    {
        print("FAIL: Open() for UnLockRecords probe failed\n\n");
        return 1;
    }

    if (!LockRecord(fh, 0, 1, REC_SHARED_IMMED, 0))
    {
        print("FAIL: LockRecord() setup for UnLockRecords probe failed\n");
        errors++;
    }
    else
    {
        records[0].rec_FH = fh;
        records[0].rec_Offset = 0;
        records[0].rec_Length = 1;
        records[0].rec_Mode = REC_SHARED_IMMED;
        records[1].rec_FH = 0;
        records[1].rec_Offset = 0;
        records[1].rec_Length = 0;
        records[1].rec_Mode = 0;

        ok = UnLockRecords(records);
        if (ok)
        {
            print("OK: UnLockRecords() no longer behaves like a stub\n");
        }
        else
        {
            print("FAIL: UnLockRecords() unexpectedly failed\n");
            errors++;
        }
    }

    Close(fh);
    DeleteFile((CONST_STRPTR)"library_unlockrecords_test.dat");

    print("\n");
    return errors;
}

static int test_dos_splitname_stub_closed(void)
{
    int errors = 0;
    WORD pos;
    char buffer[16];

    print("--- Test: DOS SplitName entry point ---\n");

    pos = SplitName((CONST_STRPTR)"SYS:Tools/Dir", ':', (STRPTR)buffer, 0, sizeof(buffer));
    if (pos == 4 && buffer[0] == 'S' && buffer[1] == 'Y' && buffer[2] == 'S' && buffer[3] == '\0')
    {
        print("OK: SplitName() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: SplitName() unexpectedly failed\n");
        errors++;
    }

    print("\n");
    return errors;
}

static int test_dos_setmode_stub_closed(void)
{
    int errors = 0;
    BPTR fh;
    BOOL ok;

    print("--- Test: DOS SetMode entry point ---\n");

    fh = Open((CONST_STRPTR)"CON:0/0/320/60/Library SetMode", MODE_OLDFILE);
    if (fh == 0)
    {
        print("FAIL: Open() for SetMode probe failed\n\n");
        return 1;
    }

    ok = SetMode(fh, 0);
    if (ok)
    {
        print("OK: SetMode() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: SetMode() unexpectedly failed\n");
        errors++;
    }

    Close(fh);

    print("\n");
    return errors;
}

static int test_dos_changemode_stub_closed(void)
{
    int errors = 0;
    BPTR lock;
    BOOL ok;

    print("--- Test: DOS ChangeMode entry point ---\n");

    lock = Lock((CONST_STRPTR)"SYS:Tests", SHARED_LOCK);
    if (lock == 0)
    {
        print("FAIL: Lock() for ChangeMode probe failed\n\n");
        return 1;
    }

    ok = ChangeMode(CHANGE_LOCK, lock, SHARED_LOCK);
    if (ok)
    {
        print("OK: ChangeMode() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: ChangeMode() unexpectedly failed\n");
        errors++;
    }

    UnLock(lock);

    print("\n");
    return errors;
}

static int test_dos_errorreport_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR old_window_ptr;
    LONG result;

    print("--- Test: DOS ErrorReport entry point ---\n");

    old_window_ptr = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;

    result = ErrorReport(ERROR_NO_DISK, REPORT_INSERT, (ULONG)"DH0:", NULL);
    if (result == DOSTRUE && IoErr() == ERROR_NO_DISK)
    {
        print("OK: ErrorReport() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: ErrorReport() unexpectedly failed\n");
        errors++;
    }

    me->pr_WindowPtr = old_window_ptr;

    print("\n");
    return errors;
}

static int test_dos_getconsoletask_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct MsgPort *probe_port;
    struct MsgPort *old_console_task;

    print("--- Test: DOS GetConsoleTask entry point ---\n");

    probe_port = CreateMsgPort();
    if (!probe_port)
    {
        print("FAIL: Could not allocate probe port for GetConsoleTask\n\n");
        return 1;
    }

    old_console_task = me->pr_ConsoleTask;
    me->pr_ConsoleTask = probe_port;

    if (GetConsoleTask() == probe_port)
    {
        print("OK: GetConsoleTask() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: GetConsoleTask() returned the wrong port\n");
        errors++;
    }

    me->pr_ConsoleTask = old_console_task;
    DeleteMsgPort(probe_port);

    print("\n");
    return errors;
}

static int test_dos_setconsoletask_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct MsgPort *probe_port;
    struct MsgPort *old_console_task;
    struct MsgPort *previous;

    print("--- Test: DOS SetConsoleTask entry point ---\n");

    probe_port = CreateMsgPort();
    if (!probe_port)
    {
        print("FAIL: Could not allocate probe port for SetConsoleTask\n\n");
        return 1;
    }

    old_console_task = me->pr_ConsoleTask;
    previous = SetConsoleTask(probe_port);

    if (previous == old_console_task && me->pr_ConsoleTask == probe_port)
    {
        print("OK: SetConsoleTask() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: SetConsoleTask() returned or stored the wrong port\n");
        errors++;
    }

    SetConsoleTask(old_console_task);
    DeleteMsgPort(probe_port);

    print("\n");
    return errors;
}

int main(void)
{
    int errors = 0;

    print("=== exec/library Test ===\n\n");

    /* Test 1: Test reference counting for all 6 previously-broken libraries */
    errors += test_library_refcount("graphics.library");
    errors += test_library_refcount("intuition.library");
    errors += test_library_refcount("utility.library");
    errors += test_library_refcount("mathtrans.library");
    errors += test_library_refcount("mathffp.library");
    errors += test_library_refcount("expansion.library");

    /* Test 2: OpenLibrary() for non-existent library */
    print("--- Test: Non-existent library ---\n");
    {
        struct Library *fake = OpenLibrary((CONST_STRPTR)"nonexistent.library", 0);
        if (fake != NULL)
        {
            print("FAIL: OpenLibrary(\"nonexistent.library\") should return NULL\n");
            CloseLibrary(fake);
            errors++;
        }
        else
        {
            print("OK: OpenLibrary(\"nonexistent.library\") correctly returned NULL\n");
        }
    }

    /* Test 3: Version check - request version higher than available */
    print("\n--- Test: Version check ---\n");
    {
        struct Library *lib = OpenLibrary((CONST_STRPTR)"graphics.library", 99);
        if (lib != NULL)
        {
            print("FAIL: OpenLibrary(\"graphics.library\", 99) should return NULL (version too high)\n");
            CloseLibrary(lib);
            errors++;
        }
        else
        {
            print("OK: OpenLibrary() correctly returned NULL for too-high version\n");
        }
    }

    /* Test 4: Verify MakeLibrary/SetFunction/SumLibrary/AddLibrary/RemLibrary */
    errors += test_exec_library_helpers();

    /* Test 5: Verify third-party libraries stay off the built-in ROM surface */
    errors += test_external_library_scope();

    /* Test 6: Verify dos.library InitLib initialized the public DOS base state */
    errors += test_dos_library_init_state();

    /* Test 7: Verify LockRecord no longer hits the stub path */
    errors += test_dos_lockrecord_stub_closed();

    /* Test 8: Verify LockRecords no longer hits the stub path */
    errors += test_dos_lockrecords_stub_closed();

    /* Test 9: Verify UnLockRecord no longer hits the stub path */
    errors += test_dos_unlockrecord_stub_closed();

    /* Test 10: Verify UnLockRecords no longer hits the stub path */
    errors += test_dos_unlockrecords_stub_closed();

    /* Test 11: Verify SplitName no longer hits the stub path */
    errors += test_dos_splitname_stub_closed();

    /* Test 12: Verify SetMode no longer hits the stub path */
    errors += test_dos_setmode_stub_closed();

    /* Test 13: Verify ChangeMode no longer hits the stub path */
    errors += test_dos_changemode_stub_closed();

    /* Test 14: Verify ErrorReport no longer hits the stub path */
    errors += test_dos_errorreport_stub_closed();

    /* Test 15: Verify GetConsoleTask no longer hits the stub path */
    errors += test_dos_getconsoletask_stub_closed();

    /* Test 16: Verify SetConsoleTask no longer hits the stub path */
    errors += test_dos_setconsoletask_stub_closed();

    /* ========== Final result ========== */
    print("\n=== Test Results ===\n");
    if (errors == 0)
    {
        print("PASS: All tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: ");
        print_num(errors);
        print(" error(s)\n");
        return 20;
    }
}
