"""검사한 네 후보만 원래 경로에 교체한다. 원본은 QA 복구본에 보존한다."""
import ctypes
from ctypes import wintypes
import json
import os
import shutil
import sys
from pathlib import Path
import revise_integration_system_rev0 as work
import verify_integration_system_rev0_independent as independent

KEYS=['integration','system','integrationCriterion','systemCriterion']

def preflight(path):
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    create=kernel.CreateFileW
    create.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
    create.restype=wintypes.HANDLE
    handle=create(str(path),0x80000000|0x40000000,0,None,3,0x80,None)
    if handle==wintypes.HANDLE(-1).value:raise OSError(ctypes.get_last_error(),f'파일 열림/쓰기 확인 실패: {path}')
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    kernel.CloseHandle(handle)

def verify_registry():
    import re
    from urllib.parse import unquote
    registry=work.ROOT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
    content=registry.read_text(encoding='utf-8')
    links=re.findall(r'\]\((DHF/[^)]+)\)',content)
    missing=[url for url in links if not (registry.parent/unquote(url)).exists()]
    assert not missing,missing
    from openpyxl import load_workbook
    s=load_workbook(work.QA/'system-candidate.xlsx',rich_text=True)['보고서_시스템 시험']
    rows={47:190,48:193,49:194,52:202,71:211,72:214,76:224,79:233}
    for tid,row in rows.items():
        assert '(QCC 검증 결과 필요)' in str(s[f'K{row}'].value),(tid,row)
        assert f'K{row}' in content
    return dict(links=len(links),qccPositions=len(rows))

def run():
    baseline=work.read('baseline.json')
    for key in ['integration','system']:
        work.verify(key)
        n=work.read(key+'-native-heights.json')
        assert n['hash']==work.digest(work.QA/f'{key}-candidate.xlsx'),('native audit stale',key)
        assert not [c for c in n['measurements']+n.get('helperMeasurements',[]) if c['needed']>c['current']+1],('height',key)
        if key=='system':assert len(n['markerChecks'])==52
    independent.verify()
    locations=verify_registry()
    for key in KEYS:preflight(work.PATHS[key])
    stages={};published=[]
    try:
        for key in KEYS:
            target=work.PATHS[key]; assert target.parent.is_dir()
            assert work.digest(target)==baseline['books'][key]['hash'],('target changed',key)
            stage=target.with_name(target.name+'.rev0-stage.tmp')
            assert not stage.exists(),stage
            shutil.copy2(work.QA/f'{key}-candidate.xlsx',stage)
            assert work.digest(stage)==work.digest(work.QA/f'{key}-candidate.xlsx')
            stages[key]=stage
        for key in KEYS:
            assert work.digest(work.PATHS[key])==baseline['books'][key]['hash'],('target changed',key)
            os.replace(stages[key],work.PATHS[key]);published.append(key)
        output=[]
        for key in KEYS:
            assert work.digest(work.PATHS[key])==work.digest(work.QA/f'{key}-candidate.xlsx')
            output.append(dict(key=key,path=str(work.PATHS[key]),hash=work.digest(work.PATHS[key]),backup=str(work.QA/f'{key}-before.xlsx')))
        work.save('publication.json',dict(files=output,registry=locations,newBoardExecution=False,
            passRecordSource='시험자의 전 항목 통과 재확인 및 기존/최신 유닛 기록 대조'))
        print(json.dumps(output,ensure_ascii=False,indent=2))
    except Exception:
        # 이번에 교체한 후보가 그대로 있을 때만 복구한다. 외부 편집이 있으면 덮어쓰지 않는다.
        for key in reversed(published):
            if work.digest(work.PATHS[key])==work.digest(work.QA/f'{key}-candidate.xlsx'):
                recovery=work.PATHS[key].with_name(work.PATHS[key].name+'.rev0-restore.tmp')
                assert not recovery.exists()
                shutil.copy2(work.QA/f'{key}-before.xlsx',recovery);os.replace(recovery,work.PATHS[key])
        raise
    finally:
        for key,stage in stages.items():
            if stage.exists() and work.digest(stage)==work.digest(work.QA/f'{key}-candidate.xlsx'):stage.unlink()

if __name__=='__main__':run()
