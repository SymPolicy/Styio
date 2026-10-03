#!/usr/bin/env python3
"""Offline orchestration/security tests: no network, compiler or real subprocess."""
from __future__ import annotations
import contextlib
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
import zipfile
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location("promotion_ci_runner", REPO_ROOT / "scripts/promotion-ci.py")
assert SPEC and SPEC.loader
subject = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(subject)
REAL_GIT_INPUTS = subject.git_inputs
REPO, WORKFLOW = "SymPolicy/Styio", ".github/workflows/styio-ci-gate.yml"
CANDIDATE = "new-promotion-sha"
POLICY = {
    "repository": REPO, "workflow": WORKFLOW, "public_channel": "release",
    "candidate_search_limit": 20, "nightly_platforms": ["linux"],
    "acceptance_platforms": ["linux", "windows", "macos"],
    "nightly_targets": ["styio", "styio_security_test"],
    "nightly_names": ["compiler.smoke"], "nightly_labels": ["security"],
    "scheduled_only_labels": {"soak": "Dedicated scheduled soak workflow"},
    "dependencies": {"pafio": {"sha": "pinned-pafio"}, "vityo": {"sha": "pinned-vityo"}},
}
CATALOG = {"tests": [
    {"name": "compiler.smoke", "command": ["/build/styio", "--smoke"], "labels": ["smoke"]},
    {"name": "security.bounds", "command": ["/build/security"], "labels": ["security", "smoke"]},
    {"name": "language.full", "command": ["/build/full"], "labels": ["language"]},
    {"name": "scheduled.soak", "command": ["/build/soak"], "labels": ["soak"]},
]}
FINGERPRINT = {"platform": "linux", "architecture": "x86_64", "toolchain": {"compiler": "offline-clang"},
               "environment": {"arch": "x64"}, "dependencySHA": "fixed-dependency"}
INPUTS = {"src/compiler.cpp": {"oid": "build-blob", "mode": "100644", "type": "blob"}}


def metadata(branch="nightly", run_id=42, attempt=1, artifact_id=51):
    return {
        "run": {"id": run_id, "run_attempt": attempt, "head_sha": "retained-source-sha", "head_branch": branch,
                "repository": {"full_name": REPO, "fork": False}, "head_repository": {"full_name": REPO, "fork": False},
                "path": WORKFLOW, "status": "completed", "conclusion": "success", "event": "push",
                "run_started_at": "2026-10-01T00:00:00Z", "updated_at": "2026-10-01T00:15:00Z",
                "check_suite_id": 900, "html_url": f"https://github.com/{REPO}/actions/runs/{run_id}"},
        "artifact": {"id": artifact_id, "name": "promotion-evidence-linux", "expired": False,
                     "created_at": "2026-10-01T00:10:00Z", "expires_at": "2099-11-03T00:00:00Z", "workflow_run": {"id": run_id, "head_sha": "retained-source-sha"},
                     "url": f"https://api.github.com/repos/{REPO}/actions/artifacts/{artifact_id}"},
        "branch": {"name": branch, "protected": True},
        "check": {"id": 61, "name": "styio-ci-gate", "head_sha": "retained-source-sha", "check_suite": {"id": 900},
                  "status": "completed", "conclusion": "success", "html_url": f"https://github.com/{REPO}/runs/61"},
    }


def verified(branch="nightly", **kwargs):
    return subject.validate_source(metadata(branch, **kwargs), REPO, WORKFLOW,
                                   ["nightly", "stable"], ["push", "workflow_dispatch"])


def junit(names, statuses=None):
    statuses = statuses or {}
    children = {"failed": "<failure/>", "skipped": "<skipped/>", "error": "<error/>"}
    return "<testsuite>" + "".join(
        f'<testcase name="{name}">{children.get(statuses.get(name), "")}</testcase>' for name in names
    ) + "</testsuite>"


class TemporaryRunnerTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build, self.out = self.root / "build/promotion", self.root / "build/promotion-evidence"
        self.out.mkdir(parents=True)
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(mock.patch.multiple(subject, ROOT=self.root, BUILD=self.build, OUT=self.out))
        self.stack.enter_context(mock.patch.dict(os.environ, {}, clear=True))
        # An overlooked I/O path must fail instead of executing real external work.
        for name in ("run", "check_output"):
            self.stack.enter_context(mock.patch.object(subject.subprocess, name, side_effect=AssertionError("unexpected real subprocess")))

    def patch(self, name, **kwargs):
        return self.stack.enter_context(mock.patch.object(subject, name, **kwargs))


class JunitExecutionTest(TemporaryRunnerTest):
    def execute(self, xml=None, returncode=0):
        def command(args, **kwargs):
            self.assertEqual("ctest", args[0])
            self.assertFalse(kwargs["check"])
            self.assertIn("--no-tests=error", args)
            self.assertFalse((self.out / "ctest-results.xml").exists(), "stale XML was not removed")
            if xml is not None:
                (self.out / "ctest-results.xml").write_text(xml)
            return subprocess.CompletedProcess(args, returncode)
        self.patch("command", side_effect=command)
        return subject.run_tests([], {"run": ["compiler.smoke", "security.bounds"], "reuse": {}}, [])

    def test_stale_junit_is_deleted_and_cannot_establish_pass(self):
        (self.out / "ctest-results.xml").write_text(junit(["compiler.smoke", "security.bounds"]))
        self.assertEqual({"unknown"}, {r["status"] for r in self.execute().values()})

    def test_missing_junit_is_unknown_even_on_zero_exit(self):
        self.assertEqual({"unknown"}, {r["status"] for r in self.execute().values()})

    def test_nonzero_exit_with_passing_junit_cannot_pass(self):
        records = self.execute(junit(["compiler.smoke", "security.bounds"]), returncode=8)
        self.assertTrue(all(r["status"] != "passed" for r in records.values()))

    def test_missing_and_skipped_testcases_remain_nonpassing(self):
        records = self.execute(junit(["compiler.smoke"], {"compiler.smoke": "skipped"}))
        self.assertEqual("skipped", records["compiler.smoke"]["status"])
        self.assertEqual("unknown", records["security.bounds"]["status"])

    def test_failure_and_error_junit_are_failed(self):
        for status in ("failed", "error"):
            with self.subTest(status=status):
                path = self.out / "results.xml"
                path.write_text(junit(["compiler.smoke"], {"compiler.smoke": status}))
                self.assertEqual("failed", subject.collect_junit(path, ["compiler.smoke"])["compiler.smoke"])

    def test_duplicate_passing_case_cannot_overwrite_failure_or_skip(self):
        for status in ("failed", "skipped"):
            with self.subTest(status=status):
                path = self.out / "results.xml"
                bad = "<failure/>" if status == "failed" else "<skipped/>"
                path.write_text(f'<testsuite><testcase name="same">{bad}</testcase><testcase name="same"/></testsuite>')
                self.assertNotEqual("passed", subject.collect_junit(path, ["same"])["same"])

    def test_reused_records_do_not_reexecute_and_only_missing_names_selected(self):
        previous = {"status": "passed", "execution": "previous"}
        def command(args, **kwargs):
            self.assertEqual("security.bounds\n", (self.out / "ctest-selection.txt").read_text())
            (self.out / "ctest-results.xml").write_text(junit(["security.bounds"]))
            return subprocess.CompletedProcess(args, 0)
        runner = self.patch("command", side_effect=command)
        records = subject.run_tests([], {"run": ["security.bounds"], "reuse": {"compiler.smoke": previous}}, [])
        self.assertEqual(previous, records["compiler.smoke"])
        self.assertEqual("current", records["security.bounds"]["execution"])
        runner.assert_called_once()

    def test_required_golden_example_failure_is_not_advisory(self):
        (self.root / "expected.txt").write_text("expected\n")
        (self.root / "input.txt").write_text("controlled input\n")
        spec = {"id": "auto-example:documented", "command": ["offline-compiler", "example.styio"],
                "expected_output_file": "expected.txt", "stdin_file": "input.txt", "timeout": 3}
        for stdout, returncode, expected in (("expected\n", 0, "passed"), ("wrong\n", 0, "failed"),
                                             ("expected\n", 2, "failed")):
            with self.subTest(stdout=stdout, returncode=returncode), mock.patch.object(
                    subject, "command", return_value=subprocess.CompletedProcess(spec["command"], returncode, stdout)) as command:
                records = subject.run_tests([], {"run": [spec["id"]], "reuse": {}}, [spec])
                self.assertEqual(expected, records[spec["id"]]["status"])
                command.assert_called_once_with(spec["command"], capture=True, check=False,
                                                input="controlled input\n", timeout=3)
        with mock.patch.object(subject, "command", side_effect=subprocess.TimeoutExpired(spec["command"], 3)):
            records = subject.run_tests([], {"run": [spec["id"]], "reuse": {}}, [spec])
            self.assertEqual("failed", records[spec["id"]]["status"])


