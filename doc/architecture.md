# lxa Architecture

This document describes the internal architecture of lxa, the Linux Amiga emulation layer.

## Overview

lxa operates by:
1. Loading the AmigaOS kickstart ROM into memory at 0xFC0000
2. Loading the target hunk-format executable into RAM
3. Executing code through the 68k emulator
4. Intercepting AmigaOS library calls and forwarding them to Linux implementations
5. Managing memory, files, and other resources between both systems

## Memory Layout

```
0x000000 - 0x09FFFF    RAM (10 MB default)
0xDFF000 - 0xDFFFFF    Custom Chips (emulated registers)
0xFC0000 - 0xFFFFFF    ROM (256 KB)
```

### Memory Management

- **RAM**: Allocated from Linux heap, exposed to m68k as contiguous memory
- **ROM**: Loaded from lxa.rom file, mapped read-only
- **Memory Access**: All m68k memory accesses go through `m68k_read_memory_*()` and `m68k_write_memory_*()` functions

## Component Architecture

### Host Emulator (src/lxa/)

The host emulator runs as a native Linux process and provides:

#### CPU Emulation (m68k*.c)
- **Musashi Engine**: Cycle-accurate 68000 emulator
- **Instruction Dispatch**: Opcode decode and execution
- **Register State**: D0-D7, A0-A7, PC, SR maintained in emulator context
- **Exception Handling**: Traps, interrupts, illegal instructions

#### Virtual File System (vfs.c/vfs.h)
- **Drive Mapping**: Maps Amiga logical drives (SYS:, Work:, DH0:) to Linux directories
- **Case-Insensitive Resolution**: Handles Amiga's case-insensitive paths on case-sensitive Linux
- **Path Translation**: Converts between Amiga notation (SYS:C/Dir) and Linux paths
- **Special Devices**: NIL:, PIPE:, etc.
- **Multi-Assign Support**: Assigns can have multiple paths, searched in order for file resolution

Features:
- Recursive case-insensitive directory search using `opendir()`/`readdir()`
- Multi-directory assigns with priority ordering (first path searched first)
- `vfs_assign_prepend_path()` adds paths to beginning of assign for priority override
- Automatic tilde expansion (~/.lxa)
- Program-local assigns: On program load, automatically prepend program's subdirectories (s/, libs/, c/, devs/, l/) to standard assigns
- Caching for performance

#### Configuration System (config.c/config.h)
- **INI Parser**: Reads ~/.lxa/config.ini
- **Drive Configuration**: Maps drives to Linux directories
- **System Settings**: ROM path, RAM size, startup options
- **Environment Variables**: LXA_PREFIX, LXA_DATA_DIR overrides

### ROM Implementation (src/rom/)

The ROM is compiled for m68k and provides AmigaOS library implementations:

#### exec.library (exec.c)
Core OS functions:
- **Memory Management**: `AllocMem()`, `FreeMem()`, `AllocVec()`, `FreeVec()`
- **Task Management**: `FindTask()`, `CreateTask()`, `DeleteTask()`
- **Process Management**: `CreateNewProc()`, `Exit()`
- **Signals**: `Wait()`, `Signal()`, `SetSignal()`, `AllocSignal()`, `FreeSignal()`
- **Message Passing**: `PutMsg()`, `GetMsg()`, `ReplyMsg()`, `WaitPort()`
- **Message Ports**: `CreateMsgPort()`, `DeleteMsgPort()`, `AddPort()`, `RemPort()`, `FindPort()`
- **Interrupts**: `AddIntServer()`, `RemIntServer()`, `SetIntVector()`
- **Semaphores**: `InitSemaphore()`, `ObtainSemaphore()`, `ReleaseSemaphore()`, `AttemptSemaphore()`
- **Lists**: `AddHead()`, `AddTail()`, `Remove()`, `Enqueue()`

