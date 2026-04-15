#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Sequence

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
COLLECTION_DIRS = [
    DOCS,
    DOCS / "rollups",
    DOCS / "archive",
    DOCS / "archive" / "history",
    DOCS / "archive" / "review",
    DOCS / "design",
    DOCS / "specs",
    DOCS / "review",
    DOCS / "plans",
    DOCS / "for-ide",
    DOCS / "for_spio",
    DOCS / "assets",
    DOCS / "assets" / "workflow",
    DOCS / "assets" / "templates",
    DOCS / "adr",
    DOCS / "history",
    DOCS / "milestones",
]
PURPOSE_RE = re.compile(r"^(?:\*\*Purpose:\*\*|\[EN\] Purpose:)\s+.+$", re.M)
LAST_UPDATED_RE = re.compile(r"^(?:\*\*Last updated:\*\*|\[EN\] Last updated:)\s+[0-9]{4}-[0-9]{2}-[0-9]{2}\s*$", re.M)
LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
DATE_FILE_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}\.md$")
ADR_FILE_RE = re.compile(r"^ADR-[0-9]{4}-[a-z0-9-]+\.md$")
MILESTONE_DIR_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}$")
MILESTONE_FILE_RE = re.compile(r"^(00-Milestone-Index|M[0-9]+-[A-Za-z0-9-]+)\.md$")
BENCHMARK_REPORT_SUMMARY_RE = re.compile(r"^benchmark/reports/[^/]+/summary\.md$")
APPROVED_TEST_DOC_NAMES = {"README.md", "REGRESSION-TEMPLATE.md"}


@dataclass(frozen=True)
class ManifestEntry:
    path: Path
    status: str
    reason: str
    character_count: int
    word_count: int

    @property
    def rel_path(self) -> str:
        return self.path.as_posix()


def strip_code_fences(text: str) -> str:
    return re.sub(r"```.*?```", "", text, flags=re.S)


def looks_like_repo_link(target: str) -> bool:
    if target.startswith("./") or target.startswith("../") or target.endswith("/"):
        return True
    return Path(target).suffix in {
        ".md",
        ".txt",
        ".out",
        ".err",
        ".json",
        ".jsonl",
        ".toml",
        ".yml",
        ".yaml",
        ".sh",
        ".py",
        ".hpp",
        ".h",
        ".cpp",
        ".c",
        ".ll",
        ".styio",
        ".png",
        ".jpg",
        ".jpeg",
        ".svg",
    }


def has_prefix(path: Path, prefix: Path) -> bool:
    prefix_parts = prefix.parts
    return path.parts[: len(prefix_parts)] == prefix_parts