class TierExecutionTest(TemporaryRunnerTest):
    def setUp(self):
        super().setUp()
        self.catalog, self.statuses = copy.deepcopy(CATALOG), {}
        self.exit_code, self.emit_junit, self.commands, self.configure_contents = 0, True, [], []
        self.command = self.patch("command", side_effect=self.fake_command)
        self.inputs = self.patch("git_inputs", return_value=copy.deepcopy(INPUTS))
        self.detect = self.patch("detect_platform", return_value=("linux", "x86_64"))
        self.probe = self.patch("probe_environment", return_value={"fingerprint_data": copy.deepcopy(FINGERPRINT), "configure_args": ["-S", self.root, "-B", self.build]})
        self.prior = self.patch("prior_evidence", return_value=([], []))
        self.patch("normalized_checkout_times")
        self.examples = self.patch("discover_examples", return_value={"specifications": [], "findings": [{"status": "unknown", "path": "example/unclassified.styio"}]})
        self.package = self.patch("package_payload", return_value="sealed-payload-id")
        self.pack = self.patch("pack")
        self.download = self.patch("download_named", side_effect=AssertionError("unexpected artifact download"))
        self.extract = self.patch("extract_archive", side_effect=AssertionError("unexpected cache extraction"))
        self.stack.enter_context(contextlib.redirect_stderr(io.StringIO()))

    def fake_command(self, args, **kwargs):
        values = [str(value) for value in args]
        self.commands.append(values)
        if values[:3] == ["git", "rev-parse", "HEAD"]:
            return subprocess.CompletedProcess(args, 0, CANDIDATE)
        if values == ["git", "status", "--porcelain"]:
            return subprocess.CompletedProcess(args, 0, "")
        if values[0] == "cmake":
            if "--build" not in values:
                self.configure_contents.append(sorted(path.name for path in self.build.iterdir()) if self.build.exists() else [])
                self.build.mkdir(parents=True, exist_ok=True)
            return subprocess.CompletedProcess(args, 0)
        if values[0] == "ctest" and "--show-only=json-v1" in values:
            return subprocess.CompletedProcess(args, 0, json.dumps(self.catalog))
        if values[0] == "ctest" and "--tests-from-file" in values:
            names = Path(values[values.index("--tests-from-file") + 1]).read_text().splitlines()
            if self.emit_junit:
                Path(values[values.index("--output-junit") + 1]).write_text(junit(names, self.statuses))
            return subprocess.CompletedProcess(args, self.exit_code)
        raise AssertionError(f"unexpected command: {values}")

    def run_tier(self, tier="stable", platform="linux"):
        result = subject.run_tier(SimpleNamespace(tier=tier, platform=platform), copy.deepcopy(POLICY))
        report = json.loads((self.out / "evidence.json").read_text())
        self.assertEqual(0 if report["status"] == "passed" else 1, result)
        self.assertIn(f"Status: {report['status']}", (self.out / "report.md").read_text())
        return result, report

    def prior_envelope(self, tier="nightly"):
        source = verified(tier)
        catalog = subject.select_tests(self.catalog, tier, POLICY)
        bid = subject.build_identity(subject.git_inputs(self.root, subject.BUILD_PATHS), FINGERPRINT)
        wanted = subject.identities(catalog, bid, FINGERPRINT)
        envelope = {"status": "passed", "tier": tier, "candidate_sha": source.head_sha, "platform": "linux",
                    "source": source.provenance(), "build_id": bid, "fingerprint": copy.deepcopy(FINGERPRINT),
                    "distribution_inputs": subject.git_inputs(self.root, subject.DISTRIBUTION_PATHS),
                    "catalog": catalog, "identities": wanted, "payload_id": "sealed-payload-id",
                    "tests": {name: {"name": name, "identity": identity, "status": "passed", "source": source.provenance()} for name, identity in wanted.items()}}
        return envelope, source, []

    def test_nightly_builds_only_fast_targets_and_smoke_security_union(self):
        result, report = self.run_tier("nightly")
        self.assertEqual(0, result)
        build = next(args for args in self.commands if args[:2] == ["cmake", "--build"])
        self.assertEqual(POLICY["nightly_targets"], build[build.index("--target") + 1:])
        self.assertEqual({"compiler.smoke", "security.bounds"}, set(report["tests"]))
        self.package.assert_not_called()

    def test_stable_executes_only_missing_scope_with_different_candidate_sha(self):
        prior = self.prior_envelope()
        self.prior.return_value = ([prior], [])
        self.download.side_effect = ValueError("optional cache expired")
        result, report = self.run_tier()
        self.assertEqual(0, result)
        self.assertNotEqual(prior[1].head_sha, report["candidate_sha"])
        self.assertEqual(["language.full"], report["plan"]["run"])
        self.assertEqual(["compiler.smoke", "security.bounds"], report["plan"]["reuse"])
        build = next(args for args in self.commands if args[:2] == ["cmake", "--build"])
        self.assertEqual(["all"], build[build.index("--target") + 1:])
        self.package.assert_called_once()

    def test_unknown_example_applicability_is_advisory(self):
        result, report = self.run_tier()
        self.assertEqual(0, result)
        self.assertEqual("unknown", report["examples"]["findings"][0]["status"])
        self.assertTrue(all(record["status"] == "passed" for record in report["tests"].values()))

    def test_failed_skipped_missing_and_nonzero_ctest_fail_current_candidate(self):
        for status, emit, code in (("failed", True, 8), ("skipped", True, 0), (None, False, 0), (None, True, 8)):
            with self.subTest(status=status, emit=emit, exit_code=code):
                self.statuses = {"language.full": status} if status else {}
                self.emit_junit, self.exit_code = emit, code
                self.package.reset_mock()
                result, report = self.run_tier()
                self.assertEqual(1, result)
                self.assertEqual("failed", report["status"])
                self.package.assert_not_called()

    def test_build_exception_writes_failed_evidence(self):
        def command(args, **kwargs):
            if args[:2] == ["cmake", "--build"]:
                raise subprocess.CalledProcessError(2, args)
            return self.fake_command(args, **kwargs)
        self.command.side_effect = command
        result, report = self.run_tier()
        self.assertEqual(1, result)
        self.assertIn("CalledProcessError", report["error"])
        self.package.assert_not_called()
        self.pack.assert_not_called()

    def test_malformed_junit_preserves_reused_successes_and_unknown_missing_scope(self):
        self.prior.return_value = ([self.prior_envelope()], [])
        self.download.side_effect = ValueError("optional cache expired")
        def command(args, **kwargs):
            result = self.fake_command(args, **kwargs)
            if "--output-junit" in args:
                (self.out / "ctest-results.xml").write_text("<malformed")
            return result
        self.command.side_effect = command
        result, report = self.run_tier()
        self.assertEqual(1, result)
        self.assertIn("ParseError", report["error"])
        self.assertEqual("tests", report["phase"])
        self.assertEqual(["language.full"], report["plan"]["run"])
        for name in ("compiler.smoke", "security.bounds"):
            self.assertEqual("passed", report["tests"][name]["status"])
            self.assertIn("source", report["tests"][name])
        self.assertEqual("unknown", report["tests"]["language.full"]["status"])
        self.assertEqual(report["identities"]["language.full"], report["tests"]["language.full"]["identity"])
        self.package.assert_not_called()

    def test_late_example_exception_preserves_completed_ctest_and_example_results(self):
        self.prior.return_value = ([self.prior_envelope()], [])
        self.download.side_effect = ValueError("optional cache expired")
        (self.root / "golden.txt").write_text("golden\n")
        specs = [{"id": f"auto-example:{name}", "command": ["offline-example", name],
                  "expected_output_file": "golden.txt", "timeout": 3} for name in ("first", "second")]
        self.examples.return_value = {"specifications": specs, "findings": []}
        def command(args, **kwargs):
            if args[0] == "offline-example":
                if args[1] == "second":
                    raise RuntimeError("interrupted second example")
                return subprocess.CompletedProcess(args, 0, "golden\n")
            return self.fake_command(args, **kwargs)
        self.command.side_effect = command
        result, report = self.run_tier()
        self.assertEqual(1, result)
        self.assertIn("interrupted second example", report["error"])
        self.assertEqual("tests", report["phase"])
        for name in ("compiler.smoke", "security.bounds", "language.full", "auto-example:first"):
            self.assertEqual("passed", report["tests"][name]["status"], name)
        self.assertEqual("current", report["tests"]["language.full"]["execution"])
        self.assertEqual("current", report["tests"]["auto-example:first"]["execution"])
        self.assertEqual("unknown", report["tests"]["auto-example:second"]["status"])
        for name, identity in report["identities"].items():
            self.assertEqual(identity, report["tests"][name]["identity"])
        self.package.assert_not_called()

    def test_changed_input_removes_old_build_and_never_downloads_old_cache(self):
        prior = self.prior_envelope()
        self.prior.return_value = ([prior], [])
        self.build.mkdir(parents=True)
        (self.build / ".promotion-build-id").write_text(prior[0]["build_id"])
        (self.build / "old-object.o").write_bytes(b"old incompatible object")
        changed = copy.deepcopy(INPUTS)
        changed["src/compiler.cpp"]["oid"] = "changed-source-blob"
        self.inputs.return_value = changed
        result, report = self.run_tier()
        self.assertEqual(0, result)
        self.assertNotEqual(prior[0]["build_id"], report["build_id"])
        self.assertEqual([], self.configure_contents[0])
        self.assertEqual([], report["plan"]["reuse"])
        self.download.assert_not_called()
        self.extract.assert_not_called()

    def test_empty_or_incomplete_nightly_scope_cannot_pass(self):
        for catalog in ({"tests": []}, {"tests": [CATALOG["tests"][0]]}, {"tests": [CATALOG["tests"][1]]}):
            with self.subTest(catalog=catalog):
                self.catalog = catalog
                result, report = self.run_tier("nightly")
                self.assertEqual(1, result)
                self.assertIn("error", report)

    def release_fixture(self):
        prior = self.prior_envelope("stable")
        directory = self.root / "downloaded-payload"
        directory.mkdir(exist_ok=True)
        payload = directory / "payload.tar.gz"
        payload.write_bytes(b"exact sealed stable payload bytes")
        prior[0]["payload_id"] = subject.checksum(payload)
        self.prior.return_value = ([prior], [])
        self.download.side_effect, self.download.return_value = None, directory
        return prior, payload

    def test_release_reuses_exact_stable_payload_without_build_or_ctest(self):
        prior, payload = self.release_fixture()
        distribution = self.patch("check_payload", return_value={"status": "passed"})
        result, report = self.run_tier("release")
        self.assertEqual(0, result)
        self.assertEqual(payload.read_bytes(), (self.out / "payload.tar.gz").read_bytes())
        self.assertEqual(prior[0]["payload_id"], report["payload_id"])
        self.assertEqual(CANDIDATE, report["candidate_sha"])
        self.assertFalse(report["admission"]["build_required"])
        self.assertTrue(all(args[0] == "git" for args in self.commands))
        distribution.assert_called_once_with(payload, prior[0]["build_id"], "linux")
        self.probe.assert_called_once()
        self.package.assert_not_called()
        self.pack.assert_not_called()

    def test_release_distribution_failure_cannot_inherit_stable_pass(self):
        self.release_fixture()
        self.patch("check_payload", side_effect=ValueError("installed execution failed"))
        result, report = self.run_tier("release")
        self.assertEqual(1, result)
        self.assertEqual("failed", report["status"])
        self.assertEqual("release", report["tier"])
        self.assertIn("installed execution failed", report["error"])
        self.assertFalse((self.out / "payload.tar.gz").exists())

    def test_release_missing_or_incompatible_evidence_fails_without_rebuild(self):
        for previous in ([], [self.prior_envelope("stable")]):
            with self.subTest(has_source=bool(previous)):
                if previous:
                    previous[0][0]["build_id"] = "incompatible-build"
                self.prior.return_value = (previous, [])
                result, report = self.run_tier("release")
                self.assertEqual(1, result)
                self.assertIn("release never rebuilds", report["error"])
        self.probe.assert_called_once()
        self.download.assert_not_called()
        self.package.assert_not_called()

    def test_release_changed_payload_checksum_fails_before_distribution(self):
        prior, payload = self.release_fixture()
        payload.write_bytes(b"different payload")
        distribution = self.patch("check_payload")
        result, report = self.run_tier("release")
        self.assertEqual(1, result)
        self.assertIn("payload identity mismatch", report["error"])
        distribution.assert_not_called()

    def test_release_rejects_native_platform_or_architecture_mismatch_before_download(self):
        self.release_fixture()
        distribution = self.patch("check_payload")
        for native in (("windows", "x86_64"), ("linux", "aarch64")):
            with self.subTest(native=native):
                self.detect.return_value = native
                result, report = self.run_tier("release")
                self.assertEqual(1, result)
                self.assertEqual("failed", report["status"])
                self.download.assert_not_called()
                distribution.assert_not_called()
        self.probe.assert_not_called()
        self.package.assert_not_called()

    def test_release_native_platform_probe_failure_is_failed_evidence(self):
        self.release_fixture()
        self.detect.side_effect = ValueError("native runner does not match requested platform")
        distribution = self.patch("check_payload")
        result, report = self.run_tier("release")
        self.assertEqual(1, result)
        self.assertIn("native runner", report["error"])
        self.download.assert_not_called()
        distribution.assert_not_called()

    def test_release_runtime_subset_change_rejects_without_payload_download(self):
        self.release_fixture()
        self.probe.return_value["fingerprint_data"]["llvm_root"] = "/different/llvm/runtime"
        distribution = self.patch("check_payload")
        result, report = self.run_tier("release")
        self.assertEqual(1, result)
        self.assertTrue(any("runtime/toolchain environment changed" in note for note in report["notes"]))
        self.probe.assert_called_once()
        self.download.assert_not_called()
        distribution.assert_not_called()

    def test_release_unused_build_tool_versions_do_not_invalidate_runtime_compatibility(self):
        self.release_fixture()
        self.probe.return_value["fingerprint_data"]["tools"] = {"cmake": "unused-new-version", "ninja": "unused-new-version"}
        self.patch("check_payload", return_value={"status": "passed"})
        result, _ = self.run_tier("release")
        self.assertEqual(0, result)
        self.probe.assert_called_once()
        self.package.assert_not_called()

    def test_packaged_legal_file_changes_invalidate_stable_payload_reuse(self):
        files = ("LICENSE", "NOTICE", "DEPENDENCY-USAGE.md", "LICENSE-POLICY.md")
        blobs = {name: "1" * 40 for name in (*files, "src/compiler.cpp", "configs/promotion-ci.json")}
        def tree(*args, **kwargs):
            return b"".join(f"100644 blob {oid}\t{name}\0".encode() for name, oid in blobs.items())
        self.inputs.side_effect = REAL_GIT_INPUTS
        distribution = self.patch("check_payload", return_value={"status": "passed"})
        with mock.patch.object(subject.subprocess, "check_output", side_effect=tree):
            for name in files:
                with self.subTest(packaged_file=name):
                    self.release_fixture()
                    self.download.reset_mock()
                    blobs[name] = "2" * 40
                    result, report = self.run_tier("release")
                    self.assertEqual(1, result, f"changed {name} reused the old sealed payload")
                    self.assertEqual("failed", report["status"])
                    self.download.assert_not_called()
                    distribution.assert_not_called()
                    blobs[name] = "1" * 40
        self.probe.assert_not_called()
        self.package.assert_not_called()

    def test_stable_records_the_distribution_input_stamp_for_all_legal_files(self):
        files = ("LICENSE", "NOTICE", "DEPENDENCY-USAGE.md", "LICENSE-POLICY.md")
        blobs = {name: str(index + 1) * 40 for index, name in enumerate(
            (*files, "src/compiler.cpp", "configs/promotion-ci.json"))}
        raw_tree = b"".join(f"100644 blob {oid}\t{name}\0".encode() for name, oid in blobs.items())
        self.inputs.side_effect = REAL_GIT_INPUTS
        with mock.patch.object(subject.subprocess, "check_output", return_value=raw_tree):
            result, report = self.run_tier("stable")
        self.assertEqual(0, result)
        self.assertEqual(set(files), set(report["distribution_inputs"]))
        for name in files:
            self.assertEqual(blobs[name], report["distribution_inputs"][name]["oid"])

    def test_release_rejects_legacy_evidence_without_distribution_stamp(self):
        prior, _ = self.release_fixture()
        del prior[0]["distribution_inputs"]
        distribution = self.patch("check_payload")
        result, report = self.run_tier("release")
        self.assertEqual(1, result)
        self.assertEqual("failed", report["status"])
        self.download.assert_not_called()
        distribution.assert_not_called()


