from pathlib import Path
from lxml import etree as E
import json,zipfile
from integrate import N,ref,sheetmap,vals,sha
H=Path(__file__).resolve().parent;plan=json.loads((H/'system-height-plan.json').read_text(encoding='utf-8'))
packages={};files=[]
for key,b in plan['books'].items():
 path=Path(b['path']);assert sha(path)==b['sha256_before'],(key,'current G hash guard')
 with zipfile.ZipFile(path) as z:infos=z.infolist();original={i.filename:z.read(i.filename) for i in infos}
 data=original.copy();root=E.fromstring(data[b['member']]);oldroot=E.fromstring(original[b['member']]);rows={r.get('r'):r for r in root.findall('x:sheetData/x:row',N)}
 with zipfile.ZipFile(H/(key+'-system-height-authored.xlsx')) as z:stage={i.filename:z.read(i.filename) for i in z.infolist()}
 sr=E.fromstring(stage[sheetmap(stage)[b['sheet']]]);srows={r.get('r'):r for r in sr.findall('x:sheetData/x:row',N)}
 for row,height in b['heights'].items():
  assert float(srows[row].get('ht'))==height,(key,row,'artifact authored height')
  rows[row].set('ht',str(height));rows[row].set('customHeight','1')
 br=root.find('x:rowBreaks',N);before_breaks=[dict(x.attrib) for x in br]
 for row in b['remove_break_before_rows']:
  for x in list(br):
   if int(x.get('id'))==row-1:br.remove(x)
 br.set('count',str(len(br)));br.set('manualBreakCount',str(sum(x.get('man')=='1' for x in br)))
 assert all(any(int(x.get('id'))==r-1 for x in br) for r in b['preserve_break_before_rows'])
 data[b['member']]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
 assert vals(data,b['member'])==vals(original,b['member']),(key,'all body/results/formula values retained')
 for oldrow,newrow in zip(oldroot.findall('x:sheetData/x:row',N),root.findall('x:sheetData/x:row',N)):
  assert [E.tostring(x) for x in oldrow]==[E.tostring(x) for x in newrow],(key,oldrow.get('r'),'all cell body/styles/rich retained')
  if oldrow.get('r') in b['heights']:
   assert {k:v for k,v in oldrow.attrib.items() if k not in ('ht','customHeight')}=={k:v for k,v in newrow.attrib.items() if k not in ('ht','customHeight')}
  else:assert oldrow.attrib==newrow.attrib,(key,'unrelated height')
 for name in ['sheetFormatPr','cols','mergeCells','pageMargins','pageSetup','printOptions']:
  old=oldroot.find('x:'+name,N);new=root.find('x:'+name,N)
  assert (E.tostring(old) if old is not None else None)==(E.tostring(new) if new is not None else None),(key,name)
 assert not root.xpath('//x:r[not(x:t/text())]',namespaces=N)
 assert max(float(r.get('ht','15')) for r in root.findall('x:sheetData/x:row',N))<=409.5
 assert all(data[n]==original[n] for n in data if n!=b['member'])
 dest=Path(b['candidate']);dest.parent.mkdir(exist_ok=True)
 with zipfile.ZipFile(dest,'w') as z:
  for i in infos:z.writestr(i,data[i.filename])
 packages[key]=(data,root)
 files.append({'key':key,'path':b['path'],'candidate':str(dest),'sha256_before':b['sha256_before'],'sha256_candidate':sha(dest),
  'changed_parts':[b['member']],'changed_rows':b['heights'],'before_heights':b['before_heights'],'removed_break_before_rows':[218],
  'new_product_execution_or_pass':False,'all_body_rich_styles_fonts_results_rtt_formulas_merges_widths_printarea_aux_unchanged':True,
  'max_modified_row_height':max(b['heights'].values()),'native_visibility':'미실행: root actual Excel export/pixel recheck required'})
c,r=packages['SSTC'],packages['SSTR'];cv=vals(c[0],plan['books']['SSTC']['member']);rv=vals(r[0],plan['books']['SSTR']['member'])
cc={x.get('r'):x for x in c[1].findall('x:sheetData/x:row/x:c',N)};rc={x.get('r'):x for x in r[1].findall('x:sheetData/x:row/x:c',N)}
for a,v in cv.items():
 col,row=ref(a)
 if 2<=col<=10 and 34<=row<=285:
  assert v==rv.get(a,''),(a,'common values')
  cis=cc[a].find('x:is',N);ris=rc[a].find('x:is',N)
  ct=E.tostring(cis) if cis is not None and cis.find('x:r',N) is not None else None
  rt=E.tostring(ris) if ris is not None and ris.find('x:r',N) is not None else None
  assert ct==rt,(a,'common rich')
assert [dict(x.attrib) for x in c[1].find('x:rowBreaks',N)]==[dict(x.attrib) for x in r[1].find('x:rowBreaks',N)]
cm={x.get('ref') for x in c[1].findall('x:mergeCells/x:mergeCell',N) if 2<=ref(x.get('ref').split(':')[0])[0] and ref(x.get('ref').split(':')[-1])[0]<=10}
rm={x.get('ref') for x in r[1].findall('x:mergeCells/x:mergeCell',N) if 2<=ref(x.get('ref').split(':')[0])[0] and ref(x.get('ref').split(':')[-1])[0]<=10}
assert cm==rm
e={'files':files,'pair_common_values_rich_merges_breaks_modified_heights':'통과','scope':plan['actual_clipped_cells'],
 'failure_archive':plan['native_failure_archive'],'native_display_pass':False,'unmodified_unit_integration_files':True}
(H/'system-height-candidates.json').write_text(json.dumps(e,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'files':[{k:v for k,v in x.items() if k in ('key','candidate','sha256_before','sha256_candidate','max_modified_row_height')} for x in files],'common':'PASS','native':'pending'},ensure_ascii=True))
