import importlib.util, json, tempfile, unittest, shutil
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_review', ROOT / 'scripts/build_review.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

class BuildTests(unittest.TestCase):

    def setUp(self):
        self.t = tempfile.TemporaryDirectory()
        # macOS may return a temporary root through a system directory alias.
        # Select its actual directory before testing intentional output links.
        self.base = Path(self.t.name).resolve()
        self.source = self.base / 'source'
        shutil.copytree(ROOT / 'fixtures', self.source)
        self.input = self.source / 'site.json'
        self.out = self.base / 'output'
        self.m = json.loads(self.input.read_text(encoding='utf-8'))

    def tearDown(self):
        self.t.cleanup()

    def write(self):
        self.input.write_text(json.dumps(self.m), encoding='utf-8')

    def test_real_fixture_build_and_navigation(self):
        result = builder.build(self.input, self.out)
        self.assertEqual(result['pages'], 5)
        self.assertFalse(result['published'])
        self.assertIn('position:sticky', (self.out / 'assets/style.css').read_text(encoding='utf-8'))
        self.assertEqual(builder.verify_output(self.out), 5)

    def test_math_preserved(self):
        builder.build(self.input, self.out)
        s = (self.out / 'research/index.html').read_text(encoding='utf-8')
        self.assertIn('<math ', s)
        self.assertIn('<mi>Γ</mi>', s)
        self.assertIn('aria-current="page"', s)

    def test_rebuild_owned_output(self):
        builder.build(self.input, self.out)
        builder.build(self.input, self.out)

    def test_refuse_unmanaged_output(self):
        self.out.mkdir()
        (self.out / 'user.txt').write_text('keep', encoding='utf-8')
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)
        self.assertEqual((self.out / 'user.txt').read_text(encoding='utf-8'), 'keep')

    def test_refuse_extra_output_file(self):
        builder.build(self.input, self.out)
        (self.out / 'user.txt').write_text('keep', encoding='utf-8')
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_refuse_modified_generated_file(self):
        builder.build(self.input, self.out)
        (self.out / 'index.html').write_text('user edit', encoding='utf-8')
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)
        self.assertEqual((self.out / 'index.html').read_text(encoding='utf-8'), 'user edit')

    def test_refuse_symlink_output(self):
        target = self.base / 'target'
        target.mkdir()
        self.out.symlink_to(target, target_is_directory=True)
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_reject_merged_draft(self):
        self.m['repositories'][0]['state']['publication'] = 'merged'
        self.write()
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_reject_unevidenced_ci_pass(self):
        self.m['repositories'][0]['state']['ci'] = 'passed'
        self.write()
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_reject_pr_head_mismatch(self):
        self.m['repositories'][0]['state']['pr']['head_commit'] = 'b' * 40
        self.write()
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_reject_output_over_source(self):
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.source)

    def test_reject_route_traversal(self):
        self.m['pages'][0]['route'] = '../outside.html'
        self.write()
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_reject_bad_table(self):
        self.m['pages'][0]['blocks'][-1]['rows'] = [['one']]
        self.write()
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_reject_import_script(self):
        (self.source / 'paper.html').write_text('<html><head></head><body><script>alert(1)</script></body></html>', encoding='utf-8')
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_reject_source_escape(self):
        self.m['pages'][-1]['html_file'] = '../secrets.html'
        self.write()
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_reject_broken_link(self):
        self.m['pages'][0]['blocks'].append({'type': 'link', 'href': 'missing.html', 'label': 'bad'})
        self.write()
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, self.out)

    def test_stale_is_visible_and_data_retained(self):
        status = self.source / 'facts-status.json'
        data = json.loads(status.read_text(encoding='utf-8'))
        data['state'] = 'stale'
        data['error'] = 'git_ref_unavailable'
        status.write_text(json.dumps(data), encoding='utf-8')
        builder.build(self.input, self.out)
        s = (self.out / 'repositories/compiler/index.html').read_text(encoding='utf-8')
        self.assertIn('保留上次成功结果', s)
        self.assertIn('expression = literal', s)

    def test_unavailable_without_facts_is_visible(self):
        (self.source / 'facts.json').unlink()
        (self.source / 'facts-status.json').write_text(json.dumps({'state': 'unavailable', 'error': 'git_ref_unavailable', 'attempted_at': '2026-10-01T00:00:00Z'}), encoding='utf-8')
        builder.build(self.input, self.out)
        self.assertIn('尚无成功快照', (self.out / 'repositories/compiler/index.html').read_text(encoding='utf-8'))

    def test_reject_parent_symlink(self):
        actual = self.base / 'actual'
        actual.mkdir()
        alias = self.base / 'alias'
        alias.symlink_to(actual, target_is_directory=True)
        with self.assertRaises(builder.Invalid):
            builder.build(self.input, alias / 'out')

    def test_explicit_canonical_parent_is_accepted(self):
        actual = self.base / 'actual'
        actual.mkdir()
        alias = self.base / 'alias'
        alias.symlink_to(actual, target_is_directory=True)
        output = (alias / 'out').resolve()
        result = builder.build(self.input, output)
        self.assertEqual(result['pages'], 5)
        self.assertEqual(builder.verify_output(output), 5)

    def test_reject_active_import_forms(self):
        for payload in ['<object data="x"></object>', '<embed src="x">', '<base href="https://example.invalid">', '<meta http-equiv="refresh" content="0;url=https://example.invalid">', '<form action="https://example.invalid"></form>', '<style>@import "x";</style>', '<style>body{background:url(https://example.invalid/x)}</style>']:
            with self.subTest(payload=payload):
                (self.source / 'paper.html').write_text('<html><head></head><body>' + payload + '</body></html>', encoding='utf-8')
                with self.assertRaises(builder.Invalid):
                    builder.build(self.input, self.out)

    def test_fresh_commit_mismatch_is_unavailable(self):
        status = self.source / 'facts-status.json'
        data = json.loads(status.read_text(encoding='utf-8'))
        data['commit'] = 'b' * 40
        status.write_text(json.dumps(data), encoding='utf-8')
        builder.build(self.input, self.out)
        s = (self.out / 'repositories/compiler/index.html').read_text(encoding='utf-8')
        self.assertIn('integrity error', s)
        self.assertNotIn('expression = literal', s)
if __name__ == '__main__':
    unittest.main()
