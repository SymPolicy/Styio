# Styio Formal Grammar (EBNF)

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

(* Wave operators *)
TOK_WAVE_LEFT      = '<~' ;
TOK_WAVE_RIGHT     = '~>' ;

(* Match and probe *)
TOK_MATCH          = '?=' ;

(* Yield / Return *)
TOK_YIELD          = '<|' ;

(* Pipe *)
TOK_PIPE           = '>>' ;

(* IO buffer *)
TOK_IO_BUF         = '>_' ;

(* Binding *)
TOK_BIND           = ':=' ;

(* Shift-back (history probe prefix inside [...]) *)
TOK_SHIFT_BACK     = '<<' ;

(* Hash (function definition prefix) *)
TOK_HASH           = '#' ;

(* Ellipsis (infinite generator) *)
TOK_ELLIPSIS       = '...' ;

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

These tokens are measured by counting contiguous repetitions:

```ebnf
BREAK_TOKEN        = '^' { '^' } ;           (* length >= 1, contiguous *)
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
program            = { statement } EOF ;

statement          = declaration
                   | assignment
                   | state_declaration
                   | flow_pipeline
                   | expression_stmt
                   | schema_def ;
```

---

## 4. Declarations

### 4.1 Function Declaration

```ebnf
declaration        = '#' identifier
                     [ ':' type_annotation ]
                     [ capture_list ]
                     ':=' function_body ;

capture_list       = '$' '(' identifier { ',' identifier } ')' ;

function_body      = '(' [ param_list ] ')' [ '=>' ] ( block | expression ) ;

param_list         = param { ',' param } ;
param              = identifier [ ':' type_annotation ] ;

type_annotation    = 'i8' | 'i16' | 'i32' | 'i64' | 'i128'
                   | 'f32' | 'f64'
                   | 'bool' | 'char' | 'string' | 'byte'
                   | identifier ;
```

### 4.2 Schema Declaration

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

## 6. State Declarations

```ebnf
state_declaration  = '@' '[' state_param ']' '(' assignment ')' ;

state_param        = integer                          (* window buffer: @[5] *)
                   | identifier '=' expression ;      (* scalar accumulator: @[total = 0] *)
```

---

## 7. Flow Pipelines

```ebnf
flow_pipeline      = stream_source [ guard ] '>>' consumer ;

stream_source      = infinite_gen
                   | collection
                   | resource
                   | identifier ;

infinite_gen       = '[' '...' ']' ;

guard              = '?' '(' expression ')' ;

consumer           = [ closure_sig ] '=>' ( block | expression )
                   | identifier ;

closure_sig        = '#' '(' [ param_list ] ')' ;

block              = '{' { statement } [ yield_expr ] '}' ;

yield_expr         = '<|' expression ;
```

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

### 7.3 Instant Pull

```ebnf
instant_pull       = '(' '<<' resource ')' ;
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
| 11 | `<~`, `~>` | Right |
| 12 | `<\|` | Right |
| 13 | `\|` (fallback) | Left |
| 14 | `??` (diagnostic) | Left |
| 15 (lowest) | `=`, `+=`, etc. | Right |

### 8.2 Expression Grammar

```ebnf
expression         = wave_expr ;

wave_expr          = logic_or_expr [ ( '<~' | '~>' ) logic_or_expr '|' logic_or_expr ] ;

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

selector           = '[' [ selector_mode ',' ] expression_list ']' ;
selector_mode      = '?' | '?=' | '<<' | 'avg' | 'max' | 'min' | 'std' | 'rsi'
                   | identifier ;

call               = '(' [ expression_list ] ')' ;

member_access      = '.' identifier ;

expression_list    = expression { ',' expression } ;
```

### 8.3 Primary Expressions

```ebnf
primary_expr       = identifier
                   | state_ref
                   | int_literal
                   | float_literal
                   | string_literal
                   | 'true' | 'false'
                   | '@'                           (* Undefined *)
                   | resource
                   | collection
                   | instant_pull
                   | '(' expression ')'
                   | block ;

state_ref          = '$' identifier [ selector ] ;
```

---

## 9. Resources

```ebnf
resource           = '@' [ identifier ] ( '{' expression '}' | '(' expression ')' ) ;
```

Examples:
- `@("localhost:8080")` — auto-detect
- `@file{"readme.txt"}` — explicit file
- `@binance{"BTCUSDT"}` — exchange feed
- `@mysql{"localhost:3306"}` — database

---

## 10. Collections

```ebnf
collection         = list_literal | tuple_literal | range_literal ;

list_literal       = '[' [ expression { ',' expression } ] ']' ;

tuple_literal      = '(' expression ',' expression { ',' expression } ')' ;

range_literal      = expression '..' expression [ '..' expression ] ;
```

---

## 11. Pattern Matching

```ebnf
match_expr         = expression '?=' match_body ;

match_body         = '{' { match_arm } [ default_arm ] '}' ;

match_arm          = pattern '=>' ( block | expression ) ;

default_arm        = '_' '=>' ( block | expression ) ;

pattern            = int_literal
                   | float_literal
                   | string_literal
                   | identifier
                   | collection_pattern ;

collection_pattern = '[' { pattern { ',' pattern } } ']'
                   | '(' { pattern { ',' pattern } } ')' ;
```

---

## 12. Control Flow Statements

```ebnf
break_stmt         = BREAK_TOKEN ;      (* ^ or ^^ or ^^^ etc. *)
continue_stmt      = CONTINUE_TOKEN ;   (* >> or >>> or >>>> etc. *)
```

---

## Appendix: Disambiguation Rules for the LL(n) Parser

### Rule 1: `>>` as Pipe vs. Continue

When the parser encounters `>>` (or longer `>>>`, `>>>>`, etc.):
- If preceded by an expression and followed by `#(`, `{`, or an identifier: **Pipe operator**
- If followed by newline, `;`, `}`, or another control token: **Continue statement**
- If inside `[` brackets: **Stride selector mode**

### Rule 2: `@` Disambiguation

- `@` alone (not followed by `[`, identifier, `{`, `(`): **Undefined value**
- `@[`: **State container declaration**
- `@` followed by identifier then `{` or `(`: **Resource with protocol**
- `@{` or `@(`: **Anonymous resource**

### Rule 3: `$` Disambiguation

- `$` followed by identifier: **State reference**
- `$` followed by `(`: **Capture list** (only valid in function declaration context)
- `$` followed by string literal: **Format string**

### Rule 4: `<~` / `~>` vs. `<` / `~` / `>`

The lexer always prefers the two-character compound token over individual characters (maximal munch). `<~` is always tokenized as a single `TOK_WAVE_LEFT`.

### Rule 5: Break Token Contiguity

`^^` followed by whitespace then `^^` produces **two separate** break tokens — which is semantically illegal. The parser must reject consecutive break tokens in the same statement.