def is_archive_doc(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    return has_prefix(rel, Path("docs/archive"))


def skip_link_check(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    if not is_archive_doc(path):
        return False
    if rel.name in {"INDEX.md", "ARCHIVE-LEDGER.md"}:
        return False
    if rel.parent == Path("docs/archive"):
        return False
    if rel.parent in {Path("docs/archive/history"), Path("docs/archive/review")} and rel.name == "README.md":
        return False
    return True


def iter_docs() -> Iterable[Path]:
    return sorted(DOCS.rglob("*.md"))


def git_tracked_markdown_paths() -> List[Path]:
    proc = subprocess.run(
        ["git", "ls-files", "-z", "--", "*.md"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        raise RuntimeError(f"git ls-files failed: {detail}")
    paths: List[Path] = []
    for raw in proc.stdout.split("\0"):
        if not raw:
            continue
        rel = Path(raw)
        if (ROOT / rel).exists():
            paths.append(rel)
    return sorted(paths, key=lambda path: path.as_posix())


def git_worktree_markdown_paths() -> List[Path]:
    proc = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", "*.md"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        raise RuntimeError(f"git ls-files worktree scan failed: {detail}")
    paths: List[Path] = []
    for raw in proc.stdout.split("\0"):
        if not raw:
            continue
        rel = Path(raw)
        if (ROOT / rel).exists():
            paths.append(rel)
    return sorted({path.as_posix(): path for path in paths}.values(), key=lambda path: path.as_posix())


def filesystem_markdown_paths() -> List[Path]:
    paths: List[Path] = []
    for path in ROOT.rglob("*.md"):
        rel = path.relative_to(ROOT)
        if ".git" in rel.parts:
            continue
        paths.append(rel)
    return sorted(paths, key=lambda candidate: candidate.as_posix())


def classify_markdown(path: Path) -> ManifestEntry:
    rel = path.as_posix()
    character_count, word_count = measure_markdown(path)

    if rel == "README.md":
        return ManifestEntry(path, "valid", "repository root entry document", character_count, word_count)
    if BENCHMARK_REPORT_SUMMARY_RE.match(rel):
        return ManifestEntry(
            path,
            "invalid",
            "generated benchmark report summary; promote conclusions into tracked docs instead",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("docs")):
        return ManifestEntry(path, "valid", "approved docs/ collection", character_count, word_count)
    if has_prefix(path, Path("benchmark")):
        return ManifestEntry(path, "valid", "approved benchmark documentation", character_count, word_count)
    if has_prefix(path, Path("templates")):
        return ManifestEntry(path, "valid", "approved reusable template documentation", character_count, word_count)
    if rel == "grammar/tree-sitter-styio/README.md":
        return ManifestEntry(
            path,
            "valid",
            "approved repository-local grammar documentation",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("tests")):
        if path.name in APPROVED_TEST_DOC_NAMES:
            return ManifestEntry(path, "valid", "approved test workflow documentation", character_count, word_count)
        return ManifestEntry(
            path,
            "invalid",
            "markdown under tests/ must be a README.md or approved template",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("src")):
        return ManifestEntry(
            path,
            "invalid",
            "markdown under src/ is outside approved documentation locations",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("grammar")):
        return ManifestEntry(
            path,
            "invalid",
            "markdown under grammar/ is only approved for grammar/tree-sitter-styio/README.md",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("build")) or has_prefix(path, Path("build-codex")):
        return ManifestEntry(
            path,
            "invalid",
            "build output or vendored dependency markdown is not a project document",
            character_count,
            word_count,
        )
    return ManifestEntry(
        path,
        "invalid",
        "markdown path is outside every approved repository documentation location",
        character_count,
        word_count,
    )


def is_han_character(ch: str) -> bool:
    code = ord(ch)
    return (
        0x3400 <= code <= 0x4DBF
        or 0x4E00 <= code <= 0x9FFF
        or 0xF900 <= code <= 0xFAFF
        or 0x20000 <= code <= 0x2A6DF
        or 0x2A700 <= code <= 0x2B73F
        or 0x2B740 <= code <= 0x2B81F
        or 0x2B820 <= code <= 0x2CEAF
        or 0x2CEB0 <= code <= 0x2EBEF
        or 0x30000 <= code <= 0x3134F
        or 0x2F800 <= code <= 0x2FA1F
    )


def is_ascii_word_char(ch: str) -> bool:
    return ch.isascii() and (ch.isalnum() or ch == "_")


def count_words(text: str) -> int:
    total = 0
    index = 0
    while index < len(text):
        ch = text[index]
        if ch.isspace():
            index += 1
            continue
        if is_han_character(ch):
            total += 1
            index += 1
            continue
        if is_ascii_word_char(ch):
            total += 1
            index += 1
            while index < len(text) and is_ascii_word_char(text[index]):
                index += 1
            continue
        total += 1
        index += 1
    return total


def measure_markdown(path: Path) -> tuple[int, int]:
    text = (ROOT / path).read_text(encoding="utf-8")
    return len(text), count_words(text)


def collect_manifest(source: str) -> List[ManifestEntry]:
    if source == "worktree":
        paths = git_worktree_markdown_paths()
    elif source == "git":
        paths = git_tracked_markdown_paths()
    elif source == "filesystem":
        paths = filesystem_markdown_paths()
    else:
        raise ValueError(f"unknown manifest source: {source}")
    return [classify_markdown(path) for path in paths]


def filter_manifest(entries: Sequence[ManifestEntry], status: str) -> List[ManifestEntry]:
    if status == "all":
        return list(entries)
    return [entry for entry in entries if entry.status == status]


def make_tree(entries: Sequence[ManifestEntry]) -> Dict[str, object]:
    root: Dict[str, object] = {
        "name": ".",
        "type": "directory",
        "path": ".",
        "children": {},
    }

    for entry in entries:
        node = root
        segments = entry.path.parts
        prefix: List[str] = []
        for segment in segments[:-1]:
            prefix.append(segment)
            children = node["children"]  # type: ignore[index]
            child = children.get(segment)  # type: ignore[union-attr]
            if child is None:
                child = {
                    "name": segment,
                    "type": "directory",
                    "path": "/".join(prefix),
                    "children": {},
                }
                children[segment] = child  # type: ignore[index]
            node = child
        children = node["children"]  # type: ignore[index]
        children[segments[-1]] = {
            "name": segments[-1],
            "type": "file",
            "path": entry.rel_path,
            "status": entry.status,
            "reason": entry.reason,
        }

    return freeze_tree(root)


def freeze_tree(node: Dict[str, object]) -> Dict[str, object]:
    if node["type"] != "directory":
        return dict(node)

    raw_children = list(node["children"].values())  # type: ignore[index]
    frozen_children = [freeze_tree(child) for child in raw_children]
    frozen_children.sort(key=lambda child: (child["type"] != "directory", str(child["name"]).lower()))

    return {
        "name": node["name"],
        "type": node["type"],
        "path": node["path"],
        "children": frozen_children,
    }


def render_tree(tree: Dict[str, object], annotate_reason: bool) -> str:
    lines = ["."]

    def visit(children: Sequence[Dict[str, object]], prefix: str) -> None:
        for index, child in enumerate(children):
            is_last = index == len(children) - 1
            branch = "└── " if is_last else "├── "
            label = str(child["name"])
            if child["type"] == "directory":
                label = f"{label}/"
            elif annotate_reason:
                label = f"{label}  [{child['reason']}]"
            lines.append(f"{prefix}{branch}{label}")
            if child["type"] == "directory":
                extension = "    " if is_last else "│   "
                visit(child["children"], prefix + extension)  # type: ignore[arg-type]

    visit(tree["children"], "")  # type: ignore[arg-type]
    return "\n".join(lines)


def render_list(entries: Sequence[ManifestEntry], include_reason: bool) -> str:
    if not entries:
        return "(empty)"
    lines: List[str] = []
    for entry in entries:
        if include_reason:
            lines.append(f"- {entry.rel_path}: {entry.reason}")
        else:
            lines.append(f"- {entry.rel_path}")
    return "\n".join(lines)


def aggregate_text_stats(entries: Sequence[ManifestEntry]) -> Dict[str, int]:
    return {
        "characters": sum(entry.character_count for entry in entries),
        "words": sum(entry.word_count for entry in entries),
    }


def manifest_payload(entries: Sequence[ManifestEntry], filtered: Sequence[ManifestEntry], source: str, status: str) -> Dict[str, object]:
    counts = {
        "total": len(entries),
        "valid": sum(1 for entry in entries if entry.status == "valid"),
        "invalid": sum(1 for entry in entries if entry.status == "invalid"),
        "selected": len(filtered),
    }
    valid_entries = [entry for entry in entries if entry.status == "valid"]
    invalid_entries = [entry for entry in entries if entry.status == "invalid"]
    return {
        "root": ROOT.as_posix(),
        "source": source,
        "status_filter": status,
        "counts": counts,
        "text_stats": {
            "total": aggregate_text_stats(entries),
            "valid": aggregate_text_stats(valid_entries),
            "invalid": aggregate_text_stats(invalid_entries),
            "selected": aggregate_text_stats(filtered),
        },
        "entries": [
            {
                "path": entry.rel_path,
                "status": entry.status,
                "reason": entry.reason,
                "character_count": entry.character_count,
                "word_count": entry.word_count,
            }
            for entry in filtered
        ],
        "tree": make_tree(filtered),
    }


def write_output(text: str, output: Path | None) -> None:
    if output is None:
        print(text)
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")


def run_manifest(status: str, fmt: str, source: str, output: Path | None) -> int:
    entries = collect_manifest(source)
    filtered = filter_manifest(entries, status)
    selected_text_stats = aggregate_text_stats(filtered)

    if fmt == "json":
        payload = manifest_payload(entries, filtered, source, status)
        rendered = json.dumps(payload, ensure_ascii=False, indent=2)
    elif fmt == "tree":
        counts = {
            "total": len(entries),
            "valid": sum(1 for entry in entries if entry.status == "valid"),
            "invalid": sum(1 for entry in entries if entry.status == "invalid"),
            "selected": len(filtered),
        }
        header = [
            f"source: {source}",
            f"status: {status}",
            f"selected: {counts['selected']} / total: {counts['total']} (valid={counts['valid']}, invalid={counts['invalid']})",
            f"selected_text: characters={selected_text_stats['characters']}, words={selected_text_stats['words']}",
            "",
        ]
        rendered = "\n".join(header) + render_tree(make_tree(filtered), annotate_reason=status != "valid")
    elif fmt == "list":
        counts = {
            "total": len(entries),
            "valid": sum(1 for entry in entries if entry.status == "valid"),
            "invalid": sum(1 for entry in entries if entry.status == "invalid"),
            "selected": len(filtered),
        }
        header = [
            f"source: {source}",
            f"status: {status}",
            f"selected: {counts['selected']} / total: {counts['total']} (valid={counts['valid']}, invalid={counts['invalid']})",
            f"selected_text: characters={selected_text_stats['characters']}, words={selected_text_stats['words']}",
            "",
        ]
        rendered = "\n".join(header) + render_list(filtered, include_reason=status != "valid")
    else:
        raise ValueError(f"unsupported format: {fmt}")

    write_output(rendered, output)
    return 0


def check_collection_dirs(errors: List[str]) -> None:
    for directory in COLLECTION_DIRS:
        readme = directory / "README.md"
        index = directory / "INDEX.md"
        if not readme.exists():
            errors.append(f"missing required README.md: {readme.relative_to(ROOT)}")
        if not index.exists():
            errors.append(f"missing required INDEX.md: {index.relative_to(ROOT)}")
        if readme.exists():
            head = readme.read_text(encoding="utf-8")[:2000]
            if "INDEX.md" not in head:
                errors.append(f"README does not point to INDEX.md: {readme.relative_to(ROOT)}")


def check_naming(errors: List[str]) -> None:
    for path in (DOCS / "history").glob("*.md"):
        if path.name not in {"README.md", "INDEX.md"} and not DATE_FILE_RE.match(path.name):
            errors.append(f"invalid history filename: {path.relative_to(ROOT)}")
    for path in (DOCS / "adr").glob("*.md"):
        if path.name not in {"README.md", "INDEX.md"} and not ADR_FILE_RE.match(path.name):
            errors.append(f"invalid ADR filename: {path.relative_to(ROOT)}")
    for path in (DOCS / "milestones").iterdir():
        if not path.is_dir():
            continue
        if not MILESTONE_DIR_RE.match(path.name):
            errors.append(f"invalid milestone directory name: {path.relative_to(ROOT)}")
            continue
        for child in path.glob("*.md"):
            if not MILESTONE_FILE_RE.match(child.name):
                errors.append(f"invalid milestone file name: {child.relative_to(ROOT)}")


def check_metadata(errors: List[str]) -> None:
    for path in iter_docs():
        text = path.read_text(encoding="utf-8")
        head = "\n".join(text.splitlines()[:16])
        if not PURPOSE_RE.search(head):
            errors.append(f"missing top-level Purpose metadata: {path.relative_to(ROOT)}")
        if not LAST_UPDATED_RE.search(head):
            errors.append(f"missing top-level Last updated metadata: {path.relative_to(ROOT)}")


def check_links(errors: List[str]) -> None:
    for path in iter_docs():
        if skip_link_check(path):
            continue
        text = strip_code_fences(path.read_text(encoding="utf-8"))
        for raw_target in LINK_RE.findall(text):
            target = raw_target.strip()
            if not target or target.startswith("http://") or target.startswith("https://") or target.startswith("mailto:") or target.startswith("#"):
                continue
            target = target.split("#", 1)[0]
            if not target:
                continue
            if re.search(r"\s", target):
                continue
            if not looks_like_repo_link(target):
                continue
            resolved = (path.parent / target).resolve()
            if not resolved.exists():
                errors.append(f"broken relative link in {path.relative_to(ROOT)} -> {raw_target}")


def check_generated_indexes(errors: List[str]) -> None:
    proc = subprocess.run(
        [sys.executable, str(ROOT / "scripts/docs-index.py"), "--check"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        errors.append(f"generated indexes are stale: {detail}")


def check_lifecycle(errors: List[str]) -> None:
    proc = subprocess.run(
        [sys.executable, str(ROOT / "scripts/docs-lifecycle.py"), "validate"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        errors.append(f"docs lifecycle validation failed: {detail}")


def run_audit() -> int:
    errors: List[str] = []
    check_collection_dirs(errors)
    check_naming(errors)
    check_metadata(errors)
    check_links(errors)
    check_generated_indexes(errors)
    check_lifecycle(errors)

    if errors:
        print("docs audit failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("docs audit passed")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit docs structure, metadata, links, and generated indexes, or print a repository-wide Markdown manifest."
    )
    parser.add_argument(
        "--manifest",
        choices=["valid", "invalid", "all"],
        help="Print a repository-wide Markdown manifest instead of running the docs audit.",
    )
    parser.add_argument(
        "--format",
        choices=["tree", "list", "json"],
        default="tree",
        help="Output format for --manifest. Defaults to tree.",
    )
    parser.add_argument(
        "--source",
        choices=["worktree", "git", "filesystem"],
        default="worktree",
        help="Manifest source. worktree scans tracked + unignored Markdown, git scans tracked Markdown only, and filesystem scans every Markdown file under the worktree.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Optional output file for --manifest. When omitted, output goes to stdout.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.manifest:
        return run_manifest(status=args.manifest, fmt=args.format, source=args.source, output=args.output)
    return run_audit()


if __name__ == "__main__":
    raise SystemExit(main())
