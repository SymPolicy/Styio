# Styio Ecosystem Machine Contract Matrix

**Purpose:** Freeze the current owner and consumer boundary among Styio, Pafio, Styio Platform, and Vityo without duplicating another product's schemas.

**Last updated:** 2026-07-30

**Plan status:** Active only until the coordinated fixed-revision acceptance
matrix passes. Product ownership and public command spelling are already frozen.

## 前置条件

1. 并行: parallel work may inspect one owner repository or one contract family at a
   time; changes to a public command, payload owner, or version remain a serial
   ecosystem decision.
2. 子智能体: sub-agents may gather read-only evidence when explicitly requested, but one
   coordinator must merge the owner matrix and fixed revisions.
3. 基座: shared workflow or documentation-gate substrate changes land through
   [Styio-Common-Foundation-Plan.md](./Styio-Common-Foundation-Plan.md) first.

## 1. Ownership Matrix

| Capability | Owner | Public entry | Consumers |
|------------|-------|--------------|-----------|
| Project manifest, lock, resolution, dependency sync, vendor, pack, publish | Pafio | `pafio` project commands | developers, Platform workers |
| Project description | Pafio | `pafio metadata --json` (`metadata v1`) | Vityo |
| Project workflow intent and status | Pafio | `pafio --json check/build/run/test` | Vityo, Platform |
| Compiler capability discovery | Styio | `styio --machine-info=json` | Pafio, Vityo |
| Compilation request | Pafio produces; Styio consumes | `styio --compile-plan <path>` | Pafio, Platform worker |
| Diagnostics, receipts, runtime events | Styio | compiler-owned files and streams | Pafio surfaces status; Vityo consumes semantics |
| Registry control, hosted workspace, cloud jobs, worker lifecycle | Styio Platform | Platform APIs | Pafio publish client, Vityo hosted adapter |

## 2. Styio Compiler Contracts

### 2.1 `styio --machine-info=json`

The response identifies Styio and advertises supported compile-plan,
diagnostic, receipt, and runtime-event contract versions. It does not describe
Pafio manifests, lock state, managed toolchains, registry policy, or hosted
workspace state.

### 2.2 `styio --compile-plan <path>`

Pafio produces a resolved compile-plan with `generated_by.tool = "pafio"`.
Styio validates and consumes that plan for `check`, `build`, `run`, and `test`.
Styio remains the authority for concrete diagnostics, `receipt.json`,
`diagnostics.jsonl`, `runtime-events.jsonl`, and compiler artifacts.

Invalid plans and CLI conflicts return stable machine-readable compiler
failures. Styio does not resolve dependencies, modify `pafio.lock`, or repair a
Pafio project.

### 2.3 Language-service contracts

Vityo consumes syntax, semantic, diagnostic, and language-service contracts
directly from Styio. Those contracts are independent of Pafio storage and
Platform hosted state.

## 3. Pafio Project Contracts

### 3.1 `pafio metadata --json`

`metadata v1` contains only package, workspace, dependencies, targets, lock,
resolution, and vendor state. It has no cloud policy, managed-toolchain, hosted
workspace, or IDE aggregation fields.

### 3.2 `pafio --json check/build/run/test`

The stable workflow envelope records Pafio's action, target intent, sync state,
status, and Styio process status. It may surface paths to compiler-owned
outputs, but it does not redefine diagnostics, receipt, or runtime-event
schemas.

All four workflows perform the same dependency sync transaction first.
`--locked` forbids lock mutation, `--offline` forbids network access, and
`--frozen` enables both restrictions.

### 3.3 External compiler discovery

Pafio selects Styio in this order: `--styio-bin`, `PAFIO_STYIO_BIN`, then
`styio` on `PATH`. Compiler installation, update, switching, pinning, source
build, and caching are outside Pafio.

## 4. Platform And Vityo Boundaries

Styio Platform owns the registry service, registry control API, hosted
workspaces, cloud jobs, and workers. A worker invokes `pafio build` with a
system-provided Styio compiler.

Vityo combines exactly three sources:

1. Pafio metadata and workflow JSON for local project operations;
2. Styio machine and language-service contracts for compiler facts;
3. Platform hosted APIs for hosted workspace and cloud execution state.

Vityo does not inspect `PAFIO_HOME` or infer any of these facts from private
filesystem layout.

## 验收条件

1. `python3 scripts/ecosystem-cli-doc-gate.py` passes for the local owner
   matrix.
2. The workspace form of the same gate passes against fixed Pafio and Vityo
   revisions.
3. Compile-plan interoperability accepts `generated_by.tool = "pafio"` as the
   only ecosystem project producer. The compiler-only self identity remains
   confined to direct single-file `styio build`, not project workflows.
4. No active Styio document assigns project metadata, dependency resolution,
   registry hosting, or IDE aggregation to the compiler.
