"""Add evidence-based SOUP management text while preserving the Word package."""
from copy import deepcopy
from datetime import datetime, timezone
from pathlib import Path
import json
import zipfile
from lxml import etree as E
from patch_sad_order import rejected, text, q, N, PATH, T

QA=T/'qa/soup'
STAMP=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')

ROWS=[
 ['항목 / 공급자','확인한 버전','사용 범위와 적용 조건'],
 ['FatFs / ChaN, onsemi 배포',
  'SDK 4.5.0.107의 소스 R0.15 patch1. 실제 링크 파일은 빌드별 확인',
  'CM3의 맵/내부기 정보/설정/갱신 파일, 부트로더의 이미지와 부트 상태 파일. 파일 결과와 처리 크기, 필요한 CRC를 확인한다.'],
 ['tiny-AES-c / kokke',
  '부트로더 패키지 표기 1.0.0. CM3 수정본은 별도 파일 해시로 구분',
  'AES-128 ECB, 블록 16바이트. 부트 이미지 복호화와 내부기 정보 파일 암복호화에 사용한다. 모든 맵 파일을 암호화하는 것은 아니다.'],
 ['CRC 지원 코드 / onsemi',
  'E8300 SDK 4.5.0.107의 crc.h 및 제공 부트로더 CRC 코드',
  'CRC 하드웨어 설정/입력/결과 읽기에 사용한다. CM3의 자체 래퍼를 통해 파일 CRC16을 계산한다. 하드웨어와 자체 코드는 외부 제공 코드와 구분한다.'],
 ['SEGGER RTT / SEGGER',
  '7.56a. 본체 Rev.23622, 설정 Rev.24316',
  '부트로더/CM3의 디버그 입출력에 사용한다. 기본 채널은 NO_BLOCK_SKIP 모드이며, CM3에는 RTT 명령 입력도 포함되어 있다.']]

