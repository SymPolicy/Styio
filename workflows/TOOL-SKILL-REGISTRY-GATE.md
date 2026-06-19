# Tool And Skill Registry Gate

**Purpose:** Define the gate that keeps repo-local skills and maintenance tools current, registered, release-wired, and mapped to owned modules.

**Last updated:** 2026-06-19

## Goal

The project keeps only current, useful repo-local tools and workflow skills. Historical compatibility shims, silent skip proxies, and unreachable scaffolding must not remain as active tools. Every release candidate must prove the retained inventory was reviewed against the current project state and that each team-owned module has at least one usable maintenance tool.

## Command

```bash
python3 scripts/tool-skill-registry-gate.py
```

`./scripts/delivery-gate.sh --mode release` runs this gate through the workflow scheduler before external audit and checkpoint health.

## What It Checks

1. Every top-level script under `scripts/` and active adapter under `benchmark/` is registered in [TOOL-SKILL-REGISTRY-GATE.toml](./TOOL-SKILL-REGISTRY-GATE.toml).
2. Deleted compatibility shims such as old `scripts/perf-route.sh`, `scripts/soak-minimize.sh`, and the retired `extend_tests.py` cannot reappear.
3. Active tool files do not carry migration markers, compatibility-wrapper language, one-release-cycle retention notes, or silent-success proxy behavior.
4. Repo-local skills under `workflows/skills/` match the registry, have current `last_updated` values, and keep references resolvable.
5. The workflow scheduler includes this gate in local, staged, push, syntax, CI, and release-oriented profiles.
6. Every team module listed in the TOML has at least one registered non-library maintenance tool and a concrete command.

## Update Rules

1. Add a tool only when it is still needed by development, CI, release, operations, or team maintenance.
2. Prefer canonical entrypoints. Do not add a wrapper that only forwards to another current script.
3. If a tool is retired, delete it and remove it from the registry in the same change.
4. If a workflow skill remains active, bump its `last_updated` date when this registry review changes the accepted project state.
5. When a new team module or business line is added, add a `module_coverage` entry before release.

## Release Policy

Release mode must fail if the inventory is stale. A release record should include the output of:

```bash
python3 scripts/tool-skill-registry-gate.py
./scripts/delivery-gate.sh --mode release
```
