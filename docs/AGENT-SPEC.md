# Styio — Agent Development Specification

**Version:** 1.0  
**Date:** 2026-03-28  
**Authority:** This document governs all AI agents and human contributors working on the Styio compiler. Agents MUST read this file before making any changes.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Repository Layout](#2-repository-layout)
3. [Architecture & Compilation Pipeline](#3-architecture--compilation-pipeline)
4. [Coding Standards](#4-coding-standards)
5. [How to Add a New Token](#5-how-to-add-a-new-token)
6. [How to Add a New AST Node](#6-how-to-add-a-new-ast-node)
7. [How to Extend the Parser](#7-how-to-extend-the-parser)
8. [How to Add LLVM CodeGen for a Node](#8-how-to-add-llvm-codegen-for-a-node)
9. [How to Add a New Compiler Intrinsic](#9-how-to-add-a-new-compiler-intrinsic)
10. [Testing Requirements](#10-testing-requirements)
11. [Build System](#11-build-system)
12. [Documentation Rules](#12-documentation-rules)
13. [Design Reference Documents](#13-design-reference-documents)
14. [Prohibited Actions](#14-prohibited-actions)
15. [Decision Authority](#15-decision-authority)

---

## 1. Project Overview

Styio is a **symbol-driven, intent-aware stream processing language** targeting financial quantitative analysis as its first domain. The compiler is written in **C++20** and emits **LLVM IR** for native execution via ORC JIT.

### Key Facts

| Property | Value |
|----------|-------|
| Language | C++20 |
| Backend | LLVM 18.1.0+ |
| Parser | Hand-written recursive descent, LL(n) |
| JIT | LLVM ORC |
| Unicode | ICU (uc, i18n) |
| Build | CMake |
| Formatting | `.clang-format` (Google-based, 2-space indent) |
| Test Framework | GoogleTest + `.styio` fixture files |

### Core Design Documents

Before making any language-level change, agents MUST read:

- `docs/Styio-Language-Design.md` — Full language specification
- `docs/Styio-EBNF.md` — Formal grammar
- `docs/Styio-Symbol-Reference.md` — Symbol lookup table
- `docs/Styio-StdLib-Intrinsics.md` — Standard library algorithms
- `docs/Styio-Resource-Driver.md` — Resource driver interface
- `docs/Styio-Research-Innovations.md` — Research novelty points (do not break these)

---

## 2. Repository Layout

```
Styio/
├── CMakeLists.txt              # Top-level build configuration
├── .clang-format               # C++ formatting rules (MUST use)
├── .gitignore
├── README.md
├── extend_tests.py             # Test scaffolding script
│
├── docs/                       # Language design & specs (Markdown)
│   ├── AGENT-SPEC.md           # THIS FILE
│   ├── Styio-Language-Design.md
│   ├── Styio-EBNF.md
│   ├── Styio-Symbol-Reference.md
│   ├── Styio-StdLib-Intrinsics.md
│   ├── Styio-Resource-Driver.md
│   ├── Styio-Research-Innovations.md
│   └── idea.md                 # Early design sketches
│
├── src/                        # Compiler source code
│   ├── main.cpp                # Entry point: CLI → Lex → Parse → Analyze → CodeGen → JIT
│   ├── include/
│   │   └── cxxopts.hpp         # Vendored CLI parsing library (DO NOT MODIFY)
│   │
│   ├── StyioToken/             # Token definitions and data types
│   │   ├── Token.hpp           # StyioTokenType, StyioNodeType, StyioOpType, StyioDataType
│   │   └── Token.cpp           # Type utility implementations
│   │
│   ├── StyioParser/            # Lexer and Parser
│   │   ├── Tokenizer.hpp       # StyioTokenizer class declaration
│   │   ├── Tokenizer.cpp       # Lexer implementation (tokenize)
│   │   ├── Parser.hpp          # Parser function declarations, StyioContext
│   │   ├── Parser.cpp          # Recursive descent parser (~2700 lines)
│   │   └── BinExprMapper.hpp   # Binary expression operator mapping
│   │
│   ├── StyioAST/               # Abstract Syntax Tree
│   │   ├── AST.hpp             # StyioAST base class + all concrete AST nodes
│   │   └── ASTDecl.hpp         # Forward declarations for all AST classes
│   │
│   ├── StyioAnalyzer/          # Semantic analysis
│   │   ├── ASTAnalyzer.hpp     # StyioAnalyzer visitor class
│   │   ├── TypeInfer.cpp       # Type inference visitor implementations
│   │   ├── ToStyioIR.cpp       # AST → StyioIR lowering visitor
│   │   ├── DFAImpl.hpp         # Data flow analysis helpers
│   │   └── Util.hpp            # Analyzer utilities
│   │
│   ├── StyioIR/                # Styio intermediate representation
│   │   ├── StyioIR.hpp         # IR base classes
│   │   ├── IRDecl.hpp          # IR node forward declarations
│   │   ├── GenIR/GenIR.hpp     # General IR node definitions
│   │   └── IOIR/IOIR.hpp       # I/O-specific IR nodes
│   │
│   ├── StyioCodeGen/           # LLVM IR generation
│   │   ├── CodeGenVisitor.hpp  # StyioToLLVM visitor class
│   │   ├── CodeGen.cpp         # Core codegen (main entry, expressions, control flow)
│   │   ├── CodeGenG.cpp        # General node codegen
│   │   ├── CodeGenIO.cpp       # I/O node codegen
│   │   ├── GetTypeG.cpp        # LLVM type resolution (general)
│   │   └── GetTypeIO.cpp       # LLVM type resolution (I/O)
│   │
│   ├── StyioJIT/               # JIT execution
│   │   ├── StyioJIT_ORC.hpp    # LLVM ORC JIT wrapper
│   │   └── JITExecutor.hpp     # Execution entry point
│   │
│   ├── StyioToString/          # AST pretty-printing
│   │   ├── ToStringVisitor.hpp # Variadic template visitor
│   │   └── ToString.cpp        # Concrete toString implementations
│   │
│   ├── StyioExtern/            # External symbols for JIT
│   │   ├── ExternLib.hpp
│   │   └── ExternLib.cpp
│   │
│   ├── StyioException/         # Error types
│   │   └── Exception.hpp
│   │
│   ├── StyioUtil/              # Shared utilities
│   │   └── Util.hpp
│   │
│   └── Deprecated/             # Old code kept for reference (DO NOT USE)
│       ├── Parser_Deprecated.cpp
│       ├── GetTypeImpl.cpp
│       ├── CodeGenImpl.cpp
│       └── CodeGenVisitor.hpp
│
└── tests/                      # Test suite
    ├── CMakeLists.txt           # GoogleTest setup
    ├── styio_test.cpp           # GoogleTest C++ tests
    ├── parsing/                 # .styio fixture files organized by feature
    │   ├── basic_exprs/
    │   ├── binding/
    │   ├── binop/
    │   ├── boolean/
    │   ├── brainfuck/
    │   ├── collections/
    │   ├── fmtstr/
    │   ├── forward/
    │   ├── func/
    │   ├── listop/
    │   ├── print/
    │   └── resources/
    ├── type_checking/           # Type system test fixtures
    └── tpc-h/                   # TPC-H benchmark fixtures
```

---

## 3. Architecture & Compilation Pipeline

The compiler executes the following stages **in order**:

```
Source (.styio)
    │
    ▼
┌──────────────────┐
│  1. Tokenizer    │  StyioTokenizer::tokenize()
│     (Lexer)      │  Produces: vector<StyioToken>
└──────────────────┘
    │
    ▼
┌──────────────────┐
│  2. Parser       │  parse_main_block(StyioContext&)
│     (LL(n))      │  Produces: MainBlockAST*
└──────────────────┘
    │
    ▼
┌──────────────────┐
│  3. Type Infer   │  StyioAnalyzer::typeInfer(MainBlockAST*)
│     (Sema)       │  Mutates AST data types in-place
└──────────────────┘
    │
    ▼
┌──────────────────┐
│  4. IR Lowering  │  StyioAnalyzer::toStyioIR(MainBlockAST*)
│                  │  Produces: StyioIR* tree
└──────────────────┘
    │
    ▼
┌──────────────────┐
│  5. LLVM CodeGen │  StyioToLLVM + IRBuilder
│                  │  Produces: llvm::Module
└──────────────────┘
    │
    ▼
┌──────────────────┐
│  6. JIT Execute  │  StyioJIT_ORC → look up "main" → call
│                  │  Returns: int (exit code)
└──────────────────┘
```

### Rule: Respect Stage Boundaries

Each stage has a clear input and output. Agents MUST NOT:
- Access LLVM types in the Parser
- Modify AST nodes during CodeGen
- Skip the IR lowering stage (AST → StyioIR → LLVM IR, never AST → LLVM IR directly)

---

## 4. Coding Standards

### 4.1 Formatting

**All C++ code MUST conform to `.clang-format`.**

Key rules:
- **Indent:** 2 spaces, no tabs
- **Column limit:** None (no forced line wrapping)
- **Brace style:** Custom — braces on new line for classes/enums/structs/namespaces, same line for functions
- **Pointer alignment:** Right (`int *p`, not `int* p`)
- **Return type:** Break after return type for top-level definitions

Run before committing:
```bash
clang-format -i src/**/*.cpp src/**/*.hpp
```

### 4.2 Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Classes / AST nodes | PascalCase | `NameAST`, `StyioContext`, `MainBlockAST` |
| Free functions | snake_case | `parse_main_block`, `read_styio_file` |
| Enum types | PascalCase | `StyioTokenType`, `StyioNodeType` |
| Enum values (tokens) | UPPER_SNAKE or PascalCase | `TOK_PIPE`, `TOK_WAVE_LEFT` |
| Enum values (nodes) | PascalCase or Mixed | `MainBlock`, `CondFlow_True` |
| Member variables | snake_case | `name_str`, `cur_pos`, `line_seps` |
| Constants | UPPER_SNAKE | `TOKEN_PRECEDENCE_MAP` |
| Template params | PascalCase | `Derived`, `Types` |

### 4.3 Header Guards

Use **both** `#pragma once` and a classic include guard:

```cpp
#pragma once
#ifndef STYIO_MODULENAME_H_
#define STYIO_MODULENAME_H_

// ... content ...

#endif  // STYIO_MODULENAME_H_
```

### 4.4 Include Ordering

Group includes with section comments:

```cpp
/* [C++ STL] */
#include <string>
#include <vector>

/* [Styio] */
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"

/* [LLVM] */
#include "llvm/IR/IRBuilder.h"

/* [Others] */
#include "../include/cxxopts.hpp"
```

### 4.5 Comments

- Prefer `//` for single-line comments
- Use `/* ... */` for section headers or block documentation
- **Do not write comments that merely narrate what the code does** — only explain non-obvious intent, trade-offs, or constraints

---

## 5. How to Add a New Token

When the language design introduces a new symbol (e.g., `<~`, `~>`, `??`):

### Step 1: Define Token Type

In `src/StyioToken/Token.hpp`, add to the `StyioTokenType` enum:

```cpp
TOK_WAVE_LEFT = /* next available value */,   // <~
TOK_WAVE_RIGHT = /* next available value */,  // ~>
TOK_DBQUESTION = /* next available value */,  // ??
```

### Step 2: Add to Token Map

If the token is a multi-character compound, add it to `StyioTokenMap` in `Token.hpp` (used by the tokenizer for maximal-munch lookup).

### Step 3: Implement Lexer Recognition

In `src/StyioParser/Tokenizer.cpp`, add recognition logic in the appropriate character dispatch section. Follow the **maximal munch principle**: when the lexer sees `<`, it must look ahead to distinguish `<`, `<-`, `<~`, `<|`, `<=`, `<:`, `<<`.

### Step 4: Add String Representation

Update `getTokName()` in `Token.hpp` or `Token.cpp` so that debug printing shows the human-readable name.

### Step 5: Write Lexer Test

Create a `.styio` file in `tests/parsing/` that uses the new token. Verify it tokenizes correctly using `--debug` mode.

---

## 6. How to Add a New AST Node

### Step 1: Choose a Node Type

Add an entry to `StyioNodeType` in `src/StyioToken/Token.hpp`:

```cpp
WaveExpr,       // <~ or ~> conditional wave expression
StateDeclExpr,  // @[...] state container declaration
StateRefExpr,   // $var shadow variable reference
```

### Step 2: Forward-Declare

Add the class name to `src/StyioAST/ASTDecl.hpp` in the appropriate section.

### Step 3: Define the Node Class

In `src/StyioAST/AST.hpp`, define the class using CRTP:

```cpp
class WaveExprAST : public StyioASTTraits<WaveExprAST>
{
private:
  StyioAST *condition;
  StyioAST *true_expr;
  StyioAST *false_expr;
  bool is_dispatch;  // true = ~>, false = <~
  StyioDataType data_type;

public:
  static WaveExprAST *Create(
    StyioAST *condition,
    StyioAST *true_expr,
    StyioAST *false_expr,
    bool is_dispatch
  ) {
    return new WaveExprAST(condition, true_expr, false_expr, is_dispatch);
  }

  const StyioNodeType getNodeType() const override {
    return StyioNodeType::WaveExpr;
  }

  const StyioDataType getDataType() const override {
    return data_type;
  }

  // Accessors
  StyioAST *getCondition() { return condition; }
  StyioAST *getTrueExpr() { return true_expr; }
  StyioAST *getFalseExpr() { return false_expr; }
  bool isDispatch() { return is_dispatch; }

private:
  WaveExprAST(StyioAST *cond, StyioAST *t, StyioAST *f, bool disp)
    : condition(cond), true_expr(t), false_expr(f), is_dispatch(disp) {}
};
```

### Step 4: Register in All Visitors

You MUST add the new type to **every** visitor template list. If you miss one, the compiler will fail with a cryptic template error.

Files to update:
1. `src/StyioToString/ToStringVisitor.hpp` — Add to `ToStringVisitor<...>` type list AND implement `toString(WaveExprAST*, int)` in `ToString.cpp`
2. `src/StyioAnalyzer/ASTAnalyzer.hpp` — Add to `AnalyzerVisitor<...>` type list AND implement `typeInfer(WaveExprAST*)` in `TypeInfer.cpp` AND implement `toStyioIR(WaveExprAST*)` in `ToStyioIR.cpp`

### Step 5: Write Parser Support

See [Section 7](#7-how-to-extend-the-parser).

### Step 6: Write Tests

Create `.styio` test fixtures that exercise the new node. Ensure:
- Parsing produces the correct AST (verify with `--styio-ast`)
- Type inference assigns correct types (verify with `--styio-ast` after type checking)
- IR lowering doesn't crash (verify with `--styio-ir`)

---

## 7. How to Extend the Parser

### Structure

The parser is a collection of **free functions** in `src/StyioParser/Parser.cpp`. Each function has the signature:

```cpp
SomeASTNode* parse_something(StyioContext& context);
```

`StyioContext` holds:
- The token stream (`vector<StyioToken>`)
- Current position (`cur_pos`)
- Navigation methods: `cur_tok()`, `peek_tok()`, `move_forward()`, `check_drop()`

### Adding a New Parse Function

1. **Declare** in `src/StyioParser/Parser.hpp`:
   ```cpp
   WaveExprAST* parse_wave_expr(StyioContext& context, StyioAST* left);
   ```

2. **Implement** in `src/StyioParser/Parser.cpp`:
   ```cpp
   WaveExprAST*
   parse_wave_expr(StyioContext& context, StyioAST* left) {
     // `left` is the condition expression already parsed
     // Current token is <~ or ~>
     bool is_dispatch = (context.cur_tok().getType() == StyioTokenType::TOK_WAVE_RIGHT);
     context.move_forward();  // consume <~ or ~>

     StyioAST* true_expr = parse_expression(context);

     context.check_drop(StyioTokenType::TOK_PIPE_SINGLE);  // consume |

     StyioAST* false_expr = parse_expression(context);

     return WaveExprAST::Create(left, true_expr, false_expr, is_dispatch);
   }
   ```

3. **Integrate** into the expression parser. The wave operators have low precedence (below `||`, above `=`). Hook into `parse_stmt_or_expr` or the binary expression precedence climbing logic at the correct level.

### Disambiguation Rules

When adding parsing logic for ambiguous tokens (e.g., `>>` as pipe vs. continue), follow the rules in `docs/Styio-EBNF.md` Appendix: Disambiguation Rules. Implement lookahead using `context.peek_tok()`.

---

## 8. How to Add LLVM CodeGen for a Node

### Step 1: Add IR Node (if needed)

If the new AST concept requires a distinct IR representation:
1. Define in `src/StyioIR/GenIR/GenIR.hpp` (or `IOIR/IOIR.hpp` for I/O nodes)
2. Forward-declare in `src/StyioIR/IRDecl.hpp`

### Step 2: Implement IR Lowering

In `src/StyioAnalyzer/ToStyioIR.cpp`, implement:

```cpp
StyioIR* StyioAnalyzer::toStyioIR(WaveExprAST* node) {
  // Lower to StyioIR representation
  // ...
}
```

### Step 3: Implement LLVM Emission

In the appropriate CodeGen file (`CodeGen.cpp` for core logic, `CodeGenG.cpp` for general, `CodeGenIO.cpp` for I/O):

```cpp
llvm::Value* StyioToLLVM::toLLVMIR(SGWaveExpr* node) {
  llvm::Value* cond = toLLVMIR(node->getCondition());
  llvm::Value* true_val = toLLVMIR(node->getTrueExpr());
  llvm::Value* false_val = toLLVMIR(node->getFalseExpr());

  // For pure expressions: use select (no branch, better for CPU)
  return builder.CreateSelect(cond, true_val, false_val, "wave");
}
```

### CodeGen Rules

- **Prefer `select` over `br` + `phi`** for conditional values without side effects
- **Mark intrinsic implementations as `alwaysinline`**
- **Never allocate on the heap** from generated code — all state must be stack-allocated or in the pre-allocated state ledger
- **Respect the type system** — always call `getDataType()` on the AST/IR node to determine the correct LLVM type

---

## 9. How to Add a New Compiler Intrinsic

Compiler intrinsics are recognized selector modes like `[avg, n]`, `[max, n]`, etc.

### Step 1: Define Selector Mode

Add the mode string to the selector_mode recognition in the parser (e.g., `"avg"`, `"max"`, `"std"`, `"ema"`, `"rsi"`).

### Step 2: Document the Algorithm

Add a full specification to `docs/Styio-StdLib-Intrinsics.md` including:
- Algorithm pseudocode
- Time complexity (must be O(1) amortized per pulse)
- Memory requirements (exact byte count)
- `@` handling behavior
- LLVM codegen hints

### Step 3: Implement State Allocation

The intrinsic's state (ring buffer, accumulator, deque) must be allocated as part of the anonymous ledger. The state analyzer must recognize the intrinsic and compute its memory footprint.

### Step 4: Implement Inline CodeGen

Generate LLVM IR that performs the algorithm directly — no function calls. The generated code must be equivalent to hand-optimized C++.

---

## 10. Testing Requirements

### 10.1 What Must Be Tested

Every change MUST include tests for:
- **Lexer:** A `.styio` file that uses the new token. Run with `--debug` to verify tokenization.
- **Parser:** A `.styio` file that uses the new syntax. Run with `--styio-ast` to verify the AST.
- **Type inference:** If the change affects types, verify with `--styio-ast` after type checking.
- **CodeGen:** If the change affects code generation, verify with `--llvm-ir` and/or `--all`.
- **Execution:** If possible, verify the JIT-executed result is correct.

### 10.2 Test File Organization

Place test fixtures in the appropriate subdirectory under `tests/parsing/`:

```
tests/parsing/
├── wave_operators/          # <~ and ~> tests
│   ├── 00001.styio
│   └── 00002.styio
├── state_containers/        # @[...] and $var tests
│   ├── 00001.styio
│   └── 00002.styio
├── stream_sync/             # & (zip) and << (snapshot) tests
...
```

Naming: `NNNNN.styio` where N is a zero-padded sequential number. Use `extend_tests.py` to generate new files.

### 10.3 GoogleTest

For C++ unit tests of internal compiler logic (not end-to-end), add to `tests/styio_test.cpp`:

```cpp
TEST(Lexer, WaveOperatorTokenization) {
  // ...
}
```

### 10.4 Regression Rule

**No existing test may break.** If a design change requires modifying existing tests, the change MUST be documented with a rationale.

---

## 11. Build System

### 11.1 CMake

The project uses a single top-level `CMakeLists.txt`. Source files are **explicitly listed** (no GLOB).

**When adding a new `.cpp` file**, you MUST add it to the `add_executable(styio ...)` list in `CMakeLists.txt`.

### 11.2 Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| LLVM | 18.1.0+ | IR generation, JIT, native target |
| ICU | Latest | Unicode string handling |
| GoogleTest | Fetched via CMake | Unit testing |
| cxxopts | Vendored in `src/include/` | CLI argument parsing |

### 11.3 Known Issues

1. **`Util.cpp` missing:** `CMakeLists.txt` references `src/StyioUtil/Util.cpp` but the file doesn't exist. Either create it or remove the reference.
2. **Hardcoded compiler paths:** `CMakeLists.txt` sets `CMAKE_C_COMPILER` etc. to `/usr/bin/clang`. On Windows/macOS, these must be overridden or removed.
3. **Include paths:** `main.cpp` uses paths like `"StyioAST/AST.hpp"` expecting `src/` on the include path, but this isn't explicitly configured in CMake. The build may rely on the IDE or build directory structure.

---

## 12. Documentation Rules

### 12.1 When to Update Docs

- **New syntax or symbol:** Update `Styio-Language-Design.md`, `Styio-EBNF.md`, and `Styio-Symbol-Reference.md`
- **New intrinsic:** Update `Styio-StdLib-Intrinsics.md`
- **New driver interface change:** Update `Styio-Resource-Driver.md`
- **Innovation-affecting change:** Verify `Styio-Research-Innovations.md` still holds

### 12.2 Doc Format

All documentation is Markdown. Use:
- `#` through `###` for hierarchy
- Tables for structured data
- Fenced code blocks with language tags (`cpp`, `ebnf`, or no tag for Styio code)
- No emojis

### 12.3 Code Examples in Docs

When showing Styio code examples, use the canonical "Golden Cross" strategy as the reference example:

```
@binance{"BTCUSDT"} >> #(p) => {
    # get_ma := (src, n) => src[avg, n]

    @[5 ](ma5  = get_ma(p, 5 ))
    @[20](ma20 = get_ma(p, 20))

    is_golden = ($ma5 > $ma20) && ($ma5[<<, 1] <= $ma20[<<, 1])

    # order_logic := (price) => { >_ ("Buy at: " + price) }

    (is_golden) ~> order_logic(p) | @
}
```

This is the "constitution" — any syntax change that breaks this example must be explicitly justified.

---

## 13. Design Reference Documents

Agents working on specific areas should consult:

| Task | Primary Reference |
|------|-------------------|
| Adding syntax | `Styio-EBNF.md` (grammar), `Styio-Symbol-Reference.md` (tokens) |
| Implementing `@` propagation | `Styio-Language-Design.md` §3.4 (Undefined type) |
| State containers `@[...]` / `$` | `Styio-Language-Design.md` §8 (State Management) |
| Wave operators `<~` / `~>` | `Styio-Language-Design.md` §6 (Wave Operators) |
| Stream sync `&` / `<<` | `Styio-Language-Design.md` §9 (Stream Synchronization) |
| Intrinsic algorithms | `Styio-StdLib-Intrinsics.md` (all algorithms with pseudocode) |
| Resource drivers | `Styio-Resource-Driver.md` (C++ interface, threading model) |
| Performance constraints | `Styio-Research-Innovations.md` (what properties must be preserved) |

---

## 14. Prohibited Actions

Agents MUST NOT:

1. **Introduce keywords.** Styio uses symbols, not words like `if`, `while`, `for`, `return`, `fn`, `let`. The only text tokens allowed are type names (`i32`, `f64`, etc.), `schema`, `true`, `false`, and user identifiers.

2. **Break the Golden Cross example.** See §12.3. This example is the language's "constitution."

3. **Allocate heap memory in generated code.** All runtime state must be stack-allocated or in the pre-allocated contiguous ledger. No `malloc`, no `new`, no GC.

4. **Modify vendored files.** `src/include/cxxopts.hpp` is third-party. Do not edit it.

5. **Use `Deprecated/` code.** The `src/Deprecated/` directory is archived reference only. Do not include, call, or copy from it.

6. **Skip visitor registration.** Every new AST node MUST be added to ALL visitor template lists. Partial registration causes hard-to-debug template errors.

7. **Add dependencies without justification.** New external libraries require explicit approval. Prefer header-only or vendored solutions.

8. **Break existing tests.** All tests must pass before and after any change. If a test must change, document why.

9. **Generate binary or hash content.** Never output binary blobs, extremely long hashes, or non-textual content.

10. **Commit build artifacts or IDE-specific files.** Respect `.gitignore`.

---

## 15. Decision Authority

### What Agents Can Decide Independently

- Implementation details within an existing design (e.g., choosing between `select` and `br`+`phi` in LLVM codegen)
- Bug fixes that don't change language semantics
- Performance optimizations that preserve observable behavior
- Adding test cases
- Improving error messages

### What Requires Human Approval

- **Any new symbol or syntax** — must be discussed and added to design docs first
- **Changes to `@` propagation semantics** — this is a core language invariant
- **Changes to the compilation pipeline stages** — the 6-stage flow is architectural
- **New external dependencies**
- **Removing or renaming existing tokens/AST nodes** — may break downstream agents
- **Changes to the EBNF grammar** — must update `Styio-EBNF.md` simultaneously

### Conflict Resolution

If two agents propose conflicting changes to the same file:
1. The change that preserves backward compatibility wins
2. If both are backward-compatible, the change with tests wins
3. If still tied, the human project owner decides

---

## Appendix A: Quick-Start Checklist for New Agents

1. Read this file (`AGENT-SPEC.md`) completely
2. Read `Styio-Language-Design.md` for the full language picture
3. Read `Styio-Symbol-Reference.md` to understand the symbol system
4. Examine `src/main.cpp` to understand the pipeline flow
5. Examine `src/StyioToken/Token.hpp` for token/node type enums
6. Examine `src/StyioAST/AST.hpp` for the AST class pattern
7. Run the compiler on a test file with `--all` to see all stages
8. Make your change
9. Run `clang-format` on modified files
10. Verify all existing tests still pass
11. Add new tests for your change
12. Update documentation if your change affects language syntax or semantics

## Appendix B: Compilation Pipeline Debug Flags

| Flag | Output |
|------|--------|
| `--debug` | Print source with line numbers + token stream |
| `--styio-ast` | Print AST after parsing (and after type inference) |
| `--styio-ir` | Print Styio IR after lowering |
| `--llvm-ir` | Print LLVM IR before JIT execution |
| `--all` | Enable all of the above |
| `--file <path>` | Specify input `.styio` file (also accepts positional arg) |

## Appendix C: Feature Implementation Status

Features from the design documents and their current implementation state:

| Feature | Design Doc Section | Lexer | Parser | AST | TypeInfer | IR | CodeGen | Status |
|---------|-------------------|:-----:|:------:|:---:|:---------:|:--:|:-------:|--------|
| Basic expressions (arithmetic, logic) | §8 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | **Working** |
| Functions (`#`) | §4 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | **Working** |
| Pattern matching (`?=`) | §5.1 | ✅ | ✅ | ✅ | Partial | Partial | Partial | **In Progress** |
| Pipe / Iterate (`>>`) | §5.4 | ✅ | ✅ | ✅ | Partial | Partial | Partial | **In Progress** |
| Print (`>_`) | §11 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | **Working** |
| Resources (`@`) | §7 | ✅ | ✅ | ✅ | Partial | Partial | — | **In Progress** |
| Bindings (`:=`, `=`) | §— | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | **Working** |
| Collections (list, tuple, set) | §10 | ✅ | ✅ | ✅ | Partial | Partial | Partial | **In Progress** |
| Format strings (`$"..."`) | §11.3 | ✅ | ✅ | ✅ | Partial | Partial | — | **In Progress** |
| Wave operators (`<~`, `~>`) | §6 | — | — | — | — | — | — | **Not Started** |
| State containers (`@[...]`) | §8.2 | — | — | — | — | — | — | **Not Started** |
| Shadow references (`$var`) | §8.3 | — | — | — | — | — | — | **Not Started** |
| History probe (`[<<, n]`) | §8.4 | — | — | — | — | — | — | **Not Started** |
| Pulse Frame Lock | §8.5 | — | — | — | — | — | — | **Not Started** |
| Break (`^^^^`) | §5.5 | — | — | — | — | — | — | **Not Started** |
| Continue (`>>>`) | §5.6 | — | — | — | — | — | — | **Not Started** |
| Stream Zip (`&`) | §9.1 | — | — | — | — | — | — | **Not Started** |
| Snapshot Pull (`<< @res`) | §9.2 | — | — | — | — | — | — | **Not Started** |
| Selector intrinsics (`[avg,n]`) | Intrinsics §2 | — | — | — | — | — | — | **Not Started** |
| Diagnostic `??` | §12.3 | — | — | — | — | — | — | **Not Started** |
| Anonymous Ledger | §8.6 | — | — | — | — | — | — | **Not Started** |
| Context capture `$(...)` | §4.3 | — | — | — | — | — | — | **Not Started** |
| Yield `<|` | §5.7 | — | — | — | — | — | — | **Not Started** |
| Infinite generator `[...]` | §5.2 | — | — | — | — | — | — | **Not Started** |
| Guard `?(expr)` | §5.3 | — | — | — | — | — | — | **Not Started** |
| Resource drivers (C++ interface) | Driver Spec | — | — | — | — | — | — | **Not Started** |
