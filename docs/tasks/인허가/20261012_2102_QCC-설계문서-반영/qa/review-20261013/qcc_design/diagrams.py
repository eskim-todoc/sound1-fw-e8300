"""Recreate diagrams from drawing code; no source bitmap is loaded or edited.

DMIC2 is wired to E8300 and QCC (user confirmation 2026-10-13).
QCC captures DMIC2 for phone-call uplink. QCC pins/codecs remain unconfirmed.
"""
from pathlib import Path
import inspect, sys

ROOT=Path(__file__).resolve().parent
TASK=ROOT.parents[2]
SOURCE=TASK.parent/'20260929_1755_DHF-펌웨어-설계출력'
sys.path.insert(0,str(SOURCE))
import draw_architecture as original

OUT=ROOT/'figures'
OUT.mkdir(parents=True,exist_ok=True)

class Canvas(original.Canvas):
    def save(self,name):self.im.save(OUT/name,dpi=(300,300))

def hardware():
    source=inspect.getsource(original.hardware)
    source=source.replace("'마이크 1',fill='#e4eef7',size=43", "'DMIC1\\n마이크 1',fill='#e4eef7',size=35")
    source=source.replace("'마이크 2',fill='#e4eef7',size=43", "'DMIC2\\n마이크 2',fill='#e4eef7',size=35")
    old="c.box((65,630,330,960),'QCC5181\\n블루투스\\n오디오 / 상태',fill='#e2eef8',size=37,bold=True)"
    new="c.box((65,630,330,960),'QCC5181\\n블루투스\\n오디오 송수신',fill='#e2eef8',size=34,bold=True)\n    c.arrow([(180,495),(180,630)])\n    c.text((276,559),'통화\\n마이크 입력',27)"
    assert old in source
    source=source.replace(old,new)
    source=source.replace("c.text((390,603),'I2S',31)","c.text((360,605),'I2S',26)")
    source=source.replace("c.text((1215,675),'I2C',32)","c.text((1197,675),'I2C',32)")
    source=source.replace("c.text((1255,535),'I2C',32)","c.text((1280,535),'I2C',32)")
    source=source.replace("c.text((280,1283),'블루투스',33)","c.text((280,1268),'블루투스',33)\n    c.text((430,1315),'통화 상향 / 오디오 하향',27)")
    (OUT/'hardware_generation_source.py').write_text(source,encoding='utf-8')
    env={'Canvas':Canvas}
    exec(compile(source,str(OUT/'hardware_generation_source.py'),'exec'),env)
    env['hardware']()

