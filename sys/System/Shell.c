/*
 * Shell - Interactive AmigaDOS Shell for lxa
 * Phase 5 implementation
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <inline/dos.h>
#include <inline/exec.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* External reference to DOS library base */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define MAX_CMD_LEN 256
#define MAX_PROMPT_LEN 64
#define MAX_PATH_LEN 256
#define MAX_NEST 16
#define MAX_PATHS 16

/* Use libnix stricmp/strnicmp */
#define strcasecmp stricmp
#define strncasecmp strnicmp

/* Default prompt */
static char prompt_str[MAX_PROMPT_LEN] = "%N.%S:> ";

/* Search paths for commands */
static char *search_paths[MAX_PATHS] = { NULL };
static int num_paths = 0;

/* Internal state */
static BOOL running = TRUE;
static int last_return_code = 0;
static int fail_at_level = 10; /* Default FAILAT 10 */

/* Conditional execution state */
typedef enum {
    STATE_EXEC,         /* Executing commands */
    STATE_SKIP_TO_ELSE, /* Skipping until ELSE or ENDIF */
    STATE_SKIP_TO_ENDIF /* Skipping until ENDIF (because ELSE executed or parent skipped) */
} BlockState;

static BlockState block_stack[MAX_NEST];
static int block_sp = 0;

static BOOL is_executing(void) {
    if (block_sp == 0) return TRUE;
    return block_stack[block_sp-1] == STATE_EXEC;
}

/* Get current path for prompt */
static void get_current_path_name(char *buf, int len)
{
    BPTR lock = CurrentDir(0);
    CurrentDir(lock); /* Restore it immediately */
    
    if (lock) {
        if (!NameFromLock(lock, (STRPTR)buf, len)) {
            strncpy(buf, "???", len);
        }
    } else {
        strncpy(buf, "SYS", len);
    }
}

/* Parse and display prompt */
static void show_prompt(void)
{
    /* Don't show prompt if not interactive or skipping */
    if (!IsInteractive(Input())) return;
    
    char path[MAX_PATH_LEN];
    char expanded[MAX_PROMPT_LEN];
    int i, j;
    
    get_current_path_name(path, MAX_PATH_LEN);
    
    struct Process *me = (struct Process *)FindTask(NULL);
    int cli_num = me->pr_TaskNum; 
    
    for (i = 0, j = 0; prompt_str[i] && j < MAX_PROMPT_LEN - 1; i++) {
        if (prompt_str[i] == '%') {
            i++;
            switch (prompt_str[i]) {
                case 'N':
                    j += snprintf(&expanded[j], MAX_PROMPT_LEN - j, "%d", cli_num);
                    break;
                case 'S':
                    j += snprintf(&expanded[j], MAX_PROMPT_LEN - j, "%s", path);
                    break;
                default:
                    expanded[j++] = prompt_str[i];
                    break;
            }
        } else {
            expanded[j++] = prompt_str[i];
        }
    }
    expanded[j] = '\0';
    
    Write(Output(), (STRPTR)expanded, strlen(expanded));
    Flush(Output());  /* Ensure prompt appears before reading input */
}

static int cmd_if(char *args);
static int cmd_else(char *args);
static int cmd_endif(char *args);
static int cmd_failat(char *args);
static int cmd_skip(char *args);
static int cmd_lab(char *args);
static int cmd_prompt(char *args);
static int cmd_path(char *args);
static int cmd_quit(char *args);
static int cmd_ask(char *args);

/* Alias support */
typedef struct AliasNode {
    struct AliasNode *next;
    char *name;
    char *value;
} AliasNode;

static AliasNode *alias_list = NULL;

