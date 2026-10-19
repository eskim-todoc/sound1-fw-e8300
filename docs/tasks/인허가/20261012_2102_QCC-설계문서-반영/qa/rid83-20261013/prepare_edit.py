from pathlib import Path
import sys,json
H=Path(__file__).resolve().parent
sys.path.insert(0,str(H.parent/'sv-clean-20261013'))
from prepare_clean import pack,sheets,cellmap,value,load,save,sha
books=[]
for f in load(H/'source-manifest.json')['files']:
    code=f['code']
    if code not in ['SRS','SSTC','SSTR']:continue
    source=H/'backups'/f'{code}.xlsx';d,_=pack(source)
    tabs=sheets(d);main=next(s for s in tabs if s['name']!='표지');compare=next(s for s in tabs if s['name']==main['name']+'_비교')
    _,cm,ss=cellmap(d,main['part']);_,cc,_=cellmap(d,compare['part'])
    notice='[E8300 보완] 검사한 사용설명서 초안과 사용자 배포용 설명서의 일치 여부를 확인한다.'
    changes={'H93':'','F94':notice+'\n'} if code=='SRS' else {
      'F285':'',
      'H285':'\n'+notice+'\n',
      'I285':'\n1. 사용설명서에 경고 및 안전사항이 기재되어 있는지 확인한다.\n2. 안전하게 사용하는 작동법이 기재되어 있는지 확인한다.\n3. 안전 예방조치, 사용 방법과 문제 해결 방법이 포함되어 있는지 확인한다.\n4. 확인한 문서의 버전과 해당 절을 기록한다.\n'}
    if code=='SSTR':changes.update({
       'K285':'\n검증 [1]\n검사 문서: IFU-TD24A-01, Rev.0(2026.08.12), 260824 형성평가용 초안.\n1. 사용설명서에 경고 및 안전사항이 기재되어 있음을 확인하였다(안전사항, 외부장치 경고/주의: 표시 3, 5~7쪽).\n2. 안전하게 사용하는 작동법이 기재되어 있음을 확인하였다(처음 사용하기, 외부장치 사용: 표시 14, 23~26쪽).\n3. 안전 예방조치, 사용 방법과 문제 해결 방법이 포함되어 있음을 확인하였다(위 안전/사용 절과 문제 해결: 표시 44쪽).\n[E8300 보완] 검사한 초안과 사용자 배포용 설명서의 일치 여부는 미확인.\n',
       'L285':'\n판정 대기\n'})
    edits=[]
    for addr,after in changes.items():
        before=value(cm[addr],ss)
        assert before!=after
        history=value(cc[addr],ss)
        def marked(t,mark):return '\n'.join(mark+' '+line for line in (t.strip().splitlines() if t.strip() else ['(빈칸)']))
        append='\n2026-10-13 RID83 정정\n'+marked(before,'[-]')+'\n'+marked(after,'[+]')+'\n'
        edits.append({'cell':addr,'before':before,'after':after,'history':history,'append':append,'compareAfter':history+append})
    groups=[{'main':main,'compare':compare,'edits':edits,'compareHeights':{'285':409.5} if code!='SRS' else {'93':205,'94':180}}]
    if code=='SSTR':
        groups[0]['compareHeights']['286']=200
        groups[0]['compareMerges']=['K285:K286']
        groups[0]['compareStyleCopies']={'K286':'K285'}
    if code!='SRS':
        main2=next(s for s in tabs if s['name']=='사이버보안 시험 연계');compare2=next(s for s in tabs if s['name']==main2['name']+'_비교')
        _,cm2,ss2=cellmap(d,main2['part']);_,cc2,_=cellmap(d,compare2['part']);before=value(cm2['G38'],ss2);history=value(cc2['G38'],ss2)
        after=before
        for id in [17,18,20]:after=after.replace(f'SYS-RID-ACDOC-{id}\n','')
        append='\n2026-10-13 RID83 정정\n[-] SYS-RID-ACDOC-17/18/20\n[+] RID83 상위 추적 빈칸\n'
        edits2=[{'cell':'G38','before':before,'after':after,'history':history,'append':append,'compareAfter':history+append}]
        replacements={'J38':[('제품 설명서 현재본 및상위 추적.','검사한 초안과 사용자 배포용 설명서의 일치 여부.') ]}
        if code=='SSTR':replacements['K38']=[('변경 통제/확인 자료 및 사용자 설명서.','변경 통제/확인 자료 및 해당 보안 설정의 설명서 안내.'),('최신 QCC 추가 조건:','시험 48/52/76의 최신 QCC 추가 조건:')]
        for addr,pairs in replacements.items():
            before=value(cm2[addr],ss2);history=value(cc2[addr],ss2);after=before;append='\n2026-10-13 RID83 정정\n'
            for old,new in pairs:
                assert old in after;after=after.replace(old,new);append+='[-] '+old+'\n[+] '+new+'\n'
            edits2.append({'cell':addr,'before':before,'after':after,'history':history,'append':append,'compareAfter':history+append})
        groups.append({'main':main2,'compare':compare2,'edits':edits2,'compareHeights':{'38':409.5}})
    books.append({'code':code,'path':str(source),'destination':f['path'],'sourceSHA256':f['sha256'],'groups':groups})
save(H/'plan.json',{'books':books,'reason':'RID83 is a self-derived software requirement; no formal matrix parent is established. Preserve internal ID and test83.'})
print(json.dumps({b['code']:[e['cell'] for g in b['groups'] for e in g['edits']] for b in books}))
