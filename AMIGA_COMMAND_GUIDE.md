# Phase 4b: Amiga-Compatible Command Implementation Guide

This document provides detailed technical specifications for converting the Unix-style commands to authentic AmigaDOS behavior.

## Overview

The current commands (DIR, TYPE, MAKEDIR, DELETE) use Unix-style command line parsing (`-l`, `-a` flags). They must be updated to use AmigaDOS's template-based argument system with keywords and switches.

## Prerequisites

Before commands can be properly implemented, these library functions are required:

### 1. ReadArgs() (dos.library)

**Function Signature:**
```c
struct RDArgs *ReadArgs(CONST_STRPTR template, LONG *array, struct RDArgs *args);
void FreeArgs(struct RDArgs *args);
```

**Template Syntax:**
- `/A` - Argument is always required
- `/M` - Multiple arguments accepted (array)
- `/S` - Switch (boolean, keyword presence = TRUE)
- `/K` - Keyword required (must use KEYWORD=value or KEYWORD)
- `/N` - Numeric value
- `/F` - Final argument (rest of line is taken as-is)

**Example Templates:**
```c
// DIR command
"DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S"
//   DIR: optional directory/pattern
//   OPT/K: keyword option (OPT A, OPT D, etc.)
//   ALL/S: switch - display all subdirectories
//   DIRS/S: switch - display only directories
//   FILES/S: switch - display only files
//   INTER/S: switch - interactive mode

// DELETE command  
"FILE/M/A,ALL/S,QUIET/S,FORCE/S"
//   FILE/M/A: required, multiple file/pattern arguments
//   ALL/S: switch - recursive delete
//   QUIET/S: switch - suppress output
//   FORCE/S: switch - override protection

// TYPE command
"FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S"
//   FROM/A/M: required, multiple source files
//   TO/K: keyword - destination file
//   OPT/K: keyword option (OPT H, OPT N)
//   HEX/S: switch - hex dump
//   NUMBER/S: switch - line numbers

// MAKEDIR command
"NAME/M"
//   NAME/M: multiple directory names (no options)
```

**Implementation Approach:**
1. Parse template string to identify argument types
2. Parse command line to match against template
3. Support both positional and keyword arguments
4. Handle pattern matching wildcards in string arguments
5. Return results in array (one slot per template item)

### 2. Pattern Matching (utility.library or dos.library)

**Function Signatures:**
```c
LONG MatchPattern(CONST_STRPTR pat, CONST_STRPTR str);
LONG ParsePattern(CONST_STRPTR src, STRPTR dst, LONG dstLen);
```

**Amiga Wildcard Syntax:**
- `#?` - Matches any string (equivalent to `*` in Unix)
- `?` - Matches any single character
- `#` - Repeat specifier (e.g., `#?` = zero or more chars)
- `|` - Alternation in character class (e.g., `(1|2)` matches 1 or 2)
- `()` - Grouping

**Examples:**
```
#?           - matches anything
#?.txt       - matches all .txt files
?            - matches any single character
T#?          - matches anything starting with T
#?(1|2)      - matches ending in 1 or 2
#?/#?        - matches paths with directory separator
```

### 3. Semaphore Support (exec.library)

**Function Signatures:**
```c
void InitSemaphore(struct SignalSemaphore *sigSem);
void ObtainSemaphore(struct SignalSemaphore *sigSem);
void ReleaseSemaphore(struct SignalSemaphore *sigSem);
```

**Purpose:** Required by C runtime for stdio locking

## Command Implementation Details

### DIR Command

**Template:** `DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S`

**Behavior:**
```
1> DIR                    ; List current directory (dirs first, files alphabetically)
1> DIR Workbench:         ; List specific directory
1> DIR #?.txt             ; Pattern match (all .txt files)
1> DIR ALL                ; Recursive listing
1> DIR DIRS               ; Show only directories
1> DIR FILES              ; Show only files
1> DIR INTER              ; Interactive mode
1> DIR Workbench: ALL     ; Recursive on specific dir
```

**Output Format:**
- Two-column display (files only, not dirs)
- Directories listed first (alphabetically), then files (alphabetically)
- No detailed info unless requested (protection bits, size, etc.)

**Interactive Mode Commands (INTER):**
```
?      - Display help
Return - Next item
E      - Enter directory
B      - Back up one level
DEL    - Delete file/directory
T      - Type file contents
C      - Execute command
Q      - Quit interactive mode
```

**Implementation Tasks:**
1. Replace getopt-style parsing with ReadArgs()
2. Support pattern matching for directory/file arguments
3. Implement two-column output
4. Sort entries (dirs first alphabetically, then files alphabetically)
5. Implement ALL (recursive) traversal
6. Add INTER mode with command parser
7. Handle Ctrl+C break during listing

### DELETE Command

**Template:** `FILE/M/A,ALL/S,QUIET/S,FORCE/S`

**Behavior:**
```
1> DELETE Old-file                    ; Delete single file
1> DELETE Work/Prog1 Work/Prog2       ; Delete multiple files
1> DELETE T#?/#?(1|2)                 ; Pattern delete
1> DELETE DF1:#? ALL                  ; Delete all files recursively
1> DELETE Protected-file FORCE        ; Override protection
1> DELETE #? QUIET                    ; Silent operation
```

**Key Differences from Current Implementation:**
- Multiple files/patterns in single command (not just one)
- ALL keyword for recursive delete (not -r)
- FORCE to override protection (not -f)
- QUIET to suppress output (not inverse of -v)
- Display names as deleted (unless QUIET)
- Ctrl+C aborts but leaves already-deleted files

