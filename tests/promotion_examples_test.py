#!/usr/bin/env python3
"""No compiler, CMake configure, or network is required for these tests."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("promotion_examples", ROOT / "scripts/promotion_examples.py")
assert SPEC is not None and SPEC.loader is not None
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)


class ExampleDiscoveryTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="promotion examples ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.git("init", "-q")

    def git(self, *arguments: str) -> None:
        subprocess.run(["git", "-C", str(self.root), *arguments], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def write(self, name: str, content: str, *, tracked: bool = True) -> None:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        if tracked:
            self.git("add", "--", name)

    def discover(self, tests: list[dict] | None = None, **kwargs: object) -> dict:
        return examples.discover_examples(self.root, {"tests": tests or []}, **kwargs)

    def entry(self, report: dict, path: str) -> dict:
        return next(item for item in report["entries"] if item["path"] == path)

    def golden(self, name: str = "example/hello.styio", source: str = '"hello" -> @stdout\n') -> None:
        self.write(name, source)
        self.write(str(Path(name).with_suffix(".out")), "hello\n")

    def test_staged_new_sources_are_automatically_discovered_without_a_commit(self) -> None:
        self.golden()
        self.write("example/untracked.styio", '"ignore" -> @stdout', tracked=False)
        report = self.discover()
        self.assertEqual(["example/hello.out", "example/hello.styio"], report["tracked_paths"])
        self.assertEqual({"tracked": 2, "registered": 0, "fallback": 1, "unverified": 0, "support": 1}, report["counts"])
        spec = report["specifications"][0]
        self.assertEqual("auto-example:example/hello.styio", spec["id"])
        self.assertEqual(["build/default/bin/styio", "--file", "example/hello.styio"], spec["command"])
        self.assertEqual("example/hello.out", spec["expected_output_file"])
        self.assertIsNone(spec["stdin_file"])
        json.dumps(report)  # Public API is serializable without a custom encoder.

    def test_spaces_and_staged_rename_keep_argv_and_identity_paths(self) -> None:
        self.golden("example/old name.styio")
        self.git("mv", "example/old name.styio", "example/new name.styio")
        self.git("mv", "example/old name.out", "example/new name.out")
        report = self.discover(compiler="/compiler path/styio", timeout=4.5)
        spec = report["specifications"][0]
        self.assertEqual("auto-example:example/new name.styio", spec["id"])
        self.assertEqual(["/compiler path/styio", "--file", "example/new name.styio"], spec["command"])
        self.assertEqual(["example/new name.out", "example/new name.styio"], spec["identity_paths"])
        self.assertEqual(4.5, spec["timeout"])
        self.assertNotIn("example/old name.styio", report["tracked_paths"])

    def test_direct_ctest_consumer_is_not_duplicated(self) -> None:
        self.golden()
        report = self.discover([{"name": "hello", "command": ["styio", "--file", str(self.root / "example/hello.styio")]}])
        self.assertEqual([], report["specifications"])
        entry = self.entry(report, "example/hello.styio")
        self.assertEqual("registered", entry["status"])
        self.assertEqual(["hello"], entry["consumers"])
        self.assertIn("not a passing result", entry["reasons"][0])

    def test_shell_catalog_maps_exact_paths_with_spaces_and_adjacent_operators(self) -> None:
        self.golden("example/space name.styio")
        command = f'"/bin/styio" --file "{self.root}/example/space name.styio"|cmp -s - "{self.root}/example/space name.out";'
        report = self.discover([{"name": "space", "command": ["bash", "-c", command]}])
        self.assertEqual([], report["specifications"])
        self.assertEqual(["space"], self.entry(report, "example/space name.styio")["consumers"])
        self.assertEqual(["example/space name.out", "example/space name.styio"], report["consumer_paths"]["space"])
        self.assertEqual(report["consumer_paths"]["space"], self.entry(report, "example/space name.styio")["identity_paths"])

    def test_ctest_working_directory_and_equals_arguments(self) -> None:
        self.golden()
        tests = [{"name": "relative", "command": ["styio", "--file=../example/hello.styio"], "properties": [{"name": "WORKING_DIRECTORY", "value": str(self.root / "build")}]}]
        report = self.discover(tests)
        self.assertEqual(["relative"], self.entry(report, "example/hello.styio")["consumers"])

    def test_git_nul_discovery_and_shell_quoting_preserve_newlines_and_quotes(self) -> None:
        name = "example/quoted 'name'\nnext.styio"
        self.golden(name)
        command = "styio --file " + shlex.quote(str(self.root / name))
        report = self.discover([{"name": "odd_path", "command": ["bash", "-c", command]}])
        self.assertIn(name, report["tracked_paths"])
        self.assertEqual(["odd_path"], self.entry(report, name)["consumers"])

    def test_similar_names_and_other_repositories_do_not_create_consumers(self) -> None:
        self.golden()
        tests = [{"name": "unrelated", "command": ["styio", str(self.root / "example/hello.styio.backup"), "/other/repo/example/hello.styio"]}]
        self.assertEqual("fallback", self.entry(self.discover(tests), "example/hello.styio")["status"])

    def test_wrapper_is_distinct_from_same_basename_source(self) -> None:
        self.golden("example/calculator.styio", "(1 + 2) -> @stdout\n")
        self.write("example/calculator.sh", "#!/bin/sh\n# generates a different source\n")
        command = f'STYIO_BIN=/bin/styio "{self.root}/example/calculator.sh" "1 + 2" | cmp -s - "{self.root}/example/calculator.out"'
        report = self.discover([{"name": "calculator", "command": ["bash", "-c", command]}])
        self.assertEqual("registered", self.entry(report, "example/calculator.sh")["status"])
        self.assertEqual("fallback", self.entry(report, "example/calculator.styio")["status"])
        self.assertEqual(["auto-example:example/calculator.styio"], [spec["id"] for spec in report["specifications"]])

    def test_missing_stdin_and_output_and_unknown_file_are_report_only(self) -> None:
        self.golden("example/read.styio", "x <- @stdin\nx -> @stdout\n")
        self.write("example/no_output.styio", '"hello" -> @stdout\n')
        self.write("example/tool.sh", "#!/bin/sh\n")
        self.write("example/mystery.xyz", "unknown\n")
        report = self.discover()
        self.assertEqual([], report["specifications"])
        self.assertEqual(4, len(report["unverified_paths"]))
        for path, reason in (("read.styio", "stdin"), ("no_output.styio", "expected-output"), ("tool.sh", "invocation"), ("mystery.xyz", "Unknown")):
            self.assertTrue(any(reason in item for item in self.entry(report, "example/" + path)["reasons"]))

    def test_stdin_fixture_is_optional_for_stdio_free_source_and_required_for_input(self) -> None:
        self.golden("example/echo.styio", "@stdin >> #(line) => { >_(line) }\n")
        self.write("example/echo.stdin", "hello\n")
        spec = self.discover()["specifications"][0]
        self.assertEqual("example/echo.stdin", spec["stdin_file"])
        self.assertEqual(["example/echo.out", "example/echo.stdin", "example/echo.styio"], spec["identity_paths"])
        self.write("example/echo.stdin", "", tracked=False)
        self.assertEqual("example/echo.stdin", self.discover()["specifications"][0]["stdin_file"])

    def test_source_and_fixture_paths_enable_change_invalidation(self) -> None:
        self.golden()
        self.write("example/hello.in", "input\n")
        before = self.discover()["specifications"][0]
        self.write("example/hello.styio", '"changed" -> @stdout\n', tracked=False)
        self.write("example/hello.out", "changed\n", tracked=False)
        after = self.discover()["specifications"][0]
        self.assertEqual(before["id"], after["id"])
        self.assertEqual({"example/hello.styio", "example/hello.out", "example/hello.in"}, set(after["identity_paths"]))

    def test_expected_root_and_mirrored_stdin_conventions(self) -> None:
        self.write("example/algorithms/sort.styio", "x <- @stdin\nx -> @stdout\n")
        self.write("example/expected/sort.out", "[1,2]\n")
        self.write("example/input/algorithms/sort.in", "[2,1]\n")
        spec = self.discover()["specifications"][0]
        self.assertEqual("example/expected/sort.out", spec["expected_output_file"])
        self.assertEqual("example/input/algorithms/sort.in", spec["stdin_file"])

    def test_untracked_expected_output_is_not_silently_used(self) -> None:
        self.write("example/hello.styio", '"hello" -> @stdout\n')
        self.write("example/hello.out", "hello\n", tracked=False)
        self.assertEqual("unverified", self.entry(self.discover(), "example/hello.styio")["status"])

    def test_ambiguous_fixtures_are_advisory(self) -> None:
        self.golden()
        self.write("example/expected/hello.out", "hello\n")
        self.write("example/hello.in", "one\n")
        self.write("example/hello.stdin", "two\n")
        entry = self.entry(self.discover(), "example/hello.styio")
        self.assertEqual("unverified", entry["status"])
        self.assertTrue(any("Ambiguous expected" in reason for reason in entry["reasons"]))
        self.assertTrue(any("Ambiguous stdin" in reason for reason in entry["reasons"]))

    def test_shared_flat_fixture_basename_requires_context(self) -> None:
        self.write("example/a/hello.styio", '"hello" -> @stdout\n')
        self.write("example/b/hello.styio", '"hello" -> @stdout\n')
        self.write("example/expected/hello.out", "hello\n")
        report = self.discover()
        self.assertEqual([], report["specifications"])
        self.assertEqual(2, len(report["unverified_paths"]))

    def test_generated_reference_and_support_sources_are_never_executed(self) -> None:
        for directory in ("generated", "reference", "support", "fixtures"):
            self.golden(f"example/{directory}/hello.styio")
        report = self.discover()
        self.assertEqual([], report["specifications"])
        self.assertEqual(8, report["counts"]["support"])

    def test_external_network_credentials_and_unknown_resources_are_advisory(self) -> None:
        for name, source in (("native", "@extern(c) => {}"), ("network", '"https://example.org" -> @stdout'), ("secret", 'api_key = "sample"'), ("file", 'x <- @{"data"}'), ("unknown", "@new_resource"), ("http_call", "http_get(endpoint)")):
            with self.subTest(name=name):
                self.golden(f"example/{name}.styio", source)
        report = self.discover()
        self.assertEqual([], report["specifications"])
        self.assertEqual(6, report["counts"]["unverified"])

    def test_symlink_source_and_fixture_are_advisory(self) -> None:
        self.golden()
        (self.root / "example/hello.out").unlink()
        (self.root / "example/hello.out").symlink_to("hello.styio")
        self.assertEqual("unverified", self.entry(self.discover(), "example/hello.styio")["status"])
        (self.root / "example/hello.styio").unlink()
        (self.root / "example/hello.styio").symlink_to("hello.out")
        self.assertEqual("unverified", self.entry(self.discover(), "example/hello.styio")["status"])

    def test_missing_worktree_source_and_invalid_utf8_are_advisory(self) -> None:
        self.golden()
        (self.root / "example/hello.styio").unlink()
        self.assertEqual("unverified", self.entry(self.discover(), "example/hello.styio")["status"])
        (self.root / "example/hello.styio").write_bytes(b"\xff")
        self.assertEqual("unverified", self.entry(self.discover(), "example/hello.styio")["status"])

    def test_unknown_applicability_never_marks_file_as_covered(self) -> None:
        self.write("example/new.filetype", "unrecognized")
        entry = self.discover()["entries"][0]
        self.assertEqual("unverified", entry["status"])
        self.assertEqual([], entry["consumers"])
        self.assertIn("report-only", entry["reasons"][0])

    def test_timeout_must_be_bounded_positive_number(self) -> None:
        for timeout in (0, -1, float("inf"), float("nan")):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                self.discover(timeout=timeout)

    def test_missing_catalog_and_empty_discovery_roots_fail_closed(self) -> None:
        with self.assertRaises(ValueError):
            examples.discover_examples(self.root, {})
        with self.assertRaises(ValueError):
            self.discover(example_roots=())

    def test_oversized_source_is_reported_without_execution(self) -> None:
        self.golden(source="x" * (examples.MAX_SOURCE_BYTES + 1))
        report = self.discover()
        self.assertEqual([], report["specifications"])
        self.assertIn("bounded discovery size", self.entry(report, "example/hello.styio")["reasons"][0])


if __name__ == "__main__":
    unittest.main()
