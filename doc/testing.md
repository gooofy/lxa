# Testing Guide

This document describes how to write, run, and debug tests for lxa.

## Overview

lxa uses a comprehensive testing strategy:
- **Integration Tests**: Test complete workflows from m68k perspective
- **Unit Tests**: Test individual functions in isolation
- **Coverage Analysis**: Ensure code paths are exercised

Testing is **mandatory** - see [AGENTS.md](../AGENTS.md) for quality requirements.

## Integration Tests

Integration tests compile m68k programs and run them via lxa, verifying output and exit codes.

### Test Structure

```
tests/
├── Makefile                # Master test runner
├── Makefile.config         # Paths to lxa, ROM, toolchain
├── test_runner.sh          # Test execution script
├── common/
│   └── test_assert.h       # Assertion helpers
├── fixtures/               # Shared test data
│   ├── fs/                 # Filesystem fixtures
│   └── scripts/            # Test scripts
└── <category>/
    └── <testname>/
        ├── main.c          # Test program (m68k)
        ├── Makefile        # Build rules
        ├── expected.out    # Expected output
        └── run_test.sh     # Optional: custom test runner
```

### Test Categories

- **dos/**: DOS library functionality (files, locks, directories)
- **exec/**: Exec library functionality (tasks, signals, messages)
- **shell/**: Shell features (scripting, control flow)
- **commands/**: Command-line tools (DIR, TYPE, COPY)

### Running Tests

**All tests:**
```bash
cd tests
make test
```

**Single test:**
```bash
make -C tests/dos/helloworld run-test
```

Or:
```bash
cd tests/dos/helloworld
make run-test
```

**Specific category:**
```bash
cd tests/dos
make test
```

### Test Output

Successful test:
```
[TEST] dos/helloworld
Building test...
Running test...
✓ PASS: dos/helloworld
```

Failed test:
```
[TEST] dos/readargs_edge
Building test...
Running test...
--- expected.out
+++ actual.out
@@ -1,2 +1,2 @@
-File: test.txt
+File: (null)
✗ FAIL: dos/readargs_edge
```

### Writing Integration Tests

#### 1. Create Test Directory

```bash
mkdir -p tests/dos/mytest
cd tests/dos/mytest
```

#### 2. Write Test Program

Create `main.c`:
```c
#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

int main(void)
{
    BPTR fh = Open("SYS:test.txt", MODE_OLDFILE);
    if (!fh) {
        Printf("FAIL: Could not open file\n");
        return 20;
    }
    
    UBYTE buffer[128];
    LONG bytes = Read(fh, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        Printf("Read %ld bytes: %s\n", bytes, buffer);
    }
    
    Close(fh);
    return 0;
}
```

#### 3. Create Makefile

Create `Makefile`:
```makefile
include ../../Makefile.config

PROG = mytest

all: $(PROG)

$(PROG): main.c
	$(M68K_CC) $(M68K_CFLAGS) -o $@ $<

run-test: $(PROG)
	@../../test_runner.sh $(PROG) expected.out

clean:
	rm -f $(PROG)

.PHONY: all run-test clean
```

#### 4. Create Expected Output

Create `expected.out`:
```
Read 13 bytes: Hello, World!
```

#### 5. Create Test Data (if needed)

```bash
echo "Hello, World!" > test.txt
```

#### 6. Run Test

```bash
make run-test
```

### Test Helpers

#### test_assert.h

Provides assertion macros for tests:

```c
#include "../common/test_assert.h"

// Assert condition is true
TEST_ASSERT(fh != NULL, "Failed to open file");

// Assert equal
TEST_ASSERT_EQUAL(bytes, 13, "Wrong byte count");

// Assert string equal
TEST_ASSERT_STR_EQUAL(buffer, "Hello", "Wrong content");
```

Example usage:
```c
#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include "../common/test_assert.h"

int main(void)
{
    BPTR lock = Lock("SYS:", SHARED_LOCK);
    TEST_ASSERT(lock != NULL, "Failed to lock SYS:");
    
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    TEST_ASSERT(fib != NULL, "Failed to allocate FIB");
    
    BOOL result = Examine(lock, fib);
    TEST_ASSERT(result, "Examine failed");
    TEST_ASSERT(fib->fib_DirEntryType > 0, "SYS: should be a directory");
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    
    Printf("OK: All assertions passed\n");
    return 0;
}
```

### Advanced Test Scenarios

#### Testing Error Conditions

```c
// Test opening non-existent file
BPTR fh = Open("NONEXISTENT:FILE", MODE_OLDFILE);
if (fh) {
    Printf("FAIL: Open succeeded on invalid path\n");
    Close(fh);
    return 20;
}

LONG err = IoErr();
if (err != ERROR_OBJECT_NOT_FOUND && err != ERROR_DEVICE_NOT_MOUNTED) {
    Printf("FAIL: Wrong error code: %ld\n", err);
    return 20;
}

Printf("OK: Correct error handling\n");
```

#### Testing Multitasking

```c
#include <exec/tasks.h>
#include <clib/exec_protos.h>

void __saveds task_func(void)
{
    struct Task *me = FindTask(NULL);
    Printf("Task %s running\n", me->tc_Node.ln_Name);
    
    Wait(SIGBREAKF_CTRL_C);
    Printf("Task %s received signal\n", me->tc_Node.ln_Name);
}

int main(void)
{
    struct Task *task = CreateTask("TestTask", 0, task_func, 4096);
    TEST_ASSERT(task != NULL, "Failed to create task");
    
    Delay(10);  // Let task start
    Signal(task, SIGBREAKF_CTRL_C);
    Delay(10);  // Let task handle signal
    
    Printf("OK: Multitasking test passed\n");
    return 0;
}
```

#### Testing ReadArgs

```c
#include <dos/rdargs.h>

int main(void)
{
    // Set up command-line arguments
    struct Process *me = (struct Process *)FindTask(NULL);
    me->pr_Arguments = "myfile SIZE=1024 VERBOSE";
    
    LONG args[3] = {0};
    struct RDArgs *rda = ReadArgs("FILE/A,SIZE/K/N,VERBOSE/S", args, NULL);
    
    TEST_ASSERT(rda != NULL, "ReadArgs failed");
    
    STRPTR file = (STRPTR)args[0];
    LONG *size = (LONG *)args[1];
    BOOL verbose = args[2];
    
    TEST_ASSERT_STR_EQUAL(file, "myfile", "Wrong filename");
    TEST_ASSERT(*size == 1024, "Wrong size");
    TEST_ASSERT(verbose, "Verbose not set");
    
    FreeArgs(rda);
    Printf("OK: ReadArgs test passed\n");
    return 0;
}
```

### Custom Test Runners

For tests that need special setup, create `run_test.sh`:

```bash
#!/bin/bash

# Setup
mkdir -p testdir
echo "test data" > testdir/file.txt

# Run test
../../build/host/bin/lxa -s . ./mytest > actual.out 2>&1
status=$?

# Cleanup
rm -rf testdir

# Check results
diff -u expected.out actual.out
if [ $? -eq 0 ] && [ $status -eq 0 ]; then
    echo "✓ PASS"
    exit 0
else
    echo "✗ FAIL"
    exit 1
fi
```

Make it executable:
```bash
chmod +x run_test.sh
```

## Unit Tests

Unit tests verify individual functions in isolation using the Unity framework.

### Location

```
tests/unit/
├── CMakeLists.txt
├── test_config.c       # Tests for config parser
├── test_vfs.c          # Tests for VFS layer
├── stubs.c             # Stubs for emucalls
└── unity/              # Unity framework
```

### Running Unit Tests

```bash
cd build
make test-unit
```

Or with CTest:
```bash
cd build
ctest
```

### Writing Unit Tests

#### 1. Create Test File

Create `tests/unit/test_mymodule.c`:

```c
#include "unity.h"
#include "../../src/lxa/mymodule.h"

void setUp(void)
{
    // Setup before each test
}

void tearDown(void)
{
    // Cleanup after each test
}

void test_myfunction_basic(void)
{
    int result = myfunction(5);
    TEST_ASSERT_EQUAL(10, result);
}

void test_myfunction_null(void)
{
    int result = myfunction_with_ptr(NULL);
    TEST_ASSERT_EQUAL(-1, result);
}

void test_myfunction_large_input(void)
{
    int result = myfunction(1000000);
    TEST_ASSERT_EQUAL(2000000, result);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_myfunction_basic);
    RUN_TEST(test_myfunction_null);
    RUN_TEST(test_myfunction_large_input);
    
    return UNITY_END();
}
```

#### 2. Add to CMakeLists.txt

Edit `tests/unit/CMakeLists.txt`:

```cmake
add_executable(test_mymodule
    test_mymodule.c
    ../../src/lxa/mymodule.c
    unity/unity.c
)

target_include_directories(test_mymodule PRIVATE
    ../../src/lxa
    ../../src/include
    unity
)

add_test(NAME test_mymodule COMMAND test_mymodule)
```

#### 3. Build and Run

```bash
cd build
cmake ..
make test-unit
```

### Unity Assertions

Common Unity assertion macros:

```c
// Integer comparisons
TEST_ASSERT_EQUAL(expected, actual);
TEST_ASSERT_NOT_EQUAL(expected, actual);
TEST_ASSERT_GREATER_THAN(threshold, actual);
TEST_ASSERT_LESS_THAN(threshold, actual);

// Boolean
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);
TEST_ASSERT(condition);

// Pointers
TEST_ASSERT_NULL(pointer);
TEST_ASSERT_NOT_NULL(pointer);
TEST_ASSERT_EQUAL_PTR(expected, actual);

// Strings
TEST_ASSERT_EQUAL_STRING(expected, actual);
TEST_ASSERT_EQUAL_MEMORY(expected, actual, length);

// Floats
TEST_ASSERT_EQUAL_FLOAT(expected, actual);
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual);

// Custom messages
TEST_ASSERT_EQUAL_MESSAGE(expected, actual, "Values should match");
```

### Stubbing Emucalls

Unit tests for ROM code need stubs for emucall functions:

```c
// tests/unit/stubs.c

uint32_t emucall_dos_open(uint32_t name, uint32_t mode)
{
    // Stub implementation for testing
    return 0x12345678;  // Fake file handle
}

uint32_t emucall_dos_read(uint32_t fh, uint32_t buffer, uint32_t length)
{
    // Simulate reading data
    return length;  // Pretend we read everything
}
```

## Code Coverage

### Enabling Coverage

```bash
cmake -DCOVERAGE=ON -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Generating Coverage Reports

```bash
make coverage
```

Output: `build/coverage/index.html`

### Viewing Reports

```bash
firefox build/coverage/index.html
```

### Coverage Targets

- **Critical Functions**: 100% line and branch coverage
  - DOS functions (Open, Read, Lock, etc.)
  - Exec functions (Signal, Wait, etc.)
  - VFS path resolution
  - ReadArgs template parsing
- **Overall Project**: Minimum 95% coverage

### Improving Coverage

1. **Identify uncovered lines**:
   ```bash
   lcov --list coverage.info
   ```

2. **Add tests** for uncovered branches

3. **Run coverage again**:
   ```bash
   make coverage
   ```

## Debugging Tests

### Running Test Under Debugger

```bash
gdb --args ../../build/host/bin/lxa -s . ./mytest
```

GDB commands:
```
break lxa.c:main
run
step
print variable
```

### Enable Debug Output

```bash
../../build/host/bin/lxa -d -s . ./mytest
```

### Enable CPU Tracing

```bash
../../build/host/bin/lxa -t -s . ./mytest
```

Warning: Very verbose!

### Check Test Memory

```bash
valgrind ../../build/host/bin/lxa -s . ./mytest
```

Detects:
- Memory leaks
- Invalid memory access
- Use of uninitialized values

### Test Timeouts

Always use timeouts when running tests - OS tests may hang!

```bash
timeout 10 ../../build/host/bin/lxa -s . ./mytest
```

Or in Makefile:
```makefile
run-test: $(PROG)
	@timeout 10 ../../test_runner.sh $(PROG) expected.out || echo "TIMEOUT"
```

## Test Best Practices

### 1. Test One Thing

Each test should verify a single behavior:

```c
// Good
void test_open_existing_file(void) { ... }
void test_open_nonexistent_file(void) { ... }
void test_open_directory(void) { ... }

// Bad
void test_file_operations(void) {
    // Tests open, read, write, close all in one
}
```

### 2. Use Descriptive Names

```c
// Good
void test_lock_returns_null_for_invalid_path(void)
void test_examine_sets_file_size_correctly(void)

// Bad
void test1(void)
void test_lock(void)
```

### 3. Test Error Conditions

```c
// Test success case
BPTR lock = Lock("SYS:", SHARED_LOCK);
TEST_ASSERT(lock != NULL, "Failed to lock SYS:");

// Test failure case
lock = Lock("INVALID:PATH", SHARED_LOCK);
TEST_ASSERT(lock == NULL, "Lock should fail for invalid path");
TEST_ASSERT_EQUAL(ERROR_OBJECT_NOT_FOUND, IoErr(), "Wrong error code");
```

### 4. Clean Up Resources

```c
int main(void)
{
    BPTR fh = Open("test.txt", MODE_OLDFILE);
    // ... test code ...
    
    if (fh) {
        Close(fh);  // Always cleanup
    }
    
    return 0;
}
```

### 5. Use Fixtures

For tests needing setup:

```bash
# In run_test.sh
mkdir -p fixtures/testdir
cp fixtures/sample.txt fixtures/testdir/
../../build/host/bin/lxa -s fixtures ./mytest
rm -rf fixtures/testdir
```

### 6. Document Test Purpose

```c
/*
 * Test: test_examine_directory
 * Purpose: Verify Examine() correctly identifies directories
 * Setup: Lock SYS: directory
 * Expect: fib_DirEntryType > 0
 */
