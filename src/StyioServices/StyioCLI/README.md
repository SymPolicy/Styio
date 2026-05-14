# StyioCLI

**Purpose:** Provide command-line service entry helpers for reusable Styio language-service contracts.

**Last updated:** 2026-05-14

## Use

The current public CLI service is syntax-only validation:

```bash
styio check --syntax --json --file path/to/file.styio
```

Optional parser selection:

```bash
styio check --syntax --json --parser-engine nightly --file path/to/file.styio
styio check --syntax --json --parser-engine legacy --file path/to/file.styio
```

The service returns one JSON object. Successful validation has `status:"ok"` and an empty diagnostics array. Lexing and parsing failures return `status:"lexical_error"` or `status:"syntax_error"` with line, column, offset, and length fields.

## Embedding

Use the CLI helper from the full `styio` binary dispatch path:

```cpp
#include "StyioServices/StyioCLI/SyntaxCheck.hpp"

return styio::services::run_syntax_check_cli(argc, argv);
```

`run_syntax_check_cli` expects `argv[1]` to be `check`. It owns argument parsing for `--syntax`, `--json`, `--file`, and `--parser-engine`.

## Guarantees

1. Runs lexing, parsing, and AST construction only.
2. Does not type-check.
3. Does not lower to StyioIR or LLVM.
4. Does not execute source code.
5. Does not access runtime resources such as `@stdin`, `@file`, `@stdout`, `||>`, or `?|`.

See the full service inventory in [../MANIFEST.md](../MANIFEST.md).

