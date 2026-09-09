# -*- coding: utf-8 -*-
"""실기 시험 절차서 생성.

SUVC/SUVR 과 같은 expected_values 에서 만든다. 손으로 옮겨 적지 않으므로
문서 사이가 어긋나지 않는다.
"""
import io, os, sys, json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from suvc_rows import build

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), u'실기 시험 절차서.md')

HEAD = u'''---
name: Sound1 유닛 검증 실기 시험 절차서
purpose: 은수님이 실기에서 직접 돌려 SUVR 의 실기 대상 유닛을 검증하기 위한 절차와 기댓값
type: tasks
maturity: experimental
tags: [인허가, SUVR, 실기시험, 하네스, 테스트벡터, 기댓값]
---

# Sound1 유닛 검증 실기 시험 절차서

**TL;DR**: SUVR 실기 대상 **%d 유닛**의 시험 절차와 기댓값이다. 그중 **%d 유닛은 기댓값을 미리 산출**했고, **%d 유닛은 라이브러리 출력에 의존해 실측을 기록**한다. 시험 코드는 **신규 파일 2 개**에만 있고 **기존 소스는 한 글자도 고치지 않았다.**

> [!NOTE]
> **이 문서는 편의용 요약이다.** 같은 내용이 SUVC 와 SUVR 의 각 행에 «시험 준비 및 초기 조건 · 가정 및 제한 조건 · 시험 절차 · 통과 기준» 으로 전부 들어가 있다. 두 문서 중 어느 하나만 보고도 시험할 수 있다.

---

## 1. 준비

| 파일 | 대상 | 진입점 | 결과 배열 |
|---|---|---|---|
| `src/1__cfx/test/tdc_unit_test_cfx.c` | CFX | `tdc_ut_run_all()` | `tdc_ut_result` (`tdc_ut_count` 개) |
| `src/2__cm3/Cortex-M3-src/test/tdc_unit_test_cm3.c` | Cortex-M3 | `tdc_ut_cm3_run_all()` | `tdc_ut_cm3_result` (`tdc_ut_cm3_count` 개) |

> [!IMPORTANT]
> **기존 소스는 수정하지 않았다.** 두 파일 모두 전체가 `#if defined(TDC_UNIT_TEST)` 안에 있어, 매크로를 정의하지 않으면 **빈 번역 단위**가 된다. 빌드에 넣어 두어도 평시 이미지는 한 바이트도 늘지 않는다.

1. 빌드 설정에 **`TDC_UNIT_TEST`** 를 정의한다.
2. 위 두 파일을 빌드 대상에 넣는다.
3. 디버거에서 진입점을 호출한다. *(원본을 고치지 않는 방법이다. 어려우면 시험할 때만 `main()` 에 호출 한 줄을 임시로 넣고 끝나면 되돌린다.)*
4. `done` 변수에 1 이 대입되는 지점에 중단점을 걸고 결과 배열을 조회한다.

값 `-999` 는 **구분자**다. 그 다음 값이 유닛 번호다.

> [!WARNING]
> 시험은 전역 상태를 바꾼다. **시험 후에는 재기동한다.**

---

'''

TAIL = u'''
---

## %d. 결과를 주시면

실측값을 주시면 SUVR 의 «측정/검토 내용» 과 «결과» 열에 반영해 문서를 확정하겠습니다.
기댓값과 어긋나는 것이 있으면 그것부터 함께 보시죠. 제 계산이 틀렸을 수도, 코드가 의도와
다를 수도 있습니다.

## %d. 시험 후

- [ ] 빌드 설정에서 `TDC_UNIT_TEST` 정의를 지운다
- [ ] 재빌드하여 평시 이미지에 시험 심볼이 없는지 확인한다 (`tdc_ut_` 로 시작하는 심볼 0 건)
- [ ] 장치를 재기동한다
'''


def esc(t):
    return (t or u'').replace(u'|', u'\\|').replace(u'\n', u'<br>')


def main():
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    V = json.load(io.open(os.path.join(HERE, 'expected_values.json'), encoding='utf-8'))
    rows = {u'%s-%03d' % (r['core'], r['id']): r for r in build()}

    n_rep = len([v for v in V if v['kind'] in (u'재현', u'상수')])
    n_meas = len([v for v in V if v['kind'] == u'실측'])
    out = [HEAD % (len(V), n_rep, n_meas)]

    sec = 2
    for kind_label, kinds in ((u'기댓값을 산출한 유닛', (u'재현', u'상수')),
                              (u'실측을 기록하는 유닛', (u'실측',))):
        items = [v for v in V if v['kind'] in kinds]
        out.append(u'## %d. %s (%d)\n' % (sec, kind_label, len(items)))
        if kinds == (u'실측',):
            out.append(u'> [!CAUTION]\n'
                       u'> **기댓값을 내지 않았다.** DSP 어셈블리 라이브러리나 HEAR 함수체인 '
                       u'출력에 의존해 재현할 수 없다. 지어내지 않고 실측값을 그대로 기록한다.\n'
                       u'> 1.5세대 SUVR 도 이 계통은 «고정소수점 연산 고려» 를 통과 기준에 달았다.\n')
        for v in items:
            r = rows.get(v['su'])
            name = r['name'] if r else v['su']
            out.append(u'### SU-ID-%s  `%s`\n' % (v['su'], v['fn']))
            out.append(u'**%s**\n' % name)
            out.append(u'%s\n' % v['note'])
            out.append(u'**시험 준비 및 초기 조건**\n')
            out.append(u'```\n%s\n```\n' % v['prep'])
            out.append(u'**가정 및 제한 조건**\n')
            out.append(u'```\n%s\n```\n' % v['limit'])
            out.append(u'**시험 절차**\n')
            out.append(u'```\n%s\n```\n' % v['proc'])
            out.append(u'**입력과 예상 출력**\n')
            out.append(u'| # | 입력 | 예상 출력 |')
            out.append(u'|---:|---|---|')
            for i, c in enumerate(v['cases'], 1):
                out.append(u'| %d | %s | %s |' % (i, esc(c['in']), esc(c['out'])))
            out.append(u'')
        sec += 1

    out.append(TAIL % (sec, sec + 1))
    io.open(OUT, 'w', encoding='utf-8', newline='').write(u'\n'.join(out))
    print(u'생성: %s' % OUT)
    print(u'유닛 %d (기댓값 %d / 실측 %d) · 케이스 %d'
          % (len(V), n_rep, n_meas, sum(len(v['cases']) for v in V)))


if __name__ == '__main__':
    main()
