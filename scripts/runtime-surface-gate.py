#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTERN_HEADER = ROOT / "src/StyioExtern/ExternLib.hpp"
EXTERN_IMPL = ROOT / "src/StyioExtern/ExternLib.cpp"
JIT_HEADER = ROOT / "src/StyioJIT/StyioJIT_ORC.hpp"
CODEGEN_DIR = ROOT / "src/StyioCodeGen"

EXPORT_RE = re.compile(r'extern\s+"C"\s+DLLEXPORT\b[^;]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', re.S)
FUNCTION_DEF_RE = re.compile(r"(?m)^([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{]*\)\s*\{")
ADD_SYMBOL_RE = re.compile(r'add_symbol\("([^"]+)"')
STRING_LITERAL_RE = re.compile(r'"(styio_[A-Za-z0-9_]+)"')
LIST_SUFFIX_BLOCK_RE = re.compile(
    r"auto\s+builtin_list_family_from_suffix\b.*?\{(.*?)\n\s*\};",
    re.S,
)
LIST_SUFFIX_RE = re.compile(r'suffix\s*==\s*"([^"]+)"')


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def parse_exports() -> set[str]:
    return set(EXPORT_RE.findall(read_text(EXTERN_HEADER)))


def parse_implementations() -> set[str]:
    return set(FUNCTION_DEF_RE.findall(read_text(EXTERN_IMPL)))


def parse_jit_registrations() -> set[str]:
    return set(ADD_SYMBOL_RE.findall(read_text(JIT_HEADER)))


def parse_builtin_list_suffixes(codegen_text: str) -> set[str]:
    match = LIST_SUFFIX_BLOCK_RE.search(codegen_text)
    suffixes = {"i64"}
    if match is None:
        return suffixes
    suffixes.update(LIST_SUFFIX_RE.findall(match.group(1)))
    return suffixes


def parse_codegen_required_helpers(exports: set[str]) -> set[str]:
    required: set[str] = set()

    codegen_files = sorted(CODEGEN_DIR.rglob("*.cpp")) + sorted(CODEGEN_DIR.rglob("*.hpp"))
    for path in codegen_files:
        text = read_text(path)
        required.update(set(STRING_LITERAL_RE.findall(text)) & exports)

    codegen_g = read_text(CODEGEN_DIR / "CodeGenG.cpp")
    list_suffixes = parse_builtin_list_suffixes(codegen_g)
    if '"styio_list_push_"' in codegen_g:
        required.update(f"styio_list_push_{suffix}" for suffix in list_suffixes)
    if '"styio_list_insert_"' in codegen_g:
        required.update(f"styio_list_insert_{suffix}" for suffix in list_suffixes)

    return required


def format_symbols(symbols: set[str]) -> str:
    return "\n".join(f"    - {symbol}" for symbol in sorted(symbols))


def main() -> int:
    exports = parse_exports()
    implementations = parse_implementations()
    jit_registrations = parse_jit_registrations()
    codegen_required = parse_codegen_required_helpers(exports)

    failures: list[str] = []

    missing_implementations = exports - implementations
    if missing_implementations:
        failures.append(
            "ExternLib exports missing implementations in src/StyioExtern/ExternLib.cpp:\n"
            + format_symbols(missing_implementations)
        )

    missing_jit_exports = exports - jit_registrations
    if missing_jit_exports:
        failures.append(
            "ExternLib exports missing ORC registrations in src/StyioJIT/StyioJIT_ORC.hpp:\n"
            + format_symbols(missing_jit_exports)
        )

    unexpected_jit_exports = jit_registrations - exports
    if unexpected_jit_exports:
        failures.append(
            "ORC registrations are not declared in src/StyioExtern/ExternLib.hpp:\n"
            + format_symbols(unexpected_jit_exports)
        )

    missing_codegen_exports = codegen_required - exports
    if missing_codegen_exports:
        failures.append(
            "Codegen emits runtime helpers that are not declared in src/StyioExtern/ExternLib.hpp:\n"
            + format_symbols(missing_codegen_exports)
        )

    missing_codegen_jit = codegen_required - jit_registrations
    if missing_codegen_jit:
        failures.append(
            "Codegen emits runtime helpers that are not registered in src/StyioJIT/StyioJIT_ORC.hpp:\n"
            + format_symbols(missing_codegen_jit)
        )

    if failures:
        print("runtime surface gate failed:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        print(
            "Keep src/StyioCodeGen/, src/StyioExtern/ExternLib.hpp/.cpp, and "
            "src/StyioJIT/StyioJIT_ORC.hpp aligned in the same delivery.",
            file=sys.stderr,
        )
        return 1

    print(
        "runtime surface gate passed "
        f"({len(exports)} exports, {len(jit_registrations)} ORC registrations, "
        f"{len(codegen_required)} codegen-required helpers)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
