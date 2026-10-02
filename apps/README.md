# lxa App Corpus

Real AmigaOS applications used for compatibility testing live **outside**
this repository in the sibling directory `../lxa-apps/` (they are not
redistributable, and that directory is not version-controlled). This
directory holds the metadata that *is* checked in:

- `<App>.json` — one manifest per app (below).
- `compat.yaml` — the compatibility database (Phase 230): one entry per app with
  its rating, the scenarios behind the rating, the last tested lxa version and
  the open divergences, each linked to a roadmap phase number.

## Manifest requirement

**Every app in `../lxa-apps/` must have a manifest `apps/<App>.json` here.**
Apps without a manifest are not part of the corpus: the scenario runner
(`tools/rdd`), the sweeps (Phase 231) and the compat DB ignore them, and
`tools/rdd/corpus.py --check` reports them as errors. (Older manifests inside
`../lxa-apps/<App>/app.json` are migrated here.)

Minimal manifest (`apps/Asm-One.json`):

```json
{
    "dir": "Asm-One",
    "name": "ASM-One",
    "version": "1.48",
    "description": "68000 assembler IDE for AmigaOS",
    "executable": "bin/ASM-One/ASM-One_V1.48",
    "assigns": { "LIBS": "bin/ASM-One/Libs" },
    "libraries": ["reqtools.library"],
    "env": { "WORKDIR": "bin/ASM-One" },
    "requirements": { "graphics": true, "intuition": true, "kickstart_version": 37 },
    "test": { "timeout": 15, "expected_returncode": 0, "args": "" },
    "notes": []
}
```

| Field | Meaning |
|---|---|
| `dir` | Directory under `../lxa-apps/` (= `APPS:<dir>` on both backends). |
| `name`, `version`, `description` | Identification, shown in the compat dashboard. |
| `executable` | Path relative to the app directory. |
| `assigns` | Logical assigns (relative to the app directory) the app needs, declared explicitly so both backends get them (real AmigaOS does not add a program's `libs/` to `LIBS:`). System assigns (`LIBS`, `FONTS`, `DEVS`, `S`, `L`, `C`, …) are extended (`ADD`), others are created. |
| `libraries` | Disk libraries the app opens. Third-party libraries must be present as real binaries (AGENTS.md §1: never stubbed). |
| `env.WORKDIR` | Current directory at launch, relative to the app directory. |
| `requirements` | `graphics`/`intuition` flags, minimum Kickstart version, optional `profile` (`aga` / `rtg`). |
| `test` | Defaults for the shallow sweep: timeout (seconds), expected return code, arguments. |
| `source` (optional) | Where the binary came from (Fish disk, Aminet path, vendor), for provenance. |

On the reference machine the same tree is mounted read-only as `APPS:`, so the
paths in the manifest resolve identically on both backends.
