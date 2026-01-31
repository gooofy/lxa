/*
 * PROTECT command - Change file protection bits
 * Phase 9.2 implementation for lxa
 * 
 * Template: FILE/A,FLAGS,ADD/S,SUB/S,ALL/S,QUIET/S
 * 
 * Features:
 *   - Set protection bits (FLAGS)
 *   - Add protection bits (ADD)
 *   - Remove protection bits (SUB)
 *   - Recursive directory processing (ALL)
 *   - Quiet mode (QUIET)
 *   - Ctrl+C break handling
 * 
 * Protection bits: HSPARWED
 *   H = Hold (pure, keep resident)
 *   S = Script
 *   P = Pure (reentrant)
 *   A = Archive
 *   R = Read
 *   W = Write
 *   E = Execute
 *   D = Delete
 * 
 * Note: In AmigaDOS, R/W/E/D bits are inverted (0 = permission granted)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "FILE/A,FLAGS,ADD/S,SUB/S,ALL/S,QUIET/S"

/* Argument array indices */
#define ARG_FILE    0
#define ARG_FLAGS   1
#define ARG_ADD     2
#define ARG_SUB     3
#define ARG_ALL     4
#define ARG_QUIET   5
#define ARG_COUNT   6

/* Buffer sizes */
#define MAX_PATH_LEN 512
#define PATTERN_BUFFER_SIZE 256

/* Protection bit definitions - use system headers */
/* FIBF_* defined in dos/dos.h */

/* Global state */
static BOOL g_user_break = FALSE;
static BOOL g_quiet = FALSE;
static int g_files_processed = 0;
static int g_errors = 0;

/* Helper: check for Ctrl+C break */
static BOOL check_break(void)
{
    if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
        g_user_break = TRUE;
        return TRUE;
    }
    return g_user_break;
}

/* Helper: output a string */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

/* Helper: output newline */
static void out_nl(void)
{
    Write(Output(), (STRPTR)"\n", 1);
}

/* Parse protection flags string and return bit mask */
static LONG parse_flags(const char *flags, BOOL *valid)
{
    LONG mask = 0;
    *valid = TRUE;
    
    if (!flags || flags[0] == '\0') {
        *valid = FALSE;
        return 0;
    }
    
    const char *p = flags;
    while (*p) {
        char c = *p;
        /* Convert to uppercase */
        if (c >= 'a' && c <= 'z') c -= 32;
        
        switch (c) {
            case 'H': mask |= FIBF_HOLD;    break;
            case 'S': mask |= FIBF_SCRIPT;  break;
            case 'P': mask |= FIBF_PURE;    break;
            case 'A': mask |= FIBF_ARCHIVE; break;
            case 'R': mask |= FIBF_READ;    break;
            case 'W': mask |= FIBF_WRITE;   break;
            case 'E': mask |= FIBF_EXECUTE; break;
            case 'D': mask |= FIBF_DELETE;  break;
            case '+':
            case '-':
                /* These are sometimes used as prefixes, ignore */
                break;
            default:
                *valid = FALSE;
                return 0;
        }
        p++;
    }
    
    return mask;
}

/* Format protection bits as string */
static void format_protection(LONG protection, char *buf)
{
    /* HSPARWED format */
    buf[0] = (protection & FIBF_HOLD)    ? 'h' : '-';
    buf[1] = (protection & FIBF_SCRIPT)  ? 's' : '-';
    buf[2] = (protection & FIBF_PURE)    ? 'p' : '-';
    buf[3] = (protection & FIBF_ARCHIVE) ? 'a' : '-';
    /* Note: R/W/E/D bits are inverted in Amiga (0 = permission granted) */
    buf[4] = !(protection & FIBF_READ)    ? 'r' : '-';
    buf[5] = !(protection & FIBF_WRITE)   ? 'w' : '-';
    buf[6] = !(protection & FIBF_EXECUTE) ? 'e' : '-';
    buf[7] = !(protection & FIBF_DELETE)  ? 'd' : '-';
    buf[8] = '\0';
}

