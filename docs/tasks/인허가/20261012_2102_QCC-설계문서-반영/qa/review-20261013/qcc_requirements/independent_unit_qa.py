from pathlib import Path
from lxml import etree as E
from PIL import Image
import json, hashlib, zipfile, re, datetime, posixpath

HERE=Path(__file__).resolve().parent
QA=HERE.parent
NS={'x':'http://schemas.openxmlformats.org/spreadsheetml/2006/main', 'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def active(v):
    if '[-] ' not in v and '[+] ' not in v: return v
    lines=[]
    for line in v.splitlines():
        if line.startswith('[-] '): continue
        if line.startswith('[+] '): line=line[4:]
        if line: lines.append(line)
    return '\n'+'\n'.join(lines)+'\n' if lines else ''

native=json.loads((QA/'native-excel/tests-native.json').read_text(encoding='utf-8'))
specs={v['code']:v for v in native if v['code'] in ['SUVC-main','SUVR-main']}
def read(spec):
    p=Path(spec['path']); before=sha(p)
    with zipfile.ZipFile(p) as z:
        workbook=E.fromstring(z.read('xl/workbook.xml'))
        rels=E.fromstring(z.read('xl/_rels/workbook.xml.rels'))
        rels={r.get('Id'):r.get('Target') for r in rels}
        sheet=next(s for s in workbook.findall('x:sheets/x:sheet',NS) if s.get('name')==spec['sheet'])
        target=rels[sheet.get('{'+NS['r']+'}id')]
        member=target.lstrip('/') if target.startswith('/') else posixpath.normpath('xl/'+target)
        root=E.fromstring(z.read(member)); styles=E.fromstring(z.read('xl/styles.xml'))
        strings=list(E.fromstring(z.read('xl/sharedStrings.xml'))) if 'xl/sharedStrings.xml' in z.namelist() else []
        cells={c.get('r'):c for c in root.findall('.//x:sheetData/x:row/x:c',NS)}
        def payload(c):
            v=c.find('x:v',NS)
            if c.get('t')=='s': return strings[int(v.text)]
            if c.get('t')=='inlineStr': return c.find('x:is',NS)
            return None
        def text(c):
            v=c.find('x:v',NS); s=payload(c)
            return ''.join(s.xpath('.//x:t/text()',namespaces=NS)) if s is not None else v.text if v is not None else ''
        def rich(c):
            s=payload(c)
            if s is None:return []
            out=[]
            for x in s:
                if E.QName(x).localname=='r':
                    pr=x.find('x:rPr',NS)
                    out.append((x.findtext('x:t','',NS),E.tostring(pr).decode() if pr is not None else None))
                elif E.QName(x).localname=='t':out.append((x.text,None))
            return out
        def style(c):
            xf=styles.find('x:cellXfs',NS)[int(c.get('s','0'))]
            parts={'xf':dict(xf.attrib),'alignment':E.tostring(xf.find('x:alignment',NS)).decode() if xf.find('x:alignment',NS) is not None else None}
            # xfId is a workbook-local cell-style index, not a rendered property.
            # These cells explicitly apply font/fill/border/alignment; compare
            # the actual applied properties rather than unrelated index values.
            parts['xf'].pop('xfId',None)
            for key,arr in [('fontId','fonts'),('fillId','fills'),('borderId','borders')]:
                parts[key]=E.tostring(styles.find('x:'+arr,NS)[int(xf.get(key,'0'))]).decode()
                parts['xf'].pop(key,None)
            return parts
        def cell(a):
            c=cells.get(a)
            return {'text':text(c),'rich':rich(c),'style':style(c),'formula':c.findtext('x:f',None,NS)} if c is not None else {'text':'','rich':[],'style':None,'formula':None}
        values={a:text(c) for a,c in cells.items()}
        snapshot={a:cell(a) for a in cells if 1727<=int(re.sub(r'\D','',a))<=1764}
        merges=[m.get('ref') for m in root.findall('x:mergeCells/x:mergeCell',NS)]
        rows={int(r.get('r')):dict(r.attrib) for r in root.findall('x:sheetData/x:row',NS)}
        return {'path':str(p),'before':before,'after':sha(p),'cell':cell,'values':values,'snapshot':snapshot,'merges':merges,'rows':rows,'part':member}

c=read(specs['SUVC-main']); r=read(specs['SUVR-main'])
diff=[]
for row in range(30,1765):
    for col in 'BCDEFGHIJK':
        a=col+str(row); left=c['cell'](a); right=r['cell'](a)
        for k in ['text','rich','formula']:
            if left[k]!=right[k]: diff.append({'cell':a,'part':k})
newstyle=[]
for row in range(1727,1765):
    for col in 'BCDEFGHIJK':
        a=col+str(row)
        if c['cell'](a)['style']!=r['cell'](a)['style']:newstyle.append(a)
def common_merges(merges):
    result=[]
    for m in merges:
        a,b=m.split(':') if ':' in m else (m,m)
        ca=re.sub(r'\d','',a); cb=re.sub(r'\d','',b)
        ra=int(re.sub(r'\D','',a)); rb=int(re.sub(r'\D','',b))
        if 'B'<=ca<='K' and 'B'<=cb<='K' and ra>=30 and rb<=1764:result.append(m)
    return sorted(result)
groups=[]
starts=[n for n in range(1727,1765) if active(c['values'].get('D'+str(n),'' )).strip().startswith('SW-FW-')]
for i,start in enumerate(starts):
    end=starts[i+1]-1 if i+1<len(starts) else 1764
    conditions=[]; validations=[]
    states=[]
    for n in range(start,end+1):
        expected=active(c['values'].get('K'+str(n),'')); observed=active(r['values'].get('L'+str(n),'')); verdict=active(r['values'].get('M'+str(n),''))
        conditions+=list(map(int,re.findall(r'(?:조건|검증)\s*\[(\d+)\]',expected)))
        validations+=list(map(int,re.findall(r'검증\s*\[(\d+)\]',observed)))
        states.append({'row':n,'expected':expected,'result':observed,'verdict':verdict,'same_condition':set(re.findall(r'(?:조건|검증)\s*\[(\d+)\]',expected))==set(re.findall(r'검증\s*\[(\d+)\]',observed)),'actual_unexecuted':('미실행' in verdict and '판정' in verdict and '통과' not in verdict)})
    groups.append({'id':active(c['values']['D'+str(start)]).strip(),'unit':active(c['values'].get('G'+str(start),'' )).strip(),'rows':[start,end],'conditions':conditions,'validations':validations,'states':states})

review={'SUVC-main':{'qcc':list(range(239,257)),'samples':[4,102,103,109,136]},'SUVR-main':{'qcc':list(range(190,206)),'samples':[4,81,82,87,108]}}
pages=[]
for key,which in review.items():
    folder=QA/'native-excel/tests'/key
    pjson=json.loads((folder/'pages.json').read_text(encoding='utf-8'))
    for p in which['qcc']+which['samples']:
        path=folder/f'page-{p:03}.png'
        with Image.open(path) as img: pixel=hashlib.sha256(img.convert('RGB').tobytes()).hexdigest().upper();size=list(img.size)
        pages.append({'code':key,'page':p,'range':'QCC 38조건' if p in which['qcc'] else '긴 RTT/정량 기준 표본','png':str(path),'sha256':sha(path),'pixelSHA256':pixel,'size':size,'mtime':datetime.datetime.fromtimestamp(path.stat().st_mtime).astimezone().isoformat(),'direct_original_size_viewed':True,'clipped_text_or_border':'없음','condition_and_result_scope':'동일 조건/현재 판정 대기 유지' if p in which['qcc'] else '기존 E8300 관찰 범위 유지'})

out={'checkedAt':datetime.datetime.now().astimezone().isoformat(),'owner':'qcc_requirements / 독립 읽기 전용 검토','scope':'SUVC/SUVR 마지막 QCC 38조건 전체 표시와 긴 RTT/정량 표본. 유닛 전체 461쪽 검토로 확대하지 않는다.','sources':[{**specs[k],'read_only_before':v['before'],'read_only_after':v['after'],'unchanged':v['before']==v['after'],'current_matches_native_metadata':v['before']==specs[k]['sourceSHA256']} for k,v in [('SUVC-main',c),('SUVR-main',r)]],'common':{'range':'B30:K1764','cells_checked':1735*10,'value_rich_formula_differences':diff,'common_merges_equal':common_merges(c['merges'])==common_merges(r['merges']),'new38style_differences':newstyle,'new38heights_equal':all(c['rows'][n].get('ht')==r['rows'][n].get('ht') for n in range(1727,1765)),'new38height_max':max(float(c['rows'][n].get('ht','0')) for n in range(1727,1765))},'groups':groups,'group_count':len(groups),'condition_count':sum(len(g['states']) for g in groups),'units':sorted(set(re.findall(r'SU-ID-QCC-\d{3}','\n'.join(g['unit'] for g in groups)))),'page_count':len(pages),'pages':pages,'native_current_link':'D24 전역 안내만 수정되고 이번 검토 PNG는 이전 렌더다. root 새 PNG의 픽셀 동일성을 확인한 뒤 현재 해시에 연결해야 한다.','status':'통과: 이 검토 범위의 표시/동일 조건 대응. 재렌더 픽셀 연결 대기. 제품 QCC 시험은 미실행/판정 대기.','findings':[],'semantic_limits':['QCC 실제 로그/측정/관찰 부재는 38조건 모두 판정에서 유지한다.','AUTH 12-byte 중계 확인과 CRA 계산 검증을 구별한다.','Classic I2S 수신/자극은 DMIC2 통화 송신의 통과가 아니다.','SPI 배터리 패킷 길이/Left-Right 대응/인증 타임아웃 경계/키 파생 및 NVM 보호 근거는 미비 설명과 함께 남는다.','기존 E8300 RTT 시험자 확인 결과는 해당 입력/예상 조건에서만 보존한다.']}
print(json.dumps({'diagnostic':{'diff_count':len(diff),'diff_first':diff[:10],'new_style_count':len(newstyle),'newstyle_first':newstyle[:10],'merges_equal':out['common']['common_merges_equal'],'height_equal':out['common']['new38heights_equal'],'units':out['units'],'group_count':len(groups),'condition_count':out['condition_count'],'bad_states':[(g['id'],s['row'],s['verdict']) for g in groups for s in g['states'] if not s['same_condition'] or not s['actual_unexecuted']]}},ensure_ascii=False))
(HERE/'independent-unit-native-qa.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
assert not diff and not newstyle and out['common']['common_merges_equal'] and out['common']['new38heights_equal']
assert out['group_count']==15 and out['condition_count']==38 and len(out['units'])==14
assert all(s['same_condition'] and s['actual_unexecuted'] for g in groups for s in g['states'])
(HERE/'independent-unit-native-qa.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:out[k] for k in ['group_count','condition_count','page_count','common','units']},ensure_ascii=False))
