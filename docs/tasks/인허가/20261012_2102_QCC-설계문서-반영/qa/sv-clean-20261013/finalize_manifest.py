from pathlib import Path
from datetime import datetime
import json,copy,sys
import prepare_clean as p

sys.stdout.reconfigure(encoding='utf-8')
h=Path(__file__).resolve().parent;task=h.parents[1]
source=p.load(h/'source-final-output-manifest.json');manifest=copy.deepcopy(source)
plan=p.load(h/'plan.json');pub=p.load(h/'published.json');renders=p.load(h/'render-checks.json')
positions=p.load(h/'current-positions.json')
assert pub['pass'] and renders['allMainMarkerFree']
published={x['code']:x for x in pub['files']};planned={x['code']:x for x in plan['books']}
for f in manifest['files']:
 if f['code'] not in published:
  assert p.sha(f['path'])==f['sha256'];continue
 b=planned[f['code']];e=published[f['code']]
 assert p.sha(f['path'])==e['sha256']==b['candidateSHA256']
 f['sha256']=e['sha256'];f['previous_followup_sha256']=e['beforeSHA256']
 f['comparison_sheets']=[s['compareName'] for s in b['sheets']]
 f['main_editorial_markers']=0
 f['changes_from_previous_followup']={'editorial_cells_cleaned':sum(not x['colorOnly'] for s in b['sheets'] for x in s['changes']),'color_only_cells':sum(x['colorOnly'] for s in b['sheets'] for x in s['changes']),'content_preserved':True,'comparison_original_rich_text_preserved':True,'native_candidate_matches_published_bytes':True}
 if f['code']=='SRS':f['changes_from_previous_followup']['main_row_layout']={'50':{'height':0,'hidden':True},'62':{'height':15},'68':{'height':15},'81':{'height':15},'purpose':'Remove deleted-only blank pages while keeping cell addresses, all 82 active IDs, formulas and comparison layout'}
 data,_=p.pack(f['path']);tabs={s['name']:s for s in p.sheets(data)}
 texts=[]
 for s in b['sheets']:
  _,cells,ss=p.cellmap(data,tabs[s['name']]['part']);texts.extend(p.value(c,ss) for c in cells.values())
 f['markers']={key:sum(t.count(key) for t in texts) for key in ['[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]']}
 f['native_previews']=[{'sheet':r['sheet'],'range':r['range'],'pdf':r['pdf'],'pdf_sha256':r['pdfSHA256'],'pages':r['pages'],'scope':'Actual Excel read-only candidate preview; published G file has identical SHA256. '+('Representative comparison range; entire source comparison preserved by package checks.' if r['comparison'] else 'Full main print area.'),'source_sha256':r['sourceSHA256']} for r in renders['records'] if r['code']==f['code']]
manifest['status']='SV Excel current sheets cleaned; editorial comparisons preserved in _비교 tabs; document checks passed, product execution and formal approval remain separate'
manifest['checked_at']=datetime.now().astimezone().isoformat()
manifest['scope']='Current 12 E8300-QCC DHF documents; this follow-up changes 7 Excel files and preserves 5 Word documents and 23 protected originals'
manifest['current_location_count']=sum(len(r['entries']) for r in positions)
manifest['SV_followup']={'main_sheets':9,'comparison_sheets_added':9,'books':7,'editorial_marker_cells':1507,'color_only_cells':9,'native_formula_values_checked':164,'current_main_marker_free':True,'policy':'SV means Software Validation; accepted current text in main, editorial history in <original>_비교; expected/observed test comparison remains in main','source_snapshot':str(h/'source-final-output-manifest.json')}
manifest['evidence']={'previous_document_review':source['evidence'],'current_package_and_pair_checks':str(h/'plan.json'),'independent_review':str(h/'independent-review.json'),'native_excel_checks':str(h/'native-checks.json'),'native_render_checks':str(h/'render-checks.json'),'visual_review':str(h/'visual-review.json'),'SV_policy_review':str(h/'policy-review.json'),'current_positions':str(h/'current-positions.json'),'published_hashes_and_protected_files':str(h/'published.json')}
for name in ['final-output-manifest.json']:
 p.save(h/name,manifest);p.save(task/'evidence'/name,manifest)
print(json.dumps({'current_files':len(manifest['files']),'updated_excel':7,'locations':manifest['current_location_count'],'manifest':str(task/'evidence/final-output-manifest.json')},ensure_ascii=False))
