import contextlib
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import refresh_review


class RefreshTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "input"
        shutil.copytree(ROOT / "fixtures", self.source)
        self.config = self.source / "refresh.json"
        self.data = {"schema_version": 1, "review_input": "site.json", "output": "../output",
                     "sources": [{"repo": "../missing-repo", "repository_url": "https://example.invalid/compiler",
                                  "ref": "HEAD", "classification": "draft", "fetch": False,
                                  "facts_output": "facts.json", "status_output": "facts-status.json"}]}
        self.write()

    def write(self):
        self.config.write_text(json.dumps(self.data), encoding="utf-8")

    def test_failed_extraction_still_renders_stale_and_reports_failure(self):
        before = (self.source / "facts.json").read_bytes()
        with contextlib.redirect_stderr(io.StringIO()):
            result = refresh_review.run(self.config)
        self.assertEqual(result["failed_sources"], 1)
        self.assertEqual(result["status"], "stale")
        self.assertFalse(result["published"])
        self.assertEqual(before, (self.source / "facts.json").read_bytes())
        page = (self.root / "output/repositories/compiler/index.html").read_text(encoding="utf-8")
        self.assertIn("保留上次成功结果", page)

    def test_first_failure_renders_unavailable(self):
        (self.source / "facts.json").unlink()
        with contextlib.redirect_stderr(io.StringIO()):
            result = refresh_review.run(self.config)
        self.assertEqual(result["status"], "stale")
        page = (self.root / "output/repositories/compiler/index.html").read_text(encoding="utf-8")
        self.assertIn("尚无成功快照", page)

    def test_refuses_to_overwrite_input_model(self):
        before = (self.source / "site.json").read_bytes()
        self.data["sources"][0]["facts_output"] = "site.json"
        self.write()
        with self.assertRaisesRegex(ValueError, "overlapping"):
            refresh_review.run(self.config)
        self.assertEqual(before, (self.source / "site.json").read_bytes())

    def test_refuses_overlapping_source_outputs(self):
        self.data["sources"].append(dict(self.data["sources"][0]))
        self.write()
        with self.assertRaisesRegex(ValueError, "overlapping"):
            refresh_review.run(self.config)


if __name__ == "__main__":
    unittest.main()
