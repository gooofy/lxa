# Developer Guide

This guide provides essential information for developers working on lxa.

## Getting Started

### Prerequisites

- Familiarity with C programming
- Basic understanding of AmigaOS concepts (libraries, tasks, files)
- Linux development environment
- Git for version control

### Setting Up Development Environment

1. **Clone repository**:
   ```bash
   git clone <repository-url>
   cd lxa
   ```

2. **Install dependencies**:
   ```bash
   # Build tools
   sudo apt-get install build-essential cmake

   # Amiga cross-compiler
   git clone https://github.com/bebbo/amiga-gcc
   cd amiga-gcc
   make update
   make all -j$(nproc)
   ```

3. **Build project**:
   ```bash
   ./build.sh
   ```

4. **Run tests**:
   ```bash
   cd tests
   make test
   ```

## Project Structure

```
lxa/
├── src/
│   ├── lxa/           # Host emulator (Linux)
│   │   ├── lxa.c      # Main emulator loop
│   │   ├── vfs.c/h    # Virtual filesystem
│   │   ├── config.c/h # Configuration parser
│   │   └── m68k*.c    # Musashi CPU emulator
│   ├── rom/           # ROM implementation (m68k)
│   │   ├── exec.c     # exec.library
│   │   ├── lxa_dos.c  # dos.library
│   │   └── ...
│   └── include/       # Shared headers
│       └── emucalls.h # Emulator call definitions
├── sys/               # System commands (m68k)
│   ├── C/             # C: commands (DIR, TYPE, etc.)
│   └── System/        # System tools (Shell)
├── tests/             # Integration tests
├── doc/               # Documentation
├── cmake/             # CMake modules
└── share/             # Runtime files
```

## Development Workflow

### 1. Consult Roadmap

**Before starting any work**, read [roadmap.md](../roadmap.md) to:
- Identify current phase
- Find next logical task
- Understand dependencies

### 2. Write Tests First (TDD)

1. **Plan tests** - What needs to be tested?
2. **Write test skeleton** - Create test directory and files
3. **Define expected output** - Create expected.out
4. **Implement feature** - Write minimum code to pass
5. **Verify** - Run tests and check coverage

Example workflow:
```bash
# Create test
mkdir -p tests/dos/newfeature
cd tests/dos/newfeature

# Write test program
cat > main.c << 'EOF'
#include <dos/dos.h>
int main(void) {
    // Test code here
}
EOF

# Create Makefile
cat > Makefile << 'EOF'
include ../../Makefile.config
PROG = newfeature
all: $(PROG)
$(PROG): main.c
	$(M68K_CC) $(M68K_CFLAGS) -o $@ $<
run-test: $(PROG)
	@../../test_runner.sh $(PROG) expected.out
clean:
	rm -f $(PROG)
.PHONY: all run-test clean
EOF

# Create expected output
cat > expected.out << 'EOF'
OK: Feature works
EOF

# Now implement the feature in ROM...
# ... edit src/rom/lxa_dos.c ...

# Test
make run-test
```

### 3. Implement Feature

Follow coding standards (see below) and implement the feature.

### 4. Update Documentation

After completing a task:
1. Update [roadmap.md](../roadmap.md) - Mark task complete
2. Update relevant docs (README, architecture, etc.)
3. Add code comments for complex logic

### 5. Verify Quality

Before committing:
```bash
# All tests pass
make -C tests test

# Check coverage
make coverage
# Target: 95%+ for critical code

# No compiler warnings
make 2>&1 | grep warning
```

## Coding Standards

### General Rules

- **Indentation**: 4 spaces (no tabs)
- **Line length**: Prefer 80-100 characters, max 120
- **Comments**: C-style `/* */` preferred
- **Naming**: `snake_case` for variables and internal functions

### Component-Specific Standards

#### ROM Code (src/rom/)

**Brace Style**: Allman (braces on new line)
```c
if (condition)
{
    do_something();
}
else
{
    do_other();
}
```

**Function declarations**:
```c
// Library functions use register arguments
BPTR __saveds __asm Lock(
    register __a0 CONST_STRPTR name,
    register __d1 LONG mode)
{
    // Implementation
}
```

**Types**: Use Amiga types
```c
LONG, ULONG    // Not long, unsigned long
STRPTR         // Not char*
BPTR           // Not void*
BOOL           // Not int
```

