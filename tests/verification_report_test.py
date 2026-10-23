#!/usr/bin/env python3
"""Focused tests for advisory discovery and evidence reporting (no compiler needed)."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("verification_report", ROOT / "scripts/verification-report.py")
reporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reporter)


class VerificationReportTest(unittest.TestCase):
    def test_git_discovers_new_staged_files_without_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
            env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
            subprocess.run(["git", "init", "-q"], cwd=root, env=env, check=True)
            for name in ("new file.cpp", "unexpected.xyz", "untracked.cpp"):
                (root / name).touch()
            subprocess.run(["git", "add", "new file.cpp", "unexpected.xyz"], cwd=root, env=env, check=True)
            with patch.dict(os.environ, env, clear=True):
                self.assertEqual(["new file.cpp", "unexpected.xyz"], reporter.tracked_files(root))

    def test_unknown_path_and_type_remain_explicit(self):
        for path in ("new-area/file.py", "src/new.weird"):
            self.assertEqual("unclassified", reporter.classify(path, {}, False)["status"])

    def test_known_directory_automatically_routes_new_file(self):
        self.assertEqual("routed", reporter.classify("scripts/another-check.py", {}, False)["status"])
        self.assertEqual("routed", reporter.classify("tests/fuzz/corpus/parser/abcdef", {}, False)["status"])

    def test_missing_database_is_unknown_not_unbuilt(self):
        self.assertEqual("target-unverified", reporter.classify("src/New.cpp", {}, False)["status"])
        self.assertEqual("no-configured-target", reporter.classify("src/New.cpp", {}, True)["status"])
        self.assertTrue(reporter.compilation_evidence(ROOT, None)[1])

    def test_command_object_path_and_multiple_targets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "src").mkdir()
            source = root / "src/new file.cpp"
            source.touch()
            build = root / "build"
            working = build / "src"
            obj = working / "CMakeFiles/core.dir/new file.cpp.o"
            obj.parent.mkdir(parents=True)
            obj.touch()
            database = build / "compile_commands.json"
            database.write_text(json.dumps([
                {"directory": str(working), "file": str(source),
                 "command": "c++ -o 'CMakeFiles/core.dir/new file.cpp.o' -c 'new file.cpp'",
                 "output": "src/CMakeFiles/core.dir/new file.cpp.o"},
                {"directory": str(working), "file": str(source),
                 "arguments": ["c++", "-o", "CMakeFiles/other.dir/new.o", "-c", str(source)]},
            ]))
            evidence, warnings = reporter.compilation_evidence(root, database)
            self.assertFalse(warnings)
            row = reporter.classify("src/new file.cpp", evidence, True)
            self.assertEqual(["core", "other"], row["targets"])
            self.assertEqual("object-present", row["status"])
            self.assertEqual([True, False], [e["object_present"] for e in evidence["src/new file.cpp"]])

    def test_configured_is_not_compiled(self):
        row = reporter.classify("src/a.cpp", {"src/a.cpp": [{"target": "a", "object_present": False}]}, True)
        self.assertEqual("configured-no-object", row["status"])

    def test_headers_and_runtime_fixtures_have_reasons(self):
        self.assertIn("indirectly", reporter.classify("src/new.hpp", {}, True)["reason"])
        row = reporter.classify("tests/features/native_interop/native/new.cpp", {}, True)
        self.assertEqual("runtime-fixture-unverified", row["status"])
        self.assertIn("compilation are unverified", row["reason"])

    def test_generated_source_still_checked(self):
        row = reporter.classify("grammar/tree-sitter-styio/src/parser.c", {}, True)
        self.assertEqual("no-configured-target", row["status"])
        self.assertIn("Generated", row["reason"])

    def test_final_consumes_results_without_running_commands(self):
        with patch.object(reporter.subprocess, "run", side_effect=AssertionError("must not run checks")):
            result = reporter.final_report({}, {"linux": {"result": "failure"}, "golden": {"result": "skipped"}},
                                           [{"name": "audit", "status": "in_progress", "conclusion": None}])
            markdown = reporter.markdown(result)
        self.assertIn("linux: failure", markdown)
        self.assertIn("golden: skipped", markdown)
        self.assertIn("audit: in_progress", markdown)
        self.assertIn("unavailable", markdown)

    def test_malformed_input_is_reported_without_blocking(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            invalid = root / "invalid.json"
            invalid.write_text("{invalid")
            output = root / "report.json"
            env = os.environ.copy()
            env["STYIO_CI_RESULTS"] = json.dumps({"linux": {"result": "failure"}, "golden": {"result": "skipped"}})
            proc = subprocess.run([sys.executable, str(ROOT / "scripts/verification-report.py"),
                                   "final", "--inventory", str(invalid), "--checks", str(invalid),
                                   "--jobs", str(invalid), "--output", str(output)], env=env, capture_output=True, text=True)
            self.assertEqual(0, proc.returncode)
            self.assertIn("malformed", output.read_text())
            self.assertIn("linux: failure", proc.stdout)
            self.assertIn("golden: skipped", proc.stdout)
            self.assertIn("run_id", output.read_text())

    def test_ci_report_is_advisory_and_not_a_required_dependency(self):
        workflow = (ROOT / ".github/workflows/styio-ci-gate.yml").read_text()
        advisory = workflow.split("\n  verification-report:\n", 1)[1]
        self.assertIn("    continue-on-error: true", advisory)
        required = workflow.split("\n  required-ci-gate:\n", 1)[1].split("\n  verification-report:\n", 1)[0]
        self.assertNotIn("verification-report", required)
        self.assertIn('if [[ "$result" != "success" ]]', required)


if __name__ == "__main__":
    unittest.main()
