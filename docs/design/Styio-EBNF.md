# Styio Formal Grammar (EBNF)

**Purpose:** 词法与语法的 **EBNF 权威定义**；资源拓扑相关附录与叙述以 [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md) 为准，语义细节以 [`Styio-Language-Design.md`](./Styio-Language-Design.md) 为准。

**Last updated:** 2026-07-20

**Version:** 1.0-draft  
**Date:** 2026-03-28  
**Parser Strategy:** LL(n) Recursive Descent with Maximal Munch Lexing

---

## 1. Notation Conventions

```
=          definition
|          alternation
{ ... }    repetition (zero or more)
[ ... ]    optional (zero or one)
( ... )    grouping
"..."      terminal string
'...'      terminal character
```

---

## 2. Lexical Grammar

The lexer follows the **Maximal Munch Principle**: when multiple token interpretations are possible, the longest valid match wins.

### 2.1 Character Sets

```ebnf
digit          = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' ;
letter         = 'a'..'z' | 'A'..'Z' | '_' ;
identifier     = letter { letter | digit } ;
```

### 2.2 Literals

```ebnf
int_literal    = [ '-' ] digit { digit } ;
float_literal  = [ '-' ] digit { digit } '.' digit { digit } ;
string_literal = '"' { any_char_except_dquote | escape_seq } '"' ;
char_literal   = "'" ( any_char_except_squote_or_backslash | char_escape_seq ) "'" ;
escape_seq     = '\' ( 'n' | 't' | 'r' | '\' | '"' | '0' ) ;
char_escape_seq = '\' ( 'n' | 't' | 'r' | '0' | '\' | '\'' ) ;
```

`int_literal` and `float_literal` are source grammar categories, not concrete
storage types. Their accepted exact-value, fail-closed materialization, late
`i64`/`f64` default, and closed scalar `Add` semantics are owned by
[Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md).
That semantic decision does not add a suffix, radix, separator, exponent,
keyword, token, or production; only spellings present in this grammar are
accepted.

### 2.3 Core Compound Symbols

These are ordered by **priority** for maximal-munch disambiguation.

