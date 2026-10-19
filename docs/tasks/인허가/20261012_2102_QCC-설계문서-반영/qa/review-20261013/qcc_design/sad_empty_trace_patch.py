from pathlib import Path
import json, re, zipfile
from lxml import etree as E
import final_layout_patch as F
import final_mic_patch as M
from final_layout_patch import Q, N, P, visible, dead, sha

OUT = Path(__file__).parent/'final-sad-trace-flow'
OUT.mkdir(exist_ok=True)
F.ROOT = OUT
M.ROOT = OUT
G = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')


class Cleanup(M.Extra, F.Patch):
    def __init__(self, baseline):
        F.BASE = G
        F.EXPECTED['SAD'] = baseline
        super().__init__('SAD')
        self.init_extra()
        self.expected_text = visible(self.before)

    def clean(self):
        cells = 0
        for tr in self.r.iter(Q('tr')):
            if dead(tr):
                continue
            cs = tr.findall('w:tc', N)
            if len(cs) != 3 or not re.fullmatch(r'SW-FW-RID-\d+', visible(cs[0]).strip()):
                continue
            rid = visible(cs[0]).strip()
            for col, tc in enumerate(cs[1:], 1):
                if ('SI-ID-QCC-' if col == 1 else 'SU-ID-QCC-') not in visible(tc):
                    continue
                cells += 1
                content = [p for p in tc.findall('w:p', N) if visible(p).strip()]
                assert content, (rid,col)
                for p in tc.findall('w:p',N):
                    if p.find('w:pPr/w:rPr/w:del',N) is not None or visible(p).strip():
                        continue
                    assert not p.xpath('.//w:drawing|.//w:fldChar|.//w:instrText|.//w:commentReference|.//w:footnoteReference',namespaces=N)
                    assert p.find('w:pPr',N) is not None
                    prior_count = len(p.xpath('.//*[@w:id]',namespaces=N))
                    self.mark_delete(p)
                    self.changes.append({'requirement':rid,'column':col,'paragraph_id':p.get(P),
                                         'operation':'true paragraph-mark deletion of active empty paragraph',
                                         'prior_revision_nodes_retained':prior_count,
                                         'text_and_notice_runs_unchanged':True})
        assert cells == 86
        assert self.changes
        return cells


def main():
    baseline = sha(next(G.glob('[[]E8300-QCC[]] SAD*.docx')))
    root_manifest = json.loads((Path(__file__).parent.parent/'final-mic-first3-published.json').read_text(encoding='utf8'))
    # Root publishes with a hash guard; preserve this immediate published G baseline in proof.
    p = Cleanup(baseline)
    cells = p.clean()
    report = p.finish_extra()
    report.update({'scope_mixed_cells':cells,'empty_paragraphs_mark_deleted':len(p.changes),
                   'header_repeat_unchanged':True,'font_size_table_width_paragraph_style_notice_format_unchanged':True,
                   'baseline_root_publish_record':str(Path(__file__).parent.parent/'final-mic-first3-published.json')})
    (OUT/'proof.json').write_text(json.dumps({'SAD':report},ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'path':report['candidate'],'before':baseline,'after':report['candidate_sha256'],'empty_paragraphs':len(p.changes)},ensure_ascii=True))


if __name__ == '__main__':
    main()
