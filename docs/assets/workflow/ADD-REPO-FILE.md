# Add Repo File

**Purpose:** Mirror the root workflow for adding repository files with indexes, metadata, gates, and ownership updates.

**Last updated:** 2026-04-23

Canonical workflow: [../../../workflows/ADD-REPO-FILE.md](../../../workflows/ADD-REPO-FILE.md)

## Workflow

1. Classify the file.
2. Apply naming and metadata rules.
3. Update indexes, registries, catalogs, and gates.
4. Add the smallest useful test.
5. Run the narrow gate and relevant repo gate.

## Gates

```bash
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
python3 scripts/repo-hygiene-gate.py --mode tracked
git diff --check
```
