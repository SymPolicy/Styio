# IM-D8 Standard Library And Domain Library Inventory

**Purpose:** Record the accepted language-core, compiler-intrinsic, standard-library, example, benchmark, and domain-library ownership decisions for IM-D8.

**Last updated:** 2026-05-21

## Scope

IM-D8 owns the library-boundary contract for Styio:

- what belongs to language-core,
- what may become a compiler intrinsic,
- what belongs to the standard library,
- how standard-library implementation is maintained now,
- how standard-library distribution moves to official packages later,
- how build-time trimming interacts with standard-library loading,
- what must be migrated to `styio-example`, `styio-benchmark`, or domain package repositories, and
- how external packages avoid becoming implicit language commitments.

IM-D8 does not define package-manager UX, registry trust, lockfiles, vendoring, or dependency resolution. Those remain IM-D10 and `styio-spio` / Styio-Platform concerns.

## Current State

Styio already has several library-like surfaces:

- language-core type-system design in [Styio Language Design](../design/Styio-Language-Design.md),
- compiler intrinsic design in [Styio Standard Library - Compiler Intrinsics Specification](../design/Styio-StdLib-Intrinsics.md),
- runtime helper surfaces in `src/StyioExtern/`, `src/StyioRuntime/`, and the ORC registration path,
- feature and algorithm tests under `tests/`, and
- benchmark/probe wrappers that point to the external `styio-benchmark` repository.

The maturity gap is not that every useful library must live in the compiler repository. The gap is to keep the three retained layers explicit and migrate all other library-like material to its owning project.

## Accepted Layer Model

Styio keeps exactly three retained layers.

| Layer | Owner | Meaning |
|-------|-------|---------|
| language-core | `styio` compiler repository | Type-system and minimal semantic facts required for the language to exist |
| compiler-intrinsic | `styio` compiler repository | Operations the compiler must recognize for typing, lowering, optimization, resource/effect reasoning, or code generation |
| standard library | temporarily in `styio`, later official packages | Small, necessary, commonly loaded library capabilities that should remain trimmable and package-manageable |

Everything else is migrated out:

| Surface | Owner |
|---------|-------|
| examples | `styio-example` |
| performance workloads, baselines, reports, and comparisons | `styio-benchmark` |
| domain libraries | independent package/domain repositories distributed through Styio-Platform / Spio |
| package lifecycle and registry UX | `styio-spio` / Styio-Platform |

External projects do not become language commitments merely because they exist in the Styio ecosystem.

## Language-Core

Accepted decision:

- Language-core is basically the type system and the minimal semantic facts required to make source code meaningful.
- Language-core stays in this compiler repository.
- Language-core includes primitive and structural type rules, type annotations, function type facts, resource/effect type facts that are part of the language, and diagnostics needed to reject invalid language forms.
- Language-core does not include domain helpers, benchmark workloads, sample programs, or convenience APIs that can be expressed as ordinary library code.

If a feature can be removed without changing the meaning of the language's type system or core semantic rules, it is not language-core.

## Compiler Intrinsics

Accepted decision:

Compiler intrinsics are allowed only when ordinary library implementation is not enough.

A capability may become a compiler intrinsic only when at least one of these is required:

1. parser or syntax recognition,
2. type inference that cannot be expressed through ordinary function signatures,
3. direct StyioIR lowering,
4. codegen specialization,
5. optimizer visibility,
6. resource/effect reasoning,
7. frame/snapshot/commit participation, or
8. a stable performance contract that depends on compiler-owned representation.

Ordinary convenience functions, domain formulas, examples, and benchmark cases must not be promoted to compiler intrinsics just because they are useful.

Intrinsic acceptance requires:

1. design specification,
2. type and absence/resource/effect rules,
3. positive tests,
4. negative diagnostics,
5. lowering or runtime evidence, and
6. performance evidence when the intrinsic is justified by performance.

## Standard Library

Accepted decision:

- The standard library is small, necessary, and commonly loaded.
- For now, standard-library contracts and any current implementation may remain in this repository.
- The repository-local standard-library envelope is `library/manifest.json`; it records active source-backed modules and planned module directories.
- Long term, the standard library should become official package content maintained separately from the compiler implementation.
- The standard library must be usable through Spio / Styio-Platform management when that package path is ready.
- Builds must support trimming/dead-code elimination so standard-library availability does not force thick artifacts.
- Standard-library APIs need named contracts, schema markers, tests, diagnostics, and manifest-gate coverage before being treated as stable.

