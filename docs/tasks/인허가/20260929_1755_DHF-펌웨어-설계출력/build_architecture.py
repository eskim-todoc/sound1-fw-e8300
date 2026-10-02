"""Patch copied source packages; preserve source styles, numbering and untouched parts."""
from pathlib import Path
from copy import deepcopy
import json, re, zipfile, shutil, hashlib
from lxml import etree as E
import openpyxl
from PIL import Image, ImageDraw, ImageFont

T=Path(__file__).resolve().parent
ROOT=T.parents[3]
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
STAGE=T/'revised'
INV=json.loads((T/'inventory.json').read_text(encoding='utf8'))
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N={'w':W,'a':'http://schemas.openxmlformats.org/drawingml/2006/main','r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
def q(n):return '{'+W+'}'+n
def txt(p):return ''.join(p.xpath('.//w:t/text()',namespaces=N))
def settext(p,text):
    runs=p.findall(q('r')); rp=deepcopy(runs[0].find(q('rPr'))) if runs and runs[0].find(q('rPr')) is not None else None
    for c in list(p):
        if c.tag!=q('pPr'):p.remove(c)
    r=E.SubElement(p,q('r'))
    if rp is not None:r.append(rp)
    for i,line in enumerate(text.split('\n')):
        if i:E.SubElement(r,q('br'))
        n=E.SubElement(r,q('t'));n.set('{http://www.w3.org/XML/1998/namespace}space','preserve');n.text=line
def newp(proto,text):
    p=deepcopy(proto);settext(p,text);return p
def celltext(cell,text):
    ps=cell.findall(q('p'));settext(ps[0],text)
    for p in ps[1:]:cell.remove(p)

def diagram(path,kind):
    im=Image.new('RGB',(1600,1050),'white');d=ImageDraw.Draw(im)
    f=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',27)
    small=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',23)
    def box(rect,text):
        d.rectangle(rect,outline='#333333',width=3,fill='#f4f4f4')
        bounds=d.multiline_textbbox((0,0),text,font=f,spacing=12,align='center')
        d.multiline_text(((rect[0]+rect[2]-bounds[2])/2,(rect[1]+rect[3]-bounds[3])/2),text,font=f,fill='black',spacing=12,align='center')
    def arrow(points,label=None,at=None,both=False):
        d.line(points,fill='#333333',width=3)
        import math
        ends=[(points[-2],points[-1])]
        if both:ends.append((points[1],points[0]))
        for p,e in ends:
            a=math.atan2(e[1]-p[1],e[0]-p[0]);d.polygon([e,(e[0]-16*math.cos(a-.4),e[1]-16*math.sin(a-.4)),(e[0]-16*math.cos(a+.4),e[1]-16*math.sin(a+.4))],fill='#333333')
        if label:d.text(at,label,font=small,fill='black')
    if kind=='hardware':
        box((70,70,430,240),'디지털 마이크 2개')
        box((620,70,1120,430),'E8300\nCFX / HEAR / Cortex-M3\n신호처리 및 자극 제어')
        box((70,450,430,700),'QCC5181\n블루투스 통신 / 오디오\n전원 정보 전달')
        box((620,660,1120,810),'FPGA / TX PMIC')
        box((620,900,1120,1020),'코일 / 내부 이식장치')
        box((1200,470,1550,640),'터치 센서 / LED\nEEPROM')
        arrow([(430,140),(620,140)],'PDM',(470,100))
        arrow([(430,495),(520,495),(520,275),(620,275)],'SPI / GPIO',(440,335),True)
        arrow([(430,635),(565,635),(565,380),(620,380)],'I2S',(470,655))
        arrow([(870,430),(870,660)],'PCM / I2C',(895,520))
        arrow([(870,810),(870,900)])
        arrow([(1120,260),(1375,260),(1375,470)],'I2C / GPIO',(1150,290),True)
    else:
        box((50,80,700,300),'CFX\n시스템 제어 / 오디오 입력 및 혼합\n신호처리 / PCM 생성')
        box((930,80,1550,300),'HEAR\n절대값 / 최댓값 / FFT\n함수체인 실행')
        box((50,500,700,760),'Cortex-M3\n시스템 제어 / 자극 파라미터 계산\n내부기 제어 / 블루투스 명령 처리')
        box((930,500,1550,760),'QCC5181\n블루투스 통신 / I2S 오디오\n내부 설계는 별도 자료로 추가')
        box((320,900,1280,1020),'부트로더 / 이미지 적재 / 무선 갱신')
        arrow([(700,170),(930,170)],'기동 / 완료',(725,125),True)
        arrow([(375,300),(375,500)],'공유 메모리 / 인터럽트',(395,390),True)
        arrow([(700,600),(930,600)],'SPI / GPIO',(720,555),True)
        arrow([(1240,500),(1240,395),(580,395),(580,300)],'I2S 오디오',(900,355))
        arrow([(550,900),(550,760)])
        arrow([(320,950),(20,950),(20,190),(50,190)])
    im.save(path)

def requirements():
    p=ROOT/'docs/참고/인허가/사운드1/DHF/02_설계 입력/SW/펌웨어'
    s=openpyxl.load_workbook(next(p.glob('*SRS*.xlsx')),data_only=True)['소프트웨어 입력 요구사항 사양서']
    result=[]
    for row in s:
        if isinstance(row[6].value,str) and re.fullmatch(r'SW-FW-RID-\d+',row[6].value):
            result.append((row[6].value,row[5].value))
    assert len(result)==81
    return result
def modules(n):
    if n in (47,48,49,52,71,72,76):return 'QCC Bluetooth Communication Module'
    if n in (53,81):return '배포 및 변경관리 절차'
    if n==54:return 'Boot Control Module / Image Loading Module'
    if n in (8,9,10,11,12,15,16,58):return 'CFX Signal Processing Module / System Control Module'+(' / QCC 오디오 연계' if n==15 else '')
    if n in (1,2,3,4,5,6,7,13,14,17,18,36,37,39,40,46,56,67,68):return 'CM3 / CFX Internal Stimulation Device Execution Module'
    if n in (24,25,26,27,28,29,30,31,32,33,34,35,38,44,45,59,60,66,69,70):return 'CM3 Bluetooth Communication Module / QCC 연계'
    if n in (50,51,73,74,75,77,78,79,80):return 'System Control Module / 저장 및 보안 처리'
    if n==57:return 'Signal Processing Module / Internal Stimulation Device Execution Module'
    return 'CM3 System Control Module'

def build(kind,changes):
    item=next(v for v in INV if v['name'].startswith(kind+'-'))
    source=Path(item['source']);assert hashlib.sha256(source.read_bytes()).hexdigest()==item['sha256']
    candidate=STAGE/item['name'].replace('TD20B','TD24A');shutil.copy2(source,candidate)
    with zipfile.ZipFile(candidate) as z:parts={n:z.read(n) for n in z.namelist()}
    root=E.fromstring(parts['word/document.xml']);ps=root.xpath('//w:body//w:p',namespaces=N)
    for i,text in changes.items():settext(ps[i],text)
    # Approval cells and revision history use the original grid and styles.
    tables=root.xpath('//w:body/w:tbl',namespaces=N)
    for table in tables:
        rows=table.findall(q('tr'))
        t=txt(table)
        if '성함' in t and '서명' in t:
            for row in rows:
                cells=row.findall(q('tc'))
                if not cells:continue
                label=txt(cells[0]).strip()
                if label in ('성함','직책','일자','서명'):
                    for j,c in enumerate(cells[1:]):celltext(c, ('김은수' if label=='성함' else '수석연구원') if j==0 and label in ('성함','직책') else '')
        if '최초제정' in t or '최초 제정' in t:
            for ri,row in enumerate(rows[1:]):
                cells=row.findall(q('tc'))
                if len(cells)==4:
                    for ci,c in enumerate(cells):celltext(c,['0','최초제정','',''][ci] if ri==0 else '')
    if kind=='SAD':
        # QCC module headings stay in their source positions; independent internals can be inserted later.
        anchor=ps[254];parent=anchor.getparent()
        for proto,text in [(ps[241],'부트로더 소프트웨어 아키텍쳐'),(ps[242],'부트 제어 및 이미지 적재 모듈'),(ps[243],'코어 실행에 필요한 하드웨어와 메모리를 설정하고 실행 이미지를 검사하여 적재한다. 기본 이미지와 대체 이미지의 기동 상태를 관리한다.'),(ps[249],'무선 갱신 모듈'),(ps[252],'QCC를 통해 수신한 갱신 데이터를 처리하고, 이미지 확인 후 다음 기동에 적용한다.')]:parent.insert(parent.index(anchor),newp(proto,text))
        table=next(t for t in tables if 'Software requirements' in txt(t))
        rows=table.findall(q('tr'));prototype=deepcopy(rows[2])
        for row in rows[2:]:table.remove(row)
        for rid,desc in requirements():
            row=deepcopy(prototype)
            for c,value in zip(row.findall(q('tc')),[rid,desc.replace('·','/'),modules(int(rid.rsplit('-',1)[1]))]):celltext(c,value)
            table.append(row)
    if kind=='SIP':
        for anchor,heading,body in [(ps[233],'부트로더 소프트웨어','실행 전 하드웨어 초기화, 이미지 검사 및 적재를 먼저 구현한다. 무선 갱신과 기동 실패 시 복구 처리는 이미지 적재 경로가 동작한 뒤 통합한다.'),(ps[276],'부트로더 소프트웨어 통합','정상 이미지, 손상 이미지 및 대체 이미지 조건에서 검사 결과와 실행 대상을 확인한다. QCC에서 전달되는 갱신 데이터의 수신부터 저장, 재기동 후 적용까지 연결하여 확인한다.')]:
            parent=anchor.getparent();parent.insert(parent.index(anchor),newp(ps[225] if anchor is ps[233] else ps[272],heading));parent.insert(parent.index(anchor),newp(ps[273],body))
    # Only literal product/document names in all text-bearing parts are changed.
    for name,data in list(parts.items()):
        if not name.startswith('word/') or not name.endswith('.xml'):continue
        x=root if name=='word/document.xml' else E.fromstring(data)
        for node in x.xpath('//w:t',namespaces=N):
            value=node.text or ''
            value=value.replace('TD20B-A-EXT/TD23D-A-EXT','TD24A-A-EXT').replace('TD20B','TD24A').replace('TD23D','TD24A')
            node.text=value
        if name=='word/document.xml' or E.tostring(x)!=E.tostring(E.fromstring(data)):
            parts[name]=E.tostring(x,xml_declaration=True,encoding='UTF-8',standalone=True)
    # Replace only the two diagram relationships; logos/signatures are not diagram candidates.
    rels=E.fromstring(parts['word/_rels/document.xml.rels'])
    byid={r.get('Id'):r.get('Target') for r in rels}
    graphic_ps=[p for p in ps if p.xpath('.//a:blip',namespaces=N)]
    diagrams=[]
    for p in graphic_ps:
        i=ps.index(p)
        if i>180:
            diagrams.extend(p.xpath('.//a:blip/@r:embed',namespaces=N))
    for j,rid in enumerate(diagrams):
        target=byid[rid];entry='word/'+target
        if not target.lower().endswith('.png'):
            # Keep relationship ID, change target and add the PNG part/content type.
            newtarget='media/sound1_'+kind+'_'+str(j)+'.png'
            for r in rels:
                if r.get('Id')==rid:r.set('Target',newtarget)
            entry='word/'+newtarget
        pic=OUT/'이미지 참조'/('시스템 블록도.png' if kind=='SAD' and j==0 else '소프트웨어 구조도.png')
        parts[entry]=pic.read_bytes()
    parts['word/_rels/document.xml.rels']=E.tostring(rels,xml_declaration=True,encoding='UTF-8',standalone=True)
    ct=E.fromstring(parts['[Content_Types].xml'])
    if not any(x.get('Extension')=='png' for x in ct):E.SubElement(ct,'{http://schemas.openxmlformats.org/package/2006/content-types}Default',Extension='png',ContentType='image/png')
    parts['[Content_Types].xml']=E.tostring(ct,xml_declaration=True,encoding='UTF-8',standalone=True)
    with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in parts.items():z.writestr(name,data)
    print(candidate)

if __name__=='__main__':
    STAGE.mkdir(exist_ok=True);(OUT/'이미지 참조').mkdir(parents=True,exist_ok=True)
    diagram(OUT/'이미지 참조/시스템 블록도.png','hardware');diagram(OUT/'이미지 참조/소프트웨어 구조도.png','software')
    build('SAD',{
        184:'인공달팽이관 시스템 전체의 하드웨어 플랫폼은 아래 <그림 1.>과 같으며, 외부장치 Sound1의 소프트웨어는 E8300과 QCC5181에서 동작한다.',
        193:'하드웨어 플랫폼을 기반으로 동작하는 소프트웨어 아키텍쳐는 <그림 2.>와 같다.',
        203:'오디오 입출력 설정, 공유 메모리 접근 및 처리 순서와 관련된 유닛들로 구성되어 CFX의 동작을 제어한다.',
        206:'부트로더의 초기화 이후 CFX의 인터럽트와 오디오 경로를 설정한다.',
        210:'두 마이크의 빔포밍 또는 마이크와 I2S 입력의 혼합 신호에 AGC, 주파수 분석 및 비선형 매핑을 적용한다. CIS 또는 N-of-M 방식에 따라 자극할 채널과 크기를 결정한다.',
        225:'CM3에서 사용하는 주변장치와 공유 상태를 초기화한다. 코어 기동 전 하드웨어 설정과 이미지 적재는 부트로더에서 수행한다.',
        239:'QCC에서 SPI로 수신한 명령을 구분하여 리모콘, 매핑 및 시스템 정보 처리를 실행하고 응답을 전송한다. 전원 정보, LED 표시 요청 및 오디오 연결 정보를 받아 시스템 제어와 공유 메모리에 반영한다.',
        241:'QCC용 소프트웨어 아키텍쳐',242:'E8300 연계',
        243:'QCC는 블루투스 명령과 응답을 E8300과 주고받으며 I2S로 오디오를 전달한다. 본 절은 E8300 측 인터페이스를 정의한다.',
        245:'시스템 상태 및 제어 항목',
        246:'E8300은 QCC가 전달한 배터리 잔량, 충전기 연결 및 크래들 뚜껑 상태를 반영한다. 내부기 연결 상태는 ISD_CHECK 핀으로, 동작 모드 제어는 QCC_CTRL 핀으로 전달한다.',
        247:'오디오 연계 항목',
        248:'CFX는 I2S 입력 상태에 따라 마이크와 오디오를 혼합한다. CM3가 수신한 오디오 종류가 크래들 마이크인 경우 크래들용 게인을 적용하며, 일반 스트리밍은 해당 게인을 적용하지 않는다.',
        249:'블루투스 통신 연계',250:'블루투스 명령과 응답은 QCC와 CM3 사이의 SPI 경로로 전달한다.',
        251:'QCC 내부 소프트웨어',252:'QCC 내부 모듈, 항목 및 유닛의 상세 내용은 QCC 펌웨어 자료에 따라 본 항목에 추가한다.',
        475:': 소프트웨어 요구사항 사양서의 81개 요구사항을 추적성 표에 연결한다. QCC 단독 요구사항은 QCC 내부 설계가 추가될 때 해당 항목과의 연결을 확인한다.',
        478:': E8300의 공유 메모리, SPI, I2S 및 제어 핀을 기준으로 모듈과 하드웨어 사이의 연계를 확인한다.',
        481:': 칩셋 제공 라이브러리와 파일 시스템 등 외부 제공 소프트웨어는 사용 버전과 인터페이스를 확인하여 통합한다.'})
    build('SIP',{
        167:'SRS-FW-TD24A',
        206:'부트로더가 하드웨어 초기화와 이미지 적재를 마친 상태에서 CFX의 인터럽트 및 오디오 경로 설정 유닛을 먼저 구현한다.',
        225:'QCC 소프트웨어',227:'E8300 측 SPI 명령 처리, I2S 입력 및 제어 핀 연계를 먼저 구현한다.',
        228:'QCC 내부 시스템 제어 모듈의 구현 순서는 QCC 펌웨어 자료에 따라 본 항목에 추가한다.',
        230:'QCC 내부 블루투스 모듈의 구현 내용은 별도 자료에 따라 추가한다. E8300에서는 리모콘과 매핑 명령의 수신, 처리 및 응답 경로를 구현한다.',
        241:'System Control Module에서 설정하는 파라미터가 신호처리에 적용되는지 확인한다. 두 마이크 빔포밍, I2S 입력 전환 및 CIS/N-of-M 자극 채널 선택을 포함한다.',
        263:'내부기 연결 상태와 QCC에서 수신한 명령에 따라 Bluetooth Communication Module의 처리와 응답을 확인한다. 배터리, 충전기, 크래들 뚜껑 상태 및 LED 표시 요청의 반영을 확인한다.',
        272:'QCC와 E8300 소프트웨어 통합',
        273:'SPI 명령을 전송하여 CM3의 처리와 응답을 확인한다. I2S 오디오를 입력하여 CFX의 마이크 혼합 및 자극 출력을 확인한다. 크래들 마이크와 일반 스트리밍의 게인 구분, 내부기 연결 상태와 QCC 제어 핀의 연계를 확인한다. QCC 내부 모듈 간 통합 항목은 QCC 자료에 따라 별도로 추가한다.'})
