/*
 * Filesystem Stress Test
 *
 * Tests filesystem operations under stress:
 *   1. Create/delete hundreds of files
 *   2. Read/write large files
 *   3. Directory traversal with many entries
 *   4. Multiple simultaneous file handles
 *   5. Lock stress testing
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

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
    char buf[32];
    char *p = buf;
    
    if (n < 0) {
        *p++ = '-';
        n = -n;
    }
    
    char tmp[16];
    int i = 0;
    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    while (i > 0) *p++ = tmp[--i];
    *p = '\0';
    
    print(buf);
}

static int tests_passed = 0;
static int tests_failed = 0;

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

static ULONG get_free_mem(void)
{
    return AvailMem(MEMF_ANY);
}

/* Generate filename for index */
static void make_filename(char *buf, int num)
{
    char *p = buf;
    char *prefix = "RAM:stress_test_";
    while (*prefix) *p++ = *prefix++;
    
    /* Add number */
    char tmp[16];
    int i = 0;
    if (num == 0) {
        tmp[i++] = '0';
    } else {
        while (num > 0) {
            tmp[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    while (i > 0) *p++ = tmp[--i];
    
    char *suffix = ".tmp";
    while (*suffix) *p++ = *suffix++;
    *p = '\0';
}

int main(void)
{
    ULONG mem_before, mem_after, mem_leaked;
    BPTR fh;
    char filename[64];
    int i, j;
    int failed;
    int created;
    int deleted;
    
    print("Filesystem Stress Test\n");
    print("======================\n\n");
    
    /* Ensure RAM: directory exists for tests */
    BPTR ram_lock = Lock((CONST_STRPTR)"RAM:", SHARED_LOCK);
    if (!ram_lock) {
        print("ERROR: RAM: device not available\n");
        return 20;
    }
    UnLock(ram_lock);
    
    /* Test 1: Create and delete 200 files */
    print("Test 1: Create/delete 200 files\n");
    
    mem_before = get_free_mem();
    failed = 0;
    created = 0;
    deleted = 0;
    
    /* Create files */
    for (i = 0; i < 200; i++) {
        make_filename(filename, i);
        fh = Open((CONST_STRPTR)filename, MODE_NEWFILE);
        if (fh) {
            /* Write a small amount of data */
            UBYTE data[32];
            for (j = 0; j < 32; j++) data[j] = (UBYTE)(i + j);
            Write(fh, data, 32);
            Close(fh);
            created++;
        } else {
            failed++;
        }
    }
    
    print("  Created: ");
    print_num(created);
    print(" files\n");
    
    /* Delete files */
    for (i = 0; i < 200; i++) {
        make_filename(filename, i);
        if (DeleteFile((CONST_STRPTR)filename)) {
            deleted++;
        }
    }
    
    print("  Deleted: ");
    print_num(deleted);
    print(" files\n");
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    /* Note: Some internal overhead expected per file operation */
    if (created >= 195 && deleted >= 195 && mem_leaked <= 16384) {
        test_pass("Create/delete 200 files");
    } else if (created < 195) {
        test_fail("Create/delete 200 files", "Too many creation failures");
    } else if (deleted < 195) {
        test_fail("Create/delete 200 files", "Too many deletion failures");
    } else {
        test_fail("Create/delete 200 files", "Significant memory leak");
    }
    
    /* Test 2: Large file write and read */
    print("\nTest 2: Large file I/O (16KB file)\n");
    
    mem_before = get_free_mem();
    
    UBYTE *buffer = (UBYTE *)AllocVec(4096, MEMF_PUBLIC);
    if (!buffer) {
        test_fail("Large file I/O", "Failed to allocate buffer");
    } else {
        int write_ok = 1;
        int read_ok = 1;
        int verify_ok = 1;
        
        /* Create large file */
        fh = Open((CONST_STRPTR)"RAM:large_test.tmp", MODE_NEWFILE);
        if (!fh) {
            test_fail("Large file I/O", "Failed to create file");
            write_ok = 0;
        } else {
            /* Write 4 x 4KB = 16KB */
            for (i = 0; i < 4 && write_ok; i++) {
                /* Fill buffer with pattern */
                for (j = 0; j < 4096; j++) {
                    buffer[j] = (UBYTE)((i * 4096 + j) & 0xFF);
                }
                
                LONG written = Write(fh, buffer, 4096);
                if (written != 4096) {
                    write_ok = 0;
                }
            }
            Close(fh);
        }
        
        print("  Wrote 16KB: ");
        print(write_ok ? "OK" : "FAILED");
        print("\n");
        
        /* Read and verify */
        if (write_ok) {
            fh = Open((CONST_STRPTR)"RAM:large_test.tmp", MODE_OLDFILE);
            if (!fh) {
                read_ok = 0;
            } else {
                for (i = 0; i < 4 && read_ok && verify_ok; i++) {
                    LONG bytes_read = Read(fh, buffer, 4096);
                    if (bytes_read != 4096) {
                        read_ok = 0;
                    } else {
                        /* Verify pattern */
                        for (j = 0; j < 4096; j++) {
                            UBYTE expected = (UBYTE)((i * 4096 + j) & 0xFF);
                            if (buffer[j] != expected) {
                                verify_ok = 0;
                                break;
                            }
                        }
                    }
                }
                Close(fh);
            }
        }
        
        print("  Read 16KB: ");
        print(read_ok ? "OK" : "FAILED");
        print("\n  Data verified: ");
        print(verify_ok ? "OK" : "FAILED");
        print("\n");
        
        /* Cleanup */
        DeleteFile((CONST_STRPTR)"RAM:large_test.tmp");
        FreeVec(buffer);
        
        mem_after = get_free_mem();
        
        if (mem_before >= mem_after) {
            mem_leaked = mem_before - mem_after;
        } else {
            mem_leaked = 0;
        }
        
        if (write_ok && read_ok && verify_ok && mem_leaked <= 4096) {
            test_pass("Large file I/O");
        } else if (!write_ok) {
            test_fail("Large file I/O", "Write failed");
        } else if (!read_ok) {
            test_fail("Large file I/O", "Read failed");
        } else if (!verify_ok) {
            test_fail("Large file I/O", "Data verification failed");
        } else {
            test_fail("Large file I/O", "Memory leak detected");
        }
    }
    
    /* Test 3: Multiple simultaneous file handles */
    print("\nTest 3: Multiple simultaneous handles (50 files)\n");
    
    mem_before = get_free_mem();
    
    BPTR handles[50];
    int handles_opened = 0;
    int handles_closed = 0;
    
    /* Create all files first */
    for (i = 0; i < 50; i++) {
        make_filename(filename, i + 1000);
        fh = Open((CONST_STRPTR)filename, MODE_NEWFILE);
        if (fh) {
            Write(fh, "test", 4);
            Close(fh);
        }
    }
    
    /* Open all files */
    for (i = 0; i < 50; i++) {
        make_filename(filename, i + 1000);
        handles[i] = Open((CONST_STRPTR)filename, MODE_OLDFILE);
        if (handles[i]) {
            handles_opened++;
        }
    }
    
    print("  Opened: ");
    print_num(handles_opened);
    print(" handles\n");
    
    /* Read from each */
    int read_success = 0;
    for (i = 0; i < 50; i++) {
        if (handles[i]) {
            UBYTE buf[8];
            LONG bytes = Read(handles[i], buf, 4);
            if (bytes == 4) {
                read_success++;
            }
        }
    }
    
    print("  Read success: ");
    print_num(read_success);
    print("\n");
    
    /* Close all handles */
    for (i = 0; i < 50; i++) {
        if (handles[i]) {
            Close(handles[i]);
            handles_closed++;
            handles[i] = 0;
        }
    }
    
    /* Delete test files */
    for (i = 0; i < 50; i++) {
        make_filename(filename, i + 1000);
        DeleteFile((CONST_STRPTR)filename);
    }
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    /* Note: Some internal overhead expected for file handles */
    if (handles_opened >= 45 && read_success >= 45 && mem_leaked <= 8192) {
        test_pass("Multiple simultaneous handles");
    } else if (handles_opened < 45) {
        test_fail("Multiple simultaneous handles", "Too many open failures");
    } else if (read_success < 45) {
        test_fail("Multiple simultaneous handles", "Too many read failures");
    } else {
        test_fail("Multiple simultaneous handles", "Significant memory leak");
    }
    
    /* Test 4: Lock stress test */
    print("\nTest 4: Lock stress (100 lock/unlock cycles)\n");
    
    mem_before = get_free_mem();
    
    int lock_success = 0;
    int unlock_success = 0;
    
    for (i = 0; i < 100; i++) {
        BPTR lock = Lock((CONST_STRPTR)"RAM:", SHARED_LOCK);
        if (lock) {
            lock_success++;
            UnLock(lock);
            unlock_success++;
        }
    }
    
    print("  Lock success: ");
    print_num(lock_success);
    print("\n  Unlock success: ");
    print_num(unlock_success);
    print("\n");
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    if (lock_success == 100 && unlock_success == 100 && mem_leaked <= 2048) {
        test_pass("Lock stress");
    } else if (lock_success < 100) {
        test_fail("Lock stress", "Lock failures");
    } else {
        test_fail("Lock stress", "Memory leak detected");
    }
    
    /* Test 5: Directory enumeration stress */
    print("\nTest 5: Directory enumeration (50 files)\n");
    
    /* Create 50 files in a test directory */
    CreateDir((CONST_STRPTR)"RAM:dir_stress");
    
    for (i = 0; i < 50; i++) {
        char path[80];
        char *p = path;
        char *prefix = "RAM:dir_stress/file_";
        while (*prefix) *p++ = *prefix++;
        
        char tmp[16];
        int num = i;
        int ti = 0;
        if (num == 0) {
            tmp[ti++] = '0';
        } else {
            while (num > 0) {
                tmp[ti++] = '0' + (num % 10);
                num /= 10;
            }
        }
        while (ti > 0) *p++ = tmp[--ti];
        
        char *suffix = ".txt";
        while (*suffix) *p++ = *suffix++;
        *p = '\0';
        
        fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
        if (fh) {
            Write(fh, "test", 4);
            Close(fh);
        }
    }
    
    /* Enumerate directory */
    BPTR lock = Lock((CONST_STRPTR)"RAM:dir_stress", SHARED_LOCK);
    if (!lock) {
        test_fail("Directory enumeration", "Failed to lock directory");
    } else {
        struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
        if (!fib) {
            test_fail("Directory enumeration", "Failed to allocate FIB");
            UnLock(lock);
        } else {
            int entries = 0;
            
            if (Examine(lock, fib)) {
                while (ExNext(lock, fib)) {
                    entries++;
                }
            }
            
            print("  Found ");
            print_num(entries);
            print(" entries\n");
            
            FreeDosObject(DOS_FIB, fib);
            UnLock(lock);
            
            if (entries >= 48) {  /* Allow 2 missing entries */
                test_pass("Directory enumeration");
            } else {
                test_fail("Directory enumeration", "Too few entries found");
            }
        }
    }
    
    /* Cleanup - delete all files and directory */
    for (i = 0; i < 50; i++) {
        char path[80];
        char *p = path;
        char *prefix = "RAM:dir_stress/file_";
        while (*prefix) *p++ = *prefix++;
        
        char tmp[16];
        int num = i;
        int ti = 0;
        if (num == 0) {
            tmp[ti++] = '0';
        } else {
            while (num > 0) {
                tmp[ti++] = '0' + (num % 10);
                num /= 10;
            }
        }
        while (ti > 0) *p++ = tmp[--ti];
        
        char *suffix = ".txt";
        while (*suffix) *p++ = *suffix++;
        *p = '\0';
        
        DeleteFile((CONST_STRPTR)path);
    }
    DeleteFile((CONST_STRPTR)"RAM:dir_stress");
    
    /* Test 6: Sequential write/read stress */
    print("\nTest 6: Sequential I/O stress (100 write/read cycles)\n");
    
    mem_before = get_free_mem();
    
    int io_success = 0;
    UBYTE write_buf[128];
    UBYTE read_buf[128];
    
    for (i = 0; i < 128; i++) {
        write_buf[i] = (UBYTE)i;
    }
    
    for (i = 0; i < 100; i++) {
        fh = Open((CONST_STRPTR)"RAM:io_stress.tmp", MODE_NEWFILE);
        if (!fh) continue;
        
        if (Write(fh, write_buf, 128) == 128) {
            Close(fh);
            
            fh = Open((CONST_STRPTR)"RAM:io_stress.tmp", MODE_OLDFILE);
            if (fh) {
                if (Read(fh, read_buf, 128) == 128) {
                    /* Quick verify */
                    int ok = 1;
                    for (j = 0; j < 128; j += 16) {
                        if (read_buf[j] != write_buf[j]) {
                            ok = 0;
                            break;
                        }
                    }
                    if (ok) io_success++;
                }
                Close(fh);
            }
        } else {
            Close(fh);
        }
    }
    
    DeleteFile((CONST_STRPTR)"RAM:io_stress.tmp");
    
    print("  Successful cycles: ");
    print_num(io_success);
    print(" of 100\n");
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    /* Note: Some internal overhead expected for repeated I/O */
    if (io_success >= 95 && mem_leaked <= 16384) {
        test_pass("Sequential I/O stress");
    } else if (io_success < 95) {
        test_fail("Sequential I/O stress", "Too many I/O failures");
    } else {
        test_fail("Sequential I/O stress", "Significant memory leak");
    }
    
    /* Summary */
    print("\n=== Filesystem Stress Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n\n");
    
    if (tests_failed == 0) {
        print("PASS: All filesystem stress tests passed\n");
    }
    
    return tests_failed > 0 ? 10 : 0;
}
