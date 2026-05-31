# IM-D3 Diagnostic Contract Inventory

**Purpose:** Record the diagnostic contract decisions for IM-D3 so Styio diagnostics can become stable machine-readable compiler and service facts instead of ad hoc error text.

**Last updated:** 2026-05-31

**Status:** Public diagnostic baseline implemented; family-specific refinement remains tracked below. This document supports [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md) §5.7 `IM-D3`.

## Contract Manifest

| Contract item | Decided position | Implementation status |
|---------------|------------------|-----------------------|
| Diagnostic code naming | Public codes use `STYIO_<PHASE>_<ERROR_FAMILY>` | Implemented through [../../src/StyioServices/DiagnosticContract.hpp](../../src/StyioServices/DiagnosticContract.hpp) |
| Public phase taxonomy | Public phases are `lex`, `parse`, `sema`, `type`, `lowering`, `ir_verify`, `codegen`, `runtime`, `native_interop`, and `service` | Implemented in the shared taxonomy and emitted by public JSON/IDE/LSP surfaces |
| JSON diagnostic schema | External diagnostics share one schema with code, phase, severity, message, file, source location, byte span, and notes | Implemented for syntax-check JSON and main CLI JSONL; CLI JSONL uses `0` spans where no precise source span exists |
| Stability rule | `code` meaning is stable after publication; `message` may be improved without changing the code | Covered by focused syntax-check, compile-plan, runtime, type, IDE, and LSP tests |
| Exit code boundary | Process exit codes stay coarse-grained; detailed causes live in diagnostic codes | Implemented; existing exit families remain lex/parse/type/runtime/service |
| IDE/LSP boundary | IDE/LSP maps compiler/service diagnostics and must not invent compiler diagnostic codes | Implemented through `Diagnostic::code`, `Diagnostic::phase`, and LSP `Diagnostic.code` / `data.phase` |
| Editor snapshot diagnostics | Editor-only diagnostics must identify an editor source and must not masquerade as compiler diagnostics | Implemented with `source:"styio-editor"` and service diagnostic codes |
| Migration policy | Implement by public surface in batches; unconverted families stay registered here with owner, gate, and status | Active policy |

## Diagnostic Code Naming

Public diagnostic codes use:

```text
STYIO_<PHASE>_<ERROR_FAMILY>
```

Rules:

1. `STYIO_` is the required public prefix.
2. `<PHASE>` is an uppercase spelling of the public phase, not a C++ class, source directory, or implementation route.
3. `<ERROR_FAMILY>` names the stable error condition, not the exact human-readable message.
4. One code should correspond to one testable public error condition.
5. Do not create separate codes only because message wording differs.

Examples:

```text
STYIO_LEX_UNTERMINATED_STRING
STYIO_PARSE_UNEXPECTED_TOKEN
STYIO_PARSE_UNSUPPORTED_SYNTAX
STYIO_SEMA_UNDECLARED_SYMBOL
STYIO_TYPE_MISMATCH
STYIO_LOWER_UNSUPPORTED_AST
STYIO_IR_VERIFY_INACTIVE_NODE
STYIO_CODEGEN_UNSUPPORTED_IR
STYIO_RUNTIME_FILE_OPEN_FAILED
STYIO_NATIVE_UNRESOLVED_SYMBOL
STYIO_SERVICE_INVALID_ARGUMENT
```

These examples are naming references, not a complete code list. The complete public list must be produced incrementally from real diagnostic sites and tests.

## Implemented Public Code Families

