"""Apply the author's planned registered-implant access condition to RID-79 only."""
from copy import deepcopy
import json
import shutil
import zipfile
from lxml import etree as E
from inspect_audit_units import FILES, TASK
from inspect_sad_unit_trace import N,q,text,sha
from patch_sad_audit_trace import parts,current_rows,edit_inserted

QA=TASK/'qa/sad-rid79-connected-20261012'
NOTE='[E8300 반영] 외부기에 등록된 내부기 정보와 실제 내부기 정보가 일치하고 정상 연결된 경우에만 로그 조회를 허용한다. 미연결, 정보 불일치 또는 연결 해제 시 조회를 차단한다.'
TRANSFER='[E8300 확인] 로그 외부 전송 시 기밀성 보호 조건을 별도로 확인한다.'

def main():
    QA.mkdir(parents=True,exist_ok=True)
    assert not (QA/'before.docx').exists()
    previous=json.loads((TASK/'qa/audit-units-four-docs-20261012/published-verification.json').read_text(encoding='utf8'))
    assert sha(FILES['sad'])==previous['outputs']['sad']['sha256'],'SAD changed externally'
    protected={str(FILES[k]):sha(FILES[k]) for k in ('sddd','suvc','suvr')}
    protected.update(json.loads((TASK/'qa/audit-units-four-docs-20261012/baseline.json').read_text(encoding='utf8'))['protected'])
    source=FILES['sad'];shutil.copyfile(source,QA/'before.docx')
    (QA/'baseline.json').write_text(json.dumps({'source':str(source),'sha256':sha(source),'protected':protected},ensure_ascii=False,indent=2),encoding='utf8')
    package=parts(source);root=E.fromstring(package['word/document.xml'])
    rows=current_rows(root);assert len(rows)==81
    untouched={rid:E.tostring(row) for rid,row in rows.items() if rid!='SW-FW-RID-79'}
    row=rows['SW-FW-RID-79'];cells=row.findall('w:tc',N)
    old=text(row)
    note=cells[2].findall('w:p',N)[-1]
    edit_inserted(note,[(NOTE+'\n'+TRANSFER,'e8300')])
    next_id=max(int(v) for v in root.xpath('//@w:id',namespaces=N) if v.isdigit())+1
    def clone(p,value):
        nonlocal next_id
        p=deepcopy(p)
        for element in p.iter():
            for key in list(element.attrib):
                if key.endswith(('}paraId','}textId')): del element.attrib[key]
            if q('id') in element.attrib:
                element.set(q('id'),str(next_id));next_id+=1
        edit_inserted(p,[(value,None)])
        return p
    template=cells[1].find('w:p',N)
    cells[1].append(clone(template,'SI-ID-CM3-003\n내부 자극 장치 실행 모듈'))
    unit_template=cells[2].find('w:p',N)
    for value in ['SU-ID-CM3-030\n내부기 실행 상태 관리','SU-ID-CM3-031\n내부기 및 송신 전원 초기화']:
        cells[2].insert(list(cells[2]).index(note),clone(unit_template,value))
    assert all(E.tostring(current_rows(root)[rid])==v for rid,v in untouched.items())
    package['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
    with zipfile.ZipFile(QA/'candidate.docx','w',zipfile.ZIP_DEFLATED) as z:
        for name,data in package.items():z.writestr(name,data)
    (QA/'changes.json').write_text(json.dumps({'id':'SW-FW-RID-79','before':old,'after':text(row),'code_modified':False,'design_condition_only':True},ensure_ascii=False,indent=2),encoding='utf8')
    print(text(row))

if __name__=='__main__':main()
