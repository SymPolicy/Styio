# std.resource

**Purpose:** Track the active compatibility standard-resource prelude while it migrates from `src/StyioPrelude/resources.styio` into a package-aware library location.

**Last updated:** 2026-06-25

Status: active compatibility prelude. The manifest entry records existing source and feature-test evidence; it does not broaden resource semantics.

Evidence:

1. Source: `src/StyioPrelude/resources.styio`
2. Manifest entry: `library/manifest.json`
3. Gate: `stdlib_manifest_gate`
4. Test routes: file-resource, stdin, stdout/stderr feature suites plus parser coverage for the prelude source file

Non-goal: this module does not publish `std.io`, package import behavior, or new resource-family semantics.
