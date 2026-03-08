/*
 * Integration Test: File I/O edge cases
 *
 * Tests:
 *   - MODE_READWRITE (append mode)
 *   - Seek beyond EOF
 *   - Large file write/read
 *   - Write position after open modes
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/exall.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

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

int main(void)
{
    BPTR fh;
    LONG result;
    UBYTE buffer[128];
    const char *test_file = "fileio_advanced.txt";
    const char *test_file_abs = "SYS:Tests/Dos/fileio_advanced.txt";
    const char *exall_dir = "SYS:Tests/Dos/exall_dir";
    const char *exall_dir_nested = "SYS:Tests/Dos/exall_dir/nested";
    const char *exall_file = "SYS:Tests/Dos/exall_dir/sample.txt";
    const char *soft_link = "SYS:Tests/Dos/soft_link_test.lnk";
    const char *hard_link = "SYS:Tests/Dos/hard_link_test.txt";
    
    print("File I/O Edge Cases Test\n");
    print("========================\n\n");
    
    /* Test 1: Create initial file with MODE_NEWFILE */
    print("Test 1: Create initial file\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_NEWFILE);
    if (!fh) {
        test_fail("Create file", "Open failed");
        return 1;
    }
    
    result = Write(fh, (CONST APTR)"Hello", 5);
    if (result != 5) {
        test_fail("Create file", "Write failed");
        Close(fh);
        return 1;
    }
    Close(fh);
    test_pass("Create file with 5 bytes");
    
    /* Test 2: MODE_READWRITE - should open existing, position at start */
    print("\nTest 2: MODE_READWRITE (open existing)\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_READWRITE);
    if (!fh) {
        test_fail("MODE_READWRITE open", "Open failed");
        return 1;
    }
    
    /* Check position is at beginning */
    result = Seek(fh, 0, OFFSET_CURRENT);
    if (result == 0) {
        test_pass("MODE_READWRITE pos at start");
    } else {
        print("  Position: ");
        print_num(result);
        print("\n");
        test_fail("MODE_READWRITE pos at start", "Wrong position");
    }
    
    /* Read should get our data */
    result = Read(fh, buffer, 5);
    if (result == 5 && buffer[0] == 'H' && buffer[4] == 'o') {
        test_pass("MODE_READWRITE read existing data");
    } else {
        test_fail("MODE_READWRITE read", "Data mismatch");
    }
    
    /* Seek to end and write more */
    Seek(fh, 0, OFFSET_END);
    result = Write(fh, (CONST APTR)"World", 5);
    if (result == 5) {
        test_pass("MODE_READWRITE write at end");
    } else {
        test_fail("MODE_READWRITE write at end", "Write failed");
    }
    
    Close(fh);
    
    /* Test 3: Verify file has both parts */
    print("\nTest 3: Verify combined content\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_OLDFILE);
    if (!fh) {
        test_fail("Verify content", "Open failed");
        return 1;
    }
    
    result = Read(fh, buffer, 20);
    if (result == 10) {
        buffer[10] = '\0';
        print("  Content: '");
        print((char *)buffer);
        print("'\n");
        if (buffer[0] == 'H' && buffer[5] == 'W' && buffer[9] == 'd') {
            test_pass("Content is 'HelloWorld'");
        } else {
            test_fail("Content check", "Wrong content");
        }
    } else {
        print("  Read returned ");
        print_num(result);
        print(" bytes\n");
        test_fail("Content check", "Wrong size");
    }
    Close(fh);
    
    /* Test 4: Seek beyond EOF */
    print("\nTest 4: Seek beyond EOF\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_OLDFILE);
    if (!fh) {
        test_fail("Seek beyond EOF", "Open failed");
        return 1;
    }
    
    /* Try to seek way past EOF */
    result = Seek(fh, 1000, OFFSET_BEGINNING);
    print("  Seek(1000) returned: ");
    print_num(result);
    print("\n");
    
    /* Read should return 0 (at/past EOF) */
    result = Read(fh, buffer, 10);
    print("  Read returned: ");
    print_num(result);
    print(" bytes\n");
    
    if (result == 0) {
        test_pass("Read at EOF returns 0");
    } else if (result == -1) {
        test_pass("Read past EOF returns error");
    } else {
        test_fail("Read past EOF", "Unexpected result");
    }
    
    Close(fh);
    
    /* Test 5: Large write/read */
    print("\nTest 5: Large write/read (1KB)\n");
    
    fh = Open((CONST_STRPTR)"large_test.dat", MODE_NEWFILE);
    if (!fh) {
        test_fail("Large write", "Open failed");
        return 1;
    }
    
    /* Write 1KB of data (pattern: 0,1,2,...,255,0,1,...) */
    {
        UBYTE write_buf[256];
        LONG total_written = 0;
        int i, j;
        
        for (i = 0; i < 256; i++) {
            write_buf[i] = (UBYTE)i;
        }
        
        for (j = 0; j < 4; j++) {
            result = Write(fh, write_buf, 256);
            if (result != 256) {
                print("  Write failed at iteration ");
                print_num(j);
                print("\n");
                break;
            }
            total_written += result;
        }
        
        print("  Wrote ");
        print_num(total_written);
        print(" bytes\n");
        
        if (total_written == 1024) {
            test_pass("Large write (1024 bytes)");
        } else {
            test_fail("Large write", "Wrong size");
        }
    }
    Close(fh);
    
    /* Verify by reading back */
    fh = Open((CONST_STRPTR)"large_test.dat", MODE_OLDFILE);
    if (!fh) {
        test_fail("Large read", "Open failed");
    } else {
        LONG total_read = 0;
        BOOL data_ok = TRUE;
        int i;
        
        while (1) {
            result = Read(fh, buffer, 128);
            if (result <= 0) break;
            
            for (i = 0; i < result; i++) {
                UBYTE expected = (UBYTE)((total_read + i) & 0xFF);
                if (buffer[i] != expected) {
                    data_ok = FALSE;
                    print("  Mismatch at byte ");
                    print_num(total_read + i);
                    print(": got ");
                    print_num(buffer[i]);
                    print(" expected ");
                    print_num(expected);
                    print("\n");
                    break;
                }
            }
            
            total_read += result;
            if (!data_ok) break;
        }
        
        print("  Read ");
        print_num(total_read);
        print(" bytes\n");
        
        if (total_read == 1024 && data_ok) {
            test_pass("Large read with data verification");
        } else {
            test_fail("Large read", "Size or data mismatch");
        }
        Close(fh);
    }
    
    /* Test 6: SetFileSize truncate/extend semantics */
    print("\nTest 6: SetFileSize semantics\n");

    fh = Open((CONST_STRPTR)test_file, MODE_READWRITE);
    if (!fh) {
        test_fail("SetFileSize open", "Open failed");
        return 1;
    }

    result = SetFileSize(fh, 4, OFFSET_BEGINNING);
    if (result == 4) {
        test_pass("SetFileSize truncate from beginning");
    } else {
        print("  SetFileSize begin returned ");
        print_num(result);
        print("\n");
        test_fail("SetFileSize truncate from beginning", "Wrong return value");
    }

    Seek(fh, 0, OFFSET_BEGINNING);
    result = Read(fh, buffer, 16);
    if (result == 4 && buffer[0] == 'H' && buffer[3] == 'l') {
        test_pass("SetFileSize truncated file contents");
    } else {
        test_fail("SetFileSize truncated file contents", "Unexpected truncated data");
    }

    result = Seek(fh, 2, OFFSET_BEGINNING);
    if (result < 0) {
        test_fail("SetFileSize seek before current-mode resize", "Seek failed");
    } else {
        result = SetFileSize(fh, 3, OFFSET_CURRENT);
        if (result == 5) {
            test_pass("SetFileSize resize from current position");
        } else {
            print("  SetFileSize current returned ");
            print_num(result);
            print("\n");
            test_fail("SetFileSize resize from current position", "Wrong return value");
        }
    }

    result = SetFileSize(fh, 3, OFFSET_END);
    if (result == 8) {
        test_pass("SetFileSize extend from end");
    } else {
        print("  SetFileSize end returned ");
        print_num(result);
        print("\n");
        test_fail("SetFileSize extend from end", "Wrong return value");
    }

    Close(fh);

    fh = Open((CONST_STRPTR)test_file, MODE_OLDFILE);
    if (!fh) {
        test_fail("SetFileSize verify reopen", "Open failed");
        return 1;
    }

    result = Read(fh, buffer, 16);
    if (result == 8 && buffer[0] == 'H' && buffer[1] == 'e' && buffer[2] == 'l' && buffer[3] == 'l' &&
        buffer[4] == 0 && buffer[5] == 0 && buffer[6] == 0 && buffer[7] == 0) {
        test_pass("SetFileSize extension zero-fills");
    } else {
        test_fail("SetFileSize extension zero-fills", "Unexpected extended data");
    }
    Close(fh);

    /* Test 6b: SetFileDate updates Examine datestamp */
    print("\nTest 6b: SetFileDate semantics\n");
    {
        struct DateStamp ds;
        struct FileInfoBlock fib;
        BPTR lock;

        DateStamp(&ds);
        if (ds.ds_Tick >= 10) {
            ds.ds_Tick -= 10;
        } else if (ds.ds_Minute > 0) {
            ds.ds_Minute--;
            ds.ds_Tick = 2990;
        } else if (ds.ds_Days > 0) {
            ds.ds_Days--;
            ds.ds_Minute = 1439;
            ds.ds_Tick = 2990;
        }

        result = SetFileDate((CONST_STRPTR)test_file_abs, &ds);
        if (result) {
            test_pass("SetFileDate returns success");
        } else {
            test_fail("SetFileDate returns success", "Call failed");
        }

        Delay(2);

        lock = Lock((CONST_STRPTR)test_file_abs, SHARED_LOCK);
        if (!lock) {
            test_fail("SetFileDate verify lock", "Lock failed");
        } else {
            if (Examine(lock, &fib) &&
                fib.fib_Date.ds_Days == ds.ds_Days &&
                fib.fib_Date.ds_Minute == ds.ds_Minute &&
                fib.fib_Date.ds_Tick == ds.ds_Tick) {
                test_pass("SetFileDate updates Examine datestamp");
            } else {
                test_fail("SetFileDate updates Examine datestamp", "Date mismatch");
            }
            UnLock(lock);
        }
    }

    /* Test 6c: SetComment, SetProtection, Info, SameLock */
    print("\nTest 6c: Metadata and lock helpers\n");
    {
        struct FileInfoBlock fib;
        struct InfoData info;
        BPTR file_lock;
        BPTR dup_lock;
        BPTR dir_lock;
        LONG same_result;

        file_lock = Lock((CONST_STRPTR)test_file_abs, SHARED_LOCK);
        if (!file_lock) {
            test_fail("Metadata helpers setup", "Lock failed");
        } else {
            dup_lock = DupLock(file_lock);
            same_result = dup_lock ? SameLock(file_lock, dup_lock) : LOCK_DIFFERENT;
            if (dup_lock && same_result == LOCK_SAME) {
                test_pass("SameLock identifies duplicate lock");
            } else {
                test_fail("SameLock identifies duplicate lock", "Wrong lock comparison result");
            }

            dir_lock = Lock((CONST_STRPTR)"SYS:Tests/Dos", SHARED_LOCK);
            same_result = dir_lock ? SameLock(file_lock, dir_lock) : LOCK_DIFFERENT;
            if (dir_lock && (same_result == LOCK_SAME_VOLUME || same_result == LOCK_SAME)) {
                test_pass("SameLock reports same volume");
            } else {
                test_fail("SameLock reports same volume", "Expected same-volume result");
            }

            if (dir_lock)
                UnLock(dir_lock);

            if (Info(file_lock, &info) && info.id_BytesPerBlock > 0 && info.id_NumBlocks >= info.id_NumBlocksUsed) {
                test_pass("Info returns volume statistics");
            } else {
                test_fail("Info returns volume statistics", "Unexpected InfoData contents");
            }

            if (SetComment((CONST_STRPTR)test_file_abs, (CONST_STRPTR)"phase78b7") &&
                Examine(file_lock, &fib) &&
                strcmp((const char *)fib.fib_Comment, "phase78b7") == 0) {
                test_pass("SetComment updates fib_Comment");
            } else {
                test_fail("SetComment updates fib_Comment", "Comment mismatch");
            }

            if (SetProtection((CONST_STRPTR)test_file_abs,
                              FIBF_WRITE | FIBF_DELETE |
                              FIBF_GRP_WRITE | FIBF_GRP_DELETE |
                              FIBF_OTR_WRITE | FIBF_OTR_DELETE) &&
                Examine(file_lock, &fib) &&
                (fib.fib_Protection & (FIBF_WRITE | FIBF_DELETE | FIBF_GRP_WRITE | FIBF_GRP_DELETE | FIBF_OTR_WRITE | FIBF_OTR_DELETE)) ==
                (FIBF_WRITE | FIBF_DELETE | FIBF_GRP_WRITE | FIBF_GRP_DELETE | FIBF_OTR_WRITE | FIBF_OTR_DELETE)) {
                test_pass("SetProtection updates fib_Protection");
            } else {
                test_fail("SetProtection updates fib_Protection", "Protection bits mismatch");
            }

            SetProtection((CONST_STRPTR)test_file_abs, 0);
            SetComment((CONST_STRPTR)test_file_abs, (CONST_STRPTR)"");

            if (dup_lock)
                UnLock(dup_lock);
            UnLock(file_lock);
        }
    }

    /* Test 6d: ExAll enumeration and filtering */
    print("\nTest 6d: ExAll enumeration\n");
    {
        BPTR dir_lock;
        struct ExAllControl *eac;
        UBYTE exall_buf[512];
        UBYTE pattern_buf[128];
        struct ExAllData *ead;
        LONG total_entries = 0;
        LONG exall_result;
        BOOL saw_file = FALSE;
        BOOL saw_dir = FALSE;

        DeleteFile((CONST_STRPTR)exall_file);
        DeleteFile((CONST_STRPTR)exall_dir_nested);
        DeleteFile((CONST_STRPTR)exall_dir);

        dir_lock = CreateDir((CONST_STRPTR)exall_dir);
        if (!dir_lock) {
            test_fail("ExAll setup dir", "CreateDir failed");
        } else {
            UnLock(dir_lock);
            dir_lock = CreateDir((CONST_STRPTR)exall_dir_nested);
            if (dir_lock) {
                UnLock(dir_lock);
            }

            fh = Open((CONST_STRPTR)exall_file, MODE_NEWFILE);
            if (fh) {
                Write(fh, (CONST APTR)"sample", 6);
                Close(fh);
            }

            dir_lock = Lock((CONST_STRPTR)exall_dir, SHARED_LOCK);
            eac = (struct ExAllControl *)AllocDosObject(DOS_EXALLCONTROL, NULL);
            if (!dir_lock || !eac) {
                test_fail("ExAll setup", "Lock or AllocDosObject failed");
            } else {
                do {
                    exall_result = ExAll(dir_lock, (struct ExAllData *)exall_buf, sizeof(exall_buf), ED_OWNER, eac);
                    ead = (struct ExAllData *)exall_buf;
                    while (ead && eac->eac_Entries > 0) {
                        total_entries++;
                        if (ead->ed_Name && strcmp((const char *)ead->ed_Name, "sample.txt") == 0 && ead->ed_Type < 0 && ead->ed_Size == 6) {
                            saw_file = TRUE;
                        }
                        if (ead->ed_Name && strcmp((const char *)ead->ed_Name, "nested") == 0 && ead->ed_Type > 0) {
                            saw_dir = TRUE;
                        }
                        ead = ead->ed_Next;
                    }
                } while (exall_result);

                if (IoErr() == ERROR_NO_MORE_ENTRIES && total_entries >= 2 && saw_file && saw_dir) {
                    test_pass("ExAll returns file and directory entries");
                } else {
                    print("  Total entries: ");
                    print_num(total_entries);
                    print(", IoErr: ");
                    print_num(IoErr());
                    print("\n");
                    test_fail("ExAll returns file and directory entries", "Unexpected enumeration results");
                }

                ExAllEnd(dir_lock, (struct ExAllData *)exall_buf, sizeof(exall_buf), ED_OWNER, eac);
                if (ParsePatternNoCase((CONST_STRPTR)"sample.txt", pattern_buf, sizeof(pattern_buf)) != 0) {
                    eac->eac_MatchString = (STRPTR)pattern_buf;
                }

                result = ExAll(dir_lock, (struct ExAllData *)exall_buf, sizeof(exall_buf), ED_NAME, eac);
                ead = (struct ExAllData *)exall_buf;

                if (eac->eac_Entries == 1 && ead && ead->ed_Name && strcmp((const char *)ead->ed_Name, "sample.txt") == 0) {
                    test_pass("ExAll match string filters entries");
                } else {
                    print("  Filter result=");
                    print_num(result);
                    print(", entries=");
                    print_num(eac->eac_Entries);
                    print(", IoErr=");
                    print_num(IoErr());
                    print("\n");
                    test_fail("ExAll match string filters entries", "Filter call failed");
                }

                ExAllEnd(dir_lock, (struct ExAllData *)exall_buf, sizeof(exall_buf), ED_NAME, eac);
                FreeDosObject(DOS_EXALLCONTROL, eac);
                UnLock(dir_lock);
            }

            DeleteFile((CONST_STRPTR)exall_file);
            DeleteFile((CONST_STRPTR)exall_dir_nested);
            DeleteFile((CONST_STRPTR)exall_dir);
        }
    }

    /* Test 6e: MakeLink/ReadLink soft links */
    print("\nTest 6e: MakeLink and ReadLink\n");
    {
        char link_target[128];
        char hard_link_data[16];
        BPTR target_lock;

        DeleteFile((CONST_STRPTR)soft_link);
        DeleteFile((CONST_STRPTR)hard_link);
        Delay(2);
        result = MakeLink((CONST_STRPTR)soft_link, (LONG)(CONST_STRPTR)test_file_abs, LINK_SOFT);
        if (result) {
            test_pass("MakeLink creates soft link");
        } else {
            print("  IoErr: ");
            print_num(IoErr());
            print("\n");
            test_fail("MakeLink creates soft link", "MakeLink failed");
        }

        result = ReadLink(NULL, 0, (CONST_STRPTR)soft_link, (STRPTR)link_target, sizeof(link_target));
        if (result >= 0 && strcmp(link_target, test_file_abs) == 0) {
            test_pass("ReadLink returns soft-link target");
        } else {
            print("  ReadLink result=");
            print_num(result);
            print(", target='");
            print(link_target);
            print("', IoErr=");
            print_num(IoErr());
            print("\n");
            test_fail("ReadLink returns soft-link target", "Unexpected target");
        }

        target_lock = Lock((CONST_STRPTR)test_file_abs, SHARED_LOCK);
        if (!target_lock) {
            test_fail("MakeLink hard-link setup", "Lock failed");
        } else {
            result = MakeLink((CONST_STRPTR)hard_link, (LONG)target_lock, LINK_HARD);
            if (result) {
                test_pass("MakeLink creates hard link");
            } else {
                print("  Hard-link IoErr: ");
                print_num(IoErr());
                print("\n");
                test_fail("MakeLink creates hard link", "MakeLink failed");
            }

            UnLock(target_lock);
        }

        fh = Open((CONST_STRPTR)hard_link, MODE_OLDFILE);
        if (!fh) {
            test_fail("MakeLink hard link readable", "Open failed");
        } else {
            result = Read(fh, hard_link_data, sizeof(hard_link_data));
            Close(fh);
            if (result == 8 &&
                hard_link_data[0] == 'H' &&
                hard_link_data[1] == 'e' &&
                hard_link_data[2] == 'l' &&
                hard_link_data[3] == 'l' &&
                hard_link_data[4] == 0 &&
                hard_link_data[5] == 0 &&
                hard_link_data[6] == 0 &&
                hard_link_data[7] == 0) {
                test_pass("MakeLink hard link preserves file data");
            } else {
                test_fail("MakeLink hard link preserves file data", "Unexpected hard-link contents");
            }
        }

        DeleteFile((CONST_STRPTR)soft_link);
        DeleteFile((CONST_STRPTR)hard_link);
    }

    /* Test 7: MODE_NEWFILE truncates existing */
    print("\nTest 7: MODE_NEWFILE truncates existing\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_NEWFILE);
    if (!fh) {
        test_fail("Truncate test", "Open failed");
        return 1;
    }
    
    result = Write(fh, (CONST APTR)"AB", 2);
    Close(fh);
    
    /* Verify file is now only 2 bytes */
    fh = Open((CONST_STRPTR)test_file, MODE_OLDFILE);
    if (fh) {
        Seek(fh, 0, OFFSET_END);
        result = Seek(fh, 0, OFFSET_CURRENT);
        print("  File size now: ");
        print_num(result);
        print(" bytes\n");
        
        if (result == 2) {
            test_pass("MODE_NEWFILE truncates");
        } else {
            test_fail("MODE_NEWFILE truncates", "File not truncated");
        }
        Close(fh);
    } else {
        test_fail("Truncate verify", "Open failed");
    }
    
    /* Cleanup */
    print("\nCleanup\n");
    DeleteFile((CONST_STRPTR)test_file);
    DeleteFile((CONST_STRPTR)"large_test.dat");
    print("  Files deleted\n");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
