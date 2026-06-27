# File Onboarding Surface

**Purpose:** Map new file classes to the repo surfaces that usually need updates.

**Last updated:** 2026-06-28

## Before Adding A File

1. Search generated indexes, `docs/adr/`, owning SSOTs, runbooks, workflow docs, plans, rollups, and source/test registries for an existing owner.
2. Prefer updating the existing owner. A new file is justified only when it has one responsibility that no existing file should own.
3. For docs, pass `--reuse-reviewed` to `scripts/docs-scaffold.py` only after the search is complete.
4. For ADRs, update the existing active ADR or owning SSOT with the current decision. Do not add old/new decision-history sections.
5. Do not use version-style names such as `v2`, `version`, `new`, `old`, `legacy`, or `latest`; name the file by the feature or transformation result.
6. Do not write developer-machine paths, server-machine paths, private endpoints, account names, hostnames, or deployment roots; use placeholders such as `<workspace-root>`, `<user-home>`, `<server-host>`, or an environment variable.
7. Avoid one-feature-many-docs drift. Split documents only by artifact class: design SSOT, test catalog, team runbook, workflow, external handoff, or ADR under review.

| Class | Usual Surfaces |
|-------|----------------|
| doc | `README.md`, `INDEX.md`, `scripts/docs_config.py` for new collections |
| workflow | `workflows/*.toml`, `workflows/workflows.toml`, workflow docs |
| skill | `workflows/skills/*/skill.toml`, `workflows/workflows.toml` |
| script | shell/python syntax check, docs gate references |
| source | target build, focused unit/security test |
| test | `tests/CMakeLists.txt` or matching test registry |
| config | config docs, parser/consumer smoke |
| fixture | repo hygiene allowlist or negate rule if needed |
