# IR Loop-Control Legality Design

**Purpose:** Define the verifier-only design boundary for IR loop-control legality.

**Last updated:** 2026-08-01

**Requirement:** `REQ-IR-LOOP-CONTEXT`

## Scope

Freeze the verifier-only design for rejecting `SGBreak` and `SGContinue` without an active enclosing loop body while preserving valid traversal through `SGLoop`, `SGForEach`, and `SGRangeFor`, including nesting.

The observed `StyioIRWalker`, SGIR ownership model, and codegen loop stack remain unchanged. Parser, Sema, lowering semantics, optimizer semantics, codegen/runtime semantics, multi-level break, adjacent verifier leaves, M7, and tuple/set/closure work are explicit non-goals. The lowering pass pipeline may carry an explicit fragment-verification policy so a block is not judged before its enclosing loop has been assembled.

## Design Pattern Assessment

This assessment covers the group and its sole material implementation node because both close one verifier-local state decision.

```text
pattern_catalog: refactoring-guru-catalog-22-v1
candidate: none
decision: reject
pressure: The existing visitor must distinguish loop-control leaves reached from an active loop body from identical leaves reached at the root or in a loop's pre-body operands, while nested visits must restore the prior context exactly.
expected_benefit: No catalog pattern provides a net benefit for one counter and five visitor overrides; the verifiable benefit instead comes from constant-time context checks, body-only scoping, and deterministic restoration.
simpler_alternative: Keep one verifier-local nesting counter, use a small RAII scope guard only around body traversal, and add two leaf conditionals. This fully satisfies the current contract in one O(N) walk with O(1) work per visited node and O(1) explicit verifier state.
application: In StyioIRVerifier only, walk each loop's condition/iterable/range operands at the inherited depth, enter a scoped increment only for its non-null body, and diagnose break/continue when the depth is zero. Acceptance independently exercises all three loop kinds, nested bodies, post-loop restoration, and pre-body operands.
costs_and_rejections: A State pattern would fragment two stable states into classes and obscure traversal; a new Visitor is unnecessary because the observed StyioIRWalker already supplies dispatch; Memento is unnecessary because one scalar can be restored directly; Template Method, Strategy, and Chain of Responsibility add variation points that do not exist. The local RAII guard is a language lifetime idiom, not a catalog-pattern layer.
```

## Architecture

### Verifier-local state

`StyioIRVerifier` in `src/StyioIR/Verifier.cpp` owns a zero-initialized `std::size_t loop_depth_`. A private, non-copyable `LoopDepthScope` (or equivalently named local scope guard) takes that counter by reference, increments on construction, and decrements in its `noexcept` destructor. No state is stored on SGIR nodes, in `StyioIRWalker`, or globally.

A private `walk_loop_body(SGBlock* body, const char* field)` helper must:

1. preserve the existing missing-required-child diagnostic when `body == nullptr` and return without changing depth;
2. otherwise create the scope guard and call the existing `walk(body)` exactly once; and
3. restore the incoming depth on every exit, including stack unwinding.

Each `verify_styio_ir` call creates a new verifier, so concurrent calls share no loop state. The existing `visited_` ownership/alias behavior is not changed.

### Visitor contract

The Worker changes only these existing or newly overridden methods in `StyioIRVerifier`:

| Visitor | Frozen traversal and check |
| --- | --- |
| `visitSGLoop` | `cond` remains optional and is walked at the inherited depth; only `body` is passed to `walk_loop_body(..., "SGLoop.body")`. |
| `visitSGForEach` | required `iterable` is walked at the inherited depth; only `body` is passed to `walk_loop_body(..., "SGForEach.body")`. |
| `visitSGRangeFor` | required `start`, `end`, and `step` are walked at the inherited depth; only `body` is passed to `walk_loop_body(..., "SGRangeFor.body")`. |
| `visitSGBreak` | when `loop_depth_ == 0`, call `add_error("break outside enclosing loop")`; otherwise remain a leaf. |
| `visitSGContinue` | when `loop_depth_ == 0`, call `add_error("continue outside enclosing loop")`; otherwise remain a leaf. |

Pre-body operands do not activate their own loop because codegen evaluates them before pushing that loop's frame. If one of these nodes is itself nested in an outer loop body, the inherited outer depth remains active, matching the existing nearest-loop behavior.

Both new errors retain `add_error`'s default phase/code: phase `ir_verify`, code `STYIO_IR_VERIFY_CONTRACT`. Diagnostic order remains depth-first traversal order. The verifier must not inspect, reinterpret, or mutate `SGBreak::depth`; SGIR construction and codegen keep the current single-level/nearest-loop contract, and this closure adds no multi-level target semantics.

### Complexity and failure semantics

The verifier remains one O(N) tree walk. Loop entry/exit and leaf legality checks are O(1); explicit added state is one counter regardless of nesting. A missing loop body reports the pre-existing required-child diagnostic and cannot leak positive loop depth into later siblings. An invalid control leaf adds a diagnostic but does not stop traversal or alter depth.

### Intermediate fragment boundary

`AstToStyioIRLowerer` optimizes some `SGBlock` fragments before their parent `SGLoop`, `SGForEach`, or `SGRangeFor` exists. Those fragment runs must continue checking ordinary IR structure while deferring only zero-depth `SGBreak` / `SGContinue` legality. The pass-pipeline option is explicit and defaults to strict verification; only intermediate block call sites enable deferral. The fully assembled `SGMainEntry` keeps strict verification before and after passes, so top-level loop control still fails closed and optimizer output cannot bypass the contract.

This is a construction-phase distinction, not a compatibility path: fragments are never accepted as final executable roots, the final root is always reverified, and the option does not change traversal, SGIR ownership, optimization, or codegen behavior.

## Acceptance Handoff

`tests/security/styio_security_test.cpp` owns the executable seam `StyioIRContract.VerifierRejectsLoopControlOutsideLoopsAndAcceptsNestedLoops`. It freezes:

- exact top-level break and continue phase, code, and message;
- independent acceptance of both controls in each of `SGLoop`, `SGForEach`, and `SGRangeFor` bodies;
- nested use across all three loop kinds, followed by illegal siblings after the outer loop to prove balanced restoration;
- rejection in every loop operand when no outer loop is active, preventing an over-broad enter-before-operands implementation; and
- acceptance of controls in nested-loop operands when an outer loop body is active, proving that operands inherit rather than clear the incoming depth.

The inactive lowering-internal target retains no duplicate compatibility test. The implementation handoff is complete when the focused verifier contract and the existing end-to-end loop-control cases in `Validation.md` pass without weakening final-root diagnostics.
