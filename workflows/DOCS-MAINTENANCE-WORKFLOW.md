# Docs Maintenance Workflow

**Purpose:** Maintain current documentation, generated indexes, archive lifecycle state, and review-page projections of verified repository facts.

**Last updated:** 2026-10-01

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
python3 tests/design_intent_contract_test.py
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

## Change Routing

| Change | Owning document and maintainer | Dependent updates | Focused verification |
|---|---|---|---|
| Feature syntax or type behavior | `docs/design/syntax/features/<feature-id>.md`, including its `toml syntax-feature`; language feature owner | EBNF/token/cross-feature sections by their role; generated feature graph and indexes | `syntax-feature-state-gate.py --write`, then the state gate and the feature's recorded positive/negative cases |
| Language model or visual design requirements | `docs/design/Styio-Language-Design.md` sections 2.4–2.5; language design owner | Concise README descriptions; research evaluation criteria | `docs-audit.py`, `tests/design_intent_contract_test.py`, evidence review |
| Observable fields, IDs, capability or degradation behavior | `src/StyioServices/StyioObservable/README.md`; Sema / IR public-contract owner | Producer/consumer fixtures; CLI for admission/receipts; Runtime for events | Snapshot/delta/query/service/consumer tests plus affected runtime selection |
| Component maintenance procedure | The matching `docs/teams/*-RUNBOOK.md`; component owner | Coordination review routing and this workflow when sequencing changes | `team-docs-gate.py`, generated indexes, refreshed team statistics |
| Delivery gap or next checkpoint | `docs/rollups/NEXT-STAGE-GAP-LEDGER.md` and the existing owning plan; coordination owner | Current-state entry links | Contract, owner, and reproducible closure evidence |
| User tutorial or programming skill | The separate tutorial/skill repository; user-documentation owner | Version-pinned feature and executable-source links | Run examples and expected diagnostics using the declared compiler revision |

The [documentation policy](../docs/specs/DOCUMENTATION-POLICY.md) defines
source authority. Runbooks record how to work on a component; they link to
semantic and wire contracts instead of copying their fields. The existing
`workflows/skills/` inventory serves compiler maintenance; user programming
skills have a separate audience and distribution boundary.

### Observable Contract Example

1. Update the decoder contract with field meaning, capability scope, incomplete
   states, and compatibility behavior. Sema / IR checks the fact's producer.
2. Update the existing producer/consumer fixtures. Include CLI / Nano when
   request admission, artifact paths, or receipts change, and Codegen / Runtime
   when event semantics or observation descriptors change.
3. Update affected runbook commands and handoff details. Keep decoder fields in
   their owning contract.
4. Update the affected capability's scope, evidence, and gap links in Language
   Design. Its `affected_capabilities` references route maintenance; they do not
   prove a general language or research claim.
5. Run focused tests, documentation gates, and applicable architecture checks.
   Record compiler revision, commands, and passed/failed/not-run results.
   External consumers report their own acceptance separately.

## Rules

For maintainer-facing HTML reviews, use
[Styio Research Review Page](./skills/styio-research-review-page/SKILL.md).
It supplies a portable generator, synthetic input fixture, and committed-fact
extractor. Keep proposals and current source facts distinct, retain failed or
unexecuted checks, and link each repository's exact Draft PR candidate. A daily
refresh or repository hook reuses the same explicit entrypoints; successful
generation does not imply that a schedule, publication, or merge occurred.

1. Collection-directory `README.md` files describe scope, naming, and maintenance rules.
2. Collection-directory `INDEX.md` files are generated inventories.
3. Every `docs/**/*.md` file must expose top-level `Purpose` and `Last updated` metadata.
4. Archive lifecycle truth lives in `docs/archive/ARCHIVE-MANIFEST.json`; `ARCHIVE-LEDGER.md` is generated.
5. Broken relative links and stale generated indexes are gate failures for active docs.
6. Repository-wide Markdown inventory and invalid-document review both run through `scripts/docs-audit.py --manifest ...`.
7. `docs-scaffold.py` requires `--reuse-reviewed`; do not create a document before checking whether an existing owner should be maintained instead.
8. Active ADR files must record the current decision only. Do not add old/new decision-history sections to active ADRs.
9. Documentation and workflow artifacts must not use version-style names such as `v2`, `version`, `new`, `old`, `legacy`, or `latest`; use the feature or transformation result.
10. Keep the single machine-readable design-intent block with its human rationale in Language Design; update exact authority/evidence links instead of copying syntax or wire schemas. Run `tests/design_intent_contract_test.py` when its schema or owner mappings change.
11. Documentation and skills must not expose developer-machine or server-machine details; use placeholders and run `python3 scripts/local-info-leak-gate.py --mode worktree`.
