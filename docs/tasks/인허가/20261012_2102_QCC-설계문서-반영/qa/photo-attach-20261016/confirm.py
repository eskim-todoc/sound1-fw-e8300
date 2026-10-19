import package as m
from pathlib import Path
from copy import deepcopy as cp
from lxml import etree as E
from zipfile import ZipFile,ZIP_DEFLATED
import sys,re,json,os

H=m.HERE;GREEN={'rFont':{'val':'맑은 고딕'},'sz':{'val':'11'},'color':{'rgb':'FF008000'},'u':{'val':'single'}}
def prepare():
    pub=m.readj('publication.json');path=Path(pub['output']);b=m.Book(path)
    assert m.digest(path)==pub['outputSHA256'],'Published file changed; inspect current edits before continuing.'
    backup=H/'confirmation-before.xlsx'
    if backup.exists():assert m.digest(backup)==m.digest(path)
    else:backup.write_bytes(path.read_bytes())
    items=[];dec=m.readj('decisions.json');annotations=[v['text'] for v in dec['pendingAnnotations']]
    def edit(sh,a,replacements=None,new=None,remove_note=False,verdict=False):
        old=b.runs(b.cells[sh][a]);rs=cp(old)
        if remove_note:
            for note in annotations:
                s=m.text(rs)
                if note in s:
                    start=s.index(note);rs=m.cut(rs,start,start+len(note))
        for before,after in (replacements or []):
            rs=m.replace_literal(rs,before,after,GREEN)
        if new is not None:rs=[{'text':new,'font':GREEN}]
        rs=m.tidy_runs(rs)
        if m.text(rs)==m.text(old):return
        items.append({'sheet':sh,'cell':a,'old':m.text(old),'new':m.text(rs),'runs':rs,'reason':'2026-10-16 사용자 확인: 알림 ON·자극 출력 / 시작 볼륨 2→3'})
    sh=m.MAIN;aux='시험 보조자료'
    edit(sh,'I158',[('리모콘에서 자극 알림을 끈다.','조건 [1]은 리모콘에서 자극 알림을 끄고, 조건 [2]는 자극 알림을 켠다.')])
    edit(sh,'K159',[('알림용 자극 OFF 확인.','자극 알림 ON 상태에서 알림용 자극 출력 확인.')],remove_note=True)
    edit(sh,'J170',[('자극 볼륨 1에서','자극 볼륨 2에서')],remove_note=True)
    edit(sh,'K170',[('자극 볼륨=3의 자극 출력 변화 확인.','자극 볼륨이 2에서 3으로 증가한 후 자극 출력 변화 확인.')])
    for a in ['L158','L169']:edit(sh,a,new='\n통과\n')
    edit(aux,'C110',[('알림용 자극 OFF 확인.','자극 알림 ON 상태에서 알림용 자극 출력 확인.'),('알림 OFF 화면, 전극의 연속 자극 파형.','알림 ON 화면, 전극의 알림용 자극 파형.')],remove_note=True)
    edit(aux,'C111',[('알림 OFF 화면, 전극의 연속 자극 파형.','조건 [1]의 알림 OFF 화면 및 조건 [2]의 알림 ON 화면, 각 조건에 대응하는 전극의 자극 파형.')])
    edit(aux,'B125',[('자극 볼륨 1에서','자극 볼륨 2에서')],remove_note=True)
    edit(aux,'C125',[('자극 볼륨=3의 자극 출력 변화 확인.','자극 볼륨이 2에서 3으로 증가한 후 자극 출력 변화 확인.')])
    for a in ['D109','D110','D124','D125','D126']:edit(aux,a,[('확인 필요','통과')])
    last=max(int(r.get('r')) for r in b.sheets[m.COMP].find(m.t('sheetData')))
    header=last+2
    for i,v in enumerate(items):v['compareRow']=header+2+i
    m.savej('confirmation-plan.json',{'source':str(path),'beforeSHA':m.digest(path),'headerRow':header,'lastRow':header+1+len(items),'changes':items,'userConfirmation':{'test28':'알림 사용(ON), 알림용 자극 출력 확인','test32':'2에서 3으로 증가'}})
    print(json.dumps({'changes':len(items),'comparisonStart':header}))

