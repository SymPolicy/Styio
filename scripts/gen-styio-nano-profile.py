#!/usr/bin/env python3

import argparse
import pathlib
import sys
import tomllib


ALLOWED_DICT_BACKENDS = ("ordered-hash", "linear")


def read_bool(table, key, default):
    value = table.get(key, default)
    if not isinstance(value, bool):
        raise ValueError(f"{key} must be true or false")
    return value


def cmake_bool(value):
    return "ON" if value else "OFF"


def escape_cmake_string(value):
    return value.replace("\\", "\\\\").replace("\"", "\\\"")


def make_definition(name, value):
    if isinstance(value, bool):
        numeric = "1" if value else "0"
        return f"{name}={numeric}"
    return f'{name}=\\"{escape_cmake_string(value)}\\"'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--cmake-out", required=True)
    args = parser.parse_args()

    input_path = pathlib.Path(args.input).resolve()
    cmake_out = pathlib.Path(args.cmake_out)

    with input_path.open("rb") as fh:
      data = tomllib.load(fh)

    profile = data.get("profile", {})
    compiler = data.get("compiler", {})
    build = data.get("build", {})
    runtime = data.get("runtime", {})

    if not isinstance(profile, dict) or not isinstance(compiler, dict) or not isinstance(build, dict) or not isinstance(runtime, dict):
        raise ValueError("profile, compiler, build, and runtime sections must be tables")

    profile_name = profile.get("name", input_path.stem)
    if not isinstance(profile_name, str) or not profile_name.strip():
        raise ValueError("profile.name must be a non-empty string")
    profile_name = profile_name.strip()

    dict_backends = runtime.get("dict_backends", ["ordered-hash"])
    if not isinstance(dict_backends, list) or not dict_backends:
        raise ValueError("runtime.dict_backends must be a non-empty list")

    normalized_backends = []
    for item in dict_backends:
        if not isinstance(item, str):
            raise ValueError("runtime.dict_backends entries must be strings")
        name = item.strip().lower()
        if name not in ALLOWED_DICT_BACKENDS:
            raise ValueError(f"unsupported runtime.dict_backends entry: {item}")
        if name not in normalized_backends:
            normalized_backends.append(name)

    defs = [
        make_definition("STYIO_NANO_BUILD", True),
        make_definition("STYIO_NANO_PROFILE_NAME", profile_name),
        make_definition("STYIO_NANO_PROFILE_SOURCE", str(input_path)),
        make_definition("STYIO_NANO_ENABLE_MACHINE_INFO", read_bool(compiler, "machine_info", True)),
        make_definition("STYIO_NANO_ENABLE_DEBUG_CLI", read_bool(compiler, "debug_cli", False)),
        make_definition("STYIO_NANO_ENABLE_AST_DUMP_CLI", read_bool(compiler, "ast_dump_cli", False)),
        make_definition("STYIO_NANO_ENABLE_STYIO_IR_DUMP_CLI", read_bool(compiler, "styio_ir_dump_cli", False)),
        make_definition("STYIO_NANO_ENABLE_LLVM_IR_DUMP_CLI", read_bool(compiler, "llvm_ir_dump_cli", False)),
        make_definition("STYIO_NANO_ENABLE_LEGACY_PARSER", read_bool(compiler, "legacy_parser", False)),
        make_definition("STYIO_NANO_ENABLE_PARSER_SHADOW_COMPARE", read_bool(compiler, "parser_shadow_compare", False)),
        make_definition("STYIO_NANO_INCLUDE_PIPELINE_CHECK", read_bool(build, "include_pipeline_check", False)),
        make_definition("STYIO_NANO_ENABLE_DICT_BACKEND_ORDERED_HASH", "ordered-hash" in normalized_backends),
        make_definition("STYIO_NANO_ENABLE_DICT_BACKEND_LINEAR", "linear" in normalized_backends),
    ]

    cmake_lines = [
        f'set(STYIO_NANO_PROFILE_NAME "{escape_cmake_string(profile_name)}")',
        f'set(STYIO_NANO_PROFILE_SOURCE "{escape_cmake_string(str(input_path))}")',
        f'set(STYIO_NANO_INCLUDE_PIPELINE_CHECK {cmake_bool(read_bool(build, "include_pipeline_check", False))})',
        "set(STYIO_NANO_COMPILE_DEFINITIONS",
    ]
    for entry in defs:
        cmake_lines.append(f'  "{entry}"')
    cmake_lines.append(")")

    cmake_out.parent.mkdir(parents=True, exist_ok=True)
    cmake_out.write_text("\n".join(cmake_lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[styio-nano-profile] {exc}", file=sys.stderr)
        sys.exit(1)
