# Audit

**Purpose:** Define the repository-local audit queue for security, correctness, and design defects discovered before they are converted into normal tracked work; closed audit reports are not retained as long-form current-tree documents.

**Last updated:** 2026-05-09

## Defect Queue

Active audit records live under ignored `docs/audit/defects/` so large exploratory audits can be written locally without committing open defect notes.

Generated inventory: [INDEX.md](./INDEX.md).

External `styio-audit` runs outside this repository and enforces this rule when an audit is performed:

1. Missing or empty `docs/audit/defects/` passes.
2. Every markdown record in `docs/audit/defects/` must declare `**Status:** Closed`, `Resolved`, or `Cleared`.
3. Closed records must include non-empty `**Closure evidence:** ...`.
4. Any open, malformed, or non-markdown record blocks the external audit result.

Move durable findings into tracked planning, issue, test, implementation work, or a compressed rollup once the record is ready for long-term ownership. Use Git history for exact old audit prose.
