"""Publish two reviewed Word artifacts; preserve all other actual current files."""
from pathlib import Path
from zipfile import ZipFile
from datetime import datetime,timezone
from urllib.parse import quote
import json,hashlib,shutil
from lxml import etree as E
Q=Path(__file__).resolve().parent;TASK=Q.parent.parent;ROOT=TASK.parents[3]
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';N={'w':W}
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def save(p,data):Path(p).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
def active(e):
 if e.tag in ('{'+W+'}del','{'+W+'}moveFrom','{'+W+'}pPrChange','{'+W+'}rPrChange'):return ''
 if e.tag=='{'+W+'}t':return e.text or ''
 return ''.join(active(c) for c in e)
plan=read(Q/'authored.json');baseline=read(Q/'baseline.json');oldmanifest=read(TASK/'evidence/final-output-manifest.json')
assert len(plan['docs'])==2
native={r['code']:r for r in read(Q/'native-after.json')}
content=read(Q/'content-saved-review.json')
assert content['status'].startswith('PASS'),content['status']
for d in plan['docs']:
 code=d['code'];visual=read(Q/(code+'-visual-final-review.json'))
 assert visual['status'].startswith('PASS'),visual['status']
 assert read(Q/(code+'-field-compare.json'))['pass']
 assert sha(d['candidate'])==d['candidate_sha256']==native[code]['sha256']
 assert d['candidate_sha256'] in json.dumps(visual).upper()
 assert d['candidate_sha256'] in json.dumps(content).upper()
 assert native[code]['pages']==11 and native[code]['globalUserUnchanged']
 assert sha(d['source'])==d['source_sha256']==sha(d['backup']), 'Source changed: '+code
 assert not Path(d['output']).exists(),'Completed target already exists'
 with ZipFile(d['candidate']) as z:
  for name in z.namelist():
   if name.endswith(('.xml','.rels')):E.fromstring(z.read(name))
  doc=E.fromstring(z.read('word/document.xml'));current=active(doc)
  assert '[QCC 자료 입력]' not in current and '[QCC/E8300 자료 입력]' not in current
  assert current.count('미확정이다.')==len(d['note_changes'])
  styles=E.fromstring(z.read('word/styles.xml'))
  assert len(styles.xpath('//w:style[w:name[starts-with(@w:val,"TDC ")]]',namespaces=N))==59
  settings=E.fromstring(z.read('word/settings.xml'))
  auto=settings.find('w:linkStyles',N)
  assert auto is None or auto.get('{'+W+'}val') in ('0','false','off')

