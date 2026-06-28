# IM-D9 IDE / LSP Service Contract Inventory

**Purpose:** Record the IDE, LSP, first-party adapter, and service-fact boundary decisions for IM-D9 after reviewing the current Vityo architecture and implementation shape.

**Last updated:** 2026-06-28

## Scope

IM-D9 owns the language-service maturity contract for Styio:

- compiler-owned syntax and semantic facts,
- editor interaction snapshots,
- in-process `StyioIDE` APIs,
- public `styio_lspd` LSP behavior,
- first-party Vityo and Spio convenience adapters,
- semantic caches and workspace indexes,
- capability negotiation and degraded-mode reporting, and
- host-facing service payloads used by IDEs, editors, CI, and package tooling.

IM-D9 does not redefine accepted Styio grammar, diagnostic taxonomy, resource semantics, package-manager ownership, or release gating. Those are owned by IM-D2, IM-D3, IM-D4, IM-D10, and IM-D6.

## Vityo Read-Through Findings

The current Vityo architecture is not a thin generic LSP client. It is a first-party product with its own editor engine and product-owned adapter layer.

Important observed constraints:

1. Vityo's product contract accepts `CLI Adapter`, `FFI Adapter`, and `Cloud Adapter` modes. The UI depends on product contracts, not on Styio internal class names.
2. The editor consumes more than basic LSP facts: tokens, semantic spans, diagnostics, quick fixes, formatting edits, completion items, hover, semantic blocks, document symbols, reference spans, definitions, rename plans, and refactor plans.
3. Vityo already has capability routing and tier readiness for syntax, semantic, authoring, navigation, and refactor capabilities.
4. Vityo's CLI JSONL bridge already decodes service facts for diagnostics, completion, hover, semantic spans, formatting, semantic blocks, inlay hints, symbols, references, definitions, code actions, rename, safe delete, inline variable, introduce variable, extract function, change signature, parameter info, surround templates, protocol version, parser engine, grammar version, toolchain id, config path, working directory, and capability states.
5. Vityo has local fallback features for syntax interaction, symbol indexing, navigation, rename, and limited refactoring. These support product UX but are explicitly not compiler semantic authority.
6. Vityo can disable local fallback for strict service behavior, so StyioServices must make capability gaps explicit instead of relying on silent local recovery.

The conclusion is that IM-D9 must not be written as "add a few missing LSP methods." Public LSP is only one transport. The real service contract is a shared StyioServices fact layer that public LSP, in-process IDE APIs, and first-party Vityo / Spio adapters can consume at different depths.

## Accepted Authority Boundary

| Layer | Authority | Accepted role |
|-------|-----------|---------------|
| Compiler parser and sema | Authoritative | Own accepted grammar, syntax validity, compiler diagnostics, type facts, semantic facts, and cross-file language truth |
| `StyioCLI` syntax check | Authoritative syntax service | Expose `styio check --syntax --json --file` for syntax validity without type checking, lowering, codegen, execution, or runtime resource access |
| `StyioIDE` semantic bridge | Authoritative when backed by compiler facts | Publish IDE-ready semantic summaries from strict compiler parse/type facts without recovery semantic publication from malformed source |
| Editor syntax snapshots | Non-authoritative interaction data | Support highlighting, folding, matching tokens, local diagnostics, and editor responsiveness without defining accepted grammar |
| Semantic cache and workspace indexes | Derived service facts | Cache and query compiler/editor facts with freshness, revision, and source metadata |
| Public LSP | Editor-neutral transport | Advertise only methods backed by implemented `StyioIDE` behavior, docs, and tests |
| Vityo / Spio first-party adapters | Deep convenience consumers | May consume richer CLI, C++ API, FFI, or cloud payloads when they reuse the same StyioServices facts and mark derived/fallback state |

First-party integration is allowed to be deeper than public LSP. It is not allowed to become a separate grammar, diagnostic, or semantic authority.

## Service Fact Envelope

Every host-facing service payload that crosses a process or repository boundary should be able to carry the following identity fields when applicable:

| Field | Use |
|-------|-----|
| `documentId` | Stable editor or tool document identity |
| `revision` | Snapshot freshness guard |
| `protocolVersion` | Payload contract version |
| `toolchainId` | Concrete Styio toolchain identity |
| `parserEngine` | Parser engine evidence, normally `nightly` for authoritative grammar |
| `grammarVersion` | Grammar contract evidence |
| `configPath` | Active compiler or project configuration path |
| `workingDirectory` | Workspace or project root used for resolution |
| `capabilityStates` | Per-capability readiness such as available, derived, empty, unsupported, unavailable, failed, protocol error, or stale |
| `capabilityMessages` | Optional human-readable reason for missing or degraded capability |

Capability state is part of the contract. A host must be able to distinguish a fresh compiler-backed result from a derived fallback, an intentionally unsupported feature, an unavailable service, a failed service, a protocol error, or stale data.

## Capability Tiers

| Tier | Required facts | Optional facts | Rule |
|------|----------------|----------------|------|
| syntax | syntax validity, diagnostics | token spans, editor syntax diagnostics | Syntax validity must come from compiler parser services. Editor token snapshots are interaction data only. |
| semantic | analysis, document symbols, references, definition | semantic tokens | Compiler-backed facts are authoritative. Derived local facts must be marked as derived or fallback. |
| authoring | completion, hover, code actions | inlay hints, parameter info, formatting | Interactive facts may be degraded, but their source and capability state must be visible to the host. |
| navigation | definition, references, document symbols | semantic blocks | Navigation must use a common symbol/reference model. String-only jumps are not an accepted service contract. |
| refactor | rename plan | safe delete, inline variable, introduce variable, extract function, change signature, surround | Refactors must return previewable edit plans, conflict data, and capability state. Public LSP exposes them only after service tests exist. |
| workspace | workspace root, config path, open-file state, background index state | persistent cache state | Multi-workspace behavior must be explicit; tools must not infer cross-workspace truth from process-global state. |

