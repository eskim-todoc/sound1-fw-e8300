"""원본/후보 XLSX의 원시 XML로 프레임 수치와 결과 연결을 별도 검사한다."""
import hashlib
import json
import re
from pathlib import Path
from zipfile import ZipFile
from lxml import etree as E

QA=Path(__file__).resolve().parent/'qa/integration-readable-frames'
NS={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}

def contents(path):
    with ZipFile(path) as z:
        assert z.testzip() is None
        ss=E.fromstring(z.read('xl/sharedStrings.xml')) if 'xl/sharedStrings.xml' in z.namelist() else []
        table=E.fromstring(z.read('xl/worksheets/sheet2.xml'))
        out={}
        for c in table.findall('.//s:c',NS):
            if c.get('t')=='s': value=''.join(ss[int(c.find('s:v',NS).text)].itertext())
            elif c.get('t')=='inlineStr':value=''.join(c.find('s:is',NS).itertext())
            else:
                v=c.find('s:v',NS);value=v.text if v is not None else ''
            out[c.get('r')]=value
        return out

checks=[]
for key in ['report','criterion']:
    before=contents(QA/f'{key}-before.xlsx');after=contents(QA/f'{key}-candidate.xlsx')
    for row in range(107,111):
        address=f'M{row}'
        old=json.loads(re.search(r'해석 프레임24개=(\[.*?\])\.',before[address]).group(1))
        lines=re.findall(r'^프레임 (\d+)~(\d+): 전극 (\d+)\(크기 (\d+)\) → NOP → NOP$',after[address],re.M)
        assert len(lines)==8
        actual=[]
        for i,(start,end,electrode,amplitude) in enumerate(lines):
            assert (int(start),int(end))==(3*i+1,3*i+3)
            actual.extend([f'E{electrode}/A{amplitude}','NOP','NOP'])
        assert actual==old and actual.count('NOP')==16
        assert re.search(r'phase ([01])→([01])',before[address]).groups()==re.search(r'phase ([01])→([01])',after[address]).groups()
        if key=='report':
            electrodes=[int(x[2]) for x in lines]
            output=[int(v) for v in re.search(r'해석한 전극 순서=\[([^]]+)\]',after[f'N{row}']).group(1).split(',')]
            assert electrodes==output
            assert '총8자극/16NOP/24프레임' in after[f'N{row}']
        checks.append(dict(book=key,cell=address,frameCount=24,stimuli=8,nop=16,orderPreserved=True))
    with ZipFile(QA/f'{key}-candidate.xlsx') as z:
        for name in z.namelist():
            if name.startswith('xl/worksheets/sheet') and name.endswith('.xml'):
                assert not E.fromstring(z.read(name)).findall('.//s:f',NS),('formula',name)
            assert not name.startswith('xl/externalLinks/'),name
    assert not any(re.search(r'\["E\d+/A\d+"',value) for value in after.values())
(QA/'independent-verification.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
print('별도 XML 검사: 8셀/192프레임 순서 및 NOP 수 유지, 결과 전극 순서 일치, 수식/외부 연결 0')
