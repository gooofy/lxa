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

#include <string.h>
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <dos/record.h>
#include <graphics/copper.h>
#include <graphics/collide.h>
#include <graphics/gels.h>
#include <graphics/layers.h>
#include <graphics/rastport.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/utility_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/graphics.h>
#include <inline/utility.h>

#undef SetFunction

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static struct UtilityBase *UtilityBase;
extern struct GfxBase *GfxBase;

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

static int test_dos_samedevice_stub_closed(void)
{
    int errors = 0;
    BPTR lock1;
    BPTR lock2;
    BOOL same;

    print("--- Test: DOS SameDevice entry point ---\n");

    lock1 = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
    lock2 = Lock((CONST_STRPTR)"SYS:Tests", SHARED_LOCK);
    if (lock1 == 0 || lock2 == 0)
    {
        print("FAIL: Lock() for SameDevice probe failed\n\n");
        if (lock2)
            UnLock(lock2);
        if (lock1)
            UnLock(lock1);
        return 1;
    }

    same = SameDevice(lock1, lock2);
    if (same == DOSTRUE)
    {
        print("OK: SameDevice() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: SameDevice() unexpectedly failed\n");
        errors++;
    }

    UnLock(lock2);
    UnLock(lock1);

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

static int test_dos_getfilesystask_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct MsgPort *probe_port;
    struct MsgPort *old_filesystem_task;

    print("--- Test: DOS GetFileSysTask entry point ---\n");

    probe_port = CreateMsgPort();
    if (!probe_port)
    {
        print("FAIL: Could not allocate probe port for GetFileSysTask\n\n");
        return 1;
    }

    old_filesystem_task = (struct MsgPort *)me->pr_FileSystemTask;
    me->pr_FileSystemTask = probe_port;

    if (GetFileSysTask() == probe_port)
    {
        print("OK: GetFileSysTask() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: GetFileSysTask() returned the wrong port\n");
        errors++;
    }

    me->pr_FileSystemTask = old_filesystem_task;
    DeleteMsgPort(probe_port);

    print("\n");
    return errors;
}

static int test_dos_setfilesystask_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct MsgPort *probe_port;
    struct MsgPort *old_filesystem_task;
    struct MsgPort *previous;

    print("--- Test: DOS SetFileSysTask entry point ---\n");

    probe_port = CreateMsgPort();
    if (!probe_port)
    {
        print("FAIL: Could not allocate probe port for SetFileSysTask\n\n");
        return 1;
    }

    old_filesystem_task = me->pr_FileSystemTask;
    previous = SetFileSysTask(probe_port);

    if (previous == old_filesystem_task && me->pr_FileSystemTask == probe_port)
    {
        print("OK: SetFileSysTask() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: SetFileSysTask() returned or stored the wrong port\n");
        errors++;
    }

    SetFileSysTask(old_filesystem_task);
    DeleteMsgPort(probe_port);

    print("\n");
    return errors;
}

static int test_dos_getargstr_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    STRPTR old_argstr;
    static char probe_args[] = "exec library arg string";

    print("--- Test: DOS GetArgStr entry point ---\n");

    old_argstr = me->pr_Arguments;
    me->pr_Arguments = probe_args;

    if (GetArgStr() == (STRPTR)probe_args)
    {
        print("OK: GetArgStr() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: GetArgStr() returned the wrong pointer\n");
        errors++;
    }

    me->pr_Arguments = old_argstr;

    print("\n");
    return errors;
}

static int test_dos_cliinitnewcli_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct DosPacket dp = { 0 };
    struct CommandLineInterface *old_cli;
    struct CommandLineInterface *cli;
    BPTR old_dir;
    BPTR restore_dir;
    BPTR new_dir;
    BPTR std_input;
    BPTR std_output;
    BPTR old_cis;
    BPTR old_cos;
    BPTR old_ces;

    print("--- Test: DOS CliInitNewcli entry point ---\n");

    if (!me || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        print("FAIL: Current task is not a process\n\n");
        return 1;
    }

    old_cli = me->pr_CLI ? (struct CommandLineInterface *)BADDR(me->pr_CLI) : NULL;
    old_dir = me->pr_CurrentDir;
    old_cis = me->pr_CIS;
    old_cos = me->pr_COS;
    old_ces = me->pr_CES;
    restore_dir = old_dir ? DupLock(old_dir) : 0;
    new_dir = Lock((CONST_STRPTR)"RAM:", SHARED_LOCK);
    std_input = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    std_output = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);

    if (!new_dir || !std_input || !std_output)
    {
        print("FAIL: Could not allocate CliInitNewcli probe resources\n");
        errors++;
        goto cleanup;
    }

    dp.dp_Res1 = 1;
    dp.dp_Arg1 = new_dir;
    dp.dp_Arg2 = std_input;
    dp.dp_Arg3 = std_output;
    dp.dp_Arg4 = std_input;

    if (CliInitNewcli(&dp) == 0 && IoErr() == 0)
    {
        cli = Cli();
        if (cli && cli->cli_StandardInput == std_input && cli->cli_StandardOutput == std_output)
            print("OK: CliInitNewcli() no longer behaves like a stub\n");
        else
        {
            print("FAIL: CliInitNewcli() did not initialize the CLI as expected\n");
            errors++;
        }
    }
    else
    {
        print("FAIL: CliInitNewcli() unexpectedly failed\n");
        errors++;
    }

cleanup:
    if (restore_dir || old_dir == 0)
        CurrentDir(restore_dir);
    me->pr_CIS = old_cis;
    me->pr_COS = old_cos;
    me->pr_CES = old_ces;

    if (!old_cli && me->pr_CLI)
    {
        FreeDosObject(DOS_CLI, (APTR)BADDR(me->pr_CLI));
        me->pr_CLI = 0;
    }

    if (std_output)
        Close(std_output);
    if (std_input)
        Close(std_input);
    if (new_dir)
        UnLock(new_dir);

    print("\n");
    return errors;
}

static int test_dos_cliinitrun_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct DosPacket dp = { 0 };
    struct CommandLineInterface *old_cli;
    struct CommandLineInterface *cli;
    BPTR old_dir;
    BPTR restore_dir;
    BPTR new_dir;
    BPTR std_input;
    BPTR old_cis;
    BPTR old_cos;
    BPTR old_ces;
    LONG result;

    print("--- Test: DOS CliInitRun entry point ---\n");

    if (!me || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        print("FAIL: Current task is not a process\n\n");
        return 1;
    }

    old_cli = me->pr_CLI ? (struct CommandLineInterface *)BADDR(me->pr_CLI) : NULL;
    old_dir = me->pr_CurrentDir;
    old_cis = me->pr_CIS;
    old_cos = me->pr_COS;
    old_ces = me->pr_CES;
    restore_dir = old_dir ? DupLock(old_dir) : 0;
    new_dir = Lock((CONST_STRPTR)"RAM:", SHARED_LOCK);
    std_input = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);

    if (!new_dir || !std_input)
    {
        print("FAIL: Could not allocate CliInitRun probe resources\n");
        errors++;
        goto cleanup;
    }

    dp.dp_Arg1 = old_cli ? MKBADDR(old_cli) : 0;
    dp.dp_Arg2 = std_input;
    dp.dp_Arg3 = 0;
    dp.dp_Arg4 = std_input;
    dp.dp_Arg5 = new_dir;
    dp.dp_Arg6 = 1;

    result = CliInitRun(&dp);
    if (IoErr() == 0)
    {
        cli = Cli();
        if (cli && cli->cli_StandardInput == std_input && cli->cli_StandardOutput != 0 &&
            (result & ((LONG)1 << 31)) != 0)
            print("OK: CliInitRun() no longer behaves like a stub\n");
        else
        {
            print("FAIL: CliInitRun() did not initialize the CLI as expected\n");
            errors++;
        }
    }
    else
    {
        print("FAIL: CliInitRun() unexpectedly failed\n");
        errors++;
    }

cleanup:
    if (me->pr_CLI)
    {
        cli = (struct CommandLineInterface *)BADDR(me->pr_CLI);
        if (cli && cli->cli_StandardOutput && cli->cli_StandardOutput != old_cos)
            Close(cli->cli_StandardOutput);
    }

    if (restore_dir || old_dir == 0)
        CurrentDir(restore_dir);
    me->pr_CIS = old_cis;
    me->pr_COS = old_cos;
    me->pr_CES = old_ces;

    if (!old_cli && me->pr_CLI)
    {
        FreeDosObject(DOS_CLI, (APTR)BADDR(me->pr_CLI));
        me->pr_CLI = 0;
    }

    if (std_input)
        Close(std_input);
    if (new_dir)
        UnLock(new_dir);

    print("\n");
    return errors;
}

static int test_dos_setargstr_stub_closed(void)
{
    int errors = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    STRPTR old_argstr;
    STRPTR previous;
    static char probe_args[] = "exec library set arg string";

    print("--- Test: DOS SetArgStr entry point ---\n");

    old_argstr = me->pr_Arguments;
    previous = SetArgStr(probe_args);

    if (previous == old_argstr && me->pr_Arguments == (STRPTR)probe_args)
    {
        print("OK: SetArgStr() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: SetArgStr() returned or stored the wrong pointer\n");
        errors++;
    }

    SetArgStr(old_argstr);

    print("\n");
    return errors;
}

