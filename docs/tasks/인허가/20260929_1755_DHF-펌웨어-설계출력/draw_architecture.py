"""SAD figures: physical containment and cross-core functional allocation."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / 'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/이미지 참조'
INK = '#17202b'
FONT = 'C:/Windows/Fonts/malgun.ttf'
BOLD = 'C:/Windows/Fonts/malgunbd.ttf'

class Canvas:
    def __init__(self, size):
        self.im = Image.new('RGB', size, 'white')
        self.d = ImageDraw.Draw(self.im)

    def text(self, xy, value, size=38, bold=False, color=INK, anchor='mm'):
        f = ImageFont.truetype(BOLD if bold else FONT, size)
        self.d.multiline_text(xy, value, font=f, fill=color, anchor=anchor,
                              align='center', spacing=10)

    def box(self, rect, value='', fill='white', outline=INK, radius=15, size=38, bold=False):
        self.d.rounded_rectangle(rect, radius, fill=fill, outline=outline, width=3)
        if value:
            self.text(((rect[0]+rect[2])/2, (rect[1]+rect[3])/2), value, size, bold)

    def dash(self, rect, color='#81909f', width=3):
        x0,y0,x1,y1=rect
        for x in range(x0,x1,22):
            self.d.line((x,y0,min(x+12,x1),y0),fill=color,width=width)
            self.d.line((x,y1,min(x+12,x1),y1),fill=color,width=width)
        for y in range(y0,y1,22):
            self.d.line((x0,y,x0,min(y+12,y1)),fill=color,width=width)
            self.d.line((x1,y,x1,min(y+12,y1)),fill=color,width=width)

    def arrow(self, points, color='#314a64', both=False, width=4):
        self.d.line(points, fill=color, width=width, joint='curve')
        for p,e in [(points[-2],points[-1])] + ([(points[1],points[0])] if both else []):
            a=math.atan2(e[1]-p[1],e[0]-p[0])
            self.d.polygon([e,(e[0]-18*math.cos(a-.45),e[1]-18*math.sin(a-.45)),
                             (e[0]-18*math.cos(a+.45),e[1]-18*math.sin(a+.45))],fill=color)

    def save(self,name):
        OUT.mkdir(parents=True,exist_ok=True)
        self.im.save(OUT/name, dpi=(300,300))

def hardware():
    c=Canvas((2100,1500))
    c.box((25,25,1625,1245),fill='#f6f9fc',outline='#42627f',radius=50)
    c.text((825,75),'외부장치 Sound1  |  TD24A-A-EXT',48,True)
    c.box((1735,260,2075,1010),fill='#fdf6e9',outline='#8b6c36',radius=40)
    c.text((1905,320),'내부 이식장치',43,True)
    c.box((1780,465,2030,640),'수신부\n자극칩',fill='white',size=42)
    c.arrow([(1905,640),(1905,730)],color='#8b6c36')
    c.box((1780,730,2030,935),'전극\n전기 자극',fill='white',size=42)
    # Microphones are within external-device boundary, outside the E8300 silicon.
    c.box((65,205,330,320),'마이크 1',fill='#e4eef7',size=43)
    c.box((65,380,330,495),'마이크 2',fill='#e4eef7',size=43)
    c.box((425,155,1185,965),fill='white',outline='#42627f',radius=30)
    c.text((805,205),'E8300',49,True)
    c.box((465,280,655,570),'오디오\n입력\nFIFO',fill='#edf2f6',size=39)
    c.arrow([(330,260),(380,260),(380,345),(465,345)])
    c.arrow([(330,437),(465,437)])
    c.text((376,490),'PDM',32)
    c.box((715,280,1135,440),'CFX  ↔  HEAR\n처리 제어 / 연산',fill='#e4f0e1',size=38)
    c.arrow([(655,380),(715,380)])
    c.box((715,495,1135,615),'공유 메모리',fill='#f4f4f4',size=39)
    c.arrow([(925,440),(925,495)],both=True)
    c.box((715,675,1135,810),'Cortex-M3\n상태 / 파라미터 제어',fill='#e8edf9',size=38)
    c.arrow([(925,615),(925,675)],both=True)
    c.text((805,885),'PCM / I2C / SPI / GPIO',36)
    # Stimulation transmit section: PCM and I2C are distinct.
    c.box((1300,280,1580,440),'FPGA',fill='#fff0d7',size=43)
    c.arrow([(1135,325),(1300,325)])
    c.text((1240,290),'PCM',32)
    c.box((1300,675,1580,810),'TX PMIC',fill='#fff0d7',size=41)
    c.arrow([(1135,720),(1300,720)],both=True)
    c.text((1215,675),'I2C',32)
    c.arrow([(1135,765),(1235,765),(1235,405),(1300,405)],both=True)
    c.text((1255,535),'I2C',32)
    c.box((1320,500,1580,585),'送受信 코일'.replace('送受信','송수신'),fill='#fff7e8',size=36)
    c.arrow([(1450,440),(1450,500)])
    c.arrow([(1450,675),(1450,585)],color='#8b6c36')
    c.text((1500,625),'전원',31)
    c.arrow([(1580,545),(1780,545)],both=True,color='#8b6c36',width=5)
    c.text((1677,440),'무선\n링크',34)
    # QCC is a separate chip, not a fourth core in E8300.
    c.box((65,630,330,960),'QCC5181\n블루투스\n오디오 / 상태',fill='#e2eef8',size=37,bold=True)
    c.arrow([(330,690),(385,690),(385,540),(465,540)])
    c.text((390,603),'I2S',31)
    c.arrow([(330,855),(715,855),(715,775)],both=True)
    c.text((515,817),'SPI / GPIO',33)
    # Peripheral interfaces, grouped but not conflated with cores.
    c.box((460,1040,740,1175),'저장장치\n이미지 / 맵',fill='white',size=36)
    c.box((800,1040,1120,1175),'터치 센서 / LED',fill='white',size=36)
    c.arrow([(600,965),(600,1040)],both=True)
    c.text((690,998),'SPI/NVM',28)
    c.arrow([(960,965),(960,1040)],both=True)
    c.text((1023,998),'I2C/GPIO',28)
    c.box((1265,1040,1580,1175),'배터리 / 전원부',fill='white',size=36)
    c.text((1420,1210),'전원 배선은 생략',28,color='#526274')
    c.box((30,1340,640,1470),'스마트폰 / 매핑 프로그램 / 크래들',fill='#f5f5f5',size=35)
    c.arrow([(190,1340),(190,960)],both=True)
    c.text((280,1283),'블루투스',33)
    c.text((1400,1360),'실선 경계: 장치 / 칩 구분',34)
    c.text((1400,1420),'주요 기능 연결이며 회로 배선도는 아님',31)
    c.save('시스템 블록도.png')

def software():
    c=Canvas((2100,1470))
    xs=[(55,500),(555,1040),(1090,1575),(1625,2045)]
    titles=['QCC5181','Cortex-M3','CFX','HEAR']
    c.text((1300,43),'E8300',45,True)
    c.d.line((555,80,2045,80),fill='#42627f',width=3)
    for (x0,x1),title in zip(xs,titles):
        c.dash((x0,105,x1,1190),color='#8693a1')
        c.text(((x0+x1)/2,153),title,45,True)
    c.text((277,202),'별도 칩 / 내부 설계 추가 영역',29)
    def band(y0,y1,x0,x1,label,fill,entries):
        c.box((x0,y0,x1,y1),fill=fill,radius=4)
        c.text(((x0+x1)/2,y0+38),label,40,True)
        for col,value in entries:
            a,b=xs[col]
            if col>0 and a-25>x0:
                c.d.line((a-25,y0+72,a-25,y1-16),fill='#9ca9b4',width=2)
            c.text(((a+b)/2,y0+126),value,33)
    band(245,450,35,2065,'시스템 제어 모듈', '#edf0f4',[
        (0,'동작 / 상태 연계\n내부 구현은 별도'),(1,'기동 / 전원 / 터치 / LED\n맵 / 사용자 설정 관리'),
        (2,'오디오 / PCM 설정\n블록 처리 순서 제어'),(3,'기동 / 정지 대상\nCFX가 제어')])
    band(480,690,535,2065,'신호처리 모듈','#dcecd5',[
        (1,'펄스폭 / T / C\n자극 파라미터 계산'),(2,'혼합 / AGC / 대역 분석\n매핑 / CIS / NofM'),(3,'절대값 / 최댓값\n윈도 FFT / 크기')])
    band(720,925,535,1595,'내부 자극 장치 실행 모듈','#fff0ca',[
        (1,'PMIC / FPGA / 내부기\n설정 / 연결 확인'),(2,'PCM 모드 처리\n자극 / 설정 패킷 출력')])
    band(955,1160,35,1060,'블루투스 통신 모듈','#dceaf6',[
        (0,'무선 연결 / SPI 중계\n내부 설계는 별도 추가'),(1,'리모콘 / 매핑 명령\n응답 / OTA 데이터 수신')])
    c.d.rectangle((1110,975,2030,1170),fill='white')
    c.text((1590,1020),'CM3 ↔ CFX : 공유 메모리 / 상태',31)
    c.text((1590,1080),'CFX ↔ HEAR : 호출 / 완료 이벤트',31)
    c.text((1590,1140),'QCC → CFX : I2S 오디오',31)
    c.box((535,1230,2065,1350),'기동 단계  |  CM3 부트로더\n이미지 검사 / 적재 → CFX 및 CM3 응용 프로그램 실행',fill='#f7f7f7',size=35)
    c.text((1050,1418),'점선 세로 영역 = 실행 위치     가로 색 영역 = 여러 실행 위치가 나누어 수행하는 기능',34)
    c.save('소프트웨어 구조도.png')

if __name__=='__main__':
    hardware()
    software()
