import collections, hashlib, json, pathlib, re, sys, zipfile
from lxml import etree as E
ROOT=pathlib.Path(r'E:\workspace\projects\sound1-fw-e8300-backup\docs\tasks\인허가\20261012_2102_QCC-설계문서-반영')
DEST=pathlib.Path(r'G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF\02_설계 입력\SW\외부기')
PLAN=json.loads((ROOT/'evidence/qcc_requirements_change_plan.json').read_text(encoding='utf8'))
X='http://schemas.openxmlformats.org/spreadsheetml/2006/main'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS={'x':X,'w':W};xn=lambda t:'{'+X+'}'+t;wn=lambda t:'{'+W+'}'+t
PREFIX={'[QCC 자료 입력]':'C00000','[E8300 보완]':'0070C0','[QCC/E8300 자료 입력]':'7030A0'}
def parts(p):
 with zipfile.ZipFile(p) as z:
  assert z.testzip() is None
  return {n:z.read(n) for n in z.namelist()}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def values(p):
 st=[]
 if 'xl/sharedStrings.xml' in p:
  st=[''.join(si.xpath('.//x:t/text()',namespaces=NS)) for si in E.fromstring(p['xl/sharedStrings.xml'])]
 cells={}
 for c in E.fromstring(p['xl/worksheets/sheet2.xml']).xpath('.//x:c',namespaces=NS):
  f=c.find(xn('f'));v=c.find(xn('v'));typ=c.get('t')
  if f is not None:text='='+f.text
  elif typ=='s':text=st[int(v.text)]
  elif typ=='inlineStr':text=''.join(c.xpath('.//x:t/text()',namespaces=NS))
  else:text=v.text if v is not None else ''
  cells[c.get('r')]={'text':text,'el':c}
 return cells
SRS_ONLY='--srs-only' in sys.argv
qa=ROOT/'qa/qcc_requirements/saved-body-validation.json'
if SRS_ONLY:
 result=json.loads(qa.read_text(encoding='utf8'))
 result['srs_missing_locations']=[];result['srs_rid_current_text']=[];result['originals']=[]
 result['word_validation_scope']='pre-root-Office field update; original authoring validation preserved'
else:result={'saved_body_basis':True,'checks':{},'srs_missing_locations':[],'srs_rid_current_text':[],'word_missing_locations':{},'word_current_sections':{},'originals':[]}
orig=next(DEST.glob('[[]E8300] SRS-*.xlsx'));out=next(DEST.glob('[[]E8300-QCC] SRS-*.xlsx'))
old=parts(orig);new=parts(out);cells=values(new);oldcells=values(old)
assert set(old)==set(new),'package member set changed'
changed=[n for n in old if old[n]!=new[n]]
assert set(changed)=={'xl/worksheets/sheet2.xml','xl/workbook.xml','xl/calcChain.xml','xl/comments1.xml','xl/drawings/vmlDrawing1.vml','xl/styles.xml'},changed
result['checks']['srs_package_modified_parts']=changed
oldnote=E.fromstring(old['xl/comments1.xml']).find('.//'+xn('comment'));newnote=E.fromstring(new['xl/comments1.xml']).find('.//'+xn('comment'))
assert oldnote.get('ref')=='B54' and newnote.get('ref')=='B55'
assert oldnote.get('authorId')==newnote.get('authorId') and E.tostring(oldnote.find(xn('text')))==E.tostring(newnote.find(xn('text')))
vns={'v':'urn:schemas-microsoft-com:office:excel'};vml=E.fromstring(new['xl/drawings/vmlDrawing1.vml'])
assert vml.find('.//v:Row',vns).text=='54'
result['checks']['original_note_preserved_at_shifted_B55']=True
for edit in PLAN['cell_edits']:assert cells[edit['cell']]['text']==edit['value'],edit['cell']
rids=[]
for ref,c in cells.items():
 m=re.fullmatch(r'="SW-FW-RID-"&(\d+)',c['text'])
 if not m:continue
 rid=int(m[1]);rids.append(rid);row=re.search(r'\d+$',ref)[0]
 result['srs_rid_current_text'].append({'id':'SW-FW-RID-'+str(rid),'sheet':'소프트웨어 입력 요구사항 사양서','id_cell':ref,'body_cell':'F'+row,'body':cells['F'+row]['text'],'upper_cell':'H'+row,'upper':cells.get('H'+row,{}).get('text','')})
