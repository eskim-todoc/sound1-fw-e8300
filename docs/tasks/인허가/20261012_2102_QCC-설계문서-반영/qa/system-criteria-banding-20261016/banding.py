from pathlib import Path
from copy import deepcopy
from collections import Counter, defaultdict
from datetime import datetime
import json, re, shutil, sys
from lxml import etree as E

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H.parent / 'unit-rebuild-20261016'))
from rebuild import Book, S, digest, xml, stripns, writebook

TARGET = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기/[E8300-QCC] [완료 - QCC 입력 대기] SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서.xlsx')
BASE_SHA = 'D3F7E5FEA16AA448BE548CF9ADB507B85DE06CADAE73C00DE94C97451F5FB301'
SHEET = '기준서_시스템 시험'
COLUMNS = 'BCDEFGHIJ'

def save(name, obj):
    (H / name).write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

def prepare():
    assert digest(TARGET) == BASE_SHA, 'Current source changed; stop without overwriting.'
    before = H / 'before.xlsx'
    if before.exists():
        assert digest(before) == BASE_SHA
    else:
        shutil.copy2(TARGET, before)
    b = Book(before)
    starts = []
    for addr in b.cells[SHEET]:
        if not re.fullmatch(r'B\d+', addr):
            continue
        m = re.fullmatch(r'\s*SW-FW-SYS-Test-ID-(\d+)\s*', b.value(SHEET, addr))
        if m:
            starts.append((int(addr[1:]), int(m[1])))
    starts.sort()
    assert len(starts) == 83 and {i for r, i in starts} == set(range(1, 84))
    assert starts[0] == (34, 1) and starts[-1] == (292, 83)
    blocks = [{'order':j+1, 'id':i, 'testID':f'SW-FW-SYS-Test-ID-{i}', 'start':r,
               'end':starts[j+1][0]-1 if j+1 < len(starts) else 292,
               'color':'FFFFFF' if j % 2 == 0 else 'F5F5F5'}
              for j, (r, i) in enumerate(starts)]
    merges = [x.get('ref') for x in b.sheets[SHEET].findall('.//' + S + 'mergeCell')]
    crossings = [ref for ref in merges if any(int(re.findall(r'\d+', ref)[0]) < r <= int(re.findall(r'\d+', ref)[-1]) for r, i in starts)]
    assert not crossings, crossings
    missing = [f'{c}{r}' for r in range(34, 293) for c in COLUMNS if f'{c}{r}' not in b.cells[SHEET]]
    assert not missing, missing
    xfs = b.styles.find(S+'cellXfs')
    fills = b.styles.find(S+'fills')
    fill_counts = Counter()
    pink = []
    for r in range(34, 293):
        for c in COLUMNS:
            addr = f'{c}{r}'
            fid = int(xfs[int(b.cells[SHEET][addr].get('s', '0'))].get('fillId', '0'))
            fill_counts[fid] += 1
            fg = fills[fid].find(S+'patternFill/'+S+'fgColor')
            if fg is not None and fg.get('rgb') == 'FFFFEBFF':
                pink.append(addr)
    plan = {'target':str(TARGET), 'sha':BASE_SHA, 'sheet':SHEET, 'columns':COLUMNS,
            'range':'B34:J292', 'blocks':blocks,
            'sourceAudit':{'sheetNames':list(b.names), 'mergeCrossings':crossings,
                           'sourceFillIDCounts':dict(fill_counts), 'pinkCells':pink,
                           'pinkTestCount':len({b.value(SHEET,a).strip() for a in pink if b.value(SHEET,a).strip()}),
                           'conditionalFormattingCount':len(b.sheets[SHEET].findall(S+'conditionalFormatting'))},
            'preservationContract':'Only fillId/applyFill of target cells, with append-only equivalent cellXfs. Every other style property, cell value/rich run/formula/cache, row/column/merge, picture and all other ZIP parts stay unchanged.',
            'marker':'Root ran the shared three-output edit marker exactly once; this helper never invokes marker.',
            'authoringState':'WAITING_FOR_ROOT_FINAL_COLOR_DECISION'}
    pink_heads = {a for a in pink if b.value(SHEET, a).strip()}
    preserved_ranges = []
    preserved = []
    for block in blocks:
        head = 'B'+str(block['start'])
        if head not in pink_heads:
            continue
        refs = [ref for ref in merges if ref.split(':')[0] == head]
        assert len(refs) <= 1
        ref = refs[0] if refs else head
        assert re.fullmatch(r'B\d+(?::B\d+)?', ref), ref
        bounds = [int(x) for x in re.findall(r'\d+', ref)]
        assert bounds[-1] <= block['end']
        preserved_ranges.append({'testID':block['testID'], 'range':ref})
        preserved.extend('B'+str(r) for r in range(bounds[0], bounds[-1]+1))
    plan['preservedPinkRanges'] = preserved_ranges
    plan['preserveAddresses'] = preserved
    plan['authoringState'] = 'AUTHORIZED_WHITE_GRAY_EXCEPT_ORIGINAL_PINK_ID_MERGES'
    plan['userColorDecision'] = 'Keep the existing pink TEST ID merged regions; alternate all other table cells white/light gray by physical TEST block order.'
    save('plan.json', plan)
    print(json.dumps({'status':'prepared', 'sha':BASE_SHA, 'blocks':len(blocks), 'cells':259*9, 'pinkCells':len(pink)}, ensure_ascii=False))

