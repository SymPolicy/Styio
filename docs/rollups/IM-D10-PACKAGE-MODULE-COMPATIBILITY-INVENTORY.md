# IM-D10 Package, Module, And Release Compatibility Inventory

**Purpose:** Record the package, module, release, `spio`, and Styio-Platform boundary for IM-D10 while explicitly listing the external confirmations that cannot be derived from this local checkout.

**Last updated:** 2026-05-22

## Scope

IM-D10 owns the boundary between compiler-side package compatibility facts and ecosystem package lifecycle behavior:

- what `styio` accepts as a compiler request,
- what `styio` emits as compiler artifacts, diagnostics, receipts, source-build facts, and nano package facts,
- what `spio` must resolve before invoking `styio`,
- what Styio-Platform may host or operate as service infrastructure,
- how standard library packaging and trimming interact with the compiler,
- how package compatibility is represented without moving package-manager UX into this repository, and
- which decisions require confirmation from the externally maintained `spio` and Styio-Platform repositories.

IM-D10 does not define language semantics, parser authority, public diagnostics taxonomy, IDE service contracts, release gate tiers, or resource semantics. Those are owned by IM-D1 through IM-D9.

## Local Read Scope

This inventory is intentionally based on the current `styio-nightly` checkout only. The local `styio-spio` and `styio-platform` checkouts must not be treated as current truth for IM-D10 because those projects may be maintained on external devices.

Allowed local evidence from this repository:

| Surface | Current compiler-side fact |
|---------|----------------------------|
| `styio --machine-info=json` | Public compiler capability handshake and supported contract discovery |
| `styio check --syntax --json --file` | Authoritative syntax-only parser service |
| `styio --compile-plan <path>` | Versioned compiler request envelope for `build`, `check`, `run`, and `test` |
| `styio --source-build-info=json` | Official source layout, source channel, controlled source graph, and build-mode handshake |
| `--nano-create` / `--nano-publish` | Local static nano package producer, verifier, and static repository materialization |
| diagnostics / receipts / runtime events | Compiler-owned machine-readable outputs under the compile-plan output roots |

Not allowed as local evidence:

1. current `spio` package-manager implementation status,
2. current Styio-Platform registry, hosted workspace, or cloud-worker implementation status,
3. current remote registry protocol shape,
4. current package signing, trust, or authentication policy, and
5. current external release cadence or deployment state.

Those items require explicit confirmation from the owning repositories.

## Compiler-Side Boundary

`styio` owns compiler facts, not package lifecycle.

Accepted `styio` responsibilities:

1. Parse and validate compile-plan v1.
2. Reject malformed plans with stable service diagnostics.
3. Accept only compiler-supported intents: `build`, `check`, `run`, and `test`.
4. Require `generated_by.tool == "spio"` for the current compile-plan producer contract.
5. Require absolute workspace, entry, build, artifact, and diagnostics paths.
6. Accept only compiler-supported target kinds: `lib`, `bin`, and `test`.
7. Accept only compiler-supported build modes, currently `minimal`.
8. Emit compiler-owned artifacts, receipts, diagnostics, and runtime event paths.
9. Publish source-build metadata for the official source graph and controlled components.
10. Produce and verify static nano packages through local/file static repository contracts.

Rejected `styio` responsibilities:

1. Dependency solving.
2. Lockfile generation or mutation.
3. Remote package source resolution.
4. Registry search, listing, install, update, or yanking UX.
5. Package publishing workflow beyond local static nano materialization.
6. Package signing, account auth, token policy, or trust root management.
7. Hosted workspace scheduling or cloud worker lifecycle.
8. Deciding which package version a project should use.

`styio` may reject a bad plan. It should not repair, complete, or reinterpret package-manager decisions on behalf of `spio`.

## Compile-Plan Input Rule

The compile-plan should be a resolved compiler input, not an unresolved package-management request.

That means `spio` or the owning package workflow should resolve these before invoking `styio`:

1. dependency graph,
2. package versions,
3. lockfile state,
4. source materialization paths,
5. target selection,
6. feature selection,
7. standard-library package or trimming plan when applicable,
8. toolchain channel and compiler identity,
9. registry/cache/vendor source selection, and
10. output root allocation.