static int test_dos_remdosentry_stub_closed(void)
{
    int errors = 0;
    struct RootNode *root;
    struct DosInfo *dos_info;
    BPTR old_head;
    struct DosList *node;

    print("--- Test: DOS RemDosEntry entry point ---\n");

    if (!DOSBase || !DOSBase->dl_Root)
    {
        print("FAIL: DOS root node is not initialized\n\n");
        return 1;
    }

    root = DOSBase->dl_Root;
    dos_info = (struct DosInfo *)BADDR(root->rn_Info);
    if (!dos_info)
    {
        print("FAIL: DOS info is not initialized\n\n");
        return 1;
    }

    node = (struct DosList *)AllocMem(sizeof(*node), MEMF_PUBLIC | MEMF_CLEAR);
    if (!node)
    {
        print("FAIL: Could not allocate probe DosList node for RemDosEntry\n\n");
        return 1;
    }

    old_head = dos_info->di_DevInfo;
    dos_info->di_DevInfo = MKBADDR(node);

    if (RemDosEntry(node) && dos_info->di_DevInfo == 0)
    {
        print("OK: RemDosEntry() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: RemDosEntry() did not unlink the probe node\n");
        errors++;
    }

    dos_info->di_DevInfo = old_head;
    FreeMem(node, sizeof(*node));

    print("\n");
    return errors;
}

static BSTR alloc_bstr(const char *name)
{
    ULONG len = 0;
    UBYTE *buffer;
    ULONG i;

    while (name[len])
        len++;

    buffer = (UBYTE *)AllocMem(len + 2, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buffer)
        return 0;

    buffer[0] = (UBYTE)len;
    for (i = 0; i < len; i++)
        buffer[i + 1] = (UBYTE)name[i];

    return MKBADDR(buffer);
}

static void free_bstr(BSTR bstr)
{
    UBYTE *buffer = (UBYTE *)BADDR(bstr);

    if (buffer)
        FreeMem(buffer, buffer[0] + 2);
}

static int test_dos_adddosentry_stub_closed(void)
{
    int errors = 0;
    struct RootNode *root;
    struct DosInfo *dos_info;
    BPTR old_head;
    struct DosList *node;
    BSTR name;

    print("--- Test: DOS AddDosEntry entry point ---\n");

    if (!DOSBase || !DOSBase->dl_Root)
    {
        print("FAIL: DOS root node is not initialized\n\n");
        return 1;
    }

    root = DOSBase->dl_Root;
    dos_info = (struct DosInfo *)BADDR(root->rn_Info);
    if (!dos_info)
    {
        print("FAIL: DOS info is not initialized\n\n");
        return 1;
    }

    node = (struct DosList *)AllocMem(sizeof(*node), MEMF_PUBLIC | MEMF_CLEAR);
    name = alloc_bstr("EXECADDDOS");
    if (!node || !name)
    {
        print("FAIL: Could not allocate probe DosList node for AddDosEntry\n\n");
        if (name)
            free_bstr(name);
        if (node)
            FreeMem(node, sizeof(*node));
        return 1;
    }

    node->dol_Type = DLT_DEVICE;
    node->dol_Name = name;

    old_head = dos_info->di_DevInfo;
    dos_info->di_DevInfo = 0;

    if (AddDosEntry(node) == DOSTRUE && BADDR(dos_info->di_DevInfo) == node && node->dol_Next == 0)
    {
        print("OK: AddDosEntry() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: AddDosEntry() did not insert the probe node\n");
        errors++;
    }

    dos_info->di_DevInfo = old_head;
    free_bstr(name);
    FreeMem(node, sizeof(*node));

    print("\n");
    return errors;
}

static int test_dos_makedosentry_stub_closed(void)
{
    int errors = 0;
    struct DosList *node;
    UBYTE *name;

    print("--- Test: DOS MakeDosEntry entry point ---\n");

    node = MakeDosEntry((CONST_STRPTR)"EXECMAKEDOS", DLT_DEVICE);
    if (!node)
    {
        print("FAIL: MakeDosEntry() returned NULL\n\n");
        return 1;
    }

    name = (UBYTE *)BADDR(node->dol_Name);
    if (node->dol_Type == DLT_DEVICE && node->dol_Next == 0 && name != NULL &&
        name[0] == 11 && name[1] == 'E' && name[11] == 'S' && name[12] == '\0')
    {
        print("OK: MakeDosEntry() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: MakeDosEntry() did not initialize the probe node correctly\n");
        errors++;
    }

    if (node->dol_Name)
        FreeVec((APTR)BADDR(node->dol_Name));
    FreeVec(node);

    print("\n");
    return errors;
}

static int test_dos_freedosentry_stub_closed(void)
{
    int errors = 0;
    struct DosList *node;

    print("--- Test: DOS FreeDosEntry entry point ---\n");

    node = MakeDosEntry((CONST_STRPTR)"EXECFREEDOS", DLT_DEVICE);
    if (!node)
    {
        print("FAIL: MakeDosEntry() returned NULL for FreeDosEntry probe\n\n");
        return 1;
    }

    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    FreeDosEntry(node);
    if (IoErr() == ERROR_OBJECT_NOT_FOUND)
    {
        print("OK: FreeDosEntry() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: FreeDosEntry() changed IoErr unexpectedly\n");
        errors++;
    }

    FreeDosEntry(NULL);

    print("\n");
    return errors;
}

static int test_dos_format_stub_closed(void)
{
    int errors = 0;
    BOOL ok;

    print("--- Test: DOS Format entry point ---\n");

    ok = Format((CONST_STRPTR)"HOME:", (CONST_STRPTR)"LIBFORMAT", ID_DOS_DISK);
    if (ok == DOSFALSE && IoErr() == ERROR_ACTION_NOT_KNOWN)
    {
        print("OK: Format() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: Format() did not fail through the public path as expected\n");
        errors++;
    }

    print("\n");
    return errors;
}

static int test_dos_relabel_stub_closed(void)
{
    int errors = 0;
    BOOL ok;

    print("--- Test: DOS Relabel entry point ---\n");

    ok = Relabel((CONST_STRPTR)"HOME:", (CONST_STRPTR)"LIBRELABEL");
    if (ok == DOSFALSE && IoErr() == ERROR_ACTION_NOT_KNOWN)
    {
        print("OK: Relabel() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: Relabel() did not fail through the public path as expected\n");
        errors++;
    }

    print("\n");
    return errors;
}

static int test_dos_inhibit_stub_closed(void)
{
    int errors = 0;
    BOOL ok;

    print("--- Test: DOS Inhibit entry point ---\n");

    ok = Inhibit((CONST_STRPTR)"HOME:", DOSTRUE);
    if (ok == DOSFALSE && IoErr() == ERROR_ACTION_NOT_KNOWN)
    {
        print("OK: Inhibit() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: Inhibit() did not fail through the public path as expected\n");
        errors++;
    }

    print("\n");
    return errors;
}

static int test_dos_addbuffers_stub_closed(void)
{
    int errors = 0;
    LONG ok;

    print("--- Test: DOS AddBuffers entry point ---\n");

    ok = AddBuffers((CONST_STRPTR)"HOME:", 1);
    if (ok == DOSFALSE && IoErr() == ERROR_ACTION_NOT_KNOWN)
    {
        print("OK: AddBuffers() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: AddBuffers() did not fail through the public path as expected\n");
        errors++;
    }

    print("\n");
    return errors;
}

static int test_dos_setowner_stub_closed(void)
{
    int errors = 0;
    BOOL ok;
    BPTR fh;

    print("--- Test: DOS SetOwner entry point ---\n");

    fh = Open((CONST_STRPTR)"T:lib_setowner_probe", MODE_NEWFILE);
    if (!fh)
    {
        print("FAIL: Open() for SetOwner probe failed\n\n");
        return 1;
    }
    Close(fh);

    ok = SetOwner((CONST_STRPTR)"T:lib_setowner_probe", 0x12345678);
    if (ok == DOSTRUE)
    {
        print("OK: SetOwner() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: SetOwner() unexpectedly failed\n");
        errors++;
    }

    DeleteFile((CONST_STRPTR)"T:lib_setowner_probe");

    print("\n");
    return errors;
}

static int test_dos_addsegment_stub_closed(void)
{
    int errors = 0;
    BPTR seg;
    LONG ok;
    struct RootNode *root;
    struct DosInfo *dos_info;
    BPTR old_head;
    struct Segment *entry;

    print("--- Test: DOS AddSegment entry point ---\n");

    seg = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
    if (!seg)
    {
        print("FAIL: LoadSeg() returned NULL for AddSegment probe\n\n");
        return 1;
    }

    root = DOSBase ? DOSBase->dl_Root : NULL;
    dos_info = root ? (struct DosInfo *)BADDR(root->rn_Info) : NULL;
    if (!dos_info)
    {
        print("FAIL: DOS info is not initialized for AddSegment probe\n\n");
        UnLoadSeg(seg);
        return 1;
    }

    old_head = dos_info->di_ResList;
    ok = AddSegment((CONST_STRPTR)"EXECADDSEG", seg, 0);
    entry = (struct Segment *)BADDR(dos_info->di_ResList);
    if (ok == DOSTRUE && IoErr() == 0 && entry && entry->seg_Seg == seg && entry->seg_Next == old_head)
    {
        print("OK: AddSegment() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: AddSegment() did not insert the resident segment as expected\n");
        errors++;
    }

    if (entry && entry->seg_Seg == seg)
    {
        dos_info->di_ResList = entry->seg_Next;
        UnLoadSeg(seg);
        FreeVec(entry);
    }
    else
    {
        UnLoadSeg(seg);
    }

    print("\n");
    return errors;
}

