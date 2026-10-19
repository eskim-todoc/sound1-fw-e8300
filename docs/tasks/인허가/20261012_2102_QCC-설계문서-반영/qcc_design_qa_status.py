from pathlib import Path
import json
T=Path(__file__).parent
p=T/'evidence/qcc_cross_review_design.json'
d=json.loads(p.read_text('utf-8'))
updates={
 'SEMANTIC-SCOPE-001':'Current G full cell values and all unit native pages independently reviewed. Scope split now explicit; PASS.',
 'SEMANTIC-FRAME-001':'SUVC/SUVR K1735 and SITC/SITR M131/M132 current G values plus v2 four unit/fourteen integration full PNG pages independently reviewed. Logical20/physical21/ACKindex20=2 and actual QCC conversion gap preserved; PASS.',
 'SEMANTIC-SCOPE-002':'SITC/SITR F134 current G value and native v2 page2 both reviewed; QCC support vs E8300 full path explicitly distinguished; PASS.'
}
for f in d['findings']:
 if f['id'] in updates:f.update(status='resolved_independent_saved_and_native_QA',resolution=updates[f['id']])
d['independent_native_evidence']='qa/cross-design-native.json'
p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf-8')
q=json.loads((T/'qa/cross-design-native.json').read_text('utf-8'))
print(json.dumps({'nested_counts':{a['code']:len(a['nested_revisions']) for a in q['package_audit']},'SIP_media':q['package_audit'][2]['original_media_by_hash'],'findings':[[f['id'],f['status']] for f in d['findings']]},ensure_ascii=True))
