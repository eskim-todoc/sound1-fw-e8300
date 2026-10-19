"""기준서의 비교 지점을 그린다. 계측기 화면이나 실제 측정값은 생성하지 않는다."""
from html import escape
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

TASK=Path(__file__).resolve().parent
OUT=TASK/'qa/validation-reports/expected-images'
FONT='C:/Windows/Fonts/malgun.ttf'
BOLD='C:/Windows/Fonts/malgunbd.ttf'
BLUE, GREEN, RED, GRAY='#245DA0','#228464','#BA3535','#65707B'
MANIFEST=[]


class Figure:
    def __init__(self, slug, title, footer, tests):
        self.slug,self.title,self.tests=slug,title,tests
        self.im=Image.new('RGB',(1100,680),'white')
        self.d=ImageDraw.Draw(self.im)
        self.svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="680" viewBox="0 0 1100 680"><rect width="1100" height="680" fill="white"/>']
        self.text(34,22,title,31,bold=True)
        self.text(34,70,'예상 도식 / 실측 화면 아님',24,RED)
        self.line([(34,111),(1066,111)],'#BEC5CB',2)
        for i,t in enumerate(footer):self.text(34,592+i*32,t,23,GRAY)

    def text(self,x,y,value,size=26,color='#202A33',bold=False):
        font=ImageFont.truetype(BOLD if bold else FONT,size)
        text=str(value)
        box=self.d.textbbox((x,y),text,font=font)
        assert box[2] <= 1090 and box[3] < 680,(self.slug,text,box)
        self.d.text((x,y),text,font=font,fill=color)
        self.svg.append(f'<text x="{x}" y="{y+size}" font-family="Malgun Gothic" font-size="{size}" font-weight="{700 if bold else 400}" fill="{color}">{escape(text)}</text>')

    def line(self,pts,color=BLUE,width=3):
        self.d.line(pts,fill=color,width=width)
        pts=' '.join(f'{x:.2f},{y:.2f}' for x,y in pts)
        self.svg.append(f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="{width}"/>')

    def rect(self,box,fill='#F1F3F5',outline='#AEB7C0'):
        self.d.rectangle(box,fill=fill,outline=outline,width=2)
        a,b,c,d=box
        self.svg.append(f'<rect x="{a}" y="{b}" width="{c-a}" height="{d-b}" fill="{fill}" stroke="{outline}"/>')

    def circle(self,x,y,r,fill):
        self.d.ellipse((x-r,y-r,x+r,y+r),fill=fill,outline='#737D86',width=2)
        self.svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="#737D86" stroke-width="2"/>')

    def bracket(self,x1,x2,y,label):
        self.line([(x1,y+8),(x1,y),(x2,y),(x2,y+8)],GRAY,2)
        f=ImageFont.truetype(FONT,24)
        width=self.d.textlength(label,font=f)
        self.text((x1+x2-width)/2,y-36,label,24,GRAY)

    def save(self,caption,basis):
        self.im.save(OUT/f'{self.slug}.png')
        (OUT/f'{self.slug}.svg').write_text('\n'.join(self.svg)+'</svg>',encoding='utf-8')
        MANIFEST.append({'slug':self.slug,'title':self.title,'tests':self.tests,'caption':caption,'basis':basis,'path':str(OUT/f'{self.slug}.png')})


def sysids(*ids):return [f'SW-FW-SYS-Test-ID-{n}' for n in ids]
def intids(*ids):return [f'SW-FW-IntegrationTest-ID-{n:03d}' for n in ids]


def stimulus_period():
    for slug,title,tests,period,ticks,foot in [
        ('SYS-01-rate','전체 자극률: 연속 자극 사이 간격',sysids(1),'50 us 이하',['0','50','100','150'],['전체 자극률 20 kHz 이상이면 자극 시작 간격은 50 us 이하이다.', '등간격 예시이며 펄스 높이와 폭은 판정값이 아니다.']),
        ('SYS-02-channel-rate','채널별 자극률: 같은 전극의 반복 간격',sysids(2),'1.000 ms',['0','1','2','3'],['중심 1000 Hz, 허용 범위 950~1050 Hz이다.', '기간 환산 0.95238~1.05263 ms. 펄스 높이와 폭은 설명용이다.']),
    ]:
        f=Figure(slug,title,foot,tests)
        f.text(36,146,'출력 전류 (상대값)',26)
        baseline=374
        f.line([(145,baseline),(1060,baseline)],'#BBC2C8',2)
        for i,x in enumerate([190,440,690,940]):
            f.line([(x-23,baseline),(x,baseline),(x,246),(x+26,246),(x+26,baseline),(x+40,baseline),(x+40,486),(x+66,486),(x+66,baseline)],BLUE,4)
            f.text(x-5,523,ticks[i],24,GRAY)
        f.bracket(190,440,210,period)
        f.text(879,553,'시간 ('+('us' if slug=='SYS-01-rate' else 'ms')+')',23,GRAY)
        f.save('같은 조건의 실제 자극 파형에서 시작 간격과 자극률을 비교한다.','시스템 시험 기준서 ID '+('1' if slug=='SYS-01-rate' else '2'))


def polarity():
    f=Figure('SYS-04-phase','맵의 선행 위상과 실제 자극의 방향',[
        '양/음 선행 맵을 각각 적용한다. 0선을 기준으로 첫 위상을 확인한다.',
        '위상 구분용 도식이며 높이, 펄스폭, 위상 간격은 비례하지 않는다.'],sysids(4))
    for y,sign,label in [(240,1,'Positive First'),(467,-1,'Negative First')]:
        f.text(40,y-88,label,28,bold=True)
        f.line([(260,y),(1050,y)],'#ADB7BE',2)
        f.text(225,y-15,'0',23)
        f.line([(270,y),(425,y),(425,y-sign*66),(560,y-sign*66),(560,y),(625,y),(625,y+sign*66),(760,y+sign*66),(760,y),(1040,y)],BLUE,5)
        f.text(448,y-sign*66-37 if sign==1 else y+82,'첫 위상',25,BLUE)
    f.save('맵에 지정한 선행 위상과 파형 방향을 비교한다.','시스템 시험 기준서 ID4')


def width():
    f=Figure('SYS-06-width','펄스폭: 설정값과 허용 범위 비교',[
        '표시한 폭은 한 위상 기준이다. 13 us와 255 us를 각각 설정한 예시다.',
        '두 행의 가로 배율은 다르다. 실제 측정 폭은 결과 칸에 기록한다.'],sysids(6))
    for y,label,expect,bounds in [(270,'최소 설정','13 us','12.35~13.65 us'),(491,'최대 설정','255 us','242.25~267.75 us')]:
        f.text(40,y-97,label,28,bold=True)
        f.line([(260,y),(435,y),(435,y-85),(825,y-85),(825,y),(1050,y)],BLUE,4)
        f.bracket(435,825,y-111,expect)
        f.text(459,y+20,'허용: '+bounds,25,GRAY)
    f.save('설정 펄스폭에 +/-5%를 적용한 경계 예시. 실제 측정값은 아니다.','시스템 시험 기준서 ID6: 13~255 us, +/-5%')


def processing():
    f=Figure('SYS-12-58-cycle','처리 주기와 한 블록의 처리 시간 구분',[
        '시작 간격 0.95~1.05 ms. 처리 완료는 다음 블록 시작보다 먼저다.',
        '처리 막대의 길이는 설명용이다. 처리 시간을 0.95~1.05 ms로 판정하지 않는다.'],sysids(12,58))
    start=[205,575,945]
    f.text(40,139,'연속 블록 시작',27,bold=True)
    for x,label in zip(start,['t0','t1','t2']):
        f.line([(x,260),(x,440)],GRAY,2)
        f.text(x-8,452,label,26)
    f.bracket(start[0],start[1],231,'주기 1.00 ms')
    f.bracket(start[1],start[2],231,'주기 1.00 ms')
    for x,label in zip(start[:2],['블록 0 처리','블록 1 처리']):
        f.rect((x,294,x+253,371),'#E7F1FA')
        f.text(x+34,318,label,25,BLUE)
        f.text(x+120,390,'완료',23,GREEN)
    f.text(197,512,'시작 시각 차이와 완료 순서를 각각 저장하여 비교한다.',26)
    f.save('ID12는 시작 주기, ID58은 시작 주기와 다음 블록 전 완료를 확인한다.','시스템 시험 기준서 ID12/58')


def battery():
    f=Figure('SYS-21-22-62-battery','배터리 잔량 경계와 LED 예상 표시',[
        '내부기/매핑 해제, 표시 허용, 오류/갱신 없음. 충전 시 뚜껑 열림.',
        '색 이름은 기준서 기준이다. 실제 노란색/주황색 구분은 사진과 대조한다.'],sysids(21,22,62))
    f.text(49,143,'잔량',26,bold=True)
    f.text(342,143,'충전 연결',26,bold=True)
    f.text(613,143,'충전 해제',26,bold=True)
    f.text(858,143,'표시',26,bold=True)
    for i,(level,color,label) in enumerate([(80,'#2DAB61','녹색 점등'),(79,'#F1CE32','노란색 점등'),(10,'#F1CE32','노란색 점등'),(9,'#F1CE32','노란색 점멸')]):
        y=230+i*89
        f.text(54,y-17,f'{level}%',30)
        for x in [402,676]:
            f.circle(x,y,25,color)
            if level==9:f.circle(x+65,y,20,'#F4F4F4')
        f.text(853,y-16,label,24)
    f.save('80/79/10/9%의 경계와 충전 연결/해제에서 같은 표시를 비교한다.','시스템 시험 기준서 ID21/22/62')


def start_stop():
    for slug,title,tests,start in [
        ('SYS-67-start','시동/부착: 준비 전 자극 없음',sysids(67),True),
        ('SYS-68-stop','종료/탈착: 출력 종료 후 이상 자극 없음',sysids(68),False),
    ]:
        f=Figure(slug,title,[
            '높이, 폭, 준비 시간과 종료 지연은 수치 기준이 없는 개념도이다.',
            '실제 과도 파형과 원치 않는 자극 유무는 스코프 사진으로 확인한다.'],tests)
        f.text(39,145,'자극 유무',27,bold=True)
        f.line([(160,405),(1045,405)],'#B7BEC4',2)
        f.line([(600,203),(600,493)],GRAY,2)
        xs=range(685,1010,87) if start else range(220,554,87)
        for x in xs:f.line([(x,405),(x,288),(x+23,288),(x+23,468),(x+46,468),(x+46,405)],BLUE,4)
        f.text(190,213,'기동 / 설정 중' if start else '정상 자극 중',28)
        f.text(690,213,'정상 자극 허용' if start else '종료 / 탈착 후',28)
        f.text(208 if start else 710,434,'자극 없음',27,GREEN)
        f.text(490,516,'준비 완료' if start else '종료 경계',26)
        f.save('준비/종료 경계에서 원치 않는 자극이 없는지 비교한다.','시스템 시험 기준서 ID'+('67' if start else '68'))


def integration_signal():
    f=Figure('INT-005-input','입력 블록에서 공유 최대값까지',[
        '나머지 11표본은 0이다. 결과 사용은 HEAR Event 0 완료 이후다.',
        '이후 전부 0인 블록을 처리하면 maxAudioInput은 0으로 갱신된다.'],intids(5))
    for j,t in enumerate(['[0]','[1]','[2]','[3]','[4]']):f.text(335+139*j,154,t,24,GRAY)
    rows=[('단일 입력',[0,-1024,544,-3200,3168]),('혼합 후 >>5',[0,-32,17,-100,99])]
    for i,(label,vals) in enumerate(rows):
        y=217+i*110;f.text(35,y+19,label,26,bold=True)
        for j,val in enumerate(vals):
            x=295+j*146;f.rect((x,y,x+135,y+74),'#F3F5F7');f.text(x+9,y+18,str(val),26)
    f.rect((295,456,1015,539),'#E7F1FA');f.text(334,476,'절대값 최대 maxAudioInput = 100',30,BLUE,bold=True)
    f.save('고정 16표본의 혼합 결과와 공유 최대값을 비교한다.','통합시험 기준서 ID005')


def integration_nofm():
    f=Figure('INT-015-nofm','NofM: 16개 선택과 8개씩 분할 전송',[
        '전극 번호가 아닌 배열 인덱스다. 각 구간 24프레임, 자극당 NOP 2개.',
        '직전 인덱스=15인 조건. 직전=0이면 마지막 15만 맨 앞으로 이동한다.'],intids(15))
    seq=[0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15]
    for g in range(2):
        y=221+g*187;f.text(38,y-64,f'phase {g}: {"첫" if g==0 else "다음"} 1 ms / 8개 자극',28,bold=True)
        for i,v in enumerate(seq[g*8:(g+1)*8]):
            x=47+i*128;f.rect((x,y,x+113,y+84),'#E7F1FA' if g==0 else '#E7F3EC');f.text(x+37,y+24,str(v),28)
    f.save('동률 입력 32개=100에서 낮은 인덱스 16개를 선택한 예상 순서.','통합시험 기준서 ID015')


def system_nofm():
    f=Figure('SYS-11-nofm','NofM: 선택 전극과 실제 출력 순서',[
        '대역별 크기 코드 0~31, 전극=대역 인덱스+1, 직전 인덱스=31인 예시.',
        '전극 1~16은 미선택. 아래 16개 전극만 순서대로 출력되어야 한다.'],sysids(11))
    seq=[17,25,18,26,19,27,20,28,21,29,22,30,23,31,24,32]
    for g in range(2):
        y=221+g*187;f.text(38,y-64,f'{g+1}번째 블록 / 전극 번호',28,bold=True)
        for i,v in enumerate(seq[g*8:(g+1)*8]):
            x=47+i*128;f.rect((x,y,x+113,y+84),'#E7F1FA' if g==0 else '#E7F3EC');f.text(x+37,y+24,str(v),28)
    f.save('보고서의 N=16/M=32 디지털 입력 예시. 실제 음향 입력과 전극 출력도 별도로 확인한다.','시스템 시험 기준서 ID11 및 매핑 후 크기 코드 예시')


if __name__=='__main__':
    OUT.mkdir(parents=True,exist_ok=True)
    stimulus_period();polarity();width();processing();battery();start_stop();integration_signal();integration_nofm();system_nofm()
    (OUT/'manifest.json').write_text(json.dumps(MANIFEST,ensure_ascii=False,indent=2),encoding='utf-8')
    print(len(MANIFEST),'expected figures')
