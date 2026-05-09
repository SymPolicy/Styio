#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from datetime import date
from pathlib import Path

from docs_config import collection_index_meta

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "scripts" / "docs_config.py"
TODAY = date.today().isoformat()


def as_repo_path(raw: str) -> Path:
    path = Path(raw)
    if path.is_absolute():
        try:
            return path.resolve().relative_to(ROOT)
        except ValueError as exc:
            raise ValueError(f"path is outside repo: {raw}") from exc
    return path


def render_markdown(title: str, purpose: str, body: str) -> str:
    text = f"# {title}\n\n**Purpose:** {purpose}\n\n**Last updated:** {TODAY}\n"
    body = body.strip("\n")
    if body:
        text += f"\n{body}\n"
    else:
        text += "\n"
    return text


def render_collection_readme(title: str, purpose: str) -> str:
    body = """## Scope

1. Store compact documents for this syntax/design slice.
2. Link to owning SSOT documents instead of duplicating long semantic rules.

## Naming Rules

1. Use stable, searchable filenames.
2. Keep generated inventories in `INDEX.md`.

## Inventory

See [INDEX.md](./INDEX.md)."""
    return render_markdown(title, purpose, body)


def write_new(path: Path, content: str, force: bool) -> bool:
    if path.exists() and not force:
        raise FileExistsError(f"{path.relative_to(ROOT)} already exists; use --force to replace")
    before = path.read_text(encoding="utf-8") if path.exists() else None
    path.parent.mkdir(parents=True, exist_ok=True)
    if before == content:
        return False
    path.write_text(content, encoding="utf-8")
    return True


def register_collection(rel_dir: Path, index_title: str, index_purpose: str) -> bool:
    rel = rel_dir.as_posix()
    if rel in collection_index_meta():
        return False

    config = CONFIG.read_text(encoding="utf-8")
    start = config.index("COLLECTION_SPECS")
    end = config.index("\n)\n\n\n", start)
    line = f"    CollectionSpec({rel!r}, {index_title!r}, {index_purpose!r}),\n"
    CONFIG.write_text(config[:end] + line + config[end:], encoding="utf-8")
    return True


def run(args: list[str]) -> None:
    proc = subprocess.run(args, cwd=ROOT, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(args)}")


def read_body(args: argparse.Namespace) -> str:
    if args.body and args.body_file:
        raise ValueError("use only one of --body or --body-file")
    if args.body_file:
        return Path(args.body_file).read_text(encoding="utf-8")
    return args.body or ""


def scaffold(args: argparse.Namespace) -> int:
    target_rel = as_repo_path(args.path)
    if target_rel.suffix != ".md":
        raise ValueError("target path must be a Markdown file")
    if not target_rel.parts or target_rel.parts[0] != "docs":
        raise ValueError("docs-scaffold only manages files under docs/")

    parent_rel = target_rel.parent
    meta = collection_index_meta()
    touched: list[Path] = []

    if parent_rel.as_posix() not in meta:
        if not args.collection_title or not args.collection_purpose:
            raise ValueError(
                f"{parent_rel.as_posix()} is not a registered collection; "
                "provide --collection-title and --collection-purpose"
            )
        readme = ROOT / parent_rel / "README.md"
        if write_new(readme, render_collection_readme(args.collection_title, args.collection_purpose), args.force):
            touched.append(readme.relative_to(ROOT))
        index_title = args.collection_index_title or f"{args.collection_title} Index"
        index_purpose = args.collection_index_purpose or (
            f"Provide the generated inventory for `{parent_rel.as_posix()}/`; "
            "document boundaries live in [README.md](./README.md)."
        )
        if register_collection(parent_rel, index_title, index_purpose):
            touched.append(CONFIG.relative_to(ROOT))

    target = ROOT / target_rel
    if write_new(target, render_markdown(args.title, args.purpose, read_body(args)), args.force):
        touched.append(target_rel)

    if not args.no_index:
        run([sys.executable, str(ROOT / "scripts" / "docs-index.py"), "--write"])

    if args.run_audit:
        run([sys.executable, str(ROOT / "scripts" / "docs-audit.py")])

    if touched:
        print("scaffolded:")
        for path in touched:
            print(f"  - {path.as_posix()}")
    else:
        print("no content changes")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a docs Markdown file and register a new docs collection when needed."
    )
    parser.add_argument("path", help="Target Markdown path under docs/, for example docs/design/foo/BAR.md")
    parser.add_argument("--title", required=True, help="Document H1 title")
    parser.add_argument("--purpose", required=True, help="Top-level Purpose metadata")
    parser.add_argument("--body", default="", help="Optional Markdown body after metadata")
    parser.add_argument("--body-file", help="Read optional Markdown body from a file")
    parser.add_argument("--collection-title", help="README title when the parent docs directory is a new collection")
    parser.add_argument("--collection-purpose", help="README Purpose metadata for a new collection")
    parser.add_argument("--collection-index-title", help="Generated INDEX title for a new collection")
    parser.add_argument("--collection-index-purpose", help="Generated INDEX Purpose metadata for a new collection")
    parser.add_argument("--force", action="store_true", help="Replace an existing target/README file")
    parser.add_argument("--no-index", action="store_true", help="Do not regenerate docs INDEX.md files")
    parser.add_argument("--run-audit", action="store_true", help="Run docs audit after scaffolding")
    return parser.parse_args()


def main() -> int:
    try:
        return scaffold(parse_args())
    except Exception as exc:
        print(f"docs-scaffold failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
