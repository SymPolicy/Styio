#!/usr/bin/env python3
"""Export committed Styio documentation and static CMake declarations."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from urllib.parse import urlsplit

class ExtractionError(Exception):
    pass

def git(repo: Path, *args: str) -> str:
    result = subprocess.run(['git', '--no-replace-objects', '-C', str(repo), *args], capture_output=True, timeout=60)
    if result.returncode:
        raise ExtractionError('git_read_failed')
    return result.stdout.decode('utf-8')

def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec='seconds').replace('+00:00', 'Z')

def atomic_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=path.name + '.', dir=path.parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as handle:
            json.dump(data, handle, ensure_ascii=False, indent=2)
            handle.write('\n')
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

def cmake_commands(text: str):
    """Read declarations without evaluating CMake or executing project scripts."""
    pos = 0
    length = len(text)

    def bracket_end(start: int) -> int | None:
        match = re.match('\\[(=*)\\[', text[start:])
        if not match:
            return None
        end = text.find(']' + match[1] + ']', start + len(match[0]))
        if end < 0:
            raise ExtractionError('unsupported_cmake_syntax')
        return end + len(match[1]) + 2
    while pos < length:
        if text[pos].isspace():
            pos += 1
            continue
        if text[pos] == '#':
            end = bracket_end(pos + 1)
            if end is None:
                end = text.find('\n', pos)
                end = length if end < 0 else end
            pos = end
            continue
        match = re.match('([A-Za-z_][A-Za-z_0-9]*)\\s*\\(', text[pos:])
        if not match:
            raise ExtractionError('unsupported_cmake_syntax')
        start = pos
        pos += len(match[0])
        depth = 1
        quoted = False
        arguments = []
        while pos < length and depth:
            ch = text[pos]
            if ch == '\\' and pos + 1 < length:
                arguments.append(text[pos:pos + 2])
                pos += 2
                continue
            if ch == '"':
                quoted = not quoted
            if not quoted:
                end = bracket_end(pos)
                if end is not None:
                    arguments.append(text[pos:end])
                    pos = end
                    continue
                if ch == '#':
                    end = bracket_end(pos + 1)
                    if end is None:
                        end = text.find('\n', pos)
                        end = length if end < 0 else end
                    arguments.append(' ')
                    pos = end
                    continue
                if ch == '(':
                    depth += 1
                elif ch == ')':
                    depth -= 1
                    if not depth:
                        pos += 1
                        break
            arguments.append(ch)
            pos += 1
        if depth or quoted:
            raise ExtractionError('unsupported_cmake_syntax')
        yield (match[1].lower(), ''.join(arguments).strip(), text.count('\n', 0, start) + 1)

def cmake_tokens(arguments: str) -> list[str]:
    tokens, pos = ([], 0)
    while pos < len(arguments):
        if arguments[pos].isspace():
            pos += 1
            continue
        bracket = re.match('\\[(=*)\\[', arguments[pos:])
        if bracket:
            end = arguments.find(']' + bracket[1] + ']', pos + len(bracket[0]))
            if end < 0:
                raise ExtractionError('unsupported_cmake_syntax')
            tokens.append(arguments[pos + len(bracket[0]):end])
            pos = end + len(bracket[1]) + 2
            continue
        quoted = arguments[pos] == '"'
        if quoted:
            pos += 1
        token = []
        while pos < len(arguments):
            ch = arguments[pos]
            if ch == '\\' and pos + 1 < len(arguments):
                token.append(arguments[pos:pos + 2])
                pos += 2
                continue
            if quoted and ch == '"' or (not quoted and ch.isspace()):
                if quoted:
                    pos += 1
                break
            token.append(ch)
            pos += 1
        tokens.append(''.join(token))
    return tokens

def export(repo: Path, repo_url: str, ref: str, view: str, fetch: bool=False) -> dict:
    parsed = urlsplit(repo_url)
    if parsed.scheme != 'https' or not parsed.netloc or parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise ExtractionError('invalid_repository_url')
    if not ref or ref.startswith('-') or '\n' in ref:
        raise ExtractionError('invalid_ref')
    if view not in {'current', 'draft'}:
        raise ExtractionError('invalid_view')
    resolved_ref = ref
    if fetch:
        if not ref.startswith(('refs/heads/', 'refs/tags/')):
            raise ExtractionError('fetch_requires_full_ref')
        git(repo, 'check-ref-format', ref)
        git(repo, 'fetch', '--no-tags', repo_url, ref)
        resolved_ref = 'FETCH_HEAD'
    commit = git(repo, 'rev-parse', '--verify', '--end-of-options', resolved_ref + '^{commit}').strip()
    if not re.fullmatch('[0-9a-f]{40}|[0-9a-f]{64}', commit):
        raise ExtractionError('invalid_commit')
    paths = git(repo, 'ls-tree', '-r', '--name-only', '-z', commit).rstrip('\x00').split('\x00')
    known = set(paths)

    def document(path: str) -> dict:
        if path not in known:
            raise ExtractionError('missing_authority')
        value = git(repo, 'show', commit + ':' + path)
        return {'path': path, 'text': value, 'sha256': hashlib.sha256(value.encode()).hexdigest()}
    grammar = []
    for path, role in (('docs/design/Styio-EBNF.md', 'composed_grammar'), ('docs/design/syntax/ACTIVE-SYNTAX.md', 'composed_authoring_map')):
        grammar.append({**document(path), 'role': role})
    cmake_files = sorted((path for path in paths if path == 'CMakeLists.txt' or (path.startswith(('src/', 'cmake/')) and (path.endswith('.cmake') or path.endswith('/CMakeLists.txt')))))
    targets, links = ([], [])
    for path in cmake_files:
        for command, arguments, line in cmake_commands(document(path)['text']):
            tokens = cmake_tokens(arguments)
            if command in {'add_library', 'add_executable'} and tokens:
                targets.append({'name': tokens[0], 'kind': command, 'path': path, 'line': line, 'arguments': arguments, 'unresolved': any((ch in tokens[0] for ch in '$;'))})
            if command == 'target_link_libraries' and len(tokens) > 1:
                visibility = 'unspecified'
                configuration = 'all'
                for item in tokens[1:]:
                    if item in {'PUBLIC', 'PRIVATE', 'INTERFACE', 'LINK_PUBLIC', 'LINK_PRIVATE'}:
                        visibility = item
                    elif item in {'debug', 'optimized', 'general'}:
                        configuration = item
                    else:
                        links.append({'from': tokens[0], 'to': item, 'visibility': visibility, 'path': path, 'line': line, 'configuration': configuration, 'unresolved': any((ch in tokens[0] + item for ch in '$;\\'))})
                        configuration = 'all'
    return {'schema_version': 1, 'source': {'repo_url': repo_url.rstrip('/'), 'ref': ref, 'commit': commit, 'view': view, 'generated_at': utc_now(), 'resolution': 'fetched_remote_ref' if fetch else 'local_git_ref', 'working_tree_included': False}, 'grammar': grammar, 'grammar_authority': 'Composed EBNF and authoring map; feature-specific decisions and lifecycle remain in docs/design/syntax/features/. These pages do not prove implementation coverage.', 'documents': [{'path': path} for path in sorted(paths) if path.startswith('docs/')], 'cmake': {'targets': targets, 'links': links, 'files': cmake_files, 'limitations': ['Static declarations only: no CMake configuration, variable expansion, condition selection, or macro execution is performed.', 'Optional branches and imported/alias targets remain visible; link declarations are not a configured dependency graph or runtime dataflow.', 'Only root, src/, and cmake/ build files are inspected; test/dependency-generated targets are outside this projection.']}, 'code': {'directories': sorted({'/'.join(path.split('/')[:2]) for path in paths if path.startswith('src/') and len(path.split('/')) > 2}), 'boundary_contract': document('scripts/architecture-layer-gate.py'), 'limitations': 'Directory inventory and the committed boundary checker describe declared organization, not a complete call graph or proof of architectural compliance.'}}

def refresh(args) -> int:
    now = utc_now()
    if args.output.resolve() == args.status_output.resolve():
        print('facts and status outputs must differ', file=sys.stderr)
        return 2
    previous = None
    if args.output.is_file():
        try:
            value = json.loads(args.output.read_text(encoding='utf-8'))
            if isinstance(value, dict) and value.get('schema_version') == 1 and isinstance(value.get('source'), dict):
                previous = value
        except (OSError, ValueError):
            pass
    try:
        data = export(args.repo, args.repository_url, args.ref, args.classification, args.fetch)
        source = previous['source'] if previous else {}
        atomic_json(args.status_output, {'state': 'stale' if previous else 'unavailable', 'attempted_at': now, 'last_success': source.get('generated_at'), 'commit': source.get('commit'), 'error': 'refresh_in_progress'})
        atomic_json(args.output, data)
        previous = data
        status = {'state': 'fresh', 'attempted_at': now, 'last_success': data['source']['generated_at'], 'commit': data['source']['commit'], 'error': None}
        atomic_json(args.status_output, status)
        return 0
    except (ExtractionError, OSError, UnicodeError, subprocess.SubprocessError) as error:
        code = str(error) if isinstance(error, ExtractionError) else 'extraction_failed'
        source = previous['source'] if previous else {}
        atomic_json(args.status_output, {'state': 'stale' if previous else 'unavailable', 'attempted_at': now, 'last_success': source.get('generated_at'), 'commit': source.get('commit'), 'error': code})
        print('fact refresh failed: ' + code, file=sys.stderr)
        return 1

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--repository-url', required=True)
    parser.add_argument('--ref', required=True)
    parser.add_argument('--classification', required=True, choices=('current', 'draft'))
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--status-output', required=True, type=Path)
    parser.add_argument('--fetch', action='store_true', help='Fetch the explicit full ref before extracting; never push or publish')
    return refresh(parser.parse_args())
if __name__ == '__main__':
    raise SystemExit(main())