static int cmd_alias(char *args)
{
    if (!args || args[0] == '\0') {
        /* List aliases */
        AliasNode *node = alias_list;
        while (node) {
            printf("%-10s %s\n", node->name, node->value);
            node = node->next;
        }
        return 0;
    }
    
    /* Parse name and value */
    char *name = args;
    char *value = NULL;
    char *space = strchr(args, ' ');
    if (space) {
        *space = '\0';
        value = space + 1;
        while (*value == ' ') value++;
        /* Remove quotes if present? Simplified for now */
    }
    
    /* Find existing */
    AliasNode *node = alias_list;
    AliasNode *prev = NULL;
    while (node) {
        if (strcasecmp(node->name, name) == 0) {
            break;
        }
        prev = node;
        node = node->next;
    }
    
    if (value && value[0]) {
        /* Set/Update */
        if (!node) {
            node = malloc(sizeof(AliasNode));
            if (!node) return 20;
            node->name = strdup(name);
            node->next = alias_list;
            alias_list = node;
        } else {
            free(node->value);
        }
        node->value = strdup(value);
    } else {
        /* Show or Delete */
        if (node) {
             /* If we just typed "ALIAS foo", show it */
             if (!space) {
                 printf("%s\n", node->value);
             } else {
                 /* ALIAS foo "" -> Delete */
                 /* We treat empty value as delete request if space was found */
                 if (prev) prev->next = node->next;
                 else alias_list = node->next;
                 free(node->name);
                 free(node->value);
                 free(node);
             }
        } else {
            if (!space) printf("ALIAS: %s not found\n", name);
        }
    }
    return 0;
}

static char *find_alias(const char *name) {
    AliasNode *node = alias_list;
    while (node) {
        if (strcasecmp(node->name, name) == 0) return node->value;
        node = node->next;
    }
    return NULL;
}

/* Internal Command: WHICH */
static int cmd_which(char *args)
{
    if (!args || args[0] == '\0') {
        printf("WHICH: Usage: WHICH <command>\n");
        return 5;
    }
    
    /* Check alias */
    char *alias = find_alias(args);
    if (alias) {
        printf("%s is an alias for %s\n", args, alias);
        return 0;
    }

    /* Check internal commands */
    if (strcasecmp(args, "CD") == 0 || strcasecmp(args, "ECHO") == 0 || 
        strcasecmp(args, "QUIT") == 0 || strcasecmp(args, "EXIT") == 0 || 
        strcasecmp(args, "ALIAS") == 0 || strcasecmp(args, "IF") == 0) {
        printf("%s is an internal command\n", args);
        return 0;
    }
    
    BPTR lock = Lock((STRPTR)args, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        printf("%s\n", args);
        return 0;
    }
    
    char buf[MAX_PATH_LEN];
    snprintf(buf, sizeof(buf), "SYS:C/%s", args);
    lock = Lock((STRPTR)buf, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        printf("%s\n", buf);
        return 0;
    }
    
    printf("WHICH: %s not found\n", args);
    return 5;
}

/* Internal Command: WHY */
static int cmd_why(char *args)
{
    int err = IoErr();
    printf("Last error: %d\n", err);
    return 0;
}

/* Internal Command: FAULT */
static int cmd_fault(char *args)
{
    if (args) {
        int err = atoi(args);
        printf("Fault %d\n", err);
    } else {
        cmd_why(NULL);
    }
    return 0;
}

/* Internal Command: CD */
static int cmd_cd(char *args)
{
    if (!args || args[0] == '\0') {
        char path[MAX_PATH_LEN];
        get_current_path_name(path, MAX_PATH_LEN);
        printf("%s\n", path);
        return 0;
    }
    
    int len = strlen(args);
    while (len > 0 && (args[len-1] == '\n' || args[len-1] == ' ')) {
        args[--len] = '\0';
    }
    
    BPTR lock = Lock((STRPTR)args, SHARED_LOCK);
    if (!lock) {
        printf("CD: Object not found\n");
        return 205; 
    }
    
    BPTR oldLock = CurrentDir(lock);
    UnLock(oldLock);
    
    return 0;
}

/* Internal Command: QUIT */
static int cmd_quit(char *args)
{
    running = FALSE;
    return 0;
}