class InputBoundaryTest(TemporaryRunnerTest):
    def test_syntax_matrix_change_invalidates_syntax_and_docs_not_language(self):
        matrix = "docs/design/syntax/SYNTAX-CONVERGENCE-MATRIX.json"
        blobs = {matrix: "1" * 40, "configs/promotion-ci.json": "2" * 40,
                 "scripts/syntax-convergence-gate.py": "3" * 40, "src/compiler.cpp": "4" * 40}
        def tree(*args, **kwargs):
            return b"".join(f"100644 blob {oid}\t{name}\0".encode() for name, oid in blobs.items())
        tests = [{"name": name, "command": [name], "labels": [label]} for name, label in
                 (("syntax_convergence_gate", "syntax_convergence"), ("docs_audit", "docs"), ("language.full", "language"))]
        with mock.patch.object(subject.subprocess, "check_output", side_effect=tree):
            before = subject.identities(tests, "same-build", FINGERPRINT)
            blobs[matrix] = "5" * 40
            after = subject.identities(tests, "same-build", FINGERPRINT)
        self.assertNotEqual(before["syntax_convergence_gate"], after["syntax_convergence_gate"])
        self.assertNotEqual(before["docs_audit"], after["docs_audit"])
        self.assertEqual(before["language.full"], after["language.full"])

    def test_nested_test_cmake_change_invalidates_build_identity(self):
        path = "tests/fuzz/CMakeLists.txt"
        raw = f"100644 blob {'1' * 40}\t{path}\0".encode()
        with mock.patch.object(subject.subprocess, "check_output", return_value=raw) as git:
            before = subject.build_identity(subject.git_inputs(self.root, subject.BUILD_PATHS), FINGERPRINT)
            git.return_value = f"100644 blob {'2' * 40}\t{path}\0".encode()
            after = subject.build_identity(subject.git_inputs(self.root, subject.BUILD_PATHS), FINGERPRINT)
        self.assertNotEqual(before, after)