def integrate():
    pl=m.readj('confirmation-plan.json');b=m.Book(H/'confirmation-before.xlsx');a=m.Book(H/'confirmation-authored.xlsx');bridge=m.StyleBridge(b);data=b.data.copy()
    for ch in pl['changes']:
        assert a.value(ch['cell'],ch['sheet'])==ch['new']
        m.inline(b.cells[ch['sheet']][ch['cell']],ch['runs'])
    for sh in [m.MAIN,'시험 보조자료']:data[b.paths[sh]]=m.dump(b.sheets[sh])
    co=b.sheets[m.COMP];sd=co.find(m.t('sheetData'));mc=co.find(m.t('mergeCells'))
    for r in a.sheets[m.COMP].find(m.t('sheetData')):
        if int(r.get('r'))<pl['headerRow']:continue
        row=cp(r)
        for c in row:
            ac=a.cells[m.COMP][c.get('r')];c.set('s',str(bridge.copy_style(a,int(c.get('s','0')))));m.inline(c,a.runs(ac))
        sd.append(row)
    for mm in a.sheets[m.COMP].findall('s:mergeCells/s:mergeCell',m.NS):mc.append(cp(mm))
    mc.set('count',str(len(mc)));co.find(m.t('dimension')).set('ref',f'A1:L{pl["lastRow"]}')
    for dn in b.wb.findall('s:definedNames/s:definedName',m.NS):
        if dn.get('name')=='_xlnm.Print_Area' and dn.get('localSheetId')==str(list(b.paths).index(m.COMP)):dn.text=f"'{m.COMP}'!$B$2:$L${pl['lastRow']}"
    data[b.paths[m.COMP]]=m.dump(co);data['xl/styles.xml']=m.dump(b.styles);data['xl/workbook.xml']=m.dump(b.wb)
    with ZipFile(H/'confirmation-after.xlsx','w',ZIP_DEFLATED) as z:
        for path,blob in data.items():z.writestr(path,blob)
    verify()

def verify():
    pl=m.readj('confirmation-plan.json');old=m.Book(H/'confirmation-before.xlsx');b=m.Book(H/'confirmation-after.xlsx');changed={(v['sheet'],v['cell']):v for v in pl['changes']};errs=[]
    for sh,cells in old.cells.items():
        if sh==m.COMP:continue
        for addr in cells:
            got=b.value(addr,sh);expected=changed[(sh,addr)]['new'] if (sh,addr) in changed else old.value(addr,sh)
            if got!=expected:errs.append((sh,addr,'value'))
    for ch in pl['changes']:
        val=b.value(ch['cell'],ch['sheet'])
        if not val.startswith('\n') or not val.endswith('\n'):errs.append(('padding',ch['cell']))
    keep=[p for p in old.data if p.startswith('xl/media/') or p.startswith('xl/drawings/')]
    for path in keep:
        if old.data[path]!=b.data[path]:errs.append(('media-drawing-change',path))
    for sh in [m.MAIN,'시험 보조자료']:
        if E.tostring(old.sheets[sh].find(m.t('mergeCells')))!=E.tostring(b.sheets[sh].find(m.t('mergeCells'))):errs.append(('merges',sh))
    for a in ['L158','L169']:
        if b.value(a).strip()!='통과':errs.append(('verdict',a))
    if 'OFF' in b.value('K159') or '확인 필요' in b.value('K159') or '자극 볼륨 1에서' in b.value('J170'):errs.append(('unresolved-marker',))
    if [v['id'] for v in old.tests()]!=[v['id'] for v in b.tests()]:errs.append(('test-order',))
    out={'beforeSHA':m.digest(old.path),'afterSHA':m.digest(b.path),'changedCells':len(pl['changes']),'preservedMediaDrawingParts':len(keep),'errors':errs,'sourceEvidence':pl['userConfirmation'],'scope':'사용자 확인 결과 반영 및 문서 검증. 새 제품 측정 실행 아님.'}
    m.savej('confirmation-validation.json',out);print(json.dumps(out,ensure_ascii=True))
    assert not errs