static int test_dos_readitem_stub_closed(void)
{
    int errors = 0;
    struct CSource source;
    LONG result;
    char buffer[16];

    print("--- Test: DOS ReadItem entry point ---\n");

    source.CS_Buffer = (STRPTR)"alpha beta\n";
    source.CS_Length = 11;
    source.CS_CurChr = 0;
    result = ReadItem((CONST_STRPTR)buffer, sizeof(buffer), &source);
    if (result == ITEM_UNQUOTED && buffer[0] == 'a' && buffer[1] == 'l' && buffer[2] == 'p' && buffer[3] == 'h' && buffer[4] == 'a' && buffer[5] == '\0')
    {
        print("OK: ReadItem() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: ReadItem() unexpectedly failed\n");
        errors++;
    }

    print("\n");
    return errors;
}

static int test_utility_packbooltags_stub_closed(void)
{
    int errors = 0;
    struct TagItem bool_map[] = {
        { TAG_USER + 0x100, 0x01 },
        { TAG_USER + 0x101, 0x02 },
        { TAG_DONE, 0 }
    };
    struct TagItem tags[] = {
        { TAG_USER + 0x100, 0 },
        { TAG_USER + 0x101, 1 },
        { TAG_DONE, 0 }
    };
    ULONG flags;

    print("--- Test: utility PackBoolTags entry point ---\n");

    UtilityBase = (struct UtilityBase *)OpenLibrary((CONST_STRPTR)"utility.library", 36);
    if (UtilityBase == NULL)
    {
        print("FAIL: OpenLibrary() for utility.library returned NULL\n\n");
        return 1;
    }

    flags = PackBoolTags(0x01, tags, bool_map);
    if (flags == 0x02)
    {
        print("OK: PackBoolTags() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: PackBoolTags() returned unexpected flags\n");
        errors++;
    }

    CloseLibrary((struct Library *)UtilityBase);
    UtilityBase = NULL;

    print("\n");
    return errors;
}

static int test_utility_filtertagchanges_stub_closed(void)
{
    int errors = 0;
    struct TagItem original[] = {
        { TAG_USER + 0x100, 1 },
        { TAG_USER + 0x101, 2 },
        { TAG_DONE, 0 }
    };
    struct TagItem changes[] = {
        { TAG_USER + 0x100, 1 },
        { TAG_USER + 0x101, 9 },
        { TAG_DONE, 0 }
    };

    print("--- Test: utility FilterTagChanges entry point ---\n");

    UtilityBase = (struct UtilityBase *)OpenLibrary((CONST_STRPTR)"utility.library", 36);
    if (UtilityBase == NULL)
    {
        print("FAIL: OpenLibrary() for utility.library returned NULL\n\n");
        return 1;
    }

    FilterTagChanges(changes, original, 1);
    if (changes[0].ti_Tag == TAG_IGNORE && changes[1].ti_Tag == TAG_USER + 0x101 && original[1].ti_Data == 9)
    {
        print("OK: FilterTagChanges() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: FilterTagChanges() returned unexpected results\n");
        errors++;
    }

    CloseLibrary((struct Library *)UtilityBase);
    UtilityBase = NULL;

    print("\n");
    return errors;
}

static int test_utility_applytagchanges_stub_closed(void)
{
    int errors = 0;
    struct TagItem changes[] = {
        { TAG_USER + 0x100, 7 },
        { TAG_USER + 0x101, 9 },
        { TAG_DONE, 0 }
    };
    struct TagItem list[] = {
        { TAG_USER + 0x100, 1 },
        { TAG_USER + 0x101, 2 },
        { TAG_DONE, 0 }
    };

    print("--- Test: utility ApplyTagChanges entry point ---\n");

    UtilityBase = (struct UtilityBase *)OpenLibrary((CONST_STRPTR)"utility.library", 36);
    if (UtilityBase == NULL)
    {
        print("FAIL: OpenLibrary() for utility.library returned NULL\n\n");
        return 1;
    }

    ApplyTagChanges(list, changes);
    if (list[0].ti_Data == 7 && list[1].ti_Data == 9)
    {
        print("OK: ApplyTagChanges() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: ApplyTagChanges() returned unexpected results\n");
        errors++;
    }

    CloseLibrary((struct Library *)UtilityBase);
    UtilityBase = NULL;

    print("\n");
    return errors;
}

static LONG exec_boundary_collision(struct VSprite *vs, WORD hits)
{
    (void)vs;
    return hits;
}

static WORD exec_animob_routine_calls;
static WORD exec_animcomp_timeout_calls;
static WORD exec_animcomp_static_calls;
static struct AnimOb *exec_last_animob;
static struct AnimComp *exec_last_timeout_comp;
static struct AnimComp *exec_last_static_comp;

static void init_exec_test_vsprite(struct VSprite *vs,
                                   WORD x,
                                   WORD y,
                                   WORD width_words,
                                   WORD height,
                                   WORD depth,
                                   WORD *image_data,
                                   WORD flags)
{
    vs->NextVSprite = NULL;
    vs->PrevVSprite = NULL;
    vs->DrawPath = NULL;
    vs->ClearPath = NULL;
    vs->OldY = 0;
    vs->OldX = 0;
    vs->Flags = flags;
    vs->Y = y;
    vs->X = x;
    vs->Height = height;
    vs->Width = width_words;
    vs->Depth = depth;
    vs->MeMask = 0;
    vs->HitMask = 0;
    vs->ImageData = image_data;
    vs->BorderLine = NULL;
    vs->CollMask = NULL;
    vs->SprColors = NULL;
    vs->VSBob = NULL;
    vs->PlanePick = 1;
    vs->PlaneOnOff = 0;
    vs->VUserExt = 0;
}

static WORD exec_animob_routine(struct AnimOb *anOb)
{
    exec_animob_routine_calls++;
    exec_last_animob = anOb;
    return 0;
}

static WORD exec_animcomp_timeout_routine(struct AnimComp *anComp)
{
    exec_animcomp_timeout_calls++;
    exec_last_timeout_comp = anComp;
    return 0;
}

static WORD exec_animcomp_static_routine(struct AnimComp *anComp)
{
    exec_animcomp_static_calls++;
    exec_last_static_comp = anComp;
    return 0;
}

static void exec_free_ucoplist_chain(struct UCopList *ucl)
{
    struct CopList *cop_list;

    if (!ucl)
        return;

    cop_list = ucl->FirstCopList;
    while (cop_list)
    {
        struct CopList *next = cop_list->Next;

        if (cop_list->CopIns)
            FreeMem(cop_list->CopIns, cop_list->MaxCount * sizeof(struct CopIns));

        FreeMem(cop_list, sizeof(struct CopList));
        cop_list = next;
    }

    ucl->FirstCopList = NULL;
    ucl->CopList = NULL;
}

