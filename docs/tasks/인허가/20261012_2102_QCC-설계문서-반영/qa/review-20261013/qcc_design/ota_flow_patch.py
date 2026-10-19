from pathlib import Path
from copy import deepcopy
import json, uuid, zipfile
from lxml import etree as E
import final_layout_patch as F
import final_toc_flow_patch as T
from final_layout_patch import Q, N, P, visible, sha

HERE = Path(__file__).parent
SOURCE = HERE / 'final-mic-patch'
OUT = HERE / 'final-ota-flow'
OUT.mkdir(exist_ok=True)
F.BASE = SOURCE
F.ROOT = OUT
F.EXPECTED['SDDD'] = 'c1478a934d78e53ecdec136037feaec628bd864d7eff225b0062eeb93bc10b30'


class Ota(T.Patch):
    def __init__(self):
        F.Patch.__init__(self, 'SDDD')
        self.created_mark_rPr = set()
        self.expected_text = visible(self.before)

    def flow(self, pid, changes):
        p = self.para(pid)
        text = visible(p)
        pictures = p.xpath('.//w:drawing[not(ancestor::w:del)]', namespaces=N)
        maximum = max(int(v) for v in self.r.xpath('.//*[local-name()="docPr"]/@id'))
        self.replace(p, keep_next=False)
        new = p.getnext()
        pp = new.find('w:pPr', N)
        for tag, value in changes.items():
            x = pp.find('w:' + tag, N)
            if x is None:
                x = E.Element(Q(tag))
                pp.insert(1, x)
            x.set(Q('val'), str(value))
        rank = {'pStyle': 0, 'keepNext': 1, 'keepLines': 2, 'pageBreakBefore': 3,
                'widowControl': 4, 'numPr': 5, 'tabs': 6, 'spacing': 7,
                'ind': 8, 'jc': 9, 'rPr': 10}
        nodes = list(pp)
        for x in nodes:
            pp.remove(x)
        for x in sorted(nodes, key=lambda v: rank.get(E.QName(v).localname, 8)):
            pp.append(x)
        if pictures:
            # Both historical and current pictures retain the same media relationship/bytes.
            # Only new drawing instance identifiers differ, to avoid a duplicate native Word ID.
            for i, x in enumerate(new.xpath('.//*[local-name()="docPr"]')):
                x.set('id', str(maximum + i + 1))
            for x in new.iter():
                if E.QName(x).localname == 'creationId' and x.get('id'):
                    x.set('id', '{' + str(uuid.uuid4()).upper() + '}')
                for key in list(x.attrib):
                    if E.QName(key).localname in ['anchorId', 'editId']:
                        x.set(key, uuid.uuid4().hex[:8].upper())
        assert visible(new) == text
        self.changes[-1].update({'flow': changes, 'reason': 'OTA51 explanation and figure34 form one readable group; remove active forced separation',
                                'drawing_media_relationship_and_extent_unchanged': True})


def main():
    p = Ota()
    p.flow('2D2692F1', {'keepNext': 1, 'keepLines': 1})
    p.flow('3BDFE6A4', {'pageBreakBefore': 0})
    report = p.finish()
    report['reject_this_flow_patch_exact_final_mic_candidate'] = True
    report['prior_final_mic_proof'] = str(SOURCE / 'proof.json')
    report['unchanged_G_guard'] = 'd7c1777f089cd37a6930b9452a928908286006bc35ec8cd131ec0199af5de776'
    gp = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기') / p.src.name
    assert sha(gp) == report['unchanged_G_guard']
    report['G_sha256_verified'] = sha(gp)
    (OUT / 'proof.json').write_text(json.dumps({'SDDD': report}, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'candidate': report['candidate'], 'sha256': report['candidate_sha256'], 'before': report['before_sha256']}, ensure_ascii=True))


if __name__ == '__main__':
    main()
