"""findings.yaml - output of an rdd-review (Phase 214).

    python3 -m rdd findings <findings.yaml>...      validate

Format (one file per reviewed snapshot or scenario):

    scenario: simplegtgadget
    snapshot: startup
    run: build/rdd                      # where the bundles came from
    reviewed: 2026-10-02
    findings:
      - id: simplegtgadget-1
        category: layout                # chrome | gadget-imagery | text | layout |
                                        # missing-content | colour | behaviour | other
        region: {x: 0, y: 12, w: 304, h: 120}   # optional, bundle coordinates
        description: Window content is ~9 px lower than on AmigaOS 3.1.
        evidence:                       # deterministic facts behind the finding
          - "tree screen[Workbench Screen].wbor: lxa [4,11,4,2] ref [4,2,4,2]"
        suspect: {library: intuition, function: OpenScreen}
        action:                         # every finding ends in a phase or an assertion
          phase: 223                    # roadmap phase that owns the fix
          assertion: "tree: screens[0].wbor == reference"   # what will prove it fixed
"""

import sys

import yaml

CATEGORIES = {"chrome", "gadget-imagery", "text", "layout", "missing-content", "colour",
              "behaviour", "other"}


def validate(doc):
    problems = []
    if not isinstance(doc, dict):
        return ["top level must be a mapping"]
    for key in ("scenario", "findings"):
        if key not in doc:
            problems.append("missing %s" % key)
    ids = set()
    for i, f in enumerate(doc.get("findings") or []):
        where = "findings[%d]" % i
        for key in ("id", "category", "description", "evidence", "action"):
            if key not in f:
                problems.append("%s: missing %s" % (where, key))
        if f.get("id") in ids:
            problems.append("%s: duplicate id %s" % (where, f.get("id")))
        ids.add(f.get("id"))
        if f.get("category") not in CATEGORIES:
            problems.append("%s: category %r not in %s" % (where, f.get("category"), sorted(CATEGORIES)))
        if not f.get("evidence"):
            problems.append("%s: needs at least one deterministic evidence line" % where)
        act = f.get("action") or {}
        if not act.get("phase") and not act.get("assertion"):
            problems.append("%s: action needs a phase and/or an assertion" % where)
        reg = f.get("region")
        if reg is not None and not all(k in reg for k in ("x", "y", "w", "h")):
            problems.append("%s: region needs x, y, w, h" % where)
    return problems


def main(paths):
    bad = 0
    for p in paths:
        with open(p) as fh:
            probs = validate(yaml.safe_load(fh))
        for pr in probs:
            print("%s: %s" % (p, pr))
        bad += bool(probs)
        if not probs:
            print("%s: OK" % p)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