`styio` then verifies the plan shape, validates compiler-facing invariants, and executes the compiler workflow.

## External Confirmation Required From Spio

The following items must be confirmed in the active `spio` repository before IM-D10 can be considered externally closed.

| Confirmation ID | Question for `spio` | Why `styio` needs the answer | Expected outcome in this repo |
|-----------------|---------------------|------------------------------|-------------------------------|
| IM-D10-S1 | What is the canonical manifest schema and package identity format? | Compile-plan `entry.package_id`, package lists, source-build handoff, and diagnostics need stable names. | Document only the compiler-facing fields that `styio` consumes. |
| IM-D10-S2 | What is the lockfile schema and compatibility rule? | `styio` must know whether compile-plan package facts are already locked and reproducible. | Keep lockfile parsing out of `styio`; accept resolved facts only. |
| IM-D10-S3 | What dependency resolver rules are accepted? | Compiler requests should not encode unresolved ranges or solver intent. | Require resolved package versions in plans if package versions become compiler-visible. |
| IM-D10-S4 | What are the canonical `spio build/check/run/test` JSON success and failure payloads? | Vityo and other tools should not parse ad hoc stdout or duplicate workflow result logic. | Keep `styio` receipts and diagnostics stable; link to Spio payload docs once confirmed. |
| IM-D10-S5 | What is the project graph payload contract? | IDEs and tools need package, target, dependency, source, lock, vendor, and toolchain facts without asking the compiler to infer them. | Do not add project graph inference to `styio`; consume only explicit compiler inputs. |
| IM-D10-S6 | What is the toolchain install/use/pin lifecycle? | `styio --machine-info=json` and `--source-build-info=json` are only capability handshakes, not toolchain management UX. | Keep toolchain management in Spio; keep compiler capability discovery stable. |
| IM-D10-S7 | How does Spio consume `--nano-create`, `--nano-publish`, and static nano repository layouts? | `styio` already owns local static nano producer/verifier behavior; Spio owns lifecycle UX. | Avoid duplicating package-manager commands in `styio`; harden producer negative paths only. |
| IM-D10-S8 | How are `fetch`, `vendor`, `pack`, `publish`, `install`, `use`, `search`, and `update` split across local and remote modes? | These are package lifecycle operations and should not leak into compiler CLI scope. | Track them as out-of-scope handoff requirements unless a compiler-facing field is needed. |
| IM-D10-S9 | How does Spio represent standard library packages and build trimming? | IM-D8 says stdlib may become official package content while remaining trimmable. | `styio` records compiler-visible stdlib contracts only after Spio confirms package shape. |
| IM-D10-S10 | What compatibility matrix does Spio publish for compiler version, package format, registry format, and lockfile version? | `styio` needs to know what to expose through machine-info without owning package policy. | Add only compiler-side supported versions to machine-info or source-build-info. |

## External Confirmation Required From Styio-Platform

The following items must be confirmed with the active Styio-Platform project before IM-D10 can be considered platform-closed.

| Confirmation ID | Question for Styio-Platform | Why `styio` needs the answer | Expected outcome in this repo |
|-----------------|-----------------------------|------------------------------|-------------------------------|
| IM-D10-P1 | Does Styio-Platform own any remote registry service protocol, or is it only hosting infrastructure for Spio-owned package semantics? | The compiler must not document the wrong owner for registry service semantics. | Keep `styio` docs owner-neutral until Platform and Spio confirm the split. |
| IM-D10-P2 | Who owns channel index, latest alias, package listing, yanking, deprecation, and package metadata APIs? | Current `styio` only owns static local repository layout, not remote registry behavior. | Keep remote registry protocol out of `styio`; link to the confirmed owner later. |
| IM-D10-P3 | Who owns package authentication, signing, provenance, trust roots, and token policy? | These are security and platform concerns, not compiler execution logic. | Do not add auth/signing/trust fields to compiler contracts unless they become compiler-visible evidence. |
| IM-D10-P4 | What hosted workspace or cloud worker API invokes `styio --compile-plan`? | Compile-plan output layout and runtime-event paths may be consumed by platform workers. | Keep compile-plan output stable and add platform consumer notes only after confirmation. |
| IM-D10-P5 | Who owns object storage, cache retention, artifact retention, and package blob lifecycle? | `styio` writes local artifacts; platform may persist or distribute them. | Keep artifact storage policy out of compiler docs except for local output path contracts. |
| IM-D10-P6 | How does Platform coordinate with Spio for registry snapshots, mirrors, offline cache, and edge distribution? | This affects package reproducibility and edge deployment but is not compiler-owned. | Record confirmed compatibility facts without implementing registry policy in `styio`. |
| IM-D10-P7 | What release promotion path publishes compiler packages, standard library packages, and platform registry data? | IM-D6 owns release gates, while IM-D10 needs package compatibility evidence. | Keep compiler release evidence separate from package/registry promotion evidence. |
| IM-D10-P8 | What hosted API payloads are first-party service facts versus product-local platform facts? | IM-D9 allows first-party adapters, but shared facts must stay authoritative and reusable. | Cross-link only stable StyioServices facts from this repo. |

