# Styio Formal Grammar (EBNF)

**Purpose:** Define the composed lexical and grammar EBNF for Styio; feature-specific decisions and lifecycle state live in the distributed [syntax feature SSOT collection](./syntax/features/README.md), resource-topology narrative lives in [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md), and shared semantic principles live in [`Styio-Language-Design.md`](./Styio-Language-Design.md).

**Last updated:** 2026-07-31

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

### 2.0 Keyword-Free Token Contract

Styio defines no keyword token kind. Every word-shaped token is emitted as
`NAME`; primitive and user type names use that same token kind.

A quoted alphabetic spelling in this document is therefore an exact-spelling
predicate over `NAME`, not a reserved lexer terminal. Such a predicate is
allowed only after a leading symbol and structural position have already opened
the containing grammar family. For example, `@` followed by `NAME("import")`
may open a top-level import declaration, but `import` remains an ordinary
identifier elsewhere. `true` and `false` are expression-context literal
spellings recognized from `NAME`, not keyword tokens.

No production may introduce a fixed word as an unanchored declaration,
control-flow, or operator head. This lexical contract is owned semantically by
[Styio-Language-Design.md](./Styio-Language-Design.md) section 1.1.1.

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

### 2.3 Core Compound Symbols

These are ordered by **priority** for maximal-munch disambiguation.

```ebnf
(* Resource / State *)
TOK_AT             = '@' ;

(* State reference *)
TOK_DOLLAR         = '$' ;

(* Arrows and redirections *)
TOK_ARROW_RIGHT    = '->' ;
TOK_ARROW_LEFT     = '<-' ;

(* Reserved wave tokens: tokenized for future use, rejected by parser today. *)
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
TOK_AWAIT_PIPE     = '?|' ;

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
TOK_PIPE_SINGLE    = '|' ;
TOK_CARET          = '^' ;
TOK_TILDE          = '~' ;
TOK_QUESTION       = '?' ;
TOK_DBQUESTION     = '??' ;
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
                   | assignment
                   | conditional_stmt
                   | await_stmt
                   | resource_effect_discard_stmt
                   | task_group_launch
                   | resource_order_stmt
                   | match_bind_expr
                   | flow_pipeline
                   | expression_stmt ;
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

1. `@import` is only valid at file top level. `import` is lexed as `NAME`;
   its spelling is inspected only after top-level `@` has opened this
   declaration family.
2. `/` is the native package/module path spelling.
3. `.` is accepted compatibility syntax and is normalized to slash form internally.
4. One import item must not mix `/` and `.`.
5. Import syntax carries no inline signature or generic body. Executable
   callable imports resolve through compiler-produced sibling `.styioi`
   metadata; schema, source, dependency, body, and ABI drift fail before
   lowering.
6. Only exported callables of a direct import enter the importing source's
   namespace. Private helper facts remain available only while checking or
   specializing their owning imported body.
7. Cross-module dependency cycles are rejected in the current
   separate-compilation slice. This is a semantic module-graph rule and adds no
   recursive-import grammar production.

### 4.2 Callable / Operation-Channel Binding

```ebnf
declaration        = callable_decl ;

callable_decl      = '#' identifier callable_decl_tail ;

callable_decl_tail = [ ':' type_annotation ] [ capture_list ]
                     ( bind_op callable_body
                     | '=>' callable_tail )
                   | '(' [ param_list ] ')' [ ':' type_annotation ] [ capture_list ]
                     ( bind_op callable_tail
                     | '=>' callable_tail
                     | '?=' match_body ) ;

bind_op            = '=' | ':=' ;

callable_body      = [ '#' ] function_body ;

callable_tail      = block | expression ;

capture_list       = '$' '(' identifier { ',' identifier } ')' ;

function_body      = '(' [ param_list ] ')' [ '=>' ] callable_tail ;

param_list         = param { ',' param } ;
param              = identifier [ ':' type_annotation ] ;

type_annotation    = type_expr ;

type_expr          = type_primary { type_suffix } ;

type_primary       = scalar_type
                   | identifier [ '[' type_arg_list ']' ]
                   | callable_type
                   | tuple_type ;

scalar_type        = 'i8' | 'i16' | 'i32' | 'i64' | 'i128'
                   | 'f32' | 'f64'
                   | 'bool' | 'char' | 'string' | 'byte'
                   | 'matrix' ;

type_arg_list      = type_expr { ',' type_expr } ;

callable_type      = '#' '(' [ type_expr { ',' type_expr } ] ')' ':' type_expr ;

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
5. The brackets in `type_primary` are type application in type position, as in
   `list[i64]`; they do not create callable generic parameters.
6. `callable_decl` deliberately has no production between `identifier` and
   `callable_decl_tail` for a generic binder. Therefore `# name[T] ...` is a
   syntax error.
7. An eligible final, non-recursive callable receives one compiler-inferred
   principal rank-1 type relation at its definition site. Compiler-generated
   type-variable names may be published in module-interface metadata but are
   not source tokens.
8. A source annotation such as `: T` refers to an existing type named `T`; it
   never declares an implicit generic variable. Concrete parameter and result
   annotations remain valid for monomorphic contracts.
