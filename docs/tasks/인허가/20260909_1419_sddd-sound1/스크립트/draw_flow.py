# -*- coding: utf-8 -*-
"""SDDD 순서도 렌더러 (PIL) - 세로 흐름 + 우측 분기 + 좌측 되돌이"""
import io, os, sys, math
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
from PIL import Image, ImageDraw, ImageFont

FONT_R = 'C:/Windows/Fonts/malgun.ttf'
FONT_B = 'C:/Windows/Fonts/malgunbd.ttf'
BG = (255, 255, 255)
LINE = (55, 55, 55)
TXT = (20, 20, 20)
MUT = (110, 110, 110)
C_TERM = (226, 239, 218)
C_PROC = (255, 255, 255)
C_DEC = (255, 242, 204)
C_SUB = (222, 235, 247)

W = 1560
CX = 620          # 주 흐름 중심
BW, BH = 620, 104  # 기본 박스
DW, DH = 660, 150  # 다이아몬드
GAP = 62
SIDE_X = 1230     # 우측 분기 중심
SIDE_W = 560
LOOP_X = 190      # 좌측 되돌이 선


def F(sz, bold=False):
    return ImageFont.truetype(FONT_B if bold else FONT_R, sz)


def tc(d, cx, cy, text, font, fill=TXT, lh=1.3):
    lines = text.split('\n')
    hs = [d.textbbox((0, 0), ln or 'A', font=font)[3] - d.textbbox((0, 0), ln or 'A', font=font)[1]
          for ln in lines]
    step = max(hs) * lh
    y = cy - step * (len(lines) - 1) / 2.0
    for ln in lines:
        bb = d.textbbox((0, 0), ln, font=font)
        d.text((cx - (bb[2] - bb[0]) / 2.0 - bb[0], y - (bb[3] - bb[1]) / 2.0 - bb[1]),
               ln, font=font, fill=fill)
        y += step


def arrow_head(d, px, py, qx, qy, w=4, h=17):
    ang = math.atan2(qy - py, qx - px)
    for s in (0.45, -0.45):
        d.line([(qx, qy), (qx - h * math.cos(ang + s), qy - h * math.sin(ang + s))],
               fill=LINE, width=w)


def poly(d, pts, w=4, head=True):
    for i in range(len(pts) - 1):
        d.line([pts[i], pts[i + 1]], fill=LINE, width=w)
    if head:
        arrow_head(d, pts[-2][0], pts[-2][1], pts[-1][0], pts[-1][1], w)


def lbl(d, x, y, text, fs=23):
    f = F(fs)
    bb = d.textbbox((0, 0), text, font=f)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    d.rectangle((x - tw / 2 - 8, y - th / 2 - 7, x + tw / 2 + 8, y + th / 2 + 7), fill=BG)
    tc(d, x, y, text, f, MUT)


