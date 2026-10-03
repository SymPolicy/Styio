#!/usr/bin/env python3
"""Discover example validation scope without a per-file inclusion manifest.

``discover_examples(root, ctest_catalog, compiler=..., timeout=...)`` accepts the
JSON object from ``ctest --show-only=json-v1`` (or its ``tests`` list). It returns
JSON-safe entries, runnable fallback specifications, and an advisory inventory.
It does not execute examples and a catalog reference is never a passing result.

Only Git-index paths below ``example/`` (configurable) are discovered, including
staged additions and renames. A plain .styio source can receive a fallback spec
when it has exactly one tracked same-basename .out, beside the source or below
an ``expected/`` directory at the source directory/example root. The root's
expected directory can mirror source subdirectories or use a unique basename.
Optional stdin uses .stdin or .in, beside the source or analogously below
``input/``. An @stdin marker requires a fixture, including an empty fixture for
EOF. Ambiguous, missing, unreadable, untracked, or symlink fixtures never qualify.

Fallback commands use argv, run from the repository root, read stdin from the
fixture (or supply EOF when absent), compare stdout bytes exactly, require exit
zero, and have a bounded timeout. The caller owns execution/result recording.
``identity_paths`` includes source and fixture paths for reuse invalidation;
compiler/build identity and the command contract must also be included by the
caller. Existing CTest references suppress fallbacks, not result verification.

Applicability is deliberately conservative lexical screening, not a language
parser or a security sandbox. Non-stdio @ resources, external/native/import
markers, URLs, and credential markers require explicit invocation context.
Markers in comments/strings may therefore yield an advisory false positive.
Shell wrappers require an existing CTest invocation. Generated/reference/support
trees and non-executable fixtures are inventoried but never auto-executed. Unknown
file types and applicability remain explicitly unverified; discovery does not
establish all-applicable completeness or execute transitive wrapper sources.
"""
from __future__ import annotations

import math
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess
from typing import Any, Mapping, Sequence


SUPPORT_DIRECTORIES = frozenset(
    {"data", "expected", "fixtures", "generated", "input", "inputs", "reference", "references", "support"}
)
SUPPORT_SUFFIXES = frozenset({".out", ".in", ".stdin", ".err", ".md", ".txt", ".json", ".csv", ".toml", ".yaml", ".yml"})
SHELL_SUFFIXES = frozenset({".sh", ".bash", ".zsh", ".bat", ".cmd", ".ps1"})
MAX_SOURCE_BYTES = 1024 * 1024


def tracked_example_paths(root: Path | str, example_roots: Sequence[str] = ("example",)) -> list[str]:
    """Read the Git index, not a glob or HEAD-only manifest (NUL-safe)."""
    prefixes = tuple(PurePosixPath(path).as_posix().rstrip("/") for path in example_roots)
    if not prefixes or any(not p or p == "." or p.startswith("/") or ".." in p.split("/") for p in prefixes):
        raise ValueError("example roots must be repository-relative directories")
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "--cached", "--full-name", "-z", "--", *(p + "/" for p in prefixes)],
        check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    return sorted({os.fsdecode(path) for path in result.stdout.split(b"\0") if path})


def _catalog_tests(catalog: Mapping[str, Any] | Sequence[Mapping[str, Any]]) -> list[Mapping[str, Any]]:
    tests = catalog.get("tests") if isinstance(catalog, Mapping) else catalog
    if not isinstance(tests, (list, tuple)):
        raise ValueError("CTest catalog must contain a tests list")
    if any(not isinstance(test, Mapping) or not isinstance(test.get("name"), str) for test in tests):
        raise ValueError("CTest entries must have string names")
    return list(tests)


def _tokens(command: Any) -> set[str]:
    """Extract argv and bounded shell words; never expand/evaluate shell code."""
    parts = [command] if isinstance(command, str) else command
    if not isinstance(parts, (list, tuple)):
        return set()
    tokens = {part for part in parts if isinstance(part, str)}
    # One shell split handles bash -c strings, including quoted paths/spaces.
    for part in tuple(tokens):
        try:
            lexer = shlex.shlex(part, posix=True, punctuation_chars="();<>|&")
            lexer.whitespace_split = True
            lexer.commenters = ""
            tokens.update(lexer)
        except ValueError:
            pass  # Unknown shell syntax must not create guessed consumers.
    return tokens


