"""검증된 두 산출물만 저장한다. 사용자 편집/참고 파일 변경은 거부한다."""
import hashlib
import json
import os
import shutil
import tempfile
from pathlib import Path

TASK=Path(__file__).resolve().parent
QA=TASK/'qa/system-cybersecurity-rev0'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    baseline=json.loads((QA/'baseline.json').read_text('utf8'))
    check=json.loads((QA/'verification.json').read_text('utf8'))
    for kind in ['system','criterion']:
        native=json.loads((QA/(kind+'-native-heights.json')).read_text('utf-8-sig'))
        candidate=QA/(kind+'-candidate.xlsx')
        assert native['hash']==sha(candidate), ('native check outdated',kind)
        assert all(x['needed']<=x['current']+1 for x in native['measurements']), ('text clipping',kind)
    assert sha(QA/'system-candidate.xlsx')==check['reportSha256']
    assert sha(QA/'criterion-candidate.xlsx')==check['criterionSha256']
    for source in baseline['sources'].values():
        assert sha(source['path'])==source['sha256'], ('source/user edit',source['path'])
    for path,value in baseline['protected'].items():
        assert sha(path)==value, ('protected',path)
    report=Path(baseline['sources']['system']['path'])
    dhf=report.parents[3]
    criterion=dhf/'03_설계 출력/SW/펌웨어/SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서_rev0(김은수 작성).xlsx'
    assert criterion.parent.is_dir()
    assert not criterion.exists(), '새 기준서 경로에 외부 편집 파일이 생겼다.'
    stages=[]
    for kind,dest in [('system',report),('criterion',criterion)]:
        handle,temporary=tempfile.mkstemp(prefix='.cybersecurity-',suffix='.pending.xlsx',dir=dest.parent)
        os.close(handle)
        temp=Path(temporary)
        shutil.copyfile(QA/(kind+'-candidate.xlsx'),temp)
        assert sha(temp)==sha(QA/(kind+'-candidate.xlsx'))
        stages.append((kind,temp,dest))
    # 파일이 잠겨 있으면 원본을 삭제하지 않고 예외를 반환한다.
    published=[]
    try:
        for kind,temp,dest in stages:
            os.replace(temp,dest)
            published.append(dict(kind=kind,path=str(dest),sha256=sha(dest)))
    finally:
        for _,temp,_ in stages:
            if temp.exists():temp.unlink()
    assert len(published)==2
    for path,value in baseline['protected'].items():
        assert sha(path)==value, ('protected after save',path)
    (QA/'publication.json').write_text(json.dumps(dict(files=published,protectedFiles=len(baseline['protected']),gitChanged=False),ensure_ascii=False,indent=2),'utf8')
    print(json.dumps(published,ensure_ascii=False,indent=2))


if __name__=='__main__':
    main()
