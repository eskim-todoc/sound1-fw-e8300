# -*- coding: utf-8 -*-
"""유닛 함수 본문 추출 - 코드 분석 검증의 근거를 소스에서 직접 뽑는다.

SDDD 의 351 유닛 각각에 대해 실제 구현 본문을 찾아 낸다.
찾지 못한 유닛은 «미확인» 으로 남긴다 - 지어내지 않는다.
"""
import io, os, re, sys, json

ROOT = u'E:/workspace/projects/sound1-fw-e8300-backup'
SRC = ROOT + u'/src'
SDDD = ROOT + u'/docs/tasks/인허가/20260909_1419_sddd-sound1/스크립트'
sys.path.insert(0, SDDD)

import cfx_units as CU
import cm3_units as CM
import boot_units as BO

CORES = [('CFX', CU.CFX_SECTIONS), ('CM3', CM.CM3_SECTIONS), ('BOOT', BO.BOOT_SECTIONS)]

SKIP_DIR = ('.metadata', 'RemoteSystemsTempFiles', '5__calibration')


def c_files(ext='.c'):
    """정의는 .c 에 있는 것이 원칙이므로 .c 를 먼저, 헤더 정의(static inline)는 뒤로 돌린다."""
    out = []
    for base, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d not in SKIP_DIR and not d.startswith('.')]
        for f in files:
            if f.endswith(ext):
                out.append(os.path.join(base, f).replace('\\', '/'))
    return out


def read(p):
    for enc in ('utf-8', 'cp949', 'latin-1'):
        try:
            return io.open(p, encoding=enc).read()
        except UnicodeDecodeError:
            continue
    return u''


def find_body(text, fn):
    """함수 정의를 찾아 중괄호 균형으로 본문 끝까지 잘라 낸다."""
    # 정의부: 반환형 + 이름 + (인자) + { , 선언(;)은 제외
    for m in re.finditer(r'(?<![A-Za-z0-9_])' + re.escape(fn) + r'\s*\(', text):
        i = m.end() - 1
        depth = 0
        j = i
        while j < len(text):
            if text[j] == '(':
                depth += 1
            elif text[j] == ')':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        # 닫는 괄호 뒤 첫 '{' 가 첫 ';' 보다 앞서야 정의다.
        # 사이에 #ifdef/#else/#endif 나 다른 서명이 끼어들 수 있으므로
        # 문장 기호(';' '=')가 없는 한 건너뛴다.
        nb = text.find('{', j + 1)
        ns = text.find(';', j + 1)
        if nb < 0 or (0 <= ns < nb):
            continue                      # 선언이다
        gap = text[j + 1:nb]
        if '=' in gap or gap.count('\n') > 8:
            continue                      # 호출문이거나 너무 멀다
        k = nb
        # 앞쪽이 호출문이면(= 또는 ; 뒤 바로) 걸러 낸다 - 정의는 줄 시작에 반환형이 온다
        ls = text.rfind('\n', 0, m.start()) + 1
        head = text[ls:m.start()].strip()
        if not head or head.endswith((';', '=', ',', '(', '&', '|', '!')):
            continue
        depth = 0
        e = k
        while e < len(text):
            if text[e] == '{':
                depth += 1
            elif text[e] == '}':
                depth -= 1
                if depth == 0:
                    break
            e += 1
        return text[ls:e + 1], text[:ls].count('\n') + 1
    return None, None


def main():
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    files = c_files('.c') + c_files('.h')      # .c 우선, 헤더 정의는 뒤로
    print(u'소스 %d개 파일 훑는다 (.c + .h).' % len(files))
    cache = [(p, read(p)) for p in files]

    result = {}
    stat = {}
    for core, sections in CORES:
        found = miss = 0
        for mod, blocks in sections:
            for blk in blocks:
                for u in blk['units']:
                    key = u'%s-%03d' % (core, u['id'])
                    fn = u.get('fn')
                    rec = {'core': core, 'id': u['id'], 'name': u['name'],
                           'fn': fn or '', 'mod': mod, 'si': blk.get('id') or '',
                           'block': blk['name'], 'found': False}
                    if fn:
                        for p, txt in cache:
                            body, ln = find_body(txt, fn)
                            if body:
                                rec.update({'found': True, 'file': p[len(ROOT) + 1:],
                                            'line': ln, 'body': body[:6000],
                                            'nline': body.count('\n') + 1})
                                break
                    if rec['found']:
                        found += 1
                    else:
                        miss += 1
                    result[key] = rec
        stat[core] = (found, miss)
        print(u'  %-5s 본문 확보 %3d / 미확보 %3d' % (core, found, miss))

    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'bodies.json')
    io.open(out, 'w', encoding='utf-8').write(
        json.dumps(result, ensure_ascii=False, indent=1))
    tot = sum(v[0] for v in stat.values())
    print(u'합계 본문 확보 %d / 전체 %d (%.1f%%)'
          % (tot, len(result), 100.0 * tot / len(result)))
    print(u'저장: %s' % out)


if __name__ == '__main__':
    main()
