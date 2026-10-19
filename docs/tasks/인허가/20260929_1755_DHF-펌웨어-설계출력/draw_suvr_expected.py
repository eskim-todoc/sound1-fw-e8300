"""고정 시험값으로 예상 도식을 만든다. 장치 캡처나 실측 이미지를 만들지 않는다."""
from html import escape
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

TASK = Path(__file__).parent
OUT = TASK / 'qa/suvr/expected-images'
OUT.mkdir(parents=True, exist_ok=True)
FONT = 'C:/Windows/Fonts/malgun.ttf'
BOLD = 'C:/Windows/Fonts/malgunbd.ttf'
BLUE, GREEN, RED, GRAY = '#245DA0', '#228464', '#BA3535', '#65707B'
MANIFEST = []


class Figure:
    """같은 좌표/텍스트로 편집 가능한 SVG와 Excel 삽입 PNG를 함께 출력한다."""
    def __init__(self, uid, slug, title, foot):
        self.uid, self.slug = uid, slug
        self.im = Image.new('RGB', (1100, 680), 'white')
        self.d = ImageDraw.Draw(self.im)
        self.svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="680" viewBox="0 0 1100 680"><rect width="1100" height="680" fill="white"/>']
        self.text(34, 22, title, 32, bold=True)
        self.text(34, 70, '예상 도식 / 실측 화면 아님', 24, RED)
        self.line([(34, 111), (1066, 111)], '#BEC5CB', 2)
        for i, line in enumerate(foot):
            self.text(34, 596 + i * 31, line, 23, GRAY)
        self.title = title

    def text(self, x, y, text, size=26, color='#202A33', bold=False):
        f = ImageFont.truetype(BOLD if bold else FONT, size)
        self.d.text((x, y), text, fill=color, font=f)
        self.svg.append(f'<text x="{x}" y="{y+size}" font-family="Malgun Gothic" font-size="{size}" font-weight="{700 if bold else 400}" fill="{color}">{escape(str(text))}</text>')

    def line(self, pts, color=BLUE, width=3):
        self.d.line(pts, fill=color, width=width)
        points = ' '.join(f'{x:.2f},{y:.2f}' for x, y in pts)
        self.svg.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="{width}"/>')

    def rect(self, box, fill='#F1F3F5', outline='#AEB7C0'):
        self.d.rectangle(box, fill=fill, outline=outline, width=2)
        x1,y1,x2,y2=box
        self.svg.append(f'<rect x="{x1}" y="{y1}" width="{x2-x1}" height="{y2-y1}" fill="{fill}" stroke="{outline}" stroke-width="2"/>')

    def circle(self, x, y, radius, fill):
        self.d.ellipse((x-radius,y-radius,x+radius,y+radius),fill=fill,outline='#737D86',width=2)
        self.svg.append(f'<circle cx="{x}" cy="{y}" r="{radius}" fill="{fill}" stroke="#737D86" stroke-width="2"/>')

    def save(self, caption, source):
        name = f'{self.uid}-{self.slug}'
        self.im.save(OUT/f'{name}.png')
        (OUT/f'{name}.svg').write_text('\n'.join(self.svg)+ '</svg>',encoding='utf-8')
        MANIFEST.append(dict(id=self.uid,file=f'{name}.png',caption=caption,source=source,width=1100,height=680))


def waveform():
    f=Figure('CFX-020','OD','OD 출력 관찰: 1 kHz 순음과 무음',[
        '파형의 높이는 설명용 상대값이다. 실제 DAC 전압/이득 기준이 아니다.',
        '순음과 무음의 구분을 확인하며, 청취로 전극 전류를 판정하지 않는다.'])
    for title, y in [('1 kHz 순음',240),('무음',465)]:
        f.text(38,y-94,title,bold=True)
        f.line([(160,y),(1040,y)],'#9FA9B3',2)
        f.text(60,y-13,'0',24)
        for x in range(5):
            xx=160+220*x
            f.line([(xx,y-60),(xx,y+62)],'#E4E8EB',1)
            f.text(xx-8,y+69,str(x),22)
        pts=[(160+i*880/800,y-(58*math.sin(2*math.pi*4*i/800) if title.startswith('1') else 0)) for i in range(801)]
        f.line(pts,BLUE,4)
    f.text(919,550,'시간 (ms)',23)
    f.save('예상 순음/무음 파형. 실제 출력 관찰 사진은 오른쪽에 첨부한다.','CFX/CFX-020.md / 1 kHz 순음 및 무음')


