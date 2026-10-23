# Feature Cutover Surface

## Search Targets

Limit the inventory to behavior explicitly superseded by the authorized change. Search matches are evidence to classify, not permission to delete. Preserve independently maintained assets required by repository rules, including Vityo's `prototype/`.

1. Old function, class, file, test, fixture, CLI flag, parser route, CMake target, workflow, and doc names.
2. Markers such as `legacy`, `compat`, `compatibility`, `fallback`, `deprecated`, `old`, `shim`, `alias`, `bridge`, `TODO`, and `remove later`.
3. Acceptance fixtures or goldens that still exercise the old behavior.
4. Docs, runbooks, catalogs, generated indexes, and external handoff pages that still describe the old path as active.

## Required Migration Surface

1. Production call sites.
2. Tests and CTest registration.
3. Docs, runbooks, workflow TOML/Markdown, and generated indexes.
4. CI or delivery gates that mention the old path.
5. Example programs and public README snippets.

## Completion Standard

The change is ready for final tests only when the new path is canonical, old implementation paths are deleted or explicitly rejected, and retained compatibility is documented as a separate owned decision rather than hidden inside the feature work.

Any retained compatibility must already be approved and outside the superseded implementation. Use a one-time search or script to prove removal; do not add permanent residue tests. Complete source review and focused repairs before the selected final regression, and reuse unchanged evidence across the cutover and commit-readiness workflows.