| Surface | Codes |
|---------|-------|
| Syntax-check lex | `STYIO_LEX_INVALID_TOKEN`, `STYIO_LEX_UNTERMINATED_STRING`, `STYIO_LEX_UNTERMINATED_BLOCK_COMMENT` |
| Syntax-check parse | `STYIO_PARSE_UNEXPECTED_TOKEN`, `STYIO_PARSE_UNSUPPORTED_SYNTAX`, `STYIO_PARSE_SHADOW_MISMATCH` |
| Service / CLI | `STYIO_SERVICE_INVALID_ARGUMENT`, `STYIO_SERVICE_READ_FAILED`, `STYIO_SERVICE_COMPILE_PLAN_INVALID`, `STYIO_SERVICE_COMPILE_PLAN_CLI_CONFLICT`, `STYIO_SERVICE_EDITOR_SYNTAX`, `STYIO_SERVICE_LSP_RESYNC_REQUIRED` |
| Sema, type, and lowering baseline | `STYIO_SEMA_IMMUTABLE_BINDING`, `STYIO_SEMA_UNDECLARED_SYMBOL`, `STYIO_SEMA_CALL_ARITY_MISMATCH`, `STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH`, `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`, `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY`, `STYIO_TYPE_ERROR`, `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH`, `STYIO_TYPE_CALL_ARGUMENT_MISMATCH`, `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`, `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`, `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE`, `STYIO_TYPE_STDIN_UNSUPPORTED_TARGET`, `STYIO_LOWER_UNSUPPORTED_AST` |
| Runtime and native baseline | Runtime helper subcodes such as `STYIO_RUNTIME_FILE_OPEN_READ`, `STYIO_RUNTIME_FILE_OPEN_WRITE`, `STYIO_RUNTIME_NUMERIC_PARSE`; fallback `STYIO_RUNTIME_ERROR`; native interop codes such as `STYIO_NATIVE_SOURCE_READ_FAILED`, `STYIO_NATIVE_SIGNATURE_NOT_FOUND`, `STYIO_NATIVE_UNSUPPORTED_SIGNATURE`, `STYIO_NATIVE_HOST_COMPILE_FAILED`, `STYIO_NATIVE_LOAD_FAILED`, `STYIO_NATIVE_SYMBOL_MISSING`, `STYIO_NATIVE_TOOLCHAIN_UNAVAILABLE`, and fallback `STYIO_NATIVE_INTEROP_ERROR` for native/toolchain messages without a narrower code |

## Public Phase Taxonomy

| Phase | Scope |
|-------|-------|
| `lex` | Tokenization and source byte classification failures |
| `parse` | Grammar and AST construction failures |
| `sema` | Name binding, declaration, capability, and language-rule failures that are not pure type mismatch |
| `type` | Type inference, type compatibility, and type-family failures |
| `lowering` | AST-to-StyioIR conversion failures |
| `ir_verify` | StyioIR verifier contract failures |
| `codegen` | LLVM/codegen/JIT preparation failures before program runtime |
| `runtime` | Failures from executed runtime helpers or managed runtime resources |
| `native_interop` | Native ABI, source, symbol, toolchain, or linked artifact failures |
| `service` | CLI, manifest, LSP/service argument, protocol, and envelope failures |

Public phases are allowed to hide internal implementation moves. A source directory rename or pass split must not force consumers to change phase handling.

## JSON Diagnostic Schema

All public service diagnostics should converge on this shape:

```json
{
  "code": "STYIO_PARSE_UNSUPPORTED_SYNTAX",
  "phase": "parse",
  "severity": "error",
  "message": "unsupported syntax in authoritative nightly parser",
  "file": "case.false.styio",
  "line": 3,
  "column": 8,
  "offset": 42,
  "length": 2,
  "notes": []
}
```

Required fields:

1. `code`
2. `phase`
3. `severity`
4. `message`
5. `file`
6. `line`
7. `column`
8. `offset`
9. `length`
10. `notes`

`notes` is always present. It may be an empty array. Future notes should use the same code/phase/severity/location discipline when a note has its own machine-meaningful detail.

## Stability Rules

1. A published `code` must not change meaning.
2. Human-readable `message` wording may change when the same code still describes the same error condition.
3. Retiring a code requires a retired/deprecated record instead of silent removal from public docs.
4. Process exit codes remain coarse-grained and should not grow into one exit code per diagnostic code.
5. Public tests should assert `code`, `phase`, `severity`, source location, and exit family rather than brittle full message text.

## IDE And LSP Boundary

`StyioIDE` and `styio_lspd` may render, rank, batch, debounce, or map diagnostics, but they must not create compiler diagnostic codes that did not come from a compiler-owned or service-owned diagnostic family.

LSP mapping rules:

1. LSP `Diagnostic.code` should carry the Styio diagnostic code when the diagnostic came from the compiler or service layer.
2. LSP severity is derived from Styio `severity`.
3. Editor snapshot diagnostics may exist for interaction, but their source must identify the editor layer, not `styio-compiler`.
4. Editor snapshot diagnostics are not proof that source is accepted or rejected by the compiler parser.

## Migration Order

