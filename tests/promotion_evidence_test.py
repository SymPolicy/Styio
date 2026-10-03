#!/usr/bin/env python3
"""Offline promotion identity, planning and admission regression tests."""
from __future__ import annotations

import copy
from datetime import datetime, timezone
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("promotion_evidence", ROOT / "scripts/promotion_evidence.py")
assert SPEC and SPEC.loader
pe = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = pe
SPEC.loader.exec_module(pe)

NOW = datetime(2026, 10, 3, tzinfo=timezone.utc)
REPO = "SymPolicy/Styio"
WORKFLOW = ".github/workflows/ci.yml"
FINGERPRINT = {"platform": "linux", "toolchain": {"compiler": "clang-19", "cmake": "3.31"},
               "environment": {"arch": "x64", "image": "ubuntu-24.04"},
               "dependencySHA": "abc123", "configuration": {"type": "Release"}}
POLICY = {"nightly_labels": ["smoke", "security"], "nightly_names": [],
          "scheduled_only_labels": {"soak": "Expensive soak runs in the dedicated scheduled workflow"}}
SMOKE = {"name": "compiler.smoke", "command": ["/build/styio", "--smoke"], "labels": ["smoke", "security"]}
SECURITY = {"name": "security.bounds", "command": ["/build/security", "bounds"], "labels": ["security"]}
FULL = {"name": "language.native", "command": ["/build/styio", "native"], "labels": ["language"]}
SOAK = {"name": "parser.soak", "command": ["/build/parser", "--soak"], "labels": ["soak", "security"]}


def api_metadata(branch="nightly"):
    return {
        "run": {"id": 42, "run_attempt": 1, "head_sha": "old-promotion-sha", "head_branch": branch,
                "run_started_at": "2026-10-01T10:00:00Z",
                "repository": {"full_name": REPO, "fork": False},
                "head_repository": {"full_name": REPO, "fork": False},
                "path": WORKFLOW, "status": "completed", "conclusion": "success", "event": "push",
                "check_suite_id": 900, "html_url": "https://github.com/SymPolicy/Styio/actions/runs/42"},
        "artifact": {"id": 51, "expired": False, "expires_at": "2026-11-03T00:00:00Z",
                     "created_at": "2026-10-01T10:05:00Z",
                     "workflow_run": {"id": 42, "head_sha": "old-promotion-sha"},
                     "url": "https://api.github.com/repos/SymPolicy/Styio/actions/artifacts/51"},
        "branch": {"name": branch, "protected": True},
        "check": {"id": 61, "head_sha": "old-promotion-sha", "check_suite": {"id": 900},
                  "status": "completed", "conclusion": "success",
                  "html_url": "https://github.com/SymPolicy/Styio/runs/61"},
    }


def merged_pr_metadata(target="stable"):
    metadata = api_metadata("feature/promotion")
    metadata["run"]["event"] = "pull_request"
    metadata["branch"] = {"name": target, "protected": True}
    metadata["pull_request"] = {
        "number": 123, "merged": True, "state": "closed", "merged_at": "2026-10-02T10:00:00Z",
        "head": {"ref": "feature/promotion", "sha": "old-promotion-sha",
                 "repo": {"full_name": REPO, "fork": False}},
        "base": {"ref": target, "sha": "base-sha", "repo": {"full_name": REPO, "fork": False}},
    }
    return metadata


def verified_pr(metadata=None):
    return pe.validate_source(metadata or merged_pr_metadata(), REPO, WORKFLOW,
                              {"nightly", "stable"}, {"push", "workflow_dispatch", "pull_request"}, now=NOW)


def verified(metadata=None):
    return pe.validate_source(metadata or api_metadata(), REPO, WORKFLOW,
                              {"nightly", "stable"}, {"push", "workflow_dispatch"}, now=NOW)


def record(name="compiler.smoke", identity="test-id", status="passed", source=None):
    return {"name": name, "identity": identity, "status": status,
            "source": (source or verified()).provenance()}


class GitIdentityTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.email", "offline-test@example.invalid")
        self.git("config", "user.name", "Offline Test")
        for path, content in {"src/compiler.cpp": "source", "CMakeLists.txt": "config",
                              "tests/compiler.styio": "fixture", "scripts/runner.py": "runner",
                              ".github/workflows/ci.yml": "workflow", "configs/promotion.json": "selector",
                              "docs/readme.md": "docs"}.items():
            self.write(path, content)
        self.git("add", ".")
        # Trees/commits exist only inside this disposable offline test repository.
        self.git("commit", "-qm", "fixture")
        self.build_paths = ["src", "CMakeLists.txt"]
        self.test_paths = ["tests", "scripts/runner.py"]
        self.policy_paths = [".github/workflows/ci.yml", "configs/promotion.json"]

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root).decode().strip()

    def write(self, path, content):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)

    def change(self, path, content="changed"):
        self.write(path, content)
        self.git("add", path)
        self.git("commit", "-qm", "change " + path)

    def identities(self, fingerprint=None):
        fingerprint = fingerprint or FINGERPRINT
        build = pe.build_identity(pe.git_inputs(self.root, self.build_paths), fingerprint)
        test = pe.test_identity(SMOKE, build, pe.git_inputs(self.root, self.test_paths), fingerprint,
                                pe.git_inputs(self.root, self.policy_paths))
        return build, test

    def test_uses_git_object_ids_not_rehashed_worktree(self):
        inputs = pe.git_inputs(self.root, ["src", "tests/*.styio"])
        self.assertEqual(self.git("rev-parse", "HEAD:src/compiler.cpp"), inputs["src/compiler.cpp"]["oid"])
        self.assertEqual({"src/compiler.cpp", "tests/compiler.styio"}, set(inputs))
        self.assertEqual("100644", inputs["src/compiler.cpp"]["mode"])
        self.write("src/compiler.cpp", "uncommitted is not trusted input")
        self.assertEqual(inputs, pe.git_inputs(self.root, ["src", "tests/*.styio"]))

    def test_promotion_sha_change_reuses_same_inputs(self):
        before_sha, identities = self.git("rev-parse", "HEAD"), self.identities()
        self.git("commit", "--allow-empty", "-qm", "promotion")
        self.assertNotEqual(before_sha, self.git("rev-parse", "HEAD"))
        self.assertEqual(identities, self.identities())
        evidence = record(identity=identities[1])
        plan = pe.plan_tests([SMOKE], {SMOKE["name"]: self.identities()[1]}, [evidence], [verified()], now=NOW)
        self.assertEqual([], plan["run"])

    def test_unrelated_docs_do_not_invalidate_compiler_tests(self):
        before = self.identities()
        self.change("docs/readme.md")
        self.assertEqual(before, self.identities())

    def test_source_and_configuration_invalidate_build_and_tests(self):
        for path in ("src/compiler.cpp", "CMakeLists.txt"):
            with self.subTest(path=path):
                before = self.identities()
                self.change(path)
                after = self.identities()
                self.assertNotEqual(before[0], after[0])
                self.assertNotEqual(before[1], after[1])

    def test_fixture_runner_workflow_and_selector_invalidate_tests_only(self):
        for path in (*self.test_paths, *self.policy_paths):
            if path == "tests":
                path = "tests/compiler.styio"
            with self.subTest(path=path):
                before = self.identities()
                self.change(path)
                after = self.identities()
                self.assertEqual(before[0], after[0])
                self.assertNotEqual(before[1], after[1])

    def test_compiler_dependency_environment_and_platform_changes_invalidate(self):
        before = self.identities()
        for section, key, value in (("toolchain", "compiler", "clang-20"),
                                    ("environment", "image", "ubuntu-26.04"),
                                    (None, "dependencySHA", "new-dependency"),
                                    (None, "platform", "windows")):
            with self.subTest(key=key):
                fingerprint = copy.deepcopy(FINGERPRINT)
                (fingerprint[section] if section else fingerprint)[key] = value
                after = self.identities(fingerprint)
                self.assertNotEqual(before[0], after[0])
                self.assertNotEqual(before[1], after[1])

    def test_added_deleted_and_mode_changed_input_invalidates(self):
        before = self.identities()
        self.change("src/added.cpp")
        self.assertNotEqual(before, self.identities())
        before = self.identities()
        self.git("rm", "src/added.cpp")
        self.git("commit", "-qm", "remove")
        self.assertNotEqual(before, self.identities())
        before = self.identities()
        self.git("update-index", "--chmod=+x", "src/compiler.cpp")
        self.git("commit", "-qm", "mode")
        self.assertNotEqual(before, self.identities())

    def test_canonical_fingerprint_order_and_missing_platform(self):
        before = self.identities()
        self.assertEqual(before, self.identities(dict(reversed(list(FINGERPRINT.items())))))
        fingerprint = dict(FINGERPRINT)
        del fingerprint["platform"]
        with self.assertRaises(ValueError):
            self.identities(fingerprint)

    def test_no_repository_root_selector(self):
        for path in (".", "*", "../other", "/outside"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                pe.git_inputs(self.root, [path])


class SelectionAndPlanningTest(unittest.TestCase):
    def test_label_overlap_selects_one_actual_test(self):
        nightly = pe.select_tests([SMOKE, SECURITY, FULL, SOAK, SMOKE], "nightly", POLICY)
        self.assertEqual(["compiler.smoke", "security.bounds"], [t["name"] for t in nightly])
        stable = pe.select_tests([SMOKE, SECURITY, FULL, SOAK], "stable", POLICY)
        self.assertEqual(["compiler.smoke", "language.native", "security.bounds"], [t["name"] for t in stable])

    def test_nightly_ignores_unselected_not_built_placeholders(self):
        placeholder = {"name": "styio_ide_test_NOT_BUILT", "properties": []}
        selected = pe.select_tests({"tests": [SMOKE, placeholder]}, "nightly", POLICY)
        self.assertEqual([SMOKE["name"]], [test["name"] for test in selected])
        with self.assertRaises(ValueError):
            pe.select_tests([SMOKE, placeholder], "stable", POLICY)

    def test_selected_missing_command_or_not_built_is_rejected(self):
        placeholders = [
            {"name": "security_test_NOT_BUILT", "properties": [{"name": "LABELS", "value": ["security"]}]},
            {"name": "security_test_NOT_BUILT", "command": ["security_test_NOT_BUILT"], "labels": ["security"]},
            {"name": "security.real", "labels": ["security"]},
        ]
        for placeholder in placeholders:
            with self.subTest(placeholder=placeholder), self.assertRaises(ValueError):
                pe.select_tests([SMOKE, placeholder], "nightly", POLICY)
        with self.assertRaises(ValueError):
            pe.select_tests([{"name": "named.smoke"}], "nightly", {"nightly_names": ["named.smoke"]})

    def test_ctest_json_properties_are_preserved(self):
        raw = {"tests": [{"name": "smoke", "command": ["check"], "properties": [
            {"name": "LABELS", "value": ["security", "smoke"]},
            {"name": "WORKING_DIRECTORY", "value": "/source"}]}]}
        selected = pe.select_tests(raw, "nightly", POLICY)
        self.assertEqual(["security", "smoke"], selected[0]["labels"])
        self.assertEqual({"WORKING_DIRECTORY": "/source"}, selected[0]["properties"])

    def test_conflicting_test_name_and_unreasoned_exclusion_rejected(self):
        changed = dict(SMOKE, command=["another-command"])
        with self.assertRaises(ValueError):
            pe.select_tests([SMOKE, changed], "stable", POLICY)
        with self.assertRaises(ValueError):
            pe.select_tests([SMOKE], "stable", dict(POLICY, scheduled_only_labels={"unit": ""}))

    def test_command_labels_and_properties_invalidate_test_identity(self):
        args = ("build", {"test": "oid"}, FINGERPRINT, {"policy": "oid"})
        baseline = pe.test_identity(SMOKE, *args)
        for change in ({"command": ["changed"]}, {"labels": ["unit"]},
                       {"properties": {"ENVIRONMENT": ["MODE=other"]}}):
            with self.subTest(change=change):
                self.assertNotEqual(baseline, pe.test_identity(dict(SMOKE, **change), *args))

    def test_stable_runs_only_missing_names_once(self):
        tests = pe.select_tests([SMOKE, SECURITY, FULL, SOAK], "stable", POLICY)
        ids = {test["name"]: "test-id" for test in tests}
        plan = pe.plan_tests(tests, ids, [record(), record(SECURITY["name"])], [verified()], now=NOW)
        self.assertEqual([FULL["name"]], plan["run"])
        self.assertEqual({SMOKE["name"], SECURITY["name"]}, set(plan["reuse"]))

    def test_failure_skip_cancel_unknown_and_tolerated_failure_are_missing(self):
        for status in ("failed", "skipped", "cancelled", "unknown", "tolerated-failure", None):
            with self.subTest(status=status):
                evidence = record(status=status)
                plan = pe.plan_tests([SMOKE], {SMOKE["name"]: "test-id"}, [evidence], [verified()], now=NOW)
                self.assertEqual([SMOKE["name"]], plan["run"])

    def test_newer_failed_record_cannot_hide_behind_older_pass(self):
        plan = pe.plan_tests([SMOKE], {SMOKE["name"]: "test-id"},
                             [record(), record(status="failed")], [verified()], now=NOW)
        self.assertEqual([SMOKE["name"]], plan["run"])

    def test_embedded_trust_claim_and_wrong_container_are_not_authority(self):
        evidence = dict(record(), trusted=True, verified=True)
        for sources in ([], [evidence["source"]]):
            plan = pe.plan_tests([SMOKE], {SMOKE["name"]: "test-id"}, [evidence], sources, now=NOW)
            self.assertEqual([SMOKE["name"]], plan["run"])
        evidence["source"]["artifact_id"] = 99
        plan = pe.plan_tests([SMOKE], {SMOKE["name"]: "test-id"}, [evidence], [verified()], now=NOW)
        self.assertEqual([SMOKE["name"]], plan["run"])

    def test_expiry_rechecked_at_planning_time(self):
        plan = pe.plan_tests([SMOKE], {SMOKE["name"]: "test-id"}, [record()], [verified()],
                             now="2026-12-03T00:00:00Z")
        self.assertEqual([SMOKE["name"]], plan["run"])

    def test_changed_identity_is_missing(self):
        plan = pe.plan_tests([SMOKE], {SMOKE["name"]: "different-id"}, [record()], [verified()], now=NOW)
        self.assertEqual([SMOKE["name"]], plan["run"])


class SourceTrustTest(unittest.TestCase):
    def test_trusts_successful_protected_terminal_source(self):
        source = verified()
        self.assertEqual(42, source.run_id)
        self.assertEqual(51, source.artifact_id)
        self.assertEqual(61, source.check_id)

    def test_untrusted_sources_fail_closed(self):
        mutations = [
            ("run", "path", "other-workflow"), ("run", "status", "in_progress"),
            ("run", "conclusion", "failure"), ("run", "conclusion", "cancelled"),
            ("run", "event", "pull_request"), ("run", "head_branch", "feature"),
            ("branch", "protected", False), ("branch", "name", "different"),
            ("artifact", "expired", True), ("artifact", "expires_at", "2026-10-02T00:00:00Z"),
            ("check", "conclusion", "failure"), ("check", "status", "queued"),
            ("check", "head_sha", "other-sha"), ("run", "run_attempt", 0),
        ]
        for section, key, value in mutations:
            with self.subTest(section=section, key=key, value=value), self.assertRaises(ValueError):
                metadata = api_metadata()
                metadata[section][key] = value
                verified(metadata)

    def test_cross_repository_fork_artifact_and_check_suite_rejected(self):
        for section, key, value in (("repository", "full_name", "elsewhere/Styio"),
                                    ("head_repository", "full_name", "fork/Styio"),
                                    ("repository", "fork", True), ("head_repository", "fork", True)):
            with self.subTest(section=section, key=key), self.assertRaises(ValueError):
                metadata = api_metadata()
                metadata["run"][section][key] = value
                verified(metadata)
        for section, child, key in (("artifact", "workflow_run", "id"),
                                    ("artifact", "workflow_run", "head_sha"),
                                    ("check", "check_suite", "id")):
            with self.subTest(section=section, key=key), self.assertRaises(ValueError):
                metadata = api_metadata()
                metadata[section][child][key] = "other"
                verified(metadata)

    def test_already_merged_same_repository_pr_uses_protected_target(self):
        source = verified_pr()
        self.assertEqual("stable", source.branch)
        self.assertEqual("old-promotion-sha", source.head_sha)
        reused = record(source=source)
        plan = pe.plan_tests([SMOKE], {SMOKE["name"]: "test-id"}, [reused], [source], now=NOW)
        self.assertEqual([], plan["run"])
        self.assertEqual(source.provenance(), plan["reuse"][SMOKE["name"]]["source"])

    def test_pr_event_requires_explicit_allowance_and_external_metadata(self):
        with self.assertRaises(ValueError):
            verified(merged_pr_metadata())
        metadata = merged_pr_metadata()
        del metadata["pull_request"]
        with self.assertRaises(ValueError):
            verified_pr(metadata)

    def test_unmerged_wrong_target_wrong_head_and_unprotected_pr_rejected(self):
        for key, value in (("merged", False), ("state", "open"), ("merged_at", None),
                           ("merged_at", "2026-10-04T00:00:00Z")):
            with self.subTest(key=key), self.assertRaises(ValueError):
                metadata = merged_pr_metadata()
                metadata["pull_request"][key] = value
                verified_pr(metadata)
        with self.assertRaises(ValueError):
            verified_pr(merged_pr_metadata("feature/other"))
        metadata = merged_pr_metadata()
        metadata["pull_request"]["head"]["sha"] = "untested-head-sha"
        with self.assertRaises(ValueError):
            verified_pr(metadata)
        metadata = merged_pr_metadata()
        metadata["branch"]["protected"] = False
        with self.assertRaises(ValueError):
            verified_pr(metadata)
        metadata = merged_pr_metadata()
        metadata["branch"]["name"] = "nightly"
        with self.assertRaises(ValueError):
            verified_pr(metadata)

    def test_merged_fork_or_cross_repository_pr_is_rejected(self):
        for side in ("head", "base"):
            for key, value in (("full_name", "other/Styio"), ("fork", True)):
                with self.subTest(side=side, key=key), self.assertRaises(ValueError):
                    metadata = merged_pr_metadata()
                    metadata["pull_request"][side]["repo"][key] = value
                    verified_pr(metadata)
        metadata = merged_pr_metadata()
        metadata["run"]["head_repository"]["fork"] = True
        with self.assertRaises(ValueError):
            verified_pr(metadata)

    def test_previous_attempt_artifact_is_rejected_for_push_and_merged_pr(self):
        for factory, validator in ((api_metadata, verified), (merged_pr_metadata, verified_pr)):
            with self.subTest(event=factory.__name__), self.assertRaises(ValueError):
                metadata = factory()
                metadata["run"]["run_attempt"] = 2
                metadata["run"]["run_started_at"] = "2026-10-02T12:00:00Z"
                validator(metadata)
        metadata = api_metadata()
        metadata["artifact"]["created_at"] = metadata["run"]["run_started_at"]
        self.assertEqual(42, verified(metadata).run_id)
        for section, key in (("artifact", "created_at"), ("run", "run_started_at")):
            with self.subTest(section=section), self.assertRaises(ValueError):
                metadata = api_metadata()
                del metadata[section][key]
                verified(metadata)

    def test_missing_deleted_metadata_is_rejected(self):
        for key in ("run", "artifact", "branch", "check"):
            with self.subTest(key=key), self.assertRaises(ValueError):
                metadata = api_metadata()
                del metadata[key]
                verified(metadata)


class ReleaseAdmissionTest(unittest.TestCase):
    def platform(self, platform="linux"):
        source = verified(api_metadata("stable"))
        ids = {"compiler.smoke": "test-" + platform}
        envelope = {"tier": "stable", "status": "passed", "platform": platform,
                    "build_id": "build-" + platform, "payload_id": "payload-" + platform,
                    "tests": {name: {"identity": identity, "status": "passed"} for name, identity in ids.items()},
                    "source": source.provenance()}
        return {"build_id": envelope["build_id"], "identities": ids, "envelope": envelope,
                "source": source, "payload_id": envelope["payload_id"]}

    def admit(self, item=None, platform="linux"):
        return pe.release_admission("new-promotion-sha", **(item or self.platform(platform)),
                                    platform=platform, now=NOW)

    def test_current_candidate_names_reused_run_artifact_check_without_retest(self):
        result = self.admit()
        self.assertEqual("new-promotion-sha", result["candidate_sha"])
        self.assertEqual("old-promotion-sha", result["reused_source"]["head_sha"])
        self.assertEqual((42, 51, 61), tuple(result["reused_source"][key] for key in
                                           ("run_id", "artifact_id", "check_id")))
        self.assertFalse(result["build_required"])
        self.assertEqual([], result["run"])

    def test_payload_build_platform_and_scope_mismatch_fail_closed(self):
        for key, value in (("payload_id", "different"), ("build_id", "different"),
                           ("platform", "windows"), ("tier", "nightly"), ("status", "failed"),
                           ("tests", {})):
            with self.subTest(key=key), self.assertRaises(ValueError):
                item = self.platform()
                item["envelope"][key] = value
                self.admit(item)
        for status in ("failed", "skipped", "cancelled", "unknown", "tolerated-failure"):
            with self.subTest(status=status), self.assertRaises(ValueError):
                item = self.platform()
                item["envelope"]["tests"]["compiler.smoke"]["status"] = status
                self.admit(item)

    def test_changed_test_identity_extra_test_and_wrong_branch_fail_closed(self):
        item = self.platform()
        item["identities"]["compiler.smoke"] = "changed"
        with self.assertRaises(ValueError):
            self.admit(item)
        item = self.platform()
        item["envelope"]["tests"]["extra"] = {"status": "passed", "identity": "extra"}
        with self.assertRaises(ValueError):
            self.admit(item)
        item = self.platform()
        item["source"] = verified(api_metadata("nightly"))
        item["envelope"]["source"] = item["source"].provenance()
        with self.assertRaises(ValueError):
            self.admit(item)

    def test_all_three_platforms_mandatory(self):
        platforms = {p: self.platform(p) for p in pe.PLATFORMS}
        result = pe.release_admissions("candidate", platforms, now=NOW)
        self.assertEqual(pe.PLATFORMS, set(result["platforms"]))
        self.assertEqual("passed", result["status"])
        del platforms["windows"]
        with self.assertRaises(ValueError):
            pe.release_admissions("candidate", platforms, now=NOW)

    def test_mixing_stable_runs_is_not_all_platform_admission(self):
        platforms = {p: self.platform(p) for p in pe.PLATFORMS}
        metadata = api_metadata("stable")
        metadata["run"]["id"] = 43
        metadata["artifact"]["workflow_run"]["id"] = 43
        different_source = verified(metadata)
        platforms["windows"]["source"] = different_source
        platforms["windows"]["envelope"]["source"] = different_source.provenance()
        with self.assertRaises(ValueError):
            pe.release_admissions("candidate", platforms, now=NOW)

    def test_cross_os_payload_substitution_and_tolerated_windows_failure_rejected(self):
        platforms = {p: self.platform(p) for p in pe.PLATFORMS}
        platforms["windows"] = self.platform("linux")
        with self.assertRaises(ValueError):
            pe.release_admissions("candidate", platforms, now=NOW)
        platforms = {p: self.platform(p) for p in pe.PLATFORMS}
        platforms["windows"]["envelope"]["tests"]["compiler.smoke"]["status"] = "tolerated-failure"
        with self.assertRaises(ValueError):
            pe.release_admissions("candidate", platforms, now=NOW)


if __name__ == "__main__":
    unittest.main()
