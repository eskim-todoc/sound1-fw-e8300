# -*- coding: utf-8 -*-
"""Sound1 SAD 블록도 2점 생성 (PIL) - 직교 라우팅"""
import io, os, sys, math
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
from PIL import Image, ImageDraw, ImageFont

FONT_R = 'C:/Windows/Fonts/malgun.ttf'
FONT_B = 'C:/Windows/Fonts/malgunbd.ttf'

C_BG    = (255, 255, 255)
C_LINE  = (60, 60, 60)
C_TEXT  = (20, 20, 20)
C_MUTED = (110, 110, 110)
C_CORE  = (222, 235, 247)
C_CHIP  = (236, 236, 236)
C_OUT   = (250, 243, 226)
C_SCOPE = (246, 246, 246)
C_BOOT  = (231, 245, 233)
C_MOD   = (255, 255, 255)


def F(sz, bold=False):
    return ImageFont.truetype(FONT_B if bold else FONT_R, sz)


def text_center(d, cx, cy, text, font, fill=C_TEXT, lh=1.3):
    lines = text.split('\n')
    hs = [d.textbbox((0, 0), ln or 'A', font=font)[3] - d.textbbox((0, 0), ln or 'A', font=font)[1]
          for ln in lines]
    step = max(hs) * lh
    y = cy - step * (len(lines) - 1) / 2.0
    for ln in lines:
        bb = d.textbbox((0, 0), ln, font=font)
        w = bb[2] - bb[0]
        d.text((cx - w / 2.0 - bb[0], y - (bb[3] - bb[1]) / 2.0 - bb[1]), ln, font=font, fill=fill)
        y += step


def dashed_rect(d, bx, color, width=3, dash=18, gap=12):
    x0, y0, x1, y1 = bx
    def seg(a, b, horiz, fixed):
        p = a
        while p < b:
            q = min(p + dash, b)
            if horiz:
                d.line([(p, fixed), (q, fixed)], fill=color, width=width)
            else:
                d.line([(fixed, p), (fixed, q)], fill=color, width=width)
            p = q + gap
    seg(x0, x1, True, y0); seg(x0, x1, True, y1)
    seg(y0, y1, False, x0); seg(y0, y1, False, x1)


def box(d, x, y, w, h, text, fill=C_CHIP, outline=C_LINE, fs=30, bold=True,
        style='solid', radius=14, tcolor=C_TEXT, ow=3):
    r = (x, y, x + w, y + h)
    if style == 'solid':
        d.rounded_rectangle(r, radius=radius, fill=fill, outline=outline, width=ow)
    else:
        d.rounded_rectangle(r, radius=radius, fill=fill, outline=None)
        dashed_rect(d, r, outline, width=ow,
                    dash=(20 if style == 'dash' else 6),
                    gap=(13 if style == 'dash' else 9))
    text_center(d, x + w / 2.0, y + h / 2.0, text, F(fs, bold), tcolor)


def container(d, x, y, w, h, text, fill, fs=29):
    d.rounded_rectangle((x, y, x + w, y + h), radius=18, fill=fill, outline=C_LINE, width=4)
    d.text((x + 26, y + 18), text, font=F(fs, True), fill=C_TEXT)


def _head(d, px, py, qx, qy, color, width, head):
    ang = math.atan2(qy - py, qx - px)
    for s in (0.45, -0.45):
        d.line([(qx, qy), (qx - head * math.cos(ang + s), qy - head * math.sin(ang + s))],
               fill=color, width=width)


def poly(d, pts, color=C_LINE, width=5, head=18, both=False, style='solid'):
    """직교 폴리라인 + 끝 화살표"""
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        if style == 'solid':
            d.line([a, b], fill=color, width=width)
        else:
            ln = math.hypot(b[0] - a[0], b[1] - a[1])
            if ln == 0:
                continue
            ux, uy = (b[0] - a[0]) / ln, (b[1] - a[1]) / ln
            t = 0.0
            while t < ln:
                t2 = min(t + 18, ln)
                d.line([(a[0] + ux * t, a[1] + uy * t), (a[0] + ux * t2, a[1] + uy * t2)],
                       fill=color, width=width)
                t = t2 + 12
    _head(d, pts[-2][0], pts[-2][1], pts[-1][0], pts[-1][1], color, width, head)
    if both:
        _head(d, pts[1][0], pts[1][1], pts[0][0], pts[0][1], color, width, head)


