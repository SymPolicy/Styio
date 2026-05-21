# Test Catalog

**Purpose:** Define the feature-based Styio test inventory, CTest labels, fixture layout, and gate commands for language acceptance coverage.

**Last updated:** 2026-05-10

---

## 1. Layout Rules

All language acceptance fixtures live under `tests/features/<feature>/`.

Each feature directory owns:

1. `t*.styio` positive executable fixtures.
2. `e*.styio` semantic or diagnostic failure fixtures when needed.
3. `expected/*.out` stdout goldens for positive fixtures.
4. `expected/*.err` diagnostic fragments for negative fixtures.
5. `data/` only when fixtures need local resource input.

CTest names use `<feature>_<fixture-name>` for positive fixtures and `<feature>_err_<fixture-name>` for negative fixtures. All feature fixtures carry the `language_feature` label plus their feature label.

Run the full language acceptance surface:

```bash
ctest --test-dir build/default -L language_feature --output-on-failure
```

Run one feature:

```bash
ctest --test-dir build/default -L scalar_expressions --output-on-failure
```

---

## 2. Feature Suites

| Feature label | Directory | Coverage focus | Targeted command |
|---------------|-----------|----------------|------------------|
| `scalar_expressions` | `tests/features/scalar_expressions/` | integer/float arithmetic, precedence, strings, bindings, comparison, logic, stdout shorthand | `ctest --test-dir build/default -L scalar_expressions --output-on-failure` |
| `functions` | `tests/features/functions/` | function definitions, typed returns, block bodies, call chains, no-param functions, nested calls | `ctest --test-dir build/default -L functions --output-on-failure` |
| `control_flow` | `tests/features/control_flow/` | match expressions, defaults, loops, break, continue, factorial/fizzbuzz examples | `ctest --test-dir build/default -L control_flow --output-on-failure` |
| `wave_dispatch` | `tests/features/wave_dispatch/` | wave merge/dispatch plus rejected fallback/noop spelling evidence | `ctest --test-dir build/default -L wave_dispatch --output-on-failure` |
| `file_resources` | `tests/features/file_resources/` | `@file`, file reads/writes, RAII, redirect, file pipelines, fail-fast file cases | `ctest --test-dir build/default -L file_resources --output-on-failure` |
| `state_resources` | `tests/features/state_resources/` | Topology-style state resources and retired state-family negative diagnostics | `ctest --test-dir build/default -L state_resources --output-on-failure` |
| `stream_processing` | `tests/features/stream_processing/` | zip, snapshots, instant pull, file-backed streams, singleton, arbitrage, stream output artifacts | `ctest --test-dir build/default -L stream_processing --output-on-failure` |
| `final_bindings` | `tests/features/final_bindings/` | bounded final binding, reads, and same-name flex rejection diagnostics | `ctest --test-dir build/default -L final_bindings --output-on-failure` |
| `stdio_output` | `tests/features/stdio_output/` | `@stdout`, `@stderr`, format strings, mixed stdout/stderr assertions, legacy print smoke | `ctest --test-dir build/default -L stdio_output --output-on-failure` |
| `stdio_input` | `tests/features/stdio_input/` | `@stdin`, typed pulls, EOF, mixed output, invalid stdio resource direction diagnostics | `ctest --test-dir build/default -L stdio_input --output-on-failure` |
| `native_interop` | `tests/features/native_interop/` | native C/C++ extern compilation, linking, raw body source, and JIT calls | `ctest --test-dir build/default -L native_interop --output-on-failure` |
| `task_resources` | `tests/features/task_resources/` | task flow, task pull, captured values, task groups, and invalid task_flow diagnostics | `ctest --test-dir build/default -L task_resources --output-on-failure` |

---

## 3. Special Gates

| Gate | Purpose | Command |
|------|---------|---------|
| `StyioDiagnostics.SyntaxCheckRejectsNonAuthoritativeParserEngine` | Prove the public syntax-check contract is locked to the authoritative nightly parser. | `ctest --test-dir build/default -R '^StyioDiagnostics\.SyntaxCheckRejectsNonAuthoritativeParserEngine$' --output-on-failure` |
| `StyioSemanticBridge.RejectsMalformedInputWithoutRecovery` | Prove IDE semantic facts are not recovered from malformed source after strict compiler parsing fails. | `ctest --test-dir build/default -R '^StyioSemanticBridge\.RejectsMalformedInputWithoutRecovery$' --output-on-failure` |
| `StyioSyntaxDrift.CorpusMatchesApprovedEnvelope` | Keep editor syntax snapshots documented as non-authoritative when they intentionally differ from strict compiler parsing on malformed source. | `ctest --test-dir build/default -R '^StyioSyntaxDrift\.CorpusMatchesApprovedEnvelope$' --output-on-failure` |
| `parser_shadow_gate_scalar_expressions_zero_fallback_and_internal_bridges` | Keep scalar expression fixtures identical across parser engines with no fallback records. | `ctest --test-dir build/default -R '^parser_shadow_gate_scalar_expressions_zero_fallback_and_internal_bridges$' --output-on-failure` |
| `parser_shadow_gate_functions_zero_fallback_and_internal_bridges` | Keep function fixtures identical across parser engines with no fallback records. | `ctest --test-dir build/default -R '^parser_shadow_gate_functions_zero_fallback_and_internal_bridges$' --output-on-failure` |
| `parser_shadow_gate_file_resources_dual_zero_expected_nonzero` | Keep file-resource parser artifacts matched while allowing registered fail-fast fixtures in `shadow-expected-nonzero.txt`. | `ctest --test-dir build/default -R '^parser_shadow_gate_file_resources_dual_zero_expected_nonzero$' --output-on-failure` |
| `parser_shadow_gate_stream_processing_zero_fallback` | Keep stream-processing fixtures matched with zero legacy fallback. | `ctest --test-dir build/default -R '^parser_shadow_gate_stream_processing_zero_fallback$' --output-on-failure` |
| `parser_shadow_gate_stream_processing_zero_internal_bridges` | Keep stream-processing fixtures matched with zero internal legacy bridges. | `ctest --test-dir build/default -R '^parser_shadow_gate_stream_processing_zero_internal_bridges$' --output-on-failure` |
| `parser_legacy_entry_audit` | Reject new parser legacy-entry regressions. | `ctest --test-dir build/default -R '^parser_legacy_entry_audit$' --output-on-failure` |

---

## 4. Recovery Gate

`checkpoint-health.sh` runs the feature catalog as part of the repository health floor through `styio_pipeline`, shadow gates, security, parser diagnostics, soak, and docs checks.

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

For docs-only changes that still touch this catalog, use the delivery gate with health skipped only when no compiler/test fixture behavior changed:

```bash
./scripts/delivery-gate.sh --skip-health
```