**Memory access**:
```c
// Use BADDR macros for BPTR conversion
APTR ptr = BADDR(bptr);
BPTR bp = MKBADDR(ptr);
```

**Logging**:
```c
LPRINTF(LOG_ERROR, "Critical error: %s\n", msg);  // Always enabled
DPRINTF(LOG_DEBUG, "Debug info: %ld\n", value);   // Only if ENABLE_DEBUG
```

**No writable static data!**
```c
// BAD - static arrays in ROM don't work
static STRPTR array[16];
array[0] = something;

// GOOD - allocate from heap
STRPTR *array = AllocVec(16 * sizeof(STRPTR), MEMF_CLEAR);
if (array) {
    array[0] = something;
    // ... use ...
    FreeVec(array);
}
```

#### System Commands (sys/)

**Brace Style**: K&R (braces on same line)
```c
if (condition) {
    do_something();
} else {
    do_other();
}
```

**Main function**:
```c
#include <dos/dos.h>
#include <clib/dos_protos.h>

static int do_mycommand(void)
{
    // Implementation
    return RETURN_OK;
}

int main(void)
{
    return do_mycommand();
}
```

**ReadArgs template**:
```c
STATIC CONST_STRPTR TEMPLATE = "FROM/A,TO,SIZE/K/N,VERBOSE/S";

LONG args[4] = {0};
struct RDArgs *rda = ReadArgs(TEMPLATE, args, NULL);
if (!rda) {
    PrintFault(IoErr(), "mycommand");
    return RETURN_FAIL;
}

STRPTR from = (STRPTR)args[0];
STRPTR to = (STRPTR)args[1];
LONG *size = (LONG *)args[2];
BOOL verbose = args[3];

// ... use arguments ...

FreeArgs(rda);
```

**Stack usage**: Keep local arrays small (<256 bytes)
```c
// BAD - too large for 4KB stack
UBYTE buffer[4096];

// GOOD - use heap for large buffers
UBYTE *buffer = AllocVec(4096, MEMF_ANY);
if (!buffer) return ERROR_NO_FREE_STORE;
// ... use buffer ...
FreeVec(buffer);
```

#### Host Code (src/lxa/)

**Brace Style**: Allman preferred
```c
int function(void)
{
    if (condition)
    {
        do_something();
    }
}
```

**Types**: Standard C types, or Amiga types when dealing with m68k memory
```c
uint32_t, int, char*     // For host code
ULONG, LONG, STRPTR      // When reading m68k memory
```

**Memory access**:
```c
// Reading m68k memory
uint32_t value = m68k_read_memory_32(addr);
uint16_t word = m68k_read_memory_16(addr);
uint8_t byte = m68k_read_memory_8(addr);

// Writing m68k memory
m68k_write_memory_32(addr, value);
m68k_write_memory_16(addr, word);
m68k_write_memory_8(addr, byte);
```

**Error handling**:
```c
int result = some_function();
if (result < 0)
{
    fprintf(stderr, "Error: %s\n", strerror(errno));
    return -1;
}
```

## Common Development Tasks

### Adding a New DOS Function

1. **Define emucall** in [src/include/emucalls.h](../src/include/emucalls.h):
   ```c
   #define EMU_CALL_DOS_MYNEWFUNC  1050
   ```

2. **Implement ROM side** in [src/rom/lxa_dos.c](../src/rom/lxa_dos.c):
   ```c
   LONG __saveds __asm MyNewFunc(
       register __a0 STRPTR arg1,
       register __d1 LONG arg2)
   {
       DPRINTF(LOG_DEBUG, "MyNewFunc(%s, %ld)\n", arg1, arg2);
       
       // Prepare arguments
       m68k_set_reg(M68K_REG_D0, (ULONG)arg1);
       m68k_set_reg(M68K_REG_D1, arg2);
       
       // Call host
       LONG result = emucall(EMU_CALL_DOS_MYNEWFUNC);
       
       return result;
   }
   ```

3. **Implement host side** in [src/lxa/lxa.c](../src/lxa/lxa.c):
   ```c
   case EMU_CALL_DOS_MYNEWFUNC:
   {
       uint32_t arg1_ptr = m68k_get_reg(NULL, M68K_REG_D0);
       uint32_t arg2 = m68k_get_reg(NULL, M68K_REG_D1);
       
       // Read string from m68k memory
       char arg1[256];
       read_amiga_string(arg1_ptr, arg1, sizeof(arg1));
       
       // Perform operation
       int result = do_host_operation(arg1, arg2);
       
       // Return result
       m68k_set_reg(M68K_REG_D0, result);
       break;
   }
   ```

