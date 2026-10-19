import pathlib,json,zipfile,copy,hashlib,re,datetime,sys
from lxml import etree as E
ROOT=pathlib.Path(__file__).parent
D=json.loads((ROOT/'inspection.json').read_text(encoding='utf-8'))
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
NS={'w':W,'a':'http://schemas.openxmlformats.org/drawingml/2006/main','wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing','r':R}
q=lambda n:'{'+W+'}'+n
DATE=datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace('+00:00','Z')
AUTHOR='김은수';COLORS={'[QCC 자료 입력]':'C00000','[QCC/E8300 자료 입력]':'7030A0','[E8300 보완]':'0070C0'}
DOTX=pathlib.Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')
FIGURE=ROOT.parent/'qcc_design/figures/시스템 블록도.png'
SR={
207:'- QCC5181 : Bluetooth Classic 오디오 송수신과 BLE 연결/인증을 처리한다. 수신 오디오를 I2S로 E8300에 전달하고 전화 통화 시 DMIC2의 음성을 수음하여 통화 상대에게 송신한다. E8300의 명령/응답 데이터는 SPI로 중계한다. QCC의 내부 구성은 소프트웨어 아키텍쳐 기술서와 상세 설계서에서 정의한다.',
238:'오디오 입력은 DMIC1/DMIC2의 디지털 마이크와 QCC5181에서 전달하는 I2S 오디오를 사용한다. DMIC2는 E8300과 QCC5181에 함께 연결되며, QCC5181은 전화 통화 상향 음성의 수음에 사용한다. IQS323 터치 센서는 사용자 입력을 감지한다.',
248:'QCC5181을 통해 Bluetooth Classic 오디오를 송수신하고, 리모콘 앱과 매핑 프로그램의 제어 명령 및 응답 데이터를 송수신한다.',
249:'QCC의 Bluetooth 수신 오디오는 I2S 24비트, 샘플링 주파수 16kHz로 E8300에 전달한다. DMIC2는 E8300과 QCC에 함께 연결되며, 전화 통화 시 QCC가 DMIC2의 음성을 수음하여 통화 상대에게 송신한다. 통화 상향 마이크 입력과 QCC에서 E8300으로 전달하는 I2S 오디오는 서로 다른 경로이며, Bluetooth Classic 송수신 기능은 SW-FW-RID-69에 따른다.',
251:'[QCC/E8300 자료 입력] DMIC2가 E8300과 QCC에 함께 연결되는 경로는 확인되었다. 3분 무송수신의 감시/갱신 범위와 제어 명령 미수신 조건의 정합성, QCC 마이크 핀/클록, 연결 상대 및 프로파일/코덱별 송수신과 동시 동작 조건 및 시험 결과를 확인한다.',
272:'외부기 소프트웨어의 요구사항은 소프트웨어 요구사항 사양서의 SW-FW-RID-1~81 및 SW-FW-RID-83으로 식별한다. Bluetooth Classic의 수신과 통화 상향 송신은 SW-FW-RID-69에 함께 정의한다.',
280:'위험 통제와 기능/보안 요구사항은 소프트웨어 요구사항 사양서에서 해당 시스템 요구사항과 연결한다. 각 요구의 구현 및 검증 완료는 해당 시험 근거로 판단하며, 미확정 조건과 실제 시험이 남은 항목은 판정을 대기한다.',
284:'무송수신 시간의 감시 대상, 인증 알고리즘/키 역할 및 인터페이스의 상충 조건은 소프트웨어 요구사항 사양서에 표시한다. 조건을 확인한 후 관련 설계와 시험 기준을 함께 조정하고 대조한다.',
288:'I2S 데이터 크기/표본율, 연결 제한 시간/횟수 및 인증 상태를 구체적으로 정의한다. 키 역할, 타이머 시작/갱신 범위와 시간 경계의 허용 조건 및 실제 검증 결과는 추가 확인이 필요하다.',
292:'I2S 조건과 인증 시간/횟수는 시험 기준과 같은 입력 조건에서 비교한다. 타이머 갱신/허용오차, 실제 보호 방식 및 오디오 송수신의 미확정 조건은 확인 전 시험 통과로 판정하지 않는다.',
296:'각 요구는 SW-FW-RID-1~81 및 SW-FW-RID-83으로 유일하게 식별한다. Bluetooth Classic 송수신은 SW-FW-RID-69, 사용 설명서는 SW-FW-RID-83에 정의한다.',
300:'Bluetooth Classic 송수신 요구 SW-FW-RID-69는 SYS-RID-SP-FW-15에 연결한다. 사용 설명서 요구 SW-FW-RID-83은 SYS-RID-ACDOC-17, SYS-RID-ACDOC-18 및 SYS-RID-ACDOC-20의 경고/안전사항, 안전한 작동법 및 동작방법에 연결한다. 다른 요구의 시스템 및 위험 통제 추적은 소프트웨어 요구사항 사양서에서 확인한다.',
301:'[E8300 보완] 제품 사용 설명서에서 경고/안전사항, 안전한 작동법 및 동작방법의 해당 절과 내용, 배포 대상 및 검토 결과를 확인한다.',
307:'오디오 송수신, 연결/인증, 키 관리 및 펌웨어 보호의 위험 통제 영향을 재평가하고 설계 입력/출력 및 시험 기준과 대조한다.',
308:'[QCC/E8300 자료 입력] 오디오 송수신, 연결/인증, 키 관리 및 펌웨어 보호에 대한 위험분석 재평가와 변경 영향의 검토 결과를 확인한다.'}
SGR={283:'√ QCC5181은 Bluetooth Classic 수신 오디오를 I2S로 E8300에 전달하고, 전화 통화 시 DMIC2의 음성을 수음하여 통화 상대에게 송신한다. DMIC2는 E8300과 QCC에 함께 연결되며, 통화 상향 마이크 입력과 I2S 수신 오디오는 서로 다른 경로다. BLE의 리모콘/매핑 명령과 응답은 SPI로 E8300에 중계한다.'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def readzip(p):
 with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def writezip(p,parts):
 with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in parts.items():z.writestr(n,b)
def xml(x):return E.tostring(x,encoding='UTF-8',xml_declaration=True,standalone=True)
def current(x):return ''.join(x.xpath('.//w:t[not(ancestor::w:del or ancestor::w:moveFrom)]/text()',namespaces=NS))
def stale(x):return bool(x.xpath('ancestor::w:del|ancestor::w:moveFrom|ancestor::w:tr[w:trPr/w:del]',namespaces=NS))
def main(label,replacements,start):
 target=pathlib.Path(D['files'][label]['path']);target_start_sha=sha(target)
 source=ROOT.parent/'before'/target.name if '--repair-sr' in sys.argv else target
 before_sha=sha(source);assert before_sha==D['files'][label]['sha256']
 parts=readzip(source);before=copy.deepcopy(parts);doc=E.fromstring(parts['word/document.xml']);styles=E.fromstring(parts['word/styles.xml']);number=E.fromstring(parts['word/numbering.xml']);body=doc.find(q('body'));pars=doc.xpath('//w:body//w:p',namespaces=NS)
 old_records={(x.tag,x.get(q('id')),x.get(q('author')),x.get(q('date'))):''.join(x.xpath('.//w:t/text()|.//w:delText/text()',namespaces=NS)) for x in doc.xpath('//w:ins|//w:del|//w:pPrChange|//w:rPrChange',namespaces=NS)}
 next_id=max([int(v) for v in doc.xpath('//@w:id',namespaces=NS) if v.isdigit()]+[0])+1;newids=[];changes=[];pstyles=[];tables=[]
 def rev(tag):
  nonlocal next_id
  e=E.Element(q(tag));e.set(q('id'),str(next_id));e.set(q('author'),AUTHOR);e.set(q('date'),DATE);newids.append(str(next_id));next_id+=1;return e
 def text_run(text,note=False):
  r=E.Element(q('r'));pr=E.SubElement(r,q('rPr'));E.SubElement(pr,q('rFonts'),{q('ascii'):'맑은 고딕',q('hAnsi'):'맑은 고딕',q('eastAsia'):'맑은 고딕',q('cs'):'맑은 고딕'})
  if note:
   prefix=next(k for k in COLORS if text.startswith(k));E.SubElement(pr,q('color'),{q('val'):COLORS[prefix]});E.SubElement(pr,q('u'),{q('val'):'single'})
  E.SubElement(pr,q('sz'),{q('val'):'24' if note else '20'});E.SubElement(pr,q('szCs'),{q('val'):'24' if note else '20'});t=E.SubElement(r,q('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=text;return r
 def replace(p,text,index=None,drawing=None,caption=False):
  old_text=current(p);inserted=rev('ins')
  runs=p.xpath('.//w:r[not(ancestor::w:del or ancestor::w:moveFrom) and (w:t or w:br or w:tab or w:drawing)]',namespaces=NS)
  for run in runs:
   parent=run.getparent();at=parent.index(run);deleted=rev('del');parent.remove(run);parent.insert(at,deleted);deleted.append(run)
   for t in run.findall(q('t')):t.tag=q('delText')
  if drawing is not None:inserted.append(drawing)
  elif caption:
   m=re.search(r'(그림\s*)(\d+)',text);inserted.append(text_run(text[:m.start(2)]));field=E.SubElement(inserted,q('fldSimple'),{q('instr'):' SEQ 그림 \\* ARABIC '});field.append(text_run(m[2]));inserted.append(text_run(text[m.end(2):]))
  else:inserted.append(text_run(text,text.startswith(tuple(COLORS))))
  p.append(inserted);changes.append({'paragraph_before_index':index,'old_text':old_text,'new_text':text,'kind':'picture' if drawing is not None else 'caption_field' if caption else 'text'})
 for idx,text in replacements.items():replace(pars[idx],text,idx)
 # Replace only the physical system block diagram; preserve old relationship/media.
 if label.endswith(' SR'):
  p=pars[196];runs=p.xpath('.//w:r[w:drawing and not(ancestor::w:del)]',namespaces=NS);assert len(runs)==1
  run=copy.deepcopy(runs[0]);rels=E.fromstring(parts['word/_rels/document.xml.rels']);rel_ns='http://schemas.openxmlformats.org/package/2006/relationships';existing={r.get('Id') for r in rels};rid='rId'+str(max([int(r[3:]) for r in existing if re.fullmatch(r'rId\d+',r)]+[0])+1)
  media='media/qcc_shared_dmic2_system.png';E.SubElement(rels,'{'+rel_ns+'}Relationship',Id=rid,Type=R+'/image',Target=media);parts['word/'+media]=FIGURE.read_bytes();parts['word/_rels/document.xml.rels']=xml(rels)
  for blip in run.xpath('.//a:blip',namespaces=NS):blip.set('{'+R+'}embed',rid)
  from PIL import Image
  width,height=Image.open(FIGURE).size
  for ext in run.xpath('.//wp:extent|.//a:xfrm/a:ext',namespaces=NS):
   cx=int(ext.get('cx'));ext.set('cy',str(round(cx*height/width)))
  ids=doc.xpath('//wp:docPr/@id',namespaces=NS);did=max([int(i) for i in ids if i.isdigit()]+[0])+1
  for pr in run.xpath('.//wp:docPr',namespaces=NS):pr.set('id',str(did));pr.set('name','공유 DMIC2 외부기 시스템 블록도')
  replace(p,'',196,drawing=run)
 # Embed all 59 TDC styles without replacing existing definitions.
 with zipfile.ZipFile(DOTX) as z:template=E.fromstring(z.read('word/styles.xml'))
 tdc=[s for s in template if s.xpath('./w:name[starts-with(@w:val,"TDC")]',namespaces=NS)];assert len(tdc)==59
 style_names={s.get(q('styleId')) for s in styles}
 for st in tdc:
  sid=st.get(q('styleId'));assert sid not in style_names,'TDC ID already present; reconcile rather than overwrite'
  styles.append(copy.deepcopy(st))
 # Existing numId/restarts/lvlText remain. Align original headings and suffix to TDC baseline.
 nums=number.xpath('./w:num[@w:numId="7"]/w:abstractNumId/@w:val',namespaces=NS);assert len(nums)==1
 abstract=number.xpath('./w:abstractNum[@w:abstractNumId=$num]',namespaces=NS,num=nums[0])[0]
 for level in abstract.findall(q('lvl')):
  il=int(level.get(q('ilvl')))
  if il>5:continue
  suffix=level.find(q('suff'))
  if suffix is None:suffix=E.SubElement(level,q('suff'))
  suffix.set(q('val'),'space')
  ppr=level.find(q('pPr'))
  if ppr is None:ppr=E.SubElement(level,q('pPr'))
  for e in list(ppr):
   if e.tag in [q('tabs'),q('ind')]:ppr.remove(e)
  E.SubElement(ppr,q('ind'),{q('left'):str(round(il*.35*567)),q('hanging'):'0'})
 # Identify headings by actual numPr and title role, not the inherited old heading style alone.
 level=1;stylemap={'10':1,'2':2,'3':3,'4':4,'5':5}
 for index,p in enumerate(pars):
  if index<start or stale(p):continue
  oldp=copy.deepcopy(p.find(q('pPr')));ppr=p.find(q('pPr'))
  if ppr is None:ppr=E.Element(q('pPr'));p.insert(0,ppr)
  text=current(p);sid=ppr.xpath('./w:pStyle/@w:val',namespaces=NS);sid=sid[0] if sid else '';hasnum=ppr.find(q('numPr')) is not None
  heading=sid in stylemap and hasnum and not (label.endswith(' SR') and index==271)
  if heading:level=stylemap[sid];newstyle='TDC'+str(level)
  elif p.xpath('ancestor::w:tc',namespaces=NS):newstyle='TDC7'
  elif p.xpath('.//w:drawing[not(ancestor::w:del)]',namespaces=NS):newstyle='TDCLevel그림'+str(level)
  elif re.search(r'^\s*<\s*그림\s*\d+',text):
   newstyle='TDCLevel그림캡션'+str(level)
   if not p.xpath('.//w:instrText[contains(text(),"SEQ")]|.//w:fldSimple[contains(@w:instr,"SEQ")]',namespaces=NS):replace(p,text,index,caption=True)
  else:newstyle='TDC'+str(level)+'0'
  for e in list(ppr):
   if e.tag in [q('pStyle'),q('spacing'),q('ind'),q('tabs'),q('jc'),q('outlineLvl'),q('keepNext'),q('keepLines')]:ppr.remove(e)
   elif e.tag==q('numPr') and not heading:ppr.remove(e)
  ppr.insert(0,E.Element(q('pStyle'),{q('val'):newstyle}))
  if text.startswith(tuple(COLORS)):E.SubElement(ppr,q('keepLines'))
  ch=rev('pPrChange');ch.append(oldp if oldp is not None else E.Element(q('pPr')));ppr.append(ch)
  pstyles.append({'paragraph_before_index':index,'text':text[:120],'style':newstyle,'level':level,'heading':heading})
  # Clear only typography/layout direct conflicts on current runs; preserve meaningful bold/color/underline.
  for r in p.xpath('.//w:r[not(ancestor::w:del or ancestor::w:moveFrom)]',namespaces=NS):
   pr=r.find(q('rPr'))
   if pr is None:continue
   oldpr=copy.deepcopy(pr);note=text.startswith(tuple(COLORS))
   for e in list(pr):
    if e.tag in [q('rFonts'),q('sz'),q('szCs'),q('position'),q('spacing')]:pr.remove(e)
   if note:
    prefix=next(k for k in COLORS if text.startswith(k))
    for e in list(pr):
     if e.tag in [q('color'),q('u')]:pr.remove(e)
    E.SubElement(pr,q('color'),{q('val'):COLORS[prefix]});E.SubElement(pr,q('u'),{q('val'):'single'});E.SubElement(pr,q('sz'),{q('val'):'24'});E.SubElement(pr,q('szCs'),{q('val'):'24'})
   if E.tostring(pr)!=E.tostring(oldpr):ch=rev('rPrChange');ch.append(oldpr);pr.append(ch)
 # Table roles are checked from actual content. The cover/approval/history tables are excluded.
 for ti,tbl in enumerate(body.findall(q('tbl'))):
  if ti<3:continue
  previous=list(body).index(tbl);preceding=body[:previous];tbllevel=1
  for pp in reversed(preceding):
   if pp.tag==q('p'):
    sty=pp.xpath('./w:pPr/w:pStyle/@w:val',namespaces=NS)
    if sty and re.fullmatch('TDC[1-6]',sty[0]):tbllevel=int(sty[0][-1]);break
  pr=tbl.find(q('tblPr'));oldpr=copy.deepcopy(pr)
  if pr is None:pr=E.Element(q('tblPr'));tbl.insert(0,pr)
  for e in list(pr):
   if e.tag in [q('tblStyle'),q('tblInd'),q('jc'),q('tblLook')]:pr.remove(e)
  pr.insert(0,E.Element(q('tblStyle'),{q('val'):'TDCLevel표'+str(tbllevel)}));E.SubElement(pr,q('jc'),{q('val'):'left'});E.SubElement(pr,q('tblInd'),{q('w'):str(round((tbllevel-1)*.35*567)+85),q('type'):'dxa'});E.SubElement(pr,q('tblLook'),{q('val'):'0000',q('firstRow'):'0',q('lastRow'):'0',q('firstColumn'):'0',q('lastColumn'):'0',q('noHBand'):'1',q('noVBand'):'1'})
  ch=rev('tblPrChange');ch.append(oldpr if oldpr is not None else E.Element(q('tblPr')));pr.append(ch)
  rows=tbl.findall(q('tr'));two=len(rows[0].findall(q('tc')))==2
  for ri,row in enumerate(rows):
   trpr=row.find(q('trPr'))
   if trpr is None:trpr=E.Element(q('trPr'));row.insert(0,trpr)
   if ri==0 and trpr.find(q('tblHeader')) is None:E.SubElement(trpr,q('tblHeader'))
   for ci,cell in enumerate(row.findall(q('tc'))):
    header=ri==0;category=two and ci==0
    cp=cell.find(q('tcPr'));oldcp=copy.deepcopy(cp)
    if cp is None:cp=E.Element(q('tcPr'));cell.insert(0,cp)
    for e in list(cp):
     if e.tag in [q('vAlign'),q('shd')]:cp.remove(e)
    E.SubElement(cp,q('vAlign'),{q('val'):'center'})
    if header or category:E.SubElement(cp,q('shd'),{q('val'):'clear',q('color'):'auto',q('fill'):'F5F5F5'})
    ch=rev('tcPrChange');ch.append(oldcp if oldcp is not None else E.Element(q('tcPr')));cp.append(ch)
    for p in cell.findall(q('p')):
     pp=p.find(q('pPr'));st=pp.find(q('pStyle'));st.set(q('val'),'TDC0' if header else 'TDC표항목' if category else 'TDC표설명' if two else 'TDC7')
     if header:E.SubElement(pp,q('keepNext'))
  # Existing normal blank after table is reused; create a tracked normal blank only if absent.
  nextel=tbl.getnext()
  if nextel is None or nextel.tag!=q('p') or current(nextel).strip():
   p=E.Element(q('p'));pp=E.SubElement(p,q('pPr'));E.SubElement(pp,q('pStyle'),{q('val'):'TDC'+str(tbllevel)+'0'});rp=E.SubElement(pp,q('rPr'));rp.append(rev('ins'));tbl.addnext(p)
  tables.append({'body_table_index':ti,'level':tbllevel,'header_role':True,'category_column':two})
 parts['word/document.xml']=xml(doc);parts['word/styles.xml']=xml(styles);parts['word/numbering.xml']=xml(number)
 settings=E.fromstring(parts['word/settings.xml'])
 for e in settings.findall(q('linkStyles')):settings.remove(e)
 if settings.find(q('trackRevisions')) is None:E.SubElement(settings,q('trackRevisions'))
 parts['word/settings.xml']=xml(settings)
 # Readable current text excludes all deletion ancestors and deleted table rows.
 oldbody=E.fromstring(before['word/document.xml']);new_current=[current(p) for p in doc.xpath('//w:body//w:p',namespaces=NS) if not stale(p)]
 assert not any('SW-FW-RID-82' in t or '원문 CC 식별' in t or '정식 상위 요구사항 미등록' in t for t in new_current)
 # Reject this batch only, leaving all older revisions untouched.
 rejected=copy.deepcopy(doc)
 for e in list(rejected.xpath('//w:ins|//w:del|//w:pPrChange|//w:rPrChange|//w:tblPrChange|//w:tcPrChange',namespaces=NS)):
  if e.get(q('id')) not in newids:continue
  parent=e.getparent()
  if parent is None:continue
  if e.tag==q('ins'):parent.remove(e)
  elif e.tag==q('del'):
   at=parent.index(e)
   for child in list(e):
    parent.insert(at,child);at+=1
    for t in child.xpath('.//w:delText[not(ancestor::w:del)]',namespaces=NS):t.tag=q('t')
    # Outer batch deletion is removed before descendant legacy deletions are assessed.
   parent.remove(e)
  else:
   old=e[0];container=parent
   for ch in list(container):container.remove(ch)
   for ch in old:container.append(copy.deepcopy(ch))
 reject_text=[current(p) for p in rejected.xpath('//w:body//w:p',namespaces=NS) if not stale(p)]
 before_text=[current(p) for p in oldbody.xpath('//w:body//w:p',namespaces=NS) if not stale(p)]
 # New tracked paragraph marks create empty rows; exclude them for content equivalence.
 reject_equal=[t for t in reject_text if t.strip()]==[t for t in before_text if t.strip()]
 assert reject_equal,'New-change rejection did not restore before text'
 oldmedia={n:hashlib.sha256(v).hexdigest() for n,v in before.items() if n.startswith('word/media/')}
 assert all(parts[n]==before[n] for n in oldmedia)
 preserved=[]
 for key,text in old_records.items():
  tag,i,author,date=key;match=doc.xpath('//*[local-name()=$tag and @w:id=$i and @w:author=$author and @w:date=$date]',namespaces=NS,tag=E.QName(tag).localname,i=i,author=author or '',date=date or '')
  preserved.append(bool(match) and ''.join(match[0].xpath('.//w:t/text()|.//w:delText/text()',namespaces=NS))==text)
 assert all(preserved),'Older tracked text changed'
 candidate=ROOT/(label.split()[-1]+'-candidate.docx');writezip(candidate,parts)
 # Reopen verifies packaged styles, body and relationship XML.
 with zipfile.ZipFile(candidate) as z:
  for n in z.namelist():
   if n.endswith('.xml') or n.endswith('.rels'):E.fromstring(z.read(n))
 proof={'target':str(target),'before_sha256':before_sha,'candidate_sha256':sha(candidate),'changes':changes,'pstyles':pstyles,'tables':tables,'embedded_tdc_style_count':59,'new_revision_ids':newids,'new_revision_author':AUTHOR,'new_revision_date':DATE,'older_revision_count':len(old_records),'older_revision_text_preserved':all(preserved),'reject_new_text_equals_before':reject_equal,'old_media_hashes':oldmedia,'old_media_preserved':True,'current_paragraphs':new_current,'native_visual_qa':'대기'}
 (ROOT/(label.split()[-1]+'-authored.json')).write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8')
 assert sha(target)==target_start_sha,'Current Word changed, abort'
 if '--write' in sys.argv:writezip(target,parts)
 print(json.dumps({'label':label,'candidate':str(candidate),'sha256':sha(candidate),'text_changes':len(changes),'style_paragraphs':len(pstyles),'rejected_new_equals_before':reject_equal,'new_revisions':len(newids),'saved':'--write' in sys.argv},ensure_ascii=False))
if __name__=='__main__':
 main('[E8300-QCC] SR',SR,150)
 if '--repair-sr' not in sys.argv:main('[E8300-QCC] SGR',SGR,137)
