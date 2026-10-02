"""Check SIP redlines, unchanged package parts and native Word QA outputs."""
from hashlib import sha256
import json
import zipfile
from lxml import etree as E
from inspect_sip_alignment import TASK, OUT, W, NS, text
from patch_sip_alignment import BASE, DESIGN

QA=TASK/'qa/sip-sddd-alignment'

def parts(path):
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        return {n:z.read(n) for n in z.namelist()}

def body_text(root):
    # Generated TOC fields can refresh independently of tracked prose.
    return [text(p) for p in root.xpath('//w:body//w:p[not(ancestor::w:sdt)]',namespaces=NS)]

def main():
    before=parts(QA/'before.docx');candidate=parts(QA/'candidate.docx')
    assert before.keys()==candidate.keys()
    different=[n for n in before if before[n]!=candidate[n]]
    assert different==['word/document.xml'],different
    root=E.fromstring(candidate['word/document.xml'])
    accepted=E.fromstring(parts(QA/'candidate-accepted.docx')['word/document.xml'])
    rejected=E.fromstring(parts(QA/'candidate-rejected.docx')['word/document.xml'])
    before_rejected=E.fromstring(parts(QA/'before-rejected.docx')['word/document.xml'])
    assert body_text(rejected)==body_text(before_rejected),'변경 거부 시 기존 본문 불일치'
    report=json.loads((QA/'patch.json').read_text(encoding='utf-8'))
    final=body_text(accepted)
    for change in report['changes']:
        assert final.count(change['after'])==1,change['after']
    active='\n'.join(final)
    for old in ('TD20B-A-EXT','TD23D-A-EXT','NRF','isd_ver','SUVC의'):
        assert old not in active,old
    for word in ('Event 0','Event 1','16표본','512표본','NofM','3프레임',
                 '0x34','0x35','0x36','Gain B','QCC_CTRL','ISD_CHECK',
                 '슬롯 1~2','슬롯 1~4','요청/완료'):
        assert word in active,word
    reds=[]
    for p in accepted.xpath('//w:body//w:p',namespaces=NS):
        if text(p).startswith('[자료 추가 위치]'):
            assert p.xpath('.//w:rPr/w:color[@w:val="C00000"]',namespaces=NS)
            reds.append(text(p))
    assert len(reds)==4
    assert not root.xpath('//w:ins//w:ins|//w:del//w:ins',namespaces=NS)
    assert E.fromstring(candidate['word/settings.xml']).find(W+'trackRevisions') is not None
    old_headings=json.loads((QA/'before-headings.json').read_text(encoding='utf-8-sig'))
    new_headings=json.loads((QA/'candidate-headings.json').read_text(encoding='utf-8-sig'))
    assert [(x['number'],x['level'],x['text']) for x in old_headings]==[
        (x['number'],x['level'],x['text']) for x in new_headings]
    assert sha256((OUT/'SDDD-FW-TD24A 소프트웨어 상세 설계서.docx').read_bytes()).hexdigest()==DESIGN
    # Added/revised text has no tiny font or hidden runs.
    for ins in root.xpath('//w:ins[w:r]',namespaces=NS):
        if ins.get(W+'author')!='김은수':continue
        assert not ins.xpath('.//w:vanish',namespaces=NS)
        assert all(int(v)>=18 for v in ins.xpath('.//w:sz/@w:val',namespaces=NS))
    result={'passed':True,'candidate_sha256':sha256((QA/'candidate.docx').read_bytes()).hexdigest(),
            'only_changed_package_part':different,'native_reject_restores_original_body':True,
            'native_headings_and_numbers_preserved':len(new_headings),
            'planned_text_updates':len(report['changes']),'qcc_red_placeholders':len(reds),
            'styles_numbering_images_headers_footers_unchanged':True,
            'input_design_sha256':DESIGN,'live_baseline_sha256':BASE,
            'scope':'Document consistency and Word layout, not firmware runtime validation.'}
    (QA/'verify.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False))

if __name__=='__main__':main()