def main():
    QA.mkdir(parents=True,exist_ok=True)
    backup=QA/'before-soup.docx'
    assert not backup.exists(), 'Inspect the existing baseline before rerunning'
    raw=PATH.read_bytes();backup.write_bytes(raw)
    with zipfile.ZipFile(PATH) as z:parts={n:z.read(n) for n in z.namelist()}
    root=E.fromstring(parts['word/document.xml']); before=deepcopy(root)
    body=root.find(q('body'))
    old=': 칩셋 제공 라이브러리와 파일 시스템 등 외부 제공 소프트웨어는 사용 버전과 인터페이스를 확인하여 통합한다.'
    hits=[p for p in body.findall(q('p')) if text(p)==old]
    assert len(hits)==1
    anchor=hits[0]; template=deepcopy(anchor.find(q('pPr')))
    counter=max(int(x) for x in root.xpath('//@w:id',namespaces=N) if x.isdigit())+1
    def change(tag):
        nonlocal counter
        element=E.Element(q(tag),{q('id'):str(counter),q('author'):'김은수',q('date'):STAMP})
        counter+=1
        return element
    def run(parent,value,deleted=False,bold=False):
        r=E.SubElement(parent,q('r'));rp=E.SubElement(r,q('rPr'))
        E.SubElement(rp,q('rFonts'),{q('ascii'):'맑은 고딕',q('hAnsi'):'맑은 고딕',q('eastAsia'):'맑은 고딕'})
        E.SubElement(rp,q('sz'),{q('val'):'20'})
        if bold:E.SubElement(rp,q('b'))
        t=E.SubElement(r,q('delText' if deleted else 't'));t.text=value
        t.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
    original=rejected(anchor)
    for c in list(anchor):
        if c.tag!=q('pPr'):anchor.remove(c)
    deletion=change('del');run(deletion,original,True);anchor.append(deletion)
    insertion=change('ins');run(insertion,': FatFs, tiny-AES-c, E8300 SDK의 CRC 지원 코드와 SEGGER RTT를 SOUP 관리 대상으로 식별하였다. 사용 목적, 적용 조건, 확인 결과와 잔여 검증 항목을 다음과 같이 관리한다.');anchor.append(insertion)
    nodes=[]
    def paragraph(value,bold=False,keep=False):
        p=E.Element(q('p'));pp=deepcopy(template);p.append(pp)
        for tag in ['numPr','outlineLvl','pageBreakBefore']:
            for x in pp.findall(q(tag)):pp.remove(x)
        sp=pp.find(q('spacing'))
        if sp is None:sp=E.SubElement(pp,q('spacing'))
        sp.set(q('after'),'100');sp.set(q('line'),'264')
        if keep:E.SubElement(pp,q('keepNext'))
        rp=E.SubElement(pp,q('rPr'));rp.append(change('ins'))
        ins=change('ins');run(ins,value,bold=bold);p.append(ins)
        nodes.append(p);return p
    paragraph('표 SOUP-1. SOUP 식별 및 사용 범위',True,True)
    table=E.Element(q('tbl'));props=E.SubElement(table,q('tblPr'))
    widths=[1800,2700,4370]
    E.SubElement(props,q('tblW'),{q('w'):str(sum(widths)),q('type'):'dxa'})
    E.SubElement(props,q('tblInd'),{q('w'):'700',q('type'):'dxa'})
    E.SubElement(props,q('tblLayout'),{q('type'):'fixed'})
    borders=E.SubElement(props,q('tblBorders'))
    for edge in ['top','left','bottom','right','insideH','insideV']:
        E.SubElement(borders,q(edge),{q('val'):'single',q('sz'):'4',q('color'):'808080'})
    margins=E.SubElement(props,q('tblCellMar'))
    for edge,value in [('top',70),('bottom',70),('left',90),('right',90)]:
        E.SubElement(margins,q(edge),{q('w'):str(value),q('type'):'dxa'})
    grid=E.SubElement(table,q('tblGrid'))
    for width in widths:E.SubElement(grid,q('gridCol'),{q('w'):str(width)})
    for i,values in enumerate(ROWS):
        row=E.SubElement(table,q('tr'));trp=E.SubElement(row,q('trPr'))
        trp.append(change('ins'));E.SubElement(trp,q('cantSplit'))
        if i==0:E.SubElement(trp,q('tblHeader'))
        for width,value in zip(widths,values):
            cell=E.SubElement(row,q('tc'));tcp=E.SubElement(cell,q('tcPr'))
            E.SubElement(tcp,q('tcW'),{q('w'):str(width),q('type'):'dxa'})
            E.SubElement(tcp,q('vAlign'),{q('val'):'center'})
            p=E.SubElement(cell,q('p'));pp=E.SubElement(p,q('pPr'))
            E.SubElement(pp,q('spacing'),{q('before'):'0',q('after'):'0',q('line'):'240',q('lineRule'):'auto'})
            if i==0:E.SubElement(pp,q('jc'),{q('val'):'center'})
            ins=change('ins');run(ins,value,bold=(i==0));p.append(ins)
    nodes.append(table)
    paragraph('SDK 소스의 버전 표기와 실제 제품에 링크된 바이너리는 구분한다. FatFs 소스 폴더와 프로젝트 include 폴더의 볼륨/상대 경로 설정이 달라, 출하 대상 빌드의 설정과 라이브러리 식별값을 함께 확인한다. 이 목록은 위 네 항목에 대한 것으로, 다른 SDK 기능과 런타임의 SOUP 검토를 제외하지 않는다.')
    paragraph('확인 결과',True,True)
    paragraph('2026년 9월 30일 코드 검토에서 파일 읽기/쓰기 결과와 바이트 수 검사, 파일 CRC 비교, AES 블록 길이 처리, RTT 비블로킹 기본 설정을 확인하였다. CM3의 내부기 정보 파일에는 CRC와 AES가 적용되며, 나머지 맵/설정 파일은 각 호출의 옵션에 따라 처리한다.')
    paragraph('동일 날짜에 부트로더와 CM3의 AES 소스를 Windows 호스트 GCC 16.1.0으로 각각 빌드하고 NIST SP 800-38A F.1의 공개 시험 벡터와 비교하였다. 각 소스에서 암호화 4건과 복호화 4건, 합계 16건이 모두 일치하였다. 이 결과는 AES 기본 연산에 대한 확인이며, 실제 장치의 파일/이미지 처리와 보안성 전체를 판정한 결과는 아니다.')
    paragraph('FatFs의 공식 갱신 이력에 있는 파일 잠금/재진입 관련 수정 조건을 현재 설정과 대조하였다. 현재 include 설정은 FF_FS_LOCK=0, FF_FS_REENTRANT=0이다. 적용 바이너리의 설정을 확정한 뒤 해당 문제의 영향 판단을 완료한다. tiny-AES-c의 ECB 및 입력 길이 제약, RTT의 버퍼/초기화 조건도 관리 항목에 포함한다.')
    paragraph('관리 및 추가 확인',True,True)
    paragraph('소스/설정/라이브러리의 버전과 파일 식별값을 기록하고, 변경 시 사용 기능과 공급자가 공개한 문제를 다시 검토한다. 시험 결과는 적용 빌드와 연결하여 보관하며, 오류의 영향과 조치 결과를 위험관리 및 소프트웨어 검증 기록에 반영한다.')
    paragraph('최종 적합성 판단 전에는 파일 저장 완료 오류와 전원 차단 복구, CRC 입력 경계 및 VOLATILE 섹션의 예외 처리, AES 파일 연계와 키 관리, RTT 출력 길이 제한과 출하 시 명령 접근을 확인한다. 일부 f_sync/f_close 결과 미확인과 RTT 자체 printf 래퍼의 길이 제한 미적용은 잔여 확인 항목으로 등록하였다. CRC와 ECB만으로 외부 데이터의 진본성을 보증하지 않는다.')
    paragraph('따라서 SOUP의 사용처와 기본 보호 처리는 확인되었으며, AES 기본 연산 시험은 통과하였다. 제품 수준의 사용 적합성은 잔여 확인 항목의 처리와 해당 검증 결과를 연결하여 최종 판정한다.')
    paragraph('참고 자료',True,True)
    paragraph('FatFs Updates and Migration Notes: https://elm-chan.org/fsw/ff/updates.html\n'
              'tiny-AES-c: https://github.com/kokke/tiny-AES-c\n'
              'SEGGER RTT: https://kb.segger.com/RTT\n'
              'NIST SP 800-38A: https://csrc.nist.gov/pubs/sp/800/38/a/final\n'
              'onsemi Ezairo 8300 SDK REVISION 4.5.0.107, ff.c/ff.h/ffconf.h/crc.h. 자료 확인일: 2026년 9월 30일.')
    pos=body.index(anchor)+1
    for node in nodes:body.insert(pos,node);pos+=1
    assert rejected(root)==rejected(before)
    before_tables=before.xpath('//w:body/w:tbl',namespaces=N)
    for a,b in zip(before_tables,root.xpath('//w:body/w:tbl',namespaces=N)):
        assert E.tostring(a)==E.tostring(b)
    parts['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
    settings=E.fromstring(parts['word/settings.xml'])
    assert settings.find(q('trackRevisions')) is not None
    candidate=QA/'candidate-soup.docx'
    with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in parts.items():z.writestr(name,data)
    assert PATH.read_bytes()==raw, 'User file changed during edit'
    PATH.write_bytes(candidate.read_bytes())
    checks={'tracked_changes':True,'original_tables_unchanged':True,
            'reject_all_original_text_preserved':True,'SOUP_rows':4,'AES_host_tests_passed':16,
            'target_tests_performed':False,'only_changed_package_part':'word/document.xml'}
    (QA/'docx-checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(checks,ensure_ascii=False))

if __name__=='__main__':main()
