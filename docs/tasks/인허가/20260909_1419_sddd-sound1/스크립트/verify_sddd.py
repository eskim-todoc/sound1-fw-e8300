# -*- coding: utf-8 -*-
"""SDDD 자체 검증 (CFX + CM3)"""
import io, sys, re, json
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
from docx import Document
from docx.oxml.ns import qn

sys.path.insert(0, '.')
import cfx_units as CU
import cm3_units as CM
import boot_units as BO

P = (u'E:/workspace/projects/sound1-fw-e8300-backup/docs/참고/인허가/사운드1/'
     u'[02] SDDD-FW-SND1 소프트웨어 상세 설계서.docx')
d = Document(P)
sec = d.sections[0]
BODY = sec.page_width.cm - sec.left_margin.cm - sec.right_margin.cm
print(u'용지 %.1f x %.1f cm / 본문 폭 %.2f cm / 표 %d / 단락 %d'
      % (sec.page_width.cm, sec.page_height.cm, BODY, len(d.tables), len(d.paragraphs)))

paras = [p.text.strip() for p in d.paragraphs]
alltext = '\n'.join(paras)
for t in d.tables:
    for row in t.rows:
        for c in row.cells:
            alltext += '\n' + c.text

print(u'\n[검증_1] 표 폭 <= 본문 폭')
bad = 0
for i, t in enumerate(d.tables):
    g = t._tbl.find(qn('w:tblGrid'))
    tot = sum(int(gc.get(qn('w:w'))) / 567.0 for gc in g.findall(qn('w:gridCol')))
    if tot > BODY + 0.02:
        bad += 1
        print(u'  X 표%d %.2f cm' % (i, tot))
print(u'  결과: %s' % (u'통과' if bad == 0 else u'실패 %d건' % bad))

print(u'\n[검증_2] 장/절 번호 상호참조 없음 (착탈 안전성)')
pat = re.compile(u'\\d+\\.\\d*\\s*[장절]|[장절]\\s*\\d+\\.\\d')
hits = []
for t in paras:
    if not t or re.match(u'^\\d+(\\.\\d+)*\\.\\s', t):
        continue
    m = pat.search(t)
    if m:
        hits.append((m.group(0), t[:70]))
for h in hits:
    print(u'  X', h)
print(u'  결과: %s (%d건)' % (u'통과' if not hits else u'실패', len(hits)))

print(u'\n[검증_3] SU-ID 연속성 및 4요소 완비')
missing = []
counts = {}
for ch, core in ((4, 'CFX'), (5, 'CM3'), (6, 'BOOT')):
    pat_h = u'^%d\\.\\d+\\.\\d+\\.\\d+\\.\\s+SU-ID-%s-' % (ch, core)
    rx = re.compile(pat_h)
    sus = [t for t in paras if rx.match(t)]
    nums = [int(re.search(u'SU-ID-%s-(\\d+)' % core, t).group(1)) for t in sus]
    counts[core] = len(nums)
    ok = nums == list(range(1, len(nums) + 1))
    print(u'  %s 유닛 %d건 / 번호 연속: %s' % (core, len(nums), 'OK' if ok else 'X %s' % nums[:6]))
    any_head = re.compile(u'^\\d+(\\.\\d+)*\\.\\s')
    for i, pp in enumerate(d.paragraphs):
        if not rx.match(pp.text.strip()):
            continue
        seg = []
        for x in d.paragraphs[i + 1:]:
            t = x.text.strip()
            if any_head.match(t):
                break
            seg.append(t)
        blk = '\n'.join(seg)
        for k in (u'입력 파라미터', u'반환 파라미터', u'수정 파라미터', u'목적', u'기능 및 동작'):
            if k not in blk:
                missing.append((pp.text.strip()[:40], k))
print(u'  합계 %d건 / 4요소 누락: %s'
      % (sum(counts.values()), u'없음' if not missing else missing))

print(u'\n[검증_4] 파라미터가 실제 서명과 일치')
idx = json.load(io.open('sig_index.json', encoding='utf-8'))


def kcount(s):
    return 0 if s.strip() == u'없음' else len([x for x in s.split(',') if x.strip()])