This preserves the "thick library, thin artifact" model: development can load a useful standard library, while production builds can remove unused code. The current `std.resource` manifest entry points at `src/StyioPrelude/resources.styio` as compatibility source until it can move to a package-aware library location. Runtime installs include `share/styio/library/manifest.json`, module README files, and `share/styio/src/StyioPrelude/resources.styio` so the installed manifest can be validated even when repository test evidence is not installed with the runtime component.

## External Examples

Accepted decision:

- Examples belong in `styio-example`.
- Repository-local examples may exist only when they are needed for compiler tests, docs smoke, or migration coverage.
- Example code does not define language or standard-library behavior.
- If an example demonstrates a behavior that should become accepted, the behavior must be promoted through the language-core, compiler-intrinsic, or standard-library process with tests and diagnostics.

`styio-example` is the external project for maintained examples.

## Benchmark Workloads

Accepted decision:

- Deep benchmark workloads, runners, baselines, reports, and performance comparisons belong in `styio-benchmark`.
- This repository may keep probes, compatibility wrappers, command references needed to integrate with `styio-benchmark`, and a tiny deterministic `benchmark/core/` corpus for release-conformance timing-schema evidence.
- A benchmark workload does not become a standard-library API by existing as a workload.
- If benchmark work identifies a useful library capability, it must still pass the standard-library or compiler-intrinsic acceptance process before becoming an accepted API.

`styio-benchmark` remains the SSOT for deep performance workloads, comparisons, baselines, and reports. `benchmark/core/` is not a competing benchmark suite; it exists so the compiler checkout always has a reproducible smoke corpus.

## Domain Libraries

Accepted decision:

- Domain libraries do not merge into the language.
- Finance, IoT, exchange, model, analytics, and other domain-specific libraries are independent packages or projects.
- Domain packages are distributed through Styio-Platform / Spio when that package path is ready.
- Domain libraries may depend on language-core, compiler intrinsics, and standard-library APIs, but they do not define those APIs.
- A domain library may propose promotion of a general capability, but promotion requires a separate language-core, compiler-intrinsic, or standard-library decision.

This keeps the compiler repository from becoming a domain application repository.

## Promotion And Migration Rules

Accepted decision:

| From | To | Required evidence |
|------|----|-------------------|
| external example | standard library | spec, stable API, positive/negative tests, schema-marker note |
| benchmark workload | compiler intrinsic | proof that compiler-owned representation is required, lowering/codegen evidence, perf evidence |
| benchmark workload | standard library | stable non-benchmark API, tests, docs, no benchmark-only assumptions |
| domain library | standard library | general-purpose API, ecosystem justification, named contract |
| standard library | compiler intrinsic | one of the intrinsic admission reasons plus compiler tests |
| compiler intrinsic | language-core | type-system or core semantic dependency |

Migration in the other direction is also allowed. If a capability no longer needs compiler ownership, it should move from intrinsic to standard library or package form.

## Standard Library Acceptance Checklist

Every standard-library capability needs:

1. purpose and API contract,
2. type signature,
3. resource/effect behavior when applicable,
4. absence/default behavior when applicable,
5. versioning status,
6. positive tests,
7. negative diagnostics,
8. build trimming behavior,
9. package/distribution note when moved out of this repository, and
10. benchmark evidence when a capability record includes performance statements.

## Stop Condition

IM-D8 can close only when:

1. the three retained layers are documented as language-core, compiler-intrinsic, and standard library;
2. examples, benchmarks, and domain libraries are documented as external surfaces;
3. standard-library contracts in this repository are identified as current temporary ownership or future official package ownership;
4. compiler-intrinsic admission rules are recorded and used by future intrinsic additions;
5. benchmark and example repositories are not treated as language API SSOTs;
6. domain-library distribution is delegated to Styio-Platform / Spio instead of this compiler repo;
7. every accepted library capability is in a test catalog, standard-library contract, intrinsic spec, or external handoff record; and
8. deferred library capabilities are explicitly marked deferred rather than implied by examples or benchmark workloads.

## Decision Closure

No IM-D8 design decision remains open in this inventory. Remaining work is implementation and migration: classify existing library-like content into language-core, compiler-intrinsic, standard-library, example, benchmark, or domain package ownership; then move non-retained surfaces to their owning repositories when the package/project path is ready.

## Source Documents

- [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md)
- [Styio Language Design](../design/Styio-Language-Design.md)
- [Styio Standard Library - Compiler Intrinsics Specification](../design/Styio-StdLib-Intrinsics.md)
- [Styio Performance Testing Route](../design/performance-testing.md)
- [Docs / Ecosystem Runbook](../teams/DOCS-ECOSYSTEM-RUNBOOK.md)
- `styio-benchmark`: <https://github.com/eBioRing/styio-benchmark>
- `styio-example`: <https://github.com/eBioRing/styio-example>