4. **Add to jump table** in [src/rom/lxa_dos.c](../src/rom/lxa_dos.c):
   ```c
   static const APTR dos_vectors[] = {
       // ... existing vectors ...
       (APTR)MyNewFunc,  // -XXX
   };
   ```

5. **Write test** in `tests/dos/mynewfunc/`:
   ```c
   #include <dos/dos.h>
   #include <clib/dos_protos.h>
   
   int main(void)
   {
       LONG result = MyNewFunc("test", 42);
       Printf("Result: %ld\n", result);
       return 0;
   }
   ```

6. **Update documentation** - Add to API reference if needed

### Adding a New Command

1. **Create source file** in `sys/C/mycommand.c`:
   ```c
   #include <dos/dos.h>
   #include <clib/dos_protos.h>
   
   #define TEMPLATE "FROM/A,TO,VERBOSE/S"
   
   static int do_mycommand(void)
   {
       LONG args[3] = {0};
       struct RDArgs *rda = ReadArgs(TEMPLATE, args, NULL);
       if (!rda) {
           PrintFault(IoErr(), "mycommand");
           return RETURN_FAIL;
       }
       
       STRPTR from = (STRPTR)args[0];
       STRPTR to = (STRPTR)args[1];
       BOOL verbose = args[2];
       
       // Implementation
       
       FreeArgs(rda);
       return RETURN_OK;
   }
   
   int main(void)
   {
       return do_mycommand();
   }
   ```

2. **Add to CMakeLists.txt** in `sys/CMakeLists.txt`:
   ```cmake
   add_m68k_executable(mycommand C/mycommand.c)
   ```

3. **Write test** in `tests/commands/mycommand/`

4. **Update roadmap** - Mark task complete

### Debugging ROM Code

**Enable debug output**:
```c
// In src/rom/util.h, uncomment:
#define ENABLE_DEBUG

// Or temporarily use LPRINTF instead of DPRINTF:
LPRINTF(LOG_INFO, "Debug: variable=%ld\n", var);
```

**Run with debug flags**:
```bash
lxa -d myprogram
```

**Check m68k registers**:
```c
// In ROM code
DPRINTF(LOG_DEBUG, "D0=%08lx A0=%08lx\n", 
        m68k_get_reg(NULL, M68K_REG_D0),
        m68k_get_reg(NULL, M68K_REG_A0));
```

**Trace function calls**:
```c
DPRINTF(LOG_DEBUG, "Entering MyFunc(%s, %ld)\n", arg1, arg2);
// ... function body ...
DPRINTF(LOG_DEBUG, "Leaving MyFunc, result=%ld\n", result);
```

### Debugging VFS Issues

**Enable VFS debug output**:
```bash
lxa -d -v myprogram
```

**Check path resolution**:
```c
// In src/lxa/vfs.c
fprintf(stderr, "VFS: Resolving '%s'\n", amiga_path);
fprintf(stderr, "VFS: Resolved to '%s'\n", linux_path);
```

**Test case sensitivity**:
```bash
# Create test file
echo "test" > ~/.lxa/System/C/TEST

# Try to open
lxa
1.SYS:> type SYS:c/test
# Should find TEST despite case difference
```

### Memory Leak Detection

**Use Valgrind**:
```bash
valgrind --leak-check=full lxa myprogram
```

**Check for FreeMem/AllocMem balance**:
```c
// Add debug counters
static ULONG alloc_count = 0;
static ULONG free_count = 0;

// In AllocMem
alloc_count++;
DPRINTF(LOG_DEBUG, "AllocMem: count=%ld\n", alloc_count);

// In FreeMem
free_count++;
DPRINTF(LOG_DEBUG, "FreeMem: count=%ld\n", free_count);

// At exit
LPRINTF(LOG_INFO, "AllocMem called %ld times, FreeMem called %ld times\n",
        alloc_count, free_count);
```

## Best Practices

### Error Handling

**Always check return values**:
```c
BPTR lock = Lock("SYS:", SHARED_LOCK);
if (!lock) {
    LONG err = IoErr();
    LPRINTF(LOG_ERROR, "Lock failed: %ld\n", err);
    return ERROR_OBJECT_NOT_FOUND;
}
```