def hear():
    f=Figure('HEAR-001','array','절대값과 최대값: 16표본 예상 배열',[
        '나머지 인덱스 5~15는 모두 0이다. 체인 완료 이벤트는 Event 0이다.',
        '최대값 100은 체인 완료 후 첫 칸을 덮어쓴다. 아래 표는 생성된 기대값이다.'])
    rows=[('입력',[0,-32,17,-100,99]),('절대값 직후',[0,32,17,100,99]),('체인 완료 후',[100,32,17,100,99])]
    for j in range(5):f.text(288+j*155,150,f'[{j}]',25,GRAY)
    for i,(label,vals) in enumerate(rows):
        y=218+i*102;f.text(40,y+20,label,27,bold=True)
        for j,v in enumerate(vals):
            f.rect((250+j*155,y,389+j*155,y+76), '#E7F1FA' if (i==2 and j==0) else '#F7F8F9')
            f.text(266+j*155,y+20,str(v),29,BLUE if (i==2 and j==0) else '#202A33')
    f.save('입력/절대값/체인 완료 결과를 같은 인덱스로 비교한다.','HEAR/HEAR-001.md / CMD 0 입력')
    f=Figure('HEAR-002','bins','FFT 예상 최대 위치: 입력별 bin',[
        '가로축은 FFT bin이다. 막대 높이는 크기 측정값이 아닌 위치 표시다.',
        'FFT/vMag 수치는 표의 오차식과 블록 지수 E로 비교한다. 완료는 Event 1.'])
    for label,y,peak,color in [('1 kHz 입력',235,32,BLUE),('2 kHz 입력',380,64,GREEN)]:
        f.text(38,y-79,label,28,bold=True)
        f.line([(190,y),(1050,y)],GRAY,2)
        for b in [0,32,64,128,192,255]:
            x=190+860*b/255
            f.line([(x,y),(x,y+8)],GRAY,2);f.text(x-14,y+17,str(b),22)
        x=190+860*peak/255;f.line([(x,y),(x,y-82)],color,8)
        f.text(x+17,y-84,f'최대 bin {peak}',27,color,True)
    f.text(38,472,'무음: FFT/vMag 전부 0',27)
    f.text(38,519,'DC / 교번 입력: vMag의 최대 인덱스 0',27)
    f.save('입력 2/3의 최대 bin 32/64, 무음과 DC/Nyquist 조건의 예상 표시.','HEAR/HEAR-002.md / 512점 입력과 통과 기준')


def led_patterns():
    f=Figure('CM3-014','steady','LED 예상 표시: 지속 점등/소등',[
        '색은 식별을 위한 도식이다. 실제 LED의 조도나 색좌표를 나타내지 않는다.',
        '연결=1, 표시 설정=2이면 일반 표시 금지. 오류/전원 표시는 예외다.'])
    rows=[('NONE / IDLE','#262626','소등'),('BATT_READY','#2DAE60','녹색 지속'),('IN_USE','#FFFFFF','앱: 흰색 / 공장: 보라색'),('BATT_MID','#F59B2B','주황색 지속'),('매핑, 내부기 미연결 / 정상 배터리','#3D74EA','파란색 지속'),('매핑, 내부기 미연결 / 낮은 배터리','#9063CE','보라색 지속')]
    for i,(label,color,desc) in enumerate(rows):
        y=159+66*i;f.circle(66,y+17,19,color);f.text(105,y,label,24);f.text(688,y,desc,24)
    f.save('지속 표시 예상색. 실제 사진과 비교하되 색좌표 합격 기준으로 사용하지 않는다.','CM3/CM3-014.md / visual-patterns, visual-indicator-gate')
    f=Figure('CM3-014','blink','LED 예상 표시: 점멸 순서',[
        '색 구간은 ON, 흰 구간은 OFF다. 각 행은 2주기를 같은 폭으로 그렸다.',
        '전원 ON/OFF는 선행 소등 400 ms 뒤 ON 180/OFF 180 ms를 4회 반복한다.'])
    rows=[('배터리 부족','#F59B2B',1100,1100),('매핑 + 내부기 연결','#3D74EA',200,800),('매핑 + 내부기 연결, 낮은 배터리','#9063CE',200,800),('PAIR','#3D74EA',500,500),('OTA_QCC','#2DAE60',1100,1100),('OTA_EZAIRO','#2DAE60',180,180),('MAP/MCU/센서/FPGA/PMIC 오류','#D94B43',180,180)]
    for i,(label,color,on,off) in enumerate(rows):
        y=138+61*i;f.text(35,y,label,23)
        for cycle in range(2):
            x=615+cycle*174;ratio=on/(on+off)
            f.rect((x,y,x+174,y+27),'white');f.rect((x,y,x+174*ratio,y+27),color)
        f.text(615,y+30,f'ON {on} / OFF {off} ms',20,GRAY)
    f.save('상태별 점멸의 색/ON/OFF 순서 예. 행별 시간축 폭은 서로 다르다.','CM3/CM3-014.md / 패턴 표, visual-special')


