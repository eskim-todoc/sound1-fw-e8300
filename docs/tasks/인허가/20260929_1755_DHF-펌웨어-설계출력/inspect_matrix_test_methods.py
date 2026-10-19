"""Read-only projection of upper verification methods and linked system tests."""
import json
import re
import sys
from remove_six_srs_notices import QA

d = json.loads((QA / 'current-readonly-evidence.json').read_text(encoding='utf8'))
m = d['matrix']['설계 입출력 검증 매트릭스']['cells']
srs_sheet = d['SRS']['소프트웨어 입력 요구사항 사양서']
s = srs_sheet['cells']
t = d['SSTR']['보고서_시스템 시험']['cells']
c = d['SSTC']['기준서_시스템 시험']['cells']
sr = {int(v.strip().rsplit('-', 1)[1]): int(a[1:]) for a, v in s.items()
      if a.startswith('G') and re.fullmatch(r'SW-FW-RID-\d+', v.strip())}
starts = sorted((int(a[1:]), int(v.strip().rsplit('-', 1)[1])) for a, v in t.items()
                if a.startswith('E') and re.fullmatch(r'SW-FW-RID-\d+', v.strip()))
ends = dict((rid, starts[i + 1][0] if i + 1 < len(starts) else 301)
            for i, (row, rid) in enumerate(starts))
ts = {rid: row for row, rid in starts}
def upper_ids(row):
    address = 'H' + str(row)
    value = s.get(address, '')
    if not value:
        for merged in srs_sheet['merges']:
            match = re.fullmatch(r'H(\d+):H(\d+)', merged)
            if match and int(match[1]) <= row <= int(match[2]):
                value = s.get('H' + match[1], '')
                break
    return {int(x) for x in re.findall(r'SYS-RID-SP-FW-(\d+)', value)}

for r in range(100, 150):
    upper = int(m['G' + str(r)].rsplit('-', 1)[1])
    if len(sys.argv) > 1 and not int(sys.argv[1]) <= upper <= int(sys.argv[2]):
        continue
    linked = [rid for rid, row in sr.items() if upper in upper_ids(row)]
    print(f'UPPER {upper} / {m.get("T" + str(r), "")} / U: {m.get("U" + str(r), "(blank)")} / V: {m.get("V" + str(r), "(blank)")}')
    for rid in linked:
        row = ts[rid]
        print(f'SW {rid}, rows {row}:{ends[rid]-1}, method {t.get("D" + str(row), "").strip()}, shared {t.get("F"+str(row), "").strip()}')
        for col, label in [('I','procedure'), ('J','criterion'), ('K','result'), ('L','judgment')]:
            values = [f'{col}{n}: {t[col + str(n)].strip()}' for n in range(row, ends[rid]) if t.get(col + str(n), '').strip()]
            print(label + ': ' + ' / '.join(values))
        diffs = [a for a in set(t) | set(c) if re.fullmatch(r'[A-J]\d+', a) and
                 row <= int(a[1:]) < ends[rid] and t.get(a, '') != c.get(a, '')]
        if diffs:
            print('Criteria-report common-column differences:', diffs)
    print()
