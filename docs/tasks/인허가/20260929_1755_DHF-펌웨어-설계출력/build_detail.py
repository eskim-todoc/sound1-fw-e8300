"""Keep reference front matter/styles; adopt the already reviewed Sound1 unit baseline."""
from build_architecture import *
from docx import Document
import posixpath

item=next(v for v in INV if v['name'].startswith('SDDD-'))
src=Path(item['source'])
assert hashlib.sha256(src.read_bytes()).hexdigest()==item['sha256']
candidate=STAGE/item['name'].replace('TD20B','TD24A')
shutil.copy2(src,candidate)
with zipfile.ZipFile(candidate) as z:parts={n:z.read(n) for n in z.namelist()}
root=E.fromstring(parts['word/document.xml']);body=root.find(q('body'))
ps=root.xpath('//w:body//w:p',namespaces=N)
base=ROOT/'docs/참고/인허가/사운드1/[02] SDDD-FW-SND1 소프트웨어 상세 설계서.docx'
with zipfile.ZipFile(base) as z:oldparts={n:z.read(n) for n in z.namelist()}
newroot=E.fromstring(oldparts['word/document.xml']);newbody=newroot.find(q('body'))
source_rels=E.fromstring(oldparts['word/_rels/document.xml.rels'])
rels=E.fromstring(parts['word/_rels/document.xml.rels'])
rmap={r.get('Id'):r for r in source_rels}
ct=E.fromstring(parts['[Content_Types].xml'])
# Original paragraph roles and automatic numbering survive; technical body uses these roles.
roles={}
for p in ps[249:]:
    sty=p.xpath('./w:pPr/w:pStyle/@w:val',namespaces=N)
    if sty and sty[0] in ('10','2','3','4','5') and sty[0] not in roles:roles[sty[0]]=p
plain=ps[230]
start=ps[249];idx=body.index(start)
for el in list(body)[idx:]:
    if el.tag!=q('sectPr'):body.remove(el)
end=body.find(q('sectPr'))
ridseq=1000; imported=[]; active=False
for el in newbody:
    s=txt(el)
    if el.tag==q('p') and re.match(r'^4\.\s+CFX',s):active=True
    if not active:continue
    if el.tag==q('sectPr'):continue
    if re.match(r'^8\.\s',s):break
    if el.tag==q('tbl'):
        table=deepcopy(el)
        for cp in table.xpath('.//w:p',namespaces=N):
            cptext=txt(cp).replace('·','/')
            replacement=newp(plain,cptext)
            cp.getparent().replace(cp,replacement)
        body.insert(body.index(end),table)
        continue
    if el.tag!=q('p'):raise RuntimeError('Unexpected element in technical baseline')
    m=re.match(r'^(\d+(?:\.\d+)*\.)\s+(.*)',s)
    if m:
        level=m.group(1).count('.');style={1:'10',2:'2',3:'3',4:'4',5:'5'}.get(level,'5')
        p=newp(roles[style],m.group(2).replace('·','/'))
    elif el.xpath('.//a:blip',namespaces=N):
        p=deepcopy(el)
        # Preserve diagram shapes but normalize paragraph layout to source figure role.
        pp=p.find(q('pPr'))
        if pp is not None:p.remove(pp)
        pp=E.SubElement(p,q('pPr'));E.SubElement(pp,q('jc')).set(q('val'),'center');p.remove(pp);p.insert(0,pp)
        for blip in p.xpath('.//a:blip',namespaces=N):
            rid=blip.get('{'+N['r']+'}embed');rel=rmap[rid];ridseq+=1;newrid='rIdSound1'+str(ridseq)
            oldtarget=posixpath.normpath('word/'+rel.get('Target'))
            newtarget='media/sound1_detail_'+str(ridseq)+Path(oldtarget).suffix
            parts['word/'+newtarget]=oldparts[oldtarget]
            rr=deepcopy(rel);rr.set('Id',newrid);rr.set('Target',newtarget);rels.append(rr)
            blip.set('{'+N['r']+'}embed',newrid);imported.append(newtarget)
    else:
        if not s and not el.xpath('.//w:br',namespaces=N):continue
        # Retain source body typography. The existing Sound1 purpose/reason fields are substantive content.
        p=newp(plain,s.replace('·','/').replace('SND1-A-EXT','TD24A-A-EXT'))
    body.insert(body.index(end),p)

# Replace QCC reserved body with the verified E8300-facing interface and future insertion slot.
qcchead=next(p for p in body.findall(q('p')) if txt(p)=='블루투스 콤보 칩셋 소프트웨어 상세 설계')
for el in list(body)[body.index(qcchead)+1:]:
    if el.tag!=q('sectPr'):body.remove(el)