def integrate():
    p = json.loads((H/'plan.json').read_text(encoding='utf-8'))
    assert p.get('authoringState') == 'AUTHORIZED_WHITE_GRAY_EXCEPT_ORIGINAL_PINK_ID_MERGES'
    assert digest(TARGET) == BASE_SHA, 'Source changed during authoring; candidate not published.'
    b = Book(H/'before.xlsx')
    a = Book(H/'authored.xlsx')
    xfs, fills = b.styles.find(S+'cellXfs'), b.styles.find(S+'fills')
    axfs, afills = a.styles.find(S+'cellXfs'), a.styles.find(S+'fills')
    cache, changes = {}, []
    for block in p['blocks']:
        fid = next(i for i, f in enumerate(fills) if f.find(S+'patternFill/'+S+'fgColor') is not None and f.find(S+'patternFill/'+S+'fgColor').get('rgb') == 'FF'+block['color'])
        for r in range(block['start'], block['end']+1):
            for col in COLUMNS:
                addr = f'{col}{r}'
                if addr in p['preserveAddresses']:
                    continue
                ac = a.cells[SHEET][addr]
                af = afills[int(axfs[int(ac.get('s','0'))].get('fillId', '0'))]
                fg = af.find(S+'patternFill/'+S+'fgColor')
                assert fg is not None and fg.get('rgb', '')[-6:] == block['color'], (addr, 'Artifact Tool authored fill mismatch')
                cell = b.cells[SHEET][addr]
                old = int(cell.get('s', '0'))
                key = old, fid
                if key not in cache:
                    xf = deepcopy(xfs[old])
                    xf.set('fillId', str(fid))
                    xf.set('applyFill', '1')
                    signature = stripns(xf)
                    idx = next((i for i, existing in enumerate(xfs) if stripns(existing) == signature), None)
                    if idx is None:
                        idx = len(xfs)
                        xfs.append(xf)
                    cache[key] = idx
                if cache[key] != old:
                    changes.append(addr)
                cell.set('s', str(cache[key]))
    xfs.set('count', str(len(xfs)))
    b.parts['xl/styles.xml'] = xml(b.styles)
    b.parts[b.names[SHEET]] = xml(b.sheets[SHEET])
    writebook(b.parts, H/'candidate.xlsx')
    verify()
    print(json.dumps({'candidateSHA':digest(H/'candidate.xlsx'), 'changedStyleReferences':len(changes)}))

