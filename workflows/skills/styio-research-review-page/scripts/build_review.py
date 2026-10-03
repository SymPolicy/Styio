#!/usr/bin/env python3
"""Portable, dependency-free static review pages. Never pushes or publishes."""
import argparse, datetime, hashlib, html, json, os, re, shutil, tempfile
from pathlib import Path, PurePosixPath
from html.parser import HTMLParser
from urllib.parse import urlsplit, unquote
import posixpath
GROUPS = {'engineering': '工程维护', 'user-docs': '使用文档', 'user-skill': '用户 Skill', 'research': '形式化研究'}
SECTIONS = [('plan', '计划'), ('implemented', '已实现'), ('value', '改法、收益与代价'), ('verification', '验证与未验证范围'), ('pending', '待办')]

class Invalid(ValueError):
    pass

def check(test, message):
    if not test:
        raise Invalid(message)

def esc(value):
    return html.escape(str(value), quote=True)

def route(value):
    p = PurePosixPath(value)
    check(isinstance(value, str) and (not p.is_absolute()) and ('..' not in p.parts) and (p.suffix == '.html') and ('\\' not in value), 'route must be a relative HTML path')
    return value

def source_file(base, value):
    check(isinstance(value, str) and (not Path(value).is_absolute()), 'source path must be relative')
    raw = base / value
    check(not any((p.is_symlink() for p in [raw, *raw.parents] if p != base.parent)), 'symlink source is not allowed')
    p = raw.resolve()
    check(p.is_relative_to(base.resolve()) and p.is_file(), 'source must exist within the input directory')
    return p

def validate_facts_pair(base, spec):
    check(set(spec) >= {'data', 'status'}, 'facts needs data and status')
    status = json.loads(source_file(base, spec['status']).read_text(encoding='utf-8'))
    check(isinstance(status, dict) and status.get('state') in ('fresh', 'stale', 'unavailable'), 'invalid facts status')
    raw = Path(spec['data'])
    check(not raw.is_absolute() and '..' not in raw.parts, 'unsafe facts path')
    if status['state'] != 'unavailable' or (base / raw).exists():
        source_file(base, spec['data'])
    return status

def href(current, target):
    if urlsplit(target).scheme:
        check(urlsplit(target).scheme in ('https', 'http'), 'unsupported link scheme')
        return target
    if target.startswith('#'):
        return target
    p = urlsplit(target)
    route(p.path)
    return posixpath.relpath(p.path, posixpath.dirname(current) or '.') + ('#' + p.fragment if p.fragment else '')

def link(current, target, label):
    return '<a href="' + esc(href(current, target)) + '">' + esc(label) + '</a>'

def validate_state(repo):
    s = repo['state']
    allowed = {'local_only', 'pushed', 'draft_pr', 'merged', 'paused'}
    check(s['publication'] in allowed, 'invalid publication state')
    check(s['ci'] in {'not_run', 'pending', 'passed', 'failed', 'not_applicable'}, 'invalid CI state')
    check(s['local'] in {'none', 'in_progress', 'complete'}, 'invalid local state')
    pr = s.get('pr')
    commit = repo.get('commit', '')
    if s['publication'] in {'pushed', 'draft_pr', 'merged'}:
        check(re.fullmatch('[0-9a-f]{40}', commit or '') is not None, 'published state requires exact 40-character commit')
        check(s.get('remote_commit') == commit, 'remote commit must match selected commit')
    if s['publication'] in {'draft_pr', 'merged'}:
        check(pr and pr.get('url', '').startswith('https://'), 'PR state requires verified HTTPS PR URL')
        check(pr.get('head_commit') == commit, 'PR head must match selected commit')
        check(pr.get('state') == ('draft' if s['publication'] == 'draft_pr' else 'merged'), 'PR/publication state mismatch')
    if s['publication'] == 'merged':
        check(s['local'] == 'complete', 'merged requires completed implementation')
    if s['ci'] == 'passed':
        e = s.get('ci_evidence', {})
        check(e.get('head_commit') == commit and e.get('url', '').startswith('https://') and e.get('scope') and e.get('checked_at'), 'CI passed requires matching-head evidence, scope and timestamp')
    check(repo.get('view') in ('current', 'draft'), 'view must distinguish current and draft')
    check(repo.get('group') in GROUPS, 'unknown navigation group')
    for key, _ in SECTIONS:
        check(key in repo.get('sections', {}), 'missing section ' + key)

