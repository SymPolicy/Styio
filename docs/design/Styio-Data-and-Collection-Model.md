# Styio Data and Collection Model

**Purpose:** Define the accepted `D1-DATA` and `D2-COLLECTIONS` identity,
pattern, ordering, indexing, view, iteration, and materialization model.

**Last updated:** 2026-07-26

**Status:** Accepted owner decisions `D1-DATA` and `D2-COLLECTIONS`.

## 1. Data identities

Tuple types are structural ordered products. Their canonical identity is the
arity plus the ordered canonical identities of their element types. Empty,
single-element, and multi-element tuple syntax must preserve arity
unambiguously; field names are not part of tuple identity.

Declared record types are nominal products. Two records with identical field
names and types are not automatically interchangeable. Field declaration
order is a source/API fact, but native ABI layout is never inferred merely
from structural similarity.

Declared variant types are nominal sums. A case belongs to exactly one
variant identity; equal tag spelling or payload shape in another variant does
not create equivalence.

FFI and layout adaptation is an explicit boundary relation. Structural
similarity never silently creates ABI compatibility.

## 2. Construction and patterns

Patterns mirror the corresponding construction form. Tuple positions, record
field names, and variant cases retain the same identities during construction,
destructuring, and matching.

A pattern binding of a value field receives a value. A binding of an affine
owner field consumes that owner unless the pattern is explicitly operating
through an admitted borrow/view. A borrow cannot escape the matched owner's
region. Destructuring cannot duplicate or silently discard an owner.

A match over a closed variant is exhaustive. Missing reachable cases,
unreachable arms, duplicate cases, invalid rest patterns, and guards that
cannot establish a complete cover fail statically. Guards do not make an
otherwise incomplete closed match exhaustive. Completion propagation follows
the ordinary operation-summary model.

Equality, hashing, and ordering are capabilities derived from the complete
component facts and the nominal type's explicit protocol implementations.
Shape alone does not grant a nominal record or variant a capability.

## 3. Materialized collections

Materialized collections use recursive value semantics. A value collection
does not become affine merely because it allocates storage internally. It is
affine only when its semantic contents include an affine owner.

The ordinary collection families have deterministic observable order:

- sequences preserve element order;
- maps preserve their specified stable iteration order;
- sets preserve their specified stable iteration order;
- matrices preserve shape and index order.

Equality, hashing, key admission, ordering, and matrix-shape compatibility are
capabilities, not guesses based on storage layout.

`list[T]` is a materialized collection type. It is not an alias or rewrite for
the stream/repetition spelling `T..`. A materialized collection, iterator,
stream, range, and resource history are distinct semantic categories.

## 4. Slices, views, and materialization

An ordinary slice expression produces a stable value snapshot. Later mutation
of the source is not observable through that snapshot.

An explicitly requested view borrows the source, follows the source's admitted
mutation rules, and cannot outlive it. Copy-on-write is an implementation
strategy only when it preserves the same value-snapshot observations.

Range, slice, stride, negative-index, empty-range, and out-of-range behavior
must be defined per collection capability and cannot be inferred from host
container behavior. Public APIs name the unit when more than one unit is
plausible, such as grapheme, scalar, byte, bit, element, row, column, or
display width.

## 5. Iterators and streams

An iterator/stream is not the collection it traverses. Its yield type,
completion state, invalidation, borrowing, mutation permission, and
single-consumer status are explicit protocol facts.

- A materialized value stream may have value semantics.
- A borrowed stream view is a borrow.
- An exclusive cursor or subscription carrying consumption/close obligations
  is an affine owner.

Iteration order is deterministic. Mutation during iteration is accepted only
when the selected protocol states a stable result; otherwise it is rejected.
Implicit tee, fan-out, duplicate consumption, and hidden materialization do not
occur.

## 6. Domain values and libraries

`string`, `bytes`, `bits`, and `blob` follow
[Styio Unicode Text and Binary Values](./Styio-Unicode-Text-and-Binary.md).
Their searches, slices, and views use explicit units.

Codec, decode, schema mapping, JSON, CSV, and database conversions are
explicitly imported standard-library facilities. They are not prelude
functions and do not add special collection grammar.