def ctest_example_consumers(
    root: Path | str,
    paths: Sequence[str],
    ctest_catalog: Mapping[str, Any] | Sequence[Mapping[str, Any]],
) -> dict[str, list[str]]:
    """Map literal command references to CTest names; do not infer wrapper use."""
    root = Path(root).resolve()
    consumers: dict[str, set[str]] = {path: set() for path in paths}
    for test in _catalog_tests(ctest_catalog):
        tokens = _tokens(test.get("command", []))
        # Support --file=PATH and shell assignments without fuzzy substrings.
        tokens.update(token.split("=", 1)[1] for token in tuple(tokens) if "=" in token)
        working_directory = root
        for prop in test.get("properties", []):
            if prop.get("name") == "WORKING_DIRECTORY" and isinstance(prop.get("value"), str):
                working_directory = Path(prop["value"])
                if not working_directory.is_absolute():
                    working_directory = root / working_directory
        normalized: set[str] = set()
        for token in tokens:
            # Lexical normalization suffices; do not follow symlinks or inspect
            # paths outside the source tree while mapping a test command.
            candidate = Path(token)
            if not candidate.is_absolute():
                candidate = working_directory / candidate
            normalized.add(os.path.normpath(str(candidate)))
        for path in paths:
            if str(root / path) in normalized:
                consumers[path].add(test["name"])
    return {path: sorted(names) for path, names in consumers.items()}


def _regular_file(root: Path, path: str) -> bool:
    candidate = root / path
    # A parent-directory symlink is also outside the bounded fallback contract.
    try:
        if any(part.is_symlink() for part in (candidate, *candidate.parents) if part.is_relative_to(root)):
            return False
        return candidate.is_file() and candidate.resolve().is_relative_to(root) and os.access(candidate, os.R_OK)
    except (OSError, ValueError):
        return False


def _fixture_candidates(source: str, example_root: str, directory: str, suffixes: Sequence[str]) -> set[str]:
    source_path = PurePosixPath(source)
    base = PurePosixPath(example_root)
    relative = source_path.relative_to(base)
    candidates: set[str] = set()
    for suffix in suffixes:
        candidates.update(
            str(candidate)
            for candidate in (
                source_path.with_suffix(suffix),
                source_path.parent / directory / (source_path.stem + suffix),
                base / directory / relative.with_suffix(suffix),
                base / directory / (source_path.stem + suffix),
            )
        )
    return candidates


def _source_advisories(source: str) -> list[str]:
    reasons: list[str] = []
    resources = re.findall(r"@\s*([A-Za-z_][A-Za-z_0-9]*|[^\s])", source)
    if set(resources) - {"stdin", "stdout", "stderr"}:
        reasons.append("Unknown or non-stdio @ resource requires explicit invocation context")
    if re.search(r"\b(?:import|include|require|exec|system|popen|getenv|socket|connect|fetch|curl|wget|ssh|network|http\w*|tcp\w*|udp\w*)\b|\b[a-z][a-z0-9+.-]*://", source, re.IGNORECASE):
        reasons.append("External, native, import, or network marker requires explicit invocation context")
    if re.search(r"\b(?:credentials?|password|api[_-]?key|access[_-]?token|secret)\b", source, re.IGNORECASE):
        reasons.append("Credential marker requires explicit invocation context")
    return reasons