static int test_graphics_addanimob_stub_closed(void)
{
    int errors = 0;
    struct VSprite head;
    struct VSprite tail;
    struct GelsInfo gels_info;
    struct RastPort rp;
    struct VSprite anim_vs_a;
    struct VSprite anim_vs_b;
    struct Bob anim_bob_a;
    struct Bob anim_bob_b;
    struct AnimComp anim_comp_a;
    struct AnimComp anim_comp_b;
    struct AnimOb anim_first;
    struct AnimOb anim_second;
    struct AnimOb *anim_key = NULL;
    WORD image_data[1] = { 0x8000 };

    print("--- Test: graphics AddAnimOb entry point ---\n");

    InitGels(&head, &tail, &gels_info);
    InitRastPort(&rp);
    rp.GelsInfo = &gels_info;

    anim_vs_a.NextVSprite = NULL;
    anim_vs_a.PrevVSprite = NULL;
    anim_vs_a.DrawPath = NULL;
    anim_vs_a.ClearPath = NULL;
    anim_vs_a.OldY = 0;
    anim_vs_a.OldX = 0;
    anim_vs_a.Flags = 0;
    anim_vs_a.Y = 4;
    anim_vs_a.X = 4;
    anim_vs_a.Height = 1;
    anim_vs_a.Width = 1;
    anim_vs_a.Depth = 1;
    anim_vs_a.MeMask = 0;
    anim_vs_a.HitMask = 0;
    anim_vs_a.ImageData = image_data;
    anim_vs_a.BorderLine = NULL;
    anim_vs_a.CollMask = NULL;
    anim_vs_a.SprColors = NULL;
    anim_vs_a.VSBob = NULL;
    anim_vs_a.PlanePick = 1;
    anim_vs_a.PlaneOnOff = 0;
    anim_vs_a.VUserExt = 0;

    anim_vs_b = anim_vs_a;
    anim_vs_b.X = 6;
    anim_vs_b.Y = 6;

    anim_bob_a.Flags = 0;
    anim_bob_a.SaveBuffer = NULL;
    anim_bob_a.ImageShadow = NULL;
    anim_bob_a.Before = NULL;
    anim_bob_a.After = NULL;
    anim_bob_a.BobVSprite = &anim_vs_a;
    anim_bob_a.BobComp = &anim_comp_a;
    anim_bob_a.DBuffer = NULL;
    anim_bob_a.BUserExt = 0;

    anim_bob_b.Flags = 0;
    anim_bob_b.SaveBuffer = NULL;
    anim_bob_b.ImageShadow = NULL;
    anim_bob_b.Before = NULL;
    anim_bob_b.After = NULL;
    anim_bob_b.BobVSprite = &anim_vs_b;
    anim_bob_b.BobComp = &anim_comp_b;
    anim_bob_b.DBuffer = NULL;
    anim_bob_b.BUserExt = 0;

    anim_comp_a.Flags = 0;
    anim_comp_a.Timer = 0;
    anim_comp_a.TimeSet = 5;
    anim_comp_a.NextComp = &anim_comp_b;
    anim_comp_a.PrevComp = NULL;
    anim_comp_a.NextSeq = &anim_comp_a;
    anim_comp_a.PrevSeq = &anim_comp_a;
    anim_comp_a.AnimCRoutine = NULL;
    anim_comp_a.YTrans = 0;
    anim_comp_a.XTrans = 0;
    anim_comp_a.HeadOb = &anim_first;
    anim_comp_a.AnimBob = &anim_bob_a;

    anim_comp_b.Flags = 0;
    anim_comp_b.Timer = 0;
    anim_comp_b.TimeSet = 9;
    anim_comp_b.NextComp = NULL;
    anim_comp_b.PrevComp = &anim_comp_a;
    anim_comp_b.NextSeq = &anim_comp_b;
    anim_comp_b.PrevSeq = &anim_comp_b;
    anim_comp_b.AnimCRoutine = NULL;
    anim_comp_b.YTrans = 0;
    anim_comp_b.XTrans = 0;
    anim_comp_b.HeadOb = &anim_first;
    anim_comp_b.AnimBob = &anim_bob_b;

    anim_first.NextOb = (struct AnimOb *)0x1;
    anim_first.PrevOb = (struct AnimOb *)0x1;
    anim_first.Clock = 0;
    anim_first.AnOldY = 0;
    anim_first.AnOldX = 0;
    anim_first.AnY = 0;
    anim_first.AnX = 0;
    anim_first.YVel = 0;
    anim_first.XVel = 0;
    anim_first.YAccel = 0;
    anim_first.XAccel = 0;
    anim_first.RingYTrans = 0;
    anim_first.RingXTrans = 0;
    anim_first.AnimORoutine = NULL;
    anim_first.HeadComp = &anim_comp_a;
    anim_first.AUserExt = 0;

    anim_second = anim_first;
    anim_second.HeadComp = NULL;

    AddAnimOb(&anim_first, &anim_key, &rp);
    AddAnimOb(&anim_second, &anim_key, &rp);

    if (anim_key != &anim_second || anim_second.NextOb != &anim_first ||
        anim_second.PrevOb != NULL || anim_first.PrevOb != &anim_second ||
        anim_first.NextOb != NULL || anim_comp_a.Timer != 5 ||
        anim_comp_b.Timer != 9 || (anim_bob_a.Flags & BWAITING) == 0 ||
        (anim_bob_b.Flags & BWAITING) == 0)
    {
        print("FAIL: AddAnimOb() did not link or initialize the AnimOb list\n");
        errors++;
    }
    else
    {
        print("OK: AddAnimOb() no longer behaves like a stub\n");
    }

    print("\n");
    return errors;
}

static int test_graphics_remibob_stub_closed(void)
{
    int errors = 0;
    struct VSprite head;
    struct VSprite tail;
    struct VSprite base_vs;
    struct VSprite cover_vs;
    struct GelsInfo gels_info;
    struct RastPort rp;
    struct Bob base_bob;
    struct Bob cover_bob;
    struct BitMap *bm;
    WORD image_data[1] = { 0x8000 };

    print("--- Test: graphics RemIBob entry point ---\n");

    bm = AllocBitMap(32, 16, 1, BMF_CLEAR, NULL);
    if (!bm)
    {
        print("FAIL: AllocBitMap() returned NULL\n");
        return 1;
    }

    InitGels(&head, &tail, &gels_info);
    InitRastPort(&rp);
    rp.BitMap = bm;
    rp.GelsInfo = &gels_info;
    SetAPen(&rp, 1);

    base_vs.NextVSprite = NULL;
    base_vs.PrevVSprite = NULL;
    base_vs.DrawPath = NULL;
    base_vs.ClearPath = NULL;
    base_vs.OldY = 0;
    base_vs.OldX = 0;
    base_vs.Flags = 0;
    base_vs.Y = 4;
    base_vs.X = 4;
    base_vs.Height = 1;
    base_vs.Width = 1;
    base_vs.Depth = 1;
    base_vs.MeMask = 0;
    base_vs.HitMask = 0;
    base_vs.ImageData = image_data;
    base_vs.BorderLine = NULL;
    base_vs.CollMask = NULL;
    base_vs.SprColors = NULL;
    base_vs.VSBob = NULL;
    base_vs.PlanePick = 1;
    base_vs.PlaneOnOff = 0;
    base_vs.VUserExt = 0;

    cover_vs = base_vs;
    cover_vs.X = 5;

    base_bob.Flags = 0;
    base_bob.SaveBuffer = NULL;
    base_bob.ImageShadow = NULL;
    base_bob.Before = NULL;
    base_bob.After = NULL;
    base_bob.BobVSprite = &base_vs;
    base_bob.BobComp = NULL;
    base_bob.DBuffer = NULL;
    base_bob.BUserExt = 0;

    cover_bob.Flags = 0;
    cover_bob.SaveBuffer = NULL;
    cover_bob.ImageShadow = NULL;
    cover_bob.Before = NULL;
    cover_bob.After = NULL;
    cover_bob.BobVSprite = &cover_vs;
    cover_bob.BobComp = NULL;
    cover_bob.DBuffer = NULL;
    cover_bob.BUserExt = 0;

    AddBob(&base_bob, &rp);
    AddBob(&cover_bob, &rp);
    DrawGList(&rp, NULL);
    RemIBob(&base_bob, &rp, NULL);

    if ((base_bob.Flags & BOBNIX) == 0 || (cover_bob.Flags & BOBNIX) == 0 ||
        head.NextVSprite != &cover_vs || ReadPixel(&rp, 4, 4) != 0 || ReadPixel(&rp, 5, 4) != 0)
    {
        print("FAIL: RemIBob() did not immediately remove and clear the Bob\n");
        errors++;
    }
    else
    {
        print("OK: RemIBob() no longer behaves like a stub\n");
    }

    FreeBitMap(bm);

    print("\n");
    return errors;
}

static int test_graphics_docollision_stub_closed(void)
{
    int errors = 0;
    struct VSprite head;
    struct VSprite tail;
    struct VSprite probe;
    struct GelsInfo gels_info;
    struct collTable coll_table;
    struct RastPort rp;
    WORD image_data[2] = { 0x8000, 0x0000 };
    WORD coll_mask[2] = { 0, 0 };
    WORD border_line[1] = { 0 };

    print("--- Test: graphics DoCollision entry point ---\n");

    InitGels(&head, &tail, &gels_info);
    InitRastPort(&rp);
    rp.GelsInfo = &gels_info;

    probe.NextVSprite = NULL;
    probe.PrevVSprite = NULL;
    probe.DrawPath = NULL;
    probe.ClearPath = NULL;
    probe.OldY = 0;
    probe.OldX = 0;
    probe.Flags = VSPRITE;
    probe.Y = 4;
    probe.X = 4;
    probe.Height = 2;
    probe.Width = 1;
    probe.Depth = 1;
    probe.MeMask = 0;
    probe.HitMask = 1 << BORDERHIT;
    probe.ImageData = image_data;
    probe.BorderLine = border_line;
    probe.CollMask = coll_mask;
    probe.SprColors = NULL;
    probe.VSBob = NULL;
    probe.PlanePick = 1;
    probe.PlaneOnOff = 0;
    probe.VUserExt = 0;

    coll_table.collPtrs[0] = (LONG (*)(struct VSprite *, struct VSprite *))exec_boundary_collision;
    gels_info.collHandler = &coll_table;
    gels_info.leftmost = 5;
    gels_info.rightmost = 20;
    gels_info.topmost = 5;
    gels_info.bottommost = 20;

    InitMasks(&probe);
    AddVSprite(&probe, &rp);
    DoCollision(&rp);

    if ((UWORD)probe.BorderLine[0] == 0x8000U &&
        (UWORD)probe.CollMask[0] == 0x8000U &&
        (UWORD)probe.CollMask[1] == 0x0000U)
    {
        print("OK: DoCollision() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: DoCollision()/InitMasks() returned unexpected results\n");
        errors++;
    }

    coll_table.collPtrs[0] = NULL;
    coll_table.collPtrs[15] = NULL;
    SetCollision(BORDERHIT, (VOID (*)())exec_boundary_collision, &gels_info);
    SetCollision(15, (VOID (*)())exec_boundary_collision, &gels_info);
    if ((APTR)gels_info.collHandler->collPtrs[BORDERHIT] == (APTR)exec_boundary_collision &&
        (APTR)gels_info.collHandler->collPtrs[15] == (APTR)exec_boundary_collision)
    {
        print("OK: SetCollision() no longer behaves like a stub\n");
    }
    else
    {
        print("FAIL: SetCollision() did not store callback\n");
        errors++;
    }

    print("\n");
    return errors;
}

