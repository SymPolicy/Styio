#!/usr/bin/env python3
"""Regression evidence for documentation intent and maintenance ownership."""
from __future__ import annotations

import copy
import importlib.util
import sys
import tempfile
import tomllib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))


def load_module(name: str, path: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


audit = load_module("docs_audit", "scripts/docs-audit.py")
teams = load_module("team_docs_gate", "scripts/team-docs-gate.py")


class DesignIntentContractTest(unittest.TestCase):
    def setUp(self):
        self.text = (ROOT / audit.DESIGN_INTENT_DOC).read_text(encoding="utf-8")
        blocks = audit.DESIGN_INTENT_RE.findall(self.text)
        self.assertEqual(len(blocks), 1)
        self.data = tomllib.loads(blocks[0])

    def assert_invalid(self, fragment: str):
        errors = audit.validate_design_intent(self.data, ROOT)
        self.assertTrue(any(fragment in error for error in errors), errors)

    def test_current_contract_and_single_authority(self):
        errors = []
        audit.check_design_intent_contract(errors)
        self.assertEqual(errors, [])

    def test_unknown_fields_do_not_create_a_second_grammar(self):
        self.data["syntax"] = "invented grammar"
        self.assert_invalid("unknown fields")

    def test_schema_boolean_is_not_integer_version(self):
        self.data["schema_version"] = True
        self.assert_invalid("integer 1")

    def test_normative_requirement_cannot_silently_be_disabled(self):
        self.data["requirements"]["visual_programming_language"] = False
        self.assert_invalid("must be true")

    def test_required_principle_and_trace_are_not_optional(self):
        del self.data["requirements"]["ordinary_compiler_layers"]
        del self.data["traceability"]["ordinary_compiler_layers"]
        self.assert_invalid("unknown fields")

    def test_missing_anchor(self):
        self.data["traceability"]["visual_programming_language"]["authority"] += "-missing"
        self.assert_invalid("missing anchor")

    def test_trace_requires_precise_section(self):
        self.data["traceability"]["visual_programming_language"]["authority"] = str(audit.DESIGN_INTENT_DOC)
        self.assert_invalid("section anchor")

    def test_unknown_capability(self):
        self.data["traceability"]["visual_programming_language"]["affected_capabilities"] = ["invented"]
        self.assert_invalid("unknown capability")

    def test_identity_and_research_routes_do_not_claim_capability_evidence(self):
        for name in ("general_purpose_language", "visual_programming_language", "research_claims_need_evidence"):
            self.assertEqual(self.data["traceability"][name]["affected_capabilities"], [])
        self.assertEqual(audit.validate_design_intent(self.data, ROOT), [])

    def test_trace_rejects_capabilities_as_evidence(self):
        trace = self.data["traceability"]["visual_programming_language"]
        trace["capabilities"] = trace.pop("affected_capabilities")
        self.assert_invalid("unknown fields")

    def test_trace_requires_list_of_affected_capabilities(self):
        self.data["traceability"]["compiler_owns_semantic_facts"]["affected_capabilities"] = "program_structure"
        self.assert_invalid("list of affected capability IDs")

    def test_unreferenced_capability(self):
        self.data["capabilities"]["orphan"] = copy.deepcopy(self.data["capabilities"]["program_structure"])
        self.assert_invalid("no design-principle traceability")

    def test_unknown_status_and_wrong_type(self):
        for value in ("ready", [], False):
            with self.subTest(value=value):
                self.data["capabilities"]["program_structure"]["status"] = value
                self.assert_invalid("unknown implementation status")

    def test_implemented_scope_requires_code_and_tests(self):
        self.data["capabilities"]["observable_service"]["evidence"] = ["src/StyioServices/StyioObservable/Snapshot.cpp"]
        self.assert_invalid("implementation and test evidence")

    def test_scope_and_gap_are_explicit(self):
        self.data["capabilities"]["program_structure"]["scope"] = ""
        self.data["capabilities"]["program_structure"]["gap"] = ""
        self.assert_invalid("explicit scope")
        self.assert_invalid("non-empty repository reference")

    def test_missing_evidence_file(self):
        self.data["capabilities"]["program_structure"]["evidence"].append("tests/nonexistent-intent-proof.cpp")
        self.assert_invalid("missing file")

    def test_paths_cannot_escape_repository(self):
        self.data["capabilities"]["program_structure"]["owner"] = "../outside.md"
        self.assert_invalid("inside the repository")

    def test_duplicate_authority_and_malformed_toml(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / audit.DESIGN_INTENT_DOC
            path.parent.mkdir(parents=True)
            path.write_text(self.text, encoding="utf-8")
            (path.parent / "Duplicate.md").write_text(self.text, encoding="utf-8")
            errors = []
            audit.check_design_intent_contract(errors, root)
            self.assertTrue(any("duplicate design-intent authority" in error for error in errors))
            path.write_text("```toml design-intent\nx = [\n```\n", encoding="utf-8")
            errors = []
            audit.check_design_intent_contract(errors, root)
            self.assertTrue(any("invalid design-intent TOML" in error for error in errors))

    def test_duplicate_block_and_absent_block(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / audit.DESIGN_INTENT_DOC
            path.parent.mkdir(parents=True)
            for text in (self.text + self.text, "# No contract\n"):
                with self.subTest(text_length=len(text)):
                    path.write_text(text, encoding="utf-8")
                    errors = []
                    audit.check_design_intent_contract(errors, root)
                    self.assertIn("language design must contain exactly one toml design-intent block", errors)


class ObservableOwnershipTest(unittest.TestCase):
    def test_owned_paths_trigger_expected_maintainers(self):
        cases = {
            "src/StyioServices/StyioObservable/Snapshot.cpp": {"sema_ir"},
            "src/StyioServices/StyioObservable/RuntimeCorrelation.cpp": {"sema_ir", "codegen_runtime"},
            "src/StyioServices/StyioObservableProducer/StaticSnapshotPublication.cpp": {"sema_ir", "cli_nano"},
            "src/StyioUtil/SemanticIdentity.cpp": {"sema_ir"},
            "src/StyioUtil/SemanticIdentity.hpp": {"sema_ir"},
            "src/StyioNative/NativeInterop.cpp": {"codegen_runtime"},
            "src/cmake/StyioBackendSources.cmake": {"codegen_runtime"},
            "src/CMakeLists.txt": {"coordination"},
            "tests/observable_static_snapshot_test.cpp": {"test_quality"},
        }
        for path, expected in cases.items():
            with self.subTest(path=path):
                actual = {rule.key for rule in teams.required_team_updates([Path(path)])}
                self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
