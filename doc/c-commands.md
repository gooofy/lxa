# C: commands and shell

lxa's `C:` commands (`sys/C/*.c`) and its shell (`sys/System/Shell.c`)
reproduce the AmigaOS 3.1 commands: same templates, output layout, messages
and return codes. Phase 221 checks this against the reference machine with
the shell parity scripts.

## Shell parity scripts

`tests/shell_parity/<name>.script` runs as an AmigaDOS command file
(`SYS:Tests/ShellParity/Run <name>`: `Execute` in a shell). On the reference
the WB 3.1 `C:` commands run, on lxa its own commands and shell. The
reference output is the golden `<name>.ref.out`:

```bash
cd tools
python3 -m rdd suite-ref --filter ShellParity --capture-ref   # reference -> <name>.ref.out
python3 -m rdd suite-ref --lxa-check --filter ShellParity     # lxa vs golden (= ctest ref_expected_outputs_*)
```

Fixtures live in `tests/shell_parity/fixtures/` (at
`SYS:Tests/ShellParity/fixtures`). Scripts create their files in their own
`work-<name>` directory and give them fixed dates with `SetDate`.

The comparison normalises only what is machine-dependent:

- the boot volume is `System:` on the reference and `SYS:` on lxa;
- the time of day (`hh:mm:ss`), because the reference clock runs;
- all numbers, in scripts marked `; parity: numbers` (`avail.script`).

| Script | Covers |
|---|---|
| `echo` | Echo (NOLINE, FIRST, LEN, `*` escapes), comments, FailAt, Why, Fault, Quit, unknown commands |
| `vars` | Set/Get/Unset, SetEnv/GetEnv/UnSetEnv, `$var`, `${var}`, `*$`, `` `cmd` ``, Alias/Unalias with `[]` |
| `control` | If/Else/EndIf (EQ/GT/GE/VAL/NOT/EXISTS/WARN/ERROR/FAIL, nesting), Skip/Lab (BACK, no label), stray Else |
| `execute` | Execute with .key/.def/.bra/.ket/.dollar, nesting, shared variables, failing command files |
| `files` | MakeDir, Type (HEX/NUMBER), SetDate, Protect, Filenote, Rename, Copy, Join, Search, Sort, Delete |
| `list` | List (NODATES/DATES/BLOCK/NOHEAD/QUICK/LFORMAT/ALL/FILES/DIRS/PAT/SINCE/UPTO/SUB) and Dir |
| `copy` | Copy (to dirs, ALL, QUIET, CLONE/DATES/COM/NOPRO, errors), Join, Sort (CASE/NUMERIC/COLSTART), Search options |
| `paths` | Path, Which, Assign (EXISTS/ADD/REMOVE/DEFER/PATH), CD, Stack, Resident |
| `misc` | Eval, Wait, Date (show/set), Version, Status, ReadArgs errors of many commands |
| `avail` | Avail layout |
| `newcmds` | AddBuffers, Lock, Relabel, LoadResource, SetFont, SetKeyboard, SetPatch, IconX |

Not covered, because the output depends on the machine or on things the
reference cannot do the same way: the order of entries in directories with
more than one entry (it is the host file system's), `Info` values, the full
`Status` list, `CPU` (lxa reports a 68030 without FPU until Phase 240),
`List KEYS` (disk block numbers), interactive commands (`Ask`,
`RequestChoice`, `RequestFile`, `Dir INTER`, `?` prompts). `AddBuffers`,
`DiskChange` and `Lock` on a device that does not exist crash AmigaOS 3.1
itself.

## WB 3.1 C: commands lxa does not ship

Decided in Phase 221. Every command of the WB 3.1 `C:` directory is either
implemented in `sys/C` or listed here.

| Command | Decision |
|---|---|
| AddBuffers | implemented (dos `AddBuffers()`, ACTION_MORE_CACHE) |
| AddDataTypes | out of scope for now: lxa's datatypes.library has its types built in and no descriptor list to add to; belongs to the datatypes.library completion (M5) |
| BindDrivers | out of scope: loads Zorro expansion drivers (hardware) |
| ConClip | out of scope: console cut-and-paste commodity; lxa's consoles are host windows (console work, not shell parity) |
| CPU | implemented (AttnFlags, CacheControl); parity test waits for the 68040 model of Phase 240 |
| DiskChange | implemented (dos `Inhibit()` on/off) |
| Ed | out of scope: full-screen editor application, not a shell command; use the host's editor or WB 3.1's binary |
| Edit | out of scope: line editor application (same reason as Ed) |
| IconX | implemented (shell and Workbench start, WINDOW/DELAY tooltypes) |
| Install | out of scope: writes floppy boot blocks (hardware) |
| IPrefs | Phase 236 (preferences) |
| LoadResource | implemented (libraries, devices, fonts, other loadable files) |
| LoadWB | Phase 264 (Workbench) |
| Lock | implemented (ACTION_WRITE_PROTECT) |
| MagTape | out of scope: SCSI tape control (hardware) |
| Mount | out of scope for now: needs real DOS handlers in the DosList (Phase 255); lxa's devices are mapped by the host |
| Relabel | implemented (dos `Relabel()`) |
| RemRAD | out of scope: removes the recoverable RAD: disk (hardware RAM drive) |
| RequestFile | implemented (asl.library) |
| SetClock | out of scope: battery-backed clock hardware; lxa's system clock is the host's (or the virtual clock); `Date` sets it |
| SetDate | implemented |
| SetFont | implemented (font of the shell's console window) |
| SetKeyboard | implemented (keymap.resource, DEVS:Keymaps, keymap.library and console.device defaults) |
| SetPatch | implemented: lxa's ROM needs no patches, SetPatch reports that (silent with QUIET) |
| Which | implemented |

lxa additionally has `GetEnv`, `Set`, `SetEnv`, `Unset`, `UnSetEnv`,
`Stack`, `Resident` and `Run` as files; on AmigaOS 3.1 they are internal
commands of the shell (lxa's shell also has them built in).

## The shell

`SYS:System/Shell` follows the 3.1 shell:

- `System()` runs its command line in a shell (redirection, variables,
  aliases); `Execute()` in a shell that reads further commands from the
  input.
- Commands run in the shell's process through `RunCommand()` (in-process, on
  a fresh stack of at least 16 KB), so they share its CLI, current directory
  and local variables. Search order: resident list, current directory, path,
  `C:`; files with the `s` bit run through `Execute`.
- `RC` and `Result2` after every command (`Result2` is 0 after success); a
  return code at or above the fail limit stops all command files
  (`<cmd> failed returncode <rc>`).
- In a command file, commands read their input from the command file (Eval,
  `?` prompts).
- Internal commands: Alias Ask CD Echo Else EndCLI EndIf EndShell EndSkip
  FailAt Fault Get GetEnv If Lab NewCLI NewShell Path Prompt Quit Resident
  Run Set SetEnv Skip Stack Unalias Unset UnSetEnv Why and the `.key`,
  `.bra`, `.ket` directives; they are on the resident list as INTERNAL.
- `C:Execute` hands its (directive-processed) command file to the shell
  through `cli_CurrentInput`; the shell returns to the previous input at its
  end and deletes `T:Command-*` files.