static int test_graphics_animate_stub_closed(void)
{
    int errors = 0;
    struct VSprite head;
    struct VSprite tail;
    struct VSprite seq_vs;
    struct VSprite next_vs;
    struct VSprite static_vs;
    struct GelsInfo gels_info;
    struct RastPort rp;
    struct Bob seq_bob;
    struct Bob next_bob;
    struct Bob static_bob;
    struct AnimComp seq_comp;
    struct AnimComp next_seq_comp;
    struct AnimComp static_comp;
    struct AnimOb anim_ob;
    struct AnimOb *anim_key = NULL;
    WORD image_data[1] = { 0x8000 };

    print("--- Test: graphics Animate entry point ---\n");

    InitGels(&head, &tail, &gels_info);
    InitRastPort(&rp);
    rp.GelsInfo = &gels_info;

    exec_animob_routine_calls = 0;
    exec_animcomp_timeout_calls = 0;
    exec_animcomp_static_calls = 0;
    exec_last_animob = NULL;
    exec_last_timeout_comp = NULL;
    exec_last_static_comp = NULL;

    seq_vs.NextVSprite = NULL;
    seq_vs.PrevVSprite = NULL;
    seq_vs.DrawPath = NULL;
    seq_vs.ClearPath = NULL;
    seq_vs.OldY = 0;
    seq_vs.OldX = 0;
    seq_vs.Flags = 0;
    seq_vs.Y = 18;
    seq_vs.X = 20;
    seq_vs.Height = 1;
    seq_vs.Width = 1;
    seq_vs.Depth = 1;
    seq_vs.MeMask = 0;
    seq_vs.HitMask = 0;
    seq_vs.ImageData = image_data;
    seq_vs.BorderLine = NULL;
    seq_vs.CollMask = NULL;
    seq_vs.SprColors = NULL;
    seq_vs.VSBob = NULL;
    seq_vs.PlanePick = 1;
    seq_vs.PlaneOnOff = 0;
    seq_vs.VUserExt = 0;

    next_vs = seq_vs;
    next_vs.X = 2;
    next_vs.Y = 2;

    static_vs = seq_vs;
    static_vs.X = 6;
    static_vs.Y = 6;

    seq_bob.Flags = 0;
    seq_bob.SaveBuffer = NULL;
    seq_bob.ImageShadow = NULL;
    seq_bob.Before = NULL;
    seq_bob.After = NULL;
    seq_bob.BobVSprite = &seq_vs;
    seq_bob.BobComp = &seq_comp;
    seq_bob.DBuffer = NULL;
    seq_bob.BUserExt = 0;

    next_bob.Flags = 0;
    next_bob.SaveBuffer = NULL;
    next_bob.ImageShadow = NULL;
    next_bob.Before = NULL;
    next_bob.After = NULL;
    next_bob.BobVSprite = &next_vs;
    next_bob.BobComp = &next_seq_comp;
    next_bob.DBuffer = NULL;
    next_bob.BUserExt = 0;

    static_bob.Flags = 0;
    static_bob.SaveBuffer = NULL;
    static_bob.ImageShadow = NULL;
    static_bob.Before = NULL;
    static_bob.After = NULL;
    static_bob.BobVSprite = &static_vs;
    static_bob.BobComp = &static_comp;
    static_bob.DBuffer = NULL;
    static_bob.BUserExt = 0;

    seq_comp.Flags = RINGTRIGGER;
    seq_comp.Timer = 1;
    seq_comp.TimeSet = 5;
    seq_comp.NextComp = &static_comp;
    seq_comp.PrevComp = NULL;
    seq_comp.NextSeq = &next_seq_comp;
    seq_comp.PrevSeq = &next_seq_comp;
    seq_comp.AnimCRoutine = exec_animcomp_timeout_routine;
    seq_comp.YTrans = 0;
    seq_comp.XTrans = 0;
    seq_comp.HeadOb = &anim_ob;
    seq_comp.AnimBob = &seq_bob;

    next_seq_comp.Flags = 0;
    next_seq_comp.Timer = 0;
    next_seq_comp.TimeSet = 4;
    next_seq_comp.NextComp = NULL;
    next_seq_comp.PrevComp = NULL;
    next_seq_comp.NextSeq = &seq_comp;
    next_seq_comp.PrevSeq = &seq_comp;
    next_seq_comp.AnimCRoutine = NULL;
    next_seq_comp.YTrans = 128;
    next_seq_comp.XTrans = 64;
    next_seq_comp.HeadOb = &anim_ob;
    next_seq_comp.AnimBob = &next_bob;

    static_comp.Flags = 0;
    static_comp.Timer = 5;
    static_comp.TimeSet = 5;
    static_comp.NextComp = NULL;
    static_comp.PrevComp = &seq_comp;
    static_comp.NextSeq = &static_comp;
    static_comp.PrevSeq = &static_comp;
    static_comp.AnimCRoutine = exec_animcomp_static_routine;
    static_comp.YTrans = 64;
    static_comp.XTrans = 0;
    static_comp.HeadOb = &anim_ob;
    static_comp.AnimBob = &static_bob;

    anim_ob.NextOb = NULL;
    anim_ob.PrevOb = NULL;
    anim_ob.Clock = 7;
    anim_ob.AnOldY = -1;
    anim_ob.AnOldX = -1;
    anim_ob.AnY = 0;
    anim_ob.AnX = 0;
    anim_ob.YVel = 64;
    anim_ob.XVel = 64;
    anim_ob.YAccel = 0;
    anim_ob.XAccel = 0;
    anim_ob.RingYTrans = 64;
    anim_ob.RingXTrans = 64;
    anim_ob.AnimORoutine = exec_animob_routine;
    anim_ob.HeadComp = &seq_comp;
    anim_ob.AUserExt = 0;
    anim_key = &anim_ob;

    AddBob(&seq_bob, &rp);
    AddBob(&static_bob, &rp);
    Animate(&anim_key, &rp);

    if (anim_ob.Clock != 8 || anim_ob.AnOldX != 0 || anim_ob.AnOldY != 0 ||
        anim_ob.AnX != 128 || anim_ob.AnY != 128 || anim_ob.HeadComp != &next_seq_comp ||
        next_seq_comp.NextComp != &static_comp || next_seq_comp.PrevComp != NULL ||
        static_comp.PrevComp != &next_seq_comp || next_seq_comp.Timer != 4 ||
        (seq_bob.Flags & BOBSAWAY) == 0 || (next_bob.Flags & BWAITING) == 0 ||
        (UWORD)seq_vs.X != 0x8001 || (UWORD)seq_vs.Y != 0x8001 || next_vs.OldX != 20 ||
        next_vs.OldY != 18 || next_vs.X != 3 || next_vs.Y != 4 || static_vs.X != 2 ||
        static_vs.Y != 3 || exec_animob_routine_calls != 1 || exec_last_animob != &anim_ob ||
        exec_animcomp_timeout_calls != 1 || exec_last_timeout_comp != &seq_comp ||
        exec_animcomp_static_calls != 1 || exec_last_static_comp != &static_comp)
    {
        print("FAIL: Animate() did not update the AnimOb/component state\n");
        errors++;
    }
    else
    {
        print("OK: Animate() no longer behaves like a stub\n");
    }

    print("\n");
    return errors;
}

