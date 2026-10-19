"""Record the bounded checks actually performed for the RID83 follow-up."""
from pathlib import Path
import json, hashlib, datetime
QA=Path(__file__).resolve().parent
def read(p): return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def save(p,v): Path(p).write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
stamp=datetime.datetime.now().astimezone().isoformat()
test=read(QA/'tests-final-review.json')
assert test['status']=='PASS_SCOPED' and not test['failures']
revisions=read(QA/'word/native-projection-checks.json')
assert all(x['nativeAcceptMatchesCurrent'] and x['nativeRejectMatchesBefore'] for x in revisions)
words=read(QA/'word/render-checks.json')
visual={'checked_at':stamp,'status':'PASS_SCOPED',
  'excel':{'native':str(QA/'native-checks.json'),'full_main_PDF_text_checks':str(QA/'render-checks.json'),
    'all_pages_visually_rechecked':False,
    'SRS_directly_reviewed':['views/SRS-0-main/page-024.png','views/SRS-0-compare/page-001.png'],
    'system_test_directly_reviewed_pages':8,'independent_review':str(QA/'tests-final-review.json'),
    'finding':'Current text and appended comparison history readable; SRS row heights and SSTR comparison K285:K286 height/merge corrected to show entire history'},
  'word':{'full_native_pages_rendered':sum(x['pages'] for x in words),
    'pixel_identical_to_previously_validated_pages':sum(len(x['pixelIdenticalPages']) for x in words),
    'changed_pages_directly_reviewed':{'SR':[13],'SAD':[51,52,53]},
    'finding':'No clipped text, table-width drift, extra pages, or unrelated visual changes in revised current views',
    'render_evidence':str(QA/'word/render-checks.json')},
  'meaning':'Document display QA only; draft manual inspection and final distribution-manual verdict remain separate'}
save(QA/'visual-review.json',visual)
evidence={
1:'RID83 and the unified product wording retained; no parallel QCC requirement created',
2:'Actual matrix/source checked; unjustified ACDOC parent removed; own SW requirement retained; policy clarified',
3:'SRS H93 and TEST83 F285 blank; SR/SAD/cyber references corrected; requirement and test IDs retained',
4:'Current combined documents, source QCC SRS, actual matrix, and manual draft contents read',
6:'SRS correction propagated to SR/SAD and both test documents; current saved bodies cross-checked',
7:'E8300-derived paragraphs, table dimensions, original styles and test-row layout preserved',
9:'Document content is inspected; existing I retained',
10:'Pass evidence defined as actual presence of the three content categories in the identified manual version',
11:'Four explicit steps cover the three categories and recording version/section',
12:'Criteria/report common B:J cells and main merged rows match; no report-only test conditions added',
13:'The same TEST83 row contains the criteria and three actual observations plus section/page references',
15:'Existing verification [1] and three numbered inspection observations preserved in one criterion/report row',
16:'K285 records actual inspected draft ID/revision/date and sections rather than fabricated product measurements',
17:'User confirmed all three categories but explicitly limited current record to the draft; separate distribution manual pending',
18:'Draft content inspection is not promoted to final-manual or security-test passage; L285 pending and other QCC tests unexecuted',
22:'Only remaining distribution-manual deficiency retained with existing E8300 prefix, blue, underline, base+2pt; QCC notices unchanged',
23:'Current saved G files re-collected into the single central QCC registry; no duplicate registries',
24:'Native displays reviewed; comparison clipping fixed by comparison-only heights/merge; main alignment/borders/rows preserved',
25:'Main sheets contain accepted current text only; red[-]/green[+] added only to existing _비교 history',
26:'Full-history candidate retains real new revisions and old history; XML new-only rejection exactly restores prior paragraph. Actual Word accepts current content; QA projection separately verifies native acceptance/rejection of new revisions. Direct full-history COM selective rejection was not verified and is not claimed',
27:'Five combined outputs only; 7 other combined files and 23 originals hash-preserved; no approval/signature/user-setting mutation',
28:'Published bytes equal reviewed candidates; package, native display, active-content and common-row checks linked',
29:'Existing Word paragraph/run styles and table dimensions preserved; author 김은수; no global Word user setting changes'}
na={5:'No firmware structure or architecture-image change in this follow-up',8:'No detailed implementation design changed',
14:'No RTT output or logs changed',19:'No prior numeric test-value convention used or extended',20:'No expected diagram or measured photo changed',21:'No time test input or approval date changed'}
policy={'checked_at':stamp,'status':'PASS_SCOPED','scope':'RID83 correction and draft manual inspection record only',
 'checks':[{'principle':i,'applies':i in evidence,'verdict':'통과' if i in evidence else '해당없음',
            'evidence':evidence.get(i) or na[i]} for i in range(1,30)],
 'evidence':{name:{'path':str(QA/name),'sha256':sha(QA/name)} for name in [
    'requirements-audit.json','user-confirmation.json','qcc-design-manual-audit.json',
    'package-checks.json','tests-final-review.json','native-checks.json','word/native-checks.json',
    'word/native-projection-checks.json','visual-review.json']},
 'product_verdict':'Draft inspection observed; separate actual distribution manual and final TEST83 verdict pending'}
save(QA/'policy-review.json',policy)
print('Bounded visual and 29-principle review recorded.')