# Observe every protected/current file immediately before publication. Past
# manifest hashes can predate the user's own edits and are never restored.
originals=read(TASK/'evidence/source-baseline.json')
protected=[{'path':o['path'],'sha256':sha(o['path']),'historical_sha256':o['sha256']} for o in originals]
other=[{'code':f['code'],'path':f['path'],'sha256':sha(f['path'])} for f in baseline['files'] if f['code'] not in ('SR','SGR')]
save(Q/'prepublish-preservation.json',{'protected':protected,'other_current':other})
shutil.copy2(TASK/'evidence/final-output-manifest.json',Q/'manifest-before-publication.json')
for name,path in [('registry',ROOT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'),('task',TASK/'작업.md')]:
 shutil.copy2(path,Q/(name+'-before-publication.md'))
published=[]
for d in plan['docs']:
 src=Path(d['source']);out=Path(d['output']);data=Path(d['candidate']).read_bytes()
 assert sha(src)==d['source_sha256'] and not out.exists()
 # Exclusive creation prevents overwriting a same-named user file. After byte
 # verification remove only the exact backed-up current source, implementing
 # the requested rename without keeping duplicate unfinished files in G.
 with out.open('xb') as f:f.write(data)
 assert sha(out)==d['candidate_sha256']
 assert sha(src)==d['source_sha256'],'Concurrent source edit; keep both copies'
 src.unlink()
 published.append({'code':d['code'],'source':str(src),'path':str(out),'backup':d['backup'],'before_sha256':d['source_sha256'],'sha256':sha(out),'matches_reviewed_candidate':True,'old_name_removed':not src.exists(),'pages':native[d['code']]['pages']})
preserved=[]
for item in protected+other:
 now=sha(item['path']);assert now==item['sha256'], 'Concurrent external file change: '+item['path']
 preserved.append({**item,'after_sha256':now,'unchanged':True})
stamp=datetime.now(timezone.utc).astimezone().isoformat()
save(Q/'published.json',{'status':'PASS_PUBLISHED_TWO_WORD_DOCS','checked_at':stamp,'published':published,'protected_original_count':len(protected),'other_current_count':len(other),'preserved':preserved,'scope':'Only SR/SGR document formatting, six input-request notice replacements and twelve actual QCC bold-red spans; no product test or approval'})

# Retain historical validation metadata in a snapshot. For files changed by
# someone else since the previous record, record only actual observed hashes.
manifest=oldmanifest;entries=[];externally_changed=[]
pub={r['code']:r for r in published};obs={r['code']:r for r in other}
for entry in manifest['files']:
 code=entry['code']
 if code in pub:
  r=pub[code];d=next(x for x in plan['docs'] if x['code']==code)
  entry={'code':code,'path':r['path'],'sha256':r['sha256'],'exists':True,'all_xml_parse':True,'previous_validation_record':str(Q/'manifest-before-publication.json'),'previous_followup_sha256':entry['sha256'],'markers':{'[QCC 자료 입력]':0,'[QCC/E8300 자료 입력]':0,'[E8300 보완]':1 if code=='SR' else 0},'current_unconfirmed_notices':len(d['note_changes']),'actual_QCC_bold_red_spans':len(d['highlighted_paragraphs']),'native_previews':[{'pdf':native[code]['pdf'],'pdf_sha256':sha(native[code]['pdf']),'pages':native[code]['pages'],'source_sha256':r['sha256'],'candidate_matches_published_bytes':True,'checked_at':stamp,'scope':'All final saved pages; visible accepted revision projection, revisions preserved in published DOCX'}],'validation':str(Q/'content-saved-review.json'),'visual_review':str(Q/(code+'-visual-final-review.json'))}
 elif code in obs and entry['sha256']!=obs[code]['sha256']:
  externally_changed.append({'code':code,'previous_record_sha256':entry['sha256'],'observed_sha256':obs[code]['sha256']})
  entry={'code':code,'path':obs[code]['path'],'sha256':obs[code]['sha256'],'exists':True,'validation_scope':'External revision observed and preserved. This Word formatting task did not revalidate its content or render.','previous_validation_record':str(Q/'manifest-before-publication.json'),'previous_validated_sha256':entry['sha256'],'checked_at':stamp}
 entries.append(entry)
manifest['files']=entries
manifest['checked_at']=stamp
manifest['status']='Current artifact index; latest verification applies to SR/SGR Word only'
manifest['prefix_policy']='SR/SGR completed Word files: QCC request prefixes removed by user instruction, actual QCC content bold C00000, unresolved facts retained without prefix. Other documents retain existing prefix rules.'
manifest['input_word_style_followup']={'checked_at':stamp,'scope':['SR','SGR'],'record':str(Q/'published.json'),'local_backups':[d['backup'] for d in plan['docs']],'external_current_hash_changes_preserved':externally_changed,'product_execution':'No product tests or new approval','completion_suffix_meaning':'Requested document formatting and QCC wording cleanup completed'}
save(TASK/'evidence/final-output-manifest.json',manifest)
print(json.dumps({'published':published,'preserved_originals':len(protected),'other_files_preserved':len(other),'external_prior_record_differences':externally_changed},ensure_ascii=False,indent=2))