#### dos.library (lxa_dos.c)
File system and process management:
- **File I/O**: `Open()`, `Close()`, `Read()`, `Write()`, `Seek()`, `FGets()`, `FPuts()`
- **Locks**: `Lock()`, `UnLock()`, `DupLock()`, `ParentDir()`, `CurrentDir()`
- **Directory Examination**: `Examine()`, `ExNext()`, `ExamineFH()`
- **File Operations**: `DeleteFile()`, `Rename()`, `CreateDir()`, `SetProtection()`, `SetComment()`
- **Pattern Matching**: `ParsePattern()`, `MatchPattern()`
- **Argument Parsing**: `ReadArgs()`, `FreeArgs()`
- **Program Loading**: `LoadSeg()`, `UnLoadSeg()`, `Execute()`, `System()`, `SystemTagList()`
- **Error Handling**: `IoErr()`, `SetIoErr()`, `Fault()`
- **Information**: `Info()`, `NameFromLock()`
- **Assignments**: `AssignLock()`, `AssignPath()`, `AssignLate()`, `AssignAdd()`, `RemAssignList()`

#### utility.library (lxa_utility.c)
Utility functions:
- **Tag Lists**: `GetTagData()`, `FindTagItem()`, `NextTagItem()`
- **Hooks**: `CallHookPkt()`
- **String/Math**: Various utility functions

#### Other Libraries
- **graphics.library** (lxa_graphics.c): Stub implementations
- **intuition.library** (lxa_intuition.c): Stub implementations
- **expansion.library** (lxa_expansion.c): Configuration management
- **console.device** (lxa_dev_console.c): Terminal I/O
- **input.device** (lxa_dev_input.c): Input handling

## Emulator Calls (emucalls)

### Communication Mechanism

AmigaOS code running on m68k needs to communicate with the host for certain operations. This is done via **emulator calls** (emucalls):

1. ROM code prepares arguments in m68k registers
2. Executes special instruction that triggers emucall
3. Host intercepts, reads registers, performs operation
4. Host writes results back to m68k memory/registers
5. ROM code continues execution

### Emucall Interface

Defined in `src/include/emucalls.h`:

```c
// System control
#define EMU_CALL_STOP        2   // Stop emulator with return code
#define EMU_CALL_TRACE       3   // Enable/disable tracing
#define EMU_CALL_EXCEPTION   5   // Handle 68k exceptions
#define EMU_CALL_LOADFILE    9   // Load executable
#define EMU_CALL_SYMBOL      10  // Register debug symbol
#define EMU_CALL_GETSYSTIME  11  // Get system time

// DOS library calls
#define EMU_CALL_DOS_OPEN      1000
#define EMU_CALL_DOS_CLOSE     1001
#define EMU_CALL_DOS_READ      1002
#define EMU_CALL_DOS_WRITE     1003
#define EMU_CALL_DOS_SEEK      1004
// ... (many more)
```

### VFS Integration

DOS filesystem operations are handled via emucalls:

1. AmigaOS code calls `Open("SYS:C/Dir", MODE_OLDFILE)`
2. dos.library ROM code sets up emucall with path and mode
3. Host VFS layer resolves "SYS:C/Dir" to Linux path
4. Host opens file using Linux `open()`
5. Returns file handle to m68k code

This WINE-like approach provides:
- **Performance**: Direct Linux system calls, no filesystem emulation overhead
- **Compatibility**: Native Linux file access, permissions, timestamps
- **Simplicity**: No need to implement FFS/OFS filesystem drivers

## Multitasking System

### Signal-Based Task Switching

lxa implements preemptive multitasking using:

1. **50Hz Timer**: Linux `setitimer()` generates SIGALRM every 20ms
2. **Interrupt Handler**: Signal handler triggers m68k Level 3 interrupt
3. **Scheduler**: exec.library switches tasks based on priority and quantum
4. **Task State**: Each task has m68k register context (D0-D7, A0-A7, PC, SR)

### Task Management

Tasks are created via:
- `CreateTask()`: Low-level task creation
- `CreateNewProc()`: High-level process creation with CLI/WorkbenchMsg support

Each task has:
- Task Control Block (struct Task)
- Stack allocated via `AllocMem()`
- Signal allocation bitmap
- Message port for signals/messages

### Message Passing

Message passing provides inter-task communication:

