# Styio Unicode Text and Binary Values

**Purpose:** Define the accepted text units, Unicode identity rules, binary
value families, indexing boundaries, normalization behavior, and standard
library boundary.

**Last updated:** 2026-07-26

**Status:** Accepted owner decision `Q06-TEXT-BINARY`.

## 1. Type identities

`string`, `char`, `scalar`, `bytes`, `bits`, and `blob` are ordinary names in
the type namespace. They are not lexer keywords and do not become callable
expression heads. Their built-in behavior follows canonical type identity,
not the spelling currently bound to a short name.

- `scalar` is one Unicode scalar value.
- `char` is one Unicode extended grapheme cluster and may contain multiple
  scalars.
- `string` is a valid, length-aware UTF-8 text value. Embedded U+0000 is
  ordinary text and does not terminate it.
- `bytes` is a contiguous binary value whose element type is `u8`.
- `bits` is an arbitrary-length bit sequence; its length need not be divisible
  by eight.
- `blob` is an opaque binary payload. Its contents have no implicit element,
  text, codec, equality, ordering, or schema interpretation.

There is no scalar `byte` type. Octets use `u8`; binary aggregates use
`bytes`, `bits`, or `blob` according to their semantic contract.

`bytes`, `bits`, and `blob` may be supplied by optional prelude profiles or
explicit standard-library modules. Their names remain ordinary and
shadowable; they add no keyword or dedicated literal grammar.

## 2. Text operations and units

The default human-text unit is the extended grapheme cluster:

- `string[index]`, `string.len`, and ordinary text slicing operate on
  grapheme-cluster boundaries;
- `.scalars` exposes an explicit scalar view;
- `.bytes` exposes an explicit UTF-8 byte view;
- APIs concerning terminal columns or rendered width must say
  `display_width` and cannot reuse grapheme length.

Grapheme indexing and slicing do not promise O(1) random access. An
implementation may cache validated boundaries, but cache presence and layout
are not observable language semantics.

Malformed UTF-8 cannot inhabit an ordinary `string`. Decoding arbitrary bytes
is a fallible operation in an explicitly imported codec/text standard-library
module.

## 3. Equality and normalization

Ordinary `string` and `char` construction performs no automatic Unicode
normalization. Equality compares the exact Unicode scalar sequence. Therefore
canonically equivalent but differently encoded text need not compare equal.

Canonical-equivalence comparison and normalization are explicit library
operations, including `canon_eq`, `nfc`, and `nfd`. They are not prelude
functions and are not silently inserted by assignment, comparison, hashing,
module loading, or I/O.

## 4. Display

The runtime preserves and emits valid Unicode text without replacing supported
content merely because it contains combining sequences, variation selectors,
Emoji modifiers, regional-indicator flags, or ZWJ sequences. This includes
complex writing systems and modern Emoji sequences.

The language guarantees the emitted scalar sequence, not the final glyph.
Font coverage, terminal capabilities, shaping, bidi presentation, and display
width remain properties of the rendering environment.

## 5. Source identifiers

Identifier admission follows Unicode XID start/continue classes. Identifier
identity is NFC-normalized. Emoji and arbitrary grapheme clusters are valid
text but are not identifiers.

The frontend fails closed for dangerous invisible controls and for confusable
identifier collisions in the same scope. Other suspicious mixed-script names
produce a deterministic diagnostic. Diagnostics must show enough escaped
scalar information to distinguish visually confusable names.

## 6. Library boundary

Encoding/decoding, codecs, JSON, CSV, database mapping, normalization, display
width, and binary interpretation live in explicitly imported standard-library
modules. None is injected into the language prelude merely because `string`,
`bytes`, `bits`, or `blob` exists.

Conversions among text and binary families are explicit and unit-named. A
view may borrow its source; a materialized conversion owns an independent
value. No operation guesses an encoding from content.