def validate_model(m, base):
    check(isinstance(m, dict), 'input must be an object')
    check(m.get('schema_version') == 1, 'unsupported schema_version')
    check(m.get('title') and m.get('updated_at'), 'title and updated_at are required')
    check(datetime.datetime.fromisoformat(m['updated_at'].replace('Z', '+00:00')).tzinfo is not None, 'updated_at must have timezone')
    routes = {'index.html'}
    ids = set()
    for r in m.get('repositories', []):
        check(r['id'] not in ids, 'duplicate repository id')
        ids.add(r['id'])
        r['route'] = route(r.get('route', 'repositories/' + r['id'] + '/index.html'))
        check(r['route'] not in routes, 'duplicate route')
        routes.add(r['route'])
        validate_state(r)
        if r.get('facts'):
            check(set(r['facts']) >= {'data', 'status'}, 'facts requires both data and status')
            validate_facts_pair(base, r['facts'])
    for p in m.get('pages', []):
        route(p['route'])
        check(p['route'] not in routes, 'duplicate route')
        routes.add(p['route'])
        check(p['group'] in GROUPS, 'unknown page group')
        check(sum((bool(p.get(k)) for k in ('blocks', 'html_file', 'facts'))) == 1, 'page needs exactly one of blocks, html_file, facts')
        if p.get('facts'):
            validate_facts_pair(base, p['facts'])
        if p.get('html_file'):
            source_file(base, p['html_file'])
    return routes

def blocks(items, current):
    out = []
    for b in items:
        t = b['type']
        if t == 'paragraph':
            out.append('<p>' + esc(b['text']) + '</p>')
        elif t == 'list':
            out.append('<ul>' + ''.join(('<li>' + esc(x) + '</li>' for x in b['items'])) + '</ul>')
        elif t == 'code':
            out.append('<pre><code>' + esc(b['text']) + '</code></pre>')
        elif t == 'link':
            out.append('<p>' + link(current, b['href'], b['label']) + '</p>')
        elif t == 'table':
            h = b['headers']
            check(h and all((len(row) == len(h) for row in b['rows'])), 'table columns differ')
            out.append('<div class="table-scroll" tabindex="0"><table><thead><tr>' + ''.join(('<th>' + esc(x) + '</th>' for x in h)) + '</tr></thead><tbody>' + ''.join(('<tr>' + ''.join(('<td>' + esc(x) + '</td>' for x in row)) + '</tr>' for row in b['rows'])) + '</tbody></table></div>')
        else:
            raise Invalid('unsupported block ' + t)
    return ''.join(out)

def nav(m, current, group):
    targets = {}
    for x in m.get('repositories', []) + m.get('pages', []):
        targets.setdefault(x['group'], x['route'])
    targets.update(m.get('navigation', {}))
    groups = ''.join(('<a' + (' aria-current="page"' if key == group else '') + ' href="' + esc(href(current, targets[key])) + '">' + label + '</a>' for key, label in GROUPS.items() if key in targets))
    return '<header class="review-nav"><a class="review-brand" href="' + esc(href(current, 'index.html')) + '">' + esc(m['title']) + '</a><nav aria-label="主要导航">' + groups + '</nav></header>'

def shell(m, current, title, group, body):
    return '<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>' + esc(title) + '</title><link rel="stylesheet" href="' + esc(href(current, 'assets/style.html').replace('style.html', 'style.css')) + '"></head><body>' + nav(m, current, group) + '<main><p class="meta">内容记录时间 ' + esc(m['updated_at']) + '</p><h1>' + esc(title) + '</h1>' + body + '</main></body></html>'

