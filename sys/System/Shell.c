/*
 * Shell - Interactive AmigaDOS Shell for lxa
 * Phase 5 implementation
 * Rewritten to use AmigaDOS I/O instead of stdio
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
#include <stdlib.h>

#include "lxa_version.h"

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

/* Default prompt - %N=CLI number, %S=current path (includes trailing colon for volumes) */
static char prompt_str[MAX_PROMPT_LEN] = "%N.%S> ";

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

/* Helper function: Output a string */
static void out_str(const char *str) {
    Write(Output(), (STRPTR)str, strlen(str));
    Flush(Output());
}

/* Helper function: Output a string with newline */
static void out_line(const char *str) {
    out_str(str);
    out_str("\n");
}

/* Helper function: Output an integer */
static void out_int(int num) {
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    int is_neg = (num < 0);
    unsigned int n = is_neg ? -num : num;
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n);
    
    if (is_neg) *--p = '-';
    out_str(p);
}

/* Helper function: Output string with width padding */
static void out_str_padded(const char *str, int width) {
    int len = strlen(str);
    out_str(str);
    while (len < width) {
        out_str(" ");
        len++;
    }
}

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

/* Flag to indicate if we're running a script (startup-sequence or user script) */
static BOOL g_running_script = FALSE;

/* Parse and display prompt */
static void show_prompt(void)
{
    /* Don't show prompt if not interactive or running a script */
    if (!IsInteractive(Input()) || g_running_script) return;
    
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
                    /* Convert number to string */
                    {
                        char numbuf[8];
                        int k = 6;
                        numbuf[7] = '\0';
                        int n = cli_num;
                        do {
                            numbuf[--k] = '0' + (n % 10);
                            n /= 10;
                        } while (n);
                        strcpy(&expanded[j], &numbuf[k]);
                        j += strlen(&numbuf[k]);
                    }
                    break;
                case 'S':
                    strcpy(&expanded[j], path);
                    j += strlen(path);
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
            out_str_padded(node->name, 10);
            out_str(" ");
            out_line(node->value);
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
                 out_line(node->value);
             } else {
                 /* ALIAS foo "" -> Delete */
                 if (prev) prev->next = node->next;
                 else alias_list = node->next;
                 free(node->name);
                 free(node->value);
                 free(node);
             }
        } else {
            if (!space) {
                out_str("ALIAS: ");
                out_str(name);
                out_line(" not found");
            }
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
        out_line("WHICH: Usage: WHICH <command>");
        return 5;
    }
    
    /* Check alias */
    char *alias = find_alias(args);
    if (alias) {
        out_str(args);
        out_str(" is an alias for ");
        out_line(alias);
        return 0;
    }

    /* Check internal commands */
    if (strcasecmp(args, "CD") == 0 || strcasecmp(args, "ECHO") == 0 || 
        strcasecmp(args, "QUIT") == 0 || strcasecmp(args, "EXIT") == 0 || 
        strcasecmp(args, "ALIAS") == 0 || strcasecmp(args, "IF") == 0) {
        out_str(args);
        out_line(" is an internal command");
        return 0;
    }
    
    BPTR lock = Lock((STRPTR)args, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        out_line(args);
        return 0;
    }
    
    char buf[MAX_PATH_LEN];
    /* Build path manually to avoid snprintf */
    strcpy(buf, "SYS:C/");
    strncat(buf, args, MAX_PATH_LEN - 7);
    buf[MAX_PATH_LEN - 1] = '\0';
    
    lock = Lock((STRPTR)buf, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        out_line(buf);
        return 0;
    }
    
    out_str("WHICH: ");
    out_str(args);
    out_line(" not found");
    return 5;
}

/* Internal Command: WHY */
static int cmd_why(char *args)
{
    int err = IoErr();
    out_str("Last error: ");
    out_int(err);
    out_str("\n");
    return 0;
}

/* Internal Command: FAULT */
static int cmd_fault(char *args)
{
    if (args) {
        int err = atoi(args);
        out_str("Fault ");
        out_int(err);
        out_str("\n");
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
        out_line(path);
        return 0;
    }
    
    int len = strlen(args);
    while (len > 0 && (args[len-1] == '\n' || args[len-1] == ' ')) {
        args[--len] = '\0';
    }
    
    BPTR lock = Lock((STRPTR)args, SHARED_LOCK);
    if (!lock) {
        out_line("CD: Object not found");
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
        
        out_str(args);
        if (!noline) out_str("\n");
    } else {
        out_str("\n");
    }
    return 0;
}

