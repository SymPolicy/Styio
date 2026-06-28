# Styio Standard Library

**Purpose:** Define the repository-local standard-library skeleton and active module manifest while Styio's package-managed standard library path is still being staged.

**Last updated:** 2026-06-25

`library/manifest.json` is the machine-readable source for the current standard-library module envelope. It records active compiler-prelude content, planned module tiers, trimming expectations, and the evidence route required before a module becomes stable API.

Current rules:

1. Active modules must have a source path that exists in this repository.
2. Planned modules may reserve a directory and contract boundary, but they are not accepted API.
3. The gate `stdlib_manifest_gate` validates schema, module uniqueness, directory presence, and active source/test paths.
4. Deep package-management behavior remains owned by Spio / Styio-Platform handoff work.

Current active evidence:

| Module | Status | Evidence |
|--------|--------|----------|
| `std.resource` | active compatibility prelude | `share/styio/prelude/resources.styio`, `library/resource/README.md`, file/stdin/stdout feature suites, and prelude parser coverage |
| all other `std.*` directories | planned | directory README plus manifest reservation only |

Maintenance checklist:

1. Do not mark a module `active` until its source path and test evidence exist.
2. Do not promote compiler intrinsics, examples, benchmarks, or algorithm-oracle fixtures into `std.*` without a manifest and IM-D8 update.
3. Keep `library/manifest.json`, this README, the affected module README, IM-D8, and `workflows/TEST-CATALOG.md` aligned in the same checkpoint.
4. Run `python3 scripts/stdlib-manifest-gate.py --repo-root . --manifest library/manifest.json` after any manifest or module README change.