## Shared Spio / Platform Confirmation

Some decisions require both Spio and Styio-Platform to agree before `styio` can reference them safely.

| Confirmation ID | Shared decision | Required answer |
|-----------------|-----------------|-----------------|
| IM-D10-X1 | Registry ownership split | Which repo owns package semantics, which repo owns hosted infrastructure, and which repo owns client UX? |
| IM-D10-X2 | Package identity | Canonical tuple for name, version, channel, namespace, target, platform, and optional feature set. |
| IM-D10-X3 | Compatibility matrix | Versioned mapping across compiler contract version, package format, lockfile format, registry protocol, and platform API. |
| IM-D10-X4 | Failure taxonomy | Which failures are compiler diagnostics, Spio package-manager diagnostics, Platform service diagnostics, or transport/runtime failures? |
| IM-D10-X5 | Standard library distribution | Whether stdlib is packaged by Spio, hosted by Platform, bundled by compiler fallback, or a staged combination. |
| IM-D10-X6 | Reproducibility evidence | Which receipts, checksums, source provenance, lockfiles, and registry snapshots are required for reproducible package builds? |
| IM-D10-X7 | Offline and edge mode | How static nano packages, local mirrors, vendored packages, and hosted registry fallback interact. |
| IM-D10-X8 | Security policy | Where signing, provenance, SBOM, audit evidence, auth tokens, and trust roots are defined and enforced. |

## Stop Condition

IM-D10 can close inside `styio` only when:

1. compiler-side package contracts are documented and tested as compiler contracts;
2. compile-plan v1 remains a resolved compiler input and not a dependency-resolution request;
3. `styio` rejects malformed package-facing plans with stable service diagnostics;
4. nano producer/verifier and static repository behavior have positive and negative tests, including malformed repository entry schemas, malformed cloud package manifests, blob integrity failures, local/static publish boundaries, and create/publish CLI guard failures;
5. `styio` docs do not present local Spio or Platform checkout observations as current implementation evidence;
6. every Spio-owned package lifecycle decision is either confirmed by active Spio docs or listed as external confirmation required;
7. every Styio-Platform registry or hosted-service decision is either confirmed by active Platform docs or listed as external confirmation required; and
8. any future compiler-visible package field has a source-of-truth owner, version, negative-path diagnostic, and compatibility test.

## Decision Closure

IM-D10 is not closed by this inventory. The compiler-side boundary is clear enough to proceed with local hardening, and the current nano producer/verifier edge tests now cover malformed static repository entries, malformed cloud manifests, create/publish guard failures, blob integrity failures, and local/static publish boundaries. The package lifecycle, remote registry, hosted platform, standard-library distribution, trust, and compatibility-matrix questions still require explicit confirmation from the active `spio` and Styio-Platform maintainers.

## Source Documents

- [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md)
- [Styio / Spio Coordination Plan](../external/for-spio/Styio-Nano-Spio-Coordination.md)
- [Styio Services](../external/SERVICES.md)
- [Styio Repository Map](../specs/REPOSITORY-MAP.md)
- [Styio Ecosystem CLI Contract Matrix](../plans/Styio-Ecosystem-CLI-Contract-Matrix.md)
- [StyioConfig README](../../src/StyioServices/StyioConfig/README.md)
- `src/StyioServices/StyioConfig/CompilePlanContract.*`
- `src/StyioServices/StyioConfig/SourceBuildInfo.*`