void test_examine_directory(void)
{
    // ... test code ...
}
```

## Common Test Patterns

### Pattern Matching Tests

```c
UBYTE pattern[256];
LONG result = ParsePattern("#?.txt", pattern, sizeof(pattern));
TEST_ASSERT(result >= 0, "ParsePattern failed");

BOOL match = MatchPattern(pattern, "test.txt");
TEST_ASSERT(match, "Should match");

match = MatchPattern(pattern, "readme.doc");
TEST_ASSERT(!match, "Should not match");
```

### ReadArgs Tests

```c
me->pr_Arguments = "file.txt SIZE=1024";

LONG args[2] = {0};
struct RDArgs *rda = ReadArgs("FILE/A,SIZE/K/N", args, NULL);
TEST_ASSERT(rda != NULL, "ReadArgs failed");

TEST_ASSERT_STR_EQUAL((STRPTR)args[0], "file.txt", "Wrong file");
TEST_ASSERT_EQUAL(*(LONG *)args[1], 1024, "Wrong size");

FreeArgs(rda);
```

### Signal Tests

```c
ULONG sigmask = AllocSignal(-1);
TEST_ASSERT(sigmask != -1, "Failed to allocate signal");

// Signal ourselves
Signal(FindTask(NULL), 1L << sigmask);

// Wait for signal
ULONG received = Wait(1L << sigmask);
TEST_ASSERT_EQUAL(1L << sigmask, received, "Wrong signal received");

FreeSignal(sigmask);
```

## Continuous Integration

### Pre-Commit Checks

Before committing code:

```bash
# Run all tests
make -C tests test

# Check coverage
make coverage
firefox build/coverage/index.html

# Verify no warnings
make 2>&1 | grep warning
```

### Automated Testing

For CI/CD pipelines:

```bash
#!/bin/bash
set -e

# Build
./build.sh

# Run integration tests
cd tests
make test

# Run unit tests
cd ../build
make test-unit

# Check coverage
make coverage

# Verify minimum coverage (95%)
coverage=$(lcov --summary coverage.info | grep lines | awk '{print $2}' | sed 's/%//')
if (( $(echo "$coverage < 95" | bc -l) )); then
    echo "Coverage too low: $coverage%"
    exit 1
fi

echo "All tests passed, coverage: $coverage%"
```
