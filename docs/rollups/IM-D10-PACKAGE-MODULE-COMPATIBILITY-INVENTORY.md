# IM-D10 Package, Module, And Release Compatibility Inventory

**Purpose:** Record the confirmed boundary between Styio compiler compatibility,
Pafio project/package workflows, and Styio Platform registry and hosted
services.

**Last updated:** 2026-07-30

## Scope

IM-D10 owns compiler-facing package compatibility facts only. It does not make
Styio responsible for project manifests, dependency resolution, locks,
vendoring, registry hosting, hosted workspaces, or IDE project aggregation.

## Confirmed Ownership

| Fact or operation | Owner | Styio responsibility |
|-------------------|-------|----------------------|
| `pafio.toml`, `pafio.lock`, resolution, cache, vendor state | Pafio | Consume only resolved compile-plan inputs |
| Project metadata and target graph | Pafio `metadata v1` | Do not infer or republish |
| `check/build/run/test` project workflow | Pafio | Validate and execute the resolved compile plan |
| Compiler capability | Styio | Publish `styio --machine-info=json` |
| Compile-plan request | Pafio produces; Styio consumes | Reject invalid producer, version, intent, path, or profile |
| Diagnostics, receipts, runtime events, compiler artifacts | Styio | Own concrete schemas and semantics |
| Registry client, vendor, pack, publish | Pafio | No compiler-side remote registry protocol |
| Registry service/control, hosted workspace, cloud job, worker | Styio Platform | Keep hosted lifecycle out of compiler contracts |
| Local project UI | Vityo via Pafio | Provide language/compiler facts directly |
| Hosted project UI | Vityo via Platform | Provide language/compiler facts directly |

## Compile-Plan Boundary

The accepted producer identity is:

```json
{
  "generated_by": {
    "tool": "pafio"
  }
}
```

Pafio resolves package versions and target intent before invocation. Styio may
reject a malformed plan, but it does not repair dependency state, select a
package release, rewrite a lock, or consult Pafio's private storage.

The compiler owns:

1. request validation;
2. compilation and execution;
3. diagnostics and exit classification;
4. `receipt.json`;
5. `diagnostics.jsonl`;
6. `runtime-events.jsonl`;
7. compiler artifacts under plan-selected output directories.

Pafio owns the surrounding sync transaction and stable workflow status
envelope. It can report compiler process state and output paths without
redefining compiler payloads.

## Release Compatibility

Compatibility is determined by explicit machine contracts:

1. Pafio probes `styio --machine-info=json`;
2. the compiler advertises the compile-plan versions it accepts;
3. Pafio emits a supported resolved request;
4. Styio rejects unknown versions or producer identities;
5. Platform workers use a fixed Pafio and system Styio pair;
6. coordinated release evidence pins immutable revisions for all owner
   repositories.

Compiler distribution remains an external prerequisite. Pafio has no compiler
install, update, switch, pin, source-build, or cache lifecycle.

## Offline And Security Consequences

Pafio owns deterministic resolution, content-addressed cache behavior, vendor
state, registry trust, package provenance, and offline project reproduction.
Styio receives resolved local inputs and writes compiler outputs. Platform owns
remote authentication, service policy, retention, and worker isolation.

No compiler contract accepts registry credentials, cloud policy, hosted
workspace identity, or Pafio home-directory state.

## Closure Evidence

IM-D10 is compiler-side complete when:

1. the compile-plan consumer accepts Pafio as the only ecosystem project
   producer; Styio's self identity remains limited to its direct single-file
   build implementation;
2. compiler-owned output contracts remain stable under focused tests;
3. the owner matrix assigns project, compiler, and hosted facts to exactly one
   repository;
4. the fixed-revision ecosystem matrix passes Pafio, Styio, Platform, and
   Vityo interoperability.

The first three are repository-local requirements. The fourth is coordinated
release evidence and does not move external product behavior into Styio.

## References

- [Pafio handoff](../external/for-pafio/Styio-Nano-Pafio-Coordination.md)
- [ecosystem machine contract matrix](../external/for-pafio/Styio-Ecosystem-Machine-Contract-Matrix.md)
- [repository map](../specs/REPOSITORY-MAP.md)
