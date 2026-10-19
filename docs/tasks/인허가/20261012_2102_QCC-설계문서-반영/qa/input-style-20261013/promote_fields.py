from build_inputs import *
a=json.loads((Q/'authored.json').read_text(encoding='utf-8'));(Q/'authored-before-fields.json').write_text(json.dumps(a,ensure_ascii=False,indent=2),encoding='utf-8')
for item in a['docs']:
 c=item['code'];proof=json.loads((Q/(c+'-field-compare.json')).read_text(encoding='utf-8'));assert proof['pass']
 final=Q/(c+'-fields.docx');item['authored_candidate']=item['candidate'];item['authored_candidate_sha256']=item['candidate_sha256'];item['candidate']=str(final);item['candidate_sha256']=sha(final);item['field_refresh_semantics']=proof['checks']
(Q/'authored.json').write_text(json.dumps(a,ensure_ascii=False,indent=2),encoding='utf-8')
print('Both field-refreshed candidates passed and selected')
