# Docs Maintenance Workflow

**Purpose:** Define the repeatable workflow for maintaining `docs/` metadata, generated indexes, archive lifecycle state, and structural validation.

**Last updated:** 2026-04-23

## Goals

1. Keep directory boundaries stable.
2. Keep collection indexes current without hand-editing every inventory list.
3. Fail fast when links, metadata, or naming rules drift.

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

1. Use `python3 scripts/docs-scaffold.py ...` when creating a docs file or collection directory.
2. Edit docs or move files.
3. Regenerate directory inventories with `python3 scripts/docs-index.py --write`.
4. Run `python3 scripts/docs-lifecycle.py validate` locally.
5. Run `python3 scripts/docs-audit.py` locally.
6. Print lifecycle candidates with `python3 scripts/docs-lifecycle.py candidates --family all --format tree` when planning compression / archive work.
7. Print the valid worktree-document tree with `python3 scripts/docs-audit.py --manifest valid --format tree` when you need a repository-wide inventory.
8. Print the invalid worktree-document list with `python3 scripts/docs-audit.py --manifest invalid --format list` when you need deletion / relocation review.
9. Use `python3 scripts/docs-audit.py --manifest valid --format json --output /tmp/styio-docs.json` when you need structured export, including aggregated `character_count` / `word_count` statistics and per-file text volume.
10. Use `--source git` for tracked-only export, or `--source filesystem` when you intentionally want to inspect local build output, vendored dependencies, or generated report Markdown currently present in the worktree.
11. If the repo is already configured, run `ctest --test-dir build/default -L docs --output-on-failure`.
12. For checkpoint-grade verification, run `./scripts/checkpoint-health.sh --no-asan --no-fuzz`.

## Rules

1. Collection-directory `README.md` files describe scope, naming, and maintenance rules.
2. Collection-directory `INDEX.md` files are generated inventories.
3. Every `docs/**/*.md` file must expose top-level `Purpose` and `Last updated` metadata.
4. Archive lifecycle truth lives in `docs/archive/ARCHIVE-MANIFEST.json`; `ARCHIVE-LEDGER.md` is generated.
5. Broken relative links and stale generated indexes are gate failures for active docs.
6. Repository-wide Markdown inventory and invalid-document review both run through `scripts/docs-audit.py --manifest ...`.
