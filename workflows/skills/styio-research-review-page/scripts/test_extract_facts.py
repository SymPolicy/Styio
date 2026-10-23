#!/usr/bin/env python3
"""Exercise committed-fact extraction with isolated Git fixtures."""
from __future__ import annotations
import argparse
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location('facts', Path(__file__).with_name('extract_facts.py'))
FACTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FACTS)

class FactsTest(unittest.TestCase):

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        subprocess.run(['git', 'init', '-q', str(self.repo)], check=True)
        for path, text in {'docs/design/Styio-EBNF.md': '# Grammar\n```ebnf\nexpr = "value" ;\n```\n', 'docs/design/syntax/ACTIVE-SYNTAX.md': '# Current forms\n', 'scripts/architecture-layer-gate.py': '# Declared layer rules\n', 'src/cmake/targets/Core.cmake': 'add_library(core STATIC ${SOURCES})\ntarget_link_libraries(core PUBLIC symbol ${LLVM_LIBS})\n', 'src/Sema/Sema.cpp': '// source\n', 'CMakeLists.txt': 'project(Fixture)\ninclude(src/cmake/targets/Core.cmake)\n'}.items():
            p = self.repo / path
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding='utf-8')
        self.git('add', '.')
        self.git('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'fixture')
        self.args = argparse.Namespace(repo=self.repo, repository_url='https://example.invalid/styio', ref='HEAD', classification='draft', output=self.root / 'facts.json', status_output=self.root / 'status.json', fetch=False)

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args], text=True).strip()

    def export(self):
        return FACTS.export(self.repo, self.args.repository_url, 'HEAD', 'draft')

    def test_fixed_commit_and_unresolved_links(self):
        data = self.export()
        self.assertEqual(data['source']['commit'], self.git('rev-parse', 'HEAD'))
        self.assertEqual(data['grammar'][0]['role'], 'composed_grammar')
        self.assertEqual(data['cmake']['targets'][0]['name'], 'core')
        self.assertEqual(data['cmake']['links'][0]['to'], 'symbol')
        self.assertFalse(data['cmake']['links'][0]['unresolved'])
        self.assertTrue(data['cmake']['links'][1]['unresolved'])

    def test_dirty_and_untracked_files_do_not_masquerade_as_commit(self):
        path = self.repo / 'docs/design/Styio-EBNF.md'
        path.write_text('uncommitted grammar', encoding='utf-8')
        (self.repo / 'docs/untracked.md').write_text('untracked', encoding='utf-8')
        data = self.export()
        self.assertNotIn('uncommitted', data['grammar'][0]['text'])
        self.assertNotIn({'path': 'docs/untracked.md'}, data['documents'])

    def test_failed_refresh_keeps_last_good_bytes(self):
        self.assertEqual(FACTS.refresh(self.args), 0)
        before = self.args.output.read_bytes()
        self.args.ref = 'missing-ref'
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(FACTS.refresh(self.args), 1)
        self.assertEqual(before, self.args.output.read_bytes())
        status = json.loads(self.args.status_output.read_text(encoding='utf-8'))
        self.assertEqual(status['state'], 'stale')
        self.assertEqual(status['commit'], self.git('rev-parse', 'HEAD'))
        self.assertNotIn(str(self.root), json.dumps(status))

    def test_first_failure_is_unavailable_not_empty_success(self):
        self.args.ref = 'missing-ref'
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(FACTS.refresh(self.args), 1)
        self.assertFalse(self.args.output.exists())
        self.assertEqual(json.loads(self.args.status_output.read_text(encoding='utf-8'))['state'], 'unavailable')

    def test_nonobject_previous_json_does_not_preserve_false_freshness(self):
        self.args.output.write_text('[]', encoding='utf-8')
        self.args.status_output.write_text('{"state":"fresh"}', encoding='utf-8')
        self.args.ref = 'missing-ref'
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(FACTS.refresh(self.args), 1)
        self.assertEqual(json.loads(self.args.status_output.read_text(encoding='utf-8'))['state'], 'unavailable')
        self.assertEqual(self.args.output.read_text(encoding='utf-8'), '[]')

    def test_git_replace_cannot_change_fixed_commit_evidence(self):
        original = self.git('rev-parse', 'HEAD')
        (self.repo / 'docs/design/Styio-EBNF.md').write_text('replacement grammar', encoding='utf-8')
        self.git('add', '.')
        self.git('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'replacement')
        replacement = self.git('rev-parse', 'HEAD')
        self.git('replace', original, replacement)
        data = FACTS.export(self.repo, self.args.repository_url, original, 'draft')
        self.assertEqual(data['source']['commit'], original)
        self.assertNotIn('replacement grammar', data['grammar'][0]['text'])

    def test_authority_is_required(self):
        self.git('rm', 'docs/design/Styio-EBNF.md')
        self.git('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'remove')
        with self.assertRaisesRegex(FACTS.ExtractionError, 'missing_authority'):
            self.export()

    def test_comments_and_quoted_parentheses(self):
        text = '# add_library(fake STATIC x)\n#[=[ add_library(fake2 STATIC x) ]=]\nadd_library(real STATIC "a(b).cpp")\n'
        commands = list(FACTS.cmake_commands(text))
        self.assertEqual(len(commands), 1)
        self.assertEqual(commands[0][2], 3)
        self.assertEqual(FACTS.cmake_tokens(commands[0][1]), ['real', 'STATIC', 'a(b).cpp'])

    def test_malformed_cmake_is_not_silently_truncated(self):
        with self.assertRaisesRegex(FACTS.ExtractionError, 'unsupported_cmake_syntax'):
            list(FACTS.cmake_commands('add_library(core STATIC "unterminated)'))

    def test_invalid_urls_and_fetch_ref_rejected_before_network(self):
        for url in ('file:///repo', 'https://user:pass@example.invalid/repo', 'https://example.invalid/repo?token=x'):
            with self.assertRaisesRegex(FACTS.ExtractionError, 'invalid_repository_url'):
                FACTS.export(self.repo, url, 'HEAD', 'draft')
        with self.assertRaisesRegex(FACTS.ExtractionError, 'fetch_requires_full_ref'):
            FACTS.export(self.repo, self.args.repository_url, 'HEAD', 'draft', True)
        for ref in ('refs/heads/main:refs/heads/other', 'refs/heads/*'):
            with self.assertRaisesRegex(FACTS.ExtractionError, 'git_read_failed'):
                FACTS.export(self.repo, self.args.repository_url, ref, 'draft', True)

    def test_cmake_bracket_escaped_space_and_link_configuration(self):
        self.assertEqual(FACTS.cmake_tokens('core PUBLIC [[foo bar]] foo\\ bar'), ['core', 'PUBLIC', 'foo bar', 'foo\\ bar'])
        p = self.repo / 'src/cmake/targets/Core.cmake'
        p.write_text('add_library(core STATIC source.cpp)\ntarget_link_libraries(core PRIVATE debug debuglib optimized optlib)\n', encoding='utf-8')
        self.git('add', '.')
        self.git('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'configs')
        links = self.export()['cmake']['links']
        self.assertEqual([(x['to'], x['configuration']) for x in links], [('debuglib', 'debug'), ('optlib', 'optimized')])

    def test_status_write_failure_describes_actual_snapshot(self):
        self.assertEqual(FACTS.refresh(self.args), 0)
        (self.repo / 'docs/design/Styio-EBNF.md').write_text('second snapshot', encoding='utf-8')
        self.git('add', '.')
        self.git('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'second')
        writer, calls = (FACTS.atomic_json, [])

        def fail_once(path, value):
            calls.append(path)
            if len(calls) == 3:
                raise OSError('simulated status write failure')
            writer(path, value)
        with patch.object(FACTS, 'atomic_json', fail_once), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(FACTS.refresh(self.args), 1)
        status = json.loads(self.args.status_output.read_text(encoding='utf-8'))
        data = json.loads(self.args.output.read_text(encoding='utf-8'))
        self.assertEqual(status['state'], 'stale')
        self.assertEqual(status['commit'], data['source']['commit'])
        self.assertEqual(status['commit'], self.git('rev-parse', 'HEAD'))

    def test_outputs_must_differ(self):
        self.args.status_output = self.args.output
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(FACTS.refresh(self.args), 2)
        self.assertFalse(self.args.output.exists())
if __name__ == '__main__':
    unittest.main()
