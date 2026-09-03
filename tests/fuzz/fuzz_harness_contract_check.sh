#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

python3 - "$ROOT/tests/fuzz/fuzz_lexer.cpp" "$ROOT/tests/fuzz/fuzz_parser.cpp" <<'PY'
from pathlib import Path
import sys

for path in (Path(p) for p in sys.argv[1:]):
    text = path.read_text()
    if "tokenizeOwned" not in text:
        raise SystemExit(f"{path} must call tokenizeOwned")
    if "StyioTokenizer::tokenize(" in text or "StyioTokenizer::tokenize " in text:
        raise SystemExit(f"{path} must not call the legacy tokenizer entry")
    if "CompilationSession" not in text:
        raise SystemExit(f"{path} must keep CompilationSession as token owner")
print("fuzz harness contract ok")
PY
