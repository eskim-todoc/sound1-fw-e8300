"""Adapt the retained TD20B source, preserving its native styles and field roles."""
from pathlib import Path
from copy import deepcopy
from collections import OrderedDict
import json,re,zipfile,hashlib,shutil,math
from lxml import etree as E
from PIL import Image,ImageDraw,ImageFont
from design_examples import EXAMPLES
from unit_flowcharts import FLOWS

T=Path(__file__).resolve().parent
ROOT=T.parents[3]; QA=T/'qa/unit-reconciliation'
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
MODEL=json.loads((QA/'model.json').read_text(encoding='utf8'))
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
M='http://schemas.openxmlformats.org/officeDocument/2006/math'
N={'w':W,'m':M,'a':'http://schemas.openxmlformats.org/drawingml/2006/main',
 'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships','wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'}
q=lambda n:'{'+W+'}'+n
txt=lambda p:''.join(p.xpath('.//w:t/text()',namespaces=N))
source=QA/'SDDD-source.docx'
assert hashlib.sha256(source.read_bytes()).hexdigest()=='7b9b14f52a27b12e7a3120702b6eafdd269424670159afb62c4231cd176dbae4'
candidate=QA/'SDDD-candidate.docx';shutil.copy2(source,candidate)
with zipfile.ZipFile(candidate) as z:parts={n:z.read(n) for n in z.namelist()}
r=E.fromstring(parts['word/document.xml']);body=r.find(q('body'))
ps=r.xpath('//w:body//w:p',namespaces=N)
roles={}
for p in ps[249:]:
    sty=p.xpath('./w:pPr/w:pStyle/@w:val',namespaces=N)
    if sty and sty[0] in ('10','2','3','4','5') and sty[0] not in roles:roles[sty[0]]=deepcopy(p)
plain=deepcopy(ps[230]); equation=deepcopy(ps[839]); generic_agc=deepcopy(ps[790])
for el in list(body)[body.index(ps[249]):]:
    if el.tag!=q('sectPr'):body.remove(el)
end=body.find(q('sectPr'))
rels=E.fromstring(parts['word/_rels/document.xml.rels'])
byid={x.get('Id'):x for x in rels}

