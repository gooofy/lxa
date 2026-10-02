# Reference system (RDD oracle)

Headless AmigaOS 3.1 machines that settle *what* real AmigaOS does
(AGENTS.md §1a). See roadmap "Canonical Reference Configuration".

| File | Purpose |
|---|---|
| `build_refsys.sh` | Builds a fresh Workbench 3.1 HD system from the user's original ADFs into `~/.cache/lxa/refsys/SYS-<profile>` (idempotent, content checksum in `.refsys-checksum`). |
| `Startup-Sequence` | HD startup with the `LXAREF:` hook: starts `C:lxaprobe` (Phase 211) or echoes `LXAREF-READY` to `SER:`. |
| `install_p96.py` | `rtg` profile: installs Picasso96 2.x and the `uaegfx` monitor (tooltype `BOARDTYPE=uaegfx`). |
| `lxa-ref.fs-uae.in` | FS-UAE template: A4000/040 + FPU, AGA, 2 MB chip, 16 MB Z3, KS 3.1 40.70, warp mode, no sound, serial on TCP. |
| `refctl` | Boots, pings, screenshots and stops instances. |

## Quick start

```bash
tools/refsys/build_refsys.sh --profile aga     # ~1 s
tools/refsys/refctl boot --id 0                # ~1.6 s to Workbench (warp)
tools/refsys/refctl ping --id 0
tools/refsys/refctl shot --id 0 --out /tmp/ref.png
tools/refsys/refctl stop --id 0                # or: stop --all
```

## Instances

Instance *N* gets Xvfb display `:N+100`, serial `tcp://127.0.0.1:47100+N`
and the state directory `~/.cache/lxa/refsys/inst/N/`:

- `SYS/` — a fresh copy of `SYS-<profile>` made at every boot, so guest
  writes never leak between runs;
- `exchange/` — mounted as `LXAREF:` (bulk data from `lxaprobe`);
- `logs/`, `shots/`, `base/` — FS-UAE state.

`APPS:` is `../lxa-apps`, mounted read-only, so app manifests resolve
identically on both backends. Processes are tracked by PID in
`state.json`; never use `pkill -f` (it also matches the calling shell).

Measured on the development machine (56 cores): boot to Workbench 1.6 s;
8 instances in parallel 2–3 s each. Savestates are not needed at this boot
speed (and conflict with directory-backed drives).

## Profiles

- `aga` — Workbench on PAL Hires 640×256, 4 colours, topaz 8, default prefs.
- `rtg` — adds Picasso96 2.0 (from the user's AmigaOS 3.9 media,
  `LXA_REF_P96_DIR`) with FS-UAE's `uaegfx` board (4 MB). The card
  initialises; switching Workbench to a uaegfx mode is Phase 211 work
  (needs the mode IDs, which `lxaprobe` enumerates).

## Media (user-supplied, never committed)

- Kickstart 3.1 r40.70 (A4000) and the six Workbench 3.1 (40.42) ADFs in
  `~/media/sys/amiga/os/3.1/` (`LXA_REF_OS_DIR`, `LXA_REF_KICKSTART`).
- Picasso96 2.x for the `rtg` profile.

These are used only to *run* the reference. Clean-room rule: never consult
AmigaOS source or Kickstart disassembly for implementation.
