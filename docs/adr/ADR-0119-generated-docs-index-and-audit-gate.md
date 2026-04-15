# ADR-0119: Generated Docs Index and Audit Gate

**Purpose:** Record the decision, context, alternatives, and consequences for the generated docs-index and docs-audit workflow.

**Last updated:** 2026-04-08

## Status

Accepted

## Context

The `docs/` tree had already been reorganized by responsibility, but two problems remained:

1. collection-directory `README.md` files still mixed scope rules with hand-maintained inventories;
2. metadata and links were inconsistent enough that a structural gate could not be enforced without first standardizing the docs tree.

A manual inventory in every `README.md` would drift again. A heavy Markdown-lint stack would cost more than it would save for this repository.

## Decision

1. Standardize top-level docs metadata on `Purpose:` and `Last updated:`.
2. Split collection-directory entry files into:
   - `README.md` for scope, naming, and maintenance rules;
   - `INDEX.md` for the generated inventory.
3. Generate `INDEX.md` files with `python3 scripts/docs-index.py --write`.
4. Enforce structure, metadata, naming, links, and generated-index freshness with `python3 scripts/docs-audit.py`.
5. Register the docs audit as a CTest gate and run it from CI and `scripts/checkpoint-health.sh`.

## Alternatives

1. Keep `README.md` as both scope document and inventory.
   - Rejected because inventories drift faster than scope rules.
2. Add a full external Markdown lint/toolchain.
   - Rejected because the repo only needs structural enforcement, not a broad style-lint stack.
3. Fully generate `README.md` as well.
   - Rejected because scope and naming rules are editorial, not generated data.

## Consequences

Positive:

1. Directory inventories stop being hand-maintained.
2. Broken links and stale indexes become gate failures.
3. The docs tree now has a clear ownership split between manual scope docs and generated inventory docs.

Negative:

1. Contributors must remember to re-run the generator after docs-tree changes.
2. The repo now owns two maintenance scripts that must stay compatible with the docs layout.
