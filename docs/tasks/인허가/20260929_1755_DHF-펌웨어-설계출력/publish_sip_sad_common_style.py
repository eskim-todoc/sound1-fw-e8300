"""Publish only the validated style candidates to their existing filenames."""
from pathlib import Path
import hashlib,json,os,shutil
T=Path(__file__).resolve().parent;ROOT=T.parents[3]
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
names={'sip':'SIP-FW-TD24A 소프트웨어 통합 계획서.docx','sad':'SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx'}
pending=[]
for kind,name in names.items():
 q=T/('qa/'+kind+'-common-style');v=json.loads((q/'verify.json').read_text(encoding='utf8'))
 candidate=q/'candidate.docx';live=OUT/name
 assert sha(live)==sha(q/'before.docx'),'External editing: '+name
 assert sha(candidate)==v['candidate_sha256'],'Candidate changed after verification'
 assert v['content_unchanged'] and v['prior_revisions_preserved']
 assert (q/'visual-check.json').exists(),'Visual review not recorded'
 pending.append((kind,q,candidate,live))
for kind,q,candidate,live in pending:
 before_hash=sha(live)
 tmp=live.with_name(live.name+'.tmp')
 assert not tmp.exists(),'Unrelated temporary file exists'
 shutil.copyfile(candidate,tmp)
 assert sha(live)==before_hash,'External edit while preparing save'
 os.replace(tmp,live)
 assert sha(live)==sha(candidate)
 report={'file':str(live),'before_sha256':before_hash,'saved_sha256':sha(live),'same_filename':True,'matches_verified_candidate':True}
 (q/'publication.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 print(json.dumps(report,ensure_ascii=False))