| Batch | Surface | Target |
|-------|---------|--------|
| D3-B1 | `styio check --syntax --json` | Completed: lex/parse/service diagnostics emit stable codes from the shared taxonomy |
| D3-B2 | Sema/type diagnostics | Baseline complete: public JSONL type diagnostics emit `STYIO_TYPE_ERROR`; immutable/final binding mutation failures now emit `STYIO_SEMA_IMMUTABLE_BINDING`; unresolved function/resource references now emit `STYIO_SEMA_UNDECLARED_SYMBOL`; user function and resource-method call arity mismatches now emit `STYIO_SEMA_CALL_ARITY_MISMATCH`; resource capability mismatches now emit `STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH`; pressure observers on unsupported resource families now emit `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`; user function and resource-method argument type mismatches now emit `STYIO_TYPE_CALL_ARGUMENT_MISMATCH`; resource-effect fallback type mismatches now emit `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH`; unsupported local container returns or local matrix/resource binding resource method bodies now emit `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY`; tuple function return annotations that fail closed before tuple value IR now emit `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`; undefined hash-tag stream routes now emit `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`; stream zip unsupported-source failures now emit `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE`; unsupported typed stdin target failures now emit `STYIO_TYPE_STDIN_UNSUPPORTED_TARGET`; narrower sema/type families remain refinement work |
| D3-B3 | Lowering, IR verifier, and codegen diagnostics | Baseline partial: unsupported-AST lowering has a public code path; narrower IR/codegen families remain refinement work |
| D3-B4 | Runtime and native interop diagnostics | Baseline complete for runtime helper subcodes, native/toolchain fallback, and source/signature/unsupported-signature/host-compile native interop refinements; remaining native symbol/toolchain families stay refinement work |
| D3-B5 | IDE/LSP bridge | Completed: LSP and C++ IDE surfaces map compiler/service codes without inventing compiler diagnostics |

Each batch must update this inventory, focused tests, and the affected public service docs in the same checkpoint.

## Pending Diagnostic Families

| Family | Current behavior | Target code family | Owner | Gate | Status |
|--------|------------------|--------------------|-------|------|--------|
| Syntax check CLI argument errors | JSON `cli_error` emits `phase:"service"` and stable service codes | `STYIO_SERVICE_*` | CLI / Nano + Test Quality | `StyioDiagnostics.*SyntaxCheck*` | Implemented D3-B1 |
| Lex failures | Syntax check reports lex phase with stable lex family codes | `STYIO_LEX_*` | Frontend + Test Quality | syntax-check lex diagnostic tests | Implemented D3-B1 |
| Parse failures | Syntax check reports parse phase with stable parse family codes | `STYIO_PARSE_*` | Frontend + Test Quality | parser/syntax-check diagnostic tests | Implemented D3-B1 |
| Sema and type failures | Public JSONL sema/type diagnostics still fall back to `STYIO_TYPE_ERROR` for broad families, with `STYIO_SEMA_IMMUTABLE_BINDING` for immutable/final binding mutation, `STYIO_SEMA_UNDECLARED_SYMBOL` for unresolved function/resource references, `STYIO_SEMA_CALL_ARITY_MISMATCH` for user function and resource-method call arity mismatches, `STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH` for resource capability mismatches, `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED` for pressure observers on unsupported resource families, `STYIO_TYPE_CALL_ARGUMENT_MISMATCH` for user function and resource-method argument type mismatches, `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH` for resource-effect fallback type mismatches, `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY` for unsupported local container returns or local matrix/resource binding resource method bodies, `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN` for unsupported tuple function return annotations, `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED` for undefined hash-tag stream routes, `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE` for non-iterable zip inputs, and `STYIO_TYPE_STDIN_UNSUPPORTED_TARGET` for unsupported typed stdin scalar/list targets; narrower semantic families are still message-family work | `STYIO_SEMA_*`, `STYIO_TYPE_*` | Sema / IR + Test Quality | focused sema/type diagnostics | Baseline implemented; immutable binding, undeclared symbol, call arity, resource capability mismatch, pressure observer unsupported-family, call argument mismatch, resource-effect fallback mismatch, resource-method unsupported body, tuple return, hash-tag route, stream zip source, and typed stdin unsupported-target refinements implemented; broader refinement pending |
| Lowering and IR verifier failures | IM-D1 fail-closed behavior exists; unsupported-AST lowering can map to `STYIO_LOWER_UNSUPPORTED_AST`; IR verifier-specific public families still need focused negative tests | `STYIO_LOWER_*`, `STYIO_IR_VERIFY_*` | Sema / IR + Codegen / Runtime | `StyioIRContract.*` plus lowering diagnostics | Baseline implemented; IR family refinement pending |
| Codegen failures | Public JSONL diagnostics now always carry a code/phase/notes envelope; narrower `STYIO_CODEGEN_*` families still need focused codegen negative tests | `STYIO_CODEGEN_*` | Codegen / Runtime | codegen negative diagnostics | Envelope implemented; family refinement pending |
| Runtime helper failures | Runtime helper subcodes are promoted to primary public codes while keeping `subcode` for compatibility | `STYIO_RUNTIME_*` | Codegen / Runtime | runtime diagnostic tests | Implemented D3-B4 |
| Native interop failures | Native/toolchain messages without narrower subcodes map to `STYIO_NATIVE_INTEROP_ERROR`; missing referenced source, explicit binding/signature misses, unsupported native signatures, and host compiler rejections report `STYIO_NATIVE_SOURCE_READ_FAILED`, `STYIO_NATIVE_SIGNATURE_NOT_FOUND`, `STYIO_NATIVE_UNSUPPORTED_SIGNATURE`, or `STYIO_NATIVE_HOST_COMPILE_FAILED`; other load/symbol/toolchain-specific families remain refinement work | `STYIO_NATIVE_*` | Frontend + Codegen / Runtime | native interop negative tests | Baseline implemented; source/signature/unsupported-signature/host-compile refinements implemented; broader family refinement pending |
| IDE/LSP diagnostics | IDE diagnostics carry code/phase; LSP publishes `Diagnostic.code` and `data.phase`; editor diagnostics identify `styio-editor` | mapped compiler/service codes plus editor source markers | IDE / LSP + Test Quality | IDE/LSP diagnostic tests | Implemented D3-B5 |

