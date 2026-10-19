"""Publish four reviewed artifacts, preserving edits and refreshing page caches only."""
import json
import re
import shutil
import zipfile
from pathlib import Path
from lxml import etree as E
from inspect_audit_units import QA, FILES, TASK
from inspect_sad_unit_trace import N, q, text
from preserve_audit_workbooks import unpack
from verify_audit_documents import sha


def page_caches(package, accepted, pages):
    root = E.fromstring(package['word/document.xml'])
    ref = E.fromstring(accepted['word/document.xml'])
    a = [p for p in root.find('w:body/w:sdt', N).findall('.//w:p', N) if text(p)]
    b = [p for p in ref.find('w:body/w:sdt', N).findall('.//w:p', N) if text(p)]
    assert len(a) == len(b)
    changes = []
    for old, new in zip(a, b):
        assert re.sub(r'[\d\s]', '', text(old)) == re.sub(r'[\d\s]', '', text(new))
        ot, nt = old.findall('.//w:t', N), new.findall('.//w:t', N)
        if ot and ot[-1].text != nt[-1].text:
            assert ot[-1].text.isdigit() and nt[-1].text.isdigit()
            changes.append(['toc', ot[-1].text, nt[-1].text])
            ot[-1].text = nt[-1].text
    package['word/document.xml'] = E.tostring(root, encoding='UTF-8', xml_declaration=True, standalone=True)
    for name, data in list(package.items()):
        if not re.match(r'word/(header|footer)\d+\.xml$', name):
            continue
        part = E.fromstring(data)
        dirty = False
        for p in part.findall('.//w:p', N):
            state = []
            for node in p.iter():
                if node.tag == q('fldChar'):
                    kind = node.get(q('fldCharType'))
                    if kind == 'begin': state.append({'instruction': '', 'result': False, 'done': False})
                    elif kind == 'separate' and state: state[-1]['result'] = True
                    elif kind == 'end' and state: state.pop()
                elif node.tag == q('instrText') and state:
                    state[-1]['instruction'] += node.text or ''
                elif node.tag == q('t') and state and state[-1]['result'] and re.match(r'^\s*NUMPAGES\b', state[-1]['instruction']):
                    assert not state[-1]['done'] and (node.text or '').isdigit()
                    state[-1]['done'] = True
                    if node.text != str(pages):
                        changes.append([name, node.text, str(pages)])
                        node.text = str(pages)
                        dirty = True
        if dirty:
            package[name] = E.tostring(part, encoding='UTF-8', xml_declaration=True, standalone=True)
    return changes


def main():
    base = json.loads((QA/'baseline.json').read_text(encoding='utf8'))
    check = json.loads((QA/'verification.json').read_text(encoding='utf8'))
    visual = json.loads((QA/'visual-review.json').read_text(encoding='utf8'))
    pages = json.loads((QA/'page-comparison.json').read_text(encoding='utf8'))
    assert visual['issues'] == [] and check['common_body_columns_identical']
    assert all(visual[k]['reviewed_pages'] == pages[k]['new_body_pages'] for k in ('sad','sddd'))
    sources = json.loads((QA/'extra-evidence.json').read_text(encoding='utf8'))['sources']
    prior = json.loads((TASK/'qa/sad-audit-trace-20261012/sources.json').read_text(encoding='utf8'))
    sources.update({p:h for p,h in prior.items() if '/src/' in p.replace('\\','/')})
    assert all(sha(Path(p)) == h for p,h in {**base['protected'], **sources}.items())
    staged = {}
    caches = {}
    for kind in ('sad','sddd','suvc','suvr'):
        assert sha(FILES[kind]) == base[kind]['sha256'], 'External edit: '+kind
        ext = FILES[kind].suffix
        if ext == '.docx':
            package = unpack(QA/(kind+ext))
            caches[kind] = page_caches(package, unpack(QA/(kind+'-accepted.docx')), pages[kind]['pages'])
            assert E.fromstring(package['word/settings.xml']).find(q('trackRevisions')) is not None
            output = QA/(kind+'-publish.docx')
            with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as z:
                for name, data in package.items(): z.writestr(name,data)
        else:
            output = QA/(kind+ext)
        temp = FILES[kind].with_name(FILES[kind].name+'.pending')
        assert not temp.exists()
        staged[kind] = (output,temp)
    # Preflight every source before replacing the first artifact.
    assert all(sha(FILES[k]) == base[k]['sha256'] for k in staged)
    for kind,(output,temp) in staged.items():
        shutil.copyfile(output,temp)
    for kind,(output,temp) in staged.items():
        assert sha(FILES[kind]) == base[kind]['sha256'], 'External edit before replacement: '+kind
        temp.replace(FILES[kind])
        assert sha(FILES[kind]) == sha(output)
    assert all(sha(Path(p)) == h for p,h in {**base['protected'], **sources}.items())
    result = {'outputs':{k:{'path':str(FILES[k]),'sha256':sha(FILES[k])} for k in staged},
              'page_cache_updates':caches,'protected_files_unchanged':len(base['protected']),
              'source_and_test_files_unchanged':len(sources),'new_unit_ids':0,
              'device_execution':False,'known_error_return_mismatches':2,
              'structural_verification':True,'visual_verification':True}
    (QA/'published-verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(result,ensure_ascii=False,indent=2))


if __name__ == '__main__': main()
