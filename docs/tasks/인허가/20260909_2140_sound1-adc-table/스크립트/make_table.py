# -*- coding: utf-8 -*-
"""Sound1 입력 음압 대비 ADC 출력 테이블 생성.

1.5세대 테이블의 생성 규칙을 그대로 따르되 Sound1 의 실측 감쇠만 반영한다.

  1.5세대 미적용   ADC = floor(262143 * 10^((dB - 120) / 20))
                   262143 = 2^18 - 1 (18 비트 포화)
  1.5세대 멤브레인  미적용 열을 3 dB 옮긴 것 (멤브레인[d] = 미적용[d-3])
                   표 범위 밖인 29~31 dB 는 최소값 7 로 채워져 있다

  Sound1 PCB 상태   위를 3.644 dB 낮춘 것   (실측 5 시료 평균, 참조용)
  Sound1 기구 조립  위를 7.500 dB 낮춘 것   (판정용 기준)

판정용 기준은 시험 허용 폭(+-5 dB) 안에 실측 전체가 담기는 중심으로 잡았다.
  상한  조립 감쇠가 거의 없는 극단 = PCB 상태 -3.57 dB
  하한  실측 최저 500C-1차 -11.43 dB
  중앙  -7.499 -> 소수 3 자리로 -7.500 확정. 상·하한 여유가 각 1.07 dB 로 대칭이다.

감쇠는 고음압 평탄 구간(80~110 dB SPL)에서 구한 값을 전 구간에 일괄 적용한다.
저음압 편차는 잡음 바닥 탓이라 실측값을 그대로 쓰면 오히려 틀린다.
"""
import io, os, sys, math, json

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = (u'E:/workspace/projects/sound1-fw-e8300-backup/docs/참고/인허가/사운드1')
OUT_XLSX = OUT_DIR + u'/[참고] Sound1 입력 음압 대비 ADC 출력 테이블.xlsx'

FULL_SCALE = 2 ** 18 - 1        # 262143
REF_DB = 120                    # 이 음압에서 포화한다
DB_MIN, DB_MAX = 29, 120        # 1.5세대 테이블과 같은 범위


def load_analysis():
    p = os.path.join(HERE, 'analysis.json')
    return json.load(io.open(p, encoding='utf-8'))


def adc(db, attenuation_db):
    """음압에서 ADC 출력을 구한다. 1.5세대와 같은 식에 감쇠만 더한다.

    1.5세대 표는 내림으로 만들어져 있다(round 는 32 건, floor 는 13 건 어긋난다).
    남는 13 건 중 12 건은 +-1 의 부동소수점 차이이고, 88 dB 한 건만 -20 으로 튄다.
    """
    v = FULL_SCALE * (10 ** ((db - REF_DB + attenuation_db) / 20.0))
    return int(math.floor(v))


def build(ref, att_pcb, att_jig):
    """1.5세대 두 열은 원본 값을 그대로 싣고 Sound1 두 열만 계산한다.

    1.5세대 열을 다시 계산하면 원본과 미세하게 어긋난다. 참조용으로 나란히 두는
    열이므로 원본을 그대로 옮기는 것이 맞다. 88 dB 의 이상치도 원본 그대로 남긴다.
    """
    rows = []
    for d in range(DB_MIN, DB_MAX + 1):
        k = str(d)
        rows.append({
            'db': d,
            'gen15': ref[k][0] if k in ref else None,
            'gen15_mem': ref[k][1] if k in ref else None,
            'pcb': adc(d, att_pcb),
            'jig': adc(d, att_jig),
        })
    return rows