1. Task A creates message, sends via `PutMsg()` to Task B's port
2. Task B waits on port with `WaitPort()` or `Wait()`
3. Signal delivery wakes Task B
4. Task B receives message with `GetMsg()`, processes it
5. Task B replies with `ReplyMsg()`, signaling Task A

## Interrupt System

### Interrupt Levels

```
Level 0: Serial receive
Level 1: Disk DMA/serial transmit
Level 2: CIA ports (keyboard, mouse, timers)
Level 3: Vertical blank (scheduler)
Level 4: Audio
Level 5: Serial transmit
Level 6: CIA-B (timers)
Level 7: NMI
```

Currently implemented:
- **Level 3**: 50Hz vertical blank for multitasking scheduler

### Interrupt Handling Flow

1. Linux timer signal (SIGALRM) arrives
2. Host sets m68k interrupt pending flag
3. m68k emulator checks interrupt pending at instruction boundaries
4. If enabled, jumps to interrupt vector
5. ROM interrupt handler runs, calls scheduler
6. Scheduler switches tasks if needed
7. Returns from interrupt (RTE)

## Build System

### CMake Structure

```
CMakeLists.txt                   # Root configuration
cmake/m68k-amigaos.cmake         # Cross-compilation toolchain
src/lxa/CMakeLists.txt           # Host emulator build
src/rom/CMakeLists.txt           # ROM build (cross-compiled)
sys/CMakeLists.txt               # System commands (cross-compiled)
tests/unit/CMakeLists.txt        # Unit tests
```

### Build Process

1. **Configure**: CMake detects compilers and sets up build
2. **Host Build**: Compile lxa emulator for Linux (gcc)
3. **ROM Build**: Cross-compile ROM for m68k (m68k-amigaos-gcc)
4. **System Build**: Cross-compile C: commands and Shell for m68k
5. **Link ROM**: Combine ROM objects with custom linker script
6. **Install**: Copy binaries to usr/ prefix

### Cross-Compilation

ROM and system code requires m68k-amigaos cross-compiler:

- **Toolchain**: m68k-amigaos-gcc (libnix)
- **Headers**: Amiga includes (exec/, dos/, clib/)
- **Startup**: libnix provides C runtime startup code
- **Linker Script**: Custom ROM linker script for memory layout

## Testing Architecture

### Integration Tests

Located in `tests/`, each test:
1. Compiles m68k program with test code
2. Runs via lxa emulator
3. Captures stdout
4. Compares to expected.out
5. Validates exit code

Tests are organized by library:
- `tests/dos/`: DOS library functionality
- `tests/exec/`: Exec library functionality
- `tests/shell/`: Shell features
- `tests/commands/`: Command-line tools

### Unit Tests

Located in `tests/unit/`, using Unity framework:
- Tests individual functions in isolation
- Host-side functions (VFS, config parser, etc.)
- ROM-side functions (with stubs for emucalls)

### Coverage

Code coverage tracked via gcov/lcov:
```bash
cmake -DCOVERAGE=ON ..
make
make coverage        # Generates HTML report
```

Target: 95%+ line coverage for critical code paths.

## Performance Considerations

### Optimization Strategies

1. **VFS Caching**: Cache directory listings to avoid repeated stat() calls
2. **BPTR Conversion**: Minimize conversions between AmigaDOS BPTR and host pointers
3. **Emucall Batching**: Reduce context switches for repeated operations
4. **Memory Pooling**: Reuse allocated memory blocks

### Profiling

Use standard Linux tools:
```bash
# CPU profiling
perf record -g ./lxa myprogram
perf report

# Memory profiling
valgrind --tool=massif ./lxa myprogram
```

## Future Architecture Considerations

### Graphics Support

Potential approaches:
- **Framebuffer Emulation**: Emulate chipset graphics registers, render to SDL/X11
- **Native Drawing**: Translate Intuition calls to GTK/Qt
- **Hybrid**: Emulate basic graphics, use native for windows/gadgets

### Networking

Amiga networking (bsdsocket.library) could map to:
- Linux socket API (WINE-like approach)
- Bridge AmigaOS network stack to Linux TCP/IP

### Audio

Emulate audio hardware or map to:
- ALSA/PulseAudio
- OpenAL
- SDL audio