## Implemented Workspace Cache Edges

| Edge | Contract | Evidence |
|------|----------|----------|
| Stable project cache roots | `Project::set_root(...)` derives cache directories from the normalized root path as `root-<hex-path>`, so persistent IDE state no longer depends on process-local `std::hash<std::string>` values. | `StyioIdeProject.EnvironmentFallbacksAndWorkspaceSkipsStayExplicit`; `StyioLspServer.WorkspaceSymbolMapsPersistentParameterAndBuiltinKinds` now seeds persistent indexes through `Project::cache_root()`. |
| Workspace scan failure handling | `Project::scan_workspace()` uses `std::filesystem` error-code traversal with `skip_permission_denied`, records `workspace_scan_error_count()`, and keeps missing or unreadable roots fail-closed with an empty file set instead of throwing. | `StyioIdeProject.EnvironmentFallbacksAndWorkspaceSkipsStayExplicit` covers missing-root fail-closed behavior, stable cache-root shape, generated-directory skips, and zero-error happy-path scanning. |

## First-Party Adapter Rule

Vityo and Spio are first-party projects, so they may have convenience adapters that are more direct than the generic external surface.

Allowed:

1. Vityo may bind directly to in-process C++ service APIs, a future FFI facade, CLI JSONL facts, or cloud service facts.
2. Spio may consume compile-plan, machine-info, source-build, package-adjacent, and future service discovery payloads directly.
3. First-party adapters may request richer envelopes than public LSP if those envelopes are documented as StyioServices facts.
4. Product-local fallback may keep the UI responsive when service facts are missing.

Not allowed:

1. Vityo or Spio must not maintain an accepted grammar separate from the compiler parser.
2. Vityo or Spio must not mint compiler diagnostic codes outside the shared diagnostic taxonomy.
3. Product-local fallback must not report compiler semantic errors that did not come from compiler-owned facts.
4. Public LSP capabilities must not be advertised merely because a first-party adapter can synthesize a local fallback.
5. Missing service capabilities must not be hidden behind silent local behavior; they must be reported as derived, unavailable, unsupported, failed, protocol error, stale, or another documented state.

## Public LSP Rule

`styio_lspd` remains the editor-neutral public protocol surface. It should stay narrower than first-party adapters whenever service behavior is not ready for generic external editors.

Public LSP may advertise a capability only when:

1. `StyioIDE` owns the backing behavior,
2. request and response payloads are documented,
3. unit or integration tests cover the method,
4. diagnostics preserve shared `Diagnostic.code` and `data.phase` identity when applicable,
5. stale or degraded service states are handled without publishing misleading facts, and
6. the feature does not depend on Vityo-only UI assumptions.

## Implementation Backlog

IM-D9 design decisions are fixed by this inventory. Remaining work is implementation and contract hardening:

1. Extend StyioServices host-facing envelopes so first-party adapters can consume the same service facts Vityo already models. Current LSP envelope hardening records initialize-time workspace selection, avoids responding to notification-style initialize messages without an id, and accepts ASCII case variants of inbound `Content-Length` while keeping canonical outbound frames.
2. Document capability states and payload shapes in `src/StyioServices/MANIFEST.md` and module READMEs when each service becomes available.
3. Keep conservative `textDocument/rename`, call-argument `textDocument/inlayHint`, and narrowly edit-producing `textDocument/codeAction` behavior covered by LSP tests before broadening their edit/action surface, and add tests before expanding public `styio_lspd` capability advertisements for broader refactor methods.
4. Keep `styio check --syntax --json --file` as the syntax-validity path for IDEs and validation pipelines.
5. Preserve strict fallback-disable behavior for hosts that want compiler/service facts only.
6. Make multi-workspace state explicit through document id, revision, root/config identity, toolchain identity, stable cache-root identity, cache freshness, capability state, and initialize-time workspace selection.

## Stop Condition

IM-D9 can close only when:

1. `src/StyioServices/MANIFEST.md` lists every public CLI, IDE, LSP, and adapter-facing service capability that is actually available;
2. every StyioServices module README describes usage, available capabilities, authority status, and public limitations;
3. public LSP capabilities match documented `styio_lspd` behavior and tests;
4. first-party Vityo / Spio adapters consume shared StyioServices facts instead of separate grammar, diagnostic, or semantic authorities;
5. local or product fallback is marked as derived/fallback and can be disabled by strict hosts;
6. syntax-validity consumers are routed to compiler parser services, not editor snapshots;
7. semantic, navigation, and refactor payloads expose source, freshness, and capability state; and
8. multi-workspace and configuration scoping are explicit in service payloads and tests.

## Decision Closure

No IM-D9 design decision remains open in this inventory. The remaining work is implementation: fill the service envelopes, capability tests, public LSP method coverage, first-party adapter payload docs, and any true multi-workspace service-state behavior beyond the explicit single-root initialization path without moving authority out of the compiler-owned StyioServices fact layer.

## Source Documents

- [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md)
- [Styio Services](../external/SERVICES.md)
- [Styio LSP Usage](../external/for-ide/LSP.md)
- [IDE / LSP Runbook](../teams/IDE-LSP-RUNBOOK.md)
- [Docs / Ecosystem Runbook](../teams/DOCS-ECOSYSTEM-RUNBOOK.md)
- `src/StyioServices/MANIFEST.md`
- First-party Vityo architecture and implementation read-through: `LanguageServiceAdapter`, product-owned adapter layer, `StyioLanguageService`, capability routing, CLI JSONL protocol, local fallback syntax validation, and strict fallback-disable tests.
