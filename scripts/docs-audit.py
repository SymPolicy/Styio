#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Iterable, List

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
COLLECTION_DIRS = [
    DOCS,
    DOCS / "design",
    DOCS / "specs",
    DOCS / "review",
    DOCS / "plans",
    DOCS / "for-ide",
    DOCS / "assets",
    DOCS / "assets" / "workflow",
    DOCS / "assets" / "templates",
    DOCS / "adr",
    DOCS / "history",
    DOCS / "milestones",
]
PURPOSE_RE = re.compile(r"^\*\*Purpose:\*\*\s+.+$", re.M)
LAST_UPDATED_RE = re.compile(r"^\*\*Last updated:\*\*\s+[0-9]{4}-[0-9]{2}-[0-9]{2}\s*$", re.M)
LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
DATE_FILE_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}\.md$")
ADR_FILE_RE = re.compile(r"^ADR-[0-9]{4}-[a-z0-9-]+\.md$")
MILESTONE_DIR_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}$")
MILESTONE_FILE_RE = re.compile(r"^(00-Milestone-Index|M[0-9]+-[A-Za-z0-9-]+)\.md$")


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


def iter_docs() -> Iterable[Path]:
    return sorted(DOCS.rglob("*.md"))


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


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit docs structure, metadata, links, and generated indexes.")
    parser.parse_args()

    errors: List[str] = []
    check_collection_dirs(errors)
    check_naming(errors)
    check_metadata(errors)
    check_links(errors)
    check_generated_indexes(errors)

    if errors:
        print("docs audit failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("docs audit passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
