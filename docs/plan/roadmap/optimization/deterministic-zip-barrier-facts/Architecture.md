# Deterministic Zip Barrier Facts

**Purpose:** Freeze the OPT-G architecture for one existing finite zip-barrier path.

**Last updated:** 2026-08-02

## Frozen closure

OPT-G closes one existing parser-through-codegen scenario: finite zip between two
materialized list values. It does not change syntax, element families, runtime
queueing, or observable iteration. Each matched index is one deterministic pulse
frame; the loop stops before a frame when either list has no item at that index.

File and standard-input drivers remain supported by their existing paths but do
not participate in this metadata migration. Snapshot joins, pressure scheduling,
timeouts, multi-writer merge, and host concurrency are separate IM-D5 work.

## Single fact owner

`SIOStreamZip` owns one inline immutable-by-convention `SGStreamZipBarrierFacts`
value. `SIOStreamZip::Create` initializes it directly; there is no nullable side
table, global registry, source-text recovery, or legacy-field fallback.

The compact value contains closed enums or fixed ordered values for:

- frame identity: the matched pair ordinal;
- members: exactly source A followed by source B;
- readiness: all members must be present;
- commit: once, after the body completes;
- termination: the shortest finite input.

The existing source-kind and element-type fields still describe how values are
loaded. They are not synchronization facts and codegen must not infer barrier,
frame, commit, or termination meaning from them.

## Construction and validation

The `SIOStreamZip::Create` factory is the only construction seam and emits a
canonical valid bundle in O(1) time and O(1) inline storage. AST lowering continues
to own source classification and calls that factory exactly once.

The StyioIR verifier checks the complete bundle before walking children. Any
unknown enum, reordered/duplicated member, non-all-members readiness, non-once
post-body commit, or non-shortest termination is malformed active IR and fails
closed. Codegen repeats a defensive validity guard because direct internal IR may
bypass the ordinary pass manager; it never reconstructs missing facts.

For materialized list/list zip, codegen consumes the bundle at the existing loop
header and body boundary:

1. compare the current pair ordinal with both lengths;
2. enter only when both members are ready;
3. load A and B for that ordinal;
4. run the body;
5. commit the pulse ledger and bounded pending resource effects once;
6. advance the ordinal.

An early body terminator does not invent another commit edge. Existing structured
control-flow handling remains authoritative.

## Complexity and representation assessment

The chosen representation adds constant inline bytes per zip node, constant-time
construction, constant-time verification, and no runtime allocation. A general
synchronization graph, event log, observer, visitor, or scheduler state machine
would only repeat two fixed members and five closed facts for this slice. Those
representations become useful only when variable-arity barriers, snapshot joins,
or runtime wait/pressure events enter scope, so none is earned here.

The 256-node probe must report exactly 256 valid bundles and exactly
`256 * sizeof(SGStreamZipBarrierFacts)` metadata bytes. It also verifies every
bundle has the canonical member order and closed semantics; duration is observed
but is not used as a machine-dependent acceptance threshold.

## Migration and non-goals

This is a complete fact migration for `SIOStreamZip`: there is no optional bundle,
compatibility inference, duplicate fact owner, or verifier exemption. Existing
tests that directly construct IR receive the canonical facts through the factory.
The slice does not satisfy the wider IM-D5 stop condition.