def state_html(r):
    s = r['state']
    text = {'local_only': '仅本地', 'pushed': '已推送', 'draft_pr': '已推送 Draft PR', 'merged': '已合并', 'paused': '暂停'}
    return '<p class="state">' + esc(('当前事实' if r['view'] == 'current' else 'Draft 候选') + ' · ' + text[s['publication']]) + '</p><dl><dt>本地实现</dt><dd>' + esc(s['local']) + '</dd><dt>CI</dt><dd>' + esc(s['ci']) + '</dd><dt>ref / commit</dt><dd><code>' + esc(r.get('ref', '')) + ' / ' + esc(r.get('commit', '未指定')) + '</code></dd></dl>'

def facts_html(repo, base):
    spec = repo['facts']
    status = validate_facts_pair(base, spec)
    if status['state'] == 'unavailable' and (not (base / spec['data']).exists()):
        return '<section id="facts"><h2>程序提取的仓库事实</h2><p class="warning">尚无成功快照；本次提取失败。' + esc(status.get('error') or '') + '</p><p>' + esc(status.get('attempted_at', '')) + '</p></section>'
    facts = json.loads(source_file(base, spec['data']).read_text(encoding='utf-8'))
    check(isinstance(facts, dict), 'facts must be an object')
    state = status.get('state', status.get('status'))
    check(state in ('fresh', 'stale', 'unavailable'), 'facts status must be explicit')
    stale = state != 'fresh'
    if status.get('commit') != facts.get('source', {}).get('commit'):
        return '<section id="facts"><h2>程序提取的仓库事实</h2><p class="warning">unavailable / integrity error：快照与状态的 commit 不一致，数据未展示。请重新执行提取。</p></section>'
    out = '<section id="facts"><h2>程序提取的仓库事实</h2><p class="' + ('warning' if stale else 'meta') + '">' + ('提取失败或已过期；以下保留上次成功结果。' if stale else '提取状态：' + esc(state)) + esc(status.get('error') or '') + '</p>'
    out += '<pre>' + esc(json.dumps(facts.get('source', {}), ensure_ascii=False, indent=2)) + '</pre>'
    grammar = facts.get('grammar', [])
    if isinstance(grammar, dict):
        grammar = grammar.get('files', [])
    out += '<h3>EBNF / 语法来源</h3>'
    for x in grammar:
        out += '<h4>' + esc(x.get('path', '')) + '</h4><pre><code>' + esc(x.get('text', '')) + '</code></pre>'
    docs = facts.get('documents', [])
    if isinstance(docs, dict):
        docs = docs.get('files', [])
    out += '<h3>文档树</h3><pre>' + esc('\n'.join((x if isinstance(x, str) else x.get('path', '') for x in docs))) + '</pre>'
    cmake = facts.get('cmake', {})
    out += '<h3>代码目录</h3><pre>' + esc('\n'.join(facts.get('code', {}).get('directories', []))) + '</pre>'
    out += '<h3>CMake 静态声明</h3><p>这些是选定 Git 对象中的静态声明，不是精确构建图或程序运行图；变量和生成器表达式保持未求值。</p>'
    out += blocks([{'type': 'table', 'headers': ['Target', 'Kind', 'Source'], 'rows': [[x.get('name', ''), x.get('kind', ''), str(x.get('path', '')) + ':' + str(x.get('line', ''))] for x in cmake.get('targets', [])]}], repo.get('route', 'index.html'))
    out += blocks([{'type': 'table', 'headers': ['From', 'To', 'Visibility', 'Configuration', 'Resolution'], 'rows': [[x.get('from', ''), x.get('to', ''), x.get('visibility', ''), x.get('configuration', ''), 'unresolved' if x.get('unresolved') else 'literal declaration'] for x in cmake.get('links', [])]}], repo.get('route', 'index.html'))
    out += '<ul>' + ''.join(('<li>' + esc(x) + '</li>' for x in cmake.get('limitations', []))) + '</ul></section>'
    return out

