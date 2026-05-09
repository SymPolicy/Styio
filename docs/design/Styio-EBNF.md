# Styio Formal Grammar (EBNF)

**Purpose:** 词法与语法的 **EBNF 权威定义**；资源拓扑相关附录与叙述以 [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md) 为准，语义细节以 [`Styio-Language-Design.md`](./Styio-Language-Design.md) 为准。

**Last updated:** 2026-05-09

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
escape_seq     = '\' ( 'n' | 't' | 'r' | '\' | '"' | '0' ) ;
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

(* Hash (function definition prefix) *)
TOK_HASH           = '#' ;

(* Dot run: two or more dots. '..', '...', and longer runs are normalized
   by context as range separators or type repetition suffixes. *)
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

These tokens use contiguous repetitions. Break keeps the spelling flexible but does
not assign semantic depth to the count.

```ebnf
BREAK_TOKEN        = '^' { '^' } ;           (* length >= 1, contiguous, depth = 1 *)
CONTINUE_TOKEN     = '>' '>' { '>' } ;       (* length >= 2, contiguous, standalone context *)
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

### 4.2 Function Declaration

```ebnf
declaration        = '#' identifier
                     [ ':' type_annotation ]
                     [ capture_list ]
                     ':=' function_body ;

capture_list       = '$' '(' identifier { ',' identifier } ')' ;

function_body      = '(' [ param_list ] ')' [ '=>' ] ( block | expression ) ;

param_list         = param { ',' param } ;
param              = identifier [ ':' type_annotation ] ;

type_annotation    = type_expr ;

type_expr          = type_primary { type_suffix } ;

type_primary       = scalar_type
                   | identifier [ '[' type_arg_list ']' ]
                   | tuple_type ;

scalar_type        = 'i8' | 'i16' | 'i32' | 'i64' | 'i128'
                   | 'f32' | 'f64'
                   | 'bool' | 'char' | 'string' | 'byte'
                   | 'matrix' ;

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

## 5. Assignments

```ebnf
assignment         = identifier assign_op expression ;

assign_op          = '=' | '+=' | '-=' | '*=' | '/=' | ':=' ;
```

---

## 6. Retired State Declarations (M6)

Retired M6 state containers and state references are not active grammar productions. The parser
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
| 12 | `\|` (fallback) | Left |
| 13 | `??` (diagnostic) | Left |
| 14 (lowest) | `=`, `+=`, etc. | Right |

### 8.2 Expression Grammar

```ebnf
expression         = apply_expr ;

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

### 8.3 Primary Expressions

```ebnf
primary_expr       = identifier
                   | int_literal
                   | float_literal
                   | string_literal
                   | 'true' | 'false'
                   | resource
                   | collection
                   | instant_pull
                   | legacy_instant_pull
                   | '(' expression ')'
                   | '?' '(' expression ')'        (* guard condition prefix; followed by => for value selection *)
                   | block ;
```

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
- `iterable_expr '>>' '@stdout'` / `iterable_expr '>>' '@stderr'` — standard-stream iterable write via `resource_write`
- `iterable_expr '>>' terminal_handle` — terminal-handle resource-write shorthand; semantic checks require an iterable, text-serializable value
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
collection         = list_literal | tuple_literal | range_literal ;

list_literal       = '[' [ expression { ',' expression } ] ']' ;

tuple_literal      = '(' expression ',' expression { ',' expression } ')' ;

range_literal      = expression '..' expression [ '..' expression ] ;
```

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
continue_stmt      = CONTINUE_TOKEN ;   (* >> or >>> or >>>> etc. *)
```

---

## Appendix: Disambiguation Rules for the LL(n) Parser

### Rule 1: `>>` as Pipe vs. Continue

When the parser encounters `>>` (or longer `>>>`, `>>>>`, etc.):
- If preceded by an expression and followed by `@` resource atom: **Resource-write shorthand**
- If preceded by an expression and followed by `#(`, `{`, or an identifier: **Pipe operator**
- If followed by newline, `;`, `}`, or another control token: **Continue statement**
- If inside `[` brackets: **Stride selector mode**

