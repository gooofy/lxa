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