/* Process a single file */
static int process_file(const char *path, LONG new_mask, BOOL add_mode, BOOL sub_mode)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    LONG current_prot;
    LONG final_prot;
    
    if (check_break()) {
        return 1;
    }
    
    /* Lock the file to get current protection */
    lock = Lock((STRPTR)path, SHARED_LOCK);
    if (!lock) {
        if (!g_quiet) {
            out_str("PROTECT: Can't access '");
            out_str(path);
            out_str("'\n");
        }
        g_errors++;
        return 1;
    }
    
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        if (!g_quiet) {
            out_str("PROTECT: Out of memory\n");
        }
        g_errors++;
        return 1;
    }
    
    if (!Examine(lock, fib)) {
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        if (!g_quiet) {
            out_str("PROTECT: Can't examine '");
            out_str(path);
            out_str("'\n");
        }
        g_errors++;
        return 1;
    }
    
    current_prot = fib->fib_Protection;
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    
    /* Calculate new protection based on mode */
    if (add_mode) {
        /* ADD: Set the specified bits (for HSPA, this means setting them;
           for RWED, since they're inverted, we need to clear them to grant permission) */
        /* Actually, ADD means "add these permissions", so for RWED we clear the bits */
        LONG hspa_mask = new_mask & (FIBF_HOLD | FIBF_SCRIPT | FIBF_PURE | FIBF_ARCHIVE);
        LONG rwed_mask = new_mask & (FIBF_READ | FIBF_WRITE | FIBF_EXECUTE | FIBF_DELETE);
        
        final_prot = current_prot | hspa_mask;  /* Set HSPA bits */
        final_prot = final_prot & ~rwed_mask;   /* Clear RWED bits (grant permission) */
    } else if (sub_mode) {
        /* SUB: Remove the specified permissions */
        LONG hspa_mask = new_mask & (FIBF_HOLD | FIBF_SCRIPT | FIBF_PURE | FIBF_ARCHIVE);
        LONG rwed_mask = new_mask & (FIBF_READ | FIBF_WRITE | FIBF_EXECUTE | FIBF_DELETE);
        
        final_prot = current_prot & ~hspa_mask; /* Clear HSPA bits */
        final_prot = final_prot | rwed_mask;    /* Set RWED bits (deny permission) */
    } else {
        /* Set mode: directly set all bits */
        /* For the flags specified, we need to handle the RWED inversion */
        /* When user says "rwed", they mean grant all permissions, so clear those bits */
        LONG hspa_mask = new_mask & (FIBF_HOLD | FIBF_SCRIPT | FIBF_PURE | FIBF_ARCHIVE);
        LONG rwed_mask = new_mask & (FIBF_READ | FIBF_WRITE | FIBF_EXECUTE | FIBF_DELETE);
        
        /* Start with all RWED denied (bits set), then clear the ones user specified */
        final_prot = (FIBF_READ | FIBF_WRITE | FIBF_EXECUTE | FIBF_DELETE);
        final_prot = final_prot & ~rwed_mask;  /* Grant specified RWED permissions */
        final_prot = final_prot | hspa_mask;   /* Set specified HSPA flags */
    }
    
    /* Set the new protection */
    if (!SetProtection((STRPTR)path, final_prot)) {
        if (!g_quiet) {
            out_str("PROTECT: Can't set protection on '");
            out_str(path);
            out_str("'\n");
        }
        g_errors++;
        return 1;
    }
    
    /* Display result if not quiet */
    if (!g_quiet) {
        char prot_str[16];
        format_protection(final_prot, prot_str);
        out_str("  ");
        out_str(path);
        out_str(": ");
        out_str(prot_str);
        out_nl();
    }
    
    g_files_processed++;
    return 0;
}

