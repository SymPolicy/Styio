#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MATRIX = ROOT / "docs" / "design" / "syntax" / "SYNTAX-CONVERGENCE-MATRIX.json"
FEATURE_ID_RE = re.compile(r"^[a-z0-9][a-z0-9_.-]*$")


def rel(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def load_json(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"{rel(path)}: invalid JSON at line {exc.lineno}: {exc.msg}") from exc
    if not isinstance(payload, dict):
        raise ValueError(f"{rel(path)}: top-level value must be an object")
    return payload


def string_list(value: Any, context: str) -> list[str]:
    if not isinstance(value, list) or not value:
        raise ValueError(f"{context} must be a non-empty list")
    result: list[str] = []
    seen: set[str] = set()
    for index, item in enumerate(value):
        if not isinstance(item, str) or not item.strip():
            raise ValueError(f"{context}[{index}] must be a non-empty string")
        cleaned = item.strip()
        if cleaned in seen:
            raise ValueError(f"{context} entries must be unique")
        seen.add(cleaned)
        result.append(cleaned)
    return result


def require_file(relative_path: str, context: str) -> Path:
    path = ROOT / relative_path
    if not path.is_file():
        raise ValueError(f"{context}: missing file {relative_path}")
    return path


def expected_oracle_paths(case_path: Path) -> list[Path]:
    parts = case_path.relative_to(ROOT).parts
    if len(parts) >= 4 and parts[0:2] == ("tests", "pipeline_cases") and case_path.name == "input.styio":
        expected = case_path.parent / "expected"
        return [
            expected / "tokens.txt",
            expected / "ast.txt",
            expected / "styio_ir.txt",
            expected / "llvm_ir.txt",
            expected / "stdout.txt",
        ]

    if len(parts) >= 4 and parts[0:2] == ("tests", "milestones") and case_path.suffix == ".styio":
        expected = case_path.parent / "expected"
        suffix = ".err" if case_path.stem.startswith("e") else ".out"
        return [expected / f"{case_path.stem}{suffix}"]

    if len(parts) >= 4 and parts[0:2] == ("tests", "features") and case_path.suffix == ".styio":
        expected = case_path.parent / "expected"
        suffix = ".err" if case_path.stem.startswith("e") else ".out"
        return [expected / f"{case_path.stem}{suffix}"]

    return []


def validate_golden_case(relative_path: str, context: str) -> None:
    path = require_file(relative_path, context)
    for oracle in expected_oracle_paths(path):
        if not oracle.is_file():
            raise ValueError(f"{context}: missing golden oracle {rel(oracle)} for {relative_path}")


def validate_feature(feature: Any, index: int, seen_ids: set[str]) -> None:
    context = f"features[{index}]"
    if not isinstance(feature, dict):
        raise ValueError(f"{context} must be an object")

    feature_id = feature.get("id")
    if not isinstance(feature_id, str) or not FEATURE_ID_RE.match(feature_id):
        raise ValueError(f"{context}.id must be a lowercase feature id")
    if feature_id in seen_ids:
        raise ValueError(f"{context}.id duplicates feature `{feature_id}`")
    seen_ids.add(feature_id)
    context = f"feature `{feature_id}`"

    if feature.get("status") != "converged":
        raise ValueError(f"{context}: status must be `converged`")
    if "implementations" in feature or "alternative_implementations" in feature:
        raise ValueError(f"{context}: syntax features must declare exactly one implementation")

    implementation = feature.get("implementation")
    if not isinstance(implementation, dict):
        raise ValueError(f"{context}: implementation must be one object")
    impl_path_value = implementation.get("path")
    impl_symbol = implementation.get("symbol")
    if not isinstance(impl_path_value, str) or not impl_path_value.strip():
        raise ValueError(f"{context}: implementation.path must be a non-empty string")
    if not isinstance(impl_symbol, str) or not impl_symbol.strip():
        raise ValueError(f"{context}: implementation.symbol must be a non-empty string")
    impl_path = require_file(impl_path_value.strip(), context)
    impl_text = impl_path.read_text(encoding="utf-8", errors="replace")
    if impl_symbol.strip() not in impl_text:
        raise ValueError(f"{context}: implementation symbol `{impl_symbol}` is not present in {impl_path_value}")

    docs = string_list(feature.get("docs"), f"{context}.docs")
    for doc in docs:
        require_file(doc, context)

    golden_cases = string_list(feature.get("golden_cases"), f"{context}.golden_cases")
    for golden_case in golden_cases:
        validate_golden_case(golden_case, context)


def validate_matrix(path: Path) -> int:
    payload = load_json(path)
    if payload.get("schema_version") != 1:
        raise ValueError(f"{rel(path)}: schema_version must be 1")

    features = payload.get("features")
    if not isinstance(features, list) or not features:
        raise ValueError(f"{rel(path)}: features must be a non-empty list")

    minimum_feature_count = payload.get("minimum_feature_count", 1)
    if not isinstance(minimum_feature_count, int) or minimum_feature_count < 1:
        raise ValueError(f"{rel(path)}: minimum_feature_count must be a positive integer")
    if len(features) < minimum_feature_count:
        raise ValueError(
            f"{rel(path)}: expected at least {minimum_feature_count} converged syntax features, found {len(features)}"
        )

    seen_ids: set[str] = set()
    for index, feature in enumerate(features):
        validate_feature(feature, index, seen_ids)

    return len(features)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate the Styio syntax convergence matrix.")
    parser.add_argument("--matrix", default=str(DEFAULT_MATRIX), help="Path to SYNTAX-CONVERGENCE-MATRIX.json")
    args = parser.parse_args(argv)

    matrix = Path(args.matrix)
    if not matrix.is_absolute():
        matrix = ROOT / matrix

    try:
        count = validate_matrix(matrix)
    except ValueError as exc:
        print(f"syntax-convergence-gate: {exc}", file=sys.stderr)
        return 1

    print(f"syntax-convergence-gate: ok ({count} converged syntax features)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