class ArchiveAndDistributionTest(TemporaryRunnerTest):
    def test_exact_artifact_id_is_downloaded_not_an_ambiguous_name(self):
        def download(args, **kwargs):
            self.assertEqual(["gh", "api", f"repos/{REPO}/actions/artifacts/765/zip"], args)
            self.assertTrue(kwargs["check"])
            with zipfile.ZipFile(kwargs["stdout"], "w") as archive:
                archive.writestr("evidence.json", '{"status":"passed"}')
            return subprocess.CompletedProcess(args, 0)
        destination = self.root / "exact-artifact"
        with mock.patch.object(subject.subprocess, "run", side_effect=download) as command:
            self.assertEqual(destination, subject.download_artifact(REPO, {"id": 765}, destination))
        self.assertEqual({"status": "passed"}, json.loads((destination / "evidence.json").read_text()))
        self.assertFalse((destination / ".download.zip").exists())
        command.assert_called_once()

    def test_downloaded_zip_rejects_traversal_absolute_paths_and_symlinks(self):
        for name, link in (("../escape", False), (str(self.root / "absolute-escape"), False), ("link", True)):
            with self.subTest(name=name):
                def download(args, **kwargs):
                    with zipfile.ZipFile(kwargs["stdout"], "w") as archive:
                        member = zipfile.ZipInfo(name)
                        if link:
                            member.create_system = 3
                            member.external_attr = 0o120777 << 16
                        archive.writestr(member, "../escape" if link else "unsafe")
                    return subprocess.CompletedProcess(args, 0)
                with mock.patch.object(subject.subprocess, "run", side_effect=download), self.assertRaises(ValueError):
                    subject.download_artifact(REPO, {"id": 765}, self.root / "zip-extract")
        self.assertFalse((self.root / "escape").exists())
        self.assertFalse((self.root / "absolute-escape").exists())

    def archive(self, entries):
        archive = self.root / "payload.tar.gz"
        with tarfile.open(archive, "w:gz") as handle:
            for name, value in entries.items():
                info = tarfile.TarInfo(name)
                if isinstance(value, tuple):
                    info.type, info.linkname = value
                    handle.addfile(info)
                else:
                    data = value.encode()
                    info.size = len(data)
                    handle.addfile(info, io.BytesIO(data))
        return archive

    def test_unsafe_archive_members_are_rejected_before_extraction(self):
        for name, content in (("../escape", "bad"), (str(self.root / "absolute-escape"), "bad"),
                              ("link", (tarfile.SYMTYPE, "../escape")), ("hard", (tarfile.LNKTYPE, "../escape")),
                              ("fifo", (tarfile.FIFOTYPE, ""))):
            with self.subTest(name=name):
                with self.assertRaises((ValueError, tarfile.FilterError)):
                    subject.extract_archive(self.archive({name: content}), self.root / "extract")
        self.assertFalse((self.root / "escape").exists())
        self.assertFalse((self.root / "absolute-escape").exists())

    def payload(self, platform="linux", **changes):
        identity = {"build_id": "sealed-build", "public_channel": "release"}
        identity.update(changes)
        return self.archive({"build-identity.json": json.dumps(identity), "LICENSE": "license",
                             "bin/styio.exe" if platform == "windows" else "bin/styio": "offline binary"})

    def test_distribution_executes_installed_payload_then_removes_temporary_install(self):
        for platform in ("linux", "windows", "macos"):
            with self.subTest(platform=platform):
                calls = []
                def command(args, **kwargs):
                    calls.append(args)
                    self.assertTrue(Path(args[0]).is_file())
                    if args[1] == "--machine-info=json":
                        return subprocess.CompletedProcess(args, 0, json.dumps({"channel": "release", "build_id": "sealed-build"}))
                    self.assertIn("distribution-ok", Path(args[2]).read_text())
                    return subprocess.CompletedProcess(args, 0, "distribution-ok\n")
                with mock.patch.object(subject, "command", side_effect=command):
                    report = subject.check_payload(self.payload(platform), "sealed-build", platform)
                self.assertEqual("passed", report["status"])
                self.assertEqual(2, len(calls))
                self.assertFalse(Path(calls[0][0]).parent.parent.exists())

    def test_distribution_rejects_wrong_identity_or_channel_before_execution(self):
        command = self.patch("command")
        for identity in ({"build_id": "different-build"}, {"public_channel": "nightly"}):
            with self.subTest(identity=identity), self.assertRaisesRegex(ValueError, "identity/public channel"):
                subject.check_payload(self.payload(**identity), "sealed-build", "linux")
        command.assert_not_called()