9. A recursive call-graph component is inferred as one group. Each member has
   one provisional monotype, and every reference from inside the group reuses
   that member's same provisional type variables.
10. After a recursive group has one stable solution, each eligible final
    binding may be generalized and its principal rank-1 relation published.
    This permits ordinary inferred generic recursion.
11. An internal recursive edge that requires the same member at a different
    instantiation is polymorphic recursion and is rejected. Diagnostics report
    the conflicting instantiations instead of requesting `[T]`.
12. Callable invocation has no explicit type-argument production. Instances are
    selected only by ordinary arguments and the concrete expected type supplied
    by the surrounding context.
13. In value position, `name[T](...)` remains an ordinary `selector` followed by
    `call`; it is never reinterpreted as callable specialization. It is rejected
    when the selected target is not indexable or the selector expression is
    otherwise invalid.
14. An inferred callable scheme is consumed by a direct named `call` or by a
    bare final noncapturing callable item under one complete concrete
    `callable_type` context. For example,
    `operation: #(i64): i64 := identity` freezes one `i64 -> i64` item.
    A bare scheme name without that context remains a semantic error.
15. The type variables in an inferred scheme range only over immutable scalar
    values and recursively plain materialized `list`/`dict` types. Resource,
    stream, file, task, matrix, topology-resource, and other
    capability-sensitive handle types remain concrete monomorphic contracts;
    no new source capability or lifetime binder is introduced.
16. Concrete callable instances are compiler-owned mono items reached from
    ordinary direct calls or contextual callable-item coercion. Their
    deterministic content identities include the concrete relation, effects,
    checked body, transitive dependencies, target, and ABI facts. There is no
    explicit-instantiation production or source spelling; recursive or
    pathological instance growth fails with a concrete instance-path
    diagnostic.
17. `callable_type` is invariant and admits neither topology suffixes nor
    implicit arity, parameter, result, variance, or numeric-signature
    adaptation. Because the result is a complete `type_expr`, a trailing suffix
    in `#(i64): i64..` belongs to the result type; it does not turn the callable
    value into a topology resource. Runtime callable slots use final `:=`
    bindings only. Capturing closures, callable address equality, and
    generalized callable storage are outside this production.

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

---

## 5. Assignments

```ebnf
assignment         = identifier assign_op expression ;

assign_op          = '=' | '+=' | '-=' | '*=' | '/=' | ':=' ;
```

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

block              = '{' { statement [ statement_sep ] } [ yield_expr [ statement_sep ] ] '}' ;

yield_expr         = '<|' expression
                   | '|<|' expression ;
```

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
                     '&' [ '[' expression ']' ]
                     stream_source '>>' closure_sig
                     '=>' block ;
```

### 7.2 Snapshot Declaration

```ebnf
snapshot_decl      = '@' '[' identifier ']' '<<' resource ;
```

### 7.3 Immediate Pull

```ebnf
instant_pull       = '(' '<-' resource ')' ;

legacy_instant_pull = '(' '<<' resource ')' ;  (* compatibility only; do not use in new design text *)
```

### 7.4 Tasks and Await

```ebnf
task_expr          = '||>' block ;

task_group_launch  = '||>' '[' task_group_entry { statement_sep task_group_entry } ']' ;

task_group_entry   = identifier ( ':=' | '=' ) block ;

await_stmt         = '?|' [ expression ] '->' identifier ':' type_annotation
                     [ '|' expression ] ;

resource_order_stmt = identifier '=>' identifier ;
```

`await_stmt` without a source (`?| -> name: T`) is reserved for bare continuation
freeze. The parser accepts it, but current semantic analysis rejects it until
continuation lowering can enforce one-shot resume/discontinue.

`await_stmt` with a source settles the task/future pull at the current source
site. Without `| fallback`, pull failure raises immediately. With `| fallback`,
the declared value type and fallback value type must unify.

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
| 11 | `<\|` | Left |
| 12 | `\|` (value fallback / guard else) | Left |
| 13 | `??` (diagnostic) | Left |
| 14 (lowest) | `=`, `+=`, etc. | Right |

### 8.2 Expression Grammar

```ebnf
expression         = resource_effect_expr
                   | range_expr
                   | apply_expr ;

range_expr         = apply_expr dot_run apply_expr ;

resource_effect_expr
                   = '?|' resource_operation { '|' resource_effect_handler } ;

resource_effect_discard_stmt
                   = '?|' resource_operation { '|' effect_handler_clause } '|' '...' ;

resource_operation = range_expr
                   | apply_expr ;

resource_effect_handler
                   = effect_handler_clause
                   | expression ;

effect_handler_clause
                   = identifier '=>' ( block | expression ) ;

apply_expr         = conditional_value_expr { '<|' conditional_value_expr } ;  (* left associative; one-shot continuation resume when lhs is captured *)

conditional_value_expr
                   = guard '=>' logic_or_expr '|' logic_or_expr
                   | logic_or_expr ;

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
                   | [ selector_mode ',' ] expression_list ;

selector_mode      = 'avg' | 'max' | 'min' | 'std' | 'rsi'
                   | identifier ;

(* Retired selector families are parser errors outside registered negative tests. *)

call               = '(' [ expression_list ] ')' ;

member_access      = '.' identifier ;

expression_list    = expression { ',' expression } ;
```

