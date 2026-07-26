# Styio Structured Resources and Concurrency

**Purpose:** Define the accepted `D3-RESOURCES` capability, ownership,
streaming, backpressure, cancellation, and structured-lifetime model.

**Last updated:** 2026-07-26

**Status:** Accepted owner decision `D3-RESOURCES`.

## 1. Capability and ownership are orthogonal

Resource-like values are described by canonical item type, capabilities,
protocol state, ownership kind, and exit obligations. The words resource,
stream, and task do not by themselves make every value affine.

- A runtime/program-owned root topology identity is not a movable first-class
  owner.
- A local close-capable handle, exclusive cursor, driver subscription, or task
  handle carrying a join/settle obligation is affine.
- A materialized snapshot, range, or value stream may use value semantics.
- A borrowed stream view is a lexical borrow and cannot escape its owner.

Capabilities such as source, sink, duplex, pull, iteration, push, close, and
clone are statically resolved protocol facts. Unknown or ambiguous capability
facts fail closed.

## 2. Direction and transfer

`source -> destination` remains one directional transfer and succeeds with
`unit`. The destination protocol declares its single `copy`, `borrow`, or
`consume` input mode and every normal/completion ownership post-state. The
arrow itself does not select ownership or scheduling.

`>>` denotes iteration, subscription, or pulse delivery according to the
statically selected source/consumer protocol. It does not turn a collection
into a stream or create an implicit queue.

Duplicate source consumption never creates an implicit broadcast. Tee,
broadcast, sharing, or fan-out requires an explicit admitted protocol.

## 3. Structured tasks and scope exit

Tasks are structured by default. A child task belongs to a lexical/session
scope and must finish, or receive cancellation followed by join, before any
resource reachable by that child is released. Version 1 has no detached or
background-task escape hatch.

Normal scope exit joins children before commit/publication and resource
release. Failure or external cancellation requests cooperative cancellation,
then joins children, aborts only unpublished pending state, and continues the
declared flush/close obligations. A cancellation request is not proof that a
child has terminated.

Already committed ownership transitions and external effects do not roll
back. Cleanup remains deterministic: dependencies order last-borrow, join,
commit/abort, flush, close, and drop; otherwise peers release in stable reverse
acquisition order. A cleanup failure does not silently skip remaining cleanup.

Cancellation is cooperative and idempotent and propagates down the owned task
tree. More permissive lifetime inference may be added only when the compiler
can prove the same obligations.

## 4. Streams, termination, and pressure

Pull and push are complementary protocol directions rather than a global
language choice. A demand-coupled implementation may pull one upstream item
and push one downstream pulse. External active sources must still enter a
bounded admission policy.

EOF is a normal terminal state distinct from absence, recoverable failure,
cancellation, shutdown, and pressure. A protocol exposes each applicable
state explicitly.

Every asynchronous edge has a declared finite buffering/backpressure policy.
There is no hidden unbounded queue. When capacity is unavailable, the default
is to suspend, decline, or report pending according to the protocol. Silent
drop, overwrite, retry, and latest-wins behavior require an explicit protocol
contract.

Pressure is an observable scheduling/resource signal, not automatically a
completion. Only a protocol-declared unrecoverable escalation produces a
nominal completion family visible to settlement.

## 5. Library and extension boundary

Files, networks, codecs, databases, devices, and concrete drivers live in
explicitly imported standard-library or package modules. They do not enter the
prelude merely because the core resource protocol exists.

User resource protocols must declare capabilities, transfer mode, item type,
buffer/pressure behavior, completion families, terminal states, and cleanup
obligations. Missing facts are rejected rather than inferred from callbacks or
method spelling.