def power():
    f=Figure('CM3-035','codes','송신 전력 설정: 최소/최대 코드',[
        'Board_1_5 ISL9122, 주소 0x18 / VoltageSet 레지스터 0x11.',
        '164/214는 전압(V)이 아닌 코드다. 물리 전압은 실측값을 별도로 기록한다.'])
    for i,(label,value) in enumerate([('최소 설정',164),('최대 설정',214)]):
        y=173+i*175;f.text(45,y,label,29,bold=True)
        f.rect((250,y-10,590,y+99));f.text(280,y+4,f'쓰기 코드 = {value}',29)
        f.text(280,y+53,'write 반환 = 1',25)
        f.line([(608,y+42),(692,y+42)],GRAY,3)
        f.rect((710,y-10,1040,y+99));f.text(740,y+4,f'읽은 코드 = {value}',29)
        f.text(740,y+53,'read 반환 = 1',25)
    f.save('PMIC 최소/최대 코드의 예상 쓰기/읽기 값. 전압 파형의 대체 자료가 아니다.','CM3/CM3-035.md / H-01/H-02, R-01/R-02')
    f=Figure('CM3-039','vp','VP 출력 허용/차단: 예상 관찰 개념도',[
        'VP 기능이 적용된 보드 조건의 유무 확인이다. 상승/하강 시간은 생략했다.',
        '현재 보드의 적용 여부를 먼저 확인하며 전압/전류 정확도는 판정하지 않는다.'])
    f.text(38,148,'요청',27,bold=True);f.text(38,230,'출력 유무',27,bold=True)
    for x,label in [(255,'비활성'),(525,'활성'),(830,'비활성/차단')]:f.text(x,152,label,26)
    f.line([(190,422),(420,422),(420,288),(735,288),(735,422),(1048,422)],BLUE,5)
    f.text(61,285,'있음',25);f.text(61,411,'없음',25)
    f.text(323,478,'활성 요청',26);f.text(699,478,'차단 요청',26)
    f.text(797,543,'진행 순서 (시간 비례 아님)',21,GRAY)
    f.save('VP 활성/차단 시 출력 유무의 예상 도식. 실제 PMIC 측정 사진은 오른쪽에 첨부한다.','CM3-039 현재 통과 기준 / 구형 보드와 현행 보드 적용 제한')


