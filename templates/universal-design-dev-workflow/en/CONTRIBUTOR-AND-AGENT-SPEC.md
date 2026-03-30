# Contributor and automation specification

**Purpose:** Rules for humans and agents: layout, workflow, tests, documentation, prohibitions. Domain: `DOMAIN-OR-PRODUCT-SPEC.md`. Topology: `DOCUMENTATION-POLICY.md`. Globals: [`./global/`](./global/).

**Version:** (set when forking)  
**Authority:** Effective once adopted.

## 1. Repository overview

Fill in: languages, build system, format/lint, test runner, package manager.

## 2. Layout

`docs/` holds `global/`, policy files, `history/`, `milestones/`, `tests/`; plus source, tests, CI config under the repo root.

## 3. Change workflow

Read domain spec and open questions; check milestone index; implement with registered tests; run formatters and tests; append `history/` for non-trivial work; update SSOT when contracts change.

## 4. Coding standards

(Project fills: format command, naming, review expectations.)

## 5. Testing

Extend tests for behavior changes unless an ADR or open question defers; do not break checks without rationale; update `tests/TEST-CATALOG.md`.

## 6. Documentation

User-visible behavior → domain spec; follow SSOT; one canonical example if used.

## 7. Prohibited actions

Examples: unapproved dependencies; editing vendor/generated code against policy; secrets or policy-violating binaries; phantom tests; changing acceptance without updating milestone docs.

## 8. Decision authority

(Project fills: self-serve vs human approval, conflict resolution.)

## Appendix — New contributor checklist

Read this spec and `DOCUMENTATION-POLICY.md`; read domain spec for your area; run build/tests; ship changes with tests and catalog rows; log significant work in `history/`; link SSOT sections in review.
