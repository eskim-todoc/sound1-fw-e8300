"""Prepare/inspect a native Word-only tracked empty-paragraph deletion probe.

No G-drive writes and no COM. Temporary bookmarks/authorship distinguish this
operation from historical revisions; final native revision author is 김은수.
"""
from pathlib import Path
from copy import deepcopy
from collections import defaultdict
from zipfile import ZipFile, ZIP_DEFLATED
import argparse, hashlib, json, re
from lxml import etree as E

ROOT = Path(__file__).parent
OUT = ROOT / 'native-trace-probe'
BASE = ROOT / 'final-sad-trace-flow' / 'SAD-before.docx'
EXPECTED_SHA = '8af46ef971b15ec6815cb5aed66e25580a4c9494d43dcdf4925852cb38c50515'
AUTHOR = 'QCCNativeParagraphProbe20261013'
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N = {'w': W}
Q = lambda s: '{' + W + '}' + s
PID = '{http://schemas.microsoft.com/office/word/2010/wordml}paraId'

def sha(data): return hashlib.sha256(data).hexdigest()
def digest(path): return sha(Path(path).read_bytes())
def compact(s): return re.sub(r'\s|\x07', '', s)
def dead(x):
    return any(a.tag == Q('del') or (a.tag == Q('tr') and a.find('w:trPr/w:del', N) is not None) for a in [x, *x.iterancestors()])
def visible(x):
    if dead(x) or x.tag in (Q('pPr'), Q('rPr'), Q('trPr'), Q('tcPr'), Q('tblPr')): return ''
    if x.tag == Q('t'): return x.text or ''
    if x.tag in (Q('br'), Q('tab')): return '\n'
    return ''.join(visible(c) for c in x)
def current_table(root):
    matches = []
    for tbl in root.iter(Q('tbl')):
        rows = []
        for tr in tbl.findall('w:tr', N):
            if dead(tr): continue
            cells = tr.findall('w:tc', N)
            if len(cells) != 3: continue
            first = compact(visible(cells[0]))
            rows.append({'id': first, 'cells': [visible(c) for c in cells]})
        ids = [r['id'] for r in rows if re.fullmatch(r'SW-FW-RID-\d+', r['id'])]
        if ids == [f'SW-FW-RID-{i}' for i in [*range(1,82),83]]:
            assert len(rows) == 83, len(rows)
            matches.append((tbl, rows))
    assert len(matches) == 1, len(matches)
    return matches[0]
def load(path):
    with ZipFile(path) as z:
        parts = {n: z.read(n) for n in z.namelist()}
    return parts, E.fromstring(parts['word/document.xml'])
def save(path, parts, root):
    data = dict(parts)
    data['word/document.xml'] = E.tostring(root, xml_declaration=True, encoding='UTF-8', standalone=True)
    with ZipFile(path, 'w', ZIP_DEFLATED) as z:
        for n, b in data.items(): z.writestr(n,b)
def revision_text(root, ignore_probe=False):
    groups = defaultdict(list)
    for rev in root.xpath('.//w:ins | .//w:del', namespaces=N):
        if ignore_probe and rev.get(Q('author')) == AUTHOR: continue
        txt = ''.join(t.text or '' for t in rev.xpath('.//w:t|.//w:delText',namespaces=N))
        if txt:
            key = '|'.join([E.QName(rev).localname, rev.get(Q('author'), ''), rev.get(Q('date'),'')[:16]])
            groups[key].append(txt)
    return {k: sha(''.join(v).encode()) for k,v in groups.items()}
def revision_inventory(root, ignore_probe=False):
    counts=defaultdict(int)
    for rev in root.xpath('.//*[@w:author]',namespaces=N):
        if ignore_probe and rev.get(Q('author'))==AUTHOR:continue
        key='|'.join([E.QName(rev).localname,rev.get(Q('author'),''),rev.get(Q('date'),'')[:16]])
        counts[key]+=1
    return dict(counts)