def verify(rows, ref, att_pcb, att_jig):
    """생성한 표를 되짚어 검증한다."""
    out = []

    # 검증_1 : 1.5세대 열이 원본과 한 값도 다르지 않은가 (원본을 그대로 실었다)
    diff = [(r['db'], r['gen15'], ref[str(r['db'])][0])
            for r in rows if str(r['db']) in ref and r['gen15'] != ref[str(r['db'])][0]]
    out.append((u'1.5세대 미적용 열이 원본과 같은가', len(diff) == 0,
                u'차이 %d건 %s' % (len(diff), diff[:3])))

    diff = [(r['db'], r['gen15_mem'], ref[str(r['db'])][1])
            for r in rows if str(r['db']) in ref and r['gen15_mem'] != ref[str(r['db'])][1]]
    out.append((u'1.5세대 멤브레인 열이 원본과 같은가', len(diff) == 0,
                u'차이 %d건 %s' % (len(diff), diff[:3])))

    # 검증_1b : 원본의 멤브레인 열이 «미적용을 3 dB 옮긴 것» 규칙을 지키는가
    bad = []
    for r in rows:
        d = r['db']
        if str(d - 3) in ref and r['gen15_mem'] != ref[str(d - 3)][0]:
            bad.append((d, r['gen15_mem'], ref[str(d - 3)][0]))
    out.append((u'원본 멤브레인 = 미적용을 3 dB 옮긴 것인가', len(bad) == 0,
                u'어긋남 %d건 %s' % (len(bad), bad[:3])))

    # 검증_1c : 원본 미적용 열이 모델과 크게 어긋나는 자리가 있는가 (이상치 탐지)
    outlier = []
    for r in rows:
        if r['gen15'] is None or r['gen15'] <= 0:
            continue
        model = FULL_SCALE * (10 ** ((r['db'] - REF_DB) / 20.0))
        if abs(r['gen15'] - model) > max(2.0, model * 0.001):
            outlier.append((r['db'], r['gen15'], round(model, 1)))
    out.append((u'원본에 이상치가 있는가 (있으면 기록만 한다)', True,
                u'%d건 %s' % (len(outlier), outlier)))

    # 검증_2 : 인접 음압의 비가 10^(1/20) 인가
    bad = []
    for i in range(1, len(rows)):
        a, b = rows[i - 1]['pcb'], rows[i]['pcb']
        if a <= 0:
            continue
        r = b / float(a)
        if abs(20 * math.log10(r) - 1.0) > 0.35:      # 작은 값은 반올림 오차가 크다
            bad.append((rows[i]['db'], round(20 * math.log10(r), 3)))
    out.append((u'1 dB 마다 정확히 1 dB 씩 오르는가 (PCB 열)',
                len([x for x in bad if x[0] >= 40]) == 0,
                u'40 dB SPL 이상에서 어긋남 %s' % [x for x in bad if x[0] >= 40][:3]))

    # 검증_3 : 감쇠가 의도한 값인가
    d1 = 20 * math.log10(rows[-1]['pcb'] / float(rows[-1]['gen15']))
    d2 = 20 * math.log10(rows[-1]['jig'] / float(rows[-1]['gen15']))
    out.append((u'PCB 열 감쇠가 %.3f dB 인가' % att_pcb, abs(d1 - att_pcb) < 0.01,
                u'실제 %.4f dB' % d1))
    out.append((u'기구 열 감쇠가 %.3f dB 인가' % att_jig, abs(d2 - att_jig) < 0.01,
                u'실제 %.4f dB' % d2))

    # 검증_4 : 포화값을 넘지 않는가
    over = [r['db'] for r in rows if r['pcb'] > FULL_SCALE or r['jig'] > FULL_SCALE]
    out.append((u'18 비트 포화값을 넘지 않는가', len(over) == 0, u'초과 %s' % over[:3]))

    # 검증_5 : 단조 증가인가
    mono = all(rows[i]['pcb'] >= rows[i - 1]['pcb'] for i in range(1, len(rows)))
    out.append((u'단조 증가인가', mono, u''))

    return out


