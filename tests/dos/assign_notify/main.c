#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/notify.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static int tests_passed = 0;
static int tests_failed = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    while (s[len]) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[32];
    char *p = buf;
    char tmp[16];
    int i = 0;

    if (n < 0) {
        *p++ = '-';
        n = -n;
    }

    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    while (i > 0) *p++ = tmp[--i];
    *p = '\0';
    print(buf);
}

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
    tests_passed++;
}

static void test_fail(const char *name, const char *reason)
{
    print("  FAIL: ");
    print(name);
    print(" - ");
    print(reason);
    print("\n");
    tests_failed++;
}

static BOOL path_accessible(CONST_STRPTR path)
{
    BPTR lock = Lock(path, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        return TRUE;
    }
    return FALSE;
}

static BOOL create_file(CONST_STRPTR path, CONST_STRPTR text)
{
    BPTR fh = Open(path, MODE_NEWFILE);
    LONG len = 0;

    if (!fh) return FALSE;
    while (text[len]) len++;
    Write(fh, (CONST APTR)text, len);
    Close(fh);
    return TRUE;
}

static void print_ioerr(const char *label)
{
    print("  ");
    print(label);
    print(": ");
    print_num(IoErr());
    print("\n");
}

static void cleanup(void)
{
    RemAssignList((STRPTR)"MULTIASSIGN", 0);
    RemAssignList((STRPTR)"LATEASSIGN", 0);
    DeleteFile((CONST_STRPTR)"notifytest/file.txt");
    DeleteFile((CONST_STRPTR)"notifytest");
    DeleteFile((CONST_STRPTR)"assign_a/file_a.txt");
    DeleteFile((CONST_STRPTR)"assign_b/file_b.txt");
    DeleteFile((CONST_STRPTR)"assign_a");
    DeleteFile((CONST_STRPTR)"assign_b");
}