static int cmd_ask(char *args) {
    if (args) out_str(args);
    
    char buf[10];
    /* Read from *interactive* input? ASK usually interacts with user even if running from script? */
    /* If script, ASK reads from script stream? No, ASK typically queries user console. */
    /* Unless redirected? */
    /* For simplicity, read from current Input() */
    
    /* We need to flush output first */
    Flush(Output());
    
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
    static char cmd_path[MAX_PATH_LEN];  /* static to ensure it persists */
    char *cmd_to_run = cmd_name;
    BPTR lock = 0;
    

    
    /* First try the command as-is (full path or in current dir) */
    lock = Lock((STRPTR)cmd_name, SHARED_LOCK);
    if (lock) {
        cmd_to_run = cmd_name;
    } else {
        /* Try SYS:C/<cmd> */
        strcpy(cmd_path, "SYS:C/");
        strncat(cmd_path, cmd_name, MAX_PATH_LEN - 7);
        cmd_path[MAX_PATH_LEN - 1] = '\0';
        lock = Lock((STRPTR)cmd_path, SHARED_LOCK);
        
        if (lock) {
            cmd_to_run = cmd_path;
        } else {
            /* Try SYS:System/C/<cmd> (for project structure) */
            strcpy(cmd_path, "SYS:System/C/");
            strncat(cmd_path, cmd_name, MAX_PATH_LEN - 14);
            cmd_path[MAX_PATH_LEN - 1] = '\0';
            lock = Lock((STRPTR)cmd_path, SHARED_LOCK);
            
            if (lock) {
                cmd_to_run = cmd_path;
            }
        }
    }
    
    if (!lock) {
        out_str("Unknown command: ");
        out_line(cmd_name);
        return 10;
    }
    UnLock(lock);
    
    /* Build full command with args */
    strncpy(full_cmd, cmd_to_run, MAX_CMD_LEN - 1);
    full_cmd[MAX_CMD_LEN - 1] = '\0';
    
    if (args && args[0]) {
        strncat(full_cmd, " ", MAX_CMD_LEN - strlen(full_cmd) - 1);
        strncat(full_cmd, args, MAX_CMD_LEN - strlen(full_cmd) - 1);
    }
    
    LONG rc = System((STRPTR)full_cmd, NULL);
    
    if (rc == -1) {
         out_str("Unknown command: ");
         out_line(cmd_name);
         return 10;
    }
    return rc;
}

static int cmd_failat(char *args) {
    if (args) {
        fail_at_level = atoi(args);
    }
    out_str("FAILAT ");
    out_int(fail_at_level);
    out_str("\n");
    return 0;
}

static int cmd_if(char *args) {
    if (block_sp >= MAX_NEST) {
        out_line("IF: Too many nested blocks");
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
        out_line("ELSE: No matching IF");
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
        out_line("ENDIF: No matching IF");
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
        out_line(prompt_str);
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
        out_line("PROMPT: String too long");
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
            out_line("No paths defined");
        } else {
            out_line("Current path:");
            for (int i = 0; i < num_paths; i++) {
                out_str("  ");
                out_line(search_paths[i]);
            }
        }
        return 0;
    }
    
    /* Check for ADD, SHOW, or RESET */
    if (strncasecmp(args, "ADD", 3) == 0) {
        char *path = args + 3;
        while (*path == ' ') path++;
        
        if (!*path) {
            out_line("PATH: Missing path to add");
            return 5;
        }
        
        if (num_paths >= MAX_PATHS) {
            out_line("PATH: Too many paths");
            return 5;
        }
        
        /* Verify path exists */
        BPTR lock = Lock((STRPTR)path, SHARED_LOCK);
        if (!lock) {
            out_str("PATH: Directory not found: ");
            out_line(path);
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
        out_line("PATH: Too many paths");
        return 5;
    }
    
    BPTR lock = Lock((STRPTR)args, SHARED_LOCK);
    if (!lock) {
        out_str("PATH: Directory not found: ");
        out_line(args);
        return 5;
    }
    UnLock(lock);
    
    search_paths[num_paths++] = strdup(args);
    return 0;
}