## IM-D3 Closure Position

IM-D3's public diagnostic baseline is implemented: every current public JSON/JSONL, IDE, and LSP diagnostic path now carries a machine-readable code and phase, and the main syntax-check, compile-plan, runtime, type, native interop, IDE, and LSP surfaces have focused tests. Current D3-B2 family refinements include immutable/final binding mutation failures reporting `STYIO_SEMA_IMMUTABLE_BINDING`, unresolved function/resource references reporting `STYIO_SEMA_UNDECLARED_SYMBOL`, user function and resource-method arity mismatches reporting `STYIO_SEMA_CALL_ARITY_MISMATCH`, resource capability mismatches reporting `STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH`, pressure observers on unsupported resource families reporting `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`, user function and resource-method argument type mismatches reporting `STYIO_TYPE_CALL_ARGUMENT_MISMATCH`, resource-effect fallback type mismatches reporting `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH`, unsupported local container returns or local matrix/resource binding resource method bodies reporting `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY`, tuple function return annotations reporting `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`, undefined hash-tag stream routes reporting `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`, stream zip unsupported-source failures reporting `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE`, and unsupported typed stdin target failures reporting `STYIO_TYPE_STDIN_UNSUPPORTED_TARGET` instead of the broad `STYIO_TYPE_ERROR`. Current D3-B4 native refinements report missing referenced native source as `STYIO_NATIVE_SOURCE_READ_FAILED`, explicit binding/signature misses as `STYIO_NATIVE_SIGNATURE_NOT_FOUND`, unsupported native signature families such as aggregate parameters or variadic signatures as `STYIO_NATIVE_UNSUPPORTED_SIGNATURE`, and host compiler rejections as `STYIO_NATIVE_HOST_COMPILE_FAILED` while keeping the TypeError exit family for those existing sema/codegen routes. These sema/type, resource-effect/resource-method, and native entries are diagnostic-only refinements over existing fail-closed behavior; they do not broaden function calling, resource capability rules, resource method dispatch, argument adaptation, symbol resolution, typed stdin target support, resource-effect recovery, resource method scalar and local list/dict flex/final preface semantics beyond the closed scalar/string-return slices, captures, dict-slice recovery, pressure stream payloads, pressure observer execution, native ABI support, native signature support, native symbol visibility, or host compiler behavior. Remaining work is not an IM-D3 blocker; it is family refinement that replaces broad fallback codes such as `STYIO_TYPE_ERROR`, `STYIO_RUNTIME_ERROR`, and `STYIO_NATIVE_INTEROP_ERROR` with narrower diagnostics when a later feature checkpoint owns those error families.
