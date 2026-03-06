# LXA Test Reliability & Performance Report

**Version**: 0.6.59 | **Date**: 2026-03-06 | **Phase**: 75b+ | **Status**: RESOLVED

---

## 1. Executive Summary

The LXA test suite has **38 GTest tests** that are **fully reliable** and support
**full parallelism** (`-j8` or higher). All known intermittent failures have been
resolved.

**Previous Issue**: `ShellTest.Variables` was hanging intermittently (~15-30%
failure rate) due to a clobbered register bug in the m68k scheduler.

**Resolution**: One-line fix in `src/rom/exceptions.s` plus a defensive NULL
guard in `src/rom/exec.c`. See Section 3 for root cause analysis.

**Current Results**:
- 38/38 tests pass at all parallelism levels (`-j1` through `-j38`)
- 50/50 ShellTest.Variables reliability loop passes
- 5/5 consecutive full suite runs at `-j38` pass
- No resource conflicts between parallel test instances

---

## 2. Parallelism & Performance

### 2.1 Benchmark Results (56-core machine)

| Parallelism | Wall Time | CPU Time | Speedup vs -j1 |
|:-----------:|:---------:|:--------:|:---------------:|
| `-j1`       | 801s      | 5m27s    | 1.0x            |
| `-j4`       | 213s      | 5m27s    | 3.8x            |
| `-j8`       | 127s      | 5m29s    | 6.3x            |
| `-j16`      | 126s      | 5m29s    | 6.4x            |
| `-j38`      | 126s      | 5m31s    | 6.4x            |

### 2.2 Analysis

- **Recommended parallelism: `-j8`** — captures nearly all speedup (6.3x).
  Going beyond `-j8` yields no further improvement because the bottleneck is the
  **longest single test** (`gadtoolsgadgets_gtest` at ~126s), not CPU contention.
- **No resource conflicts**: Each test instance runs its own emulator with
  independent memory, no shared files, no network ports. Tests are fully isolated.
- **CPU overhead is constant**: CPU time stays ~5m27-31s regardless of
  parallelism, confirming no contention or thrashing.

### 2.3 Top 10 Longest Tests

| Test                      | Time (s) |
|:--------------------------|:--------:|
| gadtoolsgadgets_gtest     | 126      |
| dpaint_gtest              | 87       |
| cluster2_gtest            | 63       |
| menulayout_gtest          | 45       |
| simplegad_gtest           | 45       |
| simplemenu_gtest          | 33       |
| asm_one_gtest             | 33       |
| api_gtest                 | 31       |
| maxonbasic_gtest          | 30       |
| simplegtgadget_gtest      | 25       |

The critical path is `gadtoolsgadgets_gtest` — any parallelism beyond what
saturates the remaining 37 tests is wasted. With `-j8`, all short tests finish
well before the long ones complete.

### 2.4 Timeout Configuration

Four tests have explicit CTest timeouts in `tests/drivers/CMakeLists.txt`:

| Test                  | Timeout | Actual Time | Margin |
|:----------------------|:-------:|:-----------:|:------:|
| gadtoolsgadgets_gtest | 200s    | ~126s       | 1.6x   |
| simplegad_gtest       | 120s    | ~45s        | 2.7x   |
| easyrequest_gtest     | 120s    | ~10s        | 12x    |
| rgbboxes_gtest        | 25s     | ~11s        | 2.3x   |

All other tests use CTest's default timeout (1500s). No tests are at risk of
timeout failures.

---

## 3. Root Cause Analysis — ShellTest.Variables Hang

### 3.1 The Bug

In `exceptions.s`, the `__do_switch` routine prepares the current task for
preemption by enqueuing it into the TaskReady list:

```asm
__do_switch:
    lea.l   TaskReady(a6), a0       ; a0 = &TaskReady
    jsr     _LVOEnqueue(a6)         ; Enqueue(a0=list, a1=ThisTask)
    move.b  #3, 0xf(a1)            ; BUG: a1 is CLOBBERED here!
```

The C function `_exec_Enqueue()` receives the node pointer in register `a1`,
but internally moves it to `a3` (a callee-saved register). The m68k ABI
treats `a0` and `a1` as scratch registers — they are NOT preserved across
function calls. Disassembly of the compiled `_exec_Enqueue()` confirms:

```asm
__exec_Enqueue:
    link.w a5,#0
    movem.l d2/a2-a4/a6,-(sp)    ; saves a2,a3,a4 -- NOT a1
    move.l  a0,d2                 ; list -> d2
    movea.l a1,a3                 ; node -> a3 (internal use)
    ...
    ; On return, a1 points to an arbitrary list node, NOT the original
```