settext(qcchead,'QCC 소프트웨어 상세 설계')
for proto,s in [(roles['2'],'E8300 연계'),(plain,'CM3는 SPI로 수신한 리모콘 및 매핑 명령을 처리하고 응답을 송신 버퍼에 준비한다. 전원 정보 명령 0x34의 배터리 잔량, 충전기 연결 상태 및 크래들 뚜껑 상태를 시스템 제어에 반영하고 수신 확인을 응답한다.'),(plain,'LED 표시 명령 0x35는 LED 상태와 E8300 갱신 연결 상태에 반영한다. 오디오 연결 명령 0x36은 연결 상태와 종류를 확인하여 크래들 마이크 여부를 공유 메모리로 CFX에 전달한다.'),(plain,'CFX는 I2S 입력 중 마이크와 외부 오디오를 혼합한다. 크래들 마이크이면 I2S 경로에 설정된 게인을 적용하고 일반 스트리밍이면 기준 게인을 유지한다. 매핑 프로그램 연결 중에는 두 경로 모두 기준 게인을 사용한다.'),(plain,'CM3는 ISD_CHECK 출력으로 내부기 연결 상태를 전달한다. 연결이 확정된 경우 High, 나머지는 Low로 설정한다. QCC_CTRL 출력은 정상 동작 요청 시 High, 종료 요청 또는 그 밖의 값이면 Low로 설정한다.'),(roles['2'],'QCC 내부 설계'),(plain,'QCC 내부 모듈과 유닛은 QCC 펌웨어 자료를 확보한 후 이 절에 추가한다. E8300 연계 항목과 별도로 식별한다.'),(roles['10'],'소프트웨어 상세 설계 검증'),(plain,'소프트웨어 항목과 유닛의 인터페이스, 입력 및 출력, 처리 순서를 검토한다. 유닛 검증 기준서의 식별자와 본 문서의 유닛 식별자가 일치하는지 확인한다.'),(plain,'QCC 내부 설계는 관련 자료 추가 후 별도로 검토한다.')]:body.insert(body.index(end),newp(proto,s))

for i,s in {214:'인공달팽이관 외부장치 Sound1은 E8300의 CFX, HEAR, Cortex-M3와 외부 QCC5181 및 주변장치로 구성된다. 부트로더가 하드웨어와 실행 이미지를 준비하고 각 코어를 기동한다.',225:'QCC와 CM3 : SPI 통신 및 GPIO 제어 핀. QCC와 CFX : I2S 오디오 입력.',242:'블루투스 통신은 QCC와 CM3의 SPI 인터페이스를 통해 명령과 응답을 주고받는다.',244:'외부 블루투스 장치에서 전달된 제어 명령을 QCC가 SPI 경로로 CM3에 전달한다.',245:'QCC는 CM3에서 준비한 응답을 수신하여 외부 장치로 전달한다.',246:'CM3는 수신 명령을 구분하여 리모콘, 매핑 및 시스템 정보 처리를 실행하고 응답을 SPI 버퍼에 준비한다.',195:'SRS-FW-TD24A'}.items():settext(ps[i],s)

# Source front matter metadata, only user-authorized fields.
for table in root.xpath('//w:body/w:tbl',namespaces=N):
    rows=table.findall(q('tr'));t=txt(table)
    if '성함' in t and '서명' in t:
        for row in rows:
            cells=row.findall(q('tc'));label=txt(cells[0]).strip()
            if label in ('성함','직책','일자','서명'):
                for j,c in enumerate(cells[1:]):celltext(c,('김은수' if label=='성함' else '수석연구원') if j==0 and label in ('성함','직책') else '')
    if '최초제정' in t or '최초 제정' in t:
        for ri,row in enumerate(rows[1:]):
            cells=row.findall(q('tc'))
            if len(cells)==4:
                for ci,c in enumerate(cells):celltext(c,['0','최초제정','',''][ci] if ri==0 else '')

# Replace source hardware diagram, retaining original size/position and relationship ID.
byid={r.get('Id'):r for r in rels}
for p in ps[213:249]:
    for blip in p.xpath('.//a:blip',namespaces=N):
        rid=blip.get('{'+N['r']+'}embed');rr=byid[rid]
        target='media/sound1_detail_hardware.png';rr.set('Target',target)
        parts['word/'+target]=(OUT/'이미지 참조/시스템 블록도.png').read_bytes()
for name,data in list(parts.items()):
    if not name.startswith('word/') or not name.endswith('.xml'):continue
    x=root if name=='word/document.xml' else E.fromstring(data)
    for node in x.xpath('//w:t',namespaces=N):node.text=(node.text or '').replace('TD20B-A-EXT/TD23D-A-EXT','TD24A-A-EXT').replace('TD20B','TD24A').replace('TD23D','TD24A')
    if name=='word/document.xml' or E.tostring(x)!=E.tostring(E.fromstring(data)):parts[name]=E.tostring(x,xml_declaration=True,encoding='UTF-8',standalone=True)
parts['word/_rels/document.xml.rels']=E.tostring(rels,xml_declaration=True,encoding='UTF-8',standalone=True)
if not any(x.get('Extension')=='png' for x in ct):E.SubElement(ct,'{http://schemas.openxmlformats.org/package/2006/content-types}Default',Extension='png',ContentType='image/png')
parts['[Content_Types].xml']=E.tostring(ct,xml_declaration=True,encoding='UTF-8',standalone=True)
with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
    for name,data in parts.items():z.writestr(name,data)
ids=re.findall(r'SU-ID-(?:CFX|CM3|BOOT)-\d+',txt(root))
# TOC contains old cached IDs until Word refresh; compare actual technical headings only.
ids=[txt(p).split(',')[0] for p in body.findall(q('p')) if re.match('SU-ID-',txt(p))]
assert len(ids)==325 and len(set(ids))==325,(len(ids),len(set(ids)))
(T/'detail_ids.json').write_text(json.dumps(ids,ensure_ascii=False,indent=2),encoding='utf8')
print('SDDD units',len(ids),'pictures',len(imported),candidate)