def discover_examples(
    root: Path | str,
    ctest_catalog: Mapping[str, Any] | Sequence[Mapping[str, Any]],
    *,
    compiler: str = "build/default/bin/styio",
    timeout: float = 20,
    example_roots: Sequence[str] = ("example",),
) -> dict[str, Any]:
    """Return registered scope, convention-based specs, and advisory findings."""
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError("example timeout must be finite and positive")
    root = Path(root).resolve()
    paths = tracked_example_paths(root, example_roots)
    tracked = set(paths)
    consumers = ctest_example_consumers(root, paths, ctest_catalog)
    consumer_paths: dict[str, list[str]] = {}
    for path, names in consumers.items():
        for name in names:
            consumer_paths.setdefault(name, []).append(path)
    entries: list[dict[str, Any]] = []
    specifications: list[dict[str, Any]] = []
    for path in paths:
        relative = PurePosixPath(path)
        base = max((str(PurePosixPath(p)) for p in example_roots if relative.is_relative_to(p)), key=len)
        kind = "source" if relative.suffix == ".styio" else "wrapper" if relative.suffix in SHELL_SUFFIXES else "unknown"
        support = bool({part.lower() for part in relative.relative_to(base).parts[:-1]} & SUPPORT_DIRECTORIES) or relative.suffix.lower() in SUPPORT_SUFFIXES
        if support:
            kind = "support"
        identities = {path}
        for name in consumers[path]:
            identities.update(consumer_paths[name])
        entry: dict[str, Any] = {
            "path": path, "kind": kind, "status": "unverified", "consumers": consumers[path],
            "reasons": [], "identity_paths": sorted(identities),
        }
        entries.append(entry)
        if support:
            entry.update(status="support", reasons=["Generated, reference, documentation, or fixture/support file; not auto-executed"])
            continue
        if consumers[path]:
            entry.update(status="registered", reasons=["Actual CTest command references this file; registration is not a passing result"])
            continue
        if kind == "wrapper":
            entry["reasons"].append("Unregistered shell wrapper needs invocation arguments and environment context")
            continue
        if kind != "source":
            entry["reasons"].append("Unknown file type/applicability; report-only until invocation context is established")
            continue
        if not _regular_file(root, path):
            entry["reasons"].append("Missing, non-regular, or symlink source needs explicit invocation context")
            continue
        try:
            with (root / path).open("rb") as handle:
                raw = handle.read(MAX_SOURCE_BYTES + 1)
            if len(raw) > MAX_SOURCE_BYTES:
                raise ValueError("source exceeds bounded discovery size")
            source = raw.decode("utf-8")
        except (OSError, UnicodeError, ValueError) as error:
            entry["reasons"].append(f"Cannot inspect source safely: {error}")
            continue
        entry["reasons"].extend(_source_advisories(source))
        output_paths = sorted(_fixture_candidates(path, base, "expected", (".out",)) & tracked)
        input_paths = sorted(_fixture_candidates(path, base, "input", (".stdin", ".in")) & tracked)
        identities.update(output_paths + input_paths)
        entry["identity_paths"] = sorted(identities)
        if not output_paths:
            entry["reasons"].append("Missing tracked same-basename .out expected-output fixture")
        if len(output_paths) > 1:
            entry["reasons"].append("Ambiguous expected-output fixtures require explicit invocation context")
        if len(input_paths) > 1:
            entry["reasons"].append("Ambiguous stdin fixtures require explicit invocation context")
        if re.search(r"@\s*stdin\b", source) and not input_paths:
            entry["reasons"].append("Source may require stdin but has no tracked .stdin/.in fixture")
        if any(not _regular_file(root, fixture) for fixture in output_paths + input_paths):
            entry["reasons"].append("Missing, non-regular, or symlink fixture requires explicit invocation context")
        # Flat root fixtures are only unambiguous for unique source basenames.
        flat = {str(PurePosixPath(base) / directory / (relative.stem + suffix)) for directory, suffix in (("expected", ".out"), ("input", ".in"), ("input", ".stdin"))}
        same_name = [other for other in paths if other != path and PurePosixPath(other).stem == relative.stem and other.endswith(".styio") and PurePosixPath(other).is_relative_to(base)]
        if same_name and (set(output_paths + input_paths) & flat):
            entry["reasons"].append("Shared flat fixture basename is ambiguous across multiple sources")
        if entry["reasons"]:
            continue
        entry.update(status="fallback", reasons=["Tracked stdout golden and bounded stdio-only invocation convention; execution remains pending"])
        specifications.append({
            "id": "auto-example:" + path,
            "command": [compiler, "--file", path],
            "cwd": str(root), "stdin_file": input_paths[0] if input_paths else None,
            "expected_output_file": output_paths[0], "timeout": timeout,
            "identity_paths": entry["identity_paths"],
        })
    return {
        "entries": entries, "specifications": specifications,
        "tracked_paths": paths,
        "registered_paths": [entry["path"] for entry in entries if entry["status"] == "registered"],
        "unverified_paths": [entry["path"] for entry in entries if entry["status"] == "unverified"],
        "support_paths": [entry["path"] for entry in entries if entry["status"] == "support"],
        "consumer_paths": dict(sorted(consumer_paths.items())),
        "counts": {"tracked": len(paths), **{status: sum(entry["status"] == status for entry in entries) for status in ("registered", "fallback", "unverified", "support")}},
    }
