from pathlib import Path
from datetime import datetime
import re
import prepare_clean as p
h=Path(__file__).resolve().parent
r=p.load(h/'render-checks.json');tests=p.load(h/'visual-tests-review.json');native=p.load(h/'native-checks.json')
srs=[x for x in r['records'] if x['code']=='SRS']
assert len(srs)==2 and next(x for x in srs if not x['comparison'])['pages']==26
assert all(t['text'].strip() for x in srs for t in x['text'])
visible='\n'.join(t['text'] for x in srs if not x['comparison'] for t in x['text'])
ids=set(map(int,re.findall(r'SW-FW-RID-(\d+)',visible)))
assert ids==set(range(1,82))|{83},('All 82 active requirement IDs must be visible in native PDF',ids)
out={'pass':True,'checkedAt':datetime.now().astimezone().isoformat(),'scope':'Final Excel display after comparison selection and SRS blank-row repair; actual native rendering, full PDF text scans, SRS contact sheets and specified detailed PNG, independent representative test-document images. Not full visual review of all test-document pages.','nativeFiles':len(native),'mainPDFPages':r['mainPages'],'allMainPDFMarkerFree':r['allMainMarkerFree'],'SRS':{'sourceSHA256':srs[0]['sourceSHA256'],'mainPages':26,'comparisonExcerptPages':4,'blankTextPages':0,'visibleActiveIDs':sorted(ids),'allContactsOpened':[{'path':q,'sha256':p.sha(q)} for x in srs for q in x['contacts']],'detailOpened':str(h/'views/SRS-main-093CE649/page-015.png'),'result':'Current black text and required red/blue/purple underlined notices readable; deleted-only blank pages removed; borders/ID alignment preserved; comparison original red/green retained.'},'testDocumentIndependentReview':{'path':str(h/'visual-tests-review.json'),'sha256':p.sha(h/'visual-tests-review.json'),'reviewer':'qcc_design','selectedPages':61,'contactSheets':16},'repairs':['Comparison cloned tabSelected cleared, existing main/active tab unchanged; no group selected at native open.','SRS deleted-only text rows reduced and obsolete empty requirement row hidden, unchanged active ID/formula/cell addresses.']}
p.save(h/'visual-review.json',out)
print('Visual review recorded; all 82 active SRS IDs visible; blank SRS PDF pages: 0')