def publish():
    pl=m.readj('confirmation-plan.json');v=m.readj('confirmation-validation.json');native=m.readj('confirmation-native-qa.json');pub=m.readj('publication.json');target=Path(pl['source']);after=H/'confirmation-after.xlsx'
    assert not v['errors'] and m.digest(after)==v['afterSHA']
    assert all(r['sourceSHA256']==v['afterSHA'] and r['sourceUnchanged'] for r in native)
    assert m.digest(target)==pl['beforeSHA'],'Output changed during editing; preserve user changes.'
    original_base=target.with_name('[E8300-QCC] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx')
    original_personal=target.with_name('[김은수 전용] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx')
    source_before={str(x):m.digest(x) for x in [original_base,original_personal]}
    # A verified backup already exists. Replace only this task's requested editable copy.
    tmp=target.with_name(target.name+'.confirmation.tmp')
    assert not tmp.exists()
    with tmp.open('xb') as f:f.write(after.read_bytes());f.flush();os.fsync(f.fileno())
    assert m.digest(tmp)==v['afterSHA'] and m.digest(target)==pl['beforeSHA']
    os.replace(tmp,target)
    assert m.digest(target)==v['afterSHA']
    assert source_before=={str(x):m.digest(x) for x in [original_base,original_personal]}
    pub.setdefault('history',[]).append({'outputSHA256':pub['outputSHA256'],'scope':'시험28/32 사용자 확인 전 자료첨부본'})
    pub.update(outputSHA256=v['afterSHA'],confirmedTests=[28,32],confirmationEvidence=pl['userConfirmation'],sourcesUnchanged=True)
    m.savej('publication.json',pub)
    m.savej('confirmation-publication.json',{'output':str(target),'beforeSHA':pl['beforeSHA'],'afterSHA':v['afterSHA'],'originalFilesBeforeAndAfter':source_before,'changedCells':len(pl['changes']),'saved':True})
    b=m.Book(target);state=m.readj('final-state.json');state['previousSHA']=state['candidateSHA'];state['candidateSHA']=v['afterSHA'];state['pending']=[];state['confirmedTests']=pl['userConfirmation'];state['confirmationValidation']='confirmation-validation.json'
    loc={}
    for sh,cells in b.cells.items():
        if sh.endswith('_비교'):continue
        for a,c in cells.items():
            val=m.text(b.runs(c))
            for prefix in ['[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]']:
                if prefix in val:loc.setdefault(prefix,{}).setdefault(sh,[]).append(a)
    state['prefixLocations']=loc;m.savej('final-state.json',state)
    root=Path(r'E:/workspace/projects/sound1-fw-e8300-backup');reg=root/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md';body=reg.read_text(encoding='utf-8')
    start=body.index('<!-- SSTR-PHOTO-20261016:BEGIN -->');end=body.index('<!-- SSTR-PHOTO-20261016:END -->',start)
    section=body[start:end]
    old='시험28 검증2의 알림 ON/OFF와 시험32 조건2의 시작 볼륨은 확인 대기다. 해당 J/K와 L에 불일치 및 `확인 필요`를 표시했다.'
    new='2026-10-16 사용자 확인으로 시험28 검증2는 알림 ON 상태의 알림용 자극 출력, 시험32 조건2는 시작 볼륨2에서3으로 증가로 확정했다. 본문 및 보조자료의 불일치 안내를 해소하고 두 시험 판정을 `통과`로 갱신했다. 해당 변경은 사용자 확인 결과의 반영이며 새 계측 실행이 아니다.'
    assert old in section;section=section.replace(old,new)
    rows=[]
    for i,(prefix,sheets) in enumerate(loc.items(),1):
        pos='; '.join('`'+sh+'!'+', '.join(cells)+'`' for sh,cells in sheets.items())
        rows.append(f'| SSTR-PHOTO-{i:02d} | `{prefix}` | {pos} | 본문 표시 유지, 실제 결과/대기는 해당 셀 확인 / 2026-10-16 |')
    section=re.sub(r'^\| SSTR-PHOTO-.*\n', '',section,flags=re.M)
    section=section.replace('|---|---|---|---|\n','|---|---|---|---|\n'+'\n'.join(rows)+'\n',1)
    body=body[:start]+section+body[end:];reg.write_text(body,encoding='utf-8')
    task=H.parent.parent/'작업.md';body=task.read_text(encoding='utf-8')
    status='''<!-- TDC-TASK-STATUS:BEGIN v1 -->
| 항목 | 내용 |
|---|---|
| 상태 | 자료첨부 사본 및 시험28·32 사용자 확인 반영 완료 |
| 현재 결과 | 사진123장/8cm, 녹색 수정·지정 삭제 반영. 시험28 ON·알림용 자극 출력, 시험32 시작2→3 확정 및 판정 통과. 원본 보존 |
| 남은 일 | 이번 두 확인사항은 해소. 기존 QCC/연계 자료 및 기준서와 정식 행 구조 확정은 해당 기록 유지 |
| 다음 행동 | 후속 자료 또는 수정 요청에 따라 현재 _자료첨부 사본에서 계속 작업 |
| 다음 담당 | 사용자 요청 / Codex |
| 확인 시각 | 2026-10-16 |
<!-- TDC-TASK-STATUS:END -->'''
    body=re.sub(r'<!-- TDC-TASK-STATUS:BEGIN v1 -->.*?<!-- TDC-TASK-STATUS:END -->',lambda _:status,body,count=1,flags=re.S)
    entry=f'''\n## 2026-10-16 후속 확인 — 시험28·32 불일치 해소

은수님이 시험28은 알림 사용(ON) 및 알림용 자극 출력, 시험32는 시작 볼륨2에서3으로 증가했다고 확인했다. 앞 절의 두 내용 대기는 이 답변으로 해소했다. G드라이브 `_자료첨부` 사본의 본문6셀과 보조자료9셀을 수정하고, 같은 파일의 `_비교` 시트1022~{pl['lastRow']}에 확인 전후 이력을 남겼다.

- 시험28: `보고서_시스템 시험!I158/K159/L158`, `시험 보조자료!C110/C111/D109:D110`. 조건1의OFF는 유지하고 조건2의ON을 절차·결과·사진 안내에 맞췄다. 부족한 실제 사진 첨부 요구는 해소로 바꾸지 않았다.
- 시험32: `보고서_시스템 시험!J170/K170/L169`, `시험 보조자료!B125/C125/D124:D126`. 2→3의 입력/응답/출력 관찰을 같은 조건으로 맞췄다. 두 시험 판정은 사용자 확인에 따라 통과로 복구했다.
- 원칙3·6·11·13·15~18·22~25·27~28을 변경 범위에서 재검사했다. 불일치 안내 제거, 본문/보조 조건·결과·판정 일치, 리치텍스트·개행·병합·다른 시험 판정 보존, 실제 Excel 읽기 전용 렌더를 확인했다. 나머지 원칙의 이전 적용/해당없음 및 기준서 동등성 미실행 범위는 유지한다.
- 매체/그림 패키지136개는 직전 산출물과 바이트가 같다. 사진123장의 크기·앵커, 행/병합 구조를 변경하지 않았다. 원본 QCC와 개인전용 파일은 수정하지 않았다.
- 저장 SHA256 `{v['afterSHA']}`. [변경15셀](qa/photo-attach-20261016/confirmation-plan.json), [검증](qa/photo-attach-20261016/confirmation-validation.json), [Excel 확인](qa/photo-attach-20261016/confirmation-native-qa.json), [독립 대조](qa/photo-attach-20261016/confirmation-audit.json), [최종 저장](qa/photo-attach-20261016/confirmation-publication.json). 변경 전 복구본은 같은 QA 폴더의 `confirmation-before.xlsx`다.

이번 결과는 시험자 확인을 문서에 반영한 사실과 문서 검사를 뜻한다. 새 제품 계측이나 정식 승인을 생성한 것으로 기록하지 않는다.
'''
    body+=entry;task.write_text(body,encoding='utf-8')
    print(json.dumps({'saved':str(target),'afterSHA':v['afterSHA'],'confirmed':[28,32]},ensure_ascii=True))

if __name__=='__main__':{'prepare':prepare,'integrate':integrate,'verify':verify,'publish':publish}[sys.argv[1]]()
