#!/usr/bin/env python3
"""Validate the current AST/Sema/IR/CodeGen layer boundary."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"


ENTRYPOINT_RULES = {
    "typeInfer": {
        "pattern": re.compile(r"\btypeInfer\s*\("),
        "allowed": (
            "src/StyioAST/",
            "src/StyioSema/",
        ),
    },
    "toStyioIR": {
        "pattern": re.compile(r"\btoStyioIR\s*\("),
        "allowed": (
            "src/StyioAST/AST.hpp",
            "src/StyioSema/SemaContext.hpp",
            "src/StyioLowering/",
        ),
    },
    "toLLVMIR": {
        "pattern": re.compile(r"\btoLLVMIR\s*\("),
        "allowed": (
            "src/StyioIR/StyioIR.hpp",
            "src/StyioCodeGen/",
        ),
    },
    "toLLVMType": {
        "pattern": re.compile(r"\btoLLVMType\s*\("),
        "allowed": (
            "src/StyioIR/StyioIR.hpp",
            "src/StyioCodeGen/",
        ),
    },
}


FORBIDDEN_INCLUDE_RULES = (
    {
        "owner": ("src/StyioCodeGen/", "src/StyioIR/", "src/StyioRuntime/", "src/StyioExtern/", "src/StyioNative/"),
        "forbidden": ("StyioAST/", "StyioParser/", "StyioSema/", "StyioLowering/"),
        "reason": "backend/runtime layers must not include frontend or middle-layer implementation headers",
        "exceptions": ("src/StyioIR/Verifier.hpp",),
    },
    {
        "owner": ("src/StyioSema/", "src/StyioLowering/"),
        "forbidden": ("StyioCodeGen/", "StyioJIT/"),
        "reason": "Sema and lowering must stay independent of LLVM/codegen execution",
        "exceptions": (),
    },
)


INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]')


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def is_allowed(path: str, allowed: tuple[str, ...]) -> bool:
    return any(path == item or path.startswith(item) for item in allowed)


def source_files() -> list[Path]:
    return sorted(
        path
        for path in SRC.rglob("*")
        if path.is_file() and path.suffix in {".cpp", ".hpp", ".h"}
    )


def validate_entrypoints(path: Path, text: str, errors: list[str]) -> None:
    path_rel = rel(path)
    for name, rule in ENTRYPOINT_RULES.items():
        if not rule["pattern"].search(text):
            continue
        if not is_allowed(path_rel, rule["allowed"]):
            errors.append(
                f"{path_rel}: `{name}(` is outside the approved layer entrypoints"
            )


def validate_includes(path: Path, text: str, errors: list[str]) -> None:
    path_rel = rel(path)
    for rule in FORBIDDEN_INCLUDE_RULES:
      if path_rel in rule["exceptions"]:
          continue
      if not is_allowed(path_rel, rule["owner"]):
          continue
      for line_number, line in enumerate(text.splitlines(), start=1):
          match = INCLUDE_RE.match(line)
          if match is None:
              continue
          include = match.group(1)
          if any(item in include for item in rule["forbidden"]):
              errors.append(
                  f"{path_rel}:{line_number}: forbidden include `{include}`: {rule['reason']}"
              )


def main() -> int:
    errors: list[str] = []
    scanned = 0
    for path in source_files():
        scanned += 1
        text = path.read_text(encoding="utf-8")
        validate_entrypoints(path, text, errors)
        validate_includes(path, text, errors)

    if errors:
        print("architecture layer gate failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(f"architecture layer gate passed ({scanned} source files scanned)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
