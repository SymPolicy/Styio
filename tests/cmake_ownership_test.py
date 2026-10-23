#!/usr/bin/env python3
"""Positive and negative checks for target-scoped runbook maintenance routing."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("team_docs_gate", ROOT / "scripts/team-docs-gate.py")
assert spec and spec.loader
teams = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = teams
spec.loader.exec_module(teams)

# Exact equality checks both required owners and the absence of unrelated owners.
MANIFEST_ROLES = {
    "StyioTargetHelpers": {"frontend", "sema_ir", "codegen_runtime", "cli_nano", "ide_lsp"},
    "StyioSymbolSources": {"frontend"},
    "StyioFrontendFoundationSources": {"frontend"},
    "StyioFrontendProfilerSources": {"perf_stability"},
    "StyioSemaIRSources": {"sema_ir"},
    "StyioNativeInteropSources": {"codegen_runtime"},
    "StyioFrontendSources": {"frontend", "sema_ir", "codegen_runtime", "perf_stability"},
    "StyioBackendSources": {"codegen_runtime"},
    "StyioRuntimeSources": {"codegen_runtime"},
    "StyioTestingSources": {"test_quality"},
    "StyioCoreSources": {"codegen_runtime", "test_quality"},
    "StyioRuntimeCorrelationSources": {"sema_ir", "codegen_runtime"},
    "StyioObservableSources": {"sema_ir", "codegen_runtime"},
    "StyioObservableProducerSources": {"sema_ir", "cli_nano"},
    "StyioCLIContractSources": {"sema_ir", "cli_nano"},
    "StyioIDESources": {"ide_lsp"},
    "StyioLSPSources": {"ide_lsp"},
    "StyioNanoCoreSources": {"cli_nano"},
    "targets/StyioSymbolCore": {"frontend"},
    "targets/StyioFrontendCore": {"frontend", "sema_ir", "codegen_runtime", "perf_stability"},
    "targets/StyioRuntimeCore": {"codegen_runtime"},
    "targets/StyioCore": {"codegen_runtime", "test_quality"},
    "targets/StyioObservableCore": {"sema_ir", "codegen_runtime"},
    "targets/StyioCLIContractCore": {"sema_ir", "cli_nano"},
    "targets/StyioIDECore": {"ide_lsp", "grammar"},
    "targets/StyioLSPD": {"ide_lsp"},
    "targets/Styio": {"cli_nano"},
    "targets/StyioNanoCore": {"cli_nano"},
    "targets/StyioNano": {"cli_nano"},
}


class CMakeOwnershipTest(unittest.TestCase):
    def owners(self, *paths):
        return {rule.key for rule in teams.required_team_updates(map(Path, paths))}

    def test_all_current_cmake_files_have_explicit_reviewed_ownership(self):
        expected = {"src/CMakeLists.txt", *(f"src/cmake/{name}.cmake" for name in MANIFEST_ROLES)}
        actual = {path.relative_to(ROOT).as_posix() for path in (ROOT / "src/cmake").rglob("*.cmake")}
        actual.add("src/CMakeLists.txt")
        self.assertEqual(actual, expected)
        self.assertEqual(set(teams.CMAKE_ROLES), expected)
        self.assertTrue(all(set(roles) <= {rule.key for rule in teams.TEAM_RULES}
                            for roles in teams.CMAKE_ROLES.values()))

    def test_each_manifest_triggers_only_its_owners(self):
        for name, expected in MANIFEST_ROLES.items():
            with self.subTest(manifest=name):
                self.assertEqual(self.owners(f"src/cmake/{name}.cmake"), expected)

    def test_entry_point_and_new_manifests_require_coordination(self):
        self.assertEqual(self.owners("src/CMakeLists.txt"), {"coordination"})
        self.assertEqual(self.owners("src/cmake/targets/NewTarget.cmake"), {"coordination"})

    def test_regular_edits_do_not_trigger_transitive_consumers(self):
        cases = {
            "src/StyioParser/Parser.cpp": {"frontend"},
            "src/StyioProfiler/FrontendProfiler.cpp": {"perf_stability"},
            "src/StyioServices/StyioIDE/Common.cpp": {"ide_lsp"},
            "src/StyioServices/StyioObservable/Snapshot.cpp": {"sema_ir"},
            "src/StyioServices/StyioObservable/RuntimeCorrelation.cpp": {"sema_ir", "codegen_runtime"},
            "src/StyioServices/StyioObservableProducer/StaticSnapshotPublication.cpp": {"sema_ir", "cli_nano"},
            "src/StyioUtil/SemanticIdentity.cpp": {"sema_ir"},
            "src/StyioUtil/SourceMap.cpp": {"frontend"},
            "src/StyioNative/NativeInterop.cpp": {"codegen_runtime"},
            "src/StyioServices/StyioIDE/TreeSitterBackend.cpp": {"ide_lsp", "grammar"},
        }
        for path, expected in cases.items():
            with self.subTest(path=path):
                self.assertEqual(self.owners(path), expected)

    def test_independent_edits_combine_only_their_owners(self):
        self.assertEqual(self.owners("src/cmake/StyioIDESources.cmake",
                                    "src/cmake/StyioBackendSources.cmake"),
                         {"ide_lsp", "codegen_runtime"})

    def test_lookalike_paths_do_not_match_named_manifest(self):
        self.assertEqual(self.owners("src/cmake/StyioIDESources.cmake.bak"), set())
        self.assertEqual(self.owners("src/cmake/StyioIDESources.cmake/notes.txt"), set())

    def test_runbook_and_generated_inventory_do_not_cascade(self):
        self.assertEqual(self.owners("docs/teams/IDE-LSP-RUNBOOK.md", "docs/teams/DOC-STATS.md",
                                    "docs/teams/INDEX.md"), set())

    def test_single_owner_delivery_passes_without_other_runbooks(self):
        changed = [Path(path) for path in ("src/cmake/StyioIDESources.cmake",
                    "docs/teams/IDE-LSP-RUNBOOK.md", "docs/teams/DOC-STATS.md")]
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(teams.run_gate(changed, False), 0)

    def test_missing_required_shared_owner_fails(self):
        changed = [Path(path) for path in ("src/cmake/StyioRuntimeCorrelationSources.cmake",
                    "docs/teams/SEMA-IR-RUNBOOK.md", "docs/teams/DOC-STATS.md")]
        error = io.StringIO()
        with contextlib.redirect_stderr(error):
            self.assertEqual(teams.run_gate(changed, False), 1)
        self.assertIn("CODEGEN-RUNTIME-RUNBOOK.md", error.getvalue())
        self.assertNotIn("CLI-NANO-RUNBOOK.md", error.getvalue())
        self.assertNotIn("IDE-LSP-RUNBOOK.md", error.getvalue())

    def test_rename_keeps_both_owners_but_copy_changes_only_destination(self):
        src = "src/cmake/StyioBackendSources.cmake"
        dst = "src/cmake/StyioIDESources.cmake"
        rename = teams.parse_name_status(f"R100\t{src}\t{dst}\n")
        self.assertEqual(self.owners(*(str(path) for path in rename)), {"codegen_runtime", "ide_lsp"})
        copied = teams.parse_name_status(f"C100\t{src}\t{dst}\n")
        self.assertEqual(self.owners(*(str(path) for path in copied)), {"ide_lsp"})


if __name__ == "__main__":
    unittest.main()