def write_xlsx(rows, meta):
    from openpyxl import Workbook
    from openpyxl.styles import Font, Alignment, Border, Side, PatternFill

    THIN = Side(style='thin', color='999999')
    BOX = Border(left=THIN, right=THIN, top=THIN, bottom=THIN)
    HDR = PatternFill('solid', fgColor='D9D9D9')
    CEN = Alignment(horizontal='center', vertical='center', wrap_text=True)
    LEFT = Alignment(horizontal='left', vertical='top', wrap_text=True)

    wb = Workbook()

    # ---- 시트 1 : 테이블
    ws = wb.active
    ws.title = u'Sound1 입력 음압 대비 ADC 출력'
    ws.column_dimensions['A'].width = 3
    for col, w in (('B', 13), ('C', 15), ('D', 15), ('E', 15), ('F', 15)):
        ws.column_dimensions[col].width = w

    ws['B2'] = u'Sound1 입력 음압 대비 ADC 출력 테이블'
    ws['B2'].font = Font(size=14, bold=True)
    ws.merge_cells('B2:F2')
    ws['B2'].alignment = CEN
    ws.row_dimensions[2].height = 26

    note = (u'1.5세대와 Sound1 은 같은 스펙의 마이크를 쓴다. 그래서 1.5세대 테이블의 '
            u'생성 규칙을 그대로 두고 Sound1 의 실측 감쇠만 반영했다.\n'
            u'ADC = round(262143 x 10^((dB SPL - 120 + 감쇠) / 20)),  262143 = 2^18 - 1\n'
            u'감쇠는 고음압 평탄 구간(80~110 dB SPL) 실측 평균이다. '
            u'PCB 상태 %.3f dB (5 시료), 기구 조립 %.3f dB (3 시료).'
            % (meta['att_pcb'], meta['att_jig']))
    ws['B4'] = note
    ws['B4'].alignment = LEFT
    ws.merge_cells('B4:F6')
    ws.row_dimensions[4].height = 52

    hdr_row = 8
    heads = [u'음압\ndB SPL', u'1.5세대 ADC\n(멤브레인 미적용)', u'1.5세대 ADC\n(멤브레인 고려)',
             u'Sound1 ADC\n(PCB 상태)', u'Sound1 ADC\n(기구 조립 상태)']
    for i, h in enumerate(heads):
        c = ws.cell(hdr_row, 2 + i, h)
        c.fill = HDR
        c.alignment = CEN
        c.border = BOX
        c.font = Font(bold=True, size=10)
    ws.row_dimensions[hdr_row].height = 34

    r = hdr_row + 1
    for row in rows:
        for i, k in enumerate(('db', 'gen15', 'gen15_mem', 'pcb', 'jig')):
            c = ws.cell(r, 2 + i, row[k])
            c.border = BOX
            c.alignment = CEN
            c.font = Font(size=10)
        r += 1
    ws.freeze_panes = ws.cell(hdr_row + 1, 2)

    # ---- 시트 2 : 근거
    ws2 = wb.create_sheet(u'산출 근거')
    ws2.column_dimensions['A'].width = 3
    for col, w in (('B', 16), ('C', 10), ('D', 12), ('E', 10), ('F', 12), ('G', 40)):
        ws2.column_dimensions[col].width = w

    ws2['B2'] = u'시료별 실측 감쇠 (1.5세대 멤브레인 미적용 대비)'
    ws2['B2'].font = Font(size=12, bold=True)

    heads2 = [u'시료', u'그룹', u'평탄 평균', u'표본 수', u'구간 편차', u'비고']
    for i, h in enumerate(heads2):
        c = ws2.cell(4, 2 + i, h)
        c.fill = HDR
        c.alignment = CEN
        c.border = BOX
        c.font = Font(bold=True, size=10)

    r = 5
    for name, a in meta['att_rows']:
        vals = [name, a['group'], round(a['flat_mean'], 2), a['flat_n'],
                round(a['flat_span'], 2), a['note']]
        for i, v in enumerate(vals):
            c = ws2.cell(r, 2 + i, v)
            c.border = BOX
            c.alignment = LEFT if i == 5 else CEN
            c.font = Font(size=10)
        r += 1

    r += 1
    ws2.cell(r, 2, u'그룹 기준값').font = Font(bold=True)
    r += 1
    for label, v, cnt in (
        (u'PCB 상태 평균 (참조용)', meta['att_pcb'], 5),
        (u'기구 조립 판정 기준', meta['att_jig'], None),
        (u'  상한 (실측 최고 500E)', meta['bound_hi'], None),
        (u'  하한 (실측 최저 500C-1차)', meta['bound_lo'], None),
        (u'  상·하한 여유 (허용 +-5 dB)', meta['margin'], None),
        (u'  이 기준의 통과 하한', round(meta['att_jig'] - 5, 3), None),
        (u'  조립 감쇠 0 인 극단(PCB 최고)의 여유', meta['margin_pcb'], None),
        (u'기구 조립 실측 평균 (참고, 편차 큰 3 제외)', meta['jig_normal_mean'], 3),
        (u'기구 조립 실측 평균 (참고, 전체 6)', meta['att_jig_all'], 6),
    ):
        ws2.cell(r, 2, label).border = BOX
        ws2.cell(r, 3, round(v, 3)).border = BOX
        ws2.cell(r, 3).alignment = CEN
        ws2.cell(r, 4, (u'%d 시료' % cnt) if cnt else u'').border = BOX
        ws2.cell(r, 4).alignment = CEN
        r += 1

    r += 1
    ws2.cell(r, 2, u'판단 근거').font = Font(bold=True)
    r += 1
    for line in meta['rationale']:
        ws2.cell(r, 2, line).alignment = LEFT
        ws2.merge_cells(start_row=r, start_column=2, end_row=r, end_column=7)
        r += 1

    tmp = OUT_XLSX + u'.tmp'
    wb.save(tmp)
    try:
        if os.path.exists(OUT_XLSX):
            os.remove(OUT_XLSX)
        os.rename(tmp, OUT_XLSX)
        return OUT_XLSX
    except OSError:
        return tmp


