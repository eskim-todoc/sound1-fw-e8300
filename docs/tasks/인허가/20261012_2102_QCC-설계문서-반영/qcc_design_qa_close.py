from pathlib import Path
from datetime import datetime, timezone
import json, hashlib
T=Path(__file__).parent
p=T/'qa/cross-design-native.json'
d=json.loads(p.read_text('utf-8'))
proof=json.loads((T/'qa/final-word/SAD-candidate-proof.json').read_text('utf-8-sig'))
assert proof['pass'] and all(proof['checks'].values())
pages=json.loads((T/'qa/final-word/SAD/pages.json').read_text('utf-8-sig'))
assert len(pages)==47
assert 'SDP-FW-TD24A' in pages[3]['text']
assert '운영 검토 완료는 확인되지 않았다' in pages[46]['text']
d['SAD_final_native']={'pages':47,'full_png_pages_viewed':[3,4,6,47],'TOC_cache_coherent':True,'TOC_entries':'Introduction4 / hardware5 / architecture6 / QCC8..10 / trace12 / SOUP46 / validation46 match native header numbers.','blank_page4_removed':'Page3 TOC directly followed by page4 Introduction; no blank intervening page.','SDP_reference':'SDP-FW-TD24A visible in page4 table.','SOUP_scope':'Page47 E8300 identified scope retained; QCC exact product/version/operation review unconfirmed with full red+underline request.','diagram_page6':'Full code-generated 4 modules / 14 units shown; chip/core and cross-core functional bands retained; implementation/measurement unconfirmed visible; no clipping.','root_candidate_proof':{'path':'qa/final-word/SAD-candidate-proof.json','pass':proof['pass'],'checks':proof['checks'],'after_sha256':proof['after_sha256']},'result':'PASS'}
d['SAD_native_revision_collection_limitation']['exhaustive_probe']='Stopped optional exhaustive probe shell after quick isolated comparison established cause and root accepted/rejected XML/figure/revision-text candidate proof passed. No G edit by probe.'
for f in d['findings_resolutions']:
 if f['finding']=='QCC diagram generic unavailable module placeholder':f['resolution']='Code-generated true tracked replacement in all three copies, old bytes/relationships retained. SDDD12/SIP5/SAD6 latest full native PASS.'
 if f['finding']=='SAD blank4 page + wrong SDP ref + SOUP overclaim':f['resolution']='Latest saved SAD47pages native3/4/47 verified: TOC/intro adjacent, TD24A ref correct, SOUP E8300 scope and QCC pending scoped. PASS.'
d['status']='PASS'
d['completed_utc']=datetime.now(timezone.utc).isoformat()
p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf-8')
authored=T/'evidence/qcc_design_authored.json'
a=json.loads(authored.read_text('utf-8'))
a['final_native_QA']={'status':'PASS','evidence':'qa/cross-design-native.json','SAD':47,'SDDD':166,'SIP':16,'tracked_body_accept_reject_preservation':'Root Office field proof plus own targeted reject-c14n proofs; no body acceptance in saved G.','final_saved_hashes':{x['code']:x['current_sha256']for x in d['package_audit']},'all_original_E8300_active_media_bytes_preserved':True,'original_E8300_hashes_unchanged':True,'new_revision_insertions_nested_in_prior_ins_or_del':0,'nested_deletions':'Existing unaccepted insertions matched by baseline author/date/text; legal deletions, not illegal new insertions.'}
authored.write_text(json.dumps(a,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'status':d['status'],'SAD_pages':len(pages),'root_candidate_proof':proof['checks'],'final_hashes':a['final_native_QA']['final_saved_hashes']},ensure_ascii=True))
