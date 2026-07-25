# Explicit Binding Initialization Architecture

**Purpose:** Define the parser, AST, Sema, IR, diagnostics, and migration boundaries for mandatory ordinary-binding RHS expressions.

**Last updated:** 2026-07-14

## Architectural rule

There is one ordinary-binding lifecycle:

```text
complete source binding -> parsed RHS -> typed RHS -> lowered value -> visible binding
```

No ordinary name exists in an empty, cold, default-pending, or runtime-checked state. If RHS parsing, evaluation, type checking, or lowering fails, binding creation does not complete.

## Context classification

| Context | Value source | Ordinary storage rule |
|---------|--------------|-----------------------|
| `name [: T] = expr`, `name [: T] := expr` | Explicit RHS expression | Mandatory RHS; owned by this plan. |
| Parameter, pattern, iteration binder | Call, selected match arm, or iteration pulse | Supplied atomically by the enclosing construct. |
| `answer : T = ?| operation | fallback` | Explicit settlement-expression RHS | Ordinary binding; no settlement-specific declaration lifecycle. |
| `operation -> destination` inside or outside `?|` | Typed directional endpoint | Generic flow operation; endpoint grammar is distinct from an ordinary declaration. |
| Record/schema field | Later record construction | Shape declaration; construction/default policy is owned elsewhere. |
| Resource topology slot | Resource protocol/driver initialization | Protocol declaration; no ordinary `T` may be observed before validity. |
| Future raw/out storage | Explicit restricted storage API | Not ordinary binding and not designed by this plan. |

Parser and Sema code must classify by grammar/AST kind. They must not infer context from a nullable RHS pointer.

## Frontend

### Grammar

The canonical production is:

```ebnf
ordinary_binding = identifier [ ':' type_annotation ] ( '=' | ':=' ) expression ;
```

After parsing `identifier ':' type_annotation` in ordinary statement context, absence of `=` or `:=` plus an expression emits the stable missing-RHS diagnostic. Error recovery synchronizes at the current statement boundary and does not create an AST node.

### AST

The ordinary binding node owns a non-null expression by construction. Its factory/constructor contract should make an absent RHS unrepresentable or assert immediately at the frontend boundary. Supplied binders, schema fields, and topology slots use their existing distinct node families; no compatibility `UninitializedBindingAST` or default-valued placeholder is introduced.

## Semantic analysis

Sema analyzes the RHS before publishing the ordinary name into the completed binding environment, while still applying the language's separately owned recursion/self-reference rules. Binding metadata records mutability/finality and the proven value type, not an initialized bit.

Control-flow analysis never needs a path-sensitive “maybe initialized” lattice for ordinary bindings. Branch-first construction binds the result of a value Block once; deliberate absence uses an explicitly initialized `? | T`.

## IR and lowering

StyioIR receives only typed ordinary bindings with a real value operand. Verifiers reject malformed programmatic IR nodes that omit it. Lowering and codegen do not call a default-value repair path and do not allocate a runtime initialized flag.

Resource-driver buffers may internally reserve or clear bytes, but their protocol validity remains separate from source ordinary bindings. An audit finding routes to the resource-topology owner; it may not be “fixed” by exposing a zero-filled ordinary value.

## Diagnostics

The implementation checkpoint selects one stable code, proposed as `STYIO_PARSE_BINDING_RHS_REQUIRED`, with source span covering the binding head and declared type. The message states that an ordinary binding requires `=` or `:=` followed by an expression. It should not suggest adding `(?)`, `()`, or zero unless that explicit value is actually the author's intent.

## Migration and deletion

1. Add negative parser/security fixtures for representative type families and the stable diagnostic.
2. Delete `make_default_value_for_decl_latest` and every call site.
3. Remove positive tests that assert manufactured defaults.
4. Migrate only fixtures whose intended construct has a canonical explicit value or supplied-binder spelling.
5. Preserve rejected `name : T` examples solely as registered negative diagnostics.
6. Run a static search proving no default helper, nullable ordinary RHS, compatibility node, or old positive fixture remains.

## Complexity and data structures

The design removes state. Parsing stays linear in source length; Sema uses its existing symbol table with no definite-assignment dataflow set; ordinary binding metadata needs no initialization bit; runtime bindings need no hidden tag. Context classification is an AST-kind dispatch, not repeated source-pattern scanning.

## Dependency direction

```text
Language SSOT -> Parser/AST contract -> Sema proof -> StyioIR verifier -> Lowering/Codegen
                                      -> diagnostics/tests/tooling mirrors
```

Later layers consume the mandatory-RHS fact and cannot weaken it. Resource topology, record construction, and future raw storage remain separate owners and do not feed defaults back into the ordinary parser path.