### Rule 2: `@` Disambiguation

- `@` alone as a source expression: **parse error**. Use resource/intrinsic-produced absence; active milestone fixtures must not author bare `@` directly.
- `@` followed by `[` : **retired M6 state-container family; parse error**
- `@` followed by identifier then `:`: **Topology v2 resource declaration**
- `@` followed by identifier then `(`: **Resource with explicit protocol**
- `@` followed by identifier then `{`: **Invalid for explicit resources; use `@name(...)`**
- `@{` or `@(`: **Anonymous resource**; `@()` is the empty resource / destroy sink
- `@` followed by `stdout`, `stderr`, or `stdin` (bare identifier, no `{}`/`()`): **Standard stream resource** — the lexer produces `TOK_AT` + `NAME("stdout"|"stderr"|"stdin")`, and the parser resolves it directly to a standard-stream resource atom.

### Rule 3: `$` Disambiguation

- `$` followed by identifier: **Retired M6 state reference; parse error**
- `$` followed by `(`: **Capture list** (only valid in function declaration context)
- `$` followed by string literal: **Format string**

### Rule 4: `<~` / `~>` vs. `<` / `~` / `>`

The lexer always prefers the two-character compound token over individual characters (maximal munch). `<~` is always tokenized as a single `TOK_WAVE_LEFT`, and `~>` is always tokenized as `TOK_WAVE_RIGHT`. Both tokens are reserved and have no active grammar production; the parser rejects them with a reserved-symbol diagnostic.

### Rule 5: Break Token Contiguity

`^^` followed by whitespace then `^^` produces **two separate** break tokens — which is semantically illegal. The parser must reject consecutive break tokens in the same statement.

---

## Appendix B: Topology v2 — Resource declarations

**Full narrative:** [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md).

This appendix records the topology grammar surface that is now folded into the main EBNF above.
The resource-topology design document owns semantic details such as capability inference,
pending writes, move-only checks, consuming methods, and commit boundaries.

### B.1 Program and top-level resource

```ebnf
program_v2         = { top_level_decl_v2 } EOF ;

top_level_decl_v2  = resource_decl_v2
                   | (* existing: function, schema, stmt … *) ;

resource_decl_v2   = "@" identifier ":" type_v2
                     { "," "@" identifier ":" type_v2 }
                     [ ":=" driver_block_v2 ] ;

driver_block_v2    = "{" stream_topology "}" ;
(* stream_topology matches existing pipe: expr ">>" "#(" id ")" "=>" block *)
```

### B.2 Types (extensions)

```ebnf
type_v2            = type_primary { type_suffix } ;

type_primary       = scalar_type
                   | identifier "[" type_arg_list "]"
                   | "(" type_v2 "," type_v2 { "," type_v2 } ")" ;

type_arg_list      = type_v2 { "," type_v2 } ;

type_suffix        = "|" expression "|"            (* T|n| exact length *)
                   | "|" dot_run expression "|"    (* T|..n| recent n *)
                   | dot_run ;                     (* T.. / T... infinite repetition *)

dot_run            = ".." { "." } ;                (* two or more dots normalize *)

scalar_type        = "f64" | "i64" | "bool" | "char" | "string" ;
```

### B.3 Type rewrite rules

```ebnf
type_rewrite_v2    = type_placeholder ":" type_v2 ":=" type_v2 ;
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
resource_write_v2  = expression "->" "@" identifier ;
assignment_v2      = identifier "=" expression ;   (* locals only *)
```

Semantic check: a topology sink write must use `expr -> @name`. A bare `@name` expression is
the resource object itself; latest-value reads must use a selector such as `@name[-1]`.

### B.5 Lexer additions

- **Target:** dot runs of length >= 2 normalize in range, selector, and type-repetition contexts: `..`, `...`, and longer runs are equivalent separators.
