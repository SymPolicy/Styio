#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GATE_PATH = ROOT / "scripts" / "syntax-feature-state-gate.py"
SPEC = importlib.util.spec_from_file_location("syntax_feature_state_gate", GATE_PATH)
assert SPEC is not None and SPEC.loader is not None
gate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = gate
SPEC.loader.exec_module(gate)


class SyntaxFeatureStateGateTest(unittest.TestCase):
    def make_root(self) -> tuple[tempfile.TemporaryDirectory[str], Path, Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        features = root / "docs" / "design" / "syntax" / "features"
        features.mkdir(parents=True)

        (root / "support.md").write_text("# Support\n", encoding="utf-8")
        (root / "impl.cpp").write_text("void feature_symbol() {}\n", encoding="utf-8")
        golden = root / "tests" / "features" / "sample"
        (golden / "expected").mkdir(parents=True)
        (golden / "t01.styio").write_text(">_(1)\n", encoding="utf-8")
        (golden / "expected" / "t01.out").write_text("1\n", encoding="utf-8")
        return temporary, root, features

    def write_feature(
        self,
        features: Path,
        feature_id: str,
        *,
        decision_state: str = "accepted",
        delivery_state: str = "converged",
        requires: str = "",
        supersedes: str = "",
        include_delivery_artifacts: bool = True,
    ) -> None:
        requirement = (
            f'{{ id = "{requires}", decision_state = "accepted", '
            'delivery_state = "converged" }'
            if requires
            else ""
        )
        supersedes_entry = f'"{supersedes}"' if supersedes else ""
        path = features / f"{feature_id.replace('.', '-')}.md"
        document = textwrap.dedent(
            f"""\
            # {feature_id}

            **Purpose:** Test feature document.

            **Last updated:** 2026-07-30

            ## Feature Contract

            ```toml syntax-feature
            schema_version = 1
            id = "{feature_id}"
            title = "{feature_id}"
            kind = "test-syntax"
            decision_state = "{decision_state}"
            delivery_state = "{delivery_state}"
            owner = "Test"
            syntax = "test"
            resolution = "test resolution"
            golden_cases = ["tests/features/sample/t01.styio"]

            [documents]
            grammar = ["support.md"]
            tokens = ["support.md"]
            semantics = ["support.md"]
            diagnostics = ["support.md"]
            compatibility = ["support.md"]
            teaching = ["support.md"]
            implementation = ["impl.cpp"]
            evidence = ["tests/features/sample/t01.styio"]

            [prerequisites]
            language-owner-approval = "support.md"
            keyword-free-contract = "support.md"
            nightly-parser-authority = "support.md"
            grammar-contract = "support.md"
            semantic-contract = "support.md"
            diagnostic-boundary = "support.md"
            compatibility-decision = "support.md"
            golden-evidence = "tests/features/sample/t01.styio"

            [implementation]
            path = "impl.cpp"
            symbol = "feature_symbol"
            owner = "Test"

            [dependencies]
            requires = [{requirement}]
            requires_any = []
            extends = []
            conflicts = []
            supersedes = [{supersedes_entry}]
            after = []
            ```

            ## Decision

            Test.
            """
        )
        if not include_delivery_artifacts:
            document = document.replace(
                'golden_cases = ["tests/features/sample/t01.styio"]',
                "golden_cases = []",
            )
            document = document.replace(
                'implementation = ["impl.cpp"]\n'
                'evidence = ["tests/features/sample/t01.styio"]\n',
                "",
            )
            document = document.replace(
                'golden-evidence = "tests/features/sample/t01.styio"\n',
                "",
            )
            document = document.replace(
                '[implementation]\n'
                'path = "impl.cpp"\n'
                'symbol = "feature_symbol"\n'
                'owner = "Test"\n\n',
                "",
            )
        path.write_text(document, encoding="utf-8")

    def test_ready_feature_renders_generated_projection(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(features_dir, "core.ready")

        features, derived, errors = gate.evaluate(root, features_dir)

        self.assertEqual([], errors)
        self.assertEqual("ready", derived["core.ready"]["readiness"])
        rendered = gate.render_graph(root, features, derived)
        self.assertIn('"generated": true', rendered)
        self.assertIn('"document": "docs/design/syntax/features/core-ready.md"', rendered)

    def test_dependency_cycle_is_rejected(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(
            features_dir,
            "core.first",
            delivery_state="not_started",
            requires="core.second",
        )
        self.write_feature(
            features_dir,
            "core.second",
            delivery_state="not_started",
            requires="core.first",
        )

        _, derived, errors = gate.evaluate(root, features_dir)

        self.assertTrue(
            any("dependency cycle" in error for error in errors),
            errors,
        )
        self.assertEqual("blocked", derived["core.first"]["readiness"])
        self.assertEqual("blocked", derived["core.second"]["readiness"])

    def test_cycle_detection_does_not_depend_on_python_recursion_depth(self) -> None:
        node_count = 1_500
        graph = {
            f"node-{index}": {f"node-{index + 1}"}
            for index in range(node_count - 1)
        }
        graph[f"node-{node_count - 1}"] = {"node-0"}

        components = gate.strongly_connected_components(graph)

        self.assertEqual(1, len(components))
        self.assertEqual(node_count, len(components[0]))

    def test_generated_projection_contains_reverse_dependency_edges(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(features_dir, "core.base")
        self.write_feature(
            features_dir,
            "core.dependent",
            delivery_state="not_started",
            requires="core.base",
        )

        features, derived, errors = gate.evaluate(root, features_dir)
        payload = json.loads(gate.render_graph(root, features, derived))
        by_id = {feature["id"]: feature for feature in payload["features"]}

        self.assertEqual([], errors)
        self.assertEqual(
            ["core.dependent"],
            by_id["core.base"]["referenced_by"]["requires"],
        )

    def test_delivery_cannot_advance_past_blocked_dependency(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(
            features_dir,
            "core.base",
            delivery_state="not_started",
        )
        self.write_feature(
            features_dir,
            "core.dependent",
            requires="core.base",
        )

        _, derived, errors = gate.evaluate(root, features_dir)

        self.assertEqual("stale", derived["core.dependent"]["readiness"])
        self.assertTrue(
            any(
                "cannot be `converged` while readiness is `stale`" in error
                for error in errors
            ),
            errors,
        )

    def test_nonaccepted_decision_cannot_advance_delivery(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(
            features_dir,
            "core.reserved",
            decision_state="reserved",
            delivery_state="implementing",
        )

        _, _, errors = gate.evaluate(root, features_dir)

        self.assertTrue(
            any("may advance delivery only" in error for error in errors),
            errors,
        )

    def test_draft_branch_may_remain_incomplete_before_delivery(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(
            features_dir,
            "core.draft",
            decision_state="draft",
            delivery_state="not_started",
            include_delivery_artifacts=False,
        )

        _, derived, errors = gate.evaluate(root, features_dir)

        self.assertEqual([], errors)
        self.assertEqual("incomplete", derived["core.draft"]["readiness"])
        self.assertEqual(
            ["golden_cases", "implementation.path", "implementation.symbol"],
            derived["core.draft"]["missing_artifacts"],
        )
        self.assertEqual(
            ["evidence", "implementation"],
            derived["core.draft"]["missing_document_roles"],
        )
        self.assertEqual(
            ["golden-evidence"],
            derived["core.draft"]["missing_prerequisites"],
        )

    def test_incomplete_accepted_feature_cannot_start_delivery(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(
            features_dir,
            "core.incomplete",
            decision_state="accepted",
            delivery_state="implementing",
            include_delivery_artifacts=False,
        )

        _, derived, errors = gate.evaluate(root, features_dir)

        self.assertEqual("incomplete", derived["core.incomplete"]["readiness"])
        self.assertTrue(
            any(
                "cannot be `implementing` while readiness is `incomplete`" in error
                for error in errors
            ),
            errors,
        )

    def test_replacement_is_blocked_until_target_is_superseded(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(features_dir, "core.original")
        self.write_feature(
            features_dir,
            "core.replacement",
            delivery_state="implementing",
            supersedes="core.original",
        )

        _, derived, errors = gate.evaluate(root, features_dir)

        self.assertEqual("blocked", derived["core.replacement"]["readiness"])
        self.assertTrue(
            any(
                "cannot be `implementing` while readiness is `blocked`" in error
                for error in errors
            ),
            errors,
        )

    def test_contract_paths_cannot_escape_repository(self) -> None:
        temporary, root, features_dir = self.make_root()
        self.addCleanup(temporary.cleanup)
        self.write_feature(features_dir, "core.external-path")
        feature_path = features_dir / "core-external-path.md"
        document = feature_path.read_text(encoding="utf-8").replace(
            "tests/features/sample/t01.styio",
            "/private/example.styio",
        )
        feature_path.write_text(document, encoding="utf-8")

        _, _, errors = gate.evaluate(root, features_dir)

        self.assertTrue(
            any("must stay inside the repository" in error for error in errors),
            errors,
        )
        self.assertNotIn("/private/example.styio", "\n".join(errors))


if __name__ == "__main__":
    unittest.main()
