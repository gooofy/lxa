# attic/

Retired files kept for historical reference. Not built, not tested, not shipped.

## Contents

- `PHASE2_IMPLEMENTATION.md`, `PHASE4_IMPLEMENTATION.md` — early phase implementation notes superseded by `roadmap.md`.
- `screenshot_review.py` — retired Phase 136-c (v0.9.17). Was an OpenRouter-backed CLI helper for sending GTest failure artifacts to a vision model. The current OpenCode coding harness supports image input natively, so failure PNGs can be passed directly to the assistant without an external tool. See `.opencode/skills/graphics-testing/SKILL.md` §8 for the current visual-investigation workflow.
- `test_screenshot_review.py` — pytest unit tests for the retired tool.
