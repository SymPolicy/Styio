# Docs Maintenance Workflow

**Purpose:** Define the repeatable workflow for maintaining `docs/` metadata, generated indexes, archive lifecycle state, and structural validation.

**Last updated:** 2026-06-28

## Goals

1. Keep directory boundaries stable.
2. Keep collection indexes current without hand-editing every inventory list.
3. Fail fast when links, metadata, or naming rules drift.
4. Prefer updating existing owner documents over creating parallel docs.
5. Keep ADRs as current decision records, not old/new decision history bundles.

## Commands

```bash
python3 scripts/docs-scaffold.py --help
python3 scripts/docs-index.py --write
python3 scripts/docs-lifecycle.py validate
python3 scripts/docs-audit.py
python3 scripts/docs-lifecycle.py candidates --family all --format tree
python3 scripts/docs-audit.py --manifest valid --format tree
python3 scripts/docs-audit.py --manifest invalid --format list
ctest --test-dir build/default -L docs --output-on-failure
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

## Workflow

1. Search existing docs, generated indexes, ADRs, owning SSOTs, runbooks, workflows, plans, and rollups before creating a docs file.
2. Update an existing owner document when it can carry the change.
3. Use `python3 scripts/docs-scaffold.py ... --reuse-reviewed` only when a new single-purpose docs file or collection directory is still necessary.
4. Edit docs or move files.
5. Regenerate directory inventories with `python3 scripts/docs-index.py --write`.
6. Run `python3 scripts/docs-lifecycle.py validate` locally.
7. Run `python3 scripts/docs-audit.py` locally.
8. Print lifecycle candidates with `python3 scripts/docs-lifecycle.py candidates --family all --format tree` when planning compression / archive work.
9. Print the valid worktree-document tree with `python3 scripts/docs-audit.py --manifest valid --format tree` when you need a repository-wide inventory.
10. Print the invalid worktree-document list with `python3 scripts/docs-audit.py --manifest invalid --format list` when you need deletion / relocation review.
11. Use `python3 scripts/docs-audit.py --manifest valid --format json --output /tmp/styio-docs.json` when you need structured export, including aggregated `character_count` / `word_count` statistics and per-file text volume.
12. Use `--source git` for tracked-only export, or `--source filesystem` when you intentionally want to inspect local build output, vendored dependencies, or generated report Markdown currently present in the worktree.
13. If the repo is already configured, run `ctest --test-dir build/default -L docs --output-on-failure`.
14. For checkpoint-grade verification, run `./scripts/checkpoint-health.sh --no-asan --no-fuzz`.

## Rules

1. Collection-directory `README.md` files describe scope, naming, and maintenance rules.
2. Collection-directory `INDEX.md` files are generated inventories.
3. Every `docs/**/*.md` file must expose top-level `Purpose` and `Last updated` metadata.
4. Archive lifecycle truth lives in `docs/archive/ARCHIVE-MANIFEST.json`; `ARCHIVE-LEDGER.md` is generated.
5. Broken relative links and stale generated indexes are gate failures for active docs.
6. Repository-wide Markdown inventory and invalid-document review both run through `scripts/docs-audit.py --manifest ...`.
7. `docs-scaffold.py` requires `--reuse-reviewed`; do not create a document before checking whether an existing owner should be maintained instead.
8. Active ADR files must record the current decision only. Do not add old/new decision-history sections to active ADRs.
9. Documentation and workflow artifacts must not use version-style names such as `v2`, `version`, `new`, `old`, `legacy`, or `latest`; use the feature or transformation result.
10. Documentation and skills must not expose developer-machine or server-machine details; use placeholders and run `python3 scripts/local-info-leak-gate.py --mode worktree`.
