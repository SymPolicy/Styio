# OPT-D final loop-control legality repair

**Purpose:** Record the repository-local test repair exposed by strict final-root loop-control verification.

**Last updated:** 2026-08-02

The final verifier correctly rejects resource-method bodies that clone `break` or `continue` outside an enclosing loop. The focused fixture now checks the cloned `BreakAST::Create(2)` and `ContinueAST` statement positions through direct resource-call lowering, where unresolved loop control is intentionally deferred, then requires lowering the same shape as a final `MainBlock` to throw `StyioTypeError` deterministically. No verifier, production lowering, or compatibility behavior is relaxed.
