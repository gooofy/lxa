# lxa Application Database (App-DB)

To ensure `lxa` remains compatible with real-world Amiga software, we maintain a collection of Amiga applications for integration testing. 

## Copyright & Repository Separation
**CRITICAL**: The App-DB contains copyrighted material. It MUST NOT be committed to the `lxa` repository.
- **Location**: The App-DB should reside in a sibling directory to the `lxa` project root (e.g., `../lxa-apps/`).
- **Exclusion**: A `.gitignore` entry in the `lxa` root ensures that even if an `apps/` directory is created locally for convenience, it is never pushed.

## Structure
The App-DB is organized by application name and version:
```
lxa-apps/
  ├── kickpascal-1.0/
  │   ├── app.json        # Metadata and test configuration
  │   ├── bin/            # The actual Amiga binaries
  │   └── expected/       # Expected output/logs for automated tests
  └── procalc-2.1/
      ├── app.json
      └── ...
```

## Manifest Format (`app.json`)
Each application includes a manifest describing its requirements and how to execute it:
```json
{
  "name": "KickPascal",
  "version": "1.0",
  "category": "Development",
  "complexity": "low",
  "entry_point": "C:KickPascal",
  "arguments": "test.pas",
  "requirements": {
    "libraries": ["dos.library", "utility.library"],
    "fonts": ["topaz.font"],
    "assigns": {
      "KICKPASCAL:": "./"
    }
  },
  "test_cases": [
    {
      "name": "compile_hello",
      "args": "hello.pas",
      "expected_output": "expected/hello.out",
      "timeout": 5
    }
  ]
}
```

## Integration Testing Workflow
A dedicated test runner (`tests/run_apps.py`) will:
1. Scan the `../lxa-apps/` directory.
2. For each app found, create a temporary `SYS:` environment.
3. Apply the required `assigns`.
4. Run `lxa` with the specified entry point and arguments.
5. Compare the output against the `expected/` files.

## Directory Opus Runtime Surface Audit

The current App-DB copy lives at `../lxa-apps/DirectoryOpus/bin/DOPUS/` and is a self-contained disk layout rather than a ROM feature request. The launch path and bundled payload establish the runtime surface that `lxa` must provide.

### Required Assigns And Search Paths

- `DOpus:` should point at `APPS:DirectoryOpus/bin/DOPUS/`
- `LIBS:` must include `APPS:DirectoryOpus/bin/DOPUS/libs/` so bundled third-party libraries stay disk-provided
- `C:` must include `APPS:DirectoryOpus/bin/DOPUS/c/` for bundled helper commands
- `L:` must include `APPS:DirectoryOpus/bin/DOPUS/l/` for bundled handlers
- `S:` must include `APPS:DirectoryOpus/bin/DOPUS/s/` for startup/config scripts
- `T:`, `RAM:`, `NIL:`, and `FONTS:` are referenced during launch and must resolve normally

### Bundled Disk-Provided Dependencies

The DOpus app bundle already ships these libraries under `APPS:DirectoryOpus/bin/DOPUS/libs/` and they must remain outside ROM scope:

- `dopus.library`
- `powerpacker.library`
- `arp.library`
- `iff.library`
- `Explode.Library`
- `Icon.Library`

The bundle also ships helper commands under `APPS:DirectoryOpus/bin/DOPUS/c/` (`Assign`, `Execute`, `GoWB`, `InitLib`, `NewCLI`, `Run`, `LoadWB`, `ppmore`, `lha`, and others) plus `L:Ram-Handler` under `APPS:DirectoryOpus/bin/DOPUS/l/`.

### Startup Sequence Observations

`APPS:DirectoryOpus/bin/DOPUS/s/startup-sequence` currently contains:

```text
InitLib >NIL:
Assign T: RAM:
DF0:DirectoryOpus
gowb
```

String scans of the main `DirectoryOpus` binary also show startup-time references to:

- `DOpus:S`, `DOpus:S/DirectoryOpus`, and `S:DirectoryOpus`
- `S:Shell-Startup`, `S:DOpus-Startup`, and `S:DOpusShell-Startup`
- `Execute S:DOpus-Startup` and `Execute S:DOpusShell-Startup`

This means launch coverage must validate both the app-local `S:` content and any shell startup hand-off path that DOpus chooses at runtime.

### Observed Runtime Library And Device Surface

String scans of `APPS:DirectoryOpus/bin/DOPUS/DirectoryOpus` show direct references to these system components during startup or early runtime:

- Libraries: `dos.library`, `intuition.library`, `diskfont.library`, `icon.library`, `workbench.library`, `commodities.library`, `rexxsyslib.library`, `utility.library`, `services.library`, `accounts.library`, `powerpacker.library`
- Devices: `keyboard.device`, `input.device`, `audio.device`, `timer.device`, `printer.device`, `console.device`

Some of these are optional feature paths rather than guaranteed first-frame requirements, but they define the compatibility surface that the remaining Phase 81 bring-up work must audit against real launch logs and host-side drivers.
