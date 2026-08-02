# Deterministic Zip Barrier Validation

**Purpose:** Freeze the executable OPT-G acceptance matrix.

**Last updated:** 2026-08-02

## Requirement matrix

| Requirement | Executable evidence |
| --- | --- |
| `REQ-OPT-G-ZIP-FACTS` | Lowering test inspects the canonical bundle on a list/list zip; verifier tests reject each malformed field; codegen test proves the generated list/list loop checks both lengths and has one body commit path. |
| `REQ-OPT-G-BOUNDED-EVIDENCE` | The 256-node benchmark reports exactly 256 bundles and exactly `256 * sizeof(SGStreamZipBarrierFacts)` bytes with all facts valid. |

## Focused scenarios

1. Lower two unequal materialized lists and require pair-ordinal identity, ordered
   A/B members, all-members readiness, after-body-once commit, and shortest-input
   termination.
2. Mutate each closed fact independently and require `verify_styio_ir` to emit a
   stable `SIOStreamZip` diagnostic without traversing into codegen.
3. Pass malformed direct IR to codegen and require fail-closed behavior instead
   of reconstruction from source flags, element types, or pulse fields.
4. Generate and execute an unequal list/list zip whose body records output;
   require exactly `min(len(A), len(B))` ordered pairs and no unmatched item.
5. Exercise a pulse-plan body and inspect one prologue/body/epilogue edge per
   matched pair, with pending commits after the body.
6. Construct 256 canonical zip nodes in the benchmark and require exact node and
   metadata-byte counts; release all nodes within the measured iteration.

## Regression boundary

The focused run builds only lowering, codegen, security, and benchmark targets,
then runs tests labelled `StyioZipBarrierFacts` and
`StyioSecurityZipBarrierFacts` plus the exact benchmark oracle. It must also pass
`git diff --check`.

After the unique Reviewer exits, final validation runs the impacted stream tests,
the exact 256-node oracle, diff hygiene, and the documentation gate once. A failed
full run is replayed only through the smallest failing command before a repair
node is planned.

## Explicit exclusions

No acceptance case adds file/stdin driver facts, snapshot-join syntax, queues,
pressure observers, timeout policy, writer merge, native scheduling, or a general
concurrency graph. Existing I/O zip coverage remains regression-only evidence.
