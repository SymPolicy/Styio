# Add Repo File

**Purpose:** Add a repository file with the required indexes, metadata, gates, and ownership updates.

**Last updated:** 2026-04-23

**TOML:** [ADD-REPO-FILE.toml](./ADD-REPO-FILE.toml) is the machine-readable workflow definition.

## Skill

Use [styio-file-onboarding/skill.toml](./skills/styio-file-onboarding/skill.toml) when adding tracked files or directories.

## Workflow

1. Classify the new file: source, test, script, doc, workflow, skill, config, or fixture.
2. Apply the local naming and metadata rules for that class.
3. Update indexes, registries, catalogs, and gates that discover that class.
4. Add or update the smallest useful test for the new file class.
5. Run the narrow gate first, then the relevant repo gate.

## Required Evidence

1. File path and class.
2. Registry or index touched, or why none is needed.
3. Test or gate that would fail if the file were incomplete.
4. `git diff --check` result.

## Gates

```bash
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
python3 scripts/repo-hygiene-gate.py --mode tracked
git diff --check
```

## Handoff

Report the file class, touched registries, gates run, and any intentionally deferred broader test.
