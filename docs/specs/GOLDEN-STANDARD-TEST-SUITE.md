# Golden Standard Test Suite

**Purpose:** Define the Styio language repository test level that makes a compiler version submittable.

**Last updated:** 2026-07-02

## CI Gates

`test / smoke` is the fast pre-submit signal. It builds the compiler and runs the smallest milestone and fuzz smoke checks that prove the runner, toolchain, lexer/parser, and executable path are alive.

`test / golden-standard` is the complete submit readiness gate. It must run after the platform gate and smoke gate. It freezes language behavior against the canonical oracle set below.

## Golden Oracle Set

The golden-standard suite is the union of these checks:

- `scripts/syntax-convergence-gate.py`: validates `docs/design/syntax/SYNTAX-CONVERGENCE-MATRIX.json`; every accepted syntax feature must declare exactly one implementation, documentation evidence, and golden cases.
- `ctest --test-dir build/golden -L milestone`: validates milestone stdout/stderr/file behavior against `tests/milestones/**/expected`.
- `ctest --test-dir build/golden -L styio_pipeline`: validates the five-layer Lexer, AST, StyioIR, LLVM IR, and subprocess stdout goldens in `tests/pipeline_cases`.
- `ctest --test-dir build/golden -R '^parser_shadow_gate_'`: validates parser shadow artifacts and route convergence gates.
- `ctest --test-dir build/golden -R '^parser_legacy_entry_audit$'`: rejects legacy parser entry points outside the parser core and explicit parity harness.

## Local Gate Profile

`styio-syntax-convergence-profile` is the repository-owned adaptation for the language core. It is maintained in this repository through the syntax convergence matrix, parser legacy entry audit, and five-layer pipeline cases. The organization-level audit only verifies that this local profile is present and covered by `test / golden-standard`; it does not define the language-specific oracle outside Styio.

Required local markers: repo-owned adaptation, syntax convergence matrix, parser legacy entry audit, five-layer pipeline cases.

## Industry Gate Group

`language / compiler-quality` is the role-specific gate group for the Styio programming language. It keeps compiler-quality checks grouped under `test / golden-standard` instead of exposing many separate required checks. The group follows compiler practice from LLVM and Rust: small regression cases, parser and IR goldens, whole-pipeline behavior, negative diagnostics, and fuzz/smoke coverage.

Required evidence markers: syntax convergence, parser shadow, five-layer, milestone, fuzz, golden cases.

## Submit Readiness

A Styio compiler change is submittable only when `platform-adaptation / linux-ci-gate`, `test / smoke`, and `test / golden-standard` all pass.

Syntax changes have one extra rule: the changed syntax family must remain converged in `SYNTAX-CONVERGENCE-MATRIX.json`. If one syntax feature needs more than one implementation, the change is not ready; split the feature, retire one implementation, or add a documented migration boundary before promotion.
