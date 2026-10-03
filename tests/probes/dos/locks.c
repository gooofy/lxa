/*
 * Probe (Phase 222a): dos.library locks and files in T: - Lock, UnLock,
 * DupLock, SameLock, ParentDir, CreateDir, Examine, ExNext, DeleteFile,
 * Rename, SetProtection, SetComment, Open modes, Read/Write/Seek,
 * SetFileSize, lock/open conflicts, NameFromLock/NameFromFH tails,
 * CurrentDir-relative names, and the IoErr() of every failure.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define BASE "T:lxaprobe_locks"

static void err(const char *label, LONG ok)
{
    LONG e = IoErr();
    probe_s(label);
    probe_s(" = ");
    probe_s(ok ? "ok" : "FAIL");
    if (!ok) {
        probe_s(" IoErr ");
        probe_dec(e);
    }
    probe_ch('\n');
}

static void writefile(const char *name, const char *data)
{
    BPTR fh = Open((STRPTR)name, MODE_NEWFILE);
    LONG n = 0;
    if (!fh)
        return;
    while (data[n])
        n++;
    Write(fh, (APTR)data, n);
    Close(fh);
}

/* last path component(s) of NameFromLock: the volume differs (RAM: vs SYS:) */
static void tail(const char *label, const char *path, int comps)
{
    int len = 0, i, seen = 0;
    while (path[len])
        len++;
    for (i = len - 1; i >= 0; i--)
        if (path[i] == '/' || path[i] == ':')
            if (++seen == comps)
                break;
    P_STR(label, path + i + 1);
}

static char lastch(const char *s)
{
    char c = 0;
    while (*s)
        c = *s++;
    return c;
}

static void cleanup(void)
{
    static const char *const files[] = {
        BASE "/sub/inner", BASE "/sub/moved", BASE "/sub", BASE "/file1", BASE "/file2", BASE "/renamed",
        BASE "/big", BASE "/newdir", BASE "/newrw", BASE "/FILE1", BASE,
    };
    int i;
    for (i = 0; i < (int)(sizeof(files) / sizeof(files[0])); i++) {
        SetProtection((STRPTR)files[i], 0);
        DeleteFile((STRPTR)files[i]);
    }
}

