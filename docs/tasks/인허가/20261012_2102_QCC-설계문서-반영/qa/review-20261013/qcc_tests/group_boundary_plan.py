from pathlib import Path
from lxml import etree as E
import json, zipfile, shutil
from integrate import sheetmap, vals, N, sha
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
V=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
out={}
for k,b in P['books'].items():
    with zipfile.ZipFile(b['path']) as z: data={i.filename:z.read(i.filename) for i in z.infolist()}
    sn=next(s for s in sheetmap(data) if s.startswith(('기준서_','보고서_')))
    member=sheetmap(data)[sn]; root=E.fromstring(data[member])
    criterion={'SUVR':'SUVC','SITR':'SITC','SSTR':'SSTC'}.get(k,k)
    cs=next(s for s in V[criterion] if s.startswith('기준서_'))
    idcol={'SU':'D','SI':'C','SS':'B'}[k[:2]]
    ids=sorted((int(a[1:]),t.strip()) for a,t in V[criterion][cs].items() if a.startswith(idcol) and a[1:].isdigit() and t.strip().startswith('SW-FW-'))
    columns={'SU':['H','J'],'SI':['J','L'],'SS':['G','I']}[k[:2]]
    comparison=P['books'][criterion]['comparison'][cs]
    selected=[{'start_row':r,'id':t} for r,t in ids[1:] if any(c+str(r) in comparison for c in columns)]
    extra=[206,218] if k.startswith('SS') else []
    rows=sorted(set([x['start_row'] for x in selected]+extra))
    existing={int(x.get('id'))+1 for x in root.findall('x:rowBreaks/x:brk',N)}
    out[k]={'path':b['path'],'sha256_before':sha(b['path']),'sheet':sn,'member':member,
            'all_group_ids':ids,'page_break_before_rows':rows,'new_break_before_rows':sorted(set(rows)-existing),
            'changed_short_group_scope':selected,'extra_condition_breaks':extra,
            'top_alignment_cells':['H82','H206'] if k.startswith('SS') else [],
            'candidate':str(H/'group-boundary-candidate'/Path(b['path']).name),
            'height_changes':{},'text_changes':{},
            'reason':'Fix each changed short preparation/procedure at its test start; preserve normal long RTT continuation. H82/H206 limited notes top aligned, J218 date comparison condition starts separately. No test input/result/PASS added.'}
archive=H/'group-boundary-before-native';archive.mkdir(exist_ok=True)
for f in (H.parent/'native-excel'/'tests').glob('*.pdf'):
    dst=archive/f.name
    if not dst.exists():shutil.copyfile(f,dst)
(H/'group-boundary-plan.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:{'groups':len(b['all_group_ids']),'selected_group_starts':len(b['changed_short_group_scope']),'new_breaks':len(b['new_break_before_rows']),'condition_breaks':b['extra_condition_breaks'],'align':b['top_alignment_cells'],'before':b['sha256_before']} for k,b in out.items()},ensure_ascii=True))
