#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import subprocess
import tempfile


SYMBOL_PATTERN = re.compile(
    r"^define\b.*@(__styio_mono_(.+?)_([0-9a-f]{64}))\(",
    re.MULTILINE,
)
CALL_PATTERN = re.compile(
    r"\bcall\b.*@(__styio_mono_.+?_[0-9a-f]{64})\("
)


def llvm_ir(styio: pathlib.Path, source: pathlib.Path) -> str:
    result = subprocess.run(
        [
            str(styio),
            "--parser-engine=nightly",
            "--llvm-ir",
            f"--file={source}",
        ],
        cwd=source.parent,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise AssertionError(
            f"LLVM IR compilation failed for {source.name}: "
            f"{result.returncode}\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    if result.stderr:
        raise AssertionError(
            f"LLVM IR compilation produced stderr for {source.name}:\n"
            f"{result.stderr}"
        )
    return result.stdout


def symbol_map(llvm: str) -> dict[str, set[str]]:
    symbols: dict[str, set[str]] = {}
    definitions: set[str] = set()
    for full_symbol, source_name, digest in SYMBOL_PATTERN.findall(llvm):
        if full_symbol in definitions:
            raise AssertionError(
                f"specialization {full_symbol} was defined more than once"
            )
        definitions.add(full_symbol)
        symbols.setdefault(source_name, set()).add(digest)

    called = set(CALL_PATTERN.findall(llvm))
    unresolved = called - definitions
    if unresolved:
        raise AssertionError(
            "specialization calls lack a single local owner: "
            + ", ".join(sorted(unresolved))
        )
    return symbols


def require_imported_concrete_reachability(
    styio: pathlib.Path,
    fixtures: pathlib.Path,
) -> None:
    with tempfile.TemporaryDirectory(
        prefix="styio-callable-specialization-"
    ) as temporary:
        workspace = pathlib.Path(temporary) / "callable_specialization"
        shutil.copytree(fixtures, workspace)
        module = workspace / "modules/reachable.styio"
        interface = module.with_suffix(".styioi")
        publication = subprocess.run(
            [
                str(styio),
                "--parser-engine=nightly",
                f"--file={module}",
                "--module-id=modules/reachable",
                f"--emit-module-interface={interface}",
            ],
            cwd=workspace,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if publication.returncode != 0 or publication.stderr:
            raise AssertionError(
                "reachable concrete module publication failed\n"
                f"stdout:\n{publication.stdout}\n"
                f"stderr:\n{publication.stderr}"
            )

        imported_ir = llvm_ir(
            styio,
            workspace / "d03_imported_concrete_root.styio",
        )
        imported_symbols = symbol_map(imported_ir)
        if len(imported_symbols.get("identity", set())) != 1:
            raise AssertionError(
                "reachable imported concrete body must instantiate its "
                "generic dependency exactly once"
            )
        for required in ("concrete_bridge", "private_concrete"):
            if not re.search(
                rf"^define\b.*@{required}\(",
                imported_ir,
                re.MULTILINE,
            ):
                raise AssertionError(
                    f"reachable imported concrete definition {required} "
                    "was not emitted"
                )
        if re.search(
            r"^define\b.*@unused_concrete\(",
            imported_ir,
            re.MULTILINE,
        ):
            raise AssertionError(
                "unreachable imported concrete definition was emitted"
            )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--styio", required=True, type=pathlib.Path)
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    args = parser.parse_args()

    styio = args.styio.resolve()
    fixtures = args.fixtures.resolve()
    first_source = fixtures / "t01_reachable_instances.styio"
    reordered_source = fixtures / "t02_reordered_instances.styio"
    dependency_base_source = fixtures / "d01_dependency_base.styio"
    dependency_changed_source = fixtures / "d02_dependency_changed.styio"

    first = symbol_map(llvm_ir(styio, first_source))
    repeated = symbol_map(llvm_ir(styio, first_source))
    reordered = symbol_map(llvm_ir(styio, reordered_source))
    if first != repeated or first != reordered:
        raise AssertionError(
            "content-addressed specialization symbols changed across "
            "repeat compilation or call-order changes"
        )

    if len(first.get("identity", set())) != 2:
        raise AssertionError(
            "identity must have exactly i64 and string specializations"
        )
    if len(first.get("composed", set())) != 1:
        raise AssertionError(
            "composed must have exactly one reachable specialization"
        )
    if first.get("unused"):
        raise AssertionError(
            "unreachable generic callable unexpectedly produced code"
        )

    dependency_base = symbol_map(
        llvm_ir(styio, dependency_base_source)
    )
    dependency_changed = symbol_map(
        llvm_ir(styio, dependency_changed_source)
    )
    if dependency_base.get("composed") == dependency_changed.get("composed"):
        raise AssertionError(
            "caller specialization identity did not change after a "
            "reachable callee body changed"
        )
    require_imported_concrete_reachability(styio, fixtures)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(error)
        raise SystemExit(1)
