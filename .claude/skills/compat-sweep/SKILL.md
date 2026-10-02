---
name: compat-sweep
description: Breadth-first compatibility work - sweep many apps on lxa and the AmigaOS reference, triage in parallel (one subagent per app), cluster divergences by root cause, turn clusters into roadmap phases. Includes the autonomous /loop iteration (python3 -m rdd loop).
---

# compat-sweep — breadth before depth (AGENTS.md §1a rule 3)

## 1. One loop iteration

```bash
cd tools
python3 -m rdd loop --build ../build --report ../doc/sweeps/<date>-loop.md
```

This runs every `tests/scenarios/*.yaml` on both backends, writes the
comparison (`build/rdd/report.html`), replays all goldens, clusters every
divergence (`build/rdd/clusters/clusters.json`) and writes the sweep report
with phase stubs for **unowned** clusters seen in ≥ 2 scenarios. Exit code 1
means: a golden regressed or an unowned multi-app cluster exists — both need
action before anything else (priority: Quality first).

For `/loop`: use `/loop python3 -m rdd loop …` with a long interval (the
reference cache makes unchanged scenarios cheap); on each wake, act only on
a non-zero exit (see §4).

## 2. Parallel triage (one subagent per app)

When a sweep shows divergences in several apps, do not review them serially
in the main context:

1. The coordinator lists the scenarios with verdict ≠ `identical` from
   `build/rdd/compare-summary.json`.
2. It launches **one subagent per app**, all in one message, each with
   `isolation: "worktree"` and this brief:
   > Load the `rdd-review` skill. Review `build/rdd/<scenario>/compare/*`
   > (compare.json first, then composite.png). Write
   > `doc/findings/<date>-<scenario>.yaml` (validated with
   > `python3 -m rdd.findings`). Do not change lxa code. Return the file.
   Read-only review; subagents never edit the roadmap.
3. The coordinator collects the findings files and clusters them together
   with the mechanical tree clusters:
   `python3 -m rdd cluster ../build/rdd --findings ../doc/findings/<date>-*.yaml`
4. Only now are phases opened (§3). Fixes are then done per cluster, not per
   app — one root cause usually repairs several apps.

## 3. From cluster to phase

- A cluster is *owned* when a golden records it with a phase, a finding names
  a phase, or the roadmap mentions it. Owned clusters need nothing new.
- Unowned cluster with ≥ 2 scenarios: take its stub from
  `clusters/phase-stubs.md`, review it with the user, then add it to
  `roadmap.md` as a numbered phase in the right milestone (M4/M5 for library
  gaps). No pooling sections.
- Unowned single-scenario cluster: a TODO in that app's phase (Phase 224 for
  legacy apps, the app's own phase otherwise).
- After fixing, re-run the loop; promote goldens that print `tighten`.

## 4. Reacting to a loop failure

1. Golden FAIL → a regression. Bisect with `git bisect run python3 -m rdd golden <dir> --build ../build`
   (rebuild in the run script); fix before any other work.
2. New unowned cluster → §3.
3. Reference run failed → check `build/rdd/<scenario>/ref.log`; never
   promote while the reference side is broken.