def draw(items, path, title=None):
    # 높이 산정
    h = 90
    for it in items:
        h += (DH if it['t'] == 'dec' else BH) + GAP
    h += 90
    img = Image.new('RGB', (W, int(h)), BG)
    d = ImageDraw.Draw(img)

    y = 70
    geom = []
    for it in items:
        t = it['t']
        if t == 'dec':
            cy = y + DH / 2
            pts = [(CX, y), (CX + DW / 2, cy), (CX, y + DH), (CX - DW / 2, cy)]
            d.polygon(pts, fill=C_DEC, outline=LINE)
            d.line(pts + [pts[0]], fill=LINE, width=4)
            tc(d, CX, cy, it['text'], F(25, False))
            geom.append((y, y + DH, cy))
            y += DH + GAP
        else:
            fill = C_TERM if t in ('start', 'end') else (C_SUB if t == 'sub' else C_PROC)
            rad = 46 if t in ('start', 'end') else 10
            d.rounded_rectangle((CX - BW / 2, y, CX + BW / 2, y + BH),
                                radius=rad, fill=fill, outline=LINE, width=4)
            if t == 'sub':
                d.line([(CX - BW / 2 + 16, y), (CX - BW / 2 + 16, y + BH)], fill=LINE, width=3)
                d.line([(CX + BW / 2 - 16, y), (CX + BW / 2 - 16, y + BH)], fill=LINE, width=3)
            tc(d, CX, y + BH / 2, it['text'], F(26, t in ('start', 'end')))
            geom.append((y, y + BH, y + BH / 2))
            y += BH + GAP

    # 세로 연결
    for i in range(len(items) - 1):
        poly(d, [(CX, geom[i][1]), (CX, geom[i + 1][0])])
        if items[i]['t'] == 'dec':
            lbl(d, CX + 34, (geom[i][1] + geom[i + 1][0]) / 2, items[i].get('yes', u'예'))

    # 우측 분기
    k = 0
    for i, it in enumerate(items):
        if it['t'] != 'dec' or not it.get('side'):
            continue
        top, bot, cy = geom[i]
        sh = 92
        sy = cy - sh / 2
        term = (it.get('join') == 'end')
        d.rounded_rectangle((SIDE_X - SIDE_W / 2, sy, SIDE_X + SIDE_W / 2, sy + sh),
                            radius=(46 if term else 10),
                            fill=(C_TERM if term else C_PROC), outline=LINE, width=4)
        tc(d, SIDE_X, sy + sh / 2, it['side'], F(24, term))
        poly(d, [(CX + DW / 2, cy), (SIDE_X - SIDE_W / 2, cy)])
        lbl(d, (CX + DW / 2 + SIDE_X - SIDE_W / 2) / 2, cy - 26, it.get('no', u'아니오'))
        # 재합류 지점 (분기마다 복귀선 x 를 어긋나게 하여 겹침 방지)
        j = it.get('join', i + 1)
        if not term and j < len(items):
            rx = SIDE_X + (k % 3 - 1) * 48
            ny = geom[j][0] - GAP / 2
            poly(d, [(SIDE_X, sy + sh), (SIDE_X, sy + sh + 26), (rx, sy + sh + 26),
                     (rx, ny), (CX, ny)], head=False)
            poly(d, [(CX + 40, ny), (CX, ny)])
            k += 1

    # 좌측 되돌이
    for i, it in enumerate(items):
        if 'loop_to' not in it:
            continue
        j = it['loop_to']
        y0 = geom[i][2]
        y1 = geom[j][2]
        poly(d, [(CX - BW / 2, y0), (LOOP_X, y0), (LOOP_X, y1), (CX - BW / 2, y1)])
        lbl(d, LOOP_X, (y0 + y1) / 2, it.get('loop_label', u'반복'), 24)

    img.save(path)
    print('  saved', os.path.basename(path), img.size)


OUT = u'E:/workspace/projects/sound1-fw-e8300-backup/docs/tasks/인허가/20260909_1419_sddd-sound1/그림'
if not os.path.isdir(OUT):
    os.makedirs(OUT)

# ---------------------------------------------------------------- SI-ID-CFX-001
draw([
    {'t': 'start', 'text': u'부트로더에서 진입'},
    {'t': 'proc', 'text': u'포화 연산 모드 설정\n인터럽트 설정 및 플래그 해제'},
    {'t': 'dec', 'text': u'반복 실행 허가?', 'side': u'대기 (자극 출력 없음)', 'no': u'아니오', 'join': 1},
    {'t': 'proc', 'text': u'HEAR 정지 및 리셋'},
    {'t': 'proc', 'text': u'HEAR 함수체인 활성화 및\n우선순위 설정 후 기동'},
    {'t': 'sub', 'text': u'오디오 경로 일괄 설정'},
    {'t': 'proc', 'text': u'PCM 및 I2S 인터페이스 초기화\nDIO 설정'},
    {'t': 'proc', 'text': u'마이크 2채널 · I2S · PCM 출력 활성화'},
    {'t': 'proc', 'text': u'출력단 설정\nPCM 상태 및 공유 플래그 초기화'},
    {'t': 'end', 'text': u'주 제어 항목으로'},
], OUT + u'/그림3_CFX_시스템초기화.png')

# ---------------------------------------------------------------- SI-ID-CFX-002
draw([
    {'t': 'start', 'text': u'주 제어 시작'},
    {'t': 'dec', 'text': u'저전력 진입 명령?', 'yes': u'아니오', 'side': u'저전력 대기 항목으로',
     'no': u'예', 'join': 'end'},
    {'t': 'dec', 'text': u'I2S 플래그 활성?', 'side': u'스트리밍 해제 · 버퍼 초기화\n마이크 2채널 복귀',
     'no': u'아니오', 'join': 4},
    {'t': 'proc', 'text': u'I2S 수신 블록 복사\n스트리밍 상태 설정 · 마이크 1채널 전환'},
    {'t': 'dec', 'text': u'입력 인터럽트 모두 도착?',
     'side': u'인터럽트 대기', 'no': u'아니오', 'join': 7},
    {'t': 'sub', 'text': u'마이크 버퍼 시프트 복사'},
    {'t': 'sub', 'text': u'시스템 이벤트 처리'},
    {'t': 'sub', 'text': u'신호 디스패치 / 자극 데이터 출력'},
    {'t': 'proc', 'text': u'인터럽트 대기', 'loop_to': 1, 'loop_label': u'다음 블록'},
], OUT + u'/그림4_CFX_주제어.png')

