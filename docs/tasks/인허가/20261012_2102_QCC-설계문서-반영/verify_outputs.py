"""Independent saved-package verification; no artifact or workbook writes."""
from pathlib import Path
from zipfile import ZipFile
import hashlib, json, re, sys, posixpath
from lxml import etree

sys.stdout.reconfigure(encoding='utf-8')

HERE=Path(__file__).resolve().parent
BASE=json.loads((HERE/'evidence/source-baseline.json').read_text(encoding='utf-8-sig'))
N={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main',
   'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def output(item): return Path(item['path'].replace('[E8300]', '[E8300-QCC]'))
def xml(z,p): return etree.fromstring(z.read(p))
def col(ref):
    n=0
    for x in re.match('[A-Z]+',ref)[0]: n=n*26+ord(x)-64
    return n
def sheet(path):
    with ZipFile(path) as z:
        strings=[]
        if 'xl/sharedStrings.xml' in z.namelist():
            strings=[''.join(s.itertext()) for s in xml(z,'xl/sharedStrings.xml').findall('s:si',N)]
        book=xml(z,'xl/workbook.xml')
        rels=xml(z,'xl/_rels/workbook.xml.rels')
        links={r.get('Id'):r.get('Target') for r in rels}
        tabs=book.findall('s:sheets/s:sheet',N)
        selected=next(s for s in tabs if s.get('name')!='표지')
        target=links[selected.get('{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id')]
        part=target.lstrip('/') if target.startswith('/') else 'xl/'+target
        root=xml(z,part)
        values={}
        for cell in root.findall('.//s:sheetData/s:row/s:c',N):
            t=cell.get('t'); v=cell.find('s:v',N)
            text=(v.text or '') if v is not None else ''
            if t=='s': text=strings[int(text)]
            elif t=='inlineStr': text=''.join(cell.find('s:is',N).itertext())
            values[cell.get('r')]=text
        merges=[m.get('ref') for m in root.findall('s:mergeCells/s:mergeCell',N)]
        heights={r.get('r'):float(r.get('ht')) for r in root.findall('s:sheetData/s:row',N) if r.get('ht')}
        text='\n'.join(values.values())
        return {'values':values,'merges':merges,'heights':heights,'text':text,'name':selected.get('name'),'part':part}
def visible(root):
    def accepted(t):
        for a in t.iterancestors():
            if a.tag==f'{{{N["w"]}}}del': return False
            if a.tag==f'{{{N["w"]}}}tr' and a.find('w:trPr/w:del',N) is not None: return False
        return True
    return ''.join(t.text or '' for t in root.findall('.//w:t',N) if accepted(t))

result={'originals':[], 'files':[], 'pairs':[], 'failures':[]}
for item in BASE:
    actual=sha(item['path'])
    okay=actual==item['sha256'].upper()
    result['originals'].append({'path':item['path'],'unchanged':okay})
    if not okay: result['failures'].append('Original changed: '+item['path'])

outitems=[i for i in BASE if '[E8300]' in i['path'] and re.search(r'\b(SR|SGR|SRS|SAD|SDDD|SIP|SSTC|SSTR|SUVC|SUVR|SITC|SITR)-',i['path'])]
lookup={}
for item in outitems:
    path=output(item); code=re.search(r'\] (\w+)-',path.name)[1]; lookup[code]=path
    record={'code':code,'path':str(path),'exists':path.is_file()}
    if not path.is_file(): result['failures'].append('Missing: '+str(path)); result['files'].append(record); continue
    record['sha256']=sha(path); record['changed_from_original']=record['sha256']!=item['sha256'].upper()
    with ZipFile(path) as z, ZipFile(item['path']) as old:
        for name in z.namelist():
            if name.endswith('.xml'): xml(z,name)
        record['all_xml_parse']=True
        if path.suffix=='.docx':
            doc=xml(z,'word/document.xml'); olddoc=xml(old,'word/document.xml')
            before={(x.tag,x.get('{'+N['w']+'}id')) for x in olddoc.findall('.//w:ins',N)+olddoc.findall('.//w:del',N)}
            revisions=doc.findall('.//w:ins',N)+doc.findall('.//w:del',N)
            new=[x for x in revisions if (x.tag,x.get('{'+N['w']+'}id')) not in before]
            record['new_revision_count']=len(new)
            record['new_revision_authors']=sorted({x.get('{'+N['w']+'}author') for x in new})
            record['visible_qcc_markers']=visible(doc).count('[QCC 자료 입력]')
            record['visible_joint_markers']=visible(doc).count('[QCC/E8300 자료 입력]')
            record['visible_e8300_markers']=visible(doc).count('[E8300 보완]')
            record['nested_revision_count']=sum(any(a.tag in (f'{{{N["w"]}}}ins',f'{{{N["w"]}}}del') for a in x.iterancestors()) for x in new)
            def track_signature(x):
                return (x.get('{'+N['w']+'}author'),(x.get('{'+N['w']+'}date') or '')[:16],
                        ''.join(t.text or '' for t in x.iter() if t.tag in (f'{{{N["w"]}}}t',f'{{{N["w"]}}}delText')))
            old_insertions={track_signature(x) for x in olddoc.findall('.//w:ins',N)}
            illegal=[]
            supported=0
            for x in revisions:
                parents=[a for a in x.iterancestors() if a.tag in (f'{{{N["w"]}}}ins',f'{{{N["w"]}}}del')]
                if not parents: continue
                if x.tag==f'{{{N["w"]}}}del' and all(a.tag==f'{{{N["w"]}}}ins' and track_signature(a) in old_insertions for a in parents): supported+=1
                else: illegal.append(x.get('{'+N['w']+'}id'))
            record['deletions_inside_original_insertions']=supported
            record['unsupported_nested_revisions']=illegal
            record['original_media_preserved']=all(n in z.namelist() and z.read(n)==old.read(n) for n in old.namelist() if n.startswith('word/media/'))
            hashes={hashlib.sha256(z.read(n)).hexdigest() for n in z.namelist() if n.startswith('word/media/')}
            active=[]
            refs=set(olddoc.xpath('//@r:embed | //@r:id',namespaces={'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}))
            for rel in xml(old,'word/_rels/document.xml.rels'):
                target=posixpath.normpath('word/'+rel.get('Target',''))
                if rel.get('Id') in refs and target.startswith('word/media/'):
                    active.append({'original_part':target,'bytes_present':hashlib.sha256(old.read(target)).hexdigest() in hashes})
            record['active_original_body_media']=active
            record['active_original_body_media_bytes_preserved']=all(m['bytes_present'] for m in active)
            if not record['active_original_body_media_bytes_preserved']: result['failures'].append(code+' active original media bytes missing')
            if not new: result['failures'].append(code+' has no new tracked changes')
            if illegal: result['failures'].append(code+' unsupported nested revisions')
        else:
            s=sheet(path); original=sheet(item['path'])
            record['row_height_over_limit']={k:v for k,v in s['heights'].items() if v>409.5}
            record['original_drawings_preserved']=all(n in z.namelist() and z.read(n)==old.read(n) for n in old.namelist() if n.startswith(('xl/drawings/','xl/media/')))
            record['changed_drawing_parts']=[n for n in old.namelist() if n.startswith(('xl/drawings/','xl/media/')) and (n not in z.namelist() or z.read(n)!=old.read(n))]
            pattern=r'SW-FW-(?:UnitTest|SystemTest|IntegrationTest|SYS-Test|RID)[-A-Za-z0-9]*'
            oldids=set(re.findall(pattern,original['text'])); newids=set(re.findall(pattern,s['text']))
            record['original_ids_missing']=sorted(oldids-newids)
            record['new_ids']=sorted(newids-oldids)
            if record['original_ids_missing']: result['failures'].append(code+' original IDs missing')
            if record['row_height_over_limit']: result['failures'].append(code+' row heights over limit')
    result['files'].append(record)

for criteria,report,end in [('SUVC','SUVR',11),('SITC','SITR',13),('SSTC','SSTR',10)]:
    if criteria not in lookup or report not in lookup: continue
    a,b=sheet(lookup[criteria]),sheet(lookup[report])
    keys=set(a['values'])|set(b['values'])
    differences=[]
    for cell in sorted(keys):
        # Cover the common test area; cover sheets contain report-specific titles.
        if col(cell)<2 or col(cell)>end or int(re.search(r'\d+',cell)[0])<24: continue
        av,bv=a['values'].get(cell,''),b['values'].get(cell,'')
        if av.strip()!=bv.strip(): differences.append({'cell':cell,'criteria':av,'report':bv})
    def common_merges(s):
        return sorted(m for m in s['merges'] if col(m.split(':')[0])>=2 and col(m.split(':')[-1])<=end and int(re.search(r'\d+',m)[0])>=24)
    ma,mb=common_merges(a),common_merges(b)
    pair={'criteria':criteria,'report':report,'differences':differences,'merges_equal':ma==mb,'criteria_merges':len(ma),'report_merges':len(mb)}
    result['pairs'].append(pair)
    if differences: result['failures'].append(criteria+'/'+report+' common cell differences')
    if ma!=mb: result['failures'].append(criteria+'/'+report+' common merge differences')

dest=HERE/'qa/saved-package-checks.json'; dest.parent.mkdir(exist_ok=True)
dest.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'originals':len(result['originals']),'outputs':len(result['files']),'failures':result['failures'],'record':str(dest)},ensure_ascii=False))
