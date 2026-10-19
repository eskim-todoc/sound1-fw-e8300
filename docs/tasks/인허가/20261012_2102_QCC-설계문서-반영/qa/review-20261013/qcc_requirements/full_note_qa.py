import pathlib,json,zipfile,re,hashlib
from lxml import etree as E
R=pathlib.Path(__file__).parent;D=json.loads((R/'inspection.json').read_text(encoding='utf-8'));A=json.loads((R/'srs-authored.json').read_text(encoding='utf-8'));p=pathlib.Path(D['files']['[E8300-QCC] SRS']['path']);ns={'x':'http://schemas.openxmlformats.org/spreadsheetml/2006/main','w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
COLORS={'[QCC 자료 입력]':'C00000','[QCC/E8300 자료 입력]':'7030A0','[E8300 보완]':'0070C0'}
def xruncheck():
 with zipfile.ZipFile(p) as z:
  d=E.fromstring(z.read('xl/worksheets/sheet2.xml'));s=E.fromstring(z.read('xl/styles.xml'));ss=E.fromstring(z.read('xl/sharedStrings.xml')) if 'xl/sharedStrings.xml' in z.namelist() else []
  cells={c.get('r'):c for c in d.xpath('//x:c',namespaces=ns)};fonts=s.find('x:fonts',ns);xfs=s.find('x:cellXfs',ns)
  notes=[]
  for block in A['blocks']:
   stream='';segments=[]
   for n in range(block['rows'][0],block['rows'][1]+1):
    c=cells['F'+str(n)];font=fonts[int(xfs[int(c.get('s','0'))].get('fontId','0'))];base=float(font.find('x:sz',ns).get('val'));item=c.find('x:is',ns) if c.get('t')=='inlineStr' else ss[int(c.find('x:v',ns).text)] if c.get('t')=='s' else c.find('x:is',ns)
    if item is None:continue
    runs=item.findall('x:r',ns) or [item]
    for run in runs:
     t=''.join(run.xpath('./x:t/text()',namespaces=ns));pr=run.find('x:rPr',ns);pr=font if pr is None else pr
     sz=pr.find('x:sz',ns);co=pr.find('x:color',ns);u=pr.find('x:u',ns);bo=pr.find('x:b',ns)
     color=(co.get('rgb','') if co is not None else '').removeprefix('FF');size=float(sz.get('val')) if sz is not None else base
     segments.append({'start':len(stream),'end':len(stream)+len(t),'cell':'F'+str(n),'size':size,'base':base,'color':color,'underline':u is not None and u.get('val')!='none','bold':bo is not None and bo.get('val') not in ['0','false']});stream+=t
   at=stream.find('[+]');start=at+3 if at>=0 else 0
   for m in re.finditer(r'\[(?:QCC 자료 입력|QCC/E8300 자료 입력|E8300 보완)\][^\n]*',stream[start:]):
    lo=start+m.start();hi=start+m.end();prefix=re.match(r'\[[^]]+\]',m[0])[0];over=[r for r in segments if max(lo,r['start'])<min(hi,r['end'])];passed=bool(over) and all(r['size']==r['base']+2 and r['color']==COLORS[prefix] and r['underline'] for r in over)
    notes.append({'rid':block['rid'],'cells':sorted({r['cell'] for r in over}),'text':m[0],'prefix':prefix,'all_characters_correct':passed,'segments':over})
  return {'target':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest().upper(),'current_note_count':len(notes),'all_notes_pass':all(n['all_characters_correct'] for n in notes),'notes':notes}
def wordcheck(label):
 p=pathlib.Path(D['files'][label]['path'])
 with zipfile.ZipFile(p) as z:
  d=E.fromstring(z.read('word/document.xml'));styles=E.fromstring(z.read('word/styles.xml'));notes=[]
  for i,para in enumerate(d.xpath('//w:body//w:p[not(ancestor::w:del or ancestor::w:tr[w:trPr/w:del])]',namespaces=ns)):
   t=''.join(para.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=ns))
   if not t.startswith(tuple(COLORS)):continue
   prefix=next(k for k in COLORS if t.startswith(k));runs=para.xpath('.//w:r[w:t and not(ancestor::w:del)]',namespaces=ns);checks=[]
   for run in runs:
    pr=run.find('w:rPr',ns);sz=pr.find('w:sz',ns) if pr is not None else None;co=pr.find('w:color',ns) if pr is not None else None;u=pr.find('w:u',ns) if pr is not None else None
    checks.append(sz is not None and sz.get('{'+ns['w']+'}val')=='24' and co is not None and co.get('{'+ns['w']+'}val')==COLORS[prefix] and u is not None and u.get('{'+ns['w']+'}val')=='single')
   notes.append({'paragraph_index':i,'text':t,'prefix':prefix,'base_size':10,'size':12,'all_characters_correct':bool(checks) and all(checks)})
  return {'target':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest().upper(),'current_note_count':len(notes),'all_notes_pass':all(n['all_characters_correct'] for n in notes),'notes':notes,'tdc_embedded_count':len(styles.xpath('./w:style[w:name[starts-with(@w:val,"TDC")]]',namespaces=ns))}
result={'SRS':xruncheck(),'SR':wordcheck('[E8300-QCC] SR'),'SGR':wordcheck('[E8300-QCC] SGR')};assert all(v['all_notes_pass'] for v in result.values());(R/'full-note-qa.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({k:{p:v[p] for p in ['sha256','current_note_count','all_notes_pass']} for k,v in result.items()},ensure_ascii=False))
