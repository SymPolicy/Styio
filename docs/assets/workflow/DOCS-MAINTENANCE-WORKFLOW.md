# Docs Maintenance Workflow

**Purpose:** Define the repeatable workflow for maintaining `docs/` metadata, generated indexes, and structural validation.

**Last updated:** 2026-04-08

## Goals

1. Keep directory boundaries stable.
2. Keep collection indexes current without hand-editing every inventory list.
3. Fail fast when links, metadata, or naming rules drift.

## Commands

```bash
python3 scripts/docs-index.py --write
python3 scripts/docs-audit.py
ctest --test-dir build -L docs --output-on-failure
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

## Workflow

1. Edit docs or move files.
2. Regenerate directory inventories with `python3 scripts/docs-index.py --write`.
3. Run `python3 scripts/docs-audit.py` locally.
4. If the repo is already configured, run `ctest --test-dir build -L docs --output-on-failure`.
5. For checkpoint-grade verification, run `./scripts/checkpoint-health.sh --no-asan --no-fuzz`.

## Rules

1. Collection-directory `README.md` files describe scope, naming, and maintenance rules.
2. Collection-directory `INDEX.md` files are generated inventories.
3. Every `docs/**/*.md` file must expose top-level `Purpose` and `Last updated` metadata.
4. Broken relative links and stale generated indexes are gate failures.
