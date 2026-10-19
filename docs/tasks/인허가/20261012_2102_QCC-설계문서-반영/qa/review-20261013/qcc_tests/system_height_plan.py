from pathlib import Path
from lxml import etree as E
import json,zipfile,math,shutil
from integrate import N,ref,sheetmap,sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
scan=json.loads((H/'system-height-all-final.json').read_text(encoding='utf-8'))
targets=['I128','I130','I132','I144','I146','I151','I155','I188','I194','I197','J218','I237','I260','I278','I280']
records=[x for x in scan['cells'] if x['key']=='SSTC' and x['cell'] in targets]
assert len(records)==len(targets)
affected=set(r for x in records for r in range(ref(x['cell'])[1],ref(x['merge'].split(':')[-1])[1]+1))
books={};heights={}
archive=H/'system-height-before-native';archive.mkdir(exist_ok=True)
for code in ['SUVC-main','SUVR-main','SITC-main','SITR-main','SSTC-main','SSTC-cyber','SSTR-main','SSTR-cyber']:
 src=H.parent/'native-excel'/'tests'/(code+'.pdf');dst=archive/(code+'.pdf')
 if not dst.exists():shutil.copy2(src,dst)
for folder in ['boundary-review-final','system-internal-boundary-final','system-additional-crops','unit-long-boundary-neighbors']:
 dst=archive/folder
 if not dst.exists():shutil.copytree(H/folder,dst)
for key in ['SSTC','SSTR']:
 b=P['books'][key]
 with zipfile.ZipFile(b['path']) as z:data={i.filename:z.read(i.filename) for i in z.infolist()}
 sn=next(s for s in sheetmap(data) if s.startswith(('기준서_','보고서_')));member=sheetmap(data)[sn]
 root=E.fromstring(data[member]);rh={int(r.get('r')):float(r.get('ht','15')) for r in root.findall('x:sheetData/x:row',N)}
 for r in affected:heights[r]=max(heights.get(r,0),rh[r])
 books[key]={'path':b['path'],'sheet':sn,'member':member,'sha256_before':sha(Path(b['path'])),'before_heights':{str(r):rh[r] for r in sorted(affected)},'candidate':str(H/'system-height-candidates'/(key+'-system-height.xlsx'))}
for x in sorted(records,key=lambda r:len(r['row_heights'])):
 rr=[int(r) for r in x['row_heights']];want=math.ceil(max(x['available']+50,x['need_estimate']*1.25+20)*4)/4
 # The actual clipped fields and adjacent expected/observed fields share rows.
 # Their content is unchanged. Extra line space accommodates the native wraps.
 maxadj=max((r['need_estimate'] for r in scan['cells'] if ref(r['cell'])[1] in rr and len(r['row_heights'])==1),default=0)
 for r in rr:
  adj=max((c['need_estimate'] for c in scan['cells'] if ref(c['cell'])[1]==r and len(c['row_heights'])==1),default=0)
  heights[r]=max(heights[r],math.ceil((adj+20)*4)/4)
 lack=max(0,want-sum(heights[r] for r in rr))
 for r in rr:heights[r]=math.ceil((heights[r]+lack/len(rr))*4)/4
assert max(heights.values())<=409.5
for b in books.values():
 b['heights']={str(r):h for r,h in sorted(heights.items())}
 b['remove_break_before_rows']=[218]
 b['preserve_break_before_rows']=[217,219]
(H/'system-height-plan.json').write_text(json.dumps({'books':books,'actual_clipped_cells':targets,'affected_rows':sorted(affected),'native_failure_archive':str(archive),'source_text_results_rtt_fonts_merges_and_printarea_change':False,'max_height':max(heights.values()),'native_visibility':'미실행: height candidate requires actual Excel native render and pixel review'},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'targets':targets,'heights':heights,'max':max(heights.values())},ensure_ascii=True))