def prepare():
    OUT.mkdir(exist_ok=True)
    assert digest(BASE) == EXPECTED_SHA
    parts, doc = load(BASE)
    tbl, rows = current_table(doc)
    targets = []
    max_id = max([int(v) for v in doc.xpath('.//w:bookmarkStart/@w:id',namespaces=N)] + [0])
    for tr in tbl.findall('w:tr', N):
        if dead(tr): continue
        cells = tr.findall('w:tc', N)
        rid = compact(visible(cells[0]))
        if not re.fullmatch(r'SW-FW-RID-\d+', rid): continue
        for col,tc in enumerate(cells[1:],2):
            if ('SI-ID-QCC-' if col == 2 else 'SU-ID-QCC-') not in visible(tc): continue
            for p in tc.findall('w:p', N):
                if p.find('w:pPr/w:rPr/w:del',N) is not None or visible(p).strip(): continue
                assert not p.xpath('.//w:drawing|.//w:fldChar|.//w:instrText',namespaces=N)
                name = f'QCCBlank_{len(targets)+1:03d}'
                max_id += 1
                start=E.Element(Q('bookmarkStart'),{Q('id'):str(max_id),Q('name'):name})
                end=E.Element(Q('bookmarkEnd'),{Q('id'):str(max_id)})
                offset = 1 if p.find('w:pPr',N) is not None else 0
                p.insert(offset,start);p.insert(offset+1,end)
                targets.append({'bookmark':name,'paragraph_id':p.get(PID),'requirement':rid,'column':col})
    assert len(targets)==183,len(targets)
    prepared=OUT/'SAD-bookmarked-source.docx'
    save(prepared,parts,doc)
    expected_xml = E.Element('expected-native-requirement-cells', rows='83', requirements='82', columns='3', source_sha256=EXPECTED_SHA)
    for index,row in enumerate(rows):
        xr=E.SubElement(expected_xml,'row',index=str(index),id=row['id'])
        for column,text in enumerate(row['cells'],1):
            E.SubElement(xr,'cell',column=str(column)).text=text
    xmlpath=OUT/'expected-cells.xml'
    xmlpath.write_bytes(E.tostring(expected_xml,xml_declaration=True,encoding='UTF-8',pretty_print=True))
    expect={'source':str(BASE),'source_sha256':EXPECTED_SHA,'prepared':str(prepared),'prepared_sha256':digest(prepared),'probe_author':AUTHOR,
            'expected_row_count':83,'expected_requirement_count':82,'rows':rows,'targets':targets,
            'comparison':'3 cells x (header + 82 requirements), actual native accepted/rejected cell text compared after removing only whitespace and cell terminator',
            'expected_xml':str(xmlpath),'expected_xml_sha256':digest(xmlpath),
            'original_revision_text_signatures':revision_text(doc),'original_revision_inventory':revision_inventory(doc),'status':'미실행'}
    (OUT/'expected-cells.json').write_text(json.dumps(expect,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'prepared':str(prepared),'targets':len(targets),'expected_rows':len(rows),'sha':digest(prepared)},ensure_ascii=True))

def inspect_native(native_json):
    expect=json.loads((OUT/'expected-cells.json').read_text(encoding='utf8'))
    record=json.loads(Path(native_json).read_text(encoding='utf-8-sig'))
    comparisons={}
    for mode in ('baseline','accept','reject_new'):
        actual=record.get(mode,{}).get('rows',[])
        differences=[]
        if len(actual)!=83: differences.append({'row_count':len(actual),'expected':83})
        for i,(a,b) in enumerate(zip(actual,expect['rows']),1):
            if len(a['cells'])!=3: differences.append({'row':i,'cell_count':len(a['cells'])});continue
            for col,(aa,bb) in enumerate(zip(a['cells'],b['cells']),1):
                if compact(aa)!=compact(bb): differences.append({'row':i,'id':b['id'],'column':col,'expected':bb,'actual':aa})
        comparisons[mode]={'pass':not differences,'differences':differences,'actual_rows':len(actual)}
    candidate=Path(record['candidate'])
    parts,doc=load(candidate)
    _,base=load(BASE)
    prior_ok=revision_text(base)==revision_text(doc,ignore_probe=True)
    comparisons['historical_revision_text_preserved']={'pass':prior_ok}
    before_inv=revision_inventory(base)
    after_inv=revision_inventory(doc,ignore_probe=True)
    prior_inventory_changes={k:{'before':before_inv.get(k,0),'after':after_inv.get(k,0)} for k in before_inv.keys()|after_inv.keys() if before_inv.get(k,0)!=after_inv.get(k,0)}
    new_revs=doc.xpath('.//*[@w:author=$author]',namespaces=N,author=AUTHOR)
    comparisons['new_native_revision_created']={'pass':bool(new_revs),'count':len(new_revs)}
    comparisons['only_paragraph_marks_deleted']={'pass':all(not r.xpath('.//w:t|.//w:delText|.//w:drawing',namespaces=N) for r in new_revs)}
    ok=record.get('status')=='PASS' and all(c['pass'] for c in comparisons.values())
    proof={'source_sha256':EXPECTED_SHA,'native_candidate':str(candidate),'native_candidate_sha256':digest(candidate),'checks':comparisons,'pass':ok,
           'historical_revision_inventory_changes':prior_inventory_changes,
           'native_evidence':str(native_json),'finalized':False,'G_published':False}
    if ok:
        # These are Word-generated paragraph revisions, renamed only after
        # independent native accept and selective-reject cell checks pass.
        for rev in new_revs:rev.set(Q('author'),'김은수')
        for bm in list(doc.xpath('.//w:bookmarkStart[starts-with(@w:name,"QCCBlank_")]',namespaces=N)):
            bid=bm.get(Q('id'));bm.getparent().remove(bm)
            for end in list(doc.xpath('.//w:bookmarkEnd[@w:id=$bid]',namespaces=N,bid=bid)):end.getparent().remove(end)
        final=OUT/'SAD-native-tracked-candidate.docx'
        save(final,parts,doc)
        proof.update({'finalized':True,'candidate':str(final),'candidate_sha256':digest(final),'new_revision_count':len(new_revs),'author':'김은수',
                      'scope':'native-generated empty paragraph deletion; unchanged ordinary text/history. Final author/bookmark-only transformation needs final native open/accept/reject guard before publish.'})
    (OUT/'proof.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'pass':ok,'finalized':proof['finalized'],'proof':str(OUT/'proof.json')},ensure_ascii=True))
    if not ok:raise SystemExit(1)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('mode',choices=['prepare','inspect']);p.add_argument('native_json',nargs='?');a=p.parse_args()
    prepare() if a.mode=='prepare' else inspect_native(a.native_json)
