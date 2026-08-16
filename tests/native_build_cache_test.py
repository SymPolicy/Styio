#!/usr/bin/env python3
"""Focused native-build runtime-object cache contract tests.

These tests intentionally exercise only the compiler-owned cache boundary.  The
benchmark runner and workload sources remain untouched: a valid artifact must
still be produced when caching is disabled, unavailable, concurrently written,
or invalidated by a mutated manifest.
"""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "styio-benchmark/workloads/parity-v2/sources/llvm-scalar-chain.styio"
OTHER_SOURCE = ROOT / "styio-benchmark/workloads/parity-v2/sources/llvm-call-graph.styio"
DEFAULT_COMPILER = ROOT / "styio-nightly/build/perf-parity-baseline/bin/styio"


class NativeBuildCacheContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        raw = os.environ.get("STYIO_TEST_COMPILER")
        cls.compiler = Path(raw) if raw else DEFAULT_COMPILER
        if not cls.compiler.is_file() or not os.access(cls.compiler, os.X_OK):
            raise unittest.SkipTest("build a Styio compiler and set STYIO_TEST_COMPILER")
        if not SOURCE.is_file():
            raise unittest.SkipTest("parity-v2 scalar source is unavailable")
        if not OTHER_SOURCE.is_file():
            raise unittest.SkipTest("parity-v2 call-graph source is unavailable")

    def build_source(
        self,
        cache: Path,
        source: Path,
        output: Path,
        extra_environment: dict[str, str] | None = None,
    ) -> subprocess.CompletedProcess[bytes]:
        environment = os.environ.copy()
        environment["STYIO_NATIVE_CACHE"] = "1"
        environment["STYIO_NATIVE_RUNTIME_CACHE_DIR"] = str(cache)
        if extra_environment:
            environment.update(extra_environment)
        return subprocess.run(
            [str(self.compiler), "build", str(source), "-o", str(output)],
            cwd=ROOT,
            input=b"",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            env=environment,
            timeout=120,
        )

    def build(self, cache: Path, output: Path) -> subprocess.CompletedProcess[bytes]:
        return self.build_source(cache, SOURCE, output)

    @staticmethod
    def manifests(cache: Path) -> list[Path]:
        return sorted(cache.rglob("manifest.txt"))

    def assert_private_manifests(self, manifests: list[Path], temporary_root: Path) -> None:
        for manifest in manifests:
            text = manifest.read_text(encoding="utf-8")
            self.assertNotIn(str(temporary_root), text)
            self.assertNotIn("/tmp/", text)
            self.assertNotIn("compile.log", text)

    @staticmethod
    def run_artifact(path: Path) -> bytes:
        result = subprocess.run(
            [str(path)],
            input=b"5\n",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            timeout=30,
        )
        if result.returncode != 0:
            raise AssertionError(result.stderr.decode(errors="replace"))
        return result.stdout

    def test_cache_hit_manifest_invalidation_and_failure_fallback(self) -> None:
        with tempfile.TemporaryDirectory(prefix="styio-native-cache-test-") as raw:
            root = Path(raw)
            cache = root / "cache"
            first = self.build(cache, root / "first.bin")
            self.assertEqual(first.returncode, 0, first.stderr.decode(errors="replace"))
            entries = self.manifests(cache)
            self.assertEqual(len(entries), 4)
            self.assert_private_manifests(entries, root)
            second = self.build(cache, root / "second.bin")
            self.assertEqual(second.returncode, 0, second.stderr.decode(errors="replace"))
            self.assertEqual(self.run_artifact(root / "first.bin"), self.run_artifact(root / "second.bin"))

            entries[0].write_text(entries[0].read_text() + "mutation=invalid\n", encoding="utf-8")
            invalidated = self.build(cache, root / "invalidated.bin")
            self.assertEqual(invalidated.returncode, 0, invalidated.stderr.decode(errors="replace"))
            self.assertFalse(any("mutation=invalid" in item.read_text(encoding="utf-8") for item in self.manifests(cache)))

            bad_root = root / "cache-file"
            bad_root.write_text("not a directory", encoding="utf-8")
            fallback = self.build(bad_root, root / "fallback.bin")
            self.assertEqual(fallback.returncode, 0, fallback.stderr.decode(errors="replace"))
            self.assertEqual(self.run_artifact(root / "first.bin"), self.run_artifact(root / "fallback.bin"))
            self.assertEqual(list(cache.rglob("*.tmp.*")), [])

    def test_corrupt_object_is_recompiled_before_link(self) -> None:
        with tempfile.TemporaryDirectory(prefix="styio-native-cache-object-") as raw:
            root = Path(raw)
            cache = root / "cache"
            first = self.build(cache, root / "first.bin")
            self.assertEqual(first.returncode, 0, first.stderr.decode(errors="replace"))
            objects = sorted(
                path
                for path in cache.rglob("artifact-ir.*")
                if path.is_file()
            )
            self.assertEqual(len(objects), 1)
            objects[0].write_bytes(b"corrupted object bytes")
            repaired = self.build(cache, root / "repaired.bin")
            self.assertEqual(repaired.returncode, 0, repaired.stderr.decode(errors="replace"))
            self.assertEqual(self.run_artifact(root / "first.bin"), self.run_artifact(root / "repaired.bin"))
            self.assertEqual(list(cache.rglob("*.tmp.*")), [])

    def test_compile_failure_falls_back_without_publishing_partial_entry(self) -> None:
        real_compiler = shutil.which("clang++")
        if real_compiler is None:
            self.skipTest("clang++ is unavailable for the delegated compiler probe")
        with tempfile.TemporaryDirectory(prefix="styio-native-cache-compile-failure-") as raw:
            root = Path(raw)
            cache = root / "cache"
            wrapper = root / "compiler-wrapper.py"
            wrapper.write_text(
                "#!/usr/bin/env python3\n"
                "import os\n"
                "import sys\n"
                f"compiler = {real_compiler!r}\n"
                "args = sys.argv[1:]\n"
                "if '-c' in args and any(arg.endswith('RuntimeState.cpp') for arg in args):\n"
                "    sys.stderr.write('intentional cache compile failure\\n')\n"
                "    raise SystemExit(42)\n"
                "os.execv(compiler, [compiler, *args])\n",
                encoding="utf-8",
            )
            wrapper.chmod(0o755)
            result = self.build_source(
                cache,
                SOURCE,
                root / "fallback.bin",
                {"STYIO_NATIVE_CXX": str(wrapper)},
            )
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
            self.assertEqual(self.run_artifact(root / "fallback.bin"), b"126005264\n")
            self.assertEqual(list(cache.rglob("*.tmp.*")), [])

    def test_concurrent_writers_publish_complete_entries(self) -> None:
        with tempfile.TemporaryDirectory(prefix="styio-native-cache-race-") as raw:
            root = Path(raw)
            cache = root / "cache"
            environment = os.environ.copy()
            environment["STYIO_NATIVE_CACHE"] = "1"
            environment["STYIO_NATIVE_RUNTIME_CACHE_DIR"] = str(cache)
            processes = [
                subprocess.Popen(
                    [str(self.compiler), "build", str(SOURCE), "-o", str(root / f"{index}.bin")],
                    cwd=ROOT,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    env=environment,
                )
                for index in range(2)
            ]
            results = [process.communicate(timeout=120) for process in processes]
            for returncode, (_, stderr) in zip((process.returncode for process in processes), results):
                self.assertEqual(returncode, 0, stderr.decode(errors="replace"))
            for index in range(2):
                self.assertTrue((root / f"{index}.bin").is_file())
            self.assertEqual(list(cache.rglob("*.tmp.*")), [])
            self.assertEqual(len(self.manifests(cache)), 4)
            self.assert_private_manifests(self.manifests(cache), root)

    def test_distinct_sources_and_generated_ir_do_not_collide(self) -> None:
        with tempfile.TemporaryDirectory(prefix="styio-native-cache-source-") as raw:
            root = Path(raw)
            cache = root / "cache"
            scalar = self.build(cache, root / "scalar.bin")
            graph = self.build_source(cache, OTHER_SOURCE, root / "graph.bin")
            self.assertEqual(scalar.returncode, 0, scalar.stderr.decode(errors="replace"))
            self.assertEqual(graph.returncode, 0, graph.stderr.decode(errors="replace"))
            self.assertNotEqual(self.run_artifact(root / "scalar.bin"), self.run_artifact(root / "graph.bin"))
            # Runtime objects and the wrapper are shared; each generated IR
            # must have its own content-bound entry.
            self.assertEqual(len(self.manifests(cache)), 5)

    def test_source_content_mutation_rebuilds_generated_object(self) -> None:
        with tempfile.TemporaryDirectory(prefix="styio-native-cache-content-") as raw:
            root = Path(raw)
            cache = root / "cache"
            mutated_source = root / "mutated.styio"
            mutated_source.write_text(
                SOURCE.read_text(encoding="utf-8").replace("value = 3", "value = 4"),
                encoding="utf-8",
            )
            original = self.build(cache, root / "original.bin")
            mutated = self.build_source(cache, mutated_source, root / "mutated.bin")
            self.assertEqual(original.returncode, 0, original.stderr.decode(errors="replace"))
            self.assertEqual(mutated.returncode, 0, mutated.stderr.decode(errors="replace"))
            self.assertNotEqual(self.run_artifact(root / "original.bin"), self.run_artifact(root / "mutated.bin"))
            self.assertEqual(len(self.manifests(cache)), 5)


if __name__ == "__main__":
    unittest.main()