/* Process directory recursively */
static int process_directory(const char *path, const char *pattern, 
                             LONG new_mask, BOOL add_mode, BOOL sub_mode)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char subpath[MAX_PATH_LEN];
    char parsed_pattern[PATTERN_BUFFER_SIZE];
    LONG pattern_type;
    
    if (check_break()) {
        return 1;
    }
    
    /* Parse the pattern */
    pattern_type = ParsePattern((STRPTR)pattern, (STRPTR)parsed_pattern, PATTERN_BUFFER_SIZE);
    
    lock = Lock((STRPTR)path, SHARED_LOCK);
    if (!lock) {
        if (!g_quiet) {
            out_str("PROTECT: Can't access directory '");
            out_str(path);
            out_str("'\n");
        }
        g_errors++;
        return 1;
    }
    
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return 1;
    }
    
    if (!Examine(lock, fib)) {
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        return 1;
    }
    
    while (ExNext(lock, fib)) {
        if (check_break()) {
            out_str("***BREAK\n");
            break;
        }
        
        /* Build full path */
        strncpy(subpath, path, MAX_PATH_LEN - 2);
        int len = strlen(subpath);
        if (len > 0 && subpath[len-1] != ':' && subpath[len-1] != '/') {
            strcat(subpath, "/");
        }
        strncat(subpath, (char *)fib->fib_FileName, MAX_PATH_LEN - strlen(subpath) - 1);
        subpath[MAX_PATH_LEN - 1] = '\0';
        
        if (fib->fib_DirEntryType > 0) {
            /* Directory - recurse */
            process_directory(subpath, pattern, new_mask, add_mode, sub_mode);
        } else {
            /* File - check pattern match */
            BOOL matches = FALSE;
            if (pattern_type == 0) {
                /* Literal pattern */
                matches = (stricmp((char *)fib->fib_FileName, pattern) == 0);
            } else if (pattern_type > 0) {
                /* Wildcard pattern */
                matches = MatchPattern((STRPTR)parsed_pattern, fib->fib_FileName);
            } else {
                /* Pattern parse error - match all */
                matches = TRUE;
            }
            
            if (matches) {
                process_file(subpath, new_mask, add_mode, sub_mode);
            }
        }
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    
    return 0;
}

