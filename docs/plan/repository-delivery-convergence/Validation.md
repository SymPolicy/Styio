# Validation

**Purpose:** Map each repository-delivery-convergence requirement to concrete repository checks and validation evidence.

**Last updated:** 2026-11-01

## Requirement Mapping

- REQ-001: Check `docs/plan/Manifest.json`, this plan directory, and `Evidence.md` source inventory.
- REQ-002: Run `python <better-plan>/scripts/manifest_tool.py validate docs/plan`.
- REQ-003: Inspect the implementation checkpoint in `Checkpoints.json` and the Open Work Signals section in `Evidence.md`.
- REQ-004: Run a repository search for the retired roots `docs/plans`, `docs/audit`, `docs/specs/audit`, and `docs/milestones` outside `docs/plan`.
- REQ-005: Inspect this validation matrix and final validation checkpoint acceptance criteria.

## Final Checks

1. Manifest validation passes for `docs/plan`.
2. No old planning root remains: `git ls-files | grep -E '^docs/(plans|audit|specs/audit|milestones)/'` returns nothing, and no other planning root exists outside `docs/plan`.
3. Product implementation remains represented as open Better Plan work unless direct code and test evidence proves completion.
