# BCPL programs in lxa

AmigaOS 3.1 still runs BCPL programs: the 1.x `C:` commands, BCPL handlers
and the KS 1.3 `Run`/`NewCLI`.  lxa supports them with its own code in
`src/rom/bcpl/` (Phase 239 made them run, Phase 245 replaced the ported
AROS code with lxa's own implementation; MIT like the rest of lxa).

## The interface (as observed on 3.1)

`tests/probes/dos/entryregs.c` and `tests/probes/dos/bcpl.c` show what 3.1
does; lxa does the same.

- Every command (`RunCommand()`, the shell, the bootstrap) is entered with
  `a2` = the process's global vector `pr_GlobVec` (the system vector
  `dl_GV`), `a5`/`a6` = the call/return routines (`dl_A5`/`dl_A6`) and
  `a1` = the bottom of the stack: BCPL frames grow upwards from there.
- A call is `movea.l G(a2),a4; moveq #framesize,d0; jsr (a5)` with the
  arguments in `d1`-`d4` (more in the caller's frame); the result comes
  back in `d1`.  The new frame starts `d0` bytes above the caller's; its
  arguments are at `0(a1)`, `4(a1)`, ... and the three longs below it hold
  the caller's frame, the return address and the routine's base (`a4`).
  Compiled code also multiplies with `jsr 16(a5)` and divides with
  `jsr 18(a5)`.
- The global vector: slot 0 holds the number of slots (135 for the system
  vector), slot 1 is the program's start, slot 2 `stop`; the system keeps
  further entries at negative offsets (wrappers of dos.library calls).
- A BCPL hunk ends with its global table: its first long is its length in
  longs, its last long the highest global it defines, and below that
  (global, offset) pairs, terminated by 0.  Installing a segment list
  writes those globals into a vector; installing one of the two system
  segments of `pr_SegList` (`[1]`, `[2]`) fills in the system globals (the
  1.3 command start-up code does this for its private vector).
- `stop(code)` unwinds like dos.library `Exit()`: `sp = pr_ReturnAddr - 4;
  rts`.

## lxa's implementation

- `bcpl_entry.S`: the call/return routines, `lxa_bcpl_enter()` (start a
  BCPL routine from C; a top-level call sets `pr_ReturnAddr` the way
  `RunCommand()` does), one 8 byte entry stub per global vector slot, the
  globals that change the control flow (`stop`, `remProcess`, `longcall`),
  a helper for the `execLib`/`libcall` globals and the console segment.
- `bcpl_globals.c`: every other system global, written in C.  An entry
  stub passes the global's offset, the global vector and the frame to
  `lxa_bcpl_dispatch()`, which calls the handler from a table indexed by
  offset.  The text output globals (`writef`, `writes`, `writet`,
  `writei`/`writen`/`writehex`/`writeoct`, `newline`) go through the
  caller's global vector, so a program that replaces `wrch` (or `writes`
  ...) sees the output; lxa's own entries are called directly.
  `rdargs` uses `ReadArgs()` and stores strings as BSTRs behind the
  argument slots.  Also here: building and installing global vectors and
  `dl_GV`/`dl_A5`/`dl_A6` at dos.library start-up.
- `bcpl_cli.c`: `createProcBCPL` (a process on a segment array that waits
  for its start-up packet, builds a private global vector large enough for
  the highest global of its segments and calls global 1 with the packet)
  and global 1 of the console segment (`rn_ConsoleSegment`): it handles
  the start-up packet of KS 1.3 `Run`/`NewCLI` (a `dp_Type` above 1 is the
  caller's own BCPL CliInit routine, which returns the routine that
  replies the packet; otherwise `CliInitNewcli()`/`CliInitRun()`) and runs
  lxa's shell.

Number output: decimal is signed and padded with blanks, hex and octal
print the bit pattern padded with zeros; the field width is a minimum.

Globals without an implementation (coroutines, `longjump`, the system
requesters, `makeGVarea`, `holdTask`, `tidyup`, `openDevInfo`,
`compareTime`) are visible stubs (`LXA_UNIMPLEMENTED`, listed in
`doc/stub-inventory.md`).

## Verification

- Probes `dos/bcpl` and `dos/entryregs` (outputs captured on AmigaOS 3.1).
- The BCPL programs of the Fred Fish corpus (`python3 -m rdd massrun
  lxa-one ...`): DeliTracker `c/Copy`, `c/Echo`, `c/EndCLI`, Wangle
  `Stack`, YachtC3 (`YachtC3`, `loadproc`), Zon `zonfile.05`.
