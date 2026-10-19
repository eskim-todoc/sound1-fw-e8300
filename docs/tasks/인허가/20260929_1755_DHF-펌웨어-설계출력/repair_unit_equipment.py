"""유닛 기준서의 장비 표만 최신 보고서의 내용/서식으로 복원한다."""
import copy
import hashlib
import json
import shutil
import sys
import zipfile
from pathlib import Path
from lxml import etree as E
from openpyxl import load_workbook
import preserve_aligned_report_packages as native

TASK=Path(__file__).resolve().parent
DHF=TASK.parents[3]/'docs/참고/인허가/사운드1/DHF'
QA=TASK/'qa/unit-equipment-repair'
TARGET=DHF/'03_설계 출력/SW/펌웨어/SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서_rev0(김은수 작성).xlsx'
SOURCE=DHF/'04_설계 검증/SW/펌웨어/SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서_rev0(김은수 작성).xlsx'
TS,SS='기준서_유닛 검증','보고서_유닛 시험'
N=native.N

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(n,obj):(QA/n).write_text(json.dumps(obj,ensure_ascii=False,indent=2),encoding='utf-8')
def read(n):return json.loads((QA/n).read_text(encoding='utf-8'))
def color(c):
    if c is None:return '#000000'
    if c.type=='rgb':return '#'+c.rgb[-6:]
    return '#000000'

def prepare():
    QA.mkdir(parents=True,exist_ok=False)
    shutil.copy2(TARGET,QA/'criterion-before.xlsx');shutil.copy2(SOURCE,QA/'report-before.xlsx')
    s=load_workbook(SOURCE,rich_text=True)[SS];t=load_workbook(TARGET,rich_text=True)[TS]
    changes=[]
    for row in s.iter_rows(min_row=13,max_row=24,min_col=2,max_col=6):
        for c in row:
            if c.value is None:continue
            f=c.font;a=c.alignment
            fmt=dict(font=dict(name=f.name,size=f.sz,bold=bool(f.bold),italic=bool(f.italic),color=color(f.color)),
                     wrapText=bool(a.wrapText),horizontalAlignment=a.horizontal or 'left',verticalAlignment=a.vertical or 'top')
            changes.append(dict(cell=c.coordinate,value=str(c.value),format=fmt))
    protected={str(p):digest(p) for p in DHF.rglob('*') if p.is_file() and not p.name.startswith('~$') and p.suffix in {'.xlsx','.docx'} and p!=TARGET}
    plan=dict(target=str(TARGET),source=str(SOURCE),targetHash=digest(TARGET),sourceHash=digest(SOURCE),sheet=TS,sourceSheet=SS,
              changes=changes,protected=protected,heights={str(r):s.row_dimensions[r].height for r in range(13,25)})
    assert len(changes)==21
    assert set(str(m) for m in t.merged_cells.ranges if 15<=m.min_row<=24)==set(str(m) for m in s.merged_cells.ranges if 15<=m.min_row<=24)
    save('plan.json',plan);print('장비 표/제목 B13:F24, 21개 값/60개 셀 서식만 복원')

def amend():
    """보존된 원본에서 제목 행도 복원 범위에 포함한다. 원본은 덮어쓰지 않는다."""
    p=read('plan.json');s=load_workbook(QA/'report-before.xlsx',rich_text=True)[SS]
    c=s['B13'];f=c.font;a=c.alignment
    p['changes'].insert(0,dict(cell='B13',value=str(c.value),format=dict(
        font=dict(name=f.name,size=f.sz,bold=bool(f.bold),italic=bool(f.italic),color=color(f.color)),
        wrapText=bool(a.wrapText),horizontalAlignment=a.horizontal or 'left',verticalAlignment=a.vertical or 'top')))
    p['heights'].update({str(r):s.row_dimensions[r].height for r in [13,14]})
    save('plan.json',p)

def preserve():
    p=read('plan.json');target=native.parts(QA/'criterion-before.xlsx');source=native.parts(QA/'report-before.xlsx')
    # 보고서의 theme minor가 대상 테마로 바뀌지 않도록 글꼴 이름을 고정한다.
    styles=native.parse(source['xl/styles.xml'])
    for scheme in styles.findall('.//s:scheme',N):
        if scheme.getparent().find('s:name',N).get('val') in {'Calibri','Calibri Light'}:scheme.getparent().remove(scheme)
    source['xl/styles.xml']=native.serialize(styles)
    imp=native.Importer(target,source,'equipment_report')
    sp=native.sheet_path(source,SS);tp=native.sheet_path(target,TS)
    sr=imp.convert_sheet(native.parse(source[sp]));root=native.parse(target[tp])
    authored=load_workbook(QA/'criterion-authored.xlsx',rich_text=True)[TS]
    for ch in p['changes']:assert str(authored[ch['cell']].value)==ch['value']
    for r in range(13,25):
        tr=root.find(f's:sheetData/s:row[@r="{r}"]',N)
        for col in ['B','C','D','E','F']:
            addr=f'{col}{r}';old=tr.find(f's:c[@r="{addr}"]',N);new=sr.find(f'.//s:c[@r="{addr}"]',N)
            if old is not None:tr.remove(old)
            if new is not None:
                at=next((i for i,c in enumerate(tr) if native.col(c.get('r'))>native.col(addr)),len(tr))
                tr.insert(at,copy.deepcopy(new))
        height=p['heights'][str(r)]
        if height is not None:tr.set('ht',str(height));tr.set('customHeight','1')
    target[tp]=native.serialize(root);imp.flush()
    with zipfile.ZipFile(QA/'criterion-candidate.xlsx','w',zipfile.ZIP_DEFLATED) as z:
        for name,data in target.items():z.writestr(name,data)
    print('원본 다른 영역/표지/병합 유지, 보고서 표의 정확한 서식 반영')