static int test_graphics_getgbuffers_stub_closed(void)
{
    int errors = 0;
    struct VSprite buf_vs_a;
    struct VSprite buf_vs_b;
    struct VSprite buf_vs_c;
    struct RastPort rp;
    struct Bob buf_bob_a;
    struct Bob buf_bob_b;
    struct Bob buf_bob_c;
    struct AnimComp buf_comp_a;
    struct AnimComp buf_comp_b;
    struct AnimComp buf_comp_c;
    struct AnimComp buf_seq_a;
    struct AnimOb buf_anim;
    WORD image_data[4] = { 0x8000, 0x2000, 0x4000, 0x1000 };
    WORD image_data_alt[2] = { 0x1111, 0x2222 };
    WORD image_data_third[4] = { 0x0f00, 0x00f0, 0x3000, 0x0003 };
    WORD collmask_a[2] = { 0, 0 };
    WORD collmask_b[2] = { 0, 0 };
    WORD collmask_c[2] = { 0, 0 };
    WORD borderline_a[1] = { 0 };
    WORD borderline_b[1] = { 0 };
    WORD borderline_c[1] = { 0 };

    print("--- Test: graphics GetGBuffers/FreeGBuffers entry points ---\n");

    InitRastPort(&rp);
    init_exec_test_vsprite(&buf_vs_a, 0, 0, 1, 2, 2, image_data, 0);
    init_exec_test_vsprite(&buf_vs_b, 0, 0, 1, 2, 2, image_data, 0);
    init_exec_test_vsprite(&buf_vs_c, 0, 0, 1, 2, 2, image_data_third, 0);

    buf_bob_a.Flags = 0;
    buf_bob_a.SaveBuffer = NULL;
    buf_bob_a.ImageShadow = NULL;
    buf_bob_a.Before = NULL;
    buf_bob_a.After = NULL;
    buf_bob_a.BobVSprite = &buf_vs_a;
    buf_bob_a.BobComp = &buf_comp_a;
    buf_bob_a.DBuffer = NULL;
    buf_bob_a.BUserExt = 0;

    buf_bob_b = buf_bob_a;
    buf_bob_b.BobVSprite = &buf_vs_b;
    buf_bob_b.BobComp = &buf_seq_a;

    buf_bob_c = buf_bob_a;
    buf_bob_c.BobVSprite = &buf_vs_c;
    buf_bob_c.BobComp = &buf_comp_b;

    buf_comp_a.Flags = 0;
    buf_comp_a.Timer = 0;
    buf_comp_a.TimeSet = 1;
    buf_comp_a.NextComp = &buf_comp_b;
    buf_comp_a.PrevComp = NULL;
    buf_comp_a.NextSeq = &buf_seq_a;
    buf_comp_a.PrevSeq = &buf_seq_a;
    buf_comp_a.AnimCRoutine = NULL;
    buf_comp_a.YTrans = 0;
    buf_comp_a.XTrans = 0;
    buf_comp_a.HeadOb = &buf_anim;
    buf_comp_a.AnimBob = &buf_bob_a;

    buf_seq_a = buf_comp_a;
    buf_seq_a.NextComp = NULL;
    buf_seq_a.PrevComp = NULL;
    buf_seq_a.NextSeq = &buf_comp_a;
    buf_seq_a.PrevSeq = &buf_comp_a;
    buf_seq_a.AnimBob = &buf_bob_b;

    buf_comp_b = buf_comp_a;
    buf_comp_b.NextComp = NULL;
    buf_comp_b.PrevComp = &buf_comp_a;
    buf_comp_b.NextSeq = &buf_comp_b;
    buf_comp_b.PrevSeq = &buf_comp_b;
    buf_comp_b.AnimBob = &buf_bob_c;

    buf_anim.NextOb = NULL;
    buf_anim.PrevOb = NULL;
    buf_anim.Clock = 0;
    buf_anim.AnOldY = 0;
    buf_anim.AnOldX = 0;
    buf_anim.AnY = 0;
    buf_anim.AnX = 0;
    buf_anim.YVel = 0;
    buf_anim.XVel = 0;
    buf_anim.YAccel = 0;
    buf_anim.XAccel = 0;
    buf_anim.RingYTrans = 0;
    buf_anim.RingXTrans = 0;
    buf_anim.AnimORoutine = NULL;
    buf_anim.HeadComp = &buf_comp_a;
    buf_anim.AUserExt = 0;

    buf_comp_c = buf_comp_b;

    buf_vs_a.CollMask = collmask_a;
    buf_vs_a.BorderLine = borderline_a;
    buf_vs_b.Depth = 1;
    buf_vs_b.ImageData = image_data_alt;
    buf_vs_b.CollMask = collmask_b;
    buf_vs_b.BorderLine = borderline_b;
    buf_vs_c.CollMask = collmask_c;
    buf_vs_c.BorderLine = borderline_c;

    buf_bob_c.BobComp = &buf_comp_c;
    buf_comp_c.AnimBob = &buf_bob_c;

    InitGMasks(&buf_anim);
    if ((UWORD)buf_vs_a.CollMask[0] != 0xc000U || (UWORD)buf_vs_a.CollMask[1] != 0x3000U ||
        (UWORD)buf_vs_a.BorderLine[0] != 0xf000U || (UWORD)buf_vs_b.CollMask[0] != 0x1111U ||
        (UWORD)buf_vs_b.CollMask[1] != 0x2222U || (UWORD)buf_vs_b.BorderLine[0] != 0x3333U ||
        (UWORD)buf_vs_c.CollMask[0] != 0x3f00U || (UWORD)buf_vs_c.CollMask[1] != 0x00f3U ||
        (UWORD)buf_vs_c.BorderLine[0] != 0x3ff3U)
    {
        print("FAIL: InitGMasks() did not initialize all component sequences\n");
        errors++;
    }
    else
    {
        print("OK: InitGMasks() no longer behaves like a stub\n");
    }

    if (!GetGBuffers(&buf_anim, &rp, TRUE) ||
        !buf_bob_a.ImageShadow || buf_vs_a.CollMask != buf_bob_a.ImageShadow ||
        !buf_bob_a.SaveBuffer || !buf_vs_a.BorderLine || !buf_bob_a.DBuffer ||
        !buf_bob_a.DBuffer->BufBuffer || !buf_bob_b.ImageShadow ||
        buf_vs_b.CollMask != buf_bob_b.ImageShadow || !buf_bob_b.SaveBuffer ||
        !buf_vs_b.BorderLine || !buf_bob_b.DBuffer || !buf_bob_b.DBuffer->BufBuffer ||
        !buf_bob_c.ImageShadow || buf_vs_c.CollMask != buf_bob_c.ImageShadow ||
        !buf_bob_c.SaveBuffer || !buf_vs_c.BorderLine || !buf_bob_c.DBuffer ||
        !buf_bob_c.DBuffer->BufBuffer)
    {
        print("FAIL: GetGBuffers() did not allocate the expected buffers\n");
        errors++;
    }
    else
    {
        print("OK: GetGBuffers() no longer behaves like a stub\n");
    }

    if (buf_bob_a.ImageShadow)
    {
        WORD *separate_mask = (WORD *)AllocMem(4, MEMF_CHIP | MEMF_CLEAR);

        if (!separate_mask)
        {
            print("FAIL: Could not allocate separate CollMask for FreeGBuffers() test\n");
            errors++;
            FreeGBuffers(&buf_anim, &rp, TRUE);
        }
        else
        {
            buf_vs_b.CollMask = separate_mask;
            FreeGBuffers(&buf_anim, &rp, TRUE);

            if (buf_bob_a.ImageShadow || buf_bob_a.SaveBuffer || buf_vs_a.CollMask || buf_vs_a.BorderLine ||
                buf_bob_a.DBuffer || buf_bob_b.ImageShadow || buf_bob_b.SaveBuffer || buf_vs_b.CollMask ||
                buf_vs_b.BorderLine || buf_bob_b.DBuffer || buf_bob_c.ImageShadow || buf_bob_c.SaveBuffer ||
                buf_vs_c.CollMask || buf_vs_c.BorderLine || buf_bob_c.DBuffer)
            {
                print("FAIL: FreeGBuffers() did not release all allocated buffers\n");
                errors++;
            }
            else
            {
                print("OK: FreeGBuffers() no longer behaves like a stub\n");
            }
        }
    }

    print("\n");
    return errors;
}

static int test_graphics_cbump_stub_closed(void)
{
    int errors = 0;
    struct UCopList ucl;
    struct CopList *first;

    print("--- Test: graphics CBump entry point ---\n");

    ucl.Next = NULL;
    ucl.FirstCopList = NULL;
    ucl.CopList = NULL;

    first = UCopperListInit(&ucl, 1);
    if (!first)
    {
        print("FAIL: UCopperListInit() returned NULL\n");
        return 1;
    }

    first->CopPtr->OpCode = COPPER_MOVE;
    first->CopPtr->u3.u4.u1.DestAddr = 0x0180;
    first->CopPtr->u3.u4.u2.DestData = 0xaaaa;
    CBump(&ucl);

    if (!first->Next || ucl.CopList != first->Next || first->CopIns[0].OpCode != CPRNXTBUF ||
        first->CopIns[0].u3.nxtlist != first->Next || ucl.CopList->Count != 1 ||
        ucl.CopList->CopPtr != (ucl.CopList->CopIns + 1) ||
        ucl.CopList->CopIns[0].OpCode != COPPER_MOVE ||
        ucl.CopList->CopIns[0].u3.u4.u1.DestAddr != 0x0180 ||
        ucl.CopList->CopIns[0].u3.u4.u2.DestData != (WORD)0xaaaa)
    {
        print("FAIL: CBump() did not advance the user copper list\n");
        errors++;
    }
    else
    {
        print("OK: CBump() no longer behaves like a stub\n");
    }

    exec_free_ucoplist_chain(&ucl);

    print("\n");
    return errors;
}

static int test_graphics_cmove_stub_closed(void)
{
    int errors = 0;
    struct UCopList ucl;
    struct CopList *first;

    print("--- Test: graphics CMove entry point ---\n");

    ucl.Next = NULL;
    ucl.FirstCopList = NULL;
    ucl.CopList = NULL;

    first = UCopperListInit(&ucl, 1);
    if (!first)
    {
        print("FAIL: UCopperListInit() returned NULL\n");
        return 1;
    }

    if (!CMove(&ucl, (APTR)0x0180, 0x55aa) ||
        first->CopIns[0].OpCode != COPPER_MOVE ||
        first->CopIns[0].u3.u4.u1.DestAddr != 0x0180 ||
        first->CopIns[0].u3.u4.u2.DestData != (WORD)0x55aa ||
        first->Count != 0 || first->CopPtr != first->CopIns)
    {
        print("FAIL: CMove() did not populate the active copper slot\n");
        errors++;
    }
    else
    {
        first->Count = first->MaxCount;
        first->CopPtr = first->CopIns + first->MaxCount;

        if (CMove(&ucl, (APTR)0x0182, 0x1234) != FALSE)
        {
            print("FAIL: CMove() did not report a full copper block\n");
            errors++;
        }
        else
        {
            print("OK: CMove() no longer behaves like a stub\n");
        }
    }

    exec_free_ucoplist_chain(&ucl);

    print("\n");
    return errors;
}

