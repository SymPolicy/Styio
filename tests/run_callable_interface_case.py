#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile


def run(command: list[str], cwd: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def emit_interface(
    styio: pathlib.Path,
    workspace: pathlib.Path,
    source: str,
    module_id: str,
) -> subprocess.CompletedProcess[str]:
    source_path = workspace / source
    interface_path = source_path.with_suffix(".styioi")
    return run(
        [
            str(styio),
            "--parser-engine=nightly",
            f"--file={source_path}",
            f"--module-id={module_id}",
            f"--emit-module-interface={interface_path}",
        ],
        workspace,
    )


def require_success(result: subprocess.CompletedProcess[str], operation: str) -> None:
    if result.returncode == 0:
        return
    raise AssertionError(
        f"{operation} failed with {result.returncode}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )


def require_failure_contains(
    result: subprocess.CompletedProcess[str],
    expected: str,
    operation: str,
) -> None:
    if result.returncode == 0:
        raise AssertionError(
            f"{operation} unexpectedly succeeded\nstdout:\n{result.stdout}"
        )
    if expected not in result.stderr:
        raise AssertionError(
            f"{operation} did not contain expected diagnostic {expected!r}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--styio", required=True, type=pathlib.Path)
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    parser.add_argument(
        "--mode",
        required=True,
        choices=[
            "run",
            "private",
            "cycle",
            "stale-source",
            "stale-schema",
            "stale-dependency",
            "missing",
            "uninstantiated-body",
            "invalid-module-id",
        ],
    )
    args = parser.parse_args()
    styio = args.styio.resolve()
    fixtures = args.fixtures.resolve()

    with tempfile.TemporaryDirectory(prefix="styio-callable-interface-") as temp:
        workspace = pathlib.Path(temp) / "callable_interfaces"
        shutil.copytree(fixtures, workspace)

        if args.mode == "cycle":
            result = run(
                [
                    str(styio),
                    "--parser-engine=nightly",
                    f"--file={workspace / 'e02_cross_module_cycle.styio'}",
                ],
                workspace,
            )
            expected = (
                workspace / "expected/e02_cross_module_cycle.err"
            ).read_text(encoding="utf-8").strip()
            require_failure_contains(result, expected, "cross-module cycle check")
            return 0

        if args.mode == "missing":
            result = run(
                [
                    str(styio),
                    "--parser-engine=nightly",
                    f"--file={workspace / 'e06_missing_interface.styio'}",
                ],
                workspace,
            )
            expected = (
                workspace / "expected/e06_missing_interface.err"
            ).read_text(encoding="utf-8").strip()
            require_failure_contains(result, expected, "missing interface check")
            return 0

        if args.mode == "uninstantiated-body":
            result = emit_interface(
                styio,
                workspace,
                "modules/invalid.styio",
                "modules/invalid",
            )
            expected = (
                workspace / "expected/e07_uninstantiated_body.err"
            ).read_text(encoding="utf-8").strip()
            require_failure_contains(
                result,
                expected,
                "uninstantiated exported body validation",
            )
            return 0

        if args.mode == "invalid-module-id":
            result = emit_interface(
                styio,
                workspace,
                "modules/core.styio",
                "modules.core",
            )
            expected = (
                workspace / "expected/e08_invalid_module_id.err"
            ).read_text(encoding="utf-8").strip()
            require_failure_contains(
                result,
                expected,
                "canonical module id check",
            )
            return 0

        if args.mode == "stale-dependency":
            require_success(
                emit_interface(
                    styio,
                    workspace,
                    "dependency/leaf.styio",
                    "leaf",
                ),
                "leaf interface publication",
            )
            require_success(
                emit_interface(
                    styio,
                    workspace,
                    "dependency/outer.styio",
                    "dependency/outer",
                ),
                "outer interface publication",
            )
            leaf = workspace / "dependency/leaf.styio"
            leaf.write_text(
                "@export { leaf_identity }\n\n"
                "# leaf_identity := (value) => value + value\n",
                encoding="utf-8",
            )
            require_success(
                emit_interface(
                    styio,
                    workspace,
                    "dependency/leaf.styio",
                    "leaf",
                ),
                "updated leaf interface publication",
            )
            result = run(
                [
                    str(styio),
                    "--parser-engine=nightly",
                    f"--file={workspace / 'e05_stale_dependency.styio'}",
                ],
                workspace,
            )
            expected = (
                workspace / "expected/e05_stale_dependency.err"
            ).read_text(encoding="utf-8").strip()
            require_failure_contains(result, expected, "stale dependency check")
            return 0

        require_success(
            emit_interface(
                styio,
                workspace,
                "modules/core.styio",
                "modules/core",
            ),
            "core interface publication",
        )

        if args.mode == "stale-source":
            core = workspace / "modules/core.styio"
            core.write_text(
                core.read_text(encoding="utf-8") + "\n",
                encoding="utf-8",
            )
            result = run(
                [
                    str(styio),
                    "--parser-engine=nightly",
                    f"--file={workspace / 'e03_stale_source.styio'}",
                ],
                workspace,
            )
            expected = (
                workspace / "expected/e03_stale_source.err"
            ).read_text(encoding="utf-8").strip()
            require_failure_contains(result, expected, "stale source check")
            return 0

        if args.mode == "stale-schema":
            interface_path = workspace / "modules/core.styioi"
            payload = json.loads(interface_path.read_text(encoding="utf-8"))
            payload["schema_version"] = 999
            interface_path.write_text(
                json.dumps(payload, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            result = run(
                [
                    str(styio),
                    "--parser-engine=nightly",
                    f"--file={workspace / 'e04_stale_schema.styio'}",
                ],
                workspace,
            )
            expected = (
                workspace / "expected/e04_stale_schema.err"
            ).read_text(encoding="utf-8").strip()
            require_failure_contains(result, expected, "stale schema check")
            return 0

        entry = (
            "t01_downstream_specialization.styio"
            if args.mode == "run"
            else "e01_private_callable.styio"
        )
        result = run(
            [
                str(styio),
                "--parser-engine=nightly",
                f"--file={workspace / entry}",
            ],
            workspace,
        )
        if args.mode == "run":
            require_success(result, "downstream specialization")
            expected = (
                workspace / "expected/t01_downstream_specialization.out"
            ).read_text(encoding="utf-8")
            if result.stdout != expected:
                raise AssertionError(
                    "downstream specialization stdout mismatch\n"
                    f"expected:\n{expected}\nactual:\n{result.stdout}"
                )
            if result.stderr:
                raise AssertionError(
                    "downstream specialization produced stderr:\n"
                    + result.stderr
                )
            return 0

        expected = (
            workspace / "expected/e01_private_callable.err"
        ).read_text(encoding="utf-8").strip()
        require_failure_contains(result, expected, "private callable check")
        return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