class Inspect(HTMLParser):

    def __init__(self):
        super().__init__()
        self.ids = []
        self.links = []
        self.runtime = []
        self.assets = []
        self.in_style = False

    def css(self, text):
        if re.search('@import|expression\\s*\\(', text, re.I):
            self.runtime.append('active/import CSS')
        for value in re.findall('url\\(\\s*["\']?([^"\')]+)', text, re.I):
            if value.startswith(('http:', 'https:', '//', 'javascript:')):
                self.runtime.append('external/active CSS')
            elif not value.startswith(('data:', '#')):
                self.assets.append(value)

    def handle_data(self, data):
        if self.in_style:
            self.css(data)

    def handle_endtag(self, t):
        if t == 'style':
            self.in_style = False

    def handle_starttag(self, t, a):
        a = dict(a)
        if a.get('id'):
            self.ids.append(a['id'])
        if t == 'a':
            self.links.append(a.get('href', ''))
        if t == 'style':
            self.in_style = True
        if a.get('style'):
            self.css(a['style'])
        if t in ('script', 'iframe', 'object', 'embed', 'base', 'form', 'input', 'textarea', 'select') or any((k.lower().startswith('on') for k in a)):
            self.runtime.append(t)
        if t == 'meta' and a.get('http-equiv', '').lower() == 'refresh':
            self.runtime.append('meta refresh')
        for key, value in a.items():
            if key.lower() in ('href', 'src', 'data', 'action', 'formaction', 'xlink:href') and re.match('\\s*(javascript|vbscript):', value, re.I):
                self.runtime.append('active URL')
        if t in ('img', 'link', 'script', 'source'):
            v = a.get('src', a.get('href', ''))
            if v.startswith(('http://', 'https://', '//')):
                self.runtime.append('external asset')
            elif v and (not v.startswith(('data:', '#'))):
                self.assets.append(v)

def verify_output(root):
    docs = {}
    for f in root.rglob('*.html'):
        p = Inspect()
        p.feed(f.read_text(encoding='utf-8'))
        check(len(p.ids) == len(set(p.ids)), 'duplicate HTML anchor')
        check(not p.runtime, 'active/external runtime content')
        docs[f] = p
    for f, p in docs.items():
        for value in p.links + p.assets:
            u = urlsplit(value)
            if u.scheme:
                check(u.scheme in ('https', 'http', 'mailto'), 'unsafe output URL')
                continue
            check(not u.path.startswith('/'), 'local-readable output needs relative links')
            dest = (f.parent / unquote(u.path)).resolve() if u.path else f
            check(dest.is_relative_to(root.resolve()) and dest.exists(), 'broken/outside output link: ' + value)
            if dest.is_dir():
                dest /= 'index.html'
            if u.fragment:
                check(dest in docs and unquote(u.fragment) in docs[dest].ids, 'broken anchor ' + value)
    return len(docs)

