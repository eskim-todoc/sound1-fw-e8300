from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from copy import deepcopy
from lxml import etree as E
from datetime import datetime,timezone
import json,hashlib,re,sys
Q=Path(__file__).resolve().parent;T=Q.parent.parent
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';N={'w':W};qn=lambda s:'{'+W+'}'+s
DATE=datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00','Z')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def xml(e):return E.tostring(e,encoding='UTF-8',xml_declaration=True,standalone=True)
def readzip(p):
 with ZipFile(p) as z:return {i:z.read(i) for i in z.namelist()}
def writezip(p,parts):
 with ZipFile(p,'w',ZIP_DEFLATED) as z:
  for k,v in parts.items():z.writestr(k,v)
def txt(p):return ''.join(p.xpath('.//w:t[not(ancestor::w:del or ancestor::w:moveFrom)]/text()',namespaces=N))
def live_runs(p):return p.xpath('.//w:r[w:t and not(ancestor::w:del or ancestor::w:moveFrom or ancestor::w:rPrChange)]',namespaces=N)
def history(doc,excluded=()):
 return {str((E.QName(n).localname,n.get(qn('id')))): {'author':n.get(qn('author')),'date':n.get(qn('date')),'text':''.join(n.xpath('.//w:t/text()|.//w:delText/text()',namespaces=N))} for n in doc.xpath('.//*[@w:author]',namespaces=N) if n.get(qn('id')) not in excluded}
NOTES={
 'SR':{250:'3분 무송수신의 감시/갱신 범위와 제어 명령 미수신 조건의 정합성, QCC 마이크 핀/클록, 연결 상대 및 프로파일/코덱별 송수신과 동시 동작 조건 및 시험 결과는 미확정이다.',260:'QCC 개발 환경의 라이브러리/ADK 버전과 빌드 구성은 미확정이다.',265:'QCC의 실제 메모리 배치, 실행 이미지/데이터 사용량과 부트 이미지 검사 설정은 미확정이다.',307:'오디오 송수신, 연결/인증, 키 관리 및 펌웨어 보호에 대한 위험분석 재평가와 변경 영향의 검토 결과는 미확정이다.'},
 'SGR':{297:'QCC의 실제 ADK/실행 환경, scheduler 구성과 사용 버전은 미확정이다.',301:'QCC 제조사 소프트웨어의 식별/버전, 사용 범위와 검토 근거는 미확정이다.'}}
RED={'SR':[206,237,247,248,249,259,264],'SGR':[145,284,285,296,300]}
def make(item):
 code=item['code'];assert sha(item['source'])==item['source_sha256'];parts=readzip(item['backup']);original=deepcopy(parts)
 doc=E.fromstring(parts['word/document.xml']);before_doc=deepcopy(doc);paras=doc.xpath('//w:body//w:p',namespaces=N)
 nextid=max([int(x) for x in doc.xpath('.//@w:id',namespaces=N) if x.isdigit()]+[0])+100
 newids=[];changes=[]
 def revision(kind):
  nonlocal nextid
  e=E.Element(qn(kind),{qn('id'):str(nextid),qn('author'):'김은수',qn('date'):DATE});newids.append(str(nextid));nextid+=1;return e
 for i,new in NOTES[code].items():
  p=paras[i];old=txt(p);assert old.startswith(('[QCC 자료 입력]','[QCC/E8300 자료 입력]'))
  for r in list(live_runs(p)):
   par=r.getparent();rev=revision('del');par.replace(r,rev);rev.append(r)
   for t in r.findall(qn('t')):t.tag=qn('delText')
  ins=revision('ins');r=E.SubElement(ins,qn('r'));rp=E.SubElement(r,qn('rPr'))
  for tag,val in [('color','000000'),('b','0'),('bCs','0'),('u','none'),('sz','20'),('szCs','20')]:E.SubElement(rp,qn(tag),{qn('val'):val})
  E.SubElement(r,qn('t')).text=new;p.append(ins)
  changes.append({'index':i,'before':old,'after':new})
 # Text revisions preserve the source text in its original older revision wrappers.
 assert history(doc,set(newids))==history(before_doc)
 parts['word/document.xml']=xml(doc)
 from style_patch import apply_style
 parts,styleproof=apply_style(parts,code)
 doc=E.fromstring(parts['word/document.xml']);paras=doc.xpath('//w:body//w:p',namespaces=N)
 red=[]
 for i in RED[code]:
  # Style helper must preserve existing paragraph order and keep blank deletions as revision marks.
  p=paras[i];content=txt(p);assert 'QCC' in content or 'BLE' in content
  span=content
  if code=='SR' and i==237:span=content[:content.index(' IQS323')]
  offset=0
  for run in list(live_runs(p)):
   t=''.join(run.xpath('./w:t/text()',namespaces=N));start=offset;end=offset+len(t);offset=end
   if start>=len(span):continue
   if end>len(span):
    assert len(run.findall(qn('t')))==1
    rest=deepcopy(run);rest.find(qn('t')).text=t[len(span)-start:]
    rest.find(qn('t')).set('{http://www.w3.org/XML/1998/namespace}space','preserve')
    for ch in rest.xpath('./w:rPr/w:rPrChange',namespaces=N):ch.getparent().remove(ch)
    run.find(qn('t')).text=t[:len(span)-start];run.addnext(rest)
   rp=run.find(qn('rPr'))
   if rp is None:rp=E.Element(qn('rPr'));run.insert(0,rp)
   for tag in ['b','bCs','color','u','sz','szCs']:
    for x in rp.findall(qn(tag)):rp.remove(x)
   for tag,val in [('b','1'),('bCs','1'),('color','C00000'),('u','none'),('sz','20'),('szCs','20')]:E.SubElement(rp,qn(tag),{qn('val'):val})
  red.append({'index':i,'text':span,'color':'C00000','bold':True,'size_pt':10})
 assert all(prefix not in txt(doc) for prefix in ['[QCC 자료 입력]','[QCC/E8300 자료 입력]'])
 parts['word/document.xml']=xml(doc)
 candidate=Q/(code+'-candidate.docx');writezip(candidate,parts)
 assert all(parts[k]==v for k,v in original.items() if k.startswith('word/media/') or k.startswith('word/header') or k.startswith('word/footer'))
 # Existing revision identity, dates and textual history remain intact after styling.
 oldh=history(before_doc);nowh=history(doc,set(newids));assert all(nowh.get(k)==v for k,v in oldh.items())
 with ZipFile(candidate) as z:
  for k in z.namelist():
   if k.endswith(('.xml','.rels')):E.fromstring(z.read(k))
 proof={**item,'candidate':str(candidate),'candidate_sha256':sha(candidate),'new_text_revision_ids':newids,'note_changes':changes,'highlighted_paragraphs':red,'style':styleproof,'existing_revision_text_author_date_preserved':True,'source_images_headers_footers_preserved':True,'source_unchanged':sha(item['source'])==item['source_sha256']}
 (Q/(code+'-authored.json')).write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8')
 print(code,'candidate',sha(candidate),'QCC notices',len(changes),'red spans',len(red))
 return proof
if __name__=='__main__':
 baseline=json.loads((Q/'baseline.json').read_text(encoding='utf-8'));docs=[make(d) for d in baseline['docs']]
 (Q/'authored.json').write_text(json.dumps({'docs':docs},ensure_ascii=False,indent=2),encoding='utf-8')