def software():
    c=Canvas((2100,1470));xs=[(55,500),(555,1040),(1090,1575),(1625,2045)]
    c.text((1300,43),'E8300',45,True);c.d.line((555,80,2045,80),fill='#42627f',width=3)
    for (x0,x1),title in zip(xs,['QCC5181','Cortex-M3','CFX','HEAR']):
        c.dash((x0,105,x1,1190),color='#8693a1');c.text(((x0+x1)/2,153),title,45,True)
    c.text((277,202),'별도 칩 / 4개 기능 모듈',29)
    for rect,label,fill in [
        ((75,260,480,440),'시스템 관리\n초기화 / 상태 / 장치\n4유닛','#edf0f4'),
        ((75,475,480,655),'HW 인터페이스\nPIO / 전원 / NVM\n3유닛','#edf0f4'),
        ((75,690,480,870),'블루투스\n수신 오디오 / 통화 송신\nBLE / AUTH / 4유닛','#dceaf6'),
        ((75,905,480,1085),'보안\nCRA / toolbox / NVM\n3유닛','#f6f9fc')]:c.box(rect,label,fill=fill,size=31)
    c.text((277,1130),'총 14유닛',29);c.text((277,1170),'기능 설계 구성',29)
    def band(y0,y1,x0,x1,label,fill,entries):
        c.box((x0,y0,x1,y1),fill=fill,radius=4);c.text(((x0+x1)/2,y0+38),label,40,True)
        for col,value in entries:
            a,b=xs[col]
            if a-25>x0:c.d.line((a-25,y0+72,a-25,y1-16),fill='#9ca9b4',width=2)
            c.text(((a+b)/2,y0+126),value,33)
    band(245,450,535,2065,'시스템 제어 모듈','#edf0f4',[(1,'기동 / 전원 / 터치 / LED\n맵 / 사용자 설정 관리'),(2,'오디오 / PCM 설정\n블록 처리 순서 제어'),(3,'기동 / 정지 대상\nCFX가 제어')])
    band(480,690,535,2065,'신호처리 모듈','#dcecd5',[(1,'펄스폭 / T / C\n자극 파라미터 계산'),(2,'혼합 / AGC / 대역 분석\n매핑 / CIS / NofM'),(3,'절대값 / 최댓값\n윈도 FFT / 크기')])
    band(720,925,535,1595,'내부 자극 장치 실행 모듈','#fff0ca',[(1,'PMIC / FPGA / 내부기\n설정 / 연결 확인'),(2,'PCM 모드 처리\n자극 / 설정 패킷 출력')])
    band(955,1160,535,1060,'블루투스 통신 모듈','#dceaf6',[(1,'리모콘 / 매핑 명령\n응답 / OTA 데이터 수신')])
    c.d.rectangle((1110,975,2030,1180),fill='white')
    c.text((1590,999),'CM3 ↔ CFX : 공유 메모리 / 상태',29)
    c.text((1590,1052),'CFX ↔ HEAR : 호출 / 완료 이벤트',29)
    c.text((1590,1105),'QCC → CFX : I2S 수신 오디오',29)
    c.text((1590,1158),'DMIC2 → QCC → 통화 상대 : 송신',29)
    c.box((535,1230,2065,1350),'기동 단계  |  CM3 부트로더\n이미지 검사 / 적재 → CFX 및 CM3 응용 프로그램 실행',fill='#f7f7f7',size=35)
    c.text((265,1270),'QCC ↔ CM3\nSPI / GPIO',32)
    c.text((1050,1418),'점선 세로 영역 = 칩 / 코어 위치     E8300 가로 색 영역 = 여러 코어가 나누는 기능',34)
    c.save('소프트웨어 구조도.png')

def bluetooth():
    c=Canvas((2100,1200));c.text((1050,55),'블루투스 통신과 E8300 연계',44,True)
    for (left,right),name in zip([(40,650),(750,1360),(1460,2070)],['외부 앱 / 매핑 프로그램','QCC5181 (별도 칩)','Cortex-M3 (E8300)']):
        c.dash((left,115,right,1100));c.text(((left+right)/2,165),name,40,True)
    c.box((25,225,2085,315),fill='#dceaf6',radius=5);c.text((1050,270),'블루투스 통신 모듈 / 명령 전달 및 응답',39,True)
    c.box((90,395,600,575),'리모콘 / 매핑 명령\n응답 확인',size=35)
    c.box((800,395,1310,575),'블루투스 연결\n명령 / 응답 전달',size=35)
    c.box((1510,395,2020,575),'SPI 수신 / 명령 해석\n처리 / 응답 준비',size=34)
    c.arrow([(600,485),(800,485)],both=True);c.text((700,375),'블루투스',28)
    c.arrow([(1310,485),(1510,485)],both=True);c.text((1410,375),'SPI',32)
    c.box((1510,690,2020,900),'리모콘 / 맵 / 시스템 상태\nOTA 데이터 수신 및 저장\n공유 메모리에 설정 반영',size=32)
    c.arrow([(1765,575),(1765,690)],both=True)
    c.box((800,690,1310,900),'시스템 / HW / Bluetooth / 보안\n4모듈 / 14유닛\n상태 / 명령 / 오디오 연계',fill='#f6f9fc',size=31)
    c.box((90,935,2020,1087),'DMIC2 → QCC → 통화 상대 : 통화 상향\nQCC → CFX : I2S 수신 오디오 / CM3 → QCC : ISD_CHECK 및 QCC_CTRL',fill='#edf0f4',size=30)
    c.text((1050,1150),'무선 송수신 / SPI 제어 / I2S 입력은 서로 다른 인터페이스 경로다.',31)
    c.save('SDDD 블루투스 연계.png')

if __name__=='__main__':hardware();software();bluetooth()