int main(void)
{
    BPTR lock_a;
    BPTR lock_b;
    struct DevProc *dp;
    struct MsgPort *port;
    struct MsgPort *device_port;
    struct NotifyRequest notify;
    struct NotifyMessage *msg;

    print("DOS Assign/Notify Test\n");
    print("======================\n\n");

    cleanup();

    lock_a = CreateDir((CONST_STRPTR)"assign_a");
    lock_b = CreateDir((CONST_STRPTR)"assign_b");
    if (!lock_a || !lock_b) {
        print("ERROR: setup failed\n");
        return 20;
    }

    UnLock(lock_a);
    UnLock(lock_b);

    create_file((CONST_STRPTR)"assign_a/file_a.txt", (CONST_STRPTR)"A");
    create_file((CONST_STRPTR)"assign_b/file_b.txt", (CONST_STRPTR)"B");

    print("Test 1: AssignLate\n");
    if (AssignLate((STRPTR)"LATEASSIGN", (STRPTR)"assign_a")) {
        if (path_accessible((CONST_STRPTR)"LATEASSIGN:file_a.txt"))
            test_pass("AssignLate resolves target");
        else
            test_fail("AssignLate", "target not reachable");
    } else {
        print_ioerr("AssignLate IoErr");
        test_fail("AssignLate", "call failed");
    }

    print("\nTest 2: AssignAdd multi-path\n");
    lock_a = Lock((CONST_STRPTR)"assign_a", SHARED_LOCK);
    lock_b = Lock((CONST_STRPTR)"assign_b", SHARED_LOCK);
    if (lock_a && lock_b && AssignLock((STRPTR)"MULTIASSIGN", lock_a) && AssignAdd((STRPTR)"MULTIASSIGN", lock_b)) {
        if (path_accessible((CONST_STRPTR)"MULTIASSIGN:file_a.txt") &&
            path_accessible((CONST_STRPTR)"MULTIASSIGN:file_b.txt"))
            test_pass("AssignAdd iterates both directories");
        else
            test_fail("AssignAdd", "one or more targets missing");
    } else {
        if (lock_a) UnLock(lock_a);
        if (lock_b) UnLock(lock_b);
        print_ioerr("AssignAdd IoErr");
        test_fail("AssignAdd", "assign creation failed");
    }

    print("\nTest 3: RemAssignList specific path\n");
    lock_b = Lock((CONST_STRPTR)"assign_b", SHARED_LOCK);
    if (lock_b && RemAssignList((STRPTR)"MULTIASSIGN", lock_b)) {
        if (path_accessible((CONST_STRPTR)"MULTIASSIGN:file_a.txt") &&
            !path_accessible((CONST_STRPTR)"MULTIASSIGN:file_b.txt"))
            test_pass("RemAssignList removes one path");
        else
            test_fail("RemAssignList path", "path list incorrect");
    } else {
        if (lock_b) UnLock(lock_b);
        test_fail("RemAssignList path", "call failed");
    }

    print("\nTest 4: GetDeviceProc multi-assign iteration\n");
    dp = GetDeviceProc((CONST_STRPTR)"MULTIASSIGN:file_a.txt", NULL);
    if (dp) {
        if (dp->dvp_Lock)
            test_pass("GetDeviceProc first target");
        else
            test_fail("GetDeviceProc first target", "missing lock");

        dp = GetDeviceProc((CONST_STRPTR)"MULTIASSIGN:file_a.txt", dp);
        if (!dp)
            test_pass("GetDeviceProc reports no more entries");
        else {
            FreeDeviceProc(dp);
            test_fail("GetDeviceProc iteration", "expected end of list");
        }
    } else {
        print_ioerr("GetDeviceProc IoErr");
        test_fail("GetDeviceProc", "initial call failed");
    }

    print("\nTest 5: StartNotify/EndNotify message delivery\n");
    if (!CreateDir((CONST_STRPTR)"notifytest")) {
        BPTR notify_lock = Lock((CONST_STRPTR)"notifytest", SHARED_LOCK);
        if (notify_lock) UnLock(notify_lock);
    }
    port = CreateMsgPort();
    if (!port) {
        test_fail("CreateMsgPort", "failed");
    } else {
        notify.nr_Name = (STRPTR)"notifytest/file.txt";
        notify.nr_FullName = NULL;
        notify.nr_UserData = 0x1234;
        notify.nr_Flags = NRF_SEND_MESSAGE;
        notify.nr_stuff.nr_Msg.nr_Port = port;
        notify.nr_Reserved[0] = 0;
        notify.nr_Reserved[1] = 0;
        notify.nr_Reserved[2] = 0;
        notify.nr_Reserved[3] = 0;

        if (StartNotify(&notify)) {
            Delay(2);
            create_file((CONST_STRPTR)"notifytest/file.txt", (CONST_STRPTR)"changed");
            Delay(5);

            msg = (struct NotifyMessage *)GetMsg(port);
            if (msg && msg->nm_Class == NOTIFY_CLASS && msg->nm_Code == NOTIFY_CODE && msg->nm_NReq == &notify) {
                test_pass("StartNotify delivered message");
                ReplyMsg((struct Message *)msg);
            } else {
                test_fail("StartNotify", "no notification message");
            }

            EndNotify(&notify);
        } else {
            test_fail("StartNotify", "call failed");
        }

        DeleteMsgPort(port);
    }

    print("\nTest 6: SetProgramDir / GetProgramDir\n");
    lock_a = Lock((CONST_STRPTR)"assign_a", SHARED_LOCK);
    if (lock_a) {
        BPTR old_progdir = SetProgramDir(lock_a);
        if (GetProgramDir() == lock_a) {
            test_pass("SetProgramDir updates PROGDIR lock");
        } else {
            test_fail("SetProgramDir", "GetProgramDir mismatch");
        }
        SetProgramDir(old_progdir);
        if (lock_a)
            UnLock(lock_a);
    } else {
        test_fail("SetProgramDir", "could not lock assign_a");
    }

    print("\nTest 7: DeviceProc via assign\n");
    device_port = DeviceProc((CONST_STRPTR)"LATEASSIGN:");
    if (device_port) {
        test_pass("DeviceProc resolves assign-backed port");
    } else {
        print_ioerr("DeviceProc IoErr");
        test_fail("DeviceProc", "returned NULL");
    }

    cleanup();

    print("\nPassed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 10 : 0;
}