class PriorEvidenceTest(TemporaryRunnerTest):
    def setUp(self):
        super().setUp()
        os.environ["GH_TOKEN"] = "offline-placeholder"
        self.meta = metadata("stable")
        self.manifest = {"schema": 1, "platform": "linux", "status": "passed", "tier": "stable",
                         "candidate_sha": self.meta["run"]["head_sha"], "run_id": 42, "run_attempt": 1,
                         "tests": {"compiler.smoke": {"identity": "id", "status": "passed", "source": {"run_id": "forged"}}}}
        self.calls = []
        self.api = self.patch("api", side_effect=self.fake_api)
        self.command = self.patch("download_artifact", side_effect=self.fake_download)

    def fake_api(self, path):
        self.calls.append(path)
        if "/actions/workflows/" in path:
            return {"workflow_runs": [copy.deepcopy(self.meta["run"])] if "branch=stable&" in path else []}
        if path.endswith("/artifacts?per_page=100"):
            return {"artifacts": [copy.deepcopy(self.meta["artifact"])]}
        if path.endswith("/jobs?per_page=100"):
            return {"jobs": [{"name": "styio-ci-gate", "check_run_url": "https://api.github.com/check-current"}]}
        if "/actions/runs/" in path:
            return copy.deepcopy(self.meta["run"])
        if "/branches/" in path:
            return copy.deepcopy(self.meta["branch"])
        if path == "https://api.github.com/check-current":
            return copy.deepcopy(self.meta["check"])
        raise AssertionError(f"unexpected API request: {path}")

    def fake_download(self, repo, artifact, target):
        self.assertEqual(REPO, repo)
        self.assertEqual(self.meta["artifact"]["id"], artifact["id"])
        target = Path(target)
        target.mkdir(parents=True, exist_ok=True)
        (target / "evidence.json").write_text(json.dumps(self.manifest))
        return target

    def test_release_queries_stable_only_and_binds_download_to_verified_api_source(self):
        accepted, notes = subject.prior_evidence(POLICY, "release", "linux")
        self.assertEqual([], notes)
        self.assertEqual(1, len(accepted))
        searches = [path for path in self.calls if "/actions/workflows/" in path and "branch=" in path]
        self.assertEqual(1, len(searches))
        self.assertIn("branch=stable&", searches[0])
        envelope, source, _ = accepted[0]
        self.assertEqual(source.provenance(), envelope["source"])
        self.assertEqual(source.provenance(), envelope["tests"]["compiler.smoke"]["source"])
        self.assertEqual({"run_id": "forged"}, envelope["tests"]["compiler.smoke"]["origin"])

    def test_current_run_check_suite_binding_required_before_download(self):
        self.meta["check"]["check_suite"]["id"] = 999
        accepted, notes = subject.prior_evidence(POLICY, "stable", "linux")
        self.assertEqual([], accepted)
        self.assertTrue(notes)
        self.command.assert_not_called()

    def test_failed_or_fork_source_not_downloaded(self):
        for case in ("failed", "fork"):
            with self.subTest(case=case):
                self.meta = metadata("stable")
                if case == "failed":
                    self.meta["check"]["conclusion"] = "failure"
                else:
                    self.meta["run"]["head_repository"]["fork"] = True
                accepted, notes = subject.prior_evidence(POLICY, "stable", "linux")
                self.assertEqual([], accepted)
                self.assertTrue(notes)
        self.command.assert_not_called()

    def test_manifest_candidate_and_tier_must_match_verified_source(self):
        for field, wrong in (("candidate_sha", "different-sha"), ("tier", "nightly")):
            with self.subTest(field=field):
                self.manifest[field] = wrong
                self.meta["artifact"]["id"] += 1
                accepted, notes = subject.prior_evidence(POLICY, "release", "linux")
                self.assertEqual([], accepted)
                self.assertTrue(notes)
                self.manifest[field] = self.meta["run"]["head_sha"] if field == "candidate_sha" else "stable"

    def test_new_run_attempt_or_artifact_cannot_reuse_prior_download(self):
        accepted, _ = subject.prior_evidence(POLICY, "release", "linux")
        self.assertEqual(1, len(accepted))
        self.command.reset_mock()
        self.meta["run"]["run_attempt"], self.meta["artifact"]["id"] = 2, 52
        self.manifest["run_attempt"] = 2
        self.manifest["tests"]["compiler.smoke"]["identity"] = "new-attempt-id"
        accepted, _ = subject.prior_evidence(POLICY, "release", "linux")
        self.assertEqual(1, len(accepted))
        self.command.assert_called_once()
        self.assertEqual("new-attempt-id", accepted[0][0]["tests"]["compiler.smoke"]["identity"])
        self.assertEqual(2, accepted[0][1].run_attempt)
        self.assertEqual(52, accepted[0][1].artifact_id)

    def test_missing_authentication_does_no_api_or_download_work(self):
        os.environ.pop("GH_TOKEN")
        accepted, notes = subject.prior_evidence(POLICY, "stable", "linux")
        self.assertEqual([], accepted)
        self.assertTrue(notes)
        self.api.assert_not_called()
        self.command.assert_not_called()

    def chronology(self, success_id, success_started, success_updated, *, omit_updated=False):
        self.meta = metadata("stable", run_id=success_id)
        self.meta["run"].update(run_started_at=success_started, updated_at=success_updated)
        self.meta["artifact"]["created_at"] = success_updated
        self.manifest["run_id"] = success_id
        self.manifest["build_id"] = "build-linux"
        failure = metadata("stable", run_id=90, attempt=2)["run"]
        failure.update(conclusion="failure", run_started_at="2026-10-02T10:00:00Z",
                       updated_at="2026-10-02T10:30:00Z")
        if omit_updated:
            failure.pop("updated_at")
            self.meta["run"].pop("updated_at")
        def api(path):
            if "/actions/workflows/" in path and "branch=stable&" in path:
                return {"workflow_runs": [copy.deepcopy(self.meta["run"]), copy.deepcopy(failure)]}
            return self.fake_api(path)
        self.api.side_effect = api
        self.patch("failed_platform_evidence", return_value={"order": subject.run_order(failure),
                   "run_id": failure["id"], "run_attempt": failure["run_attempt"],
                   "build_id": "build-linux", "phase": "build", "tests": {}})

    def assert_chronology_invalidated(self, accepted):
        self.assertEqual(1, len(accepted))
        envelope, source, _ = accepted[0]
        self.assertNotEqual(90, source.run_id, "failed source must never receive a validated handle")
        self.assertEqual(90, envelope["invalidated_by"]["run_id"])
        self.assertEqual(2, envelope["invalidated_by"]["run_attempt"])
        self.assertEqual(90, envelope["tests"]["compiler.smoke"]["invalidated_by"]["run_id"])
        plan = subject.plan_tests([CATALOG["tests"][0]], {"compiler.smoke": "id"}, envelope["tests"], [source])
        self.assertEqual(["compiler.smoke"], plan["run"])
        self.command.assert_called_once()

    def test_later_failed_rerun_invalidates_earlier_success_despite_lower_run_id(self):
        self.chronology(100, "2026-10-02T09:00:00Z", "2026-10-02T09:30:00Z")
        accepted, notes = subject.prior_evidence(POLICY, "release", "linux")
        self.assert_chronology_invalidated(accepted)

    def test_fresh_success_after_failure_reusable_even_with_lower_run_id(self):
        self.chronology(80, "2026-10-02T11:00:00Z", "2026-10-02T11:30:00Z")
        accepted, notes = subject.prior_evidence(POLICY, "release", "linux")
        self.assertEqual(1, len(accepted), f"newer successful evidence must not be blocked by run ID: {notes}")
        self.assertEqual(80, accepted[0][1].run_id)
        self.assertNotIn("invalidated_by", accepted[0][0])
        self.assertNotIn("invalidated_by", accepted[0][0]["tests"]["compiler.smoke"])
        self.command.assert_called_once()

    def test_failed_attempt_chronology_falls_back_to_run_start_time(self):
        self.chronology(100, "2026-10-02T09:00:00Z", "2026-10-02T09:30:00Z", omit_updated=True)
        accepted, notes = subject.prior_evidence(POLICY, "release", "linux")
        self.assert_chronology_invalidated(accepted)

    def test_later_attempt_wins_tied_timestamps_before_numeric_run_id(self):
        self.chronology(100, "2026-10-02T10:00:00Z", "2026-10-02T10:30:00Z")
        accepted, notes = subject.prior_evidence(POLICY, "release", "linux")
        self.assert_chronology_invalidated(accepted)

    def test_payload_artifact_must_belong_to_verified_run_and_not_be_expired(self):
        source = verified("stable")
        expired = dict(copy.deepcopy(self.meta["artifact"]), name="promotion-payload-linux", expired=True)
        wrong_run = dict(copy.deepcopy(self.meta["artifact"]), name="promotion-payload-linux", workflow_run={"id": 999})
        for artifact in (expired, wrong_run):
            with self.subTest(artifact=artifact), self.assertRaises(ValueError):
                subject.download_named(POLICY, ({}, source, [artifact]), "promotion-payload-linux", self.root / "payload")
        self.command.assert_not_called()


