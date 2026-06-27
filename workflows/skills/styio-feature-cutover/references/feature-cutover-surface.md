# Feature Cutover Surface

## Search Targets

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