def build(input_path, output):
    base = input_path.parent.resolve()
    m = json.loads(input_path.read_text(encoding='utf-8'))
    validate_model(m, base)
    check(not any((p.is_symlink() for p in [output, *output.parents])), 'symlink output or parent is not allowed')
    output = output.resolve()
    check(not base.is_relative_to(output), 'output may not contain source input')
    check(output != Path(output.anchor) and (not output.is_symlink()), 'unsafe output directory')
    marker = output / '.review-output.json'
    if output.exists() and any(output.iterdir()):
        check(marker.is_file(), 'refuse nonempty unmanaged output')
        hashes = json.loads(marker.read_text(encoding='utf-8'))['files']
        owned = set(hashes) | {'.review-output.json'}
        actual = {str(f.relative_to(output)) for f in output.rglob('*') if f.is_file()}
        check(actual == owned and (not any((p.is_symlink() for p in output.rglob('*')))), 'modified output ownership inventory')
        check(all((hashlib.sha256((output / f).read_bytes()).hexdigest() == digest for f, digest in hashes.items())), 'generated output was edited; preserve it before rebuilding')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.review-stage-', dir=output.parent) as tmp:
        stage = Path(tmp)
        (stage / 'assets').mkdir()
        shutil.copyfile(Path(__file__).resolve().parents[1] / 'assets/style.css', stage / 'assets/style.css')
        index = ''
        for r in m.get('repositories', []):
            current = r['route']
            body = state_html(r)
            for key, label in SECTIONS:
                body += '<section id="' + key + '"><h2>' + label + '</h2>' + blocks(r['sections'][key], current) + '</section>'
            pr = r['state'].get('pr')
            body += '<h2 id="draft-pr">Draft PR</h2>' + (link(current, pr['url'], pr['url']) + '<p>' + esc(pr['state']) + '</p>' if pr else '<p>本次记录没有 Draft PR；不等同于已合并。</p>')
            if r.get('facts'):
                body += facts_html(r, base)
            dest = stage / current
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(shell(m, current, r['name'], r['group'], body), encoding='utf-8')
            index += '<section><h2>' + link('index.html', current, r['name']) + '</h2>' + state_html(r) + '</section>'
        for p in m.get('pages', []):
            current = p['route']
            dest = stage / current
            dest.parent.mkdir(parents=True, exist_ok=True)
            if p.get('html_file'):
                raw = source_file(base, p['html_file']).read_text(encoding='utf-8')
                i = Inspect()
                i.feed(raw)
                check(not i.runtime, 'imported HTML must be self-contained without scripts/handlers')
                check(re.search('<head\\b', raw, re.I) and re.search('<body\\b[^>]*>', raw, re.I), 'import must be a full HTML document')
                csshref = href(current, 'assets/style.html').replace('style.html', 'style.css')
                raw = re.sub('</head>', '<link rel="stylesheet" href="' + esc(csshref) + '"></head>', raw, count=1, flags=re.I)
                raw = re.sub('(<body\\b[^>]*>)', lambda x: x[1] + nav(m, current, p['group']), raw, count=1, flags=re.I)
                dest.write_text(raw, encoding='utf-8')
            else:
                dest.write_text(shell(m, current, p['title'], p['group'], facts_html(p, base) if p.get('facts') else blocks(p['blocks'], current)), encoding='utf-8')
            index += '<section><h2>' + link('index.html', current, p['title']) + '</h2></section>'
        (stage / 'index.html').write_text(shell(m, 'index.html', m['title'], '', index), encoding='utf-8')
        count = verify_output(stage)
        files = {str(p.relative_to(stage)): hashlib.sha256(p.read_bytes()).hexdigest() for p in stage.rglob('*') if p.is_file()}
        (stage / '.review-output.json').write_text(json.dumps({'schema_version': 1, 'files': files}, indent=2), encoding='utf-8')
        backup = output.with_name(output.name + '.review-backup')
        check(not backup.exists(), 'backup exists; inspect it before rebuilding')
        if output.exists():
            output.rename(backup)
        try:
            stage.rename(output)
        except Exception:
            if output.exists():
                shutil.rmtree(output)
            if backup.exists():
                backup.rename(output)
            raise
        if backup.exists():
            shutil.rmtree(backup)
    return {'status': 'passed', 'pages': count, 'output': str(output), 'published': False, 'visual_qa': 'not performed'}

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--input', type=Path, required=True)
    p.add_argument('--output', '--output-dir', dest='output', type=Path, required=True)
    a = p.parse_args()
    try:
        print(json.dumps(build(a.input.resolve(), a.output), ensure_ascii=False))
        return 0
    except (Invalid, ValueError, KeyError, OSError, TypeError) as e:
        print(json.dumps({'status': 'failed', 'error': str(e), 'published': False}, ensure_ascii=False))
        return 2
if __name__ == '__main__':
    raise SystemExit(main())
