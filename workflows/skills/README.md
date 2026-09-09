# Workflow Skills

**Purpose:** Store repo-local skills used by root-level workflows.

**Last updated:** 2026-09-08

## Scope

1. Each child directory defines the skill in `skill.toml`.
2. UI-facing agent metadata uses TOML, for example `agents/openai.toml`.
3. Skills stay concise and point to workflow docs or references for details.
4. Root workflows define sequencing; skills define reusable execution discipline.
5. Functional-change skills use the cutover and commit-readiness self-checks before final tests, handoff, or commit. These are evidence checks performed by the agent; separate approval requirements remain effective. Reuse evidence across skills and run the required complete regression only after all source review, repairs, and focused checks finish.
6. Skills must never contain developer-machine paths, server-machine paths, private endpoints, account names, or deployment roots; use placeholders such as `<workspace-root>`, `<user-home>`, `<server-host>`, or environment variables.
7. A standard `SKILL.md` entrypoint may accompany the required `skill.toml`; keep their name and description aligned and assign detailed execution instructions to one owner. [Styio Performance Research](./styio-performance-research/SKILL.md) uses this form for direct agent dispatch, with optional topic guidance linked from the entrypoint. Workflow and UI metadata files remain TOML.

## Inventory

See [INDEX.md](./INDEX.md).