def fit():
    """보고서의 서식은 유지하며 대상 표에서 필요한 높이만 확보한다."""
    import math
    p=read('plan.json');checks=read('native-checks.json')['checks']
    for item in checks:
        row=''.join(c for c in item['cell'] if c.isdigit())
        p['heights'][row]=max(p['heights'][row] or 15,math.ceil((item['needed']+3)/1.5)*1.5)
    save('plan.json',p);print('Excel 높이에 맞춰 표 행 높이 보완',p['heights'])

def verify():
    p=read('plan.json');old=load_workbook(QA/'criterion-before.xlsx',rich_text=True);new=load_workbook(QA/'criterion-candidate.xlsx',rich_text=True)
    source=load_workbook(QA/'report-before.xlsx',rich_text=True)[SS];count=0;scope=[]
    assert old.sheetnames==new.sheetnames
    for a,b in zip(old,new):
        assert a.max_row==b.max_row and a.max_column==b.max_column
        assert set(map(str,a.merged_cells.ranges))==set(map(str,b.merged_cells.ranges))
        for row in a:
            for c in row:
                count+=1;n=b[c.coordinate]
                in_table=a.title==TS and 13<=c.row<=24 and 2<=c.column<=6
                if not in_table:
                    assert c.value==n.value,(a.title,c.coordinate,'value')
                    for prop in ['font','alignment','border','fill','protection']:
                        assert copy.copy(getattr(c,prop))==copy.copy(getattr(n,prop)),(a.title,c.coordinate,prop)
                    assert c.number_format==n.number_format
                else:
                    sc=source[c.coordinate]
                    assert str(n.value or '')==str(sc.value or ''),c.coordinate
                    for prop in ['alignment','border','fill','protection']:
                        assert copy.copy(getattr(n,prop))==copy.copy(getattr(sc,prop)),(c.coordinate,prop)
                    if sc.value is not None:
                        f=copy.copy(sc.font)
                        if f.name in {'Calibri','Calibri Light'}:f.scheme=None
                        assert copy.copy(n.font)==f,(c.coordinate,'font')
                    assert n.number_format==sc.number_format
                    scope.append(c.coordinate)
        assert {k:dict(v) for k,v in a.column_dimensions.items()}=={k:dict(v) for k,v in b.column_dimensions.items()}
        for r,d in a.row_dimensions.items():
            wanted=p['heights'].get(str(r),d.height) if a.title==TS else d.height
            assert b.row_dimensions[r].height==(wanted if wanted is not None else d.height),(a.title,r)
    bp=native.parts(QA/'criterion-before.xlsx');np=native.parts(QA/'criterion-candidate.xlsx')
    assert set(bp)==set(np)
    changed=[name for name in bp if bp[name]!=np[name]]
    assert set(changed)<={native.sheet_path(bp,TS),'xl/styles.xml','[Content_Types].xml'},changed
    for path,h in p['protected'].items():assert digest(path)==h,path
    data=dict(checkedCells=count,restoredCells=scope,otherValuesAndStylesPreserved=True,sourceUnchanged=digest(SOURCE)==p['sourceHash'],packageChanges=changed,candidateHash=digest(QA/'criterion-candidate.xlsx'))
    save('verification.json',data);print(json.dumps(data,ensure_ascii=False,indent=2))

def publish():
    p=read('plan.json');v=read('verification.json')
    expected=read('publication.json')['sha256'] if (QA/'publication.json').exists() else p['targetHash']
    assert digest(TARGET)==expected,'대상 파일에 외부 편집이 있다'
    assert read('native-checks.json')['heightShortage']==0,'Excel 표시 검사가 완료되지 않았다'
    assert digest(SOURCE)==p['sourceHash']
    assert digest(QA/'criterion-candidate.xlsx')==v['candidateHash']
    for path,h in p['protected'].items():assert digest(path)==h,path
    stage=TARGET.with_name(TARGET.name+'.tdc-stage');assert not stage.exists()
    shutil.copy2(QA/'criterion-candidate.xlsx',stage);stage.replace(TARGET)
    assert digest(TARGET)==v['candidateHash']
    save('publication.json',dict(path=str(TARGET),sha256=digest(TARGET)));print('기준서 기존 파일명으로 저장 완료. 보고서 불변')

if __name__=='__main__':globals()[sys.argv[1]]()