/* Internal Command: ECHO */
static int cmd_echo(char *args)
{
    if (args) {
        int len = strlen(args);
        int noline = 0;
        
        /* Check for NOLINE at end (simple check) */
        if (len >= 6 && strcasecmp(args + len - 6, "NOLINE") == 0) {
            /* Ensure it's a separate word */
            if (len == 6 || args[len-7] == ' ') {
                noline = 1;
                args[len-6] = 0;
                /* Trim trailing space */
                if (len > 6 && args[len-7] == ' ') args[len-7] = 0;
            }
        }
        
        printf("%s", args);
        if (!noline) printf("\n");
        /* Flush output? */
        // fflush(stdout);
    } else {
        printf("\n");
    }
    return 0;
}

static int cmd_ask(char *args) {
    if (args) printf("%s", args);
    
    char buf[10];
    /* Read from *interactive* input? ASK usually interacts with user even if running from script? */
    /* If script, ASK reads from script stream? No, ASK typically queries user console. */
    /* Unless redirected? */
    /* For simplicity, read from current Input() */
    
    /* We need to flush output first */
    // Flush(Output());
    
    /* We use Read(Input()) */
    BPTR input = Input();
    long len = Read(input, (APTR)buf, sizeof(buf)-1);
    
    if (len > 0) {
        buf[len] = 0;
        if (strnicmp(buf, "Y", 1) == 0) return 0; /* Yes */
    }
    return 5; /* No (WARN) */
}

/* Execute external command */
static int execute_command(char *cmd_name, char *args)
{
    char full_cmd[MAX_CMD_LEN];
    snprintf(full_cmd, MAX_CMD_LEN, "%s %s", cmd_name, args ? args : "");
    
    LONG rc = System((STRPTR)full_cmd, NULL);
    if (rc == -1) {
         printf("Unknown command: %s\n", cmd_name);
         return 10;
    }
    return rc;
}

static int cmd_failat(char *args) {
    if (args) {
        fail_at_level = atoi(args);
    }
    printf("FAILAT %d\n", fail_at_level);
    return 0;
}

static int cmd_if(char *args) {
    if (block_sp >= MAX_NEST) {
        printf("IF: Too many nested blocks\n");
        return 20;
    }
    
    if (!is_executing()) {
        block_stack[block_sp++] = STATE_SKIP_TO_ENDIF;
        return 0;
    }
    
    BOOL cond = FALSE;
    
    if (args && strcasecmp(args, "WARN") == 0) {
        cond = (last_return_code >= 5);
    } else if (args && strcasecmp(args, "ERROR") == 0) {
        cond = (last_return_code >= 10);
    } else {
        /* Assume true for now if unknown (simplified) or treat as exists? */
        /* Normally IF EXISTS file... */
        if (args && strncasecmp(args, "EXISTS", 6) == 0) {
             char *fname = args + 6;
             while (*fname == ' ') fname++;
             BPTR lock = Lock((STRPTR)fname, SHARED_LOCK);
             if (lock) {
                 UnLock(lock);
                 cond = TRUE;
             }
        }
    }
    
    if (cond) {
        block_stack[block_sp++] = STATE_EXEC;
    } else {
        block_stack[block_sp++] = STATE_SKIP_TO_ELSE;
    }
    return 0;
}

static int cmd_else(char *args) {
    if (block_sp == 0) {
        printf("ELSE: No matching IF\n");
        return 20;
    }
    
    BlockState current = block_stack[block_sp-1];
    
    if (current == STATE_EXEC) {
        block_stack[block_sp-1] = STATE_SKIP_TO_ENDIF;
    } else if (current == STATE_SKIP_TO_ELSE) {
        /* Only if parent is executing */
        if (block_sp == 1 || block_stack[block_sp-2] == STATE_EXEC)
             block_stack[block_sp-1] = STATE_EXEC;
        else
             block_stack[block_sp-1] = STATE_SKIP_TO_ENDIF; /* Parent skipping, we skip */
    }
    return 0;
}

static int cmd_endif(char *args) {
    if (block_sp == 0) {
        printf("ENDIF: No matching IF\n");
        return 20;
    }
    block_sp--;
    return 0;
}

static int cmd_lab(char *args) {
    return 0;
}

