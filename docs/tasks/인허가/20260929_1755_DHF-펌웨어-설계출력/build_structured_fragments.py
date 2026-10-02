"""Create semantic unit tables from saved final-view fields, not new test results."""
from pathlib import Path
import json,re,zipfile
from lxml import etree as E
T=Path(__file__).resolve().parent;Q=T/'qa/design-readability/structured-units'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';q=lambda t:'{'+W+'}'+t
meta=json.loads((Q/'prepare.json').read_text(encoding='utf8'));ids=meta['styles']
units=json.loads((Q/'units-before.json').read_text(encoding='utf8'))
folder=Q/'fragments';folder.mkdir(exist_ok=True)
simple={'CFX-014','CFX-020','CM3-019','CM3-023','CM3-024','BOOT-002','BOOT-005','BOOT-009','BOOT-016'}
essential={
 'CFX-016':'AUDIO_INPUT_RSHIFT = 5. 각 signed 입력을 오른쪽으로 5비트 이동한 뒤 혼합한다.',
 'CFX-017':'AUDIO_INPUT_RSHIFT = 5, AUDIO_MIX_NORMALIZE_RSHIFT = 1. 입력을 축소한 뒤 지연 합산값을 2로 나눈다. 블록 길이는 16표본이다.',
 'CFX-011':'mapChangeFlag.cm3Command_mapChange는 CM3의 변경 요청이다. CFX는 반영 후 cfx_Reloaded_MapdataFlag를 1로 설정하고 요청을 0으로 해제한다.',
 'CFX-022':'Addr_SharedMem->maxAudioInput은 현재 16표본 블록의 절대값 최대를 공유한다. 0 입력은 공유값 0을 유지하되 로그 연산 입력만 1로 바꾼다.',
 'BOOT-018':'s_tdc_service_led_iter와 s_tdc_service_led_color는 서비스 표시의 반복 횟수와 현재 색상 상태를 유지한다.',
}
functions={'CFX-002':'normal_init / SYS_HEAR_PAUSE / SYS_HEAR_RESET / SYS_HEAR_FUNCTIONCHAIN_ENABLE / SYS_HEAR_FUNCTIONCHAIN_PRIORITY / SYS_HEAR_START',
 'BOOT-001':'main', 'BOOT-018':'s_tdc_service_led_tick / s_tdc_service_reboot'}
def el(p,t,**a):
    x=E.SubElement(p,q(t))
    for k,v in a.items():x.set(q(k),str(v))
    return x
def para(parent,text,style):
    p=el(parent,'p');pr=el(p,'pPr');el(pr,'pStyle',val=ids[style]);el(pr,'outlineLvl',val=9)
    r=el(p,'r');t=el(r,'t');t.text=text;t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');return p
def caption(parent,title):
    p=para(parent,'표 ','TDC 표 캡션 4')
    for typ in ['begin','separate','end']:
        r=el(p,'r');el(r,'fldChar',fldCharType=typ)
        if typ=='begin':r=el(p,'r');el(r,'instrText').text=' SEQ 표 \\* ARABIC '
        if typ=='separate':r=el(p,'r');el(r,'t').text='1'
    r=el(p,'r');el(r,'t').text='. '+title
def table(parent,rows,heading):
    tbl=el(parent,'tbl');pr=el(tbl,'tblPr');el(pr,'tblStyle',val=ids['TDC 표 4']);el(pr,'tblW',w=8986,type='dxa');el(pr,'jc',val='left');el(pr,'tblInd',w=525,type='dxa');el(pr,'tblLayout',type='fixed')
    grid=el(tbl,'tblGrid');widths=[1304,7682]
    for w in widths:el(grid,'gridCol',w=w)
    for ri,row in enumerate([heading]+rows):
        tr=el(tbl,'tr');rp=el(tr,'trPr');el(rp,'cantSplit')
        if ri==0:el(rp,'tblHeader')
        for ci,(v,w) in enumerate(zip(row,widths)):
            tc=el(tr,'tc');cp=el(tc,'tcPr');el(cp,'tcW',w=w,type='dxa');el(cp,'vAlign',val='center')
            para(tc,v,'TDC 표 머리행' if ri==0 else ('TDC 표 항목' if ci==0 else 'TDC 표 설명'))
manifest=[]
for u in units:
    uid=u['id'];fields={}
    for f in u['fields']:
        k,v=f.split(' :',1);v=v.strip()
        if k.startswith('구현 위치'):continue
        if k.startswith('인터페이스'):
            if v.startswith('src/'):continue
            k='관련 함수'
        fields.setdefault(k,[]).append(v)
    fields={k:' / '.join(v) if k=='관련 함수' else '\n'.join(v) for k,v in fields.items()}
    if uid in functions:fields['관련 함수']=functions[uid]
    if uid in essential:fields['필수 데이터/상수']=essential[uid]
    root=E.Element(q('document'),nsmap={'w':W});body=el(root,'body')
    design=[(k,fields[k]) for k in ['목적','입력','출력','처리'] if k in fields]
    conditions=[(k,fields[k]) for k in ['관련 함수','동작 조건','필수 데이터/상수'] if k in fields]
    if uid in simple:
        for k,v in design+conditions:para(body,k+' : '+v,'TDC 본문 4')
    else:
        caption(body,u['name']+' / 유닛 설계 내용');table(body,design,['항목','설계 내용'])
        if conditions:caption(body,u['name']+' / 구현 조건');table(body,conditions,['구분','구현 조건'])
    if '입출력 예' in fields:
        p=para(body,'입출력 예 : '+fields['입출력 예'],'TDC 주석 4');el(p.find(q('pPr')),'spacing',before=120,after=120)
    para(body,'','TDC 본문 4')
    # InsertXML consumes Word's Flat OPC package, not a naked 2006 w:document.
    PKG='http://schemas.microsoft.com/office/2006/xmlPackage'
    pkg=E.Element('{'+PKG+'}package',nsmap={'pkg':PKG})
    def part(name,ctype,node):
        p=E.SubElement(pkg,'{'+PKG+'}part',{'{'+PKG+'}name':name,'{'+PKG+'}contentType':ctype})
        x=E.SubElement(p,'{'+PKG+'}xmlData');x.append(node)
    R='http://schemas.openxmlformats.org/package/2006/relationships'
    relationships=E.Element('{'+R+'}Relationships',nsmap={None:R})
    E.SubElement(relationships,'{'+R+'}Relationship',Id='rId1',Type='http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument',Target='word/document.xml')
    part('/_rels/.rels','application/vnd.openxmlformats-package.relationships+xml',relationships)
    rel=E.Element('{'+R+'}Relationships',nsmap={None:R})
    E.SubElement(rel,'{'+R+'}Relationship',Id='rIdStyles',Type='http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles',Target='styles.xml')
    part('/word/_rels/document.xml.rels','application/vnd.openxmlformats-package.relationships+xml',rel)
    part('/word/document.xml','application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml',root)
    with zipfile.ZipFile(Q/'styled.docx') as z:stylexml=E.fromstring(z.read('word/styles.xml'))
    part('/word/styles.xml','application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml',stylexml)
    path=folder/(uid+'.xml');path.write_bytes(E.tostring(pkg,xml_declaration=True,encoding='UTF-8',standalone=True))
    manifest.append({'unit':uid,'bookmark':'UnitFields_'+uid.replace('-','_'),'fragment':str(path),'tables':0 if uid in simple else 1+bool(conditions),'fields':fields})
(Q/'fragments.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'units':len(manifest),'prose_units':len(simple),'tables':sum(x['tables'] for x in manifest)},ensure_ascii=False))
