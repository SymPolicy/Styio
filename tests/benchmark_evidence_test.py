#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "benchmark-compare.py"


def _load_compare_module():
    spec = importlib.util.spec_from_file_location("benchmark_compare", SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"failed to load benchmark compare script: {SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BenchmarkEvidenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compare = _load_compare_module()

    def test_load_results_preserves_route_cache_and_ir_allocations(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            payload = {
                "schema": "styio.benchmark.v1",
                "git_sha": "deadbeef",
                "build_type": "Release",
                "timestamp": 1,
                "samples": [
                    {
                        "phase": "route_cache",
                        "label": "name_resolution_5k",
                        "duration_ns": 0,
                        "route_cache_scan_count": 5001,
                        "route_cache_miss_count": 5001,
                    },
                    {
                        "phase": "sema",
                        "label": "name_resolution_5k",
                        "duration_ns": 123,
                        "ir_arena_allocations": 77,
                        "ir_raw_allocations": 2,
                        "ir_bytes_allocated": 4096,
                        "ir_node_count": 79,
                        "ir_max_node_count": 79,
                        "ir_destructor_calls": 79,
                    },
                    {
                        "phase": "scheduler",
                        "label": "task_queue_mode",
                        "duration_ns": 0,
                        "task_scheduler_queue_kind": 1,
                        "task_scheduler_worker_count": 4,
                    },
                ],
            }
            sample_path = temp_path / "sample.json"
            sample_path.write_text(json.dumps(payload), encoding="utf-8")

            loaded = self.compare.load_results(str(sample_path))
            route_cache = loaded["samples"]["route_cache/name_resolution_5k"]
            sema = loaded["samples"]["sema/name_resolution_5k"]
            scheduler = loaded["samples"]["scheduler/task_queue_mode"]

            self.assertEqual(route_cache["route_cache_scan_count"], 5001)
            self.assertEqual(route_cache["route_cache_miss_count"], 5001)
            self.assertEqual(sema["ir_arena_allocations"], 77)
            self.assertEqual(sema["ir_raw_allocations"], 2)
            self.assertEqual(sema["ir_bytes_allocated"], 4096)
            self.assertEqual(sema["ir_node_count"], 79)
            self.assertEqual(sema["ir_max_node_count"], 79)
            self.assertEqual(sema["ir_destructor_calls"], 79)
            self.assertEqual(scheduler["task_scheduler_queue_kind"], 1)
            self.assertEqual(scheduler["task_scheduler_worker_count"], 4)

    def test_compare_script_reports_route_cache_and_ir_allocations(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            baseline = temp_path / "baseline.json"
            current = temp_path / "current.json"
            payload = {
                "schema": "styio.benchmark.v1",
                "git_sha": "deadbeef",
                "build_type": "Release",
                "timestamp": 1,
                "samples": [
                    {
                        "phase": "route_cache",
                        "label": "name_resolution_5k",
                        "duration_ns": 0,
                        "route_cache_scan_count": 5001,
                        "route_cache_miss_count": 5001,
                    },
                    {
                        "phase": "sema",
                        "label": "name_resolution_5k",
                        "duration_ns": 123,
                        "ir_arena_allocations": 77,
                        "ir_raw_allocations": 2,
                        "ir_bytes_allocated": 4096,
                        "ir_node_count": 79,
                        "ir_max_node_count": 79,
                        "ir_destructor_calls": 79,
                    },
                    {
                        "phase": "type",
                        "label": "typed_bindings_1k",
                        "duration_ns": 456,
                        "ir_arena_allocations": 128,
                        "ir_raw_allocations": 0,
                        "ir_bytes_allocated": 8192,
                        "ir_node_count": 128,
                        "ir_max_node_count": 128,
                        "ir_destructor_calls": 128,
                    },
                    {
                        "phase": "scheduler",
                        "label": "task_queue_mode",
                        "duration_ns": 0,
                        "task_scheduler_queue_kind": 1,
                        "task_scheduler_worker_count": 4,
                    },
                ],
            }
            baseline.write_text(json.dumps(payload), encoding="utf-8")
            current.write_text(json.dumps(payload), encoding="utf-8")
            markdown = temp_path / "report.md"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    str(baseline),
                    str(current),
                    "--route-cache",
                    "--ir-alloc",
                    "--scheduler",
                    "--markdown",
                    str(markdown),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            self.assertIn("Route cache counters:", completed.stdout)
            self.assertIn("IR allocation stats:", completed.stdout)
            self.assertIn("Scheduler metadata:", completed.stdout)
            self.assertTrue(markdown.is_file())
            rendered = markdown.read_text(encoding="utf-8")
            self.assertIn("| route_cache/name_resolution_5k | scan | 5001 | 5001 | +0 |", rendered)
            self.assertIn("| sema/name_resolution_5k | arena |", rendered)
            self.assertIn("| type/typed_bindings_1k | arena |", rendered)
            self.assertIn("| scheduler/task_queue_mode | queue_kind | 1 | 1 | +0 |", rendered)

    def test_compare_script_auto_detects_baseline_next_to_current(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            baseline = temp_path / "baseline.json"
            current = temp_path / "current.json"
            baseline_payload = {
                "schema": "styio.benchmark.v1",
                "git_sha": "baseline123",
                "build_type": "Release",
                "timestamp": 1,
                "samples": [
                    {
                        "phase": "parse",
                        "label": "many_stmts_1k",
                        "duration_ns": 100,
                    }
                ],
            }
            current_payload = {
                "schema": "styio.benchmark.v1",
                "git_sha": "current456",
                "build_type": "Release",
                "timestamp": 2,
                "samples": [
                    {
                        "phase": "parse",
                        "label": "many_stmts_1k",
                        "duration_ns": 125,
                    }
                ],
            }
            baseline.write_text(json.dumps(baseline_payload), encoding="utf-8")
            current.write_text(json.dumps(current_payload), encoding="utf-8")

            completed = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "auto",
                    current.name,
                    "--baseline-dir",
                    str(temp_path),
                    "--threshold",
                    "30",
                ],
                cwd=temp_path,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            self.assertIn("Baseline: baseline123 (Release)", completed.stdout)
            self.assertIn("Current:  current456 (Release)", completed.stdout)
            self.assertIn("parse/many_stmts_1k", completed.stdout)
            self.assertIn("100 ns", completed.stdout)
            self.assertIn("125 ns", completed.stdout)
            self.assertIn("stable", completed.stdout)
            self.assertNotIn("N/A", completed.stdout)
            self.assertIn("PASS: all benchmarks within 30.0% threshold.", completed.stdout)


if __name__ == "__main__":
    unittest.main()