**Implementation Tasks:**
1. Support multiple FILE arguments via /M modifier
2. Pattern matching for each argument
3. Recursive deletion with ALL option
4. Check protection bits before delete (unless FORCE)
5. Progress output (unless QUIET)
6. Ctrl+C handling

### TYPE Command

**Template:** `FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S`

**Behavior:**
```
1> TYPE S:Startup-sequence            ; Display file
1> TYPE File1 File2 File3             ; Display multiple files
1> TYPE File TO Output.txt            ; Redirect to file
1> TYPE File HEX                      ; Hex dump
1> TYPE File NUMBER                   ; With line numbers
1> TYPE File HEX NUMBER               ; Both (mutually exclusive in spec, but allowed)
```

**Output Features:**
- Concatenate multiple files
- Space bar = pause, Backspace/Return/Ctrl+X = resume
- Ctrl+C = break
- HEX format: offset, hex bytes, ASCII representation

**Implementation Tasks:**
1. Support multiple FROM files via /M modifier
2. TO keyword for output redirection
3. HEX dump with ASCII column
4. Line numbering (NUMBER)
5. Pause/resume on keypress
6. Ctrl+C break handling

### MAKEDIR Command

**Template:** `NAME/M`

**Behavior:**
```
1> MAKEDIR Tests                       ; Create single directory
1> MAKEDIR Documents Payables Orders   ; Create multiple directories
1> MAKEDIR DF1:Xyz                     ; Create on specific device
```

**Key Differences from Current Implementation:**
- No `-p` option (unlike Unix mkdir -p)
- Works only within one directory level
- Parent directories must already exist
- Multiple directories in single command

**Implementation Tasks:**
1. Support multiple NAME arguments via /M modifier
2. No parent creation (fail if parent doesn't exist)
3. Single directory level only

## Implementation Strategy

### Phase 1: ReadArgs() Foundation

**Files to modify:**
- `src/rom/lxa_dos.c` - Add ReadArgs()/FreeArgs() stubs
- `src/include/emucalls.h` - Add EMU_CALL_DOS_READARGS
- `src/lxa/lxa.c` - Implement host-side parsing

**Implementation steps:**
1. Define RDArgs structure
2. Parse template string (/A, /M, /S, /K, /N, /F)
3. Parse command line (handle keywords, switches, values)
4. Support positional and keyword arguments
5. Return results in array format

### Phase 2: Pattern Matching

**Files to modify:**
- `src/rom/lxa_utility.c` or `src/rom/lxa_dos.c` - Add MatchPattern()
- May need utility.library implementation

**Implementation steps:**
1. Parse Amiga pattern syntax
2. Convert to regex or implement matcher
3. Handle `#?`, `?`, `#`, `|`, `()` constructs
4. Test against various patterns

### Phase 3: Command Updates

Update each command in `sys/C/`:

**Common pattern for all commands:**
```c
#include <dos/dos.h>
#include <dos/rdargs.h>

int main(int argc, char **argv)
{
    // Build command line string from argv
    char cmdline[1024];
    build_cmdline(argc, argv, cmdline, sizeof(cmdline));
    
    // Define template
    CONST_STRPTR template = "FILE/M/A,ALL/S,QUIET/S,FORCE/S";
    
    // Parse using ReadArgs
    struct RDArgs *rda = AllocDosObject(DOS_RDARGS, NULL);
    LONG args[5] = {0};  // One per template item
    
    rda->RDA_Source.CS_Buffer = cmdline;
    rda->RDA_Source.CS_Length = strlen(cmdline);
    
    if (ReadArgs(template, args, rda)) {
        // args[0] = FILE array
        // args[1] = ALL switch
        // args[2] = QUIET switch
        // args[3] = FORCE switch
        
        // Execute command...
        
        FreeArgs(rda);
    } else {
        // Print template help
        PrintFault(IoErr(), "DELETE");
    }
    
    FreeDosObject(DOS_RDARGS, rda);
    return 0;
}
```

### Phase 4: Semaphore Support

Required for C runtime to function properly.

**Files to modify:**
- `src/rom/exec.c` - Implement InitSemaphore, ObtainSemaphore, ReleaseSemaphore

## Testing Commands

Create test scripts:

```bash
# Test DIR
./lxa -c config.ini SYS:C/DIR
./lxa -c config.ini SYS:C/DIR "ALL"
./lxa -c config.ini SYS:C/DIR "Workbench:" "DIRS"

# Test DELETE
./lxa -c config.ini SYS:C/DELETE "Old-file"
./lxa -c config.ini SYS:C/DELETE "File1" "File2" "File3"
./lxa -c config.ini SYS:C/DELETE "#?.bak" "ALL" "QUIET"

# Test TYPE
./lxa -c config.ini SYS:C/TYPE "S:Startup-sequence"
./lxa -c config.ini SYS:C/TYPE "File" "HEX"
./lxa -c config.ini SYS:C/TYPE "File" "TO" "Output.txt"

# Test MAKEDIR
./lxa -c config.ini SYS:C/MAKEDIR "NewDir"
./lxa -c config.ini SYS:C/MAKEDIR "Dir1" "Dir2" "Dir3"
```

## References

- `shell_spec.md` - Complete AmigaDOS command reference
- AmigaDOS 3.1 documentation for template syntax
- AROS source for ReadArgs() reference implementation

## Summary

Converting commands to Amiga-compatible syntax requires:

1. **ReadArgs() parser** - Core infrastructure for all commands
2. **Pattern matching** - For wildcards in file arguments  
3. **Semaphore support** - For C runtime compatibility
4. **Command updates** - Replace getopt with ReadArgs()
5. **Behavior alignment** - Match output format and features exactly

This is foundational work that will enable proper AmigaDOS compatibility for all future C: commands.