class NegativeScopeCollectorTest(TemporaryRunnerTest):
    """Exercise real positive and negative collection against independent API fixtures."""
    def setUp(self):
        super().setUp()
        os.environ["GH_TOKEN"] = "offline-placeholder"
        self.sources, self.artifacts, self.manifests = {}, {}, {}
        self.platform_jobs = {100: ["linux", "windows", "macos"], 90: ["linux", "windows", "macos"]}
        self.api_calls, self.downloaded = [], []
        for run_id, attempt, hour, conclusion in ((100, 1, "09", "success"), (90, 2, "10", "failure")):
            data = metadata("stable", run_id=run_id, attempt=attempt, artifact_id=run_id * 10)
            sha = f"source-{run_id}"
            data["run"].update(head_sha=sha, conclusion=conclusion, check_suite_id=run_id * 100,
                               run_started_at=f"2026-10-02T{hour}:00:00Z", updated_at=f"2026-10-02T{hour}:30:00Z")
            data["check"].update(head_sha=sha, conclusion=conclusion, id=run_id * 10,
                                 check_suite={"id": run_id * 100})
            self.sources[run_id] = data
            self.artifacts[run_id] = []
            for index, platform in enumerate(POLICY["acceptance_platforms"]):
                artifact = dict(copy.deepcopy(data["artifact"]), id=run_id * 10 + index,
                                name=f"promotion-evidence-{platform}", created_at=f"2026-10-02T{hour}:20:00Z",
                                workflow_run={"id": run_id, "head_sha": sha})
                self.artifacts[run_id].append(artifact)
                self.manifests[run_id, platform] = {
                    "schema": 1, "candidate_sha": sha, "source_head_sha": sha, "tier": "stable", "platform": platform,
                    "build_id": f"build-{platform}", "payload_id": "sealed-payload", "phase": "tests",
                    "status": "passed" if conclusion == "success" else "failed",
                    "tests": {name: {"name": name, "identity": f"{name}-id", "status": "passed"}
                              for name in ("compiler.smoke", "language.full")}}
        self.manifests[90, "linux"]["tests"]["compiler.smoke"]["status"] = "failed"
        self.pr = {"merged": True, "state": "closed", "merged_at": "2026-10-02T08:00:00Z",
                   "head": {"ref": "nightly", "sha": "source-90", "repo": {"full_name": REPO, "fork": False}},
                   "base": {"ref": "stable", "repo": {"full_name": REPO, "fork": False}}}
        self.parents = ["base-sha", "source-90"]
        self.api = self.patch("api", side_effect=self.fake_api)
        self.download = self.patch("download_artifact", side_effect=self.fake_download)

    def fake_api(self, path):
        self.api_calls.append(path)
        if "/actions/workflows/" in path:
            if "event=pull_request" in path:
                self.assertNotIn("status=success", path, "failed merged PR reruns must be discoverable")
                runs = [self.sources[90]["run"]] if self.sources[90]["run"]["event"] == "pull_request" else []
            elif "branch=stable&" in path:
                runs = [self.sources[100]["run"]]
                if self.sources[90]["run"]["event"] != "pull_request":
                    runs.append(self.sources[90]["run"])
            else:
                runs = []
            return {"workflow_runs": copy.deepcopy(runs)}
        if path == f"repos/{REPO}/pulls/7":
            return copy.deepcopy(self.pr)
        if "/git/commits/" in path:
            return {"parents": [{"sha": sha} for sha in self.parents]}
        if path.startswith(f"repos/{REPO}/branches/"):
            return {"name": path.rsplit("/", 1)[1], "protected": True}
        for run_id, data in self.sources.items():
            prefix = f"repos/{REPO}/actions/runs/{run_id}"
            if path == prefix:
                return copy.deepcopy(data["run"])
            if path == prefix + "/artifacts?per_page=100":
                return {"artifacts": copy.deepcopy(self.artifacts[run_id])}
            if path == prefix + "/jobs?per_page=100":
                return {"jobs": [{"name": "styio-ci-gate", "check_run_url": f"https://api.github.com/check/{run_id}"}] +
                        [{"name": f"promotion / {platform}"} for platform in self.platform_jobs[run_id]]}
            if path == f"https://api.github.com/check/{run_id}":
                return copy.deepcopy(data["check"])
        raise AssertionError(f"unexpected API request: {path}")

    def fake_download(self, repo, artifact, destination):
        self.assertEqual(REPO, repo)
        run_id = artifact["workflow_run"]["id"]
        platform = artifact["name"].removeprefix("promotion-evidence-")
        self.downloaded.append((run_id, platform))
        destination.mkdir(parents=True, exist_ok=True)
        (destination / "evidence.json").write_text(json.dumps(self.manifests[run_id, platform]))
        return destination

    def collect(self, platform="linux"):
        accepted, notes = subject.prior_evidence(POLICY, "release", platform)
        self.assertEqual(1, len(accepted), notes)
        envelope, source, _ = accepted[0]
        self.assertEqual(100, source.run_id, "a failed run must not become a positive source handle")
        return envelope, source, notes

    def plan(self, envelope, source):
        tests = [CATALOG["tests"][0], CATALOG["tests"][2]]
        return subject.plan_tests(tests, {name: f"{name}-id" for name in ("compiler.smoke", "language.full")},
                                  envelope["tests"], [source])

    def assert_all_reusable(self, envelope, source):
        self.assertNotIn("invalidated_by", envelope)
        self.assertTrue(all(not record.get("invalidated_by") for record in envelope["tests"].values()))
        self.assertEqual([], self.plan(envelope, source)["run"])

    def test_matching_failed_skipped_or_unknown_test_invalidates_only_that_test(self):
        for status in ("failed", "skipped", "unknown"):
            with self.subTest(status=status):
                self.manifests[90, "linux"]["tests"]["compiler.smoke"]["status"] = status
                envelope, source, _ = self.collect()
                self.assertNotIn("invalidated_by", envelope)
                self.assertEqual(90, envelope["tests"]["compiler.smoke"]["invalidated_by"]["run_id"])
                self.assertNotIn("invalidated_by", envelope["tests"]["language.full"])
                plan = self.plan(envelope, source)
                self.assertEqual(["compiler.smoke"], plan["run"])
                self.assertEqual(["language.full"], list(plan["reuse"]))
                with self.assertRaises(ValueError):
                    subject.release_admission(CANDIDATE, "build-linux",
                        {name: f"{name}-id" for name in envelope["tests"]}, envelope, source,
                        "sealed-payload", platform="linux")

    def test_matching_failed_build_invalidates_platform_and_all_its_tests(self):
        self.manifests[90, "linux"]["phase"] = "build"
        envelope, source, _ = self.collect()
        self.assertEqual(90, envelope["invalidated_by"]["run_id"])
        self.assertEqual(2, envelope["invalidated_by"]["run_attempt"])
        self.assertEqual(["compiler.smoke", "language.full"], self.plan(envelope, source)["run"])

    def test_different_build_failure_does_not_invalidate_compatible_evidence(self):
        self.manifests[90, "linux"].update(build_id="different-build-context", phase="build")
        envelope, source, _ = self.collect()
        self.assert_all_reusable(envelope, source)

    def test_different_test_identity_failure_does_not_invalidate_older_compatible_test(self):
        self.manifests[90, "linux"]["tests"]["compiler.smoke"]["identity"] = "different-input-context"
        envelope, source, _ = self.collect()
        self.assert_all_reusable(envelope, source)

    def test_other_platform_failure_does_not_invalidate_this_platform_pass(self):
        self.manifests[90, "linux"]["status"] = "passed"
        self.manifests[90, "windows"]["phase"] = "build"
        envelope, source, _ = self.collect("linux")
        self.assert_all_reusable(envelope, source)
        self.assertNotIn((90, "windows"), self.downloaded)

    def test_linux_only_failed_job_cannot_invalidate_windows_or_macos(self):
        self.platform_jobs[90] = ["linux"]
        for platform in ("windows", "macos"):
            with self.subTest(platform=platform):
                envelope, source, _ = self.collect(platform)
                self.assert_all_reusable(envelope, source)
                self.assertNotIn((90, platform), self.downloaded)
        self.assertFalse(any("runs/90/artifacts" in path for path in self.api_calls))

    def test_failed_without_build_id_or_unknown_status_invalidates_unbounded_scope(self):
        for status, build_id in (("failed", None), ("unknown", "build-linux")):
            with self.subTest(status=status, build_id=build_id):
                self.manifests[90, "linux"].update(status=status, build_id=build_id)
                envelope, source, notes = self.collect()
                self.assertTrue(any("scope is unknown" in note for note in notes))
                self.assertEqual(90, envelope["invalidated_by"]["run_id"])
                self.assertEqual(["compiler.smoke", "language.full"], self.plan(envelope, source)["run"])

    def test_actual_failed_platform_job_with_missing_manifest_is_unknown_not_passed(self):
        self.artifacts[90] = [item for item in self.artifacts[90] if item["name"] != "promotion-evidence-linux"]
        envelope, source, notes = self.collect()
        self.assertTrue(notes)
        self.assertIn("invalidated_by", envelope)
        self.assertEqual(["compiler.smoke", "language.full"], self.plan(envelope, source)["run"])

    def merged_pr_failure(self):
        self.sources[90]["run"].update(event="pull_request", head_branch="nightly", pull_requests=[{"number": 7}])
        for platform in POLICY["acceptance_platforms"]:
            self.manifests[90, platform]["candidate_sha"] = "tested-pr-merge-sha"

    def test_failed_merged_same_repo_pr_rerun_invalidates_matching_test(self):
        self.merged_pr_failure()
        envelope, source, _ = self.collect()
        self.assertEqual(["compiler.smoke"], self.plan(envelope, source)["run"])
        self.assertIn((90, "linux"), self.downloaded)
        self.assertTrue(any("event=pull_request" in path and "status=success" not in path for path in self.api_calls))

    def test_unmerged_or_fork_pr_failure_has_no_invalidation_authority(self):
        self.merged_pr_failure()
        original = copy.deepcopy(self.pr)
        for fault in ("unmerged", "fork", "other_repo", "head_mismatch"):
            with self.subTest(fault=fault):
                self.pr = copy.deepcopy(original)
                if fault == "unmerged":
                    self.pr.update(merged=False, state="open", merged_at=None)
                elif fault == "fork":
                    self.pr["head"]["repo"]["fork"] = True
                elif fault == "other_repo":
                    self.pr["head"]["repo"]["full_name"] = "other/Styio"
                else:
                    self.pr["head"]["sha"] = "unrelated-head"
                envelope, source, _ = self.collect()
                self.assert_all_reusable(envelope, source)
                self.assertNotIn((90, "linux"), self.downloaded)

    def test_failed_pr_manifest_missing_parent_or_head_binding_is_not_bounded_evidence(self):
        self.merged_pr_failure()
        for fault in ("parent", "head"):
            with self.subTest(fault=fault):
                self.parents = ["unrelated-parent"] if fault == "parent" else ["base-sha", "source-90"]
                self.manifests[90, "linux"]["source_head_sha"] = "unrelated-head" if fault == "head" else "source-90"
                envelope, source, notes = self.collect()
                self.assertTrue(any("scope is unknown" in note for note in notes))
                self.assertIn("invalidated_by", envelope)
                self.assertEqual(["compiler.smoke", "language.full"], self.plan(envelope, source)["run"])