assert sorted(rids)==list(range(1,84)),rids
result['checks']['all_83_ids_unique']=True
oldrids={int(re.fullmatch(r'="SW-FW-RID-"&(\d+)',c['text'])[1]):ref for ref,c in oldcells.items() if re.fullmatch(r'="SW-FW-RID-"&(\d+)',c['text'])}
for rid,ref in oldrids.items():
 row=int(re.search(r'\d+$',ref)[0]);newrow=row+(row>=49)
 assert oldcells.get('H'+str(row),{}).get('text','')==cells.get('H'+str(newrow),{}).get('text',''),('upper changed',rid)
result['checks']['existing_upper_ids_unchanged']=True
s=E.fromstring(new['xl/worksheets/sheet2.xml']);merges=[m.get('ref') for m in s.find(xn('mergeCells'))]
assert len(merges)==len(set(merges))
assert 'B20:B49' in merges and 'C49:E49' in merges
result['checks']['srs_new_row_merge_checked']=True
sheetids=[s.get('sheetId') for s in E.fromstring(new['xl/workbook.xml']).find(xn('sheets'))]
chain=E.fromstring(new['xl/calcChain.xml']);chainrefs=[];sid=None
for c in chain:
 sid=c.get('i',sid)
 if sid==sheetids[1]:chainrefs.append(c.get('r'))
assert len(chainrefs)==len(set(chainrefs)),('duplicate calcchain',chainrefs)
assert all(cells[ref]['el'].find(xn('f')) is not None for ref in chainrefs)
result['checks']['calc_chain_matches_formula_cells']=True
styles=E.fromstring(new['xl/styles.xml']);fonts=styles.find(xn('fonts'));xfs=styles.find(xn('cellXfs'))
original_styles=E.fromstring(old['xl/styles.xml']);original_xfs=original_styles.find(xn('cellXfs'))
assert all(E.tostring(xfs[i])==E.tostring(xf) for i,xf in enumerate(original_xfs))
assert E.tostring(styles.find(xn('fonts')))==E.tostring(original_styles.find(xn('fonts')))
for edit in PLAN['cell_edits']:
 if edit['cell'].startswith('I'):assert xfs[int(cells[edit['cell']]['el'].get('s'))].find(xn('alignment')).get('wrapText')=='1'
result['checks']['source_column_wrap_checked_existing_styles_preserved']=True
for ref,c in cells.items():
 runs=c['el'].xpath('./x:is/x:r',namespaces=NS)
 for r in runs:
  text=''.join(r.xpath('.//x:t/text()',namespaces=NS));prefix=next((p for p in PREFIX if text.lstrip().startswith(p)),None)
  if not prefix:continue
  pr=r.find(xn('rPr'));base=fonts[int(xfs[int(c['el'].get('s','0'))].get('fontId','0'))];bs=base.find(xn('sz'));base_size=float(bs.get('val')) if bs is not None else 10
  color=pr.find(xn('color')).get('rgb');size=float(pr.find(xn('sz')).get('val'));under=pr.find(xn('u')) is not None
  assert color=='FF'+PREFIX[prefix] and size==base_size+2 and under,(ref,color,size,base_size)
  result['srs_missing_locations'].append({'sheet':'소프트웨어 입력 요구사항 사양서','cell':ref,'prefix':prefix,'text':text.strip(),'font_size':size,'base_size':base_size,'color':'#'+PREFIX[prefix],'underline':under})