int main(void)
{
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    BPTR l1, l2, l3, fh, fh2;
    char buf[256];
    LONG r;

    if (!fib)
        return 20;
    /* no "insert volume" requesters: unknown volumes just fail */
    ((struct Process *)FindTask(NULL))->pr_WindowPtr = (APTR)-1;
    cleanup();

    P_SECTION("CreateDir/Lock");
    l1 = CreateDir((STRPTR)BASE);
    err("CreateDir(" BASE ")", l1 != 0);
    l2 = CreateDir((STRPTR)BASE);
    err("CreateDir again", l2 != 0);
    if (l2)
        UnLock(l2);
    UnLock(l1);
    l1 = Lock((STRPTR)BASE "/missing", SHARED_LOCK);
    err("Lock(missing)", l1 != 0);
    l1 = Lock((STRPTR)"T:lxaprobe_nodir/missing", SHARED_LOCK);
    err("Lock(missing dir/missing)", l1 != 0);
    l1 = Lock((STRPTR)"LXAPROBENOVOL:", SHARED_LOCK);
    err("Lock(unknown volume)", l1 != 0);
    writefile(BASE "/file1", "hello world\n");
    writefile(BASE "/file2", "");
    l1 = Lock((STRPTR)BASE "/file1/x", SHARED_LOCK);
    err("Lock(file/x)", l1 != 0);
    if (l1)
        UnLock(l1);
    l1 = CreateDir((STRPTR)BASE "/file1");
    err("CreateDir(existing file)", l1 != 0);
    if (l1)
        UnLock(l1);
    l1 = CreateDir((STRPTR)BASE "/nodir/sub");
    err("CreateDir(missing parent)", l1 != 0);
    if (l1)
        UnLock(l1);

    P_SECTION("Examine");
    l1 = Lock((STRPTR)BASE, SHARED_LOCK);
    r = Examine(l1, fib);
    err("Examine(dir)", r);
    P_STR("fib_FileName", fib->fib_FileName);
    P_LONG("fib_DirEntryType", fib->fib_DirEntryType);
    P_LONG("fib_EntryType", fib->fib_EntryType);
    P_HEX("fib_Protection", fib->fib_Protection);
    P_STR("fib_Comment", fib->fib_Comment);
    {
        char names[8][32];
        LONG types[8], sizes[8];
        int n = 0, i, j;
        while (n < 8 && ExNext(l1, fib)) {
            for (i = 0; i < 31 && fib->fib_FileName[i]; i++)
                names[n][i] = fib->fib_FileName[i];
            names[n][i] = 0;
            types[n] = fib->fib_DirEntryType;
            sizes[n] = fib->fib_Size;
            n++;
        }
        P_LONG("ExNext end IoErr", IoErr());
        for (i = 0; i < n; i++)         /* the order is file-system specific */
            for (j = i + 1; j < n; j++) {
                int a = 0;
                while (names[i][a] && names[i][a] == names[j][a])
                    a++;
                if ((UBYTE)names[i][a] > (UBYTE)names[j][a]) {
                    char t[32];
                    LONG x;
                    for (a = 0; a < 32; a++) { t[a] = names[i][a]; names[i][a] = names[j][a]; names[j][a] = t[a]; }
                    x = types[i]; types[i] = types[j]; types[j] = x;
                    x = sizes[i]; sizes[i] = sizes[j]; sizes[j] = x;
                }
            }
        for (i = 0; i < n; i++) {
            probe_s("entry ");
            probe_s(names[i]);
            probe_s(" type ");
            probe_dec(types[i]);
            probe_s(" size ");
            probe_dec(sizes[i]);
            probe_ch('\n');
        }
    }
    l2 = Lock((STRPTR)BASE "/file1", SHARED_LOCK);
    r = Examine(l2, fib);
    err("Examine(file)", r);
    P_STR("fib_FileName", fib->fib_FileName);
    P_LONG("fib_DirEntryType", fib->fib_DirEntryType);
    P_LONG("fib_Size", fib->fib_Size);
    P_HEX("fib_Protection", fib->fib_Protection);
    /* ExNext() on a file lock is file-system specific (RAM: answers one
     * empty entry), not probed */

    P_SECTION("SameLock/DupLock/ParentDir/NameFromLock");
    l3 = DupLock(l2);
    P_LONG("SameLock(file, DupLock)", SameLock(l2, l3));
    P_LONG("SameLock(file, dir)", SameLock(l2, l1));
    UnLock(l3);
    l3 = ParentDir(l2);
    P_LONG("SameLock(ParentDir(file), dir)", SameLock(l3, l1));
    UnLock(l3);
    l3 = DupLock(0);
    P_BOOL("DupLock(0) == 0", l3 == 0);
    r = NameFromLock(l2, (STRPTR)buf, sizeof(buf));
    err("NameFromLock(file)", r);
    tail("NameFromLock(file) tail", buf, 2);
    r = NameFromLock(l1, (STRPTR)buf, sizeof(buf));
    tail("NameFromLock(dir) tail", buf, 1);
    r = NameFromLock(l2, (STRPTR)buf, 6);
    err("NameFromLock(file, 6 bytes)", r);
    {
        BPTR root = Lock((STRPTR)"SYS:", SHARED_LOCK);
        NameFromLock(root, (STRPTR)buf, sizeof(buf));
        P_BOOL("NameFromLock(SYS:) ends with ':'", buf[0] && lastch(buf) == ':');
        l3 = ParentDir(root);
        P_BOOL("ParentDir(volume root) == 0", l3 == 0);
        P_LONG("ParentDir(root) IoErr", IoErr());
        if (l3)
            UnLock(l3);
        UnLock(root);
    }
    UnLock(l2);

    P_SECTION("CurrentDir-relative");
    {
        BPTR old = CurrentDir(l1);
        BPTR rel = Lock((STRPTR)"file1", SHARED_LOCK);
        err("Lock(\"file1\") relative", rel != 0);
        if (rel) {
            l3 = Lock((STRPTR)"/lxaprobe_locks/file2", SHARED_LOCK);
            err("Lock(\"/lxaprobe_locks/file2\")", l3 != 0);
            if (l3)
                UnLock(l3);
            l3 = Lock((STRPTR)"", SHARED_LOCK);
            P_LONG("SameLock(Lock(\"\"), current dir)", SameLock(l3, l1));
            if (l3)
                UnLock(l3);
            UnLock(rel);
        }
        CurrentDir(old);
    }

    P_SECTION("Open modes and conflicts");
    fh = Open((STRPTR)BASE "/missing", MODE_OLDFILE);
    err("Open(missing, OLDFILE)", fh != 0);
    fh = Open((STRPTR)BASE, MODE_OLDFILE);
    err("Open(dir, OLDFILE)", fh != 0);
    if (fh)
        Close(fh);
    fh = Open((STRPTR)BASE "/nodir/x", MODE_NEWFILE);
    err("Open(missing dir/x, NEWFILE)", fh != 0);
    l2 = Lock((STRPTR)BASE "/file1", EXCLUSIVE_LOCK);
    err("Lock(file1, EXCLUSIVE)", l2 != 0);
    l3 = Lock((STRPTR)BASE "/file1", SHARED_LOCK);
    err("Lock(file1, SHARED) while exclusive", l3 != 0);
    if (l3)
        UnLock(l3);
    fh = Open((STRPTR)BASE "/file1", MODE_OLDFILE);
    err("Open(file1, OLDFILE) while exclusive", fh != 0);
    if (fh)
        Close(fh);
    if (l2)
        UnLock(l2);
    fh = Open((STRPTR)BASE "/file1", MODE_OLDFILE);
    fh2 = Open((STRPTR)BASE "/file1", MODE_OLDFILE);
    err("two OLDFILE opens", fh && fh2);
    if (fh2)
        Close(fh2);
    fh2 = Open((STRPTR)BASE "/file1", MODE_NEWFILE);
    err("NEWFILE while open OLDFILE", fh2 != 0);
    if (fh2)
        Close(fh2);
    l3 = Lock((STRPTR)BASE "/file1", EXCLUSIVE_LOCK);
    err("EXCLUSIVE lock while open", l3 != 0);
    if (l3)
        UnLock(l3);
    r = DeleteFile((STRPTR)BASE "/file1");
    err("DeleteFile while open", r);
    if (fh)
        Close(fh);
    fh = Open((STRPTR)BASE "/file1", MODE_READWRITE);
    err("Open(file1, READWRITE)", fh != 0);
    if (fh)
        Close(fh);
    fh = Open((STRPTR)BASE "/newrw", MODE_READWRITE);
    err("Open(new file, READWRITE) creates", fh != 0);
    if (fh) {
        Close(fh);
        DeleteFile((STRPTR)BASE "/newrw");
    }

    P_SECTION("Read/Write/Seek/SetFileSize");
    fh = Open((STRPTR)BASE "/big", MODE_NEWFILE);
    if (fh) {
        LONG i;
        for (i = 0; i < 256; i++)
            buf[i] = (char)i;
        P_LONG("Write 256", Write(fh, buf, 256));
        P_LONG("Write 0", Write(fh, buf, 0));
        P_LONG("Seek(0, CURRENT) old pos", Seek(fh, 0, OFFSET_CURRENT));
        P_LONG("Seek(10, BEGINNING) old pos", Seek(fh, 10, OFFSET_BEGINNING));
        P_LONG("Seek(-6, END) old pos", Seek(fh, -6, OFFSET_END));
        P_LONG("Read 100 at 250", Read(fh, buf, 100));
        P_BYTES("data", buf, 6);
        P_LONG("Read at EOF", Read(fh, buf, 10));
        P_LONG("Seek(1000, BEGINNING) past EOF", Seek(fh, 1000, OFFSET_BEGINNING));
        P_LONG("  IoErr", IoErr());
        P_LONG("Seek(-1, BEGINNING)", Seek(fh, -1, OFFSET_BEGINNING));
        P_LONG("  IoErr", IoErr());
        P_LONG("Seek(0, CURRENT) after failures", Seek(fh, 0, OFFSET_CURRENT));
        P_LONG("Seek(0, 5) bad mode", Seek(fh, 0, 5));
        P_LONG("  IoErr", IoErr());
        P_LONG("SetFileSize(100, BEGINNING)", SetFileSize(fh, 100, OFFSET_BEGINNING));
        P_LONG("Seek(0, END) old pos", Seek(fh, 0, OFFSET_END));
        P_LONG("Seek(0, CURRENT)", Seek(fh, 0, OFFSET_CURRENT));
        P_LONG("SetFileSize(-10, END)", SetFileSize(fh, -10, OFFSET_END));
        P_LONG("SetFileSize(400, BEGINNING) grows", SetFileSize(fh, 400, OFFSET_BEGINNING));
        /* where the position ends after growing is file-system specific */
        Close(fh);
        l2 = Lock((STRPTR)BASE "/big", SHARED_LOCK);
        Examine(l2, fib);
        P_LONG("fib_Size after", fib->fib_Size);
        UnLock(l2);
    }
    fh = Open((STRPTR)BASE "/file2", MODE_OLDFILE);
    if (fh) {
        P_LONG("Read empty file", Read(fh, buf, 10));
        P_LONG("Write to OLDFILE handle", Write(fh, (APTR)"abc", 3));
        P_LONG("Seek(0, BEGINNING) after write", Seek(fh, 0, OFFSET_BEGINNING));
        P_LONG("Read back", Read(fh, buf, 10));
        Close(fh);
    }

    P_SECTION("Rename/Delete/Protection/Comment");
    l3 = CreateDir((STRPTR)BASE "/sub");
    if (l3)
        UnLock(l3);
    writefile(BASE "/sub/inner", "x");
    r = Rename((STRPTR)BASE "/file2", (STRPTR)BASE "/renamed");
    err("Rename(file2 -> renamed)", r);
    r = Rename((STRPTR)BASE "/renamed", (STRPTR)BASE "/file1");
    err("Rename onto existing", r);
    r = Rename((STRPTR)BASE "/missing", (STRPTR)BASE "/x");
    err("Rename(missing)", r);
    r = Rename((STRPTR)BASE "/renamed", (STRPTR)BASE "/sub/moved");
    err("Rename into subdir", r);
    r = Rename((STRPTR)BASE "/sub", (STRPTR)BASE "/sub/inner2");
    err("Rename dir into itself", r);
    r = Rename((STRPTR)BASE "/file1", (STRPTR)BASE "/FILE1");
    err("Rename case only", r);
    r = DeleteFile((STRPTR)BASE "/sub");
    err("DeleteFile(non-empty dir)", r);
    r = DeleteFile((STRPTR)BASE "/missing");
    err("DeleteFile(missing)", r);
    r = SetProtection((STRPTR)BASE "/FILE1", FIBF_DELETE | FIBF_WRITE);
    err("SetProtection(no delete, no write)", r);
    l2 = Lock((STRPTR)BASE "/FILE1", SHARED_LOCK);
    Examine(l2, fib);
    P_HEX("fib_Protection", fib->fib_Protection);
    P_STR("fib_FileName (after case rename)", fib->fib_FileName);
    UnLock(l2);
    r = DeleteFile((STRPTR)BASE "/FILE1");
    err("DeleteFile(protected)", r);
    fh = Open((STRPTR)BASE "/FILE1", MODE_NEWFILE);
    err("Open NEWFILE (write-protected)", fh != 0);
    if (fh)
        Close(fh);
    fh = Open((STRPTR)BASE "/FILE1", MODE_OLDFILE);
    err("Open OLDFILE (write-protected)", fh != 0);
    if (fh)
        Close(fh);
    SetProtection((STRPTR)BASE "/FILE1", 0);
    r = SetComment((STRPTR)BASE "/FILE1", (STRPTR)"a comment");
    err("SetComment", r);
    l2 = Lock((STRPTR)BASE "/FILE1", SHARED_LOCK);
    Examine(l2, fib);
    P_STR("fib_Comment", fib->fib_Comment);
    UnLock(l2);
    r = SetComment((STRPTR)BASE "/FILE1",
                   (STRPTR)"0123456789012345678901234567890123456789012345678901234567890123456789012345678901");
    err("SetComment(80+ chars)", r);
    r = SetComment((STRPTR)BASE "/missing", (STRPTR)"x");
    err("SetComment(missing)", r);
    r = SetProtection((STRPTR)BASE "/missing", 0);
    err("SetProtection(missing)", r);

    UnLock(l1);
    cleanup();
    l1 = Lock((STRPTR)BASE, SHARED_LOCK);
    err("cleaned up (Lock fails)", l1 != 0);
    FreeDosObject(DOS_FIB, fib);
    return 0;
}