**Set proper error codes**:
```c
if (file_not_found) {
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return NULL;
}
```

**Clean up on error paths**:
```c
APTR mem = AllocMem(1024, MEMF_ANY);
if (!mem) {
    return ERROR_NO_FREE_STORE;
}

BPTR fh = Open("file", MODE_OLDFILE);
if (!fh) {
    FreeMem(mem, 1024);  // Don't leak!
    return IoErr();
}
```

### Performance

**Avoid repeated operations**:
```c
// BAD
for (int i = 0; i < strlen(str); i++) {  // strlen called every iteration!
    process(str[i]);
}

// GOOD
size_t len = strlen(str);
for (int i = 0; i < len; i++) {
    process(str[i]);
}
```

**Cache lookups**:
```c
// BAD
for (int i = 0; i < count; i++) {
    struct Node *node = FindName(&list, names[i]);  // Search every time
    process(node);
}

// GOOD
// Build lookup table once
struct Node *nodes[count];
for (int i = 0; i < count; i++) {
    nodes[i] = FindName(&list, names[i]);
}
for (int i = 0; i < count; i++) {
    process(nodes[i]);
}
```

### Code Documentation

**Document complex algorithms**:
```c
/*
 * Parse ReadArgs template and build parameter structure.
 * 
 * Template format: "ARG1/A,ARG2/K,ARG3/S,ARG4/M"
 * Modifiers: /A (required), /K (keyword), /S (switch), /M (multiple)
 * 
 * Returns: Parameter count, or -1 on error
 */
static int parse_template(const char *template, struct Param *params)
{
    // ...
}
```

**Explain non-obvious code**:
```c
// BPTR is byte-pointer (>> 2), convert to real pointer
APTR real_ptr = BADDR(bptr);

// Set bit 31 to mark as BCPL string
BPTR bcpl_str = MKBADDR(ptr) | 0x80000000;
```

**Document assumptions**:
```c
/*
 * NOTE: Caller must hold lock on directory
 * NOTE: Buffer must be at least 256 bytes
 * NOTE: Returns borrowed pointer - do not free
 */
```

## Tools and Utilities

### GDB Debugging

```bash
# Debug host emulator
gdb --args lxa myprogram

# Useful commands
(gdb) break lxa.c:main
(gdb) run
(gdb) step
(gdb) print variable
(gdb) backtrace
```

### Profiling

```bash
# CPU profiling
perf record -g lxa myprogram
perf report

# Cache misses
perf stat -e cache-misses lxa myprogram
```

### Static Analysis

```bash
# Cppcheck
cppcheck --enable=all src/lxa/

# Clang analyzer
scan-build make
```

### Code Formatting

```bash
# Format with clang-format (if available)
clang-format -i src/lxa/*.c

# Or use indent
indent -linux src/lxa/*.c
```

## Resources

### AmigaOS References

- **RKM (Rom Kernel Manual)**: Authoritative AmigaOS documentation
- **AROS source**: Modern open-source AmigaOS implementation
- **NDK includes**: Native Development Kit headers

### lxa-Specific

- [Architecture](architecture.md) - Internal design
- [Building](building.md) - Build system details
- [Testing](testing.md) - Testing guide
- [AGENTS.md](../AGENTS.md) - AI agent guidelines (includes quality standards)
- [roadmap.md](../roadmap.md) - Development roadmap

### Musashi (68k Emulator)

- Handles instruction decode and execution
- Located in `src/lxa/m68k*.c`
- Configuration in `src/lxa/m68kconf.h`

### Community

- GitHub issues - Bug reports and feature requests
- Contributions welcome - See [Contributing](#contributing)

## Contributing

### Before Submitting

1. Read [AGENTS.md](../AGENTS.md) for quality requirements
2. Write tests (target 100% coverage for new code)
3. Update documentation
4. Run full test suite: `make -C tests test`
5. Check for warnings: `make 2>&1 | grep warning`

### Commit Messages

```
<component>: <brief summary>

<detailed description>

Fixes #123
```

Example:
```
dos: Implement SetDate() function

Add SetDate() to set file modification time. Includes:
- Emucall EMU_CALL_DOS_SETDATE
- Host-side utime() implementation
- Test case in tests/dos/setdate/

Fixes #42
```

### Pull Request Guidelines

- One feature/fix per PR
- Include tests
- Update documentation
- Ensure CI passes
- Respond to review comments

Thank you for contributing to lxa!
