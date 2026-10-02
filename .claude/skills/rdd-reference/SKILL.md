---
name: rdd-reference
description: Operate the real AmigaOS 3.1 reference machine (FS-UAE + lxaprobe agent) and the twin runner - write scenarios, run them on lxa and the reference, promote reference goldens. Use whenever an expected value is needed (AGENTS.md §1a rule 1).
---

# rdd-reference — the AmigaOS 3.1 oracle

AGENTS.md §1a: real AmigaOS 3.1 settles *what* happens. Never hand-write an
expected value that the reference can produce. Details of the machine:
`tools/refsys/README.md`; bundle format: `doc/rdd-snapshot.md`.

## 1. Prerequisites (user-supplied, never committed)

- Kickstart 3.1 r40.70 + Workbench 3.1 ADFs in `~/media/sys/amiga/os/3.1/`
  (`LXA_REF_OS_DIR`, `LXA_REF_KICKSTART`); Picasso96 for `profile: rtg`.
- `fs-uae`, `Xvfb`, Python 3 with PyYAML and Pillow.
- Check: `ctest --test-dir build -R refsys_selftest` passes (skips, exit 77,
  when the media are missing — then STOP and tell the user; do not invent
  reference values).

Clean-room: the media are only *run*. Never read AmigaOS source or
Kickstart disassembly.

## 2. Manual probing

```bash
tools/refsys/build_refsys.sh --profile aga      # idempotent
tools/refsys/refctl boot --id 0
tools/refsys/refctl cmd --id 0 "RUN SYS:Utilities/Clock" "WAIT_WINDOW Clock" \
    "WAIT_IDLE" "DUMP_TREE tree.json" "SNAP screen.snap" "QUIT"
ls ~/.cache/lxa/refsys/inst/0/exchange/          # LXAREF: = this directory
tools/refsys/refctl stop --id 0
```

Instance N: Xvfb `:N+100`, serial `tcp://127.0.0.1:47100+N`. IDs 0–19 are
for manual work, 20–39 belong to the twin runner pool. Never `pkill -f`
(it matches your own shell); use `refctl stop`.

Agent commands (`tools/refsys/agent/lxaprobe.c` header): PING, RUN, ASSIGN,
DELAY, WAIT_WINDOW, WAIT_IDLE, DUMP_TREE, SNAP, MOVE/CLICK/PRESS/RELEASE,
KEY, TYPE, MENU, TEXT_START/DUMP/STOP, TRACE/TRACE_DUMP/TRACE_STOP, MODES,
QUIT, SETCLOCK, BYE.

## 3. Scenarios and the twin runner

A scenario (`tests/scenarios/*.yaml`, DSL in `tools/rdd/scenario.py`) runs
unchanged on both backends:

```yaml
name: gadtoolsgadgets
app: {sample: GadToolsGadgets}         # or {manifest: DPaintV} -> apps/DPaintV.json
steps:
  - launch
  - wait_window: "GadTools Gadget Demo"
  - wait_idle
  - snapshot: {name: startup, window: "GadTools Gadget Demo"}
  - quit
```

```bash
cd tools
python3 -m rdd run ../tests/scenarios/x.yaml --out ../build/rdd   # both backends, -j 8
python3 -m rdd report ../build/rdd                                # compare + composites
```

- lxa runs are deterministic (virtual clock, epoch 2024-01-01); the
  reference clock is set to the same epoch by `SETCLOCK`.
- Reference runs are cached in `~/.cache/lxa/rdd/ref/` keyed by scenario,
  app binaries, refsys checksum and agent source; `--no-cache` forces a
  re-run (e.g. after a reference looked wrong).
- Apps come from `apps/<App>.json` + `../lxa-apps` (`LXA_APPS`), mounted as
  `APPS:` on both sides.

## 4. Goldens

```bash
python3 -m rdd promote ../tests/scenarios/x.yaml --phase N   # owner of today's divergence
python3 -m rdd golden --all --build ../build                 # = ctest -L golden
```

Promote only after reviewing the comparison (`rdd-review` skill). A golden
records the reference bundle plus a ratchet of the current lxa divergence;
after a fix, `golden` prints `tighten` — re-promote so the gain is locked in.
Never edit a golden's reference bundle (rule 4).

## 5. Troubleshooting

- `ERR timeout` from WAIT_WINDOW: check the title (exact prefix match) and
  that the program exists on the reference (`LXAREF:bin/` for samples).
- Reference screenshot for a human: `refctl shot --id N --out x.png`.
- Agent changes need `build_refsys.sh` (it rebuilds `C:lxaprobe`) and
  invalidate the reference cache automatically.
