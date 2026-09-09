# -*- coding: utf-8 -*-
"""SDDD 유닛 제외 체크리스트 생성

체크리스트를 은수님이 편집하신 뒤 make_sddd.py 를 다시 돌리면
체크된 항목과 유닛이 빠진 문서가 만들어진다.
"""
import io, os, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import cfx_units as CU
import cm3_units as CM
import boot_units as BO

OUT = (u'E:/workspace/projects/sound1-fw-e8300-backup/docs/tasks/인허가/'
       u'20260909_1419_sddd-sound1/유닛 제외 체크리스트.md')

CHAPTERS = [
    (4, 'CFX', u'CFX Core', CU.CFX_SECTIONS),
    (5, 'CM3', u'Cortex-M3', CM.CM3_SECTIONS),
    (6, 'BOOT', u'부트로더', BO.BOOT_SECTIONS),
]

HEAD = u'''---
name: SDDD 유닛 제외 체크리스트
purpose: 상세 설계서에서 뺄 항목과 유닛을 은수님이 골라 표시하는 작업용 목록
type: tasks
maturity: experimental
tags: [인허가, SDDD, 체크리스트, 유닛, 제외]
---

# SDDD 유닛 제외 체크리스트

**TL;DR**: 상세 설계서의 **항목 %d개 · 유닛 %d개** 전체 목록이다. **체크한 것이 문서에서 빠진다.**
표시를 마친 뒤 알려 주시면 문서를 다시 만든다.

---

> [!CAUTION]
> **체크 = 제외다.** `- [ ]` 는 유지, `- [x]` 는 문서에서 뺀다.
> 남길 것을 고르는 목록이 아니라 **뺄 것을 고르는 목록**이다.

## 쓰는 법

| 무엇 | 어떻게 |
|---|---|
| 유닛 하나를 뺀다 | 그 유닛 줄의 `[ ]` 를 `[x]` 로 바꾼다 |
| 항목을 통째로 뺀다 | **항목 제목 줄**의 `[ ]` 를 `[x]` 로 바꾼다. 그 아래 유닛을 하나씩 체크하지 않아도 된다 |
| 되돌린다 | `[x]` 를 `[ ]` 로 되돌린다. 원본 데이터는 그대로 있으므로 몇 번이든 오갈 수 있다 |

> [!NOTE]
> **번호는 원본 기준이다.** 유닛을 빼면 문서의 SU-ID 는 빈 번호 없이 다시 매겨진다.
> 이 체크리스트의 번호는 어느 유닛인지 가리키는 이름표이므로 그대로 둔다.
>
> 순서도가 붙은 항목(★)을 빼면 그 순서도도 함께 빠진다.

## 현황

| 장 | 모듈 | 항목 | 유닛 |
|---|---|---:|---:|
%s

---

'''


def esc(t):
    return t.replace('|', '\\|')


def main():
    lines = []
    stat = []
    n_item_all = 0
    n_unit_all = 0

    for ch, core, chname, sections in CHAPTERS:
        n_item = sum(len(bs) for _, bs in sections)
        n_unit = sum(len(b['units']) for _, bs in sections for b in bs)
        n_item_all += n_item
        n_unit_all += n_unit
        stat.append(u'| %d | %s | %d | %d |' % (ch, chname, n_item, n_unit))

        lines.append(u'## %d. %s  (항목 %d · 유닛 %d)\n' % (ch, chname, n_item, n_unit))
        si = 0
        for modname, blocks in sections:
            si += 1
            lines.append(u'### %d.%d. %s\n' % (ch, si, modname))
            bi = 0
            for blk in blocks:
                bi += 1
                title = (u'%s, %s' % (blk['id'], blk['name'])) if blk['id'] else blk['name']
                flow = u' ★' if blk.get('flow') else u''
                lines.append(u'- [ ] **[항목] %d.%d.%d. %s%s**  (유닛 %d)'
                             % (ch, si, bi, esc(title), flow, len(blk['units'])))
                for u in blk['units']:
                    fn = (u' · `%s`' % u['fn']) if u.get('fn') else u''
                    purpose = (u.get('purpose') or u'').strip()
                    if len(purpose) > 78:
                        purpose = purpose[:78] + u'…'
                    lines.append(u'    - [ ] `%s-%03d` %s%s<br>%s'
                                 % (core, u['id'], esc(u['name']), fn, esc(purpose)))
                lines.append(u'')
        lines.append(u'')

    body = HEAD % (n_item_all, n_unit_all, '\n'.join(stat)) + '\n'.join(lines)
    body += (u'\n---\n\n## 표시 후\n\n'
             u'표시를 마치면 알려 주십시오. `make_sddd.py` 가 이 파일을 읽어 '
             u'체크된 항목과 유닛을 뺀 문서를 다시 만든다. 원본 유닛 데이터는 건드리지 않으므로 '
             u'언제든 되돌릴 수 있다.\n')

    io.open(OUT, 'w', encoding='utf-8', newline='').write(body)
    print(u'생성: %s' % OUT)
    print(u'  항목 %d · 유닛 %d' % (n_item_all, n_unit_all))


if __name__ == '__main__':
    main()