After `_exec_Enqueue()` returns, `a1` no longer points to ThisTask. The
instruction `move.b #3, 0xf(a1)` writes `TS_READY` (3) to the `tc_State`
field of a **random task** in the list, while the actual preempted task
retains state `TS_RUN` (2).

### 3.2 Consequences

1. **Task in TaskReady with wrong state**: The preempted task sits in the
   ready list with `tc_State = TS_RUN` instead of `TS_READY`.

2. **Random task state corruption**: A different task (the one `a1` happens
   to point to after Enqueue returns) gets its `tc_State` overwritten.

3. **Scheduler confusion**: The `_exec_Schedule` guard that checks
   `tc_State == TS_RUN` before proceeding may behave incorrectly, and the
   corrupted state can lead to double-enqueue of nodes into the TaskReady
   list.

4. **Circular list formation**: A double-enqueued node creates a circular
   `ln_Succ` chain, causing any subsequent list walk (`Enqueue()`,
   `Allocate()`, etc.) to loop forever.

5. **Observed symptom**: The CPU spins in `_exec_Enqueue()` at
   `PC=0x00f82b28` with `SR=0x2704` (supervisor mode, interrupts masked),
   the test hangs permanently.

### 3.3 Why It Was Intermittent

The bug only manifests when:
- An IRQ3 fires at the right time relative to task execution
- The scheduler decides to preempt (quantum expired, higher-priority task
  ready, or TF_EXCEPT set)
- The clobbered `a1` points to a node whose `tc_State` corruption triggers
  a subsequent double-enqueue

This depends on exact timing of VBlank interrupts relative to task execution
progress, which varies between runs.

### 3.4 The Fix

```asm
__do_switch:
    move.b  #3, 0xf(a1)            ; Ready -> ThisTask->tc_State (BEFORE Enqueue)
    lea.l   TaskReady(a6), a0       ; &SysBase->TaskReady -> a0
    jsr     _LVOEnqueue(a6)         ; Enqueue() (a1 may be clobbered)
```

Setting `tc_State = TS_READY` before the Enqueue call avoids the clobbered
register entirely. This is also semantically correct: the task should be
marked as READY before being inserted into the ready list, so that any
concurrent observer (interrupt handler examining list state) sees a
consistent state.

---

## 4. Additional Fix: Signal() NULL Guard

A defensive fix was also applied to `_exec_Signal()` in `exec.c`:

```c
if (!thisTask ||
    ___task->tc_Node.ln_Pri > thisTask->tc_Node.ln_Pri ||
    thisTask->tc_State != TS_RUN)
```

`SysBase->ThisTask` can be NULL when `Signal()` is called from an interrupt
handler after the last running task has called `RemTask()` (which sets
`ThisTask = NULL`). Without this guard, the priority comparison would
dereference NULL.

---

## 5. Previous Diagnostic Changes (All Reverted)

The following changes from earlier debugging sessions were applied in the
working tree but have been **reverted** as they are no longer needed:

| Change | Status | Reason |
|--------|--------|--------|
| `m68k_set_irq(0)` removal in `lxa_api.c` | REVERTED | Caused DeviceTest.ConsoleAsync regression |
| Unconditional `_timer_check_expired()` in EMU_CALL_WAIT | REVERTED | Not needed with scheduler fix |
| EMU_CALL_WAIT diagnostic dumps | REVERTED | Diagnostic instrumentation |
| Double-enqueue guard in `_exec_Enqueue()` | REVERTED | Not needed with root cause fixed |
| Test driver timeout increases | REVERTED | Not needed with root cause fixed |

---

## 6. Test Results

### Before Fix
```
ShellTest.Variables: ~22/30 pass (27% failure rate)
Full suite: 37/38 (ShellTest.Variables intermittent)
```

### After Fix
```
ShellTest.Variables: 50/50 pass (0% failure rate)
Full suite: 38/38 pass (5 consecutive runs at -j38)
```

---

## 7. Files Modified (Final)

| File | Change |
|------|--------|
| `src/rom/exceptions.s` | Move `tc_State = TS_READY` before `Enqueue()` call in `__do_switch` |
| `src/rom/exec.c` | Add NULL guard for `thisTask` in `_exec_Signal()` reschedule check |

---

## 8. Theories Ruled Out During Investigation

| Theory | Status | Evidence |
|--------|--------|----------|
| Stack overflow (4KB too small) | RULED OUT | 16KB stack test: same failure rate |
| `_exec_Allocate()` free list corruption | RULED OUT | PC was in Enqueue, not Allocate |
| Timer delivery failure | RULED OUT | No expired timers in queue during hang |
| CPU contention under -j4 | RULED OUT | Same rate in isolation on 56-core machine |
| DOS Packet race | RULED OUT | LoadSeg uses emucall, not packets |
| `m68k_set_irq(0)` race | NOT ROOT CAUSE | Removing it caused ConsoleAsync regression |