/* Internal Command: PROMPT */
static int cmd_prompt(char *args) {
    if (!args || args[0] == '\0') {
        /* Show current prompt format */
        printf("%s\n", prompt_str);
        return 0;
    }
    
    /* Remove quotes if present */
    int len = strlen(args);
    if (len >= 2 && args[0] == '"' && args[len-1] == '"') {
        args++;
        args[len-2] = '\0';
        len -= 2;
    }
    
    /* Set new prompt */
    if (len >= MAX_PROMPT_LEN) {
        printf("PROMPT: String too long\n");
        return 5;
    }
    strncpy(prompt_str, args, MAX_PROMPT_LEN - 1);
    prompt_str[MAX_PROMPT_LEN - 1] = '\0';
    return 0;
}

/* Internal Command: PATH */
static int cmd_path(char *args) {
    if (!args || args[0] == '\0') {
        /* Show current paths */
        if (num_paths == 0) {
            printf("No paths defined\n");
        } else {
            printf("Current path:\n");
            for (int i = 0; i < num_paths; i++) {
                printf("  %s\n", search_paths[i]);
            }
        }
        return 0;
    }
    
    /* Check for ADD, SHOW, or RESET */
    if (strncasecmp(args, "ADD", 3) == 0) {
        char *path = args + 3;
        while (*path == ' ') path++;
        
        if (!*path) {
            printf("PATH: Missing path to add\n");
            return 5;
        }
        
        if (num_paths >= MAX_PATHS) {
            printf("PATH: Too many paths\n");
            return 5;
        }
        
        /* Verify path exists */
        BPTR lock = Lock((STRPTR)path, SHARED_LOCK);
        if (!lock) {
            printf("PATH: Directory not found: %s\n", path);
            return 5;
        }
        UnLock(lock);
        
        search_paths[num_paths++] = strdup(path);
        return 0;
    }
    
    if (strncasecmp(args, "RESET", 5) == 0) {
        for (int i = 0; i < num_paths; i++) {
            free(search_paths[i]);
            search_paths[i] = NULL;
        }
        num_paths = 0;
        return 0;
    }
    
    if (strncasecmp(args, "SHOW", 4) == 0) {
        return cmd_path(NULL); /* Show paths */
    }
    
    /* Default: ADD */
    if (num_paths >= MAX_PATHS) {
        printf("PATH: Too many paths\n");
        return 5;
    }
    
    BPTR lock = Lock((STRPTR)args, SHARED_LOCK);
    if (!lock) {
        printf("PATH: Directory not found: %s\n", args);
        return 5;
    }
    UnLock(lock);
    
    search_paths[num_paths++] = strdup(args);
    return 0;
}

