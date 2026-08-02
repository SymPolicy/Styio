# IR Loop-Control Legality Validation

**Purpose:** Define the focused acceptance evidence for IR loop-control legality.

**Last updated:** 2026-08-01

**Requirement:** `REQ-IR-LOOP-CONTEXT`

## Focused Acceptance

Run once after the Worker and Verifier handoff:

```sh
cmake --build build --target styio_security_test -j2 && ctest --test-dir build -R '^(StyioIRContract\.VerifierRejectsLoopControlOutsideLoopsAndAcceptsNestedLoops|StyioIRContract\.VerifierRejectsInactiveIR)$' --output-on-failure
```

The new executable case must prove all of the following in one fixture boundary:

1. Direct children of `SGMainEntry` containing `SGBreak` and `SGContinue` produce, in traversal order, `ir_verify` / `STYIO_IR_VERIFY_CONTRACT` diagnostics with exact messages `break outside enclosing loop` and `continue outside enclosing loop`.
2. Each supported loop kind independently accepts both leaves in its body.
3. An `SGLoop` containing an `SGForEach` containing an `SGRangeFor` accepts inner and enclosing-body controls; controls after the outer loop remain illegal, proving nested decrement and outer restoration.
4. A control leaf used as the current loop's condition, iterable, start, end, or step remains outside that loop body's context when there is no enclosing outer loop.
5. The same operand positions accept control leaves when the current loop is nested in an outer loop body, proving that operands inherit rather than clear the incoming outer depth.
6. The existing unsupported-active-node contract test remains green; missing-child and inactive-node behavior are unchanged and are re-covered at the group boundary.

The focused run is expected to fail before `IR-LOOP-IMPLEMENT` because the current verifier inherits both control leaves as unchecked. Do not weaken exact diagnostic assertions to make the scaffold pass early.

## Fragment Integration Acceptance

The failure-driven repair must additionally prove that intermediate `SGBlock` pass-pipeline runs defer only unresolved loop-control context, while the assembled `SGMainEntry` remains strict. Use the three existing legal loop/codegen cases and `RejectsBreakAndContinueOutsideLoop` as the executable oracle; do not replace them with fragment-only tests or disable structural verification globally.

## Group Regression

After exactly one group Reviewer, run the impacted regression once:

```sh
cmake --build build --target styio_security_test -j2 && ctest --test-dir build -R '^(StyioIRContract\.(VerifierRejectsLoopControlOutsideLoopsAndAcceptsNestedLoops|VerifierRejectsInactiveIR)|StyioSecurityNightlyCodegen\.(BreakRunsFileScopeCleanupBeforeLoopExitBranch|ContinueRunsFileScopeCleanupBeforeLoopStepBranch|LongStandaloneContinueNormalizesToNearestLoop|RejectsBreakAndContinueOutsideLoop))$' --output-on-failure && bash scripts/docs-gate.sh
```

Pass criteria are: the frozen focused case passes unchanged; existing required-child, unsupported-active, and inactive-node verifier contracts pass; codegen cleanup/nearest-loop and top-level rejection tests remain green; and documentation gates pass. This is validation of the verifier closure, not authority to change parser, Sema, codegen, runtime, SGIR ownership, or multi-level behavior.
