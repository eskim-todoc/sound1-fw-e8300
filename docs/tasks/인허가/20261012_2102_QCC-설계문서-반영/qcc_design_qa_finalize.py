"""Read-only independent package audit; writes only this agent's QA JSON."""
from pathlib import Path
from datetime import datetime, timezone
import json, zipfile, hashlib, posixpath
from lxml import etree as E
T=Path(__file__).parent
B=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main','r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships','a':'http://schemas.openxmlformats.org/drawingml/2006/main'}
W='{'+N['w']+'}'
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def package(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def signature(c):
    text=''.join(c.xpath('.//w:t/text()|.//w:delText/text()',namespaces=N))
    return (c.get(W+'author'),(c.get(W+'date')or'')[:16],text)
def media(p):
    d=E.fromstring(p['word/document.xml']);rel=E.fromstring(p['word/_rels/document.xml.rels'])
    out=[]
    for r in rel:
        if r.get('Type','').endswith('/image'):
            part=posixpath.normpath('word/'+r.get('Target'))
            out.append({'rid':r.get('Id'),'part':part,'sha256':sha(p[part]) if part in p else None,'drawing_references_all':len(d.xpath('//*[@r:embed="'+r.get('Id')+'"]',namespaces=N))})
    return out
expected={'SAD':'7B79A7D3F17AD78E02366FF79E6AD5A9E0FCB03F989AB4C337A2792097BA48B5','SDDD':'C95A3405788121F8DDBDB8F296A49ECD41C28947044635EA9B7B432EB99D289C','SIP':'7EB0438F5AF2BF991BB06BA843D4A6040A38683CD53F6DBFE58F9DDEC2305D02'}
audit=[]
for code in ['SAD','SDDD','SIP']:
    original=next(B.glob('[[]E8300] '+code+'*.docx'));current=next(B.glob('[[]E8300-QCC] '+code+'*.docx'))
    op=package(original);cp=package(current);o=E.fromstring(op['word/document.xml']);c=E.fromstring(cp['word/document.xml'])
    baselines=[('E8300 original',o)]
    for name in [code+'-before-final-targeted.docx',code+'-before-SOUP-clarification.docx']:
        path=T/'qa/qcc_design'/name
        if path.exists():baselines.append((name,E.fromstring(package(path)['word/document.xml'])))
    signatures={name:{signature(n) for n in root.xpath('//w:ins',namespaces=N)}for name,root in baselines}
    nested=[]
    for n in c.xpath('//w:ins[ancestor::w:ins or ancestor::w:del]|//w:del[ancestor::w:ins or ancestor::w:del]',namespaces=N):
        ancestors=n.xpath('ancestor::w:ins|ancestor::w:del',namespaces=N)
        outer=ancestors[-1]
        matches=[name for name,sigs in signatures.items() if signature(outer)in sigs]
        nested.append({'inner_kind':E.QName(n).localname,'inner_id':n.get(W+'id'),'inner_date':n.get(W+'date'),'outer_kind':E.QName(outer).localname,'outer_id':outer.get(W+'id'),'outer_date':outer.get(W+'date'),'outer_signature_matches_baselines':matches,'classification':'deletion inside a pre-existing unaccepted insertion' if n.tag==W+'del' and outer.tag==W+'ins' and matches else 'requires review'})
    om=media(op);cm=media(cp);byhash={m['sha256']:m['part'] for m in cm}
    media_map=[{**m,'current_same_bytes_part':byhash.get(m['sha256']),'classification':'bytes preserved (part name may change)' if m['sha256']in byhash else 'unreferenced original orphan removed by Office' if m['drawing_references_all']==0 else 'active original missing'}for m in om]
    audit.append({'code':code,'path':str(current),'current_sha256':sha(current.read_bytes()),'original_path':str(original),'original_sha256':sha(original.read_bytes()),'original_unchanged':sha(original.read_bytes())==expected[code],'nested_revisions':nested,'nested_insertions_count':sum(n['inner_kind']=='ins' for n in nested),'nested_under_deletion_count':sum(n['outer_kind']=='del' for n in nested),'all_nested_are_baseline_insertion_deletions':all(n['classification']=='deletion inside a pre-existing unaccepted insertion'for n in nested),'revision_id_difference_is_not_new_revision_evidence':'Word field saves renumber revisions; use author/date/content and ancestor baseline signatures.','original_media_by_hash':media_map,'all_original_active_media_preserved':all(m['current_same_bytes_part']for m in media_map if m['drawing_references_all']>0),'current_media':cm})
    assert audit[-1]['original_unchanged']
    assert audit[-1]['nested_insertions_count']==0
    assert audit[-1]['nested_under_deletion_count']==0
    assert audit[-1]['all_nested_are_baseline_insertion_deletions']
    assert audit[-1]['all_original_active_media_preserved']

def coverage(name,full,contacts=True):
    p=T/'qa/excel-native'/name
    pngs=sorted(p.glob('page-*.png'))
    return {'directory':str(p),'page_count':len(pngs),'contact_all_pages_viewed':contacts,'full_png_pages_viewed':full,'result':'PASS','notes':'IDs and merged boundary labels visible; long content within cells; missing-data colors/underline visible; reports remain unexecuted/pending.'}
v=[coverage('SRS-6-94',[3,6,7,8,10,11,12,13,14,15,17,18,20,21]),coverage('SUVC-1686-1773',list(range(8,18))),coverage('SUVR-1686-1773',list(range(6,14))),coverage('SITC-119-145',[2,3,4,5]),coverage('SITR-119-145',[2,3,4]),coverage('SUVC-QCC-final',list(range(1,18))),coverage('SUVR-QCC-final',list(range(1,16))),coverage('SRS-H49-fixed',[1],False),coverage('SITC-final-v2',list(range(1,8)),False),coverage('SITR-final-v2',list(range(1,8)),False),coverage('SUVC-final-v2',[1,2],False),coverage('SUVR-final-v2',[1,2],False)]
for n in ['SUVC-18-20','SUVR-18-20','SITC-18-20','SITR-18-20']:v.append(coverage(n,[1],False))
pages_path=T/'qa/final-word/SDDD/pages.json';pages=json.loads(pages_path.read_text('utf-8-sig'))
assert len(pages)==166
assert [p['page']for p in pages]==list(range(1,167))
current_state={'timestamp_utc':datetime.now(timezone.utc).isoformat(),'reviewer':'qcc_design independent review','scope':'Read-only saved G copies and native Office PDF/PNG; no other agent files modified.','status':'Excel and SDDD/SIP PASS; final SAD refreshed PNG verification pending','excel_native':v,'excel_final_v2_observations':{'integration_all_14_full_pages':'018..024 exactly one complete group on each criterion/report page; labels and all conditions readable.','scope_019':'QCC signal/state support distinguished from E8300 identity/RF/stimulation/notification completion.','security_020_021_024':'Pattern relay is separate from actual CRA; timers/key derivation/vector/NVM gaps preserved.','SPI_unit4_and_integration018':'logical buffer max20 bytes separated from physical length-prefixed SPI21 bytes; E8300 ACK index20=2 distinguished from QCC length field.','result_columns':'All new report conditions unexecuted / actual data unprovided / judgement pending.','SRS_H49':'SYS-RID-SP-HW-11 wraps within H bounds and does not overlap origin I.','SRS82_83':'Red QCC microphone TX design gap and blue E8300 manual upper trace gaps visible in native output.'},'sddd_native':{'pages_json':str(pages_path),'all_166_page_metadata_and_text_parsed':True,'all_14_contact_sheets_all_166_pages_viewed':True,'full_png_pages_viewed':[3,12,13]+list(range(149,167)),'full_QCC_14_unit_tables_readable':True,'new_unit002_support_test4_and_main5':'visible on native page152; coherent with unit criterion group4','new_diagram_page12':'4 modules / 14 units; detailed design8.2; implementation/measurement unconfirmed; retained chip/core/interface boundaries; no clipping','table_continuations':'Repeat column headers present; no clipped text. Some labels/processing paragraphs span adjacent pages and remain fully readable. White space from table flow and chapter-start boundaries is not evidence of omitted text.','TOC':'Native page3 QCC sections148/149/153/156/159/163 and validation165 match body header page numbers; physical cover offset1.','result':'PASS'},'sip_new_native_diagram':{'page':5,'full_png_viewed':True,'4_modules_14_units':True,'E8300_core_function_boundaries_retained':True,'implementation_measurement_unconfirmed_visible':True,'result':'PASS'},'package_audit':audit,'SAD_native_revision_collection_limitation':{'known_id':'pPrChange3424 at TOC final field-end paragraph','xml_duplicate_format_change':False,'diagnostic':'Current item38/39 null, prior targeted backup item38 deletion/item39 paragraph deletion readable. Remove only3424 from diagnostic copy while preserving1pt current format makes38/39 readable. All diagnostic files read-only/native unsaved; G not changed.','proof_files':['qa/qcc_design/SAD-format-track-xml-probe.json','qa/qcc_design/SAD-format-track-native-quick.json'],'completion_validation':'Root QA-copy field save compares accepted/rejected complete body text, ordered image hashes, author+minute-date revision text, and current56 gap notices. See qa/final-word/SAD-candidate-proof.json; PASS stated by root.'},'findings_resolutions':[{'finding':'Unit/integration merged ID labels split at print page boundary','resolution':'15 unit/7 integration group starts manual break by test owner; latest full native pages PASS.'},{'finding':'SRS H49 upper trace overflow','resolution':'WrapText style191; native H49 one-page verification PASS.'},{'finding':'Unit002 missing support test4','resolution':'Tracked sibling insertion in unit002; native152 verified; main test5 retained.'},{'finding':'QCC diagram generic unavailable module placeholder','resolution':'New code-generated diagram true tracked replacement, original media retained. SDDD12/SIP5 latest native full PASS; SAD pending.'},{'finding':'SAD blank4 page + wrong SDP ref + SOUP overclaim','resolution':'TOC tail tracked1pt formatting, D20B→D24A tracked text, scoped E8300/QCC SOUP tracked clarification; root47page final render pending independent visual recheck.'}]}
dest=T/'qa/cross-design-native.json'
if dest.exists():current_state['prior_review_record']=json.loads(dest.read_text('utf-8-sig'))
dest.write_text(json.dumps(current_state,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'written':str(dest),'audit':[{k:a[k]for k in ['code','current_sha256','original_unchanged','nested_insertions_count','nested_under_deletion_count','all_nested_are_baseline_insertion_deletions','all_original_active_media_preserved']}for a in audit],'SDDD_pages':len(pages),'Excel_final_full_pages':18},ensure_ascii=True))