static int test_graphics_cwait_stub_closed(void)
{
    int errors = 0;
    struct UCopList ucl;
    struct CopList *first;

    print("--- Test: graphics CWait entry point ---\n");

    ucl.Next = NULL;
    ucl.FirstCopList = NULL;
    ucl.CopList = NULL;

    first = UCopperListInit(&ucl, 1);
    if (!first)
    {
        print("FAIL: UCopperListInit() returned NULL\n");
        return 1;
    }

    CWait(&ucl, 0x0033, 0x0077);

    if (first->CopIns[0].OpCode != COPPER_WAIT ||
        first->CopIns[0].u3.u4.u1.VWaitPos != (WORD)0x0033 ||
        first->CopIns[0].u3.u4.u2.HWaitPos != (WORD)0x0077 ||
        first->Count != 0 || first->CopPtr != first->CopIns)
    {
        print("FAIL: CWait() did not populate the active copper slot\n");
        errors++;
    }
    else
    {
        first->CopIns[0].OpCode = COPPER_MOVE;
        first->Count = first->MaxCount;
        first->CopPtr = first->CopIns + first->MaxCount;
        CWait(&ucl, 0x0011, 0x0022);

        if (first->CopIns[0].OpCode != COPPER_MOVE)
        {
            print("FAIL: CWait() overwrote a full copper block\n");
            errors++;
        }
        else
        {
            print("OK: CWait() no longer behaves like a stub\n");
        }
    }

    exec_free_ucoplist_chain(&ucl);

    print("\n");
    return errors;
}

static int test_graphics_calcivg_stub_closed(void)
{
    int errors = 0;
    struct View view;
    struct ViewPort vp;
    struct RasInfo ras_info;
    struct BitMap bit_map;
    struct CopIns cop_ins[3];
    struct CopList cop_list;

    print("--- Test: graphics CalcIVG entry point ---\n");

    memset(&view, 0, sizeof(view));
    memset(&vp, 0, sizeof(vp));
    memset(&ras_info, 0, sizeof(ras_info));
    memset(&bit_map, 0, sizeof(bit_map));
    memset(cop_ins, 0, sizeof(cop_ins));
    memset(&cop_list, 0, sizeof(cop_list));

    cop_ins[0].OpCode = COPPER_MOVE;
    cop_ins[1].OpCode = COPPER_MOVE;
    cop_ins[2].OpCode = COPPER_WAIT;
    cop_list.CopIns = cop_ins;
    cop_list.Count = 3;
    cop_list.MaxCount = 3;

    bit_map.BytesPerRow = 40;
    bit_map.Rows = 200;
    bit_map.Depth = 2;

    ras_info.BitMap = &bit_map;

    view.ViewPort = &vp;
    view.Modes = LACE;
    vp.DspIns = &cop_list;
    vp.RasInfo = &ras_info;
    vp.DWidth = 320;
    vp.DHeight = 200;
    vp.Modes = HIRES | LACE;

    if (CalcIVG(&view, &vp) != 1)
    {
        print("FAIL: CalcIVG() returned unexpected laced result\n");
        errors++;
    }
    else
    {
        bit_map.Depth = 8;
        vp.DWidth = 640;
        if (CalcIVG(&view, &vp) != 0)
        {
            print("FAIL: CalcIVG() ignored saturated display bandwidth\n");
            errors++;
        }
        else
        {
            vp.DspIns = NULL;
            if (CalcIVG(&view, &vp) != 0)
            {
                print("FAIL: CalcIVG() accepted a viewport without DspIns\n");
                errors++;
            }
            else
            {
                print("OK: CalcIVG() no longer behaves like a stub\n");
            }
        }
    }

    print("\n");
    return errors;
}

static int test_graphics_setchiprev_stub_closed(void)
{
    int errors = 0;
    UBYTE original_bits = GfxBase->ChipRevBits0;

    print("--- Test: graphics SetChipRev entry point ---\n");

    if (SetChipRev(SETCHIPREV_A) != SETCHIPREV_A ||
        GfxBase->ChipRevBits0 != SETCHIPREV_A)
    {
        print("FAIL: SetChipRev() did not switch to OCS/ECS Agnus bits\n");
        errors++;
    }
    else if (SetChipRev(SETCHIPREV_ECS) != SETCHIPREV_ECS ||
             GfxBase->ChipRevBits0 != SETCHIPREV_ECS)
    {
        print("FAIL: SetChipRev() did not switch to ECS bits\n");
        errors++;
    }
    else if (SetChipRev(SETCHIPREV_AA) != SETCHIPREV_AA ||
             GfxBase->ChipRevBits0 != SETCHIPREV_AA)
    {
        print("FAIL: SetChipRev() did not switch to AGA bits\n");
        errors++;
    }
    else if (SetChipRev(0x13579BDFUL) != SETCHIPREV_AA ||
             GfxBase->ChipRevBits0 != SETCHIPREV_AA)
    {
        print("FAIL: SetChipRev() changed state for unsupported chip request\n");
        errors++;
    }
    else if (SetChipRev(SETCHIPREV_BEST) != SETCHIPREV_AA ||
             GfxBase->ChipRevBits0 != SETCHIPREV_AA)
    {
        print("FAIL: SetChipRev() did not report best available chip set\n");
        errors++;
    }
    else
    {
        print("OK: SetChipRev() no longer behaves like a stub\n");
    }

    GfxBase->ChipRevBits0 = original_bits;

    print("\n");
    return errors;
}

static int test_graphics_syncsbitmap_stub_closed(void)
{
    int errors = 0;
    struct Layer_Info *li = NULL;
    struct BitMap *screen_bm = NULL;
    struct BitMap *super_bm = NULL;
    struct Layer *layer = NULL;
    struct RastPort screen_rp;
    struct RastPort super_rp;

    print("--- Test: graphics SyncSBitMap entry point ---\n");

    li = NewLayerInfo();
    screen_bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    super_bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!li || !screen_bm || !super_bm)
    {
        print("FAIL: Could not allocate SuperBitMap test setup\n");
        errors++;
        goto cleanup;
    }

    InitBitMap(screen_bm, 1, 64, 64);
    InitBitMap(super_bm, 1, 64, 64);
    screen_bm->Planes[0] = AllocRaster(64, 64);
    super_bm->Planes[0] = AllocRaster(64, 64);
    if (!screen_bm->Planes[0] || !super_bm->Planes[0])
    {
        print("FAIL: Could not allocate SuperBitMap raster planes\n");
        errors++;
        goto cleanup;
    }

    layer = CreateBehindLayer(li, screen_bm, 10, 12, 29, 31, LAYERSUPER | LAYERSMART, super_bm);
    if (!layer || !layer->rp)
    {
        print("FAIL: Could not create SuperBitMap layer\n");
        errors++;
        goto cleanup;
    }

    InitRastPort(&screen_rp);
    screen_rp.BitMap = screen_bm;
    SetRast(&screen_rp, 0);

    InitRastPort(&super_rp);
    super_rp.BitMap = super_bm;
    SetRast(&super_rp, 0);

    layer->Scroll_X = 2;
    layer->Scroll_Y = 3;
    SetAPen(&screen_rp, 1);
    WritePixel(&screen_rp, 14, 18);

    SyncSBitMap(layer);

    if (ReadPixel(&super_rp, 2, 3) != 1 || ReadPixel(&super_rp, 4, 6) != 0)
    {
        print("FAIL: SyncSBitMap() did not copy visible content into the SuperBitMap\n");
        errors++;
    }
    else
    {
        layer->Flags &= ~LAYERSUPER;
        SetRast(&super_rp, 0);
        SyncSBitMap(layer);
        if (ReadPixel(&super_rp, 2, 3) != 0)
        {
            print("FAIL: SyncSBitMap() ignored the LAYERSUPER guard\n");
            errors++;
        }
        else
        {
            print("OK: SyncSBitMap() no longer behaves like a stub\n");
        }
    }

cleanup:
    if (layer)
        DeleteLayer(0, layer);
    if (screen_bm)
    {
        if (screen_bm->Planes[0])
            FreeRaster(screen_bm->Planes[0], 64, 64);
        FreeMem(screen_bm, sizeof(struct BitMap));
    }
    if (super_bm)
    {
        if (super_bm->Planes[0])
            FreeRaster(super_bm->Planes[0], 64, 64);
        FreeMem(super_bm, sizeof(struct BitMap));
    }
    if (li)
        DisposeLayerInfo(li);

    print("\n");
    return errors;
}

static int test_graphics_copysbitmap_stub_closed(void)
{
    int errors = 0;
    struct Layer_Info *li = NULL;
    struct BitMap *screen_bm = NULL;
    struct BitMap *super_bm = NULL;
    struct Layer *layer = NULL;
    struct RastPort screen_rp;
    struct RastPort super_rp;

    print("--- Test: graphics CopySBitMap entry point ---\n");

    li = NewLayerInfo();
    screen_bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    super_bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!li || !screen_bm || !super_bm)
    {
        print("FAIL: Could not allocate CopySBitMap test setup\n");
        errors++;
        goto cleanup;
    }

    InitBitMap(screen_bm, 1, 64, 64);
    InitBitMap(super_bm, 1, 64, 64);
    screen_bm->Planes[0] = AllocRaster(64, 64);
    super_bm->Planes[0] = AllocRaster(64, 64);
    if (!screen_bm->Planes[0] || !super_bm->Planes[0])
    {
        print("FAIL: Could not allocate CopySBitMap raster planes\n");
        errors++;
        goto cleanup;
    }

    layer = CreateBehindLayer(li, screen_bm, 10, 12, 29, 31, LAYERSUPER | LAYERSMART, super_bm);
    if (!layer || !layer->rp)
    {
        print("FAIL: Could not create CopySBitMap layer\n");
        errors++;
        goto cleanup;
    }

    InitRastPort(&screen_rp);
    screen_rp.BitMap = screen_bm;
    SetRast(&screen_rp, 0);

    InitRastPort(&super_rp);
    super_rp.BitMap = super_bm;
    SetRast(&super_rp, 0);

    layer->Scroll_X = 2;
    layer->Scroll_Y = 3;
    SetAPen(&super_rp, 1);
    WritePixel(&super_rp, 2, 3);

    CopySBitMap(layer);

    if (ReadPixel(&screen_rp, 14, 18) != 1 || ReadPixel(&screen_rp, 16, 21) != 0)
    {
        print("FAIL: CopySBitMap() did not copy backing content into the visible layer\n");
        errors++;
    }
    else
    {
        layer->Flags &= ~LAYERSUPER;
        SetRast(&screen_rp, 0);
        CopySBitMap(layer);
        if (ReadPixel(&screen_rp, 14, 18) != 0)
        {
            print("FAIL: CopySBitMap() ignored the LAYERSUPER guard\n");
            errors++;
        }
        else
        {
            print("OK: CopySBitMap() no longer behaves like a stub\n");
        }
    }