def verify():
    p = json.loads((H/'plan.json').read_text(encoding='utf-8'))
    b, a = Book(H/'before.xlsx'), Book(H/'candidate.xlsx')
    assert a.parts.keys() == b.parts.keys()
    exceptions = {'xl/styles.xml', b.names[SHEET]}
    exact_parts = [k for k in b.parts if k not in exceptions]
    assert all(a.parts[k] == b.parts[k] for k in exact_parts)
    ax, bx = a.styles.find(S+'cellXfs'), b.styles.find(S+'cellXfs')
    assert all(stripns(x) == stripns(ax[i]) for i, x in enumerate(bx)), 'Existing styles were changed'
    aa, bb = deepcopy(a.styles), deepcopy(b.styles)
    aa.remove(aa.find(S+'cellXfs'))
    bb.remove(bb.find(S+'cellXfs'))
    assert stripns(aa) == stripns(bb), 'Non-cellXfs style groups changed'
    preserved = set(p['preserveAddresses'])
    expected = {f'{col}{r}':block['color'] for block in p['blocks'] for r in range(block['start'],block['end']+1) for col in COLUMNS if f'{col}{r}' not in preserved}
    assert a.cells[SHEET].keys() == b.cells[SHEET].keys()
    changed = []
    for addr, cell in a.cells[SHEET].items():
        orig = b.cells[SHEET][addr]
        cc, oo = deepcopy(cell), deepcopy(orig)
        if addr in expected:
            af, bf = deepcopy(ax[int(cell.get('s','0'))]), deepcopy(bx[int(orig.get('s','0'))])
            fill = a.styles.find(S+'fills')[int(af.get('fillId','0'))]
            assert fill.find(S+'patternFill').get('patternType') == 'solid'
            assert fill.find(S+'patternFill/'+S+'fgColor').get('rgb') == 'FF'+expected[addr]
            for key in ('fillId', 'applyFill'):
                af.attrib.pop(key, None)
                bf.attrib.pop(key, None)
            assert stripns(af) == stripns(bf), (addr, 'Non-fill style changed')
            if cell.get('s') != orig.get('s'):
                changed.append(addr)
            cc.attrib.pop('s', None)
            oo.attrib.pop('s', None)
        assert stripns(cc) == stripns(oo), (addr, 'Cell content/attributes changed')
    ash, bsh = deepcopy(a.sheets[SHEET]), deepcopy(b.sheets[SHEET])
    for sh in (ash, bsh):
        for c in sh.findall('.//'+S+'sheetData/'+S+'row/'+S+'c'):
            if c.get('r') in expected:
                c.attrib.pop('s', None)
    assert stripns(ash) == stripns(bsh), 'Non-style sheet XML changed'
    assert digest(TARGET) == BASE_SHA, 'Live source changed; no publication permitted'
    q = {'status':'PASS', 'candidateSHA':digest(H/'candidate.xlsx'), 'sourceSHA':BASE_SHA,
         'sourceUnchanged':True, 'sheet':SHEET, 'range':p['range'], 'blocks':len(p['blocks']),
         'targetCellsChecked':len(expected), 'allMainSheetCellsChecked':len(a.cells[SHEET]),
         'preservedPinkIDCellsChecked':len(preserved), 'preservedPinkRanges':p['preservedPinkRanges'],
         'changedStyleReferences':len(changed), 'otherZipPartsByteIdentical':exact_parts,
         'cellContentRichTextFormulaCachedValues':'exact unchanged',
         'nonFillStyleProperties':'exact unchanged',
         'sheetStructureMergesRowsHeightsColumnsWidthsPrintSettings':'exact unchanged',
         'otherSheetsMediaDrawingsRelationships':'byte-identical',
         'SV29':{'24':'Native display QA assigned to design agent; layout XML unchanged.', '25':'Fill-only edit; existing sheet structure/history untouched.', '27':'Source/approval metadata preserved.', '28':'OOXML strict comparison passed; native QA pending.', 'otherPrinciples':'해당없음: content/design/test-result changes outside this fill-only request.'},
         'nativeVisualQA':'PENDING_DESIGN_AGENT'}
    save('validation.json', q)
    prefixes = ['[QCC 자료 입력]', '[QCC/E8300 자료 입력]', '[E8300 보완]']
    notices = {'sourceSHA':BASE_SHA, 'candidateSHA':q['candidateSHA'], 'sheets':{}}
    for name in a.names:
        if name.endswith('_비교'):
            continue
        notices['sheets'][name] = {prefix:[addr for addr in a.cells[name] if prefix in a.value(name,addr)] for prefix in prefixes}
    save('notices.json', notices)
    print(json.dumps({'status':q['status'], 'blocks':q['blocks'], 'cells':len(expected), 'candidateSHA':q['candidateSHA']}))

if __name__ == '__main__':
    {'prepare':prepare, 'integrate':integrate, 'verify':verify}[sys.argv[1]]()