/* stricmp is provided by string.h - use it for pattern matching */

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int result = 0;
    
    (void)argc;
    (void)argv;
    
    /* Clear state */
    g_user_break = FALSE;
    g_quiet = FALSE;
    g_files_processed = 0;
    g_errors = 0;
    SetSignal(0L, SIGBREAKF_CTRL_C);
    
    /* Parse arguments */
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        out_str("PROTECT: Bad arguments\n");
        out_str("Usage: PROTECT <file|pattern> [FLAGS] [ADD] [SUB] [ALL] [QUIET]\n");
        out_str("Flags: H=Hold S=Script P=Pure A=Archive R=Read W=Write E=Execute D=Delete\n");
        return 20;
    }
    
    const char *file_arg = (const char *)args[ARG_FILE];
    const char *flags_arg = (const char *)args[ARG_FLAGS];
    BOOL add_mode = args[ARG_ADD] ? TRUE : FALSE;
    BOOL sub_mode = args[ARG_SUB] ? TRUE : FALSE;
    BOOL all_mode = args[ARG_ALL] ? TRUE : FALSE;
    g_quiet = args[ARG_QUIET] ? TRUE : FALSE;
    
    /* ADD and SUB are mutually exclusive */
    if (add_mode && sub_mode) {
        out_str("PROTECT: ADD and SUB are mutually exclusive\n");
        FreeArgs(rda);
        return 20;
    }
    
    /* If no flags specified, just show current protection */
    if (!flags_arg || flags_arg[0] == '\0') {
        /* Show protection for file */
        BPTR lock = Lock((STRPTR)file_arg, SHARED_LOCK);
        if (!lock) {
            out_str("PROTECT: Can't access '");
            out_str(file_arg);
            out_str("'\n");
            FreeArgs(rda);
            return 20;
        }
        
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        if (fib) {
            if (Examine(lock, fib)) {
                char prot_str[16];
                format_protection(fib->fib_Protection, prot_str);
                out_str("  ");
                out_str(file_arg);
                out_str(": ");
                out_str(prot_str);
                out_nl();
            }
            FreeDosObject(DOS_FIB, fib);
        }
        UnLock(lock);
        FreeArgs(rda);
        return 0;
    }
    
    /* Parse the flags */
    BOOL valid;
    LONG flag_mask = parse_flags(flags_arg, &valid);
    if (!valid) {
        out_str("PROTECT: Invalid protection flags '");
        out_str(flags_arg);
        out_str("'\n");
        out_str("Valid flags: H S P A R W E D\n");
        FreeArgs(rda);
        return 20;
    }
    
    /* Check if file_arg is a directory or contains wildcards */
    BPTR test_lock = Lock((STRPTR)file_arg, SHARED_LOCK);
    if (test_lock) {
        struct FileInfoBlock *test_fib = AllocDosObject(DOS_FIB, NULL);
        if (test_fib && Examine(test_lock, test_fib)) {
            if (test_fib->fib_DirEntryType > 0) {
                /* It's a directory */
                if (all_mode) {
                    /* Process all files in directory recursively */
                    FreeDosObject(DOS_FIB, test_fib);
                    UnLock(test_lock);
                    result = process_directory(file_arg, "#?", flag_mask, add_mode, sub_mode);
                } else {
                    /* Just process the directory itself */
                    FreeDosObject(DOS_FIB, test_fib);
                    UnLock(test_lock);
                    result = process_file(file_arg, flag_mask, add_mode, sub_mode);
                }
            } else {
                /* It's a file */
                FreeDosObject(DOS_FIB, test_fib);
                UnLock(test_lock);
                result = process_file(file_arg, flag_mask, add_mode, sub_mode);
            }
        } else {
            if (test_fib) FreeDosObject(DOS_FIB, test_fib);
            UnLock(test_lock);
            result = 10;
        }
    } else {
        /* Doesn't exist as-is - might be a pattern */
        /* Find the directory part and pattern part */
        const char *p = file_arg;
        const char *last_sep = NULL;
        while (*p) {
            if (*p == '/' || *p == ':') last_sep = p;
            p++;
        }
        
        char dir_path[MAX_PATH_LEN];
        const char *pattern;
        
        if (last_sep) {
            int dir_len = last_sep - file_arg + 1;
            strncpy(dir_path, file_arg, dir_len);
            dir_path[dir_len] = '\0';
            pattern = last_sep + 1;
        } else {
            strcpy(dir_path, "");
            pattern = file_arg;
        }
        
        /* Check if pattern contains wildcards */
        char parsed_pattern[PATTERN_BUFFER_SIZE];
        LONG pattern_type = ParsePattern((STRPTR)pattern, (STRPTR)parsed_pattern, PATTERN_BUFFER_SIZE);
        
        if (pattern_type > 0) {
            /* Has wildcards - process matching files */
            BPTR dir_lock;
            if (dir_path[0]) {
                dir_lock = Lock((STRPTR)dir_path, SHARED_LOCK);
            } else {
                BPTR cur = CurrentDir(0);
                CurrentDir(cur);
                dir_lock = cur ? DupLock(cur) : Lock((STRPTR)"", SHARED_LOCK);
            }
            
            if (dir_lock) {
                struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
                if (fib && Examine(dir_lock, fib)) {
                    while (ExNext(dir_lock, fib)) {
                        if (check_break()) {
                            out_str("***BREAK\n");
                            break;
                        }
                        
                        if (MatchPattern((STRPTR)parsed_pattern, fib->fib_FileName)) {
                            char full_path[MAX_PATH_LEN];
                            if (dir_path[0]) {
                                strncpy(full_path, dir_path, MAX_PATH_LEN - 2);
                                strncat(full_path, (char *)fib->fib_FileName, 
                                        MAX_PATH_LEN - strlen(full_path) - 1);
                            } else {
                                strncpy(full_path, (char *)fib->fib_FileName, MAX_PATH_LEN - 1);
                            }
                            full_path[MAX_PATH_LEN - 1] = '\0';
                            
                            if (fib->fib_DirEntryType > 0 && all_mode) {
                                process_directory(full_path, "#?", flag_mask, add_mode, sub_mode);
                            } else {
                                process_file(full_path, flag_mask, add_mode, sub_mode);
                            }
                        }
                    }
                }
                if (fib) FreeDosObject(DOS_FIB, fib);
                UnLock(dir_lock);
            } else {
                out_str("PROTECT: Can't access directory\n");
                result = 20;
            }
        } else {
            out_str("PROTECT: Can't find '");
            out_str(file_arg);
            out_str("'\n");
            result = 20;
        }
    }
    
    /* Summary if not quiet */
    if (!g_quiet && g_files_processed > 0) {
        out_str("PROTECT: ");
        char buf[16];
        char *p = buf + 15;
        *p = '\0';
        int n = g_files_processed;
        do { *--p = '0' + (n % 10); n /= 10; } while (n);
        out_str(p);
        out_str(" file(s) processed");
        if (g_errors > 0) {
            out_str(", ");
            p = buf + 15;
            *p = '\0';
            n = g_errors;
            do { *--p = '0' + (n % 10); n /= 10; } while (n);
            out_str(p);
            out_str(" error(s)");
        }
        out_nl();
    }
    
    FreeArgs(rda);
    return result;
}