static int cmd_skip(char *args) {
    BPTR input = Input();
    if (IsInteractive(input)) {
        printf("SKIP: Not supported in interactive mode\n");
        return 20;
    }
    
    char buf[MAX_CMD_LEN];
    char label[64];
    snprintf(label, sizeof(label), "LAB %s", args);
    
    while (1) {
        long len = Read(input, (APTR)buf, MAX_CMD_LEN - 1);
        if (len <= 0) {
            printf("SKIP: Label not found\n");
            return 20;
        }
        buf[len] = 0;
        if (strncasecmp(buf, label, strlen(label)) == 0) {
            break; 
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    char cmd_buf[MAX_CMD_LEN];
    BPTR input = Input();
    BPTR script_file = 0;
    
    if (argc > 1) {
        script_file = Open((STRPTR)argv[1], MODE_OLDFILE);
        if (script_file) {
            input = script_file;
        } else {
            printf("Shell: Could not open %s\n", argv[1]);
            return 20;
        }
    } else {
        printf("lxa Shell v%s\n", "1.0");
    }
    
    /* Initialize default PATH with SYS:C */
    BPTR c_lock = Lock("SYS:C", SHARED_LOCK);
    if (c_lock) {
        UnLock(c_lock);
        search_paths[0] = strdup("SYS:C");
        num_paths = 1;
    }
    
    while (running) {
        if (input == script_file || !IsInteractive(input)) {
             if (last_return_code >= fail_at_level) {
                 printf("Shell: Command failed (rc=%d)\n", last_return_code);
                 break;
             }
        }
        
        show_prompt();
        
        long len = Read(input, (APTR)cmd_buf, MAX_CMD_LEN - 1);
        
        if (len <= 0) {
            break;
        }
        
        cmd_buf[len] = '\0';
        if (len > 0 && cmd_buf[len-1] == '\n') cmd_buf[len-1] = '\0';
        if (cmd_buf[0] == '\0') continue;
        
        char *cmd_name = cmd_buf;
        char *cmd_args = NULL;
        char *space = strchr(cmd_buf, ' ');
        if (space) {
            *space = '\0';
            cmd_args = space + 1;
            while (*cmd_args == ' ') cmd_args++;
        }
        
        /* Check Aliases */
        char *alias_val = find_alias(cmd_name);
        if (alias_val) {
            /* Simple alias replacement: execute alias value + args */
            /* Note: this is recursive expansion unsafe and simplistic */
            char new_cmd[MAX_CMD_LEN];
            snprintf(new_cmd, MAX_CMD_LEN, "%s %s", alias_val, cmd_args ? cmd_args : "");
            
            /* Reparse new command */
            /* For now, just recurse or handle simply. 
               Let's update cmd_name/args/buf pointers. */
            
            /* WATCH OUT: cmd_buf is the buffer. We need to handle this carefully. */
            strncpy(cmd_buf, new_cmd, MAX_CMD_LEN);
            cmd_buf[MAX_CMD_LEN-1] = '\0';
            
            cmd_name = cmd_buf;
            cmd_args = NULL;
            space = strchr(cmd_buf, ' ');
            if (space) {
                *space = '\0';
                cmd_args = space + 1;
                while (*cmd_args == ' ') cmd_args++;
            }
        }
        
        int is_if = (strcasecmp(cmd_name, "IF") == 0);
        int is_else = (strcasecmp(cmd_name, "ELSE") == 0);
        int is_endif = (strcasecmp(cmd_name, "ENDIF") == 0);
        
        if (!is_executing() && !is_if && !is_else && !is_endif) {
            continue;
        }
        
        if (is_if) {
            cmd_if(cmd_args);
        } else if (is_else) {
            cmd_else(cmd_args);
        } else if (is_endif) {
            cmd_endif(cmd_args);
        } else if (strcasecmp(cmd_name, "CD") == 0) {
            last_return_code = cmd_cd(cmd_args);
        } else if (strcasecmp(cmd_name, "QUIT") == 0 || strcasecmp(cmd_name, "EXIT") == 0 || strcasecmp(cmd_name, "ENDCLI") == 0) {
            last_return_code = cmd_quit(cmd_args);
        } else if (strcasecmp(cmd_name, "ECHO") == 0) {
            last_return_code = cmd_echo(cmd_args);
        } else if (strcasecmp(cmd_name, "FAILAT") == 0) {
            last_return_code = cmd_failat(cmd_args);
        } else if (strcasecmp(cmd_name, "SKIP") == 0) {
            last_return_code = cmd_skip(cmd_args);
        } else if (strcasecmp(cmd_name, "LAB") == 0) {
            last_return_code = cmd_lab(cmd_args);
        } else if (strcasecmp(cmd_name, "ASK") == 0) {
            last_return_code = cmd_ask(cmd_args);
        } else if (strcasecmp(cmd_name, "ALIAS") == 0) {
            last_return_code = cmd_alias(cmd_args);
        } else if (strcasecmp(cmd_name, "WHICH") == 0) {
            last_return_code = cmd_which(cmd_args);
        } else if (strcasecmp(cmd_name, "WHY") == 0) {
            last_return_code = cmd_why(cmd_args);
        } else if (strcasecmp(cmd_name, "FAULT") == 0) {
            last_return_code = cmd_fault(cmd_args);
        } else if (strcasecmp(cmd_name, "PROMPT") == 0) {
            last_return_code = cmd_prompt(cmd_args);
        } else if (strcasecmp(cmd_name, "PATH") == 0) {
            last_return_code = cmd_path(cmd_args);
        } else {
            last_return_code = execute_command(cmd_name, cmd_args);
        }
        
        SetIoErr(last_return_code);
    }
    
    if (script_file) {
        Close(script_file);
    }
    
    return last_return_code;
}