def elabel(d, cx, cy, text, fs=25, fill=C_MUTED):
    f = F(fs, False)
    bb = d.textbbox((0, 0), text, font=f)
    w, h = bb[2] - bb[0], bb[3] - bb[1]
    d.rectangle((cx - w / 2 - 10, cy - h / 2 - 9, cx + w / 2 + 10, cy + h / 2 + 9), fill=C_BG)
    text_center(d, cx, cy, text, f, fill)


# ==========================================================================
# 그림 1. 인공와우 시스템 블록도
# ==========================================================================
def figure1(path):
    W, H = 2560, 1560
    img = Image.new('RGB', (W, H), C_BG)
    d = ImageDraw.Draw(img)

    container(d, 40, 40, 1690, 1470, '외부기 (Sound1)', (252, 252, 252))

    # 좌측 주변장치
    box(d, 90, 300, 240, 160, '디지털 마이크\nDMIC 0 / DMIC 2', C_CHIP, fs=25)
    box(d, 90, 620, 240, 130, 'EEPROM', C_CHIP, fs=29)
    box(d, 90, 900, 240, 140, '터치 센서\nIQS323', C_CHIP, fs=26)
    box(d, 90, 1180, 240, 130, 'LED', C_CHIP, fs=29)

    # Ezairo 8300
    container(d, 430, 200, 620, 1200, 'Ezairo 8300 (메인 칩셋)', (240, 246, 252))
    box(d, 520, 300, 480, 150, 'CFX Core\n(오디오 신호처리)', C_CORE, fs=27)
    box(d, 520, 490, 480, 160, 'HEAR Core\n(HW 가속\nFunction Chain)', C_CORE, fs=25)
    box(d, 520, 700, 480, 150, 'Cortex-M3\n(시스템 제어)', C_CORE, fs=27)
    box(d, 520, 900, 480, 140, '부트로더 / OTA', C_BOOT, fs=27, style='dash')
    d.text((524, 1075), '※ 파선 = 착탈 가능 항목', font=F(23), fill=C_MUTED)

    # 우측 칩
    box(d, 1150, 260, 440, 240, 'QCC5181\n블루투스 콤보\n(펌웨어 별도 관리)', C_SCOPE, fs=27, style='dot')
    box(d, 1150, 640, 440, 160, 'FPGA', C_CHIP, fs=30)
    box(d, 1150, 900, 440, 160, 'TX PMIC\n(링크 파워)', C_CHIP, fs=27)
    box(d, 1150, 1160, 440, 150, '송신 코일', C_CHIP, fs=28)

    # 외부 개체
    box(d, 1840, 240, 660, 190, '크래들\n(충전 · 뚜껑 개폐)', C_OUT, fs=28)
    box(d, 1840, 520, 660, 210, '리모콘 앱\n매핑 프로그램', C_OUT, fs=28)
    box(d, 1840, 1130, 660, 210, '내부 이식장치\n(Internal Device)', C_OUT, fs=28)

    # ---- 연결 (직교) ----
    poly(d, [(330, 380), (430, 380)]);              elabel(d, 380, 343, 'PDM')
    poly(d, [(330, 685), (430, 685)], both=True);   elabel(d, 380, 648, 'SPI')
    poly(d, [(330, 970), (430, 970)], both=True);   elabel(d, 380, 933, 'I2C')
    poly(d, [(430, 1245), (330, 1245)]);            elabel(d, 380, 1208, 'GPIO')

    poly(d, [(1050, 380), (1150, 380)], both=True); elabel(d, 1100, 340, 'SPI · I2S')
    poly(d, [(1050, 720), (1150, 720)]);            elabel(d, 1100, 683, 'PCM · I2C')

    poly(d, [(1050, 980), (1150, 980)]);            elabel(d, 1100, 943, 'I2C')

    poly(d, [(1370, 800), (1370, 900)]);            elabel(d, 1370, 850, '자극 데이터')
    poly(d, [(1370, 1060), (1370, 1160)]);          elabel(d, 1370, 1110, '링크 구동')

    poly(d, [(1590, 1235), (1840, 1235)], style='dash', width=6)
    elabel(d, 1715, 1190, '경피 유도 링크')

    poly(d, [(1590, 335), (1840, 335)], both=True); elabel(d, 1715, 296, '충전 · 상태')
    poly(d, [(1590, 440), (1786, 440), (1786, 625), (1840, 625)], both=True, style="dash")
    elabel(d, 1786, 532, '블루투스', fs=24)

    img.save(path)
    print('saved', os.path.basename(path), img.size)