The `selector` followed by `call` sequence does not create callable type
arguments. For example, `identity[i64](1)` follows ordinary value-position
postfix parsing and is never a generic instantiation. A generic callable use is
instantiated from call arguments and its concrete expected result context; an
underconstrained call is rejected instead of accepting authored type arguments.

`range_expr` is the naked expression-level range form, such as `start..end`.
It is not a list literal. Step range spelling such as `start..end..step` is
reserved, not active syntax, and not canonical.

`resource_effect_expr` is the only resource fallback surface. `?| op` settles
the resource operation in place and raises a structured error immediately if it
fails. `?| op | fallback` evaluates `fallback` only for resource-effect failure,
then type-checks the operation success value and fallback value against the same
use-site type. `?| op | effect => handler` handles only the named typed effect
family and must still type-check against the same use-site type. Multiple named
handlers may be chained; duplicate handlers are rejected, and any catch-all
fallback must be last. A bare `op | fallback` is not resource fallback.
Statement-shaped resource operations that become `?|`-eligible must route
through the same settlement contract rather than adding a trailing bare
`| fallback`. `?=` does not catch resource effects; it only matches ordinary
values that have already been materialized. `?| op | ...` is a separate
statement-only discard form: it settles the operation, discards business recovery
for effects at that site, produces no value, and continues with the next
statement. It is rejected in expression contexts such as assignment, call
arguments, and branch values. A bare `@()` handler is also rejected because `@()`
is the empty resource / destroy sink, not an executable empty action.

### 8.3 Primary Expressions

```ebnf
primary_expr       = identifier
                   | int_literal
                   | float_literal
                   | string_literal
                   | char_literal
                   | 'true' | 'false'
                   | resource
                   | collection
                   | instant_pull
                   | legacy_instant_pull
                   | '(' expression ')'
                   | '?' '(' expression ')'        (* guard condition prefix; followed by => for value selection *)
                   | block ;
```

`true` and `false` are tokenized as `NAME` and interpreted as Boolean literal
spellings only in expression context. They do not create keyword tokens.

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

reserved_step_range = expression dot_run expression dot_run expression ;
                   (* reserved / not active / not canonical *)
```

`[start..end]` is a materialized range. It materializes the expression-level
range `start..end` as an iterable `list[i64]` source, and
`[start..end] >> #(x) => { ... }` pushes each materialized element into the
consumer one at a time. The parser must not interpret `[start..end]` as a
`list_literal` containing one naked `range_expr`; there is no comma-separated
list element in that form. `[start..end..step]` and equivalent step spellings
remain reserved and are rejected by the active parser.

`matrix` annotations reuse nested `list_literal` syntax as their source form. A binding such as `m: matrix = [[1,0],[0,1]]` triggers rectangular numeric row validation in the typed context and lowers to a matrix handle; untyped nested list literals remain ordinary lists. Matrix operations such as `matmul(a,b)`, `transpose(m)`, `mat_shape(m)`, and `mat_set(m,r,c,v)` are ordinary identifier calls at the grammar level and are recognized by semantic analysis.

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

---

## 12. Control Flow Statements

```ebnf
break_stmt         = BREAK_TOKEN ;      (* ^ or ^^ or ^^^ etc.; always nearest loop *)
continue_stmt      = CONTINUE_TOKEN ;   (* >> or >>> or >>>> etc.; count ignored *)
```

---

## Appendix: Disambiguation Rules for the LL(n) Parser

### Rule 1: `>>` as Pipe vs. Continue

When the parser encounters `>>` (or a longer contiguous run such as `>>>`, `>>>>`, etc.):
- If the two-character `>>` spelling is preceded by an expression and followed by `@` resource atom: **Resource-write shorthand**
- If the two-character `>>` spelling is preceded by an expression and followed by `#(`, `{`, or an identifier: **Pipe operator**. The left side is an iterable or pulse source; the operator pushes each item as a pulse into the right-side channel/consumer.
- If the token is the only non-trivia item in the statement and is followed by newline, `;`, `}`, or EOF: **Continue statement**. Longer spellings have the same meaning as `>>`; they do not encode nesting depth.
- If inside `[` brackets: **Stride selector mode**

### Rule 2: `@` Disambiguation

- `@` alone as a source expression: **parse error**. Use resource/intrinsic-produced absence; active feature fixtures must not author bare `@` directly.
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

The lexer always prefers the two-character compound token over individual characters (maximal munch). `<~` is always tokenized as a single `TOK_WAVE_LEFT`, and `~>` is always tokenized as `TOK_WAVE_RIGHT`. Both tokens are reserved and have no active grammar production; the parser rejects them with a reserved-symbol diagnostic.

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
                   | (* existing callable and statement forms *) ;

resource_decl_topology   = "@" identifier ":" type_topology
                     { "," "@" identifier ":" type_topology }
                     [ ":=" driver_block_topology ] ;

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