mis = []
chk = 0
for core, secs_all in (('CFX', CU.CFX_SECTIONS), ('CM3', CM.CM3_SECTIONS),
                       ('BOOT', BO.BOOT_SECTIONS)):
    for _, secs in secs_all:
        for b in secs:
            for u in b['units']:
                for key in ('fn', 'fn2'):
                    f = u.get(key)
                    if not f:
                        continue
                    v = idx.get(f)
                    if not v:
                        mis.append((core, u['id'], f, u'서명 없음'))
                        continue
                    chk += 1
                    if key != 'fn':
                        continue
                    if kcount(u['inp']) != v['argc']:
                        mis.append((core, u['id'], f,
                                    u'인자 %d != %d' % (kcount(u['inp']), v['argc'])))
                    if not u.get('fn2'):
                        if (v['ret'].strip() == 'void') != (u['ret'].strip() == u'없음'):
                            mis.append((core, u['id'], f, u'반환 불일치'))
print(u'  서명 대조 %d건 / 불일치: %s' % (chk, u'없음' if not mis else mis))

print(u'\n[검증_5] 교차참조(U/M 라벨) 반영')
xr = sorted({x.strip() for _, ss in CU.CFX_SECTIONS for b in ss for u in b['units']
             if u.get('xref') for x in u['xref'].split(',') if x.strip().startswith('U')},
            key=lambda s: int(s[1:]))
print(u'  U 라벨: %s (%d/14)' % (' '.join(xr), len(xr)))
lost = [u'U%d' % i for i in range(1, 15) if u'U%d' % i not in xr]
print(u'  누락: %s' % (u'없음' if not lost else lost))

print(u'\n[검증_6] 폐기 부품 언급')
found = 0
for b in [u'nRF', u'NRF', u'가속도', u'이어피스', u'USB', u'7150', u'텔레코일']:
    n = [ln.strip()[:70] for ln in alltext.split('\n') if b in ln]
    if n:
        found += len(n)
        print(u'  ? %s : %s' % (b, n[:2]))
print(u'  결과: %s (%d건)' % (u'통과' if found == 0 else u'검토 필요', found))

print(u'\n[검증_7] 그림')
imgs = [r for r in d.part.rels.values() if 'image' in r.reltype]
print(u'  이미지 %d개 (블록도 2 + 순서도 17 = 19 기대)' % len(imgs))

print(u'\n[검증_8] 짝 유닛(fn2) 정합')
pairbad = []
npair = 0
for core, secs_all in (('CFX', CU.CFX_SECTIONS), ('CM3', CM.CM3_SECTIONS),
                       ('BOOT', BO.BOOT_SECTIONS)):
    for _, secs in secs_all:
        for b in secs:
            for u in b['units']:
                f2 = u.get('fn2')
                if not f2:
                    continue
                npair += 1
                v = idx.get(f2)
                v1 = idx.get(u['fn']) if u.get('fn') else None
                if not v:
                    continue
                ok = (v['argc'] == 0) or (v1 and v['argc'] <= v1['argc'])
                if not ok:
                    pairbad.append((core, u['id'], f2,
                                    u'짝 함수 인자 %d (주 함수 %s)'
                                    % (v['argc'], v1['argc'] if v1 else '-')))
print(u'  짝 유닛 %d건 / 이상: %s' % (npair, u'없음' if not pairbad else pairbad))

print(u'\n[검증_9] 서술 분량')
tot_u = tot_s = 0
for core, secs_all in (('CFX', CU.CFX_SECTIONS), ('CM3', CM.CM3_SECTIONS), ('BOOT', BO.BOOT_SECTIONS)):
    n = p_ = d_ = w_ = l_ = 0
    for _, secs in secs_all:
        for b in secs:
            for u in b['units']:
                n += 1
                p_ += 1 if u.get('purpose') else 0
                d_ += len(u.get('desc') or [])
                w_ += len(u.get('why') or [])
                l_ += len(u.get('limit') or [])
    print(u'  %-5s 유닛 %3d / 목적 %3d / 동작 %3d / 근거 %3d / 제약 %3d  (유닛당 %.1f줄)'
          % (core, n, p_, d_, w_, l_, (p_ + d_ + w_ + l_) / float(n)))
    tot_u += n
    tot_s += p_ + d_ + w_ + l_
print(u'  합계  유닛 %d / 서술 %d줄 (유닛당 %.1f줄)' % (tot_u, tot_s, tot_s / float(tot_u)))