cleanup:
    if (layer)
        DeleteLayer(0, layer);
    if (screen_bm)
    {
        if (screen_bm->Planes[0])
            FreeRaster(screen_bm->Planes[0], 64, 64);
        FreeMem(screen_bm, sizeof(struct BitMap));
    }
    if (super_bm)
    {
        if (super_bm->Planes[0])
            FreeRaster(super_bm->Planes[0], 64, 64);
        FreeMem(super_bm, sizeof(struct BitMap));
    }
    if (li)
        DisposeLayerInfo(li);

    print("\n");
    return errors;
}

static int test_graphics_getextsprite_stub_closed(void)
{
    int errors = 0;
    struct ExtSprite single_sprite;
    struct ExtSprite attached_primary;
    struct ExtSprite attached_secondary;
    struct ExtSprite specific_primary;
    struct ExtSprite specific_secondary;
    struct ExtSprite odd_secondary;
    struct TagItem attached_tags[] = {
        { GSTAG_ATTACHED, (ULONG)&attached_secondary },
        { TAG_DONE, 0 }
    };
    struct TagItem specific_tags[] = {
        { GSTAG_SPRITE_NUM, 4 },
        { GSTAG_ATTACHED, (ULONG)&specific_secondary },
        { TAG_DONE, 0 }
    };
    struct TagItem odd_tags[] = {
        { GSTAG_SPRITE_NUM, 3 },
        { GSTAG_ATTACHED, (ULONG)&odd_secondary },
        { TAG_DONE, 0 }
    };

    print("--- Test: graphics GetExtSpriteA entry point ---\n");

    single_sprite.es_SimpleSprite.num = 99;
    if (GetExtSpriteA(&single_sprite, NULL) != 0 || single_sprite.es_SimpleSprite.num != 0)
    {
        print("FAIL: GetExtSpriteA() did not allocate the first free single sprite\n");
        errors++;
    }
    else
    {
        print("OK: GetExtSpriteA() allocates a free single sprite\n");
    }

    attached_primary.es_SimpleSprite.num = 99;
    attached_secondary.es_SimpleSprite.num = 99;
    if (GetExtSpriteA(&attached_primary, attached_tags) != 2 ||
        attached_primary.es_SimpleSprite.num != 2 ||
        attached_secondary.es_SimpleSprite.num != 3)
    {
        print("FAIL: GetExtSpriteA() did not allocate an attached sprite pair\n");
        errors++;
    }
    else
    {
        print("OK: GetExtSpriteA() allocates an attached sprite pair\n");
    }

    if (GetExtSpriteA(&specific_primary, specific_tags) != 4 ||
        specific_primary.es_SimpleSprite.num != 4 ||
        specific_secondary.es_SimpleSprite.num != 5)
    {
        print("FAIL: GetExtSpriteA() did not honor GSTAG_SPRITE_NUM for attached allocation\n");
        errors++;
    }
    else
    {
        print("OK: GetExtSpriteA() honors GSTAG_SPRITE_NUM for attached allocation\n");
    }

    odd_secondary.es_SimpleSprite.num = 99;
    if (GetExtSpriteA(&specific_primary, odd_tags) != -1 ||
        specific_primary.es_SimpleSprite.num != (UWORD)-1 ||
        odd_secondary.es_SimpleSprite.num != (UWORD)-1)
    {
        print("FAIL: GetExtSpriteA() accepted an odd-numbered attached request\n");
        errors++;
    }
    else
    {
        print("OK: GetExtSpriteA() rejects odd-numbered attached requests\n");
    }

    FreeSprite(0);
    FreeSprite(2);
    FreeSprite(3);
    FreeSprite(4);
    FreeSprite(5);

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

    /* Test 14: Verify SameDevice no longer hits the stub path */
    errors += test_dos_samedevice_stub_closed();

    /* Test 15: Verify ErrorReport no longer hits the stub path */
    errors += test_dos_errorreport_stub_closed();

    /* Test 16: Verify GetConsoleTask no longer hits the stub path */
    errors += test_dos_getconsoletask_stub_closed();

    /* Test 17: Verify SetConsoleTask no longer hits the stub path */
    errors += test_dos_setconsoletask_stub_closed();

    /* Test 18: Verify GetFileSysTask no longer hits the stub path */
    errors += test_dos_getfilesystask_stub_closed();

    /* Test 19: Verify SetFileSysTask no longer hits the stub path */
    errors += test_dos_setfilesystask_stub_closed();

    /* Test 20: Verify GetArgStr no longer hits the stub path */
    errors += test_dos_getargstr_stub_closed();

    /* Test 21: Verify CliInitNewcli no longer hits the stub path */
    errors += test_dos_cliinitnewcli_stub_closed();

    /* Test 22: Verify CliInitRun no longer hits the stub path */
    errors += test_dos_cliinitrun_stub_closed();

    /* Test 23: Verify SetArgStr no longer hits the stub path */
    errors += test_dos_setargstr_stub_closed();

    /* Test 24: Verify RemDosEntry no longer hits the stub path */
    errors += test_dos_remdosentry_stub_closed();

    /* Test 25: Verify AddDosEntry no longer hits the stub path */
    errors += test_dos_adddosentry_stub_closed();

    /* Test 26: Verify MakeDosEntry no longer hits the stub path */
    errors += test_dos_makedosentry_stub_closed();

    /* Test 27: Verify FreeDosEntry no longer hits the stub path */
    errors += test_dos_freedosentry_stub_closed();

    /* Test 28: Verify Format no longer hits the stub path */
    errors += test_dos_format_stub_closed();

    /* Test 29: Verify Relabel no longer hits the stub path */
    errors += test_dos_relabel_stub_closed();

    /* Test 30: Verify Inhibit no longer hits the stub path */
    errors += test_dos_inhibit_stub_closed();

    /* Test 31: Verify AddBuffers no longer hits the stub path */
    errors += test_dos_addbuffers_stub_closed();

    /* Test 32: Verify SetOwner no longer hits the stub path */
    errors += test_dos_setowner_stub_closed();

    /* Test 33: Verify AddSegment no longer hits the stub path */
    errors += test_dos_addsegment_stub_closed();

    /* Test 34: Verify ReadItem no longer hits the stub path */
    errors += test_dos_readitem_stub_closed();

    /* Test 35: Verify PackBoolTags no longer hits the stub path */
    errors += test_utility_packbooltags_stub_closed();

    /* Test 36: Verify FilterTagChanges no longer hits the stub path */
    errors += test_utility_filtertagchanges_stub_closed();

    /* Test 37: Verify ApplyTagChanges no longer hits the stub path */
    errors += test_utility_applytagchanges_stub_closed();

    /* Test 38: Verify AddAnimOb no longer hits the stub path */
    errors += test_graphics_addanimob_stub_closed();

    /* Test 39: Verify RemIBob no longer hits the stub path */
    errors += test_graphics_remibob_stub_closed();

    /* Test 40: Verify DoCollision/InitMasks/SetCollision no longer hit the stub path */
    errors += test_graphics_docollision_stub_closed();

    /* Test 41: Verify Animate no longer hits the stub path */
    errors += test_graphics_animate_stub_closed();

    /* Test 42: Verify GetGBuffers no longer hits the stub path */
    errors += test_graphics_getgbuffers_stub_closed();

    /* Test 43: Verify CBump no longer hits the stub path */
    errors += test_graphics_cbump_stub_closed();

    /* Test 44: Verify CMove no longer hits the stub path */
    errors += test_graphics_cmove_stub_closed();

    /* Test 45: Verify CWait no longer hits the stub path */
    errors += test_graphics_cwait_stub_closed();

    /* Test 46: Verify CalcIVG no longer hits the stub path */
    errors += test_graphics_calcivg_stub_closed();

    /* Test 47: Verify SetChipRev no longer hits the stub path */
    errors += test_graphics_setchiprev_stub_closed();

    /* Test 48: Verify SyncSBitMap no longer hits the stub path */
    errors += test_graphics_syncsbitmap_stub_closed();

    /* Test 49: Verify CopySBitMap no longer hits the stub path */
    errors += test_graphics_copysbitmap_stub_closed();

    /* Test 50: Verify GetExtSpriteA no longer hits the stub path */
    errors += test_graphics_getextsprite_stub_closed();

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
