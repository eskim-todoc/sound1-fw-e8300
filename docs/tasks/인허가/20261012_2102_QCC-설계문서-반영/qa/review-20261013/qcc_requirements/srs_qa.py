import json,pathlib,zipfile,re,hashlib
from lxml import etree as E
ROOT=pathlib.Path(__file__).parent
P=json.loads((ROOT/'requirements-contract.json').read_text(encoding='utf-8'))
A=json.loads((ROOT/'srs-authored.json').read_text(encoding='utf-8'))
D=json.loads((ROOT/'inspection.json').read_text(encoding='utf-8'))
target=pathlib.Path(D['files']['[E8300-QCC] SRS']['path'])
ns={'x':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
with zipfile.ZipFile(target) as z:
 doc=E.fromstring(z.read('xl/worksheets/sheet2.xml'))
 strings=[''.join(x.xpath('.//x:t/text()',namespaces=ns)) for x in E.fromstring(z.read('xl/sharedStrings.xml'))] if 'xl/sharedStrings.xml' in z.namelist() else []
 style=E.fromstring(z.read('xl/styles.xml'))
 cells={x.get('r'):x for x in doc.xpath('//x:c',namespaces=ns)}
 def value(c):
  e=cells.get(c)
  if e is None:return ''
  v=e.find('x:v',ns)
  return ''.join(e.xpath('.//x:t/text()',namespaces=ns)) if e.get('t')=='inlineStr' else strings[int(v.text)] if e.get('t')=='s' and v is not None else v.text if v is not None else ''
 def active(text):return text.split('[+]',1)[-1].strip('\n ') if '[+]' in text else '' if '[-]' in text else text.strip('\n')
 result=[]
 for b in A['blocks']:
  n1,n2=b['rows'];p=next(p for p in P['requirements'] if p['rid']==b['rid'])
  f=active(''.join(value('F'+str(n)) for n in range(n1,n2+1)))
  h=active(value('H'+str(n1)));i=active(value('I'+str(n1)))
  result.append({'rid':b['rid'],'rows':b['rows'],'F_equal':f==p['F'].strip('\n'),'H_equal':h==p['H'].strip('\n'),'I_equal':i==p['I'].strip('\n'),'matrix_base_exact':f.startswith(p['base_text']) if p['matrix_match'] else None,'matrix_actual_rows':[m['row'] for m in p['matrix']],'existing_upper_preserved':p['existing_upper_preserved'],'current_F':f,'current_H':h,'current_I':i})
 notes=[]
 for c,e in cells.items():
  state='old' if '[-]' in value(c) else 'new'
  for run in e.xpath('./x:is/x:r',namespaces=ns):
   text=''.join(run.xpath('./x:t/text()',namespaces=ns))
   if text.lstrip().startswith('[+]'):state='new'
   if text.lstrip().startswith('[-]'):state='old'
   if state=='new' and text.lstrip().startswith(('[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]')):
    prefix=re.match(r'\[[^]]+\]',text.lstrip())[0]
    expected={'[QCC 자료 입력]':'FFC00000','[QCC/E8300 자료 입력]':'FF7030A0','[E8300 보완]':'FF0070C0'}[prefix]
    xf=style.find('x:cellXfs',ns)[int(e.get('s','0'))];font=style.find('x:fonts',ns)[int(xf.get('fontId','0'))]
    base=float(font.find('x:sz',ns).get('val'));pr=run.find('x:rPr',ns)
    sz=pr.find('x:sz',ns);color=pr.find('x:color',ns);u=pr.find('x:u',ns)
    # Unchanged notes may use shared-string rich text instead; those are inspected separately.
    notes.append({'cell':c,'text':text.strip(),'prefix':prefix,'size':float(sz.get('val')),'base':base,'color':color.get('rgb'),'underline':u is not None,'pass':float(sz.get('val'))==base+2 and color.get('rgb')==expected and u is not None})
 protected=[]
 for label in ['[E8300] SRS','[E8300] SR','[E8300] SGR','QCC SRS']:
  if label in D['files']:
   d=D['files'][label];sha=hashlib.sha256(pathlib.Path(d['path']).read_bytes()).hexdigest().upper();protected.append({'label':label,'sha256':sha,'unchanged':sha==d['sha256']})
 out={'target':str(target),'sha256':hashlib.sha256(target.read_bytes()).hexdigest().upper(),'active_count':len(result),'requirements':result,'notes':notes,'protected':protected,'all_active_matches':all(r['F_equal'] and r['H_equal'] and r['I_equal'] for r in result),'no_current_qraw_origin':not any(re.search(r'QCC SRS|CC-RI|RI-?\d',r['current_I']) for r in result),'retired82_not_current':active(''.join(value('F'+str(n)) for n in [A['rowmap']['49']]))=='','matrix_equality_scope':{'matched_requirement_count':sum(p['matrix_match'] for p in P['requirements']),'source_local_count':sum(not p['matrix_match'] for p in P['requirements']),'source_local_reason':'원본 E8300 SRS의 상위 추적 칸이 빈 하위 요구사항이다. 상위 ID를 추정하지 않고 기존 본문을 기준으로 유지했다.'},'native_visual_qa':'대기'}
 assert out['all_active_matches'] and out['no_current_qraw_origin'] and all(p['unchanged'] for p in protected)
 assert all(n['pass'] for n in notes)
 (ROOT/'srs-qa.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps({k:out[k] for k in ['active_count','all_active_matches','no_current_qraw_origin','retired82_not_current','matrix_equality_scope']},ensure_ascii=False))
