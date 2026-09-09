# -*- coding: utf-8 -*-
"""SUVC/SUVR 행 데이터 - 334 유닛의 검증 방법 분류와 시험 서식.

SDDD 와 같은 제외 규칙(체크리스트)을 적용하고 같은 방식으로 번호를 재부여한다.
따라서 여기의 SU-ID 는 재생성된 SDDD 의 SU-ID 와 정확히 일치한다.
"""
import io, os, re, sys

ROOT = u'E:/workspace/projects/sound1-fw-e8300-backup'
SDDD_DIR = ROOT + u'/docs/tasks/인허가/20260909_1419_sddd-sound1'
CHECKLIST = SDDD_DIR + u'/유닛 제외 체크리스트.md'
sys.path.insert(0, SDDD_DIR + u'/스크립트')

import cfx_units as CU
import cm3_units as CM
import boot_units as BO

CORES = [('CFX', CU.CFX_SECTIONS), ('CM3', CM.CM3_SECTIONS), ('BOOT', BO.BOOT_SECTIONS)]

# 모듈 이름 -> SUVC «Module» 열 표기 (1.5세대는 줄바꿈을 넣어 좁게 썼다)
MOD_LABEL = {
    u'System Control Module': u'시스템\n제어\n모듈',
    u'HEAR Function Chain': u'HEAR\n함수\n체인',
    u'Signal Processing Module': u'신호\n처리\n모듈',
    u'Internal Stimulation Device Execution Module': u'내부기\n실행\n모듈',
    u'Bluetooth Communication Module': u'블루투스\n통신\n모듈',
    u'Boot Control Module': u'부트\n제어\n모듈',
    u'Image Loading Module': u'이미지\n적재\n모듈',
    u'Wireless Update Module': u'무선\n갱신\n모듈',
    u'Service Module': u'서비스\n모듈',
}

# ---------------------------------------------------------------- 실기(D) 대상
# 원본 SU-ID 기준이다. 제외 17건으로 번호가 밀리므로 아래에서 신 번호로 사상한다.
# 선정 기준: 입력을 주면 결정적 출력 수치가 나오는 순수 연산 유닛.
D_UNITS_ORIG = {
    'CFX': [76, 77, 78, 80,                      # AGC
            85, 86, 87,                          # 주파수 분석
            88, 89, 90, 91, 93,                  # 비선형 매핑
            94, 95, 96, 97, 98, 99, 100, 101, 102,   # 자극 기법
            103, 104, 105, 106, 107, 108, 109,   # PCM 비트스트림
            113],                                # 채널별 검사 프레임
    'CM3': [117, 118, 119, 120, 121],            # 자극 파라미터 연산
    'BOOT': [],
}


def load_exclusions():
    """체크된(= 제외할) 유닛과 항목을 읽는다. make_sddd.py 와 같은 규칙."""
    ex_units, ex_items = set(), set()
    if not os.path.exists(CHECKLIST):
        return ex_units, ex_items
    for line in io.open(CHECKLIST, encoding='utf-8'):
        m = re.match(r'\s*-\s*\[([ xX])\]\s*\*\*\[항목\]\s*(\d+\.\d+\.\d+)\.', line)
        if m:
            if m.group(1).lower() == 'x':
                ex_items.add(m.group(2))
            continue
        m = re.match(r'\s*-\s*\[([ xX])\]\s*\`([A-Z0-9]+)-(\d+)\`', line)
        if m and m.group(1).lower() == 'x':
            ex_units.add((m.group(2), int(m.group(3))))
    return ex_units, ex_items


def build():
    """제외를 적용하고 번호를 재부여한 유닛 목록을 만든다."""
    ex_units, ex_items = load_exclusions()
    out = []
    for core, sections in CORES:
        ch = {'CFX': 4, 'CM3': 5, 'BOOT': 6}[core]
        seq = 0
        si = 0
        for mod, blocks in sections:
            si += 1
            bi = 0
            for blk in blocks:
                bi += 1
                key = u'%d.%d.%d' % (ch, si, bi)
                if key in ex_items:
                    continue
                for u in blk['units']:
                    if (core, u['id']) in ex_units:
                        continue
                    seq += 1
                    out.append({
                        'core': core,
                        'orig_id': u['id'],
                        'id': seq,
                        'su_id': u'SU-ID-%s-%03d' % (core, seq),
                        'name': u['name'],
                        'fn': u.get('fn') or '',
                        'fn2': u.get('fn2') or '',
                        'mod': mod,
                        'mod_label': MOD_LABEL.get(mod, mod),
                        'si_id': blk.get('id') or '',
                        'block': blk['name'],
                        'purpose': u.get('purpose') or '',
                        'desc': u.get('desc') or [],
                        'limit': u.get('limit') or [],
                        'method': 'D' if u['id'] in D_UNITS_ORIG.get(core, []) else 'A',
                    })
    # TEST ID 는 코어별 1부터
    n = {}
    for r in out:
        n[r['core']] = n.get(r['core'], 0) + 1
        r['test_id'] = u'SW-FW-UnitTest-%s-ID-%d' % (r['core'], n[r['core']])
    return out


def short_name(name):
    """«... 유닛» 에서 «유닛» 을 떼어 통과 기준 문구로 쓴다."""
    s = name.strip()
    for tail in (u' 유닛', u'유닛'):
        if s.endswith(tail):
            s = s[:-len(tail)].strip()
            break
    return s


if __name__ == '__main__':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    rows = build()
    print(u'유닛 %d건' % len(rows))
    for core in ('CFX', 'CM3', 'BOOT'):
        rs = [r for r in rows if r['core'] == core]
        a = len([r for r in rs if r['method'] == 'A'])
        d = len([r for r in rs if r['method'] == 'D'])
        print(u'  %-5s %3d건  (A %3d / D %2d)' % (core, len(rs), a, d))
    print(u'  %-5s %3d건  (A %3d / D %2d)'
          % (u'합계', len(rows),
             len([r for r in rows if r['method'] == 'A']),
             len([r for r in rows if r['method'] == 'D'])))
    print()
    print(u'=== 실기(D) 대상 - 원본번호 -> 신번호 ===')
    for r in rows:
        if r['method'] == 'D':
            print(u'  %s-%03d -> %s  %-30s %s'
                  % (r['core'], r['orig_id'], r['su_id'], r['name'][:30], r['fn']))
