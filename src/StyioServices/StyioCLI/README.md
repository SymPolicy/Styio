# StyioCLI

**Purpose:** Provide command-line service entry helpers for reusable Styio language-service contracts.

**Last updated:** 2026-05-21

## Use

The current public CLI service is syntax-only validation:

```bash
styio check --syntax --json --file path/to/file.styio
```

Explicit authority-preserving parser selection:

```bash
styio check --syntax --json --parser-engine nightly --file path/to/file.styio
```

`legacy`, `new`, and any other parser engine names are rejected by this public syntax service. The accepted grammar is owned by the hand-written nightly compiler parser.

The service returns one JSON object. Successful validation has `status:"ok"` and an empty diagnostics array. Lexing and parsing failures return `status:"lexical_error"` or `status:"syntax_error"` with stable `code`, `phase`, `severity`, `message`, line, column, offset, length, `source_context`, and `notes` fields.

Current public syntax-check code families include:

1. `STYIO_LEX_INVALID_TOKEN`
2. `STYIO_LEX_UNTERMINATED_STRING`
3. `STYIO_LEX_UNTERMINATED_BLOCK_COMMENT`
4. `STYIO_PARSE_UNEXPECTED_TOKEN`
5. `STYIO_PARSE_UNSUPPORTED_SYNTAX`
6. `STYIO_PARSE_SHADOW_MISMATCH`
7. `STYIO_SERVICE_INVALID_ARGUMENT`
8. `STYIO_SERVICE_READ_FAILED`

`source_context` is designed for Clang-style caret rendering by IDEs and terminals:

```json
{
  "line_text": "# broken := (a: i32) => a +",
  "range_start_column": 1,
  "range_end_column": 2,
  "caret": "^"
}
```

For parser failures, the command may continue after a failed statement to collect more diagnostics, but a result with any parser diagnostic is still `status:"syntax_error"`. Recovery diagnostics never make malformed source accepted.

## Embedding

Use the CLI helper from the full `styio` binary dispatch path:

```cpp
#include "StyioServices/StyioCLI/SyntaxCheck.hpp"

return styio::services::run_syntax_check_cli(argc, argv);
```

`run_syntax_check_cli` expects `argv[1]` to be `check`. It owns argument parsing for `--syntax`, `--json`, `--file`, and the authority-preserving `--parser-engine nightly` spelling.

## Guarantees

1. Runs lexing, authoritative nightly parsing, and AST construction only.
2. Does not type-check.
3. Does not lower to StyioIR or LLVM.
4. Does not execute source code.
5. Does not access runtime resources such as `@stdin`, `@file`, `@stdout`, `||>`, or `?|`.
6. Does not accept alternate parser engines for public syntax validity.

See the full service inventory in [../MANIFEST.md](../MANIFEST.md).