# ==========================================================================
# 그림 2. 소프트웨어 아키텍쳐 블록도
# ==========================================================================
def figure2(path):
    W, H = 2560, 1460
    img = Image.new('RGB', (W, H), C_BG)
    d = ImageDraw.Draw(img)

    container(d, 40, 40, 1700, 1370, 'Ezairo 8300 소프트웨어', (251, 251, 251))

    # CFX
    container(d, 90, 130, 800, 640, 'CFX Core', (240, 246, 252), fs=27)
    box(d, 130, 225, 720, 135, '시스템 제어 모듈\n초기화 · 시스템 제어 · 오디오 입력 / 믹서', C_MOD, fs=24)
    box(d, 130, 385, 720, 135, '신호처리 모듈\nAGC · 주파수 분석 · 비선형 매핑 · 자극 기법', C_MOD, fs=24)
    box(d, 130, 545, 720, 135, '내부 자극 장치 실행 모듈\nPCM 제어', C_MOD, fs=24)

    # HEAR
    container(d, 960, 130, 690, 300, 'HEAR Core', (240, 246, 252), fs=27)
    box(d, 1000, 225, 610, 160, 'Function Chain\n(CFX 시스템 제어 항목이 호출)', C_MOD, fs=25)

    # CM3
    container(d, 90, 830, 1560, 530, 'Cortex-M3 (CM3)', (240, 246, 252), fs=27)
    box(d, 130, 915, 730, 130, '시스템 제어 모듈\n초기화 · 전원 · LED · 터치 · 배터리', C_MOD, fs=24)
    box(d, 900, 915, 710, 130, '내부 자극 장치 실행 모듈\n내부기 설정 · 연결 검사 · 매핑 실행', C_MOD, fs=24)
    box(d, 130, 1065, 730, 130, '신호처리 모듈\n자극 파라미터 산출', C_MOD, fs=24)
    box(d, 900, 1065, 710, 130, '블루투스 통신 모듈\n리모콘 · 매핑 명령 처리', C_MOD, fs=24)
    box(d, 130, 1215, 1480, 110, '부트로더 모듈    무결성(CRC) 검증 · 코어 기동 · OTA 갱신',
        C_BOOT, fs=25, style='dash')
    d.text((96, 1372), '※ 파선 = 착탈 가능 항목', font=F(23), fill=C_MUTED)

    # QCC
    container(d, 1800, 40, 720, 1370, 'QCC5181 소프트웨어', (247, 247, 247), fs=27)
    box(d, 1850, 150, 620, 230, '시스템 제어 모듈', C_SCOPE, fs=28, style='dot')
    box(d, 1850, 430, 620, 230, '블루투스 통신 모듈', C_SCOPE, fs=28, style='dot')
    box(d, 1850, 710, 620, 230, '전원 · 배터리 관리 모듈', C_SCOPE, fs=28, style='dot')
    text_center(d, 2160, 1130, '점선 = 본 문서 범위 밖 (펌웨어 별도 관리)\n\n외부기 전체 아키텍쳐로 승격될 때\n이 자리에 모듈 · 항목을 기술한다',
                F(24), C_MUTED)

    # ---- 인터페이스 ----
    poly(d, [(490, 680), (490, 915)], both=True)
    elabel(d, 490, 797, '공유 메모리')

    poly(d, [(850, 292), (1000, 292)], both=True)
    elabel(d, 925, 240, '함수체인 호출', fs=23)

    poly(d, [(890, 610), (1800, 610)], both=True, style='dash')
    elabel(d, 1345, 610, 'I2S (오디오)')

    poly(d, [(1610, 1130), (1770, 1130), (1770, 800), (1800, 800)], both=True)
    elabel(d, 1770, 960, 'SPI')

    img.save(path)
    print('saved', os.path.basename(path), img.size)


if __name__ == '__main__':
    out = u'E:/workspace/projects/sound1-fw-e8300-backup/docs/tasks/인허가/20260909_1129_sad-sound1/그림'
    if not os.path.isdir(out):
        os.makedirs(out)
    figure1(out + '/그림1_시스템_블록도.png')
    figure2(out + '/그림2_소프트웨어_아키텍쳐_블록도.png')