# ---------------------------------------------------------------- SI-ID-CFX-003
draw([
    {'t': 'start', 'text': u'저전력 진입 명령 수신'},
    {'t': 'proc', 'text': u'PCM 출력 정지\n출력 모드를 영 채움으로 변경'},
    {'t': 'proc', 'text': u'출력단 · I2S · ADC · 마이크 비활성화'},
    {'t': 'proc', 'text': u'IOC 및 FIFO 비활성화\nHEAR 정지'},
    {'t': 'proc', 'text': u'PCM 및 I2S DIO 복원\n신호원 고정 레벨 설정'},
    {'t': 'proc', 'text': u'대기 모드 인터럽트 전환\n저전력 진입 명령 해제'},
    {'t': 'dec', 'text': u'기상 인터럽트?', 'side': u'미처리 인터럽트 해제 후 대기',
     'no': u'아니오', 'join': 6},
    {'t': 'end', 'text': u'주 제어 항목으로 복귀'},
], OUT + u'/그림5_CFX_저전력대기.png')

# ---------------------------------------------------------------- SI-ID-CFX-004
draw([
    {'t': 'start', 'text': u'신호 디스패치 시작'},
    {'t': 'dec', 'text': u'매핑 프로그램 연결?', 'yes': u'예',
     'side': u'사용자 설정 게인 조회', 'no': u'아니오', 'join': 2},
    {'t': 'proc', 'text': u'두 경로 모두 기준 게인 적용'},
    {'t': 'dec', 'text': u'I2S 스트리밍 중?',
     'side': u'마이크 개수 확인', 'no': u'아니오', 'join': 5},
    {'t': 'proc', 'text': u'게인 적용 2버퍼 믹싱\n출력 버퍼 인덱스 교대'},
    {'t': 'proc', 'text': u'1채널: 단일 버퍼 믹싱\n2채널: 전방 마이크 기준 지향성 믹싱'},
    {'t': 'end', 'text': u'자동 이득 제어 전처리 체인 호출'},
], OUT + u'/그림6_CFX_신호디스패치.png')

# ---------------------------------------------------------------- SI-ID-CFX-005
draw([
    {'t': 'start', 'text': u'전처리 체인 완료 인터럽트'},
    {'t': 'sub', 'text': u'자동 이득 제어\n(목표 이득 결정 · 시정수 적용 · 이득 적용)'},
    {'t': 'proc', 'text': u'주파수 분석 입력 갱신'},
    {'t': 'sub', 'text': u'주파수 변환 체인 호출'},
    {'t': 'proc', 'text': u'분석 대역 대표값 산출'},
    {'t': 'proc', 'text': u'비선형 매핑 적용'},
    {'t': 'dec', 'text': u'자극 기법?', 'yes': u'NofM',
     'side': u'CIS: 지정 순서로 전 대역 자극', 'no': u'CIS', 'join': 8},
    {'t': 'proc', 'text': u'NofM: 피크 선택 · 인터리빙\n패킷 생성 · PCM 기록'},
    {'t': 'end', 'text': u'자극 데이터 완성'},
], OUT + u'/그림7_CFX_신호처리실행.png')

# ---------------------------------------------------------------- SI-ID-CFX-006
draw([
    {'t': 'start', 'text': u'PCM 출력 실행'},
    {'t': 'proc', 'text': u'공유 메모리에서 PCM 출력 모드 읽기'},
    {'t': 'dec', 'text': u'충전기 연결?', 'yes': u'아니오',
     'side': u'출력하지 않음', 'no': u'예', 'join': 6},
    {'t': 'dec', 'text': u'출력 모드?', 'yes': u'실시간 자극',
     'side': u'프리앰블 / 특정 명령 / 대기 /\n연결 검사 / 영 채움 생성', 'no': u'그 외', 'join': 5},
    {'t': 'proc', 'text': u'실시간 자극 비트스트림 생성'},
    {'t': 'proc', 'text': u'PCM 출력 버퍼에 기록'},
    {'t': 'end', 'text': u'종료'},
], OUT + u'/그림8_CFX_PCM제어.png')
