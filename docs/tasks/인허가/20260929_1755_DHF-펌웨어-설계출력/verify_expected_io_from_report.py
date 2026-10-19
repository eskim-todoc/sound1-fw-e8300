"""Independently compare exported Markdown against raw XLSX XML cell text."""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
from collections import Counter
from pathlib import Path
from urllib.parse import unquote
from zipfile import ZipFile
import xml.etree.ElementTree as ET

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
QA = TASK / 'qa/expected-io-from-report'
DEST = ROOT / 'docs/참고/인허가/사운드1/DHF/04_설계 검증/SW/펌웨어/유닛 시험 예상 입출력'
NS = {'s': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
REL_NS = '{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id'
SOURCE = re.compile(r'<!-- source-cell:([A-Z]+\d+) -->\n(.*?)\n<!-- /source-cell:\1 -->', re.S)
LOG = re.compile(r'^\[UT-(?:CFX|HEAR|CM3|BOOT)-\d+\] C\d+ (?:A\[.*|I\d+ .*|(?:PASS|FAIL|OBSERVED) .*)$', re.M)


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def xlsx_source(path):
    with ZipFile(path) as z:
        wb = ET.fromstring(z.read('xl/workbook.xml'))
        sheet = next(s for s in wb.findall('s:sheets/s:sheet', NS) if s.get('name') == '보고서_유닛 시험')
        rels = ET.fromstring(z.read('xl/_rels/workbook.xml.rels'))
        target = next(r.get('Target') for r in rels if r.get('Id') == sheet.get(REL_NS))
        target = target.lstrip('/') if target.startswith('/') else 'xl/' + target
        shared = []
        if 'xl/sharedStrings.xml' in z.namelist():
            for item in ET.fromstring(z.read('xl/sharedStrings.xml')).findall('s:si', NS):
                shared.append(''.join(t.text or '' for t in item.iterfind('.//s:t', NS)))
        root = ET.fromstring(z.read(target))
        values = {}
        for cell in root.findall('.//s:sheetData/s:row/s:c', NS):
            typ = cell.get('t')
            if typ == 'inlineStr':
                val = ''.join(t.text or '' for t in cell.findall('.//s:t', NS))
            else:
                v = cell.find('s:v', NS)
                val = '' if v is None else v.text or ''
                if typ == 's':
                    val = shared[int(val)]
            val = val.replace('\r\n', '\n').replace('\r', '\n').strip('\n')
            if val:
                values[cell.get('r')] = val
        merges = [m.get('ref') for m in root.findall('s:mergeCells/s:mergeCell', NS)]
        return values, merges


def decode(block):
    lines = block.split('\n')
    result = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if line == '```text':
            if result and result[-1] == '':
                result.pop()  # blank before an inserted fenced block
            i += 1
            while i < len(lines) and lines[i] != '```':
                result.append(lines[i])
                i += 1
            i += 1
            if i < len(lines) and lines[i] == '':
                i += 1  # blank after an inserted fenced block
            continue
        if line.endswith('<br>'):
            line = line[:-4]
        line = html.unescape(re.sub(r'\\([\\`*_{}\[\]#|])', r'\1', line))
        result.append(line)
        i += 1
    return '\n'.join(result)


def verify(published=False):
    baseline = json.loads((QA / 'baseline.json').read_text(encoding='utf-8'))
    report = Path(baseline['report'])
    cells, merges = xlsx_source(report)
    directory = DEST if published else QA / 'candidate'
    docs = {p.relative_to(directory).as_posix(): p.read_text(encoding='utf-8') for p in directory.rglob('*.md')}
    units = []
    for address, value in cells.items():
        if not address.startswith('D'):
            continue
        m = re.fullmatch(r'SW-FW-UnitTest-(CFX|HEAR|CM3|BOOT)-ID-(\d+)', value.strip())
        if not m:
            continue
        first = int(address[1:])
        end = next((int(ref.split(':')[1][1:]) for ref in merges if ref.startswith(address + ':D')), first)
        units.append((m[1], int(m[2]), first, end))
    errors, stats = [], Counter()
    all_result_logs = []
    for core, num, first, last in units:
        name = f'{core}-{num:03d}'
        path = f'{core}/{name}.md'
        if path not in docs:
            errors.append(('missing_unit', path)); continue
        found = SOURCE.findall(docs[path])
        actual = {address: decode(body) for address, body in found}
        if len(actual) != len(found):
            errors.append(('duplicate_cell', path))
        expected = {f'{col}{r}': cells[f'{col}{r}'] for col in 'EFGHIJKLM'
                    for r in range(first, last + 1) if cells.get(f'{col}{r}', '')}
        if actual != expected:
            for address in set(actual) | set(expected):
                if actual.get(address) != expected.get(address):
                    errors.append(('cell_diff', path, address, repr(actual.get(address)), repr(expected.get(address))))
        for col in 'EFGHIJKLM':
            addresses = [a for a, _ in found if a.startswith(col)]
            if addresses != sorted(addresses, key=lambda a: int(a[1:])):
                errors.append(('cell_order', path, col))
        stats['units'] += 1
        stats['unit_cells'] += len(expected)
        stats['result_cells'] += sum(a.startswith('L') for a in expected)
        results = {a: v for a, v in expected.items() if a.startswith('L')}
        logs = [line for value in results.values() for line in LOG.findall(value)]
        all_result_logs.extend(logs)
        log_path = f'예상 로그/{name}.md'
        if log_path in docs:
            copies = {a: decode(body) for a, body in SOURCE.findall(docs[log_path])}
            wanted = {a: v for a, v in results.items() if '[UT-' in v}
            if copies != wanted:
                errors.append(('log_cell_diff', log_path))
            stats['log_documents'] += 1
            if LOG.findall(docs[log_path]) != logs:
                errors.append(('log_lines', log_path))
        elif logs:
            errors.append(('missing_logs', log_path))
    for name, body in docs.items():
        for match in re.finditer(r'(?<!\\)\[[^\]\n]+\]\(([^)\n]+)\)', body):
            href = unquote(match[1].split('#')[0])
            if not href or re.match(r'[a-z]+:', href):
                continue
            final = (DEST / name).parent.joinpath(href).resolve()
            if final.is_relative_to(DEST):
                exists = final.relative_to(DEST).as_posix() in docs
            else:
                exists = final.exists()
            if not exists:
                errors.append(('broken_link', name, href))
            stats['links'] += 1
    for name in ('HEAR-001', 'HEAR-002'):
        body = docs[f'예상 로그/{name}.md']
        if f'[UT-{name}] C99 로그 출력 없음.' not in decode(body):
            # This text is Markdown-escaped outside a code fence.
            flattened = '\n'.join(decode(b) for _, b in SOURCE.findall(body))
            if f'[UT-{name}] C99 로그 출력 없음.' not in flattened:
                errors.append(('missing_c99_absence', name))
        if re.search(rf'\[UT-{name}\] C99 (?:A\[|FAIL|PASS)', body):
            errors.append(('incorrect_c99_emission', name))
    if any('legacy_current' in body for body in docs.values()):
        errors.append(('legacy_remaining',))
    if '[UT-CM3-027] C1 PASS N=2 F=0' in '\n'.join(docs.values()):
        errors.append(('old_cm3_log',))
    changed = [p for p, h in baseline['protected'].items() if not (ROOT / p).exists() or sha(ROOT / p) != h]
    if changed:
        errors.append(('protected_changed', changed))
    if sha(report) != baseline['report_sha256']:
        errors.append(('source_changed',))
    stats['rtt_conditions'] = len(set(re.match(r'(\[UT-[^\]]+\] C\d+)', x)[1] for x in all_result_logs))
    stats['rtt_lines'] = len(all_result_logs)
    stats['documents'] = len(docs)
    stats['protected_files'] = len(baseline['protected'])
    result = {'ok': not errors, 'report_sha256': sha(report), 'published': published,
              'counts': dict(stats), 'errors': errors}
    (QA / ('published-verification.json' if published else 'verification.json')).write_text(
        json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if errors:
        raise SystemExit(1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--published', action='store_true')
    verify(parser.parse_args().published)