```ebnf
(* Resource / State *)
TOK_AT             = '@' ;

(* Retired state-reference prefix: `$` before an identifier is a parse error *)
TOK_DOLLAR         = '$' ;

(* Derived-binding capture head `$(`; design-accepted, parser pending *)
TOK_DOLLAR_PAREN   = '$(' ;

(* Rightward directional transfer plus the separately defined left-arrow token *)
TOK_ARROW_RIGHT    = '->' ;
TOK_ARROW_LEFT     = '<-' ;

(* Reserved wave tokens: reserved symbols only. They participate in no syntax
   feature, and no grammar production may reference them, until the language
   design explicitly declares an activation. The parser rejects every
   occurrence. *)
TOK_WAVE_LEFT      = '<~' ;
TOK_WAVE_RIGHT     = '~>' ;

(* Match and probe *)
TOK_MATCH          = '?=' ;

(* Yield / Return *)
TOK_YIELD          = '<|' ;
TOK_INLINE_RETURN  = '|<|' ;
TOK_PIPE_SEMI      = '|;' ;

(* Pipe *)
TOK_PIPE           = '>>' ;
TOK_TASK_LAUNCH    = '||>' ;
TOK_AWAIT_PIPE     = '?|' ;      (* legacy token name; operation settlement or callable completion-bound marker by grammar context *)
TOK_SESSION_MARK   = '|?|' ;   (* resource session marker; mid-transfer placement *)
TOK_SESSION_EXIT   = '|!|' ;   (* session-exit special handling, e.g. |!|(cleanup) *)
TOK_SETTLE_FWD     = '|>' ;    (* deferred settlement / control transfer after session *)
(* '|<-' remains reserved; no production consumes it. *)

(* IO buffer *)
TOK_IO_BUF         = '>_' ;

(* Binding *)
TOK_BIND           = ':=' ;

(* Resource copy / compatibility pull *)
TOK_SHIFT_BACK     = '<<' ;

(* Hash (callable / operation-channel binding prefix) *)
TOK_HASH           = '#' ;

(* Dot run: two or more dots. '..', '...', and longer runs are normalized
   by context as range separators, selector separators, or type repetition suffixes. *)
TOK_DOT_RUN        = '..' { '.' } ;

(* Standard operators *)
TOK_PLUS           = '+' ;
TOK_MINUS          = '-' ;
TOK_STAR           = '*' ;
TOK_SLASH          = '/' ;
TOK_PERCENT        = '%' ;
TOK_POWER          = '**' ;
TOK_EQ             = '==' ;
TOK_NEQ            = '!=' ;
TOK_GT             = '>' ;
TOK_LT             = '<' ;
TOK_GTE            = '>=' ;
TOK_LTE            = '<=' ;
TOK_AND            = '&&' ;
TOK_OR             = '||' ;
TOK_NOT            = '!' ;

(* Delimiters *)
TOK_LPAREN         = '(' ;
TOK_RPAREN         = ')' ;
TOK_LBRACKET       = '[' ;
TOK_RBRACKET       = ']' ;
TOK_LBRACE         = '{' ;
TOK_RBRACE         = '}' ;
TOK_COMMA          = ',' ;
TOK_COLON          = ':' ;
TOK_SEMICOLON      = ';' ;
TOK_DOT            = '.' ;
TOK_ASSIGN         = '=' ;
TOK_PLUS_ASSIGN    = '+=' ;
TOK_MINUS_ASSIGN   = '-=' ;
TOK_STAR_ASSIGN    = '*=' ;
TOK_SLASH_ASSIGN   = '/=' ;
TOK_PIPE_SINGLE    = '|' ;    (* anchored grammar separator; never a general value operator *)
TOK_CARET          = '^' ;
TOK_TILDE          = '~' ;
TOK_QUESTION       = '?' ;
```

### 2.4 Variable-Length Tokens

These tokens use contiguous repetitions. Break and standalone continue keep the
spelling flexible but do not assign semantic depth to the count.

```ebnf
BREAK_TOKEN        = '^' { '^' } ;           (* length >= 1, contiguous, depth = 1 *)
CONTINUE_TOKEN     = '>' '>' { '>' } ;       (* length >= 2, contiguous, standalone context, depth = 1 *)
```

### 2.5 Comments

```ebnf
line_comment       = '//' { any_char_except_newline } ;
block_comment      = '/*' { any_char } '*/' ;
```

---

## 3. Program Structure

```ebnf
program            = { top_level_statement [ statement_sep ] } EOF ;

statement_sep      = ';' | '|;' ;

top_level_statement = import_declaration
                    | type_rewrite_decl
                    | resource_slot_decl
                    | internal_resource_decl
                    | resource_member_decl
                    | statement ;

statement          = declaration
                   | ordinary_binding
                   | compound_assignment
                   | conditional_stmt
                   | task_group_launch
                   | resource_order_stmt
                   | match_bind_expr
                   | flow_pipeline
                   | expression_stmt
                   | schema_def ;
```

---

## 4. Declarations

### 4.1 Import Declaration

```ebnf
import_declaration = '@' 'import' '{'
                     import_path { import_separator import_path }
                     '}' ;

import_separator  = ',' | ';' ;

import_path       = identifier { '/' identifier }
                  | identifier { '.' identifier } ;
```

Notes:

1. `@import` is only valid at file top level.
2. `/` is the native package/module path spelling.
3. `.` is accepted compatibility syntax and is normalized to slash form internally.
4. One import item must not mix `/` and `.`.

### 4.2 Callable / Operation-Channel Binding

```ebnf
declaration        = callable_decl ;

callable_decl      = '#' identifier callable_decl_tail ;

callable_decl_tail = [ callable_contract ] [ capture_list ]
                     ( bind_op callable_body
                     | '=>' callable_tail )
                   | '(' [ param_list ] ')' [ callable_contract ] [ capture_list ]
                     ( bind_op callable_tail
                     | '=>' callable_tail
                     | '?=' match_body ) ;

callable_contract  = ':' type_annotation [ completion_upper_bound ] ;

completion_upper_bound = '?|' '{'
                         completion_family_ref
                         { ',' completion_family_ref }
                         '}' ;

completion_family_ref = identifier ;

bind_op            = '=' | ':=' ;

callable_body      = [ '#' ] function_body ;

callable_tail      = block | expression ;

capture_list       = '$' '(' identifier { ',' identifier } ')' ;

function_body      = '(' [ param_list ] ')' [ '=>' ] callable_tail ;

param_list         = param { ',' param } ;
param              = identifier [ ':' type_annotation ] ;

type_annotation    = type_expr ;

type_expr          = optional_type ;

optional_type      = '?' '|' type_expr
                   | suffixed_type ;

suffixed_type      = type_primary { type_suffix } ;

type_primary       = scalar_type
                   | identifier [ '[' type_arg_list ']' ]
                   | '(' type_expr ')'
                   | tuple_type ;

scalar_type        = 'i8' | 'i16' | 'i32' | 'i64' | 'i128'
                   | 'f32' | 'f64'
                   | 'bool' | 'char' | 'string' | 'byte'
                   | 'matrix' | 'unit' | 'never' ;

type_arg_list      = type_expr { ',' type_expr } ;

tuple_type         = '(' type_expr ',' type_expr { ',' type_expr } ')' ;

type_suffix        = fixed_length_suffix
                   | recent_length_suffix
                   | infinite_repeat_suffix ;

fixed_length_suffix = '|' expression '|' ;       (* T|n| *)

recent_length_suffix = '|' dot_run expression '|' ;  (* T|..n| *)

infinite_repeat_suffix = dot_run ;               (* T.., T..., T..... *)

dot_run            = '..' { '.' } ;              (* length >= 2 *)
```

Notes:

1. `#` marks the binding target as callable or operation-channel-like. It is not
   a resource prefix and should not be taught as a traditional `def` keyword.
2. `=` creates a mutable binding; `:=` creates a final binding. The rule is the
   same for ordinary names and for `#` callable bindings.
3. `# name = #(args) => expr` and `# name := #(args) => expr` use the optional
   hash on the right side as an explicit callable-body marker in a `#` binding.
4. A resource atom cannot be the direct right side of a `#` binding. For example,
   `# sink = @stdout` is invalid because `@stdout` must stay visibly a resource.
   Native `@ extern(...)` import bindings are the separate native-callable import
   form, not resource aliasing.
5. `unit` and `never` are written as literals in this grammar for readability,
   but the tokenizer emits ordinary `NAME` tokens; type resolution recognizes
   them contextually and adds no keyword token.
6. `? | T` is the canonical source spelling for optionality. Repeated forms are
   accepted but normalize as a set-like union, so `? | (? | T) == ? | T`.
   `? | unit` still has two states because optional presence is independent of
   Unit payload bytes.
7. `?|` is contiguous in `completion_upper_bound`; it is a different lexer
   token from the spaced `? | T` Optional prefix. The completion clause is
   available only after a callable's normal result annotation and is not an
   ordinary `type_expr` or value union.
8. `# f : T ?| {io, parse} := ...` declares the finite nominal completion
   upper bound `{io, parse}`. The braces and commas are compile-time signature
   structure, not a runtime set/dict/block value. Family references are
   identifiers resolved by Sema; duplicate or non-family names are errors.
9. The family list is non-empty and has no trailing comma. A callable contract
   written as `: T` with no completion clause always declares the empty upper
   bound; `?| {}` is redundant and rejected.
10. Only an eligible non-boundary lexical-local or module-private callable that
    omits the entire `callable_contract` may infer its complete operation
    summary. Public/exported, recursive, native/FFI, and typed protocol-boundary
    callables require every parameter type and the callable contract. Writing
    `: T` never asks the compiler to infer hidden completion families.
11. Eligibility requires a capture-safe, final `:=`, non-recursive callable
    value. Its definition-site principal constrained rank-1 scheme generalizes
    only variables not free in the lexical environment. The detailed rule is
    owned by [Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md).
12. Every permitted use freshly instantiates that stable scheme. First use,
    future callers, source/hash order, numeric or backend defaults, `any`, and
    `dynamic` cannot determine it.
13. A bare `# f = ...` only checks a replacement against an already established
    scheme. It never creates or changes a scheme; an initial mutable callable
    requires a complete explicit contract.
14. Compiler displays such as `forall T`,
    `Add(T, IntegerLiteral(5), T, Completion(T))`, and type
    variables are metalanguage, not source. The concrete closed relation is
    owned by
    [Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md),
    while `F02` owns any future author-written generic or completion-row
    surface.
15. `overflow` is a payload-free prelude completion-family identifier, not a
    keyword. Checked integer `+`, strict floating `+`, literal defaulting, and
    compile-time completion edges are semantic rules and add no token or
    production.

### 4.3 Type Rewrite Declaration

```ebnf
type_rewrite_decl  = type_placeholder ':' type_expr ':=' type_expr ;

type_placeholder   = '__' { '_' } ;              (* two or more underscores *)
```

Examples:

```styio
__ : list[T] := T..
__ : string := char..
__ : dict[K, V] := (K, V)..
```

### 4.4 Schema Declaration

```ebnf
schema_def         = '#' identifier ':=' 'schema' '{'
                       { schema_field }
                     '}' ;

schema_field       = '@' '[' ( integer | identifier ) ']' identifier ;
```

---

## 5. Ordinary Bindings And Assignments

```ebnf
ordinary_binding   = identifier [ ':' type_annotation ] bind_op expression ;

compound_assignment = identifier compound_assign_op expression ;

compound_assign_op = '+=' | '-=' | '*=' | '/=' ;

derived_binding    = identifier ':=' '$(' identifier { ',' identifier } ')'
                     '=>' expression ;
```

`ordinary_binding` covers both creation and mutable `=` rebinding; semantic
analysis distinguishes them from the symbol table. `:=` creates a final binding
and cannot rebind an existing name. The RHS `expression` is mandatory in every
ordinary case: `name : T` is not a production and is rejected rather than
creating zero, Unit, absence, an uninitialized slot, or an implicit default.

Typed binder positions are owned by their enclosing productions rather than by
`ordinary_binding`. For example, parameters and pattern/iteration binders
receive a value atomically from the call, match, or iteration operation. Schema
fields and resource topology slots are declarations of shape/protocol, not
empty ordinary storage bindings, and this grammar grants them no implicit
construction default. Settlement introduces no special target-declaration
exception: bind its produced value with an ordinary RHS, for example
`answer : T = ?| operation | fallback`.

(* Derived binding: frame-committed derived slot; see Language Design §5.3.
   Design-accepted, parser pending, fails closed. Fail-closed whitelist:
   module/topology scope only; `:=` only; the body is one pure expression
   checked by the dedicated derived-binding effect-free whitelist; captured
   names are module-scope
   mutable bindings or other derived bindings; `$()`, duplicate names, unused
   captures, identity aliases (`x := $(y) => y`), and cycles including
   self-capture are rejected; after initialization, captured variables may be
   rewritten only inside pulse-frame contexts; derived names stay out of task
   blocks until the task memory model lands. The removed head spelling
   `name $(deps) := expr` is not syntax. *)

---

## 6. Retired State Declarations (state-resource)

Retired state-resource state containers and state references are not active grammar productions. The parser
rejects the retired prefixes with migration diagnostics. New topology code uses `@name : Type`
resource declarations, `expr -> @name` writes, and resource-object selectors.

---

## 7. Flow Pipelines

```ebnf
conditional_stmt   = guard '=>' ( block | expression ) [ '|' block ] ;

flow_pipeline      = stream_source '>>' consumer
                   | conditional_loop ;

conditional_loop   = infinite_gen '>>' guard '=>' ( block | expression ) ;

stream_source      = infinite_gen
                   | collection
                   | resource
                   | identifier ;

infinite_gen       = '[' dot_run ']' ;

guard              = '?' '(' expression ')' ;

consumer           = [ closure_sig ] '=>' ( block | expression )
                   | identifier ;

closure_sig        = '#' '(' [ param_list ] ')' ;

block              = '{' [ block_content ] '}' ;

block_content      = sole_expression
                   | statement_sequence [ statement_sep ]
                   | [ statement_sequence statement_sep ] block_yield [ statement_sep ] ;

sole_expression    = expression ;

statement_sequence = statement { statement_sep statement } ;

block_yield        = '<|' expression
                   | '|<|' expression '|;' ;
```

`sole_expression` is selected only when the Block contains exactly one ordinary
expression item and no separator or explicit yield. It is value sugar:
`=> expr`, `=> { expr }`, and `=> { <| expr }` have the same result. It does not
create general tail-expression semantics; a multi-item Block needs an explicit
`block_yield` for a non-Unit result. The inline spelling has its own mandatory
`|;` in the production and therefore cannot be accepted without it.

Every reachable natural closing brace contributes `() : unit`. Reachable normal
exits must have compatible canonical result types; `T` plus reachable Unit
fallthrough is an error and never synthesizes a default or `? | T`. A proven
non-completing edge has type `never` and joins as `join(T, never) = T` without
fallback. Both yield spellings target only the immediately owning lexical Block;
only the outer function-body Block result is adapted to a function result.
`<| ()` is legal. Unit-only consumers reject a non-Unit yield, and structurally
unreachable siblings after unconditional completion are compile-time errors.

Q03-F adds no production. Top-level items recognized by `statement_sequence`
form the Block's explicit order-sensitive sequence: an earlier item normally
settles before a later order-sensitive item starts, and completion skips later
ordinary items while preserving mandatory exit obligations. By contrast,
comma-separated arguments/elements and ordinary operator operands do not gain
a source-position time edge. They are strict value prerequisites; two unordered
order-sensitive siblings are rejected and must be prebound/settled as Block
items. See
[Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md).

A `conditional_stmt` without an else block is statement-only. If its guard is
false, it executes no branch statements and completes with Unit `()`; it never
manufactures absence or a default. A value-position guard is instead parsed by
`conditional_value_expr` and requires both branches separated by its
grammar-anchored `|`.

In `flow_pipeline`, the `>>` operator is the two-character iterator/pulse transfer
operator. The `stream_source` side must produce an iterable sequence or pulse stream;
the operator advances that source one item at a time and pushes each item as a pulse
into the `consumer`. This is not a bit shift, shell pipe, or single bulk send.

`stream_source guard '>>' consumer` is intentionally not part of the grammar.
Conditional infinite loops use `[...] >> ?(condition) => { ... }`, so
`[...] ?(condition) >> { ... }` is rejected before type checking.

### 7.1 Stream Zip (Aligned Sync)

```ebnf
zip_pipeline       = stream_source '>>' closure_sig
                     '&'
                     stream_source '>>' closure_sig
                     '=>' block ;

(* `&` is an event-arrival barrier: the first-arriving side blocks and waits
   until the other side delivers, then the block fires once per matched pair.
   The removed tolerance-window spelling `&[expr]` is rejected; `&` takes no
   bracketed argument. Pressure observer streams are not zip sources: a
   conflated level stream contradicts the barrier semantics. *)
```

### 7.1.1 Pressure Observer Stream

```ebnf
pressure_observer  = expression '.' 'pressure' '>>' closure_sig '=>' block ;
```

(* `pressure` is a Sema-recognized member attribute in the member namespace,
   not a reserved grammar word. The parser routes `expr.pressure >> #(p) =>`
   through the attribute/iterator path; Sema rejects every current resource
   family with `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`.
   Contract at activation (Language Design §6, backpressure): delivery is a
   single-slot conflated latest-wins level sensor (no reading queue, no
   meta-pressure); the payload is a prelude-declared read-only struct
   `{ pending: i64, limit: i64, peak: i64 }` pending the general struct story;
   pulses fire only on hysteresis state transitions (enter / exit / escalate);
   observer bodies run as ordinary frames off the writer path; observer-to-
   observed-resource write cycles, duplicate observers, non-module-scope
   observers, and pressure streams as zip sources are rejected. *)

### 7.2 Snapshot Declaration

```ebnf
snapshot_decl      = '@' '[' identifier ']' '<<' resource ;
```

### 7.3 Immediate Pull

```ebnf
instant_pull       = '(' '<-' resource ')' ;

legacy_instant_pull = '(' '<<' resource ')' ;  (* compatibility only; do not use in new design text *)
```

### 7.4 Tasks

```ebnf
task_expr          = '||>' block ;

task_group_launch  = '||>' '[' task_group_entry { statement_sep task_group_entry } ']' ;

task_group_entry   = identifier ( ':=' | '=' ) block ;

resource_order_stmt = identifier '=>' identifier ;
```

Tasks do not own a special await/binder production. A task-producing operation
is settled by the same `settlement_expr` as every other settleable operation,
and an ordinary result is bound through the ordinary binding grammar:
`answer : T = ?| job | fallback`. The shapes `?| job -> answer : T` and
`?| -> answer : T` are not productions. The former must not be confused with a
valid generic directional transfer such as `job -> answer`, nor with settlement
of such a transfer: `?| job -> answer | fallback` groups as
`?| (job -> answer) | fallback`.

### 7.5 Resource Session

```ebnf
session_block      = '|?|' block ;

session_exit       = '|!|' '(' effect_name ')' '=>' ( block | expression ) ;

session_forward    = '|>' expression ;

(* Mid-transfer: execution symbols both before and after |?|.
   Examples:  # f => |?| { ... } |!|(cleanup) => handler
              # f := |?| { ... } |> g
              a => |?| { ... } |> b |> c |> cleanup => handler

   Statement-start settlement opens with ?|:
              ?| |?| { ... } | cleanup => handler
              ?| |?| { ... } |> next |> cleanup => handler

   Body whitelist: handles and anchors only. Topology @name : Type inside
   session_block is rejected (Resource Topology §4.1 / §4.2).
   Design-accepted; parser-pending and fail-closed until implementation. *)

effect_name        = identifier ;  (* cleanup, ResourceCleanupFailure, … *)
```

---

## 8. Expressions

### 8.1 Expression Precedence (High to Low)

| Level | Operators | Associativity |
|-------|-----------|---------------|
| 1 (highest) | `()`, `[]`, `.` | Left |
| 2 | Unary: `!`, `-`, `^...` | Right |
| 3 | `**` | Right |
| 4 | `*`, `/`, `%` | Left |
| 5 | `+`, `-` | Left |
| 6 | `>`, `<`, `>=`, `<=` | Left |
| 7 | `==`, `!=` | Left |
| 8 | `&&` | Left |
| 9 | `\|\|` | Left |
| 10 | `>>`, `?=` | Left |
| 11 (lowest) | `=`, `+=`, etc. | Right |

Precedence and associativity determine only the parse tree. They never create
an operand-evaluation timeline. Q03-F separately requires strict prerequisites,
explicit dependency/effect edges, and static rejection of unordered
order-sensitive siblings.

`<|` does not appear in the expression precedence table: it is a lexical
Block-completion marker only (`block_yield`), and the infix apply-pipe
production is removed. Ordinary application uses call syntax `f(a)(b)`;
continuation syntax is not activated by this rule.

A single `|` is not an expression operator and therefore has no precedence.
The parser consumes it only inside a grammar production that has already fixed
its role: the union delimiter in type position (including `? | T`), the else
separator after `?(cond) => ...`, or a handler/fallback separator inside a
settlement expression that begins with `?|`. The parser rejects `a | b`,
`true | false`, `0 | 1`, and longer bare-pipe chains before semantic analysis;
type inference, truthiness, and purity analysis may not reinterpret them.

There is no ordinary value-level fallback or coalescing operator. In
particular, neither `a | b` nor `a ?? b` is a source expression. `??` has no
token, grammar production, or semantic role in the target language.

### 8.2 Expression Grammar

```ebnf
expression         = settlement_expr
                   | directional_transfer
                   | range_expr
                   | conditional_value_expr ;

range_expr         = conditional_value_expr dot_run conditional_value_expr ;

settlement_expr    = '?|' settleable_operation
                     { '|' named_completion_arm }
                     [ '|' expression ] ;

settleable_operation
                   = directional_transfer
                   | range_expr
                   | conditional_value_expr ;

named_completion_arm
                   = completion_family [ '(' identifier ')' ]
                     '=>' ( block | expression ) ;

completion_family  = identifier ;

directional_transfer
                   = transfer_source '->' transfer_destination ;

transfer_source    = range_expr
                   | conditional_value_expr ;

transfer_destination
                   = postfix_expr
                   | terminal_handle ;

(* `->` has one graphical, directional meaning: place the value produced at the
   left location into the destination/receiver endpoint drawn on the right.
   A name, resource, task-result receiver, channel, or terminal is an endpoint
   kind, not a separate arrow role. The destination does not declare a name and
   must independently resolve to a legal writable endpoint. Successful transfer
   has result `() : unit`; it never yields an implicit source, destination, or
   receipt. The endpoint protocol determines compatibility, completion families,
   and lowering. Q03-F fixes preparation ordering without adding grammar:
   source value and endpoint capability are independent prerequisites of the
   transfer, so arrow direction does not imply source-before-endpoint
   preparation. Two order-sensitive preparations require prior Block items.
   This rule still does not decide ownership, backpressure scheduling, chaining,
   or arrow associativity.

   `?|` is orthogonal: it settles the complete `settleable_operation`. Therefore
   `?| source -> destination | fallback` is always
   `?| (source -> destination) | fallback`; it never invokes a task-only await
   binder and never declares `destination`. *)

(* The infix apply-pipe production (`f <| a <| b`) is removed. Ordinary
   application uses `postfix_expr` call chains such as `f(a)(b)`. `<|` appears
   only in `block_yield`; no continuation surface follows from this grammar. *)

conditional_value_expr
                   = guard '=>' logic_or_expr '|' logic_or_expr
                   | logic_or_expr ;

(* There is deliberately no production of the form
   expression '|' expression. Bare binary value pipe is a syntax error, not a
   type-directed or purity-directed fallback candidate. There is likewise no
   `??` value-fallback production. *)

logic_or_expr      = logic_and_expr { '||' logic_and_expr } ;

logic_and_expr     = equality_expr { '&&' equality_expr } ;

equality_expr      = relational_expr { ( '==' | '!=' ) relational_expr } ;

relational_expr    = additive_expr { ( '>' | '<' | '>=' | '<=' ) additive_expr } ;

additive_expr      = multiplicative_expr { ( '+' | '-' ) multiplicative_expr } ;

multiplicative_expr = power_expr { ( '*' | '/' | '%' ) power_expr } ;

power_expr         = unary_expr [ '**' power_expr ] ;

unary_expr         = ( '!' | '-' ) unary_expr
                   | postfix_expr ;

postfix_expr       = primary_expr { selector | call | member_access } ;

selector           = '[' selector_body ']' ;

selector_body      = dot_run                         (* x[..], x[...] *)
                   | expression dot_run [ expression ]  (* x[a..], x[a..b] *)
                   | dot_run expression              (* x[..b] *)
                   | '%' expression                  (* x[%n] stride selector *)
                   | expression_list ;

(* Selectors are a pure-symbol selection algebra: no identifier participates
   in selector syntax. The word-mode production `[ selector_mode ',' ] ...`
   with `avg` / `max` / `min` / `std` / `rsi` is removed from the design.
   Series intrinsics use ordinary call syntax `avg(series, n)` recognized by
   semantic analysis, the same model as matrix helper calls. The parser path
   that still accepts `[avg, n]` / `[max, n]` is compatibility debt.

   The stride selector `x[%n]` keeps elements at index ≡ 0 (mod n); `n` is a
   positive integer, `[%1]` is identity, `[%0]` is rejected. Parser, Sema,
   lowering, code generation, and runtime support are active. There is no left
   operand inside the bracket, so `[%` never collides with binary modulo. *)

(* Retired selector families are parser errors outside registered negative tests. *)

call               = '(' [ expression_list ] ')' ;

member_access      = '.' identifier ;

expression_list    = expression { ',' expression } ;
```

Calls, ordinary operators, selectors, and expression lists are strict: every
required child must produce its value before the parent operation starts.
Independent children do not gain a left-to-right time edge from their source
positions. If two children are order-sensitive and no accepted data, control,
resource, ownership, or Block edge orders them, Sema rejects the parent and the
author first binds/settles them in consecutive Block items. `&&` and `||` are
the exception only in the precise sense that their right operand is selected
by a short-circuit control edge and is not evaluated when unselected.

`range_expr` is the naked expression-level range form, such as `start..end`.
It is not a list literal. Step range spelling such as `start..end..step` is
removed from the design and rejected by the parser.

`settlement_expr` is the only effect-settlement surface and always wraps one
complete operation. A bare `operation | fallback` is not settlement syntax.
Resource and task operations do not create another `?|` grammar role, and an
inner generic directional transfer remains a directional transfer. `?=` does
not catch effects; it matches only ordinary values already materialized.

`completion_family` and the optional payload binder are identifiers, not
keywords. `io(err)` therefore means “resolve the family identifier `io` and
bind its payload locally as `err`”; neither spelling is reserved. The bare form
matches the same exact nominal family without binding its payload. Binding a
no-payload family is rejected by Sema.

The operation executes once. Success bypasses recovery; only the selected arm
executes, lazily and once, with no implicit retry. Named arms match exact family
identity, duplicates are rejected, and a catch-all expression must be last. A
bare fallback catches only remaining recoverable failures. All normally
completing arms must join with the operation's success type; `never` contributes
no normal value. Unhandled families and failures from recovery expressions
propagate statically. Absence remains `? | T`; EOF, cancellation, and shutdown
are not matched by bare fallback; fatal/trap is outside settlement; pressure is
not a completion family until an owning protocol explicitly escalates it.

There is deliberately no `?| operation | ...` production. That retired parser
candidate is a syntax error in statement and expression positions.

### 8.3 Primary Expressions

```ebnf
primary_expr       = identifier
                   | int_literal
                   | float_literal
                   | string_literal
                   | char_literal
                   | 'true' | 'false'
                   | unit_value
                   | optional_empty
                   | resource
                   | collection
                   | instant_pull
                   | legacy_instant_pull
                   | '(' expression ')'
                   | '?' '(' expression ')'        (* guard condition prefix; followed by => for value selection *)
                   | block ;

unit_value         = '(' ')' ;

optional_empty     = '(' '?' ')'
                   | '[' '?' ']'
                   | '{' '?' '}' ;
```

All three `optional_empty` productions construct the same empty Optional value;
the delimiters do not create different types or states. They require an expected
`? | T` payload type or a compatible control-flow join. Exact-token recognition
disambiguates `(?)` from a parenthesized expression, `[?]` from a collection, and
`{?}` from a Block before the enclosing generic production is entered. `()` is
the sole value of `unit` and is not Optional empty.

---

## 9. Resources

```ebnf
resource           = std_stream_resource
                   | '@' identifier '(' expression ')'
                   | '@' '{' expression '}'
                   | '@' '(' [ expression ] ')' ;

std_stream_resource = '@stdout' | '@stderr' | '@stdin' ;

resource_slot_decl  = '@' identifier ':' type_annotation
                      { ',' '@' identifier ':' type_annotation }
                      [ ':=' driver_block ] ;

driver_block        = '{' flow_pipeline '}' ;

internal_resource_decl = '@' identifier [ ':' type_annotation ] ':=' '#' '(' [ param_list ] ')' '=>' block ;

resource_member_decl = '@' identifier resource_member_selector ( '=' | ':=' )
                       ( function_body | expression ) ;

resource_member_selector = '::' identifier
                         | '.' identifier ;
```

Examples:
- `@price : f64|..10|` — top-level resource slot
- `@("localhost:8080")` — auto-detect
- `@()` — empty resource / destroy sink
- `@file("readme.txt")` — explicit file
- `@binance("BTCUSDT")` — exchange feed
- `@mysql("localhost:3306")` — database
- `@file::close = () => { @file -> @() }` — mutable resource-family method

### 9.1 Standard Stream Resources

Standard streams are compiler-recognized resource atoms over the terminal device primitive
`>_`. User programs may use `@stdout`, `@stderr`, and `@stdin` directly; internally, these
resources are still governed by Styio prelude declarations rather than by a C++ name registry.

Usage patterns (reuse existing productions):
- `expr '->' '@stdout'` / `expr '->' '@stderr'` — canonical standard-stream write via `resource_redirect`
- `iterable_expr '>>' '@stdout'` / `iterable_expr '>>' '@stderr'` / `iterable_expr '>>' '@file(...)'` — writable-resource iterable write via `resource_write`; lowering advances the iterable item by item into the sink
- `iterable_expr '>>' terminal_handle` — terminal-handle resource-write shorthand; semantic checks require an iterable, text-serializable value, then advance it item by item into the terminal sink
- `string_expr '.lines()' '>>' terminal_handle` — explicit newline split before terminal-handle iterable write
- `'@stdin' '>>' '#' '(' param_list ')' '=>' block` — iterate via `iterator`
- `'@' 'stdin' ':=' '#' '(' ')' '=>' '{' '<|' terminal_handle '}'` — internal stdin declaration shorthand (`<|[>_]` or `<|(>_)`)
- `'@' 'stdin' ':=' '#' '(' ')' '=>' '{' '<|' '<-' terminal_handle '}'` — internal stdin declaration expanded form
- `'@' 'file' ':' 'ftype' ':=' '#' '(' identifier ')' '=>' block` — internal file resource declaration; the body must not call `file(path)`
- `'(' '<-' '@stdin' ')'` — immediate pull via `instant_pull`
- `'(' '<<' '@stdin' ')'` — compatibility pull via `legacy_instant_pull`
- `identifier (',' identifier)* '<-' '@stdin' ':' (type | '(' type (',' type)* ')')` — typed stdin pull; scalar, tuple, and single-target `list[T]` forms share the same `instant_pull` AST entry.

```ebnf
terminal_handle    = '[' '>_' ']'
                   | '(' '>_' ')' ;  (* compatibility terminal-device spelling *)

string_lines       = expression '.' 'lines' '(' ')' ;
```

Note: `expr '>>' '@stdin'` is syntactically accepted as `resource_write`, then rejected by
semantic checks because `@stdin` is read-only.

---

## 10. Collections

```ebnf
collection         = list_literal | tuple_literal | materialized_range ;

list_literal       = '[' [ expression { ',' expression } ] ']' ;

tuple_literal      = '(' expression ',' expression { ',' expression } ')' ;

materialized_range = '[' range_expr ']' ;

(* Step range spellings such as `[start..end..step]` are removed from the
   design. There is no step-range production; the parser rejects those
   spellings, and `[start..end]` is the only materialized range form. *)
```

`[start..end]` is a materialized range. It materializes the expression-level
range `start..end` as an iterable `list[i64]` source, and
`[start..end] >> #(x) => { ... }` pushes each materialized element into the
consumer one at a time. The parser must not interpret `[start..end]` as a
`list_literal` containing one naked `range_expr`; there is no comma-separated
list element in that form. `[start..end..step]` and equivalent step spellings
are removed from the design and rejected by the parser.

`matrix` annotations reuse nested `list_literal` syntax as their source form. A binding such as `m: matrix = [[1,0],[0,1]]` triggers rectangular numeric row validation in the typed context and lowers to a matrix handle; untyped nested list literals remain ordinary lists. Matrix operations such as `matmul(a,b)`, `transpose(m)`, `mat_shape(m)`, and `mat_set(m,r,c,v)` are ordinary identifier calls at the grammar level and are recognized by semantic analysis.

List and tuple elements are strict prerequisites of construction. Comma order
preserves element position in the resulting value but does not itself impose a
time order between independent element computations; unordered
order-sensitive elements are rejected under Q03-F. A materialized range first
obtains its required bound values under the same rule.

---

## 11. Pattern Matching

```ebnf
match_expr         = expression '?=' match_body ;

match_bind_expr    = '#(' identifier '=' expression ')' '?=' match_body ;

match_body         = '{' { match_arm } [ default_arm ] '}' ;

match_arm          = pattern '=>' ( block | expression ) ;

default_arm        = wildcard '=>' ( block | expression ) ;

wildcard           = '_' | underscore_identifier ;

pattern            = int_literal
                   | guarded_int_pattern
                   | float_literal
                   | string_literal
                   | identifier
                   | collection_pattern ;

guarded_int_pattern = '(' identifier '==' int_literal ')'
                    | '(' int_literal '==' identifier ')' ;

underscore_identifier = '_' '_' { '_' } ;

collection_pattern = '[' { pattern { ',' pattern } } ']'
                   | '(' { pattern { ',' pattern } } ')' ;
```

`#(name = expr) ?= { ... }` binds the scrutinee once and matches the bound
name. Integer literal arms and guarded integer equality arms such as `(n == 1)`
are canonicalized to the same match arm value when the guard references the
match scrutinee. Source spellings that are semantically equivalent converge in
the StyioIR optimizer before LLVM codegen.

The scrutinee is evaluated exactly once before arm selection. Arms are tested
in their accepted lexical-priority order; any guard runs only after its pattern
matches, and only the selected arm body runs. Unselected guards and bodies have
no operation, effect, or completion event. This control graph is distinct from
ordinary sibling-operand ordering.

---

## 12. Control Flow Statements

```ebnf
break_stmt         = BREAK_TOKEN ;      (* ^ or ^^ or ^^^ etc.; always nearest loop *)
continue_stmt      = CONTINUE_TOKEN ;   (* >> or >>> or >>>> etc.; count ignored *)

(* Settled decision, not an open question: break and continue are single-level
   only. Token length never encodes nesting depth, and multi-level jump
   spellings are permanently rejected (goto-hell prevention). *)
```

---

## Appendix: Disambiguation Rules for the LL(n) Parser

### Rule 1: `>>` as Pipe vs. Continue

When the parser encounters `>>` (or a longer contiguous run such as `>>>`, `>>>>`, etc.):
- If the two-character `>>` spelling is preceded by an expression and followed by `@` resource atom: **Resource-write shorthand**
- If the two-character `>>` spelling is preceded by an expression and followed by `#(`, `{`, or an identifier: **Pipe operator**. The left side is an iterable or pulse source; the operator pushes each item as a pulse into the right-side channel/consumer.
- If the token is the only non-trivia item in the statement and is followed by newline, `;`, `}`, or EOF: **Continue statement**. Longer spellings have the same meaning as `>>`; they do not encode nesting depth.
- Inside `[` brackets, `>>` has no meaning: the old stride selector mode (`[>>, 2]`) is removed from the design and must not be reintroduced.
- The multi-role service of `>>` (pipe / iterate, resource-write shorthand, standalone continue) is a settled design requirement, not an open question. Disambiguation stays compiler-owned context logic, and the roles will not be split across different symbols.

### Rule 2: `@` Disambiguation

- `@` alone as a source expression: **parse error**. Optional empty is `(?)`
  under static type `? | T`; a resource/intrinsic result that may be absent must
  expose that Optional type rather than producing a hidden `@` value.
- `@` followed by `[` : **retired state-resource state-container family; parse error**
- `@` followed by identifier then `:`: **resource topology resource declaration**
- `@` followed by identifier then `(`: **Resource with explicit protocol**
- `@` followed by identifier then `{`: **Invalid for explicit resources; use `@name(...)`**
- `@{` or `@(`: **Anonymous resource**; `@()` is the empty resource / destroy sink
- `@` followed by `stdout`, `stderr`, or `stdin` (bare identifier, no `{}`/`()`): **Standard stream resource** — the lexer produces `TOK_AT` + `NAME("stdout"|"stderr"|"stdin")`, and the parser resolves it directly to a standard-stream resource atom.

### Rule 3: `$` Disambiguation

- `$` followed by identifier: **Retired state-resource state reference; parse error**
- `$` followed by `(`: **Capture list** (only valid in function declaration context)
- `$` followed by string literal: **Format string**

### Rule 4: `<~` / `~>` vs. `<` / `~` / `>`

The lexer always prefers the two-character compound token over individual characters (maximal munch). `<~` is always tokenized as a single `TOK_WAVE_LEFT`, and `~>` is always tokenized as `TOK_WAVE_RIGHT`. Both are reserved symbols only: they participate in no syntax feature, and no grammar production may consume them, until the language design explicitly declares an activation. The parser rejects them with a reserved-symbol diagnostic.

### Rule 5: Break Token Contiguity

`^^` followed by whitespace then `^^` produces **two separate** break tokens — which is semantically illegal. The parser must reject consecutive break tokens in the same statement.

---

## Appendix B: resource topology — Resource declarations

**Full narrative:** [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md).

This appendix records the topology grammar surface that is now folded into the main EBNF above.
The resource-topology design document owns semantic details such as capability inference,
pending writes, resource block snapshots, consuming methods, and commit boundaries.

### B.1 Program and top-level resource

```ebnf
program_topology         = { top_level_decl_topology } EOF ;

top_level_decl_topology  = resource_decl_topology
                   | (* existing: function, schema, stmt … *) ;

resource_decl_topology   = "@" identifier ":" type_topology
                     { "," "@" identifier ":" type_topology }
                     [ ":=" driver_block_topology ] ;

(* Scope rule: resource_decl_topology is top-level only. A declaration inside
   any local block — including inside a |?| resource session — is rejected with
   "The global resource cannot be initialized in a local block." Resource
   sessions (|?| { ... }, Resource Topology §4.2) authorize handles and anchors
   only, not local topology nodes. Scoped subtopology remains a separate
   fail-closed reserve. Resources as first-class dynamic values are
   permanently rejected. *)

driver_block_topology    = "{" stream_topology "}" ;
(* stream_topology matches existing pipe: expr ">>" "#(" id ")" "=>" block *)
(* resource >> block forms enter a snapshot at >> and commit that snapshot at block exit. *)
(* Chained block stages, for example a => { ... } => { ... }, repeat that snapshot/commit rule once per block stage. *)
```

### B.2 Types (extensions)

```ebnf
type_topology            = type_primary { type_suffix } ;

type_primary       = scalar_type
                   | identifier "[" type_arg_list "]"
                   | "(" type_topology "," type_topology { "," type_topology } ")" ;

type_arg_list      = type_topology { "," type_topology } ;

type_suffix        = "|" expression "|"            (* T|n| exact length *)
                   | "|" dot_run expression "|"    (* T|..n| recent n *)
                   | dot_run ;                     (* T.. / T... infinite repetition *)

dot_run            = ".." { "." } ;                (* two or more dots normalize *)

scalar_type        = "f64" | "i64" | "bool" | "char" | "string" ;
```

### B.3 Type rewrite rules

```ebnf
type_rewrite_topology    = type_placeholder ":" type_topology ":=" type_topology ;
type_placeholder   = "__" { "_" } ;
```

Examples:

```styio
__ : list[T] := T..
__ : string := char..
__ : dict[K, V] := (K, V)..
```

### B.4 Resource write vs assignment (strict topology mode)

```ebnf
resource_write_topology  = expression "->" "@" identifier ;
assignment_topology      = identifier "=" expression ;   (* locals only *)
```

Semantic check: a topology sink write must use `expr -> @name`. A bare `@name` expression is
the resource object itself; latest-value reads must use a selector such as `@name[-1]`.

### B.5 Lexer additions

- **Target:** dot runs of length >= 2 normalize in range, selector, and type-repetition contexts: `..`, `...`, and longer runs are equivalent separators.
