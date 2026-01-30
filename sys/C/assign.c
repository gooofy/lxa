/*
 * ASSIGN command - Manage logical assignments
 * Phase 7 implementation for lxa
 * 
 * Template: NAME,TARGET,LIST/S,EXISTS/S,DISMOUNT/S,DEFER/S,PATH/S,ADD/S,REMOVE/S
 * 
 * Usage:
 *   ASSIGN               - List all assigns
 *   ASSIGN LIST          - List all assigns (explicit)
 *   ASSIGN name:         - Remove assign 'name'
 *   ASSIGN name: target  - Create assign 'name' pointing to 'target'
 *   ASSIGN name: target ADD  - Add 'target' to multi-assign 'name'
 *   ASSIGN name: EXISTS  - Check if assign 'name' exists
 *   ASSIGN name: REMOVE  - Remove assign 'name'
 *   ASSIGN name: target PATH - Create non-binding path assign
 *   ASSIGN name: target DEFER - Create late-binding assign
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <string.h>

#define VERSION "1.0"

/* EMU_CALL numbers from emucalls.h */
#define EMU_CALL_DOS_ASSIGN_LIST   1032

/* emucall2 - call emulator with 2 parameters */
static ULONG emucall2(ULONG func, ULONG param1, ULONG param2)
{
    ULONG res;
    __asm volatile(
        "move.l    %1, d0\n\t"
        "move.l    %2, d1\n\t"
        "move.l    %3, d2\n\t"
        "illegal\n\t"
        "move.l    d0, %0\n"
        : "=r" (res)
        : "r" (func), "r" (param1), "r" (param2)
        : "cc", "d0", "d1", "d2"
    );
    return res;
}

/* External reference to DOS library base */
extern struct DosLibrary *DOSBase;

/* Command template - AmigaOS compatible */
#define TEMPLATE "NAME,TARGET,LIST/S,EXISTS/S,DISMOUNT/S,DEFER/S,PATH/S,ADD/S,REMOVE/S"

/* Argument array indices */
#define ARG_NAME      0
#define ARG_TARGET    1
#define ARG_LIST      2
#define ARG_EXISTS    3
#define ARG_DISMOUNT  4
#define ARG_DEFER     5
#define ARG_PATH      6
#define ARG_ADD       7
#define ARG_REMOVE    8
#define ARG_COUNT     9

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

/* Strip trailing colon from assign name */
static void strip_colon(char *name)
{
    int len = strlen(name);
    if (len > 0 && name[len - 1] == ':') {
        name[len - 1] = '\0';
    }
}

/* List all assigns */
static int list_assigns(void)
{
    /* Buffer to receive assign list from host */
    char buffer[4096];
    
    out_str("Volumes:\n");
    out_str("  SYS [Mounted]\n");
    out_nl();
    
    out_str("Directories:\n");
    
    /* First, show hardcoded system assigns that are typically mapped */
    BPTR lock;
    
    lock = Lock((STRPTR)"C:", SHARED_LOCK);
    if (lock) {
        out_str("  C           SYS:C\n");
        UnLock(lock);
    }
    
    lock = Lock((STRPTR)"S:", SHARED_LOCK);
    if (lock) {
        out_str("  S           SYS:S\n");
        UnLock(lock);
    }
    
    lock = Lock((STRPTR)"LIBS:", SHARED_LOCK);
    if (lock) {
        out_str("  LIBS        SYS:Libs\n");
        UnLock(lock);
    }
    
    lock = Lock((STRPTR)"DEVS:", SHARED_LOCK);
    if (lock) {
        out_str("  DEVS        SYS:Devs\n");
        UnLock(lock);
    }
    
    lock = Lock((STRPTR)"T:", SHARED_LOCK);
    if (lock) {
        out_str("  T           SYS:T\n");
        UnLock(lock);
    }
    
    lock = Lock((STRPTR)"L:", SHARED_LOCK);
    if (lock) {
        out_str("  L           SYS:L\n");
        UnLock(lock);
    }
    
    /* Now list user-created assigns from the host VFS */
    int count = (int)emucall2(EMU_CALL_DOS_ASSIGN_LIST, (ULONG)buffer, sizeof(buffer));
    
    if (count > 0) {
        /* Buffer contains: name\0path\0name\0path\0...\0\0 */
        char *p = buffer;
        
        for (int i = 0; i < count; i++) {
            const char *name = p;
            p += strlen(p) + 1;
            const char *path = p;
            p += strlen(p) + 1;
            
            /* Don't show duplicates with system assigns */
            if (strcasecmp(name, "C") == 0 ||
                strcasecmp(name, "S") == 0 ||
                strcasecmp(name, "LIBS") == 0 ||
                strcasecmp(name, "DEVS") == 0 ||
                strcasecmp(name, "T") == 0 ||
                strcasecmp(name, "L") == 0) {
                continue;
            }
            
            /* Format: NAME spaces PATH (simplified) */
            out_str("  ");
            out_str(name);
            
            /* Pad to column 14 */
            int len = (int)strlen(name);
            while (len < 12) {
                out_str(" ");
                len++;
            }
            
            out_str(path);
            out_nl();
        }
    }
    
    out_nl();
    return 0;
}

/* Check if an assign exists */
static int check_exists(const char *name)
{
    char namebuf[256];
    strncpy(namebuf, name, sizeof(namebuf) - 2);
    namebuf[sizeof(namebuf) - 2] = '\0';
    
    /* Add colon if needed */
    int len = strlen(namebuf);
    if (len > 0 && namebuf[len - 1] != ':') {
        namebuf[len] = ':';
        namebuf[len + 1] = '\0';
    }
    
    BPTR lock = Lock((STRPTR)namebuf, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        return 0;  /* Success - exists */
    }
    
    out_str("ASSIGN: ");
    out_str(name);
    out_str(" does not exist\n");
    return 5;  /* WARN - does not exist */
}