def settext(p,s):
    runs=p.findall(q('r')); rp=deepcopy(runs[0].find(q('rPr'))) if runs and runs[0].find(q('rPr')) is not None else None
    for c in list(p):
        if c.tag!=q('pPr'):p.remove(c)
    run=E.SubElement(p,q('r'))
    if rp is not None:run.append(rp)
    for i,line in enumerate(s.split('\n')):
        if i:E.SubElement(run,q('br'))
        t=E.SubElement(run,q('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=line
    return p

def prop(p,name,**attrs):
    pp=p.find(q('pPr'))
    if pp is None:pp=E.Element(q('pPr'));p.insert(0,pp)
    x=pp.find(q(name))
    if x is None:x=E.SubElement(pp,q(name))
    for k,v in attrs.items():x.set(q(k),str(v))
    return x

def add(s='',level=None,bold=False):
    p=deepcopy(roles[str(level)] if level else plain)
    settext(p,s)
    for key in list(p.attrib):del p.attrib[key]
    if not level:
        pp=p.find(q('pPr'))
        for node in list(pp):
            if node.tag in (q('numPr'),q('pageBreakBefore'),q('keepNext'),q('ind')):pp.remove(node)
        prop(p,'ind',left=600)
        prop(p,'spacing',after=60,line=264,lineRule='auto')
        if bold:
            rp=p.find('.//'+q('rPr'))
            if rp is None:rp=E.SubElement(p.find(q('r')),q('rPr'))
            E.SubElement(rp,q('b'))
    else:
        prop(p,'keepNext')
    body.insert(body.index(end),p);return p

def copy_para(i):
    p=deepcopy(ps[i]);body.insert(body.index(end),p);return p

def math_line(s):
    p=add();prop(p,'jc',val='center')
    for child in list(p):
        if child.tag!=q('pPr'):p.remove(child)
    om=E.SubElement(E.SubElement(p,'{'+M+'}oMathPara'),'{'+M+'}oMath')
    mr=E.SubElement(om,'{'+M+'}r');E.SubElement(mr,'{'+M+'}t').text=s
    return p

tables=0
def table(title,headers,rows,widths):
    global tables
    tables+=1
    cap=add(f'표 {tables}. {title}');prop(cap,'keepNext')
    tbl=E.Element(q('tbl'));pr=E.SubElement(tbl,q('tblPr'))
    E.SubElement(pr,q('tblW'),{q('w'):str(sum(widths)),q('type'):'dxa'})
    E.SubElement(pr,q('tblLayout'),{q('type'):'fixed'})
    E.SubElement(pr,q('tblInd'),{q('w'):'400',q('type'):'dxa'})
    borders=E.SubElement(pr,q('tblBorders'))
    for edge in ['top','left','bottom','right','insideH','insideV']:E.SubElement(borders,q(edge),{q('val'):'single',q('sz'):'4',q('color'):'808080'})
    mar=E.SubElement(pr,q('tblCellMar'))
    for k,v in [('top',65),('bottom',65),('left',85),('right',85)]:E.SubElement(mar,q(k),{q('w'):str(v),q('type'):'dxa'})
    grid=E.SubElement(tbl,q('tblGrid'))
    for width in widths:E.SubElement(grid,q('gridCol'),{q('w'):str(width)})
    for ri,row in enumerate([headers]+rows):
        tr=E.SubElement(tbl,q('tr'));trpr=E.SubElement(tr,q('trPr'));E.SubElement(trpr,q('cantSplit'))
        if ri==0:E.SubElement(trpr,q('tblHeader'))
        for j,value in enumerate(row):
            cell=E.SubElement(tr,q('tc'));tcpr=E.SubElement(cell,q('tcPr'))
            E.SubElement(tcpr,q('tcW'),{q('w'):str(widths[j]),q('type'):'dxa'})
            E.SubElement(tcpr,q('vAlign'),{q('val'):'center'})
            if ri==0:E.SubElement(tcpr,q('shd'),{q('fill'):'E7E6E6'})
            p=E.SubElement(cell,q('p'));prop(p,'spacing',before=0,after=0,line=240,lineRule='auto');prop(p,'ind',left=0,right=0,firstLine=0)
            run=E.SubElement(p,q('r'));rp=E.SubElement(run,q('rPr'));E.SubElement(rp,q('sz'),{q('val'):'20'})
            if ri==0:E.SubElement(rp,q('b'))
            for k,line in enumerate(str(value).split('\n')):
                if k:E.SubElement(run,q('br'))
                E.SubElement(run,q('t')).text=line
    body.insert(body.index(end),tbl);add()

# Front matter is minimally updated in the source, including split-run product names.
changes={175:'본 문서는 TD24A-A-EXT 제품의 펌웨어를 구현하기 위한 소프트웨어의 유닛을 설계하고 기술한다.',
181:'TD24A-A-EXT 제품의 펌웨어 개발 프로젝트에 참여하는 모든 구성원은 이 문서를 참고하도록 한다.',
195:'SRS-FW-TD24A',214:'인공달팽이관 외부장치 Sound1은 E8300의 CFX, HEAR, Cortex-M3와 외부 QCC5181 및 주변장치로 구성된다. 부트로더가 하드웨어와 실행 이미지를 준비하고 각 코어를 기동한다.',
225:'QCC와 CM3 : SPI 통신 및 GPIO 제어 핀. QCC와 CFX : I2S 오디오 입력.',
230:'신호처리 모듈은 CM3, CFX와 HEAR에서 역할을 나누어 실행하며 코어 간 파라미터는 공유 메모리로 전달한다.',
232:'CM3는 맵의 T/C 전류와 펄스폭으로 내부기 파라미터를 계산한다. CFX는 전달된 디지털 T/C와 오디오 설정으로 매핑 계수를 계산하고, HEAR의 벡터/주파수 분석 결과로 자극 크기를 만든다.',
237:'CM3는 I2C를 사용하여 FPGA와 통신하고, 내부기 실행에 필요한 명령을 공유 메모리로 CFX에 전달한다.',
242:'블루투스 통신은 QCC와 CM3의 SPI 인터페이스를 통해 명령과 응답을 주고받는다.',
244:'외부 블루투스 장치에서 전달된 제어 명령을 QCC가 SPI 경로로 CM3에 전달한다.',
245:'QCC는 CM3에서 준비한 응답을 수신하여 외부 장치로 전달한다.',
246:'CM3는 수신 명령을 구분하여 리모콘, 매핑 및 시스템 정보 처리를 실행하고 응답을 SPI 버퍼에 준비한다.'}
for i,s in changes.items():settext(ps[i],s)
for tbl in body.findall(q('tbl')):
    rows=tbl.findall(q('tr'));text=txt(tbl)
    if '성함' in text and '서명' in text:
        for row in rows:
            cells=row.findall(q('tc'));label=txt(cells[0]).strip()
            if label in ['성함','직책','일자','서명']:
                for j,cell in enumerate(cells[1:]):
                    settext(cell.find(q('p')),('김은수' if label=='성함' else '수석연구원') if j==0 and label in ['성함','직책'] else '')
                    for p in cell.findall(q('p'))[1:]:cell.remove(p)
    if '최초제정' in text or '최초 제정' in text:
        for ri,row in enumerate(rows[1:]):
            cells=row.findall(q('tc'))
            if len(cells)==4:
                for ci,cell in enumerate(cells):settext(cell.find(q('p')),['0','최초제정','',''][ci] if ri==0 else '')
image_names=['시스템 블록도.png','SDDD 시스템 제어 연계.png','SDDD 신호처리 연계.png','SDDD 내부기 실행 연계.png','SDDD 블루투스 연계.png']
for i,name in zip([216,223,233,240,243],image_names):
    p=ps[i];blip=p.xpath('.//a:blip',namespaces=N)[0];rid=blip.get('{'+N['r']+'}embed')
    target='media/units_'+str(i)+'.png';byid[rid].set('Target',target);parts['word/'+target]=(OUT/'이미지 참조'/name).read_bytes()
    im=Image.open(OUT/'이미지 참조'/name);cx=5800000;cy=round(cx*im.height/im.width)
    for extent in p.xpath('.//wp:extent|.//a:xfrm/a:ext',namespaces=N):extent.set('cx',str(cx));extent.set('cy',str(cy))
    prop(p,'jc',val='center');prop(p,'ind',left=0)
    if i!=216:
        cap=deepcopy(ps[218]);settext(cap,f'<그림 {image_names.index(name)+1}. {name[:-4].removeprefix("SDDD ")}>')
        p.addnext(cap)

module_english={'시스템 제어 모듈':'System Control Module','신호처리 모듈':'Signal Processing Module',
 '내부 자극 장치 실행 모듈':'Internal Stimulation Device Execution Module','블루투스 통신 모듈':'Bluetooth Communication Module',
 '부트 제어 및 이미지 적재 모듈':'Boot Control and Image Loading Module','무선 갱신 모듈':'Wireless Update Module'}

# Functions below keep the original technical exposition and extend its numeric detail.
def agc_design():
    add('AGC 동작 원리',4)
    copy_para(788)
    add('AGC는 입력 크기에 따라 이득을 바꾸는 기능이다. 일반적인 입력/출력 관계는 다음 그림과 같다.')
    body.insert(body.index(end),deepcopy(generic_agc));add('<그림 6. 일반적인 AGC 특성>')
    add('본 제품에서는 ADC 출력 이후 단계에 AGC를 사용하며, 오디오 볼륨에 따라 증폭 이득과 압축 기울기를 조절한다.')
    add('Sound1은 혼합된 16표본의 절대값 최대값 x를 사용한다. x는 AGC 적용 전 정수 크기이며 dB SPL 자체가 아니다. 음압과 ADC 크기의 교정 관계는 이 내부 로그 계산과 구분한다.')
    math_line('d = 10 log₁₀(max(x, 1) × 32 / 2⁴⁷)')
    add('d는 Q8.16으로 저장한다. 잡음 경계는 -7700296, 회전점은 -6209352이며 등호는 증폭 영역에 포함한다. 아래 식의 d와 g는 정수 Q8.16 값이다. N은 Q12.12, A/S/B는 Q8.16이다.')
    math_line('잡음 영역: g = (((131072 × d) >> 20) + N - (d >> 4)) << 4')
    math_line('증폭 영역: g = A')
    math_line('압축 영역: g = ((S × d) >> 16) + B - d')
    amp=[0,108138,216277,324416,432555,540694,648833,756971,865110,973249]
    slopes=[47999,43193,38387,33581,28774,23968,19162,14356,9550,4744]
    biases=[-1661530,-2008762,-2355995,-2703227,-3050459,-3397692,-3744924,-4092157,-4439389,-4786621]
    noises=[481268,488027,494785,501544,508303,515061,521820,528579,535337,542096]
    table('오디오 볼륨별 AGC 정수 계수',['볼륨','A','N','S','B'],[[i+1,amp[i],noises[i],slopes[i],biases[i]] for i in range(10)],[800,1950,1950,1950,1950])
    agc_chart(amp,slopes,biases,noises)
    add('이득이 커질 때는 천천히, 작아질 때는 빠르게 바꾸어 급격한 출력 변화를 줄인다. gprev가 목표 g 이하이면 (655×g+64880×gprev)>>16, 그보다 크면 (62259×g+3276×gprev)>>16을 사용한다. 두 계수의 합은 65535이며 고정소수점 계산에서 이 정수 계수를 그대로 사용한다.')
    add('시간 변화가 적용된 로그 이득에서 1972830(Q8.16, 10log10(1024))을 뺀 뒤 cfx_dB_to_pwr를 호출한다. Q1.47 결과를 24비트 우측 이동하고 4194304(Q12.12, 1024)와 곱한 뒤 23비트 우측 이동하여 Q12.12 선형 이득으로 복원한다. 각 16표본에는 (표본×이득)>>12를 적용하고 signed 24비트 범위로 포화한다.')
    add('입력 크기 공유, 목표 로그 이득 계산, 시간 변화, 표본 적용을 각각의 처리 단계로 구성한다. 목표 로그 이득은 중간 계산값이고, 함수의 최종 반환값은 시간 변화가 반영된 선형 이득이다.')

def fft_design():
    add('주파수 분석 원리',4)
    for i in [814,815,816,817]:copy_para(i)
    add('대역 대표값에는 현재 구현의 4비트 축소 보상을 적용한다. 주파수별 음향 응답이나 마이크 감도 보정은 이 정수 축소 연산과 별개다.')
    for i in [820,821,823,824,825,827,828]:copy_para(i)
    add('CFX는 매 1ms의 AGC 출력 16표본을 역순으로 FIFO에 쓰며 최근 512표본을 분석한다. 16kHz 표본화에서 bin 간격은 31.25Hz이다. HEAR는 CMD 1에서 Win_DFT_R_Forward와 vMag를 이어 실행하고 완료 이벤트 1을 알린다. 체인을 서로 독립된 완료 이벤트로 처리하지 않는다.')
    for i in [830,832,833]:copy_para(i)
    add('bin 대역 번호 -1은 제외, 0~31은 유효 대역이다. 맵의 분석 대역 수와 준비된 대역 수가 같을 때 256개 인덱스를 복사한다. 사용하지 않는 대역의 크기는 0으로 유지한다. HEAR의 크기 연산과 CFX의 대역 최대값 선택은 별도 유닛으로 확인한다.')

def mapping_design():
    add('비선형 매핑 원리',4);copy_para(838);body.insert(body.index(end),deepcopy(equation))
    add('x는 대역 대표 크기, y는 자극의 디지털 크기이다. p는 0.15이며 Q1.23 저장값은 1258291이다. CM3가 실제 T/C 전류를 DAC 디지털 크기로 변환하여 전달하면 CFX가 해당 T/C와 맵의 xmin/xmax로 A/B 계수를 계산한다.')
    math_line('A = (Cnew - T) / (xmaxᵖ - xminᵖ),   B = Cnew - A × xmaxᵖ')
    add('A/B는 Q16.8, x의 p승은 Q8.16으로 계산한다. xmin/xmax는 현재 맵의 입력 범위를 사용하며 구형 문서의 고정 숫자로 대체하지 않는다. 오디오 볼륨을 반영하는 매핑 계수 경로에는 별도의 볼륨 표가 사용되므로 AGC의 A 계수를 대신 적용하지 않는다.')
    add('자극 볼륨 1/2/3/4는 T를 유지하면서 Cnew=T+((C-T)×r>>22)로 상한을 조절한다. r은 2936012, 3355443, 3774873, 4194304이다. 예를 들어 T=50, C=150이면 정수 절삭 후 Cnew는 119/129/139/150이다.')
    add('묵음 모드 1은 T+오프셋보다 작은 계산값을 0으로 만든다. 모드 2는 T 미만을 T로 제한한다. 두 모드 모두 Cnew를 상한으로 사용한다. 묵음 제어에는 정의된 모드 1/2를 사용하며, 공유 관찰값은 자극 알림 덮어쓰기 전 값으로 저장한다.')

def strategy_design():
    add('CIS와 NofM의 자극 순서',4)
    add('신호처리 모듈에서 전략과 대역을 선택하고 기본 순서를 정한다. 내부 자극 장치 실행 모듈은 이 결과로 패킷 헤더와 PCM 데이터를 구성하여 출력한다.')
    add('CIS는 사용 전극을 순서대로 출력한다. NofM은 32대역 중 크기가 큰 16개를 선택한다. 점수는 (크기<<5)|(31-인덱스)이며 같은 크기에서는 낮은 인덱스를 우선한다. 선택된 인덱스를 정렬한 후 낮은 절반과 높은 절반을 교차 배치한다.')
    table('NofM 16대역 순서 예',['조건','인덱스 순서'],[
      ['선택 결과','0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15'],
      ['기본 교차 순서','0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15'],
      ['직전 마지막이 0','15, 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7']], [2300,6700])
    add('현재 순서의 처음과 끝 중 직전 마지막 전극에서 더 먼 쪽을 비교한다. 끝이 더 멀면 마지막 한 개를 먼저 보내고 나머지 순서는 유지한다. 전체 배열을 역순으로 바꾸는 처리가 아니다. 채널당 3프레임, 1ms당 8개 자극으로 16개를 두 구간에 나누고 나머지 프레임은 NOP로 채운다. 이 표의 0~31은 내부 배열 인덱스이다.')

def cm3_signal_design():
    add('펄스폭과 자극 전류 변환',4)
    add('맵의 T/C는 uA 단위의 전류이다. 이를 그대로 PCM 크기 필드에 넣지 않고 내부기 DAC 범위와 오프셋을 선택한 뒤 0~255의 디지털 값으로 변환한다. 맵 변경 후 계산이 끝난 자료만 공유하여 CFX의 매핑 입력으로 사용한다.')
    math_line('필요 시간 = 2 × 펄스폭 + 4 + 10   (us)')
    add('한 PCM 프레임을 41us로 취급한다. 필요 시간이 프레임 수×41 이상인 동안 프레임 수를 늘린다. 1ms의 전송 가능 채널 수는 24/프레임 수의 정수 몫이다. 펄스폭 13/14us는 각각 1/2프레임, 24/12개다. NofM은 사용 대역 16개 이상인 설정에서 3프레임/8개로 구성한다.')
    table('현재 내부기 설정의 DAC 환산',['항목','A','B','C','D'],[
      ['자극 범위 상한 uA','400','803','1203','1606'],['자극 환산 계수 Q1.15','20871','10402','6942','5201'],
      ['오프셋 범위 상한 uA','295','592','-','-'],['오프셋 환산 계수 Q1.15','14435','7185','-','-']], [3000,1500,1500,1500,1500])
    add('isd_ver1_1_2 설정에서 자극 디지털값은 (전류-오프셋 전류)에 선택 범위 환산 계수를 곱하고 15비트 우측 이동하여 구한다. 오프셋 레지스터도 선택한 오프셋 계수로 계산한다. 펄스폭 25us, 32채널 모두 T=100/C=300uA이면 A/A 범위, 오프셋 전류 100uA, 오프셋 레지스터 44, T=0/C=127이 된다. 범위 선택과 전하량 조건의 분기는 각각 확인한다.')

def unit_flow(u):
    key=u['su'].removeprefix('SU-ID-')
    if key not in FLOWS:return
    flow=FLOWS[key]
    if flow['note']:add('흐름 참고 : '+flow['note'])
    path=OUT/f'이미지 참조/SDDD 순서도 {key}.png'
    with Image.open(path) as im:width,height=im.size
    p=deepcopy(generic_agc)
    prop(p,'ind',left=0);prop(p,'jc',val='center')
    rid='rIdFlow'+key.replace('-','')
    target='media/sound1_flow_'+key+'.png'
    rr=deepcopy(byid['rId20']);rr.set('Id',rid);rr.set('Target',target);rels.append(rr)
    p.xpath('.//a:blip',namespaces=N)[0].set('{'+N['r']+'}embed',rid)
    cx=5760000;cy=round(cx*height/width)
    for x in p.xpath('.//wp:extent|.//a:xfrm/a:ext',namespaces=N):x.set('cx',str(cx));x.set('cy',str(cy))
    for x in p.xpath('.//wp:docPr',namespaces=N):
        x.set('id',str(1000+list(FLOWS).index(key)));x.set('name','Flow '+key)
        x.set('descr',u['su']+' '+u['name']+' 설계 순서도')
    parts['word/'+target]=path.read_bytes();body.insert(body.index(end),p)
    add('그림 0. '+u['su']+' '+u['name']+' 순서도')

def unit(u):
    add(u['su']+', '+u['name'],4)
    add('목적 : '+u['purpose'])
    add('입력 : '+u['inp'])
    add('출력 : '+u['out'])
    steps=u['steps']
    for s in [' 공개 문서의 시험 벡터에는 실제 개인 정보나 키를 넣지 않는다.',
              ' 공개 시험 자료에는 실제 키나 개인 식별 값을 넣지 않는다.']:
        steps=steps.replace(s,'')
    steps=steps.replace('삭제 범위는 시험용 저장소에서 확인한다.','초기화 대상은 호출한 경로의 선택 범위로 한정한다.')
    add('처리 : '+steps)
    if u['method']=='A':
        conditions=u['expected']
        conditions=conditions.replace('전체 시험 자료를 대상으로 해야 한다. 원본 제품 저장소에서 시험하지 않는다.','전체 저장 자료를 대상으로 한다.')
        conditions=conditions.replace(' QCC 내부 무선 프로토콜의 구현을 이 시험으로 대신하지 않는다.',' 이 유닛은 E8300 측 명령 처리 범위이며 QCC 내부 무선 프로토콜은 별도로 구성한다.')
        conditions=conditions.replace('길이/정렬 전제를 벗어난 처리는 별도 검증 대상으로 구분한다.','데이터 길이와 정렬 조건은 이 함수의 입력 전제로 구분한다.')
        add('동작 조건 : '+conditions)
    if u['symbols']:
        for k in range(0,len(u['symbols']),4):add(('인터페이스 : ' if k==0 else '인터페이스 계속 : ')+', '.join(u['symbols'][k:k+4]))
    else:add('인터페이스 : '+u['anchor'])
    # Keep file locations concise; full line/hash evidence stays in the task record.
    paths=list(dict.fromkeys(ref['path'] for ref in u['refs']))
    for path in paths[:3]:add('구현 위치 : '+path)
    if u['method']=='D':add('입출력 예 : '+EXAMPLES[u['su'].removeprefix('SU-ID-')])
    unit_flow(u)

def agc_chart(amp,slopes,biases,noises):
    im=Image.new('RGB',(1400,820),'white');d=ImageDraw.Draw(im)
    font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',25)
    small=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',22)
    left,top,right,bottom=145,85,1250,670
    def pt(x,y):return (left+(x+130)/50*(right-left),bottom-(y+140)/65*(bottom-top))
    for x in range(-130,-79,10):
        px=pt(x,-140)[0];d.line((px,top,px,bottom),fill='#CCCCCC',width=1);d.text((px-25,bottom+15),str(x),font=small,fill='black')
    for y in range(-140,-74,10):
        py=pt(-130,y)[1];d.line((left,py,right,py),fill='#CCCCCC',width=1);d.text((left-75,py-15),str(y),font=small,fill='black')
    for i in range(10):
        points=[]
        for x in range(-13000,-7999,10):
            v=round((x/100)*65536)
            if v < -7700296:g=(((131072*v>>20)+noises[i]-(v>>4))<<4)
            elif v > -6209352:g=(slopes[i]*v>>16)+biases[i]-v
            else:g=amp[i]
            points.append(pt(v/65536,(v+g)/65536))
        d.line(points,fill=('#234B73' if i==0 else '#B34035' if i==9 else '#9EA4AB'),width=4 if i in [0,9] else 2)
    d.rectangle((left,top,right,bottom),outline='black',width=2)
    for val,label in [(-7700296/65536,'잡음 경계'),(-6209352/65536,'회전점')]:
        px=pt(val,-140)[0]
        for y in range(top,bottom,14):d.line((px,y,px,min(y+7,bottom)),fill='#555555',width=2)
        d.text((px-40,top-40),label,font=small,fill='black')
    d.text((155,8),'출력 로그 값 d + 목표 이득',font=font,fill='black')
    d.text((490,735),'입력 로그 값 d (내부 단위)',font=font,fill='black')
    for x,color,label in [(760,'#234B73','볼륨 1'),(950,'#B34035','볼륨 10')]:
        d.line((x,25,x+50,25),fill=color,width=5);d.text((x+60,8),label,font=small,fill='black')
    path=OUT/'이미지 참조/SDDD AGC 특성.png';im.save(path)
    p=deepcopy(generic_agc);prop(p,'ind',left=0);prop(p,'jc',val='center')
    rid='rIdSound1AGC';rr=deepcopy(byid['rId20']);rr.set('Id',rid);rr.set('Target','media/sound1_agc_characteristic.png');rels.append(rr)
    p.xpath('.//a:blip',namespaces=N)[0].set('{'+N['r']+'}embed',rid)
    for x in p.xpath('.//wp:extent|.//a:xfrm/a:ext',namespaces=N):x.set('cx','5500000');x.set('cy',str(round(5500000*820/1400)))
    for x in p.xpath('.//wp:docPr',namespaces=N):x.set('id','900');x.set('descr','Sound1 AGC 볼륨별 목표 입출력 특성')
    parts['word/media/sound1_agc_characteristic.png']=path.read_bytes();body.insert(body.index(end),p)
    add('<그림 7. Sound1 AGC 볼륨별 목표 입출력 특성>')
    add('곡선은 위 정수 계수에 따른 목표 특성이다. 시간 변화와 로그/지수 라이브러리 양자화 전이며, 축은 음압 dB SPL이 아닌 내부 로그 단위다. 가운데 회색 곡선은 볼륨 2~9이다.')

for core in ['CFX','HEAR','CM3','BOOT']:
    label={'BOOT':'부트로더'}.get(core,core)
    add(label+' 소프트웨어 상세 설계',10)
    if core=='CFX':
        add('소프트웨어 아키텍쳐 기술서에서 정의한 기능 모듈을 소프트웨어 항목과 유닛으로 구체화한다. 한 유닛은 하나의 기능을 수행하는 단위이며 같은 상태의 설정/조회 함수는 함께 둔다. 계산 단계나 완료 시점이 다른 기능은 별도 유닛으로 나눈다.')
        add('소프트웨어 항목에는 SI-ID, 유닛에는 SU-ID를 부여하여 실행 코어와 기능을 구분한다. 각 유닛의 목적, 입력, 출력, 처리 순서와 인터페이스를 아래에 정의한다.')
        add('분기, 반복 또는 상태 전환이 중요한 유닛에는 설계 순서도를 둔다. 단순 설정/조회는 본문으로 설명한다. 마름모는 조건 판단, 사각형은 처리, 둥근 상자는 시작/종료를 나타낸다. 다음 호출을 기다리는 경로는 해당 호출에서 끝나며 다음 이벤트에서 이어진다.')
    units=[u for u in MODEL if u['core']==core]
    table(label+' 유닛 목록',['소프트웨어 항목','유닛 ID','유닛 이름'],[[u['si'],u['su'],u['name']] for u in units],[2250,2250,4500])
    groups=OrderedDict()
    for u in units:groups.setdefault(u['module'],[]).append(u)
    for module,rows in groups.items():
        add(module_english[module],2);add(rows[0]['si']+', '+module,3)
        if core=='CFX' and module=='신호처리 모듈':
            add('입력 혼합과 빔포밍',4)
            add('단일 입력은 각 표본을 5비트 우측 이동한다. 독립 두 입력의 혼합은 축소된 두 값을 합하며 1/2로 나누지 않는다. 두 마이크의 빔포밍만 한 표본 지연 경로와 기준 경로를 더한 뒤 1비트 우측 이동하여 평균한다. 블록의 마지막 표본은 지연 경로의 직전 블록 첫 표본으로 이어 연결한다.')
            add('좌측 전면은 DMIC2, 우측 전면은 DMIC1을 지연 경로로 사용한다. I2S 입력이 있으면 마이크/I2S 혼합 경로를 선택하고, 매핑 연결/크래들 마이크 상태에 따라 입력 게인을 구분한다.')
            agc_design();fft_design();mapping_design();strategy_design()
        if core=='CM3' and module=='신호처리 모듈':cm3_signal_design()
        if core=='BOOT' and module=='무선 갱신 모듈':add('CM3가 수신한 이미지와 기동 상태를 읽어 다음 실행 위치를 정한다. 갱신 전송, 파일 완결성 검사, 기동 위치 선택을 구분하며 이미지 CRC가 인증 기능을 대신하지 않는다.')
        for u in rows:unit(u)

add('QCC 소프트웨어 상세 설계',10);add('E8300 연계',2)
add('E8300 측 설계는 CM3의 블루투스 통신 모듈과 CFX의 오디오 경로에 포함한다. CM3는 SPI 수신 0x34의 배터리/충전기/뚜껑 정보, 0x35의 LED/갱신 연결 정보, 0x36의 오디오 연결 종류를 처리한다. 리모콘과 매핑 명령은 각 처리 경로로 전달하고 SPI 응답 버퍼에 결과를 준비한다.')
add('CFX는 I2S가 있을 때 마이크와 외부 입력을 혼합한다. 크래들 마이크만 해당 게인을 사용하고 일반 스트리밍 및 매핑 연결은 지정된 기준 게인을 사용한다. CM3는 QCC_CTRL로 정상 동작/종료 요청, ISD_CHECK로 내부기 연결 상태를 전달한다.')
add('QCC 내부 설계',2)
add('QCC 내부 모듈과 유닛은 QCC 펌웨어 자료를 확보한 후 이 절에 추가한다. E8300 연계 유닛과 구분되는 QCC 식별자를 부여하고 각 유닛의 목적, 입출력, 처리와 인터페이스를 정의한다.')
add('소프트웨어 상세 설계 검증',10)
add('소프트웨어 아키텍쳐 기술서의 기능 모듈과 본 문서의 소프트웨어 항목을 대조하여 기능의 누락과 역할 중복을 확인한다. 각 유닛의 목적, 입력, 출력, 처리 순서와 인터페이스가 상위 설계를 구체화하고 있는지 검토한다.')
add('유닛 사이에 전달하는 데이터의 단위와 저장 형식, 처리 완료 시점, 초기 상태 및 오류 시 동작이 서로 일치해야 한다. 신호처리 수식과 정수 계수는 입출력 예와 함께 검토한다. QCC 내부 상세설계는 해당 자료를 추가한 후 같은 기준으로 검토한다.')

# Number all visible figures in reading order, including retained original images.
figure_number=0
for paragraph in body.findall(q('p')):
    s=txt(paragraph)
    if re.match(r'<?그림\s+\d+[.\s]',s):
        figure_number+=1
        settext(paragraph,re.sub(r'^(<?그림\s+)\d+',lambda m:m.group(1)+str(figure_number),s))
assert figure_number==7+len(FLOWS),figure_number

# Replace all literal product names across split runs in the front matter/header/footer.
for name,data in list(parts.items()):
    if not name.startswith('word/') or not name.endswith('.xml'):continue
    x=r if name=='word/document.xml' else E.fromstring(data)
    for p in x.xpath('//w:p',namespaces=N):
        s=txt(p);new=s.replace('TD20B-A-EXT/TD23D-A-EXT','TD24A-A-EXT').replace('TD20B','TD24A').replace('TD23D','TD24A')
        if new!=s:settext(p,new)
    if name=='word/document.xml' or E.tostring(x)!=E.tostring(E.fromstring(data)):parts[name]=E.tostring(x,xml_declaration=True,encoding='UTF-8',standalone=True)
parts['word/_rels/document.xml.rels']=E.tostring(rels,xml_declaration=True,encoding='UTF-8',standalone=True)
ct=E.fromstring(parts['[Content_Types].xml'])
if not any(x.get('Extension')=='png' for x in ct):E.SubElement(ct,'{http://schemas.openxmlformats.org/package/2006/content-types}Default',Extension='png',ContentType='image/png')
parts['[Content_Types].xml']=E.tostring(ct,xml_declaration=True,encoding='UTF-8',standalone=True)
with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
    for name,data in parts.items():z.writestr(name,data)
assert len([p for p in body.findall(q('p')) if txt(p).startswith('SU-ID-')])==115
print('Candidate:',candidate,'units=115')
