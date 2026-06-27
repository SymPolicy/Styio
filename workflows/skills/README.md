# Workflow Skills

**Purpose:** Store repo-local skills used by root-level workflows.

**Last updated:** 2026-06-28

## Scope

1. Each child directory defines the skill in `skill.toml`.
2. UI-facing agent metadata uses TOML, for example `agents/openai.toml`.
3. Skills stay concise and point to workflow docs or references for details.
4. Root workflows define sequencing; skills define reusable execution discipline.
5. Functional-change skills must preserve cutover and commit-readiness prompts before final tests, handoff, or commit.
6. Skills must never contain developer-machine paths, server-machine paths, private endpoints, account names, or deployment roots; use placeholders such as `<workspace-root>`, `<user-home>`, `<server-host>`, or environment variables.

## Inventory

See [INDEX.md](./INDEX.md).