/* Create an assign */
static int create_assign(const char *name, const char *target, BOOL defer, BOOL path_assign, BOOL add)
{
    char namebuf[256];
    strncpy(namebuf, name, sizeof(namebuf) - 1);
    namebuf[sizeof(namebuf) - 1] = '\0';
    strip_colon(namebuf);
    
    if (strlen(namebuf) == 0) {
        out_str("ASSIGN: Invalid assign name\n");
        return 10;  /* ERROR */
    }
    
    /* Lock the target to verify it exists */
    BPTR lock = Lock((STRPTR)target, SHARED_LOCK);
    if (!lock) {
        out_str("ASSIGN: ");
        out_str(target);
        out_str(" not found\n");
        return 10;  /* ERROR */
    }
    
    BOOL result;
    
    if (defer) {
        /* Late-binding assign */
        UnLock(lock);  /* AssignLate uses path string, not lock */
        result = AssignLate((STRPTR)namebuf, (STRPTR)target);
    } else if (path_assign) {
        /* Non-binding path assign */
        UnLock(lock);  /* AssignPath uses path string, not lock */
        result = AssignPath((STRPTR)namebuf, (STRPTR)target);
    } else if (add) {
        /* Add to multi-assign */
        result = AssignAdd((STRPTR)namebuf, lock);
        /* AssignAdd consumes the lock on success */
        if (!result) {
            UnLock(lock);
        }
    } else {
        /* Regular assign */
        result = AssignLock((STRPTR)namebuf, lock);
        /* AssignLock consumes the lock on success */
        if (!result) {
            UnLock(lock);
        }
    }
    
    if (!result) {
        out_str("ASSIGN: Failed to create assign ");
        out_str(namebuf);
        out_str(":\n");
        return 10;  /* ERROR */
    }
    
    return 0;  /* Success */
}

/* Remove an assign */
static int remove_assign(const char *name)
{
    char namebuf[256];
    strncpy(namebuf, name, sizeof(namebuf) - 1);
    namebuf[sizeof(namebuf) - 1] = '\0';
    strip_colon(namebuf);
    
    if (strlen(namebuf) == 0) {
        out_str("ASSIGN: Invalid assign name\n");
        return 10;  /* ERROR */
    }
    
    /* Use RemAssignList with NULL lock to remove the assign */
    LONG result = RemAssignList((STRPTR)namebuf, (BPTR)0);
    
    if (!result) {
        out_str("ASSIGN: ");
        out_str(namebuf);
        out_str(": not assigned\n");
        return 5;  /* WARN */
    }
    
    return 0;  /* Success */
}

int main(int argc, char **argv)
{
    struct RDArgs *rda;
    LONG args[ARG_COUNT] = {0};
    int result = 0;
    
    /* DEBUG: Show what arguments we received */
    #if 0
    out_str("DEBUG: argc=");
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    int n = argc;
    do { *--p = '0' + (n % 10); n /= 10; } while (n);
    out_str(p);
    out_nl();
    for (int i = 0; i < argc; i++) {
        out_str("  argv[");
        p = buf + sizeof(buf) - 1;
        *p = '\0';
        n = i;
        do { *--p = '0' + (n % 10); n /= 10; } while (n);
        out_str(p);
        out_str("]=");
        out_str(argv[i]);
        out_nl();
    }
    #endif
    
    /* Parse arguments */
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    
    if (!rda) {
        /* Check for /? help request */
        out_str("Usage: ASSIGN [name:] [target] [LIST] [EXISTS] [DEFER] [PATH] [ADD] [REMOVE]\n");
        out_str("Template: ");
        out_str(TEMPLATE);
        out_nl();
        return 1;
    }
    
    CONST_STRPTR name = (CONST_STRPTR)args[ARG_NAME];
    CONST_STRPTR target = (CONST_STRPTR)args[ARG_TARGET];
    BOOL list_flag = args[ARG_LIST] ? TRUE : FALSE;
    BOOL exists_flag = args[ARG_EXISTS] ? TRUE : FALSE;
    BOOL dismount_flag = args[ARG_DISMOUNT] ? TRUE : FALSE;
    BOOL defer_flag = args[ARG_DEFER] ? TRUE : FALSE;
    BOOL path_flag = args[ARG_PATH] ? TRUE : FALSE;
    BOOL add_flag = args[ARG_ADD] ? TRUE : FALSE;
    BOOL remove_flag = args[ARG_REMOVE] ? TRUE : FALSE;
    
    /* DISMOUNT is an alias for REMOVE */
    if (dismount_flag) {
        remove_flag = TRUE;
    }
    
    /* Decide what to do */
    if (list_flag || (!name && !target)) {
        /* List assigns */
        result = list_assigns();
    } else if (exists_flag && name) {
        /* Check if assign exists */
        result = check_exists((const char *)name);
    } else if (remove_flag && name) {
        /* Remove assign */
        result = remove_assign((const char *)name);
    } else if (name && target) {
        /* Create assign */
        result = create_assign((const char *)name, (const char *)target, 
                               defer_flag, path_flag, add_flag);
    } else if (name && !target) {
        /* Name without target - remove the assign */
        result = remove_assign((const char *)name);
    } else {
        out_str("ASSIGN: Invalid arguments\n");
        result = 10;
    }
    
    FreeArgs(rda);
    return result;
}