class AuxiliaryArtifactTrustTest(TemporaryRunnerTest):
    def setUp(self):
        super().setUp()
        self.source = verified("stable")
        self.live = metadata("stable")
        self.metadata = self.patch("source_metadata", side_effect=self.live_metadata)
        self.download = self.patch("download_artifact", side_effect=lambda repo, artifact, target: target)

    def live_metadata(self, repo, run, artifact):
        self.assertEqual(REPO, repo)
        result = copy.deepcopy(self.live)
        result["artifact"] = copy.deepcopy(artifact)
        return result

    def artifact(self, kind="payload", artifact_id=81):
        artifact = copy.deepcopy(self.live["artifact"])
        artifact.update(id=artifact_id, name=f"promotion-{kind}-linux",
                        url=f"https://api.github.com/repos/{REPO}/actions/artifacts/{artifact_id}")
        return artifact

    def download_named(self, artifacts, kind="payload"):
        return subject.download_named(POLICY, ({}, self.source, artifacts), f"promotion-{kind}-linux", self.root / "download")

    def test_build_and_payload_downloads_validate_the_actual_auxiliary_artifact(self):
        for kind in ("build", "payload"):
            with self.subTest(kind=kind):
                artifact = self.artifact(kind)
                self.metadata.reset_mock()
                self.download.reset_mock()
                self.download_named([artifact], kind)
                self.metadata.assert_called_once()
                self.assertEqual(artifact, self.metadata.call_args.args[-1])
                self.download.assert_called_once()
                self.assertEqual(artifact, self.download.call_args.args[1])

    def test_newest_matching_artifact_id_selected_instead_of_first(self):
        for kind in ("build", "payload"):
            with self.subTest(kind=kind):
                older, newest = self.artifact(kind, 81), self.artifact(kind, 85)
                expired = dict(self.artifact(kind, 90), expired=True)
                self.download.reset_mock()
                self.download_named([older, expired, self.artifact("unrelated", 100), newest], kind)
                self.assertEqual(newest, self.download.call_args.args[1])

    def test_auxiliary_artifact_rejects_expiry_wrong_run_head_and_stale_attempt(self):
        for kind in ("build", "payload"):
            for fault in ("expired", "expired_timestamp", "wrong_run", "wrong_head", "stale_attempt"):
                with self.subTest(kind=kind, fault=fault):
                    artifact = self.artifact(kind)
                    if fault == "expired":
                        artifact["expired"] = True
                    elif fault == "expired_timestamp":
                        artifact["expires_at"] = "2000-01-01T00:00:00Z"
                    elif fault == "wrong_run":
                        artifact["workflow_run"]["id"] = 999
                    elif fault == "wrong_head":
                        artifact["workflow_run"]["head_sha"] = "different-source"
                    else:
                        artifact["created_at"] = "2026-09-30T00:00:00Z"
                    self.download.reset_mock()
                    with self.assertRaises(ValueError):
                        self.download_named([artifact], kind)
                    self.download.assert_not_called()

    def test_auxiliary_download_rejects_live_failed_check_or_wrong_check_suite(self):
        for kind in ("build", "payload"):
            for fault in ("failed_check", "wrong_suite"):
                with self.subTest(kind=kind, fault=fault):
                    self.live = metadata("stable")
                    if fault == "failed_check":
                        self.live["check"]["conclusion"] = "failure"
                    else:
                        self.live["check"]["check_suite"]["id"] = 999
                    self.download.reset_mock()
                    with self.assertRaises(ValueError):
                        self.download_named([self.artifact(kind)], kind)
                    self.download.assert_not_called()

    def test_auxiliary_download_cannot_silently_rebind_to_another_valid_source(self):
        for kind in ("build", "payload"):
            for fault in ("run", "attempt", "head", "check"):
                with self.subTest(kind=kind, fault=fault):
                    self.live = metadata("stable")
                    if fault == "run":
                        self.live["run"]["id"] = 99
                        self.live["artifact"]["workflow_run"]["id"] = 99
                    elif fault == "attempt":
                        self.live["run"]["run_attempt"] = 2
                    elif fault == "head":
                        self.live["run"]["head_sha"] = "changed-source-head"
                        self.live["check"]["head_sha"] = "changed-source-head"
                        self.live["artifact"]["workflow_run"]["head_sha"] = "changed-source-head"
                    else:
                        self.live["check"]["id"] = 999
                    self.download.reset_mock()
                    with self.assertRaises(ValueError):
                        self.download_named([self.artifact(kind)], kind)
                    self.download.assert_not_called()


