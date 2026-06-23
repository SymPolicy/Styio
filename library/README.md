# Styio Standard Library

**Purpose:** Define the repository-local standard-library skeleton and active module manifest while Styio's package-managed standard library path is still being staged.

**Last updated:** 2026-06-21

`library/manifest.json` is the machine-readable source for the current standard-library module envelope. It records active compiler-prelude content, planned module tiers, trimming expectations, and the evidence route required before a module becomes stable API.

Current rules:

1. Active modules must have a source path that exists in this repository.
2. Planned modules may reserve a directory and contract boundary, but they are not accepted API.
3. The gate `stdlib_manifest_gate` validates schema, module uniqueness, directory presence, and active source/test paths.
4. Deep package-management behavior remains owned by Spio / Styio-Platform handoff work.