static int cmd_skip(char *args) {
    BPTR input = Input();
    if (IsInteractive(input)) {
        out_line("SKIP: Not supported in interactive mode");
        return 20;
    }
    
    char buf[MAX_CMD_LEN];
    char label[64];
    /* Build label string manually */
    strcpy(label, "LAB ");
    strncat(label, args, sizeof(label) - 5);
    label[sizeof(label) - 1] = '\0';
    
    while (1) {
        long len = Read(input, (APTR)buf, MAX_CMD_LEN - 1);
        if (len <= 0) {
            out_line("SKIP: Label not found");
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
    char read_buf[MAX_CMD_LEN];
    char cmd_buf[MAX_CMD_LEN];
    BPTR input = Input();
    BPTR script_file = 0;
    int buf_pos = 0;
    int buf_len = 0;
    BOOL run_startup = FALSE;
    
    if (argc > 1) {
        script_file = Open((STRPTR)argv[1], MODE_OLDFILE);
        if (script_file) {
            input = script_file;
            g_running_script = TRUE;
        } else {
            out_str("Shell: Could not open ");
            out_line(argv[1]);
            return 20;
        }
    } else {
        out_str("lxa Shell v");
        out_str(LXA_VERSION_STRING);
        out_str(" (");
        out_str(LXA_BUILD_DATE);
        out_line(")");
        
        /* Run startup-sequence if it exists (only for interactive shell) */
        run_startup = TRUE;
    }
    
    /* Initialize default PATH with SYS:C */
    BPTR c_lock = Lock((STRPTR)"SYS:C", SHARED_LOCK);
    if (c_lock) {
        UnLock(c_lock);
        search_paths[0] = strdup("SYS:C");
        num_paths = 1;
    }
    
    /* Execute startup-sequence if requested */
    if (run_startup) {
        BPTR startup = Lock((STRPTR)"SYS:S/Startup-Sequence", SHARED_LOCK);
        if (startup) {
            UnLock(startup);
            script_file = Open((STRPTR)"SYS:S/Startup-Sequence", MODE_OLDFILE);
            if (script_file) {
                input = script_file;
                g_running_script = TRUE;
            }
        }
    }
    
    while (running) {
        if (script_file && !run_startup) {
             /* Only check failat for user-specified scripts, not startup-sequence */
             if (last_return_code >= fail_at_level) {
                 out_str("Shell: Command failed (rc=");
                 out_int(last_return_code);
                 out_line(")");
                 break;
             }
        }
        
        show_prompt();
        
        /* Read line by line - handles both interactive and piped input */
        int cmd_pos = 0;
        BOOL got_line = FALSE;
        
        while (cmd_pos < MAX_CMD_LEN - 1 && !got_line) {
            /* Refill buffer if needed */
            if (buf_pos >= buf_len) {
                buf_len = Read(input, (APTR)read_buf, MAX_CMD_LEN - 1);
                if (buf_len <= 0) {
                    /* EOF - process any partial line */
                    if (cmd_pos > 0) {
                        got_line = TRUE;
                    }
                    break;
                }
                buf_pos = 0;
            }
            
            /* Copy until newline or buffer end */
            while (buf_pos < buf_len && cmd_pos < MAX_CMD_LEN - 1) {
                char c = read_buf[buf_pos++];
                if (c == '\n') {
                    got_line = TRUE;
                    break;
                }
                cmd_buf[cmd_pos++] = c;
            }
        }
        
        if (!got_line && cmd_pos == 0) {
            /* EOF - if we were running startup-sequence, switch to interactive */
            if (run_startup && script_file) {
                Close(script_file);
                script_file = 0;
                input = Input();
                buf_pos = 0;
                buf_len = 0;
                run_startup = FALSE;
                g_running_script = FALSE;
                continue;
            }
            break;  /* EOF with no data */
        }
        
        cmd_buf[cmd_pos] = '\0';
        if (cmd_buf[0] == '\0') continue;
        
        /* Skip leading whitespace */
        char *line = cmd_buf;
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0') continue;
        
        /* Skip comments (lines starting with ; or *) */
        if (line[0] == ';' || line[0] == '*') continue;
        
        char *cmd_name = line;
        char *cmd_args = NULL;
        char *space = strchr(line, ' ');
        if (space) {
            *space = '\0';
            cmd_args = space + 1;
            while (*cmd_args == ' ') cmd_args++;
        }
        
        /* Check Aliases */
        char *alias_val = find_alias(cmd_name);
        if (alias_val) {
            /* Simple alias replacement: execute alias value + args */
            char new_cmd[MAX_CMD_LEN];
            strncpy(new_cmd, alias_val, MAX_CMD_LEN - 1);
            new_cmd[MAX_CMD_LEN - 1] = '\0';
            
            if (cmd_args && cmd_args[0]) {
                strncat(new_cmd, " ", MAX_CMD_LEN - strlen(new_cmd) - 1);
                strncat(new_cmd, cmd_args, MAX_CMD_LEN - strlen(new_cmd) - 1);
            }
            
            /* Reparse new command */
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
