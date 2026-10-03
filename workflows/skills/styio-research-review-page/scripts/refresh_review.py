#!/usr/bin/env python3
"""Refresh explicit fact sources, then render even when extraction is stale."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

import build_review
import extract_facts


def run(config_path: Path) -> dict:
    base = config_path.resolve().parent
    config = json.loads(config_path.read_text(encoding="utf-8"))
    if not isinstance(config, dict) or config.get("schema_version") != 1:
        raise ValueError("unsupported refresh configuration")
    review_input = base / config["review_input"]
    output = base / config["output"]
    input_root = review_input.resolve().parent
    sources = config.get("sources")
    if not isinstance(sources, list) or not sources:
        raise ValueError("refresh sources must be a nonempty list")
    reserved = {review_input.resolve(), config_path.resolve()}
    destinations = set()
    jobs = []
    for source in sources:
        values = {}
        for key in ("facts_output", "status_output"):
            raw = Path(source[key])
            target = base / raw
            if (raw.is_absolute() or not target.resolve().is_relative_to(input_root)
                    or target.suffix != ".json"
                    or any(p.is_symlink() for p in [target, *target.parents])
                    or target.resolve() in reserved | destinations):
                raise ValueError("unsafe or overlapping fact output")
            destinations.add(target.resolve())
            values[key] = target
        if not isinstance(source.get("fetch", False), bool):
            raise ValueError("fetch must be a boolean")
        jobs.append(argparse.Namespace(repo=base / source["repo"],
                    repository_url=source["repository_url"], ref=source["ref"],
                    classification=source["classification"], fetch=source.get("fetch", False),
                    output=values["facts_output"], status_output=values["status_output"]))
    # Callers must serialize runs sharing an input/output directory. Each source
    # remains explicit; this entrypoint does not discover or publish repositories.
    failures = sum(extract_facts.refresh(job) != 0 for job in jobs)
    rendered = build_review.build(review_input, output)
    return {"status": "stale" if failures else "passed", "failed_sources": failures,
            "pages": rendered["pages"], "published": False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = run(args.config)
    except (ValueError, KeyError, TypeError, OSError) as error:
        # Do not serialize arbitrary exception text containing local paths.
        print(json.dumps({"status": "failed", "error": "refresh_or_render_failed", "published": False}))
        return 2
    print(json.dumps(result))
    return 1 if result["failed_sources"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