def boot():
    f=Figure('BOOT-002','cal','Calibration 핀: 지연 호출 전/후 관찰 예',[
        '30.72 MHz / 1000 us 요청의 Sys_Delay 인자 = 30,000 cycle.',
        '핀 펄스에는 호출/토글 오버헤드가 포함된다. 폭을 정확히 1 ms로 단정하지 않는다.'])
    f.text(40,154,'핀 HIGH',26);f.text(40,383,'핀 LOW',26)
    f.line([(190,402),(365,402),(365,185),(822,185),(822,402),(1050,402)],BLUE,5)
    f.line([(365,270),(822,270)],GRAY,2)
    f.text(416,295,'지연 구간 + 오버헤드',27)
    f.text(301,438,'호출 전 토글',24);f.text(758,438,'호출 후 토글',24)
    f.text(237,521,'0 us 요청도 토글/호출 시간은 남을 수 있다.',26)
    f.save('선택 측정의 예상 Calibration 펄스. 실제 시간 수치는 스코프 사진에 기록한다.','BOOT/BOOT-002.md / 기준서의 선택 Calibration 관찰')
    f=Figure('BOOT-004','fade','기동 LED: 예상 PWM 듀티 변화',[
        '세로축은 요청 듀티(%), 가로축은 주입/설정 시간(ms)이다. 조도 실측이 아니다.',
        '30 ms 상승, 300 ms 유지, 30 ms 하강. 360 ms 소등, 361 ms DONE.'])
    x=lambda t:180+t*2.3;y=lambda d:475-d*6
    f.text(40,140,'듀티 (%)',26)
    for d in [0,20,40]:f.line([(170,y(d)),(1050,y(d))],'#D8DDE2',2);f.text(110,y(d)-16,str(d),23)
    for value,color,label in [(40,BLUE,'B 최대 40%'),(30,GREEN,'G 최대 30%')]:
        f.line([(x(0),y(0)),(x(30),y(value)),(x(330),y(value)),(x(360),y(0))],color,5)
        f.text(x(150),y(value)-39,label,27,color)
    for t in [0,30,330,360]:
        f.line([(x(t),475),(x(t),487)],GRAY,2);f.text(x(t)-17,498,str(t),23)
    f.text(900,544,'시간 (ms)',24)
    f.save('기동 LED의 예상 듀티/상태 전환. 실제 색/시간 관찰 사진을 오른쪽에 첨부한다.','BOOT/BOOT-004.md / tdc_boot_led.c의 30/300/30 ms, G30/B40')
    f=Figure('BOOT-015','transfer','메모리 전송: 입력 바이트와 단위',[
        'X/Y 전송 모드는 8→24비트이며 16 byte를 전달한다.',
        '아래는 전송 입력/단위 도식이다. 실제 24비트 메모리 패킹 화면은 별도로 첨부한다.'])
    f.text(40,143,'입력 word[0..3]',28,bold=True)
    for i,v in enumerate(['0x11','0x22','0x33','0x44']):
        x=45+i*255;f.rect((x,194,x+235,259));f.text(x+57,208,v,30)
        f.text(x,285,f'{v[2:]} 00 00 00',28)
    f.text(40,358,'원시 입력: 16 byte / 순서 보존',29,bold=True)
    f.text(40,421,'X/Y 영역: DMA count 16, mode 0x58000',27)
    f.text(40,477,'P/CM3/IOMEM: DMA count 4 word, mode 0',27)
    f.save('0x11/22/33/44 입력의 바이트 순서와 전송 단위 도식.','BOOT/BOOT-015.md / 32비트 입력과 DMA count/mode')
    f=Figure('BOOT-018','service','서비스 LED: 예상 색 순환과 재기동 요청',[
        '각 전환 간격은 380 tick이다. 실제 경과 시간/색 밝기는 별도 관찰한다.',
        '재기동 훅 1회는 실제 재부팅 완료를 뜻하지 않는다. 요청과 재진입을 구분한다.'])
    colors=['#222222','#D84B43','#36A968','#EBD64C','#4279D8','#AF5ABA','#53B7C2','#FFFFFF']
    names=['꺼짐','R','G','R+G','B','R+B','G+B','R+G+B']
    for i,(color,name) in enumerate(zip(colors,names)):
        x=86+i*134;f.circle(x,216,30,color);f.text(x-9,157,str(i),25);f.text(x-33,266,name,23)
    f.text(53,335,'색 번호 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 0',28)
    f.rect((45,416,1050,551),'#F5F7F9')
    f.text(70,434,'reboot 명령: 1 ms 지연 요청 500회 후 reset 호출 1회',26)
    f.text(70,486,'시험 훅 결과: reset=1 / GPIO 쓰기 증가=1 / 코어 기동=0',25)
    f.save('서비스 모드의 예상 RGB 순환과 재기동 요청 조건.','BOOT/BOOT-018.md / 380 tick 색 비트, C1')


waveform();hear();led_patterns();power();boot()
(OUT/'manifest.json').write_text(json.dumps(MANIFEST,ensure_ascii=False,indent=2),encoding='utf-8')
print(f'{len(MANIFEST)} expected diagrams, {len({m["id"] for m in MANIFEST})} units')