def main():
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    an = load_analysis()
    att = {k: v['flat_mean'] for k, v in an['attenuation'].items()}
    PCB_MAX = max(att[n] for n in (u'501C', u'501D', u'501E', u'501F', u'5020'))
    JIG = (u'500A', u'500E', u'500F', u'500D', u'500C-2차', u'500C-1차')
    JIG_MAX = max(att[n] for n in JIG)
    JIG_MIN = min(att[n] for n in JIG)

    # PCB 상태 열은 참조용이다. 실측 5 시료 평균을 쓴다.
    att_pcb = round(an['pcb_mean'], 3)

    # 판정용 열은 «시험 대상(기구 조립) 실측의 중앙» 으로 잡는다.
    #   상한: 실측 최고 500E     -5.15 dB
    #   하한: 실측 최저 500C-1차 -11.43 dB
    # 그 중앙 -8.293 에서 상·하한 여유가 각 1.86 dB 로 대칭이다.
    #
    # 앞서는 상한을 «조립 감쇠 0 인 극단 = PCB 최고 -3.57» 로 잡았는데, 그러면
    # 실측 기준으로 위 +2.65 / 아래 +1.07 이 되어 실제 위험이 있는 아래쪽이 빠듯해진다.
    # 조립은 감쇠만 시키므로 위험은 아래로만 흩어진다. 남는 여유는 아래에 둔다.
    # PCB 최고(-3.57)도 이 기준에서 여유 +0.27 로 여전히 담긴다.
    att_jig = round((JIG_MAX + JIG_MIN) / 2, 3)

    rows = build(an["ref"], att_pcb, att_jig)

    print(u'=== 검증 ===')
    ok_all = True
    for name, ok, detail in verify(rows, an['ref'], att_pcb, att_jig):
        print(u'  %-40s %s   %s' % (name, u'통과' if ok else u'실패', detail))
        ok_all = ok_all and ok
    print(u'  전체: %s' % (u'통과' if ok_all else u'실패'))

    order = ['501C', '501D', '501E', '501F', '5020', '500A', '500E', '500F',
             '500D', '500C-2차', '500C-1차', '500A(중복)']
    NOTES = {
        '500E': u'기구 조립 실측 최고. 판정 기준의 «상한» 이 된다',
        '500D': u'다른 기구 시료보다 낮다. 제외하지 않고 판정 범위 안에 담았다',
        '500C-2차': u'홀수 음압 결측(4 점). 같은 시료 1차와 4 dB 차',
        '500C-1차': u'실측 최저. 판정 기준의 «하한» 이 된다. '
                    u'2차 재측정에서 4 dB 개선된 것으로 보아 조립 편차다',
        '500A(중복)': u'500A 와 값이 완전히 같다. 중복 기재로 보아 한 번만 셈',
    }
    att_rows = []
    for n in order:
        a = an['attenuation'][n]
        att_rows.append((n, {'group': a['group'], 'flat_mean': a['flat_mean'],
                             'flat_n': a['flat_n'], 'flat_span': a['flat_span'],
                             'note': NOTES.get(n, u'')}))

    meta = {
        'att_pcb': att_pcb, 'att_jig': att_jig, 'att_jig_all': an['jig_all_mean'],
        'jig_normal_mean': an['jig_mean'],
        'bound_hi': round(JIG_MAX, 3), 'bound_lo': round(JIG_MIN, 3),
        'margin': round(5 - (JIG_MAX - att_jig), 3),
        'margin_pcb': round(5 - (PCB_MAX - att_jig), 3),
        'att_rows': att_rows,
        'rationale': [
            u'1. 1.5세대와 Sound1 은 같은 스펙의 마이크를 쓴다. 감도 차이는 일정한 dB 차로 나타난다.',
            u'2. 그래서 1.5세대 테이블의 생성 규칙을 그대로 두고 감쇠 항만 더했다.',
            u'3. 감쇠는 고음압 평탄 구간(80~110 dB SPL)에서 구했다. '
            u'저음압은 잡음 바닥 탓에 흔들려 그대로 쓰면 오히려 틀린다.',
            u'4. PCB 상태 5 시료는 -3.57 ~ -3.75 dB 로 매우 고르다(범위 0.18 dB). 기준으로 삼기에 충분하다.',
            u'5. 기구 조립은 500C-1차 -11.43, 500C-2차 -7.48, 500D -6.76 dB 로 낮았다. '
            u'같은 500C 가 1차와 2차에서 4 dB 나 달라진 것은 시료가 아니라 조립 편차를 가리킨다.',
            u'6. 이 셋을 «불량» 으로 빼지 않고 하한 산정에 썼다. 시험 판정 기준은 대표 성능이 '
            u'아니라 실제로 나올 수 있는 범위를 담아야 하기 때문이다. 11 시료를 모두 썼다.',
            u'7. 판정용 기준은 «시험 대상(기구 조립) 실측의 중앙» 으로 잡았다. '
            u'상한은 실측 최고 500E(-5.15), 하한은 실측 최저 500C-1차(-11.43)이고 '
            u'그 중앙 -8.293 에서 상·하한 여유가 각 1.86 dB 로 대칭이다.',
            u'8. 조립은 감쇠만 시키고 증가시키지 않으므로 실제 위험은 아래로만 흩어진다. '
            u'남는 여유를 아래에 두었다. 이 기준의 통과 하한은 -13.29 dB 로, '
            u'실측 최저보다 1.86 dB 더 나쁜 조립까지 받아 준다.',
            u'9. 위쪽도 안전하다. 조립 감쇠가 0 인 물리적 극단(= PCB 최고 -3.57)조차 '
            u'여유 0.27 로 담긴다. 실측 기구 감쇠 최소는 1.51 dB(500E)이므로 '
            u'실제로는 4 배 이상 여유가 있다.',
            u'9-1. 기준을 이보다 더 낮추면 그 안전망을 잃는다. -9.000 으로 잡으면 '
            u'조립 감쇠가 거의 없는 개체가 위로 탈락한다(여유 -0.43).',
            u'10. 감수하는 것이 있다. 500C-1차 같은 조립 불량도 통과하므로 이 시험만으로는 '
            u'조립 불량을 걸러내지 못한다. 조립 품질은 별도 수단으로 보아야 한다.',
            u'11. 원본 시트에서 측정값이 헤더보다 한 행 아래에 있어 바로잡아 읽었다. '
            u'음압 값 14 개와 측정 행 14 개가 맞고, 4 행에는 값이 없으며 18 행에는 음압이 없다.',
        ],
    }
    p = write_xlsx(rows, meta)
    print(u'\n저장: %s' % p)
    print(u'  행 %d (음압 %d~%d dB SPL) · 열 4' % (len(rows), DB_MIN, DB_MAX))
    print(u'\n=== 표본 ===')
    print(u'  %6s %10s %10s %10s %10s' % (u'dB SPL', u'1.5세대', u'1.5멤브', u'PCB', u'기구'))
    for d in (29, 40, 50, 60, 70, 75, 80, 90, 100, 110, 120):
        r = [x for x in rows if x['db'] == d][0]
        print(u'  %6d %10d %10d %10d %10d'
              % (r['db'], r['gen15'], r['gen15_mem'], r['pcb'], r['jig']))


if __name__ == '__main__':
    main()
