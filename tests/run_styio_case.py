#!/usr/bin/env python3
"""Portable CTest helpers for Styio CLI golden and artifact tests."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def _read_bytes(path: Path) -> bytes:
    return path.read_bytes()


def _norm_newlines(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n")


def _fail(message: str) -> int:
    print(f"[styio-test] {message}", file=sys.stderr)
    return 1


def _env_with(overrides: list[str]) -> dict[str, str]:
    env = os.environ.copy()
    for item in overrides:
        if "=" not in item:
            raise ValueError(f"--env expects KEY=VALUE, got {item!r}")
        key, value = item.split("=", 1)
        env[key] = value
    return env


def _remove_path(path: Path) -> None:
    if path.is_dir():
        shutil.rmtree(path, ignore_errors=True)
    else:
        path.unlink(missing_ok=True)


def _materialize_source(source: Path, replacements: list[list[str]]) -> tuple[Path, tempfile.TemporaryDirectory[str] | None]:
    if not replacements:
        return source, None

    temp = tempfile.TemporaryDirectory(prefix="styio-case-")
    rendered = Path(temp.name) / source.name
    text = source.read_text(encoding="utf-8")
    for old, new in replacements:
        text = text.replace(old, new)
    rendered.write_text(text, encoding="utf-8", newline="\n")
    return rendered, temp


def _compare(label: str, actual: bytes, expected_path: Path) -> int:
    expected = _read_bytes(expected_path)
    if _norm_newlines(actual) == _norm_newlines(expected):
        return 0
    print(f"[styio-test] {label} mismatch against {expected_path}", file=sys.stderr)
    print("[styio-test] --- actual ---", file=sys.stderr)
    sys.stderr.buffer.write(_norm_newlines(actual))
    if not actual.endswith(b"\n"):
        print(file=sys.stderr)
    print("[styio-test] --- expected ---", file=sys.stderr)
    sys.stderr.buffer.write(_norm_newlines(expected))
    if not expected.endswith(b"\n"):
        print(file=sys.stderr)
    return 1


def _run_process(argv: list[str], cwd: Path, env: dict[str, str], stdin: bytes | None) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        argv,
        cwd=str(cwd),
        env=env,
        input=stdin,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def command_run(args: argparse.Namespace) -> int:
    source, temp = _materialize_source(args.source, args.replace_literal)
    for raw in args.cleanup:
        _remove_path(Path(raw))
    try:
        stdin = args.stdin_text.encode("utf-8") if args.stdin_text is not None else None
        if args.stdin_file:
            stdin = _read_bytes(args.stdin_file)
        result = _run_process(
            [str(args.styio), "--file", str(source)],
            args.cwd,
            _env_with(args.env),
            stdin,
        )

        if args.expect_failure:
            if result.returncode == 0:
                return _fail("expected command to fail, but it exited 0")
        elif result.returncode != args.expect_exit:
            sys.stderr.buffer.write(result.stderr)
            return _fail(f"expected exit {args.expect_exit}, got {result.returncode}")

        if args.expect_stdout and not args.discard_stdout:
            rc = _compare("stdout", result.stdout, args.expect_stdout)
            if rc != 0:
                return rc
        if args.expect_stderr:
            rc = _compare("stderr", result.stderr, args.expect_stderr)
            if rc != 0:
                return rc
        if args.stderr_contains_file:
            needle = _norm_newlines(_read_bytes(args.stderr_contains_file)).rstrip(b"\n")
            haystack = _norm_newlines(result.stderr)
            if needle not in haystack:
                sys.stderr.buffer.write(result.stderr)
                return _fail(f"stderr did not contain {args.stderr_contains_file}")
        for path, expected in args.expect_file_text:
            actual_path = Path(path)
            if not actual_path.exists():
                return _fail(f"expected output file was not created: {actual_path}")
            if _norm_newlines(actual_path.read_bytes()) != _norm_newlines(expected.encode("utf-8")):
                return _fail(f"output file text mismatch: {actual_path}")
        for path, expected_path in args.expect_file_equals:
            actual_path = Path(path)
            if not actual_path.exists():
                return _fail(f"expected output file was not created: {actual_path}")
            rc = _compare(f"file {actual_path}", actual_path.read_bytes(), Path(expected_path))
            if rc != 0:
                return rc
        return 0
    finally:
        if temp is not None:
            temp.cleanup()
        for raw in args.cleanup:
            _remove_path(Path(raw))


def command_build_run(args: argparse.Namespace) -> int:
    source, temp = _materialize_source(args.source, args.replace_literal)
    try:
        args.output.unlink(missing_ok=True)
        if args.profile_out:
            args.profile_out.unlink(missing_ok=True)

        build = _run_process(
            [str(args.styio), "build", str(source), "-o", str(args.output)],
            args.cwd,
            _env_with(args.env),
            None,
        )
        if build.returncode != 0:
            sys.stderr.buffer.write(build.stdout)
            sys.stderr.buffer.write(build.stderr)
            return _fail(f"styio build failed with exit {build.returncode}")
        if not args.output.is_file():
            return _fail(f"native artifact was not produced: {args.output}")
        if os.name != "nt" and not os.access(args.output, os.X_OK):
            return _fail(f"native artifact is not executable: {args.output}")

        stdin = args.stdin_text.encode("utf-8") if args.stdin_text is not None else None
        if args.stdin_file:
            stdin = _read_bytes(args.stdin_file)
        env_items = list(args.env)
        if args.profile_out:
            env_items.append(f"STYIO_NATIVE_PROFILE_OUT={args.profile_out}")
        run = _run_process([str(args.output)], args.cwd, _env_with(env_items), stdin)
        if run.returncode != args.expect_exit:
            sys.stderr.buffer.write(run.stdout)
            sys.stderr.buffer.write(run.stderr)
            return _fail(f"native artifact expected exit {args.expect_exit}, got {run.returncode}")
        if args.expect_stdout:
            rc = _compare("artifact stdout", run.stdout, args.expect_stdout)
            if rc != 0:
                return rc
        if args.profile_out:
            if not args.profile_out.is_file():
                return _fail(f"profile was not created: {args.profile_out}")
            profile = args.profile_out.read_text(encoding="utf-8", errors="replace")
            for needle in args.profile_contains:
                if needle not in profile:
                    return _fail(f"profile missing expected text {needle!r}: {args.profile_out}")
        return 0
    finally:
        if temp is not None:
            temp.cleanup()


def command_calc(args: argparse.Namespace) -> int:
    if not re.fullmatch(r"[0-9\s+*/().-]+", args.expression):
        return _fail(f"unsupported characters in expression: {args.expression}")
    with tempfile.TemporaryDirectory(prefix="styio-calc-") as temp:
        source = Path(temp) / "program.styio"
        source.write_text(f"({args.expression}) -> @stdout\n", encoding="utf-8", newline="\n")
        result = _run_process(
            [str(args.styio), "--file", str(source)],
            args.cwd,
            _env_with(args.env),
            None,
        )
        if result.returncode != 0:
            sys.stderr.buffer.write(result.stderr)
            return _fail(f"calculator expression failed with exit {result.returncode}")
        return _compare("calculator stdout", result.stdout, args.expect_stdout)


def command_bootstrap_scope(args: argparse.Namespace) -> int:
    script = args.repo_root / "scripts" / "bootstrap-dev-env.sh"
    docs = args.repo_root / "docs" / "BUILD-AND-DEV-ENV.md"
    script_text = script.read_text(encoding="utf-8", errors="replace")
    docs_text = docs.read_text(encoding="utf-8", errors="replace")
    required = [
        "installs dependencies only",
        "does not configure, build, test",
    ]
    for needle in required:
        if needle not in script_text:
            return _fail(f"bootstrap help text missing {needle!r}")
    if "Bootstrap scope:" not in docs_text:
        return _fail("docs/BUILD-AND-DEV-ENV.md is missing Bootstrap scope")

    bash = shutil.which("bash")
    if bash and os.name != "nt":
        syntax = subprocess.run([bash, "-n", str(script)], cwd=str(args.repo_root), check=False)
        if syntax.returncode != 0:
            return _fail("bootstrap-dev-env.sh failed bash -n")
        help_result = subprocess.run(
            [bash, str(script), "--help"],
            cwd=str(args.repo_root),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if help_result.returncode != 0:
            sys.stderr.buffer.write(help_result.stderr)
            return _fail("bootstrap-dev-env.sh --help failed")
        help_text = help_result.stdout.decode("utf-8", errors="replace")
        for needle in required:
            if needle not in help_text:
                return _fail(f"bootstrap --help missing {needle!r}")
        plan_result = subprocess.run(
            [bash, str(script), "--print-plan"],
            cwd=str(args.repo_root),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if plan_result.returncode != 0:
            sys.stderr.buffer.write(plan_result.stderr)
            return _fail("bootstrap-dev-env.sh --print-plan failed")
        plan_text = plan_result.stdout.decode("utf-8", errors="replace")
        if sys.platform == "darwin":
            plan_needles = ["Host: macOS", "Homebrew", "llvm@18", "node@24"]
        else:
            plan_needles = ["Host: Debian/Ubuntu", "apt", "clang-18", "llvm-18-dev"]
        for needle in plan_needles:
            if needle not in plan_text:
                return _fail(f"bootstrap --print-plan missing {needle!r}")
    return 0


def add_common_run_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--styio", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--cwd", type=Path, required=True)
    parser.add_argument("--stdin-file", type=Path)
    parser.add_argument("--stdin-text")
    parser.add_argument("--expect-stdout", type=Path)
    parser.add_argument("--env", action="append", default=[])
    parser.add_argument("--replace-literal", nargs=2, action="append", default=[])


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    run = sub.add_parser("run")
    add_common_run_args(run)
    run.add_argument("--expect-stderr", type=Path)
    run.add_argument("--stderr-contains-file", type=Path)
    run.add_argument("--expect-file-text", nargs=2, action="append", default=[])
    run.add_argument("--expect-file-equals", nargs=2, action="append", default=[])
    run.add_argument("--cleanup", action="append", default=[])
    run.add_argument("--expect-exit", type=int, default=0)
    run.add_argument("--expect-failure", action="store_true")
    run.add_argument("--discard-stdout", action="store_true")
    run.set_defaults(func=command_run)

    build_run = sub.add_parser("build-run")
    add_common_run_args(build_run)
    build_run.add_argument("--output", type=Path, required=True)
    build_run.add_argument("--profile-out", type=Path)
    build_run.add_argument("--profile-contains", action="append", default=[])
    build_run.add_argument("--expect-exit", type=int, default=0)
    build_run.set_defaults(func=command_build_run)

    calc = sub.add_parser("calc")
    calc.add_argument("--styio", type=Path, required=True)
    calc.add_argument("--cwd", type=Path, required=True)
    calc.add_argument("--expression", required=True)
    calc.add_argument("--expect-stdout", type=Path, required=True)
    calc.add_argument("--env", action="append", default=[])
    calc.set_defaults(func=command_calc)

    bootstrap = sub.add_parser("bootstrap-scope")
    bootstrap.add_argument("--repo-root", type=Path, required=True)
    bootstrap.set_defaults(func=command_bootstrap_scope)

    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except Exception as exc:
        print(f"[styio-test] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
