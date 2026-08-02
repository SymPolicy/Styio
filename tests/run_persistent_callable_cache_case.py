#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import tempfile


EXPECTED_STDOUT = "7\ncached\n"
STATS_SCHEMA = "styio.callable-cache-stats.v1"


def command(
    styio: pathlib.Path,
    source: pathlib.Path,
    cache: pathlib.Path | None = None,
    *extra: str,
) -> list[str]:
    result = [
        str(styio),
        "--parser-engine=nightly",
        f"--file={source}",
    ]
    if cache is not None:
        result.extend(
            [
                f"--callable-cache-dir={cache}",
                "--callable-cache-stats",
            ]
        )
    result.extend(extra)
    return result


def invoke(
    styio: pathlib.Path,
    source: pathlib.Path,
    cache: pathlib.Path | None = None,
    *extra: str,
) -> tuple[subprocess.CompletedProcess[str], dict[str, object] | None]:
    completed = subprocess.run(
        command(styio, source, cache, *extra),
        cwd=source.parent,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise AssertionError(
            "cached compilation failed with exit "
            f"{completed.returncode}\nstdout:\n{completed.stdout}\n"
            f"stderr:\n{completed.stderr}"
        )
    if completed.stdout != EXPECTED_STDOUT:
        raise AssertionError(
            "cache changed program output\n"
            f"actual:\n{completed.stdout}"
        )
    if cache is None:
        if completed.stderr:
            raise AssertionError(
                "cache-disabled compilation emitted diagnostics\n"
                f"{completed.stderr}"
            )
        return completed, None

    lines = completed.stderr.splitlines()
    if len(lines) != 1:
        raise AssertionError(
            "explicit cache statistics mode must emit exactly one "
            f"path-free JSON object, got:\n{completed.stderr}"
        )
    try:
        stats = json.loads(lines[0])
    except json.JSONDecodeError as error:
        raise AssertionError(
            f"cache statistics are not JSON: {error}"
        ) from error
    if stats.get("schema") != STATS_SCHEMA:
        raise AssertionError(
            f"unexpected cache statistics schema: {stats!r}"
        )
    if str(cache) in lines[0] or "cache_dir" in stats:
        raise AssertionError(
            "cache statistics exposed a machine-specific cache path"
        )
    timing = stats.get("timing_ns")
    if not isinstance(timing, dict):
        raise AssertionError("cache statistics omitted timing_ns")
    for key in ("hashing", "lookup", "verification", "materialization"):
        value = timing.get(key)
        if not isinstance(value, int) or value < 0:
            raise AssertionError(
                f"cache timing {key!r} is not a nonnegative integer"
            )
    return completed, stats


def artifacts(cache: pathlib.Path) -> list[pathlib.Path]:
    return sorted(cache.rglob("*.styobj"))


def require_counts(
    stats: dict[str, object],
    **expected: int,
) -> None:
    for key, value in expected.items():
        if stats.get(key) != value:
            raise AssertionError(
                f"cache statistic {key!r}: expected {value}, "
                f"got {stats.get(key)!r}; full stats={stats!r}"
            )


def require_no_temporary_files(cache: pathlib.Path) -> None:
    leftovers = [
        path
        for path in cache.rglob("*")
        if path.is_file() and ".tmp-" in path.name
    ]
    if leftovers:
        raise AssertionError(
            "atomic cache writes left temporary files behind"
        )


def warm_hit_corruption(
    styio: pathlib.Path,
    source: pathlib.Path,
) -> None:
    with tempfile.TemporaryDirectory(
        prefix="styio-callable-cache-warm-"
    ) as raw_cache:
        cache = pathlib.Path(raw_cache)
        _, cold = invoke(styio, source, cache)
        assert cold is not None
        require_counts(
            cold,
            lookups=3,
            hits=0,
            misses=3,
            corruptions=0,
            writes=3,
        )
        if len(artifacts(cache)) != 3:
            raise AssertionError(
                "cold compilation did not persist one object per "
                "reachable specialization"
            )

        _, warm = invoke(styio, source, cache)
        assert warm is not None
        require_counts(
            warm,
            lookups=3,
            hits=3,
            misses=0,
            corruptions=0,
            writes=0,
        )

        victim = artifacts(cache)[0]
        victim.write_bytes(b"corrupt")
        _, repaired = invoke(styio, source, cache)
        assert repaired is not None
        require_counts(
            repaired,
            lookups=3,
            hits=2,
            misses=1,
            corruptions=1,
            writes=1,
        )
        if len(artifacts(cache)) != 3:
            raise AssertionError(
                "corrupt cache entry was not atomically replaced"
            )
        require_no_temporary_files(cache)


def dependency_and_backend_identity(
    styio: pathlib.Path,
    source: pathlib.Path,
) -> None:
    with tempfile.TemporaryDirectory(
        prefix="styio-callable-cache-key-"
    ) as temporary:
        workspace = pathlib.Path(temporary)
        cache = workspace / "cache"
        base = workspace / "program.styio"
        base.write_text(source.read_text(encoding="utf-8"), encoding="utf-8")
        invoke(styio, base, cache)

        changed = base.read_text(encoding="utf-8").replace(
            "(value) => value\n",
            "(value) => value + 0\n",
            1,
        )
        if changed == base.read_text(encoding="utf-8"):
            raise AssertionError(
                "dependency invalidation fixture did not change"
            )
        base.write_text(changed, encoding="utf-8")
        _, dependency_change = invoke(styio, base, cache)
        assert dependency_change is not None
        require_counts(
            dependency_change,
            lookups=3,
            hits=1,
            misses=2,
            writes=2,
        )

        _, backend_change = invoke(
            styio,
            base,
            cache,
            "--dict-impl=linear",
        )
        assert backend_change is not None
        require_counts(
            backend_change,
            lookups=3,
            hits=0,
            misses=3,
            writes=3,
        )
        namespace_root = (
            cache
            / "styio"
            / "callable-specializations"
            / "v1"
        )
        namespaces = sorted(
            path
            for path in namespace_root.iterdir()
            if path.is_dir()
        )
        if len(namespaces) != 2:
            raise AssertionError(
                "backend ABI change did not select a separate cache "
                "namespace"
            )


def retention_limits(
    styio: pathlib.Path,
    source: pathlib.Path,
) -> None:
    with tempfile.TemporaryDirectory(
        prefix="styio-callable-cache-files-"
    ) as raw_cache:
        cache = pathlib.Path(raw_cache)
        _, stats = invoke(
            styio,
            source,
            cache,
            "--callable-cache-max-files=2",
        )
        assert stats is not None
        if len(artifacts(cache)) != 2 or int(stats["evictions"]) < 1:
            raise AssertionError(
                "file-count retention did not deterministically bound "
                "the active namespace"
            )

    with tempfile.TemporaryDirectory(
        prefix="styio-callable-cache-bytes-"
    ) as raw_cache:
        cache = pathlib.Path(raw_cache)
        _, stats = invoke(
            styio,
            source,
            cache,
            "--callable-cache-max-bytes=1",
        )
        assert stats is not None
        if artifacts(cache) or int(stats["evictions"]) != 3:
            raise AssertionError(
                "byte retention did not evict oversized artifacts"
            )

    with tempfile.TemporaryDirectory(
        prefix="styio-callable-cache-age-"
    ) as raw_cache:
        cache = pathlib.Path(raw_cache)
        invoke(styio, source, cache)
        for artifact in artifacts(cache):
            os.utime(artifact, (1, 1))
        _, stats = invoke(
            styio,
            source,
            cache,
            "--callable-cache-max-age-seconds=1",
        )
        assert stats is not None
        require_counts(
            stats,
            lookups=3,
            hits=0,
            misses=3,
            writes=3,
        )
        if int(stats["evictions"]) < 3:
            raise AssertionError(
                "age retention did not evict expired artifacts"
            )


def concurrent_atomic_writes(
    styio: pathlib.Path,
    source: pathlib.Path,
) -> None:
    with tempfile.TemporaryDirectory(
        prefix="styio-callable-cache-race-"
    ) as raw_cache:
        cache = pathlib.Path(raw_cache)
        processes = [
            subprocess.Popen(
                command(styio, source, cache),
                cwd=source.parent,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            for _ in range(2)
        ]
        for process in processes:
            stdout, stderr = process.communicate(timeout=30)
            if process.returncode != 0 or stdout != EXPECTED_STDOUT:
                raise AssertionError(
                    "concurrent cache writer failed\n"
                    f"stdout:\n{stdout}\nstderr:\n{stderr}"
                )
            try:
                stats = json.loads(stderr)
            except json.JSONDecodeError as error:
                raise AssertionError(
                    "concurrent writer did not emit valid statistics"
                ) from error
            if stats.get("schema") != STATS_SCHEMA:
                raise AssertionError(
                    "concurrent writer emitted the wrong stats schema"
                )

        _, final = invoke(styio, source, cache)
        assert final is not None
        require_counts(
            final,
            lookups=3,
            hits=3,
            misses=0,
            corruptions=0,
            writes=0,
        )
        if len(artifacts(cache)) != 3:
            raise AssertionError(
                "concurrent writes did not converge to one artifact per key"
            )
        require_no_temporary_files(cache)


def invalid_limits_are_cli_errors(
    styio: pathlib.Path,
    source: pathlib.Path,
) -> None:
    result = subprocess.run(
        command(
            styio,
            source,
            None,
            "--callable-cache-max-files=2",
        ),
        cwd=source.parent,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 6:
        raise AssertionError(
            "cache limits without an explicit cache root must be a CLI error"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--styio", required=True, type=pathlib.Path)
    parser.add_argument("--source", required=True, type=pathlib.Path)
    args = parser.parse_args()

    styio = args.styio.resolve()
    source = args.source.resolve()

    invoke(styio, source)
    warm_hit_corruption(styio, source)
    dependency_and_backend_identity(styio, source)
    retention_limits(styio, source)
    concurrent_atomic_writes(styio, source)
    invalid_limits_are_cli_errors(styio, source)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, subprocess.TimeoutExpired) as error:
        print(error)
        raise SystemExit(1)
