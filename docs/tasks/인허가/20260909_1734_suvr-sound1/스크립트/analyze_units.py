# -*- coding: utf-8 -*-
"""유닛 본문에서 «검증 가능한 사실» 을 뽑는다 - 코드 분석 시험의 실행체.

지어내지 않는다. 본문에 실제로 있는 것만 추출한다.
  - 호출하는 함수
  - 레지스터 / 전역 변수 대입과 그 값
  - 분기 조건과 개수
  - 반복문
  - 반환 값
결과는 units_facts.json 에 쌓이고, 이를 근거로 SUVR 의 «측정/검토 내용» 을 쓴다.
"""
import io, os, re, sys, json

HERE = os.path.dirname(os.path.abspath(__file__))

# C 키워드와 흔한 매크로는 «호출» 로 세지 않는다
NOT_CALL = set('''if while for switch return sizeof do else case break continue
goto typedef struct union enum static const void int char long short float double
unsigned signed volatile extern register inline defined'''.split())


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    s = re.sub(r'//[^\n]*', ' ', s)
    return s


def body_only(src):
    """서명을 떼고 중괄호 안쪽만 남긴다."""
    i = src.find('{')
    return src[i + 1:src.rfind('}')] if i >= 0 else src


def facts(src):
    """본문에서 사실을 뽑는다."""
    code = strip_comments(src)
    inner = body_only(code)

    calls = []
    for m in re.finditer(r'(?<![A-Za-z0-9_])([A-Za-z_][A-Za-z0-9_]*)\s*\(', inner):
        n = m.group(1)
        if n not in NOT_CALL and n not in calls:
            calls.append(n)

    # 대입: 좌변이 레지스터/구조체 멤버/전역으로 보이는 것
    assigns = []
    for m in re.finditer(
            r'(?<![=!<>+\-*/%&|^])'
            r'([A-Za-z_][A-Za-z0-9_]*(?:\s*(?:->|\.|\[[^\];]*\])\s*[A-Za-z0-9_]*)*)'
            r'\s*(=(?!=)|\|=|&=|\^=|\+=|-=|<<=|>>=)\s*'
            r'([^;]{1,80});', inner):
        lhs = re.sub(r'\s+', '', m.group(1))
        rhs = re.sub(r'\s+', ' ', m.group(3)).strip()
        if lhs and not lhs.isdigit():
            assigns.append((lhs, m.group(2), rhs))

    ifs = re.findall(r'(?<![A-Za-z0-9_])if\s*\(([^{;]{1,110})\)', inner)
    switches = re.findall(r'(?<![A-Za-z0-9_])switch\s*\(([^{;]{1,60})\)', inner)
    cases = re.findall(r'(?<![A-Za-z0-9_])case\s+([A-Za-z0-9_]+)\s*:', inner)
    loops = (len(re.findall(r'(?<![A-Za-z0-9_])for\s*\(', inner))
             + len(re.findall(r'(?<![A-Za-z0-9_])while\s*\(', inner)))
    rets = [re.sub(r'\s+', ' ', r).strip()
            for r in re.findall(r'(?<![A-Za-z0-9_])return\s+([^;]{1,60});', inner)]

    # 전처리 분기
    ppif = re.findall(r'#\s*if(?:def|ndef)?\s+([^\n]{1,60})', code)

    # 숫자 상수 (의미 있는 것만: 0/1 제외)
    consts = []
    for m in re.finditer(r'(?<![A-Za-z0-9_.])((?:0[xX][0-9a-fA-F]+)|(?:\d{2,}))', inner):
        v = m.group(1)
        if v not in consts:
            consts.append(v)

    return {
        'calls': calls[:40],
        'assigns': assigns[:40],
        'ifs': [re.sub(r'\s+', ' ', x).strip() for x in ifs[:20]],
        'switches': [re.sub(r'\s+', ' ', x).strip() for x in switches[:6]],
        'cases': cases[:24],
        'loops': loops,
        'returns': rets[:12],
        'ppif': [re.sub(r'\s+', ' ', x).strip() for x in ppif[:8]],
        'consts': consts[:20],
        'nline': src.count('\n') + 1,
    }


def main():
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    bodies = json.load(io.open(os.path.join(HERE, 'bodies.json'), encoding='utf-8'))

    out = {}
    nb = 0
    for key, rec in bodies.items():
        f = dict(rec)
        if rec.get('found') and rec.get('body'):
            f['facts'] = facts(rec['body'])
            nb += 1
        else:
            f['facts'] = None
        f.pop('body', None)
        out[key] = f

    p = os.path.join(HERE, 'units_facts.json')
    io.open(p, 'w', encoding='utf-8').write(json.dumps(out, ensure_ascii=False, indent=1))
    print(u'사실 추출 %d개 유닛 / 저장 %s' % (nb, p))

    # 표본 출력
    for k in ('CFX-001', 'CM3-107', 'BOOT-010'):
        r = out.get(k)
        if not r or not r['facts']:
            continue
        fa = r['facts']
        print(u'\n=== %s %s (%s) %s:%s / %d줄 ==='
              % (k, r['name'], r['fn'], r.get('file'), r.get('line'), fa['nline']))
        print(u'  호출 : %s' % ', '.join(fa['calls'][:10]))
        print(u'  대입 : %s' % '; '.join('%s %s %s' % a for a in fa['assigns'][:6]))
        print(u'  분기 : %s' % ' | '.join(fa['ifs'][:4]))
        print(u'  반복 : %d   반환 : %s' % (fa['loops'], fa['returns'][:3]))
        print(u'  전처리 : %s' % ', '.join(fa['ppif'][:4]))


if __name__ == '__main__':
    main()
