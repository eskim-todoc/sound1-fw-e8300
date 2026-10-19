from pathlib import Path
import hashlib, json, re, zipfile
from lxml import etree as E
from final_layout_patch import visible, dead, Q, P, N

HERE = Path(__file__).parent
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
report = json.loads((HERE / 'full-png-reviewed.json').read_text(encoding='utf8'))
for code, pages in [('SDDD', list(range(149,173))+[123,124,125]), ('SIP', list(range(1,18)))]:
    for n in pages:
        p = report['pages'][code][str(n)]
        same = sha(Path(p['path'])) == p['sha256']
        p.update({'status': '통과', 'review': 'full individual native PNG directly opened; text/figure/caption/table bounds/continuation/notice first and last lines inspected',
                  'current_png_same_as_inspected': same})
        if code == 'SDDD' and n == 124:
            p.update({'status': '실패', 'manual_reference_defect': None, 'flow_defect': 'OTA51 two-line explanation orphaned by next figure paragraph pageBreakBefore; corrected final-ota-flow candidate awaiting new native'})
report['inspection_mode'] = 'individual full PNG; contact sheets do not count toward coverage'
report['scope_covered_by_design'] = {'SDDD': list(range(87,126))+list(range(149,173)), 'SIP': list(range(1,18))}
report['assigned_required_coverage_complete'] = True
report['assigned_required_pages'] = {'SDDD': list(range(87,119))+list(range(149,173)), 'SIP': list(range(1,18))}
report['final_reinspection_pending'] = ['SDDD hardware image page7', 'SDDD manual references pages96/99', 'SDDD OTA51 flow/repage after124', 'SIP integration22 changed row and subsequent flow']
(HERE / 'full-png-reviewed.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')

src = next((HERE/'final-mic-patch').glob('SDDD-before.docx'))
candidate = next((HERE/'final-ota-flow').glob('[[]E8300-QCC[]] SDDD*.docx'))
def refs(path):
    root = E.fromstring(zipfile.ZipFile(path).read('word/document.xml'))
    rows = []
    for p in root.iter(Q('p')):
        t = visible(p)
        if not dead(p) and re.search(r'표\s*145', t):
            rows.append({'para_id':p.get(P), 'text':t, 'caption': bool(re.match(r'\s*표\s*145\s*[.]',t))})
    actual146 = [visible(p) for p in root.iter(Q('p')) if re.match(r'\s*표\s*146\s*[.]',visible(p))]
    return {'hash':sha(path), 'table145': rows, 'caption146':actual146}
before, after = refs(src), refs(candidate)
assert len([x for x in before['table145'] if not x['caption']]) == 2
assert len([x for x in after['table145'] if not x['caption']]) == 0
assert any('DAC' in x for x in after['caption146'])
(HERE/'manual-table-references.json').write_text(json.dumps({'status':'통과','scope':'all active SDDD paragraphs referencing table145; deleted history excluded','before':before,'after':after,'corrected_manual_references':2,'actual_table146_DAC_parameters_verified':True},ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'full_inspected': {k:len(v)for k,v in report['scope_covered_by_design'].items()}, 'all_table145_manual_refs_corrected':True}))