class RouteAndAggregateTest(TemporaryRunnerTest):
    def route(self, tier, head=None, repo=REPO, kind="pull_request", pulls=None):
        event = {"pull_request": {"base": {"ref": tier}, "head": {"ref": head, "repo": {"full_name": repo}}}}
        path, output = self.root / "event.json", self.root / "output"
        path.write_text(json.dumps(event))
        output.unlink(missing_ok=True)
        wanted = {"compiler.smoke": "current-test-identity"}
        envelope = {"fingerprint": FINGERPRINT, "build_id": subject.build_identity(INPUTS, FINGERPRINT),
                    "catalog": [], "identities": wanted, "distribution_inputs": INPUTS}
        artifacts = [{"name": f"promotion-{kind}-{platform}", "expired": False}
                     for platform in POLICY["acceptance_platforms"] for kind in ("evidence", "payload")]
        producers = getattr(self, "route_producers", [(envelope, verified("stable"), artifacts)])
        with mock.patch.dict(os.environ, {"GITHUB_EVENT_PATH": str(path), "GITHUB_EVENT_NAME": kind,
                                          "GITHUB_REPOSITORY": REPO, "GITHUB_REF_NAME": tier,
                                          "GITHUB_SHA": CANDIDATE, "GITHUB_OUTPUT": str(output)}), \
             mock.patch.object(subject, "api", return_value=pulls or []), \
             mock.patch.object(subject, "prior_evidence", return_value=(producers, [])), \
             mock.patch.object(subject, "git_inputs", return_value=INPUTS), \
             mock.patch.object(subject, "identities", return_value=wanted):
            self.assertEqual(0, subject.route_event(POLICY))
        return dict(line.split("=", 1) for line in output.read_text().splitlines())

    def test_route_nightly_linux_and_acceptance_all_three_platforms(self):
        for tier, head, platforms in (("nightly", "feature", ["linux"]), ("stable", "nightly", ["linux", "windows", "macos"]),
                                     ("release", "stable", ["linux", "windows", "macos"])):
            with self.subTest(tier=tier):
                output = self.route(tier, head)
                self.assertEqual(tier, output["tier"])
                self.assertEqual(platforms, [item["platform"] for item in json.loads(output["matrix"])["include"]])
                self.assertEqual("pinned-pafio", output["pafio_sha"])

    def test_release_route_requires_retained_complete_stable_producer(self):
        self.route_producers = []
        with self.assertRaisesRegex(ValueError, "three-platform stable producer"):
            self.route("release", "stable")

    def test_stable_release_reject_wrong_predecessor_or_fork(self):
        for tier, head, repo in (("stable", "feature", REPO), ("release", "nightly", REPO),
                                 ("stable", "nightly", "fork/Styio"), ("release", "stable", "fork/Styio")):
            with self.subTest(tier=tier, head=head, repo=repo), self.assertRaises(ValueError):
                self.route(tier, head, repo)

    def test_direct_promotion_push_requires_matching_merged_predecessor_pr(self):
        for tier, head in (("stable", "nightly"), ("release", "stable")):
            with self.subTest(tier=tier):
                with self.assertRaises(ValueError):
                    self.route(tier, kind="push")
                pull = {"merged_at": "2026-10-03T00:00:00Z", "merge_commit_sha": CANDIDATE,
                        "base": {"ref": tier}, "head": {"ref": head, "repo": {"full_name": REPO}}}
                self.route(tier, kind="push", pulls=[pull])
                for field, value in (("merged_at", None), ("merge_commit_sha", "other-sha")):
                    changed = copy.deepcopy(pull)
                    changed[field] = value
                    with self.assertRaises(ValueError):
                        self.route(tier, kind="push", pulls=[changed])

    def envelopes(self, tier="stable"):
        return [{"platform": platform, "candidate_sha": CANDIDATE, "tier": tier, "status": "passed",
                 "source": {"run_id": 42, "run_attempt": 1, "head_sha": "stable-sha"}, "distribution": {"status": "passed"}}
                for platform in POLICY["acceptance_platforms"]]

    def aggregate(self, envelopes, tier="stable"):
        directory = Path(tempfile.mkdtemp(dir=self.root))
        for index, envelope in enumerate(envelopes):
            path = directory / str(index) / "evidence.json"
            path.parent.mkdir()
            path.write_text(json.dumps(envelope))
        result = subject.aggregate(tier, directory, CANDIDATE, POLICY)
        return result, json.loads((directory / "admission.json").read_text())

    def test_aggregate_requires_real_current_candidate_all_platforms(self):
        result, admission = self.aggregate(self.envelopes())
        self.assertEqual(0, result)
        self.assertEqual("passed", admission["status"])
        self.assertEqual(set(POLICY["acceptance_platforms"]), set(admission["platforms"]))
        self.assertFalse(admission["published"])

    def test_aggregate_missing_failed_skipped_or_stale_windows_cannot_pass(self):
        for field, value in (("status", "failed"), ("status", "skipped"), ("status", "cancelled"), ("status", "unknown"),
                             ("candidate_sha", "other-sha"), ("tier", "nightly")):
            with self.subTest(field=field, value=value):
                envelopes = self.envelopes()
                envelopes[1][field], envelopes[1]["continue_on_error"] = value, True
                with self.assertRaises(ValueError):
                    self.aggregate(envelopes)
        with self.assertRaises(ValueError):
            self.aggregate([item for item in self.envelopes() if item["platform"] != "windows"])

    def test_aggregate_rejects_duplicate_or_unexpected_platform_evidence(self):
        for added in (self.envelopes()[0], dict(self.envelopes()[0], platform="other")):
            with self.subTest(added=added), self.assertRaises(ValueError):
                self.aggregate(self.envelopes() + [added])

    def test_nightly_aggregate_accepts_exactly_linux(self):
        self.assertEqual(0, self.aggregate(self.envelopes("nightly")[:1], "nightly")[0])
        with self.assertRaises(ValueError):
            self.aggregate(self.envelopes("nightly"), "nightly")

    def test_release_requires_same_stable_run_attempt_and_distribution_pass(self):
        self.assertEqual(0, self.aggregate(self.envelopes("release"), "release")[0])
        for key, value in (("run_id", 43), ("run_attempt", 2), ("head_sha", "other-stable-sha")):
            with self.subTest(key=key):
                envelopes = self.envelopes("release")
                envelopes[1]["source"][key] = value
                with self.assertRaises(ValueError):
                    self.aggregate(envelopes, "release")
        for distribution in ({}, {"status": "failed"}, {"status": "skipped"}):
            with self.subTest(distribution=distribution):
                envelopes = self.envelopes("release")
                envelopes[1]["distribution"] = distribution
                with self.assertRaises(ValueError):
                    self.aggregate(envelopes, "release")


class WorkflowContractTest(unittest.TestCase):
    def test_existing_required_check_identity_and_real_job_results_preserved(self):
        workflow = (REPO_ROOT / WORKFLOW).read_text()
        aggregate = workflow.split("\n  required-ci-gate:\n", 1)[1]
        for expected in ("    name: styio-ci-gate\n", "    if: always()\n", "    needs: [route, validate]\n",
                         'test "$ROUTE_RESULT" = success', 'test "$VALIDATION_RESULT" = success',
                         '${{ github.run_id }}', 'gh run download "$RUN_ID"', "--aggregate ../evidence"):
            self.assertIn(expected, aggregate)

    def test_no_platform_tolerated_and_evidence_uploaded_after_failures(self):
        workflow = (REPO_ROOT / WORKFLOW).read_text()
        platform_jobs = workflow.split("\n  validate:\n", 1)[1].split("\n  required-ci-gate:\n", 1)[0]
        self.assertNotIn("continue-on-error:", platform_jobs)
        # Diagnostic collection/report upload may continue; real platform
        # outcomes and current-candidate admission may never be tolerated.
        self.assertEqual(2, workflow.count("continue-on-error: true"))
        self.assertIn('test "$VALIDATION_RESULT" = success', workflow)
        self.assertIn("      fail-fast: false", workflow)
        self.assertIn("      matrix: ${{ fromJSON(needs.route.outputs.matrix) }}", workflow)
        self.assertIn("      - name: Preserve current-candidate evidence and advisory findings\n        if: always()", workflow)
        self.assertIn("python3 -m unittest discover -s tests -p 'promotion_*_test.py'", workflow)


if __name__ == "__main__":
    unittest.main()
