# Add Repo File

**Purpose:** Add a repository file with the required indexes, metadata, gates, and ownership updates.

**Last updated:** 2026-06-28

**TOML:** [ADD-REPO-FILE.toml](./ADD-REPO-FILE.toml) is the machine-readable workflow definition.

## Skill

Use [styio-file-onboarding/skill.toml](./skills/styio-file-onboarding/skill.toml) when adding tracked files or directories.

## Workflow

1. Search existing docs, ADRs, generated indexes, owner SSOTs, runbooks, workflows, plans, and rollups before adding a tracked file.
2. Prefer maintaining an existing owner file. Add a new file only when it has a single responsibility that no existing file should own.
3. Classify the new file: source, test, script, doc, workflow, skill, config, or fixture.
4. Apply the local naming and metadata rules for that class, including the ban on version-style placeholders such as `v2`, `new`, `old`, `legacy`, or `latest`; name by feature or transformation result.
5. Replace developer-machine paths, server-machine paths, private endpoints, account names, hostnames, and deployment roots with placeholders.
6. Update indexes, registries, catalogs, and gates that discover that class.
7. Add or update the smallest useful test for the new file class.
8. Run the narrow gate first, then the relevant repo gate.

## Required Evidence

1. File path and class.
2. Existing owner files searched and why they could not carry this content.
3. Naming check proving the file is named by feature or transformation result, not a version-style placeholder.
4. Local/server info placeholder check and `local-info-leak-gate.py` result.
5. Registry or index touched, or why none is needed.
6. Test or gate that would fail if the file were incomplete.
7. `git diff --check` result.

## Gates

```bash
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
python3 scripts/local-info-leak-gate.py --mode worktree
python3 scripts/repo-hygiene-gate.py --mode tracked
git diff --check
```

## Handoff

Report the file class, touched registries, gates run, and any intentionally deferred broader test.