for key in ([] if SRS_ONLY else ['SR','SGR']):
 orig=next(DEST.glob('[[]E8300] '+key+'-*.docx'));out=next(DEST.glob('[[]E8300-QCC] '+key+'-*.docx'));o=parts(orig);n=parts(out)
 assert set(o)==set(n)
 changed=[k for k in o if o[k]!=n[k]];assert set(changed)<={'word/document.xml','word/settings.xml','docProps/core.xml'},changed
 a=E.fromstring(o['word/document.xml']);b=E.fromstring(n['word/document.xml'])
 oldrevs={int(e.get(wn('id'))):E.tostring(e) for e in a.xpath('//w:ins|//w:del',namespaces=NS)}
 newrevs={int(e.get(wn('id'))):e for e in b.xpath('//w:ins|//w:del',namespaces=NS)}
 for rid,raw in oldrevs.items():assert E.tostring(newrevs[rid])==raw,(key,'existing revision changed',rid)
 added={rid:e for rid,e in newrevs.items() if rid not in oldrevs};assert all(e.get(wn('author'))=='김은수' for e in added.values())
 missing=[];paras=b.xpath('//w:body//w:p',namespaces=NS);current='\n'.join(''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS)) for p in paras)
 for i,p in enumerate(paras,1):
  text=''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS));prefix=next((v for v in PREFIX if text.startswith(v)),None)
  if not prefix:continue
  runs=p.xpath('.//w:r[w:t and not(ancestor::w:del)]',namespaces=NS)
  fmt=[]
  for r in runs:
   pr=r.find(wn('rPr'));color=pr.find(wn('color')).get(wn('val'));size=int(pr.find(wn('sz')).get(wn('val')))/2
   assert color==PREFIX[prefix] and pr.find(wn('u')) is not None
   fmt.append({'font_size':size,'color':'#'+color,'underline':True})
  missing.append({'paragraph_index':i,'prefix':prefix,'text':text,'format':fmt})
 for op,items in PLAN['word_edits'][key].items():
  for index,items2 in items.items():
   texts=items2 if isinstance(items2,list) else [items2]
   for t in texts:assert t in current,(key,index,t)
 result['word_current_sections'][key]=[{'paragraph_index':i,'text':''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))} for i,p in enumerate(paras,1) if (key=='SR' and 148<=i<=210) or any(prefix in ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS)) for prefix in PREFIX)]
 result['word_missing_locations'][key]=missing
 result['checks'][key]={'package_modified_parts':changed,'old_revisions_count':len(oldrevs),'new_revisions_count':len(added),'existing_revision_xml_unchanged':True,'new_author_checked':True,'all_current_plan_text_present':True}
for key in ['SRS','SR','SGR']:
 ext='xlsx' if key=='SRS' else 'docx';orig=next(DEST.glob('[[]E8300] '+key+'-*.'+ext));out=next(DEST.glob('[[]E8300-QCC] '+key+'-*.'+ext))
 assert sha(orig)==PLAN['sha_initial'][key]
 result['originals'].append({'file':str(orig),'sha256':sha(orig),'unchanged':True})
 if not SRS_ONLY or key=='SRS':result['checks'][key+'_saved_sha256']=sha(out)
result['all_checks_passed']=True
qa.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
evidence=ROOT/'evidence/qcc_requirements_authored.json';ev=json.loads(evidence.read_text(encoding='utf8'));ev['saved_body_validation']={'file':str(qa),'all_checks_passed':True,'checks':result['checks']};ev['srs_missing_locations']=result['srs_missing_locations'];ev['word_missing_locations']=result['word_missing_locations'];ev['final_requirements']=result['srs_rid_current_text'];evidence.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf8')
ev['word_authoring_hash_scope']='before root Office field/page-cache update';ev['word_final_office_hash_owner']='root';ev['srs_current_hash_finalized']=True;evidence.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'validation':str(qa),'all_checks_passed':True,'srs_missing_count':len(result['srs_missing_locations']),'word_missing_count':{k:len(v) for k,v in result['word_missing_locations'].items()},'word_scope':'pre-office' if SRS_ONLY else 'current'},ensure_ascii=False))
