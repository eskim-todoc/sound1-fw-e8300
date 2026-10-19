"""작성기와 분리해 XLSX XML을 읽는 내용/패키지 검토. 파일은 수정하지 않는다."""
from pathlib import Path
import json
import re
import zipfile
import hashlib
from lxml import etree as E

TASK=Path(__file__).resolve().parent
QA=TASK/'qa/integration-system-rev0'
NS={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
R='{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id'

def sheets(path):
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        w=E.fromstring(z.read('xl/workbook.xml'));rels=E.fromstring(z.read('xl/_rels/workbook.xml.rels'))
        shared=[]
        if 'xl/sharedStrings.xml' in z.namelist():
            shared=[''.join(n.itertext()) for n in E.fromstring(z.read('xl/sharedStrings.xml'))]
        result={}
        for s in w.find('s:sheets',NS):
            target=next(r.get('Target') for r in rels if r.get('Id')==s.get(R))
            path=target.lstrip('/') if target.startswith('/') else 'xl/'+target
            root=E.fromstring(z.read(path));cells={}
            for c in root.findall('.//s:sheetData/s:row/s:c',NS):
                if c.get('t')=='s':value=shared[int(c.find('s:v',NS).text)]
                elif c.get('t')=='inlineStr':value=''.join(c.find('s:is',NS).itertext())
                else:
                    n=c.find('s:v',NS);value=n.text if n is not None else ''
                cells[c.get('r')]=value
            result[s.get('name')]=(root,cells)
        return result

def verify():
    report={}
    for key,total,idcol,expectedcol,resultcol,verdictcol in [('integration',17,'C','M','N','O'),('system',81,'B','J','K','L')]:
        path=QA/f'{key}-candidate.xlsx'
        if not path.exists():continue
        books=sheets(path);main=next(n for n in books if n.startswith('보고서_'));root,cells=books[main]
        ids={int(re.search(r'\d+',a)[0]):int(v.strip().rsplit('-',1)[1]) for a,v in cells.items() if re.fullmatch(idcol+r'\d+',a) and v.strip().startswith('SW-FW-')}
        assert set(ids.values())==set(range(1,total+1))
        assert all(cells[verdictcol+str(r)].strip()=='통과' for r in ids)
        limits=[]
        for node in root.findall('s:sheetData/s:row',NS):
            assert float(node.get('ht','0'))<=409.5
        for a,value in cells.items():
            if re.fullmatch(resultcol+r'\d+',a) and int(re.search(r'\d+',a)[0])>=min(ids):
                assert not re.search(r'기존 기록 불일치|결과 대조 필요|결과 기입 필요|출력:\s*출력:|^(?:RTT C\d|검증 \[\])',value,re.M),(key,a)
        if key=='integration':
            assert set(books)=={'표지','보고서_통합시험'}
            conditions=[]
            for r,tid in sorted(ids.items()):
                end=next((n for n in sorted(ids) if n>r),131)
                for row in range(r,end):
                    a=cells[expectedcol+str(row)];b=cells[resultcol+str(row)]
                    m=re.match(r'\s*조건 \[(\d+)\]',a);n=re.match(r'\s*검증 \[(\d+)\]',b)
                    assert m and n and m[1]==n[1],(tid,row)
                    conditions.append((tid,int(m[1]),a,b))
            assert len(conditions)==106
            by={(t,c):(a,b) for t,c,a,b in conditions}
            def has(t,c,text):assert text in by[t,c][1],(t,c,text)
            has(3,4,'복사 후 조회값=1');has(3,4,'해제 후=0')
            mix=[x>>5 for x in [0,-1024,544,-3200,3168]+[0]*11]
            has(5,1,'['+','.join(map(str,mix))+']');has(5,1,'maxAudioInput=100')
            has(9,3,'C[0..31]='+str(((300-100)*20871)//32768));has(9,3,'offset 코드=44')
            has(9,6,'75300pC');has(9,6,'반환=0')
            has(11,1,'180000, 자동 종료 요청 없음');has(11,2,'카운터=0')
            has(12,5,'플래그=0');has(12,5,'[0x36,1]')
            expected_mix=list(range(1,16))+[32]
            has(15,1,'['+','.join(map(str,expected_mix))+']')
            order=[v for pair in zip(range(8),range(8,16)) for v in pair]
            has(15,4,str(order).replace(' ',''));has(15,5,str([15]+order[:-1]).replace(' ',''))
            for case,values in [(6,order[:8]),(7,order[8:]),(8,([15]+order[:-1])[:8]),(9,([15]+order[:-1])[8:])]:has(15,case,str(values).replace(' ',''))
            has(16,5,'-1/-1/-1');has(17,6,'[240,3]');has(17,6,'[240,4]')
            for _,_,a,b in conditions:
                for log in re.findall(r'^\[UT-[^\r\n]+',a,re.M):assert log in b
            report[key]=dict(tests=total,conditions=106,numericChecks=22,conditionAndRttLinks=True)
        else:
            assert '시험 보조자료' in books and '예상 그림' in books
            condition_count=0;log_count=0;paired={}
            for row,tid in sorted(ids.items()):
                end=next((n for n in sorted(ids) if n>row),282)
                expected='\n'.join(cells.get('J'+str(n),'') for n in range(row,end))
                result='\n'.join(cells.get('K'+str(n),'') for n in range(row,end))
                def split(text,label):
                    matches=list(re.finditer(r'(?:^|\n)'+label+r' \[(\d+)\](?!의)',text))
                    return [(int(m[1]),text[m.start():matches[i+1].start() if i+1<len(matches) else len(text)].strip()) for i,m in enumerate(matches)]
                lhs=split(expected,'조건');rhs=split(result,'(?:검증|측정)')
                assert [n for n,t in lhs]==[n for n,t in rhs] and lhs,tid
                for (n,a),(_,b) in zip(lhs,rhs):
                    assert a!=b and len(b)>20,(tid,n)
                    assert not re.search(r'해야 한다|기록한다|확인한다|저장한다|____',b),(tid,n,b)
                    raw=lambda text:re.findall(r'^\[(?:UT-[^\]]+|BT)\][^\r\n]+',text,re.M)
                    if raw(a):assert raw(a)==raw(b),(tid,n,'log gap')
                    log_count+=len(raw(b));condition_count+=1;paired[tid,n]=(a,b)
            assert condition_count==246
            # 작업자의 상세 검토와 별개로 과거 누락/입출력 혼동 위치를 재검사한다.
            assert '3' in paired[26,2][1]
            for tid,values in [(31,[2,6,9]),(32,[2,3,4])]:
                for n,value in enumerate(values,1):assert str(value) in paired[tid,n][1],(tid,n,value)
            assert 'copy_done=1' in paired[15,4][1] and '해제 후=0' in paired[15,4][1]
            assert '[BT] CMD 0x34, CHARGER STATE: 1, BATT LEVEL 37 PERCENT, LID STATE 1' in paired[63,3][1]
            assert '[BT] CMD 0x34, CHARGER STATE: 0, BATT LEVEL 72 PERCENT, LID STATE 2' in paired[63,3][1]
            helper=books['시험 보조자료'][1];helper_count=0
            normalize=lambda v:re.sub(r'\s+','',v)
            for row in range(6,299):
                expected=helper.get('B'+str(row),'');match=re.match(r'\s*조건 \[(\d+)\]',expected)
                if not match:continue
                tid=int(helper['A'+str(row)].strip().rsplit('-',1)[1]);number=int(match[1])
                a,b=paired[tid,number]
                assert normalize(expected) in normalize(a),('helper criterion mismatch',tid,number,row)
                actual=helper.get('C'+str(row),'').split('사진/자료 안내')[0].strip()
                assert normalize(actual) in normalize(b),('helper result mismatch',tid,number,row,actual)
                helper_count+=1
            assert helper_count==246
            qcc=[47,48,49,52,71,72,76,79]
            for tid in qcc:
                row=next(r for r,v in ids.items() if v==tid)
                end=next((n for n in sorted(ids) if n>row),row+1)
                assert any('(QCC 검증 결과 필요)' in cells.get('K'+str(n),'') for n in range(row,end)),tid
            photo=sum('(사진필요)' in v for a,v in cells.items() if re.fullmatch(r'K\d+',a) and int(a[1:])>=34)
            # 사진 안내는 파란 굵은 글꼴, QCC 자료 안내는 빨간 굵은 글꼴.
            styled_markers=0
            for name,(sheet,_) in books.items():
                for run in sheet.findall('.//s:c/s:is/s:r',NS):
                    text=''.join(run.findall('s:t',NS)[0].itertext())
                    color='FFC00000' if '(QCC 검증 결과 필요)' in text else ('FF0000FF' if '(사진필요)' in text else None)
                    if color:
                        styled_markers+=1
                        assert run.find('s:rPr/s:b',NS) is not None
                        assert run.find('s:rPr/s:color',NS).get('rgb')==color
            assert styled_markers>=photo+len(qcc),('evidence markers not styled',styled_markers,photo)
            def media_hashes(file):
                with zipfile.ZipFile(file) as z:return sorted(hashlib.sha256(z.read(n)).hexdigest() for n in z.namelist() if n.startswith('xl/media/'))
            assert media_hashes(path)==media_hashes(QA/'system-before.xlsx')
            report[key]=dict(tests=total,conditions=condition_count,rawLogLines=log_count,numericAndStateChecks=10,
                qccAttachmentPositions=len(qcc),photoResultCells=photo,helperConditionsMatched=helper_count,sourceImagesPreserved=True,photoAndQccColors=True)
    (QA/'independent-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))

if __name__=='__main__':verify()
