"""Read saved packages and register current missing-data locations in the central MD."""
from pathlib import Path
from zipfile import ZipFile
from collections import defaultdict
from urllib.parse import quote
from lxml import etree
import argparse,json,re,sys
sys.stdout.reconfigure(encoding='utf-8')
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
S='http://schemas.openxmlformats.org/spreadsheetml/2006/main'
R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
N={'w':W,'s':S}
PREFIXES=['[QCC 자료 입력]','[E8300 보완]','[QCC/E8300 자료 입력]']
baseline=json.loads((HERE/'evidence/source-baseline.json').read_text(encoding='utf-8-sig'))
STAMP='2026-10-13'
parser=argparse.ArgumentParser()
parser.add_argument('--record',type=Path,default=HERE/'qa/review-20261013/current-positions.json')
parser.add_argument('--clean-main',action='store_true')
args=parser.parse_args()
RECORD=args.record
records=[]
def active_excel(value):
    keep=True; fragments=[]
    for part in re.split(r'(\[-\]|\[\+\])',value):
        if part=='[-]': keep=False
        elif part=='[+]': keep=True
        elif keep: fragments.append(part)
    return ''.join(fragments).strip()
def text(node):
    def okay(t):
        for a in t.iterancestors():
            if a.tag==f'{{{W}}}del': return False
            if a.tag==f'{{{W}}}tr' and a.find('w:trPr/w:del',N) is not None: return False
        return True
    return ''.join(t.text or '' for t in node.findall('.//w:t',N) if okay(t))
def link(path): return quote(str(path).replace('\\','/'),safe='/:')
for item in baseline:
    p=Path(item['path'].replace('[E8300]','[E8300-QCC]'))
    completed=p.with_stem(p.stem+'_완료')
    if completed.exists(): p=completed
    if p==Path(item['path']) or not p.exists(): continue
    code=re.search(r'\] (\w+)-',p.name)[1]
    if code not in ['SR','SGR','SRS','SAD','SDDD','SIP','SUVC','SUVR','SITC','SITR','SSTC','SSTR']: continue
    entries=[]
    with ZipFile(p) as z:
        if p.suffix=='.docx':
            doc=etree.fromstring(z.read('word/document.xml'))
            styles=etree.fromstring(z.read('word/styles.xml'))
            headings={e.get(f'{{{W}}}styleId') for e in styles.findall('w:style',N) if e.find('w:pPr/w:outlineLvl',N) is not None}
            body=doc.find('w:body',N); heading='본문'; table=0
            for b in body:
                if b.tag==f'{{{W}}}sdt': continue # Generated TOC is not a current insertion location.
                if b.tag==f'{{{W}}}p':
                    t=text(b)
                    style=b.find('w:pPr/w:pStyle',N)
                    if style is not None and style.get(f'{{{W}}}val') in headings and t.strip(): heading=t.strip()
                    for prefix in PREFIXES:
                        if prefix in t: entries.append({'prefix':prefix,'location':heading,'text':t[t.index(prefix):]})
                    if code in ('SR','SGR') and p.stem.endswith('_완료'):
                        red=[]
                        for run in b.findall('.//w:r',N):
                            if any(a.tag==f'{{{W}}}del' for a in run.iterancestors()): continue
                            color=run.find('w:rPr/w:color',N); bold=run.find('w:rPr/w:b',N)
                            if color is not None and color.get(f'{{{W}}}val')=='C00000' and bold is not None and bold.get(f'{{{W}}}val','1') not in ('0','false','off'):
                                red.append(text(run))
                        if ''.join(red).strip(): entries.append({'prefix':'QCC 반영 내용(굵은 빨강)','location':heading,'text':''.join(red)})
                        if t.strip().endswith('미확정이다.'):
                            entries.append({'prefix':'접두어 해제 후 미확정','location':heading,'text':t})
                elif b.tag==f'{{{W}}}tbl':
                    table+=1
                    for row in b.findall('w:tr',N):
                        if row.find('w:trPr/w:del',N) is not None: continue
                        cells=[text(c) for c in row.findall('w:tc',N)]
                        ids=re.findall(r'(?:SW-FW-RID-\d+|SU-ID-QCC-\d+|SI-ID-QCC-\d+)', ' '.join(cells))
                        primary=[s for s in ids if s.startswith('SW-FW-RID-')] or ids
                        label=' / '.join(dict.fromkeys(primary)) or (cells[0].strip()[:65] if cells else '')
                        for c in cells:
                            for prefix in PREFIXES:
                                if prefix in c: entries.append({'prefix':prefix,'location':heading+' > '+label,'text':c[c.index(prefix):]})
        else:
            book=etree.fromstring(z.read('xl/workbook.xml'))
            rel=etree.fromstring(z.read('xl/_rels/workbook.xml.rels'))
            targets={e.get('Id'):e.get('Target') for e in rel}
            strings=[]
            if 'xl/sharedStrings.xml' in z.namelist():
                strings=[''.join(e.itertext()) for e in etree.fromstring(z.read('xl/sharedStrings.xml')).findall('s:si',N)]
            tabs=book.findall('s:sheets/s:sheet',N)
            main=next(t.get('name') for t in tabs if t.get('name')!='표지')
            for tab in tabs:
                if tab.get('name') not in (main,'사이버보안 시험 연계'): continue
                target=targets[tab.get(f'{{{R}}}id')]
                target=target.lstrip('/') if target.startswith('/') else 'xl/'+target
                sheet=etree.fromstring(z.read(target))
                for c in sheet.findall('.//s:sheetData/s:row/s:c',N):
                    v=c.find('s:v',N); t=(v.text or '') if v is not None else ''
                    if c.get('t')=='s': t=strings[int(t)]
                    elif c.get('t')=='inlineStr': t=''.join(c.find('s:is',N).itertext())
                    if args.clean_main and re.search(r'\[[-+]\]',t):
                        raise ValueError(f'Editorial marker remains in current main sheet: {code}/{tab.get("name")}/{c.get("r")}')
                    t=active_excel(t)
                    for prefix in PREFIXES:
                        if prefix in t: entries.append({'prefix':prefix,'location':tab.get('name')+'!'+c.get('r'),'text':t[t.index(prefix):]})
    records.append({'code':code,'path':str(p),'entries':entries})
RECORD.write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
order=['SR','SGR','SRS','SAD','SDDD','SIP','SUVC','SUVR','SITC','SITR','SSTC','SSTR']
records.sort(key=lambda r:order.index(r['code']))
def compact(locations):
    cells=defaultdict(set); other=[]
    for location in dict.fromkeys(locations):
        m=re.fullmatch(r'(.+)!([A-Z]+)(\d+)',location)
        if m: cells[(m[1],m[2])].add(int(m[3]))
        else: other.append(location)
    for (sheet,col),rows in cells.items():
        numbers=sorted(rows); groups=[]; start=last=numbers[0]
        for number in numbers[1:]:
            if number==last+1: last=number; continue
            groups.append(f'{col}{start}'+(f':{col}{last}' if last!=start else '')); start=last=number
        groups.append(f'{col}{start}'+(f':{col}{last}' if last!=start else ''))
        other.append(sheet+'!'+', '.join(groups))
    return other
lines=['<!-- E8300-QCC-CURRENT:BEGIN -->','## E8300-QCC 현재본 — '+STAMP,
'', '기존 `[E8300]` 문서와 `QCC_` 원문은 보존했다. 아래 12개 사본은 제공된 QCC 설계를 반영한 현재본이다. 원문의 스마트케이스/CC는 미갱신 표기로 보고 외부기 QCC에 대응시켰다. 빈 추적표를 새로운 기능으로 옮기지 않았다. 기존 날짜별 `[E8300]` 기록은 그 원본의 이력이며 아래 사본의 현재 위치와 구분한다.',
'', '현재 활성 요구사항은 82개(기존1~81과83), QCC 설계는 4개 모듈/14개 유닛이다. 중복 RID82의 통화 송신은 기존 RID69에 통합했다. 시스템 시험82는 별도 송신 조건으로 유지하여 RID69와 상위FW15에 연결하며 기존 수신 결과를 송신 통과로 확대하지 않는다. 사용설명서 RID83은 자체 소프트웨어 요구사항이며 정식 상위 연결이 확인되지 않은 ACDOC17/18/20을 제거하고 상위 칸을 비웠다. QCC 유닛15그룹/38조건과 통합7그룹/14조건의 실행/판정 대기는 유지한다. DMIC2의 E8300/QCC 양쪽 연결과 통화 수음, 설계/시험 설명 및 최신 작성 원칙을 반영했다. 문서 반영/검사 완료는 제품 시험 통과나 신규 승인을 뜻하지 않는다.',
'', '### 파일과 현재 미비 위치', '', '| 등록 ID / 파일 | 담당과 검색 접두어 | 저장 본문 검색 위치 | 상태 / 확인일 |','|---|---|---|---|']
for r in records:
    groups=defaultdict(list)
    for e in r['entries']: groups[e['prefix']].append(e['location'])
    if not groups: groups['없음']=[]
    for prefix,locations in groups.items():
        suffix={'[QCC 자료 입력]':'Q','[E8300 보완]':'E','[QCC/E8300 자료 입력]':'QE','없음':'N','QCC 반영 내용(굵은 빨강)':'Q-REF','접두어 해제 후 미확정':'Q-OPEN'}[prefix]
        owner={'Q':'QCC','E':'E8300','QE':'QCC/E8300','N':'-','Q-REF':'QCC/연계','Q-OPEN':'QCC 또는 연계 검토'}[suffix]
        unique=compact(locations)
        loc='; '.join('`'+s.replace('|','/')+'`' for s in unique) or '현재 본문 미비 표시 없음'
        state=('내용 반영/접두어 보존, 실제 잔여 자료·제품 판정은 본문 참조' if suffix in ('Q','QE') else '보완/검증 사항은 본문 참조') if unique else '문서 반영'
        if suffix=='Q-REF': state='사용자 지시로 안내 접두어 해제, 실제 반영 내용 굵은 빨강. 제품 시험 통과를 뜻하지 않음'
        if suffix=='Q-OPEN': state='입력 요청 접두어 해제, 실제 미확정 범위는 검정 본문으로 보존'
        lines.append(f"| QCC-NEW-{r['code']}-{suffix} / [{r['code']}]({link(r['path'])}) | {owner} `{prefix}` | {loc} | {state} / {STAMP} |")
lines += ['', 'Word는 변경 후 표시의 본문 제목·요구사항/유닛 ID와 접두어로 찾는다. 목차, 삭제 추적 문구 및 삭제된 표 행은 현재 위치에서 제외했다. Excel은 변경 사항이 적용된 원래 시트의 위 셀에서 해당 시험 ID·기능·조건을 함께 확인한다. 변경 전후의 빨간 [-]/초록 [+] 문구는 별도 `_비교` 시트에 보관하며 현재 미비 위치 수집에서 제외한다. 본문에는 변경 비교 표식과 삭제 이력을 두지 않는다. 시험 예상값/관찰값과 통과 기준/결과의 비교는 본문의 같은 시험 행에 유지하며 보고서의 결과·판정 열은 기준서에 추가하지 않는다. QCC/공동 접두어는 사용자의 해제 전까지 남기며, 접두어가 있다는 이유로 이미 받은 내용을 미수신으로 기록하지 않는다.', '', '### 받은 자료와 남은 확인', '', '| 등록 ID | 필요한 자료와 연결 | 담당 / 상태 |','|---|---|---|',
'| QCC-NEW-DESIGN | 모듈/14유닛 구성, 소프트웨어 상태, 인증·데이터·오디오 설계와 38개 유닛 조건은 제공 자료에 따라 반영. 구현 버전/빌드/실행 로그는 별도 필요. SAD QCC 설계와 요구사항 연결, SDDD QCC 세부설계, SIP QCC-I-01~07, 유닛 QCC-ID-1~15에서 확인 | QCC / 설계 반영·실행 근거 대기 |',
'| QCC-NEW-ENV | 실제 QCC 하드웨어/ADK·운영환경·기성 소프트웨어 버전, 빌드 언어/도구와 프로그램 크기, 변경 위험 재평가 및 검토 근거. SR 하드웨어 플랫폼/프로그램 언어/사이즈/위험분석, SGR 운영 체재/기성용 소프트웨어에서 확인 | 본문의 QCC 또는 공동 접두어 담당 / 자료 대기 |',
'| QCC-NEW-PIO | shutdown_ind 원문/그림의 상충, 실제 PIO 번호·극성·반전·종료대기와 I2S enable 물리 대응. E8300 확정 핀 동작을 QCC 논리 이름으로 바꾸지 않음. SDDD SU-ID-QCC-005/006, 통합019 및 시스템 관련 요구사항에서 확인 | QCC / 상충 보존 |',
'| QCC-NEW-POWER | 충전 4상태를 SPI 0x34의 0/1 연결 상태로 변환하는 표, 통보 순서·전압/배터리 임계·NVM 실패 경로. 유닛 QCC-ID-2/6 및 통합018에서 확인 | QCC / 자료 대기 |',
'| QCC-NEW-BLE | UUID/길이/권한, AUTH Confirm write/notify 상충, GAIA 주소/type/data/CRC 및 오류 응답. 논리1~20바이트와 SPI 물리21바이트를 구분. 유닛9/10/11, 통합020/023, RID44~49에서 확인 | QCC / 계약 대기 |',
'| QCC-NEW-AUTH | SHA-256 키 파생과 AES-CMAC CRA 역할의 원문 상충, 키 ID/설치키·생성키, 입력 직렬화·nonce·결과 벡터·권한별 게이트. 유닛12~15, 통합021/024, RID52/76에서 확인 | QCC / 원문 역할 정리·구현 검증 대기 |',
'| QCC-NEW-TIME | 무송수신180초 리셋 범위와 기존 무제어명령 의도의 정합, 인증30초의 시작/리셋/허용오차, 연속5회 실패 후60초 제한의 카운터/재접속/경계. RID47/71/72, 관련 유닛/통합/시스템 같은 조건 행에서 확인 | QCC / 기준 구체화·미측정 보존 |',
'| QCC-NEW-AUDIO | DMIC2의 E8300/QCC 양쪽 연결과 QCC 통화 수음 경로는 사용자 확인으로 반영. 남은 자료는 QCC 마이크 핀/클록, 24bit/16kHz I2S 클록·프레임·좌우·시작/중단 파형, 단측/양측 좌우값 상충, TWS/프로파일/코덱, 통화 상대·동시 동작 및 실제 관찰/측정. RID15/16/69, 유닛8/10/11, 통합022/023, 시스템69/82에서 확인 | QCC / 경로 반영·세부 계약/제품 검증 대기 |',
'| QCC-NEW-UPDATE | QCC 자체 OTA와 E8300 이미지 전달의 명령/슬롯/버퍼/순서/재시도/실패 복구, QCC 부트 무결성 범위. E8300 CRC 확인을 QCC 무결성 통과로 확대하지 않음. SDDD 무결성/OTA 안내와 SIP 부트로더 통합, RID54 관련 위치에서 확인 | QCC / 자료 대기 |',
'| QCC-NEW-E-SCOPE | QCC 상태 전달/광고 지원은 E8300의 등록 내부기 식별, 자극 중단, 경보 전체 경로의 통과가 아님. 실제 전체 기능 판정은 해당 E8300 조건과 시스템 시험에서 확인 | 담당 범위 정리 / 기존 관찰 범위 보존 |',
'| QCC-NEW-MANUAL | RID83은 자체 소프트웨어 요구사항으로 유지하고 상위 추적은 빈칸이다. 동봉문서 ACDOC17/18/20과의 임의 연결을 SRS/SR/SAD/시스템 기준·보고 및 사이버보안 연계에서 제거했다. IFU-TD24A-01 Rev.0(2026.08.12)의 260824 형성평가용 초안에서 경고/안전사항, 안전한 작동법, 예방/사용/문제 해결 기재를 확인해 시스템83 결과에 기록했다. 은수님도 세 항목의 기재를 직접 확인했으며 실제 배포본은 별도라고 확인했다. 현재 결과 범위는 초안 검사로 한정하고, 별도 배포본의 해당 내용 확인과 최종 판정은 대기한다. | E8300 `[E8300 보완]` / 초안 기재 확인 완료·배포본 확인 및 최종 판정 대기 |',
'| QCC-NEW-RESULT | QCC 실제 버전/시험 환경/시험자/입력과 전체 응답/파형/사진/측정 및 로그 필요. 예상 데이터·정적 코드 분석·시험자 확인·실제 수집 기록을 구분. 신규 보고서 결과 셀의 미실행/판정 대기를 실제 근거가 확보된 뒤 수정 | QCC 또는 실제 표시된 공동 담당 / 시험 미실행 |',
'', '확인일: '+STAMP+'. 위 파일 링크와 검색 위치는 저장된 현재 문서 본문을 기준으로 재수집했다. 과거 서명/일자를 새 승인으로 복사하지 않았고 온라인 설계 입출력 검증 매트릭스는 읽기 전용으로 확인하여 이번 작업에서 수정하지 않았다.', '<!-- E8300-QCC-CURRENT:END -->', '']
target=ROOT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
if any(Path(r['path']).stem.endswith('_완료') and r['code'] in ('SR','SGR') for r in records):
    at=lines.index('### 받은 자료와 남은 확인')
    lines.insert(at,'2026-10-13 사용자 지시에 따라 설계 입력의 SR/SGR Word 완료본은 QCC/공동 입력 요청 문구를 해제하고 실제 반영 내용을 굵은 빨강으로 표시했다. 미확정 버전/사용량/설정/검토 결과는 같은 본문의 일반 검정 설명과 이 목록에 유지한다. `_완료`는 이번 문서 정리 완료이며 제품 검증이나 자료 완비를 뜻하지 않는다. 다른 문서의 표시 규칙은 유지한다.\n')
    lines=[line.replace('본문의 QCC 또는 공동 접두어 담당 / 자료 대기','SR/SGR 완료본의 접두어 없는 미확정 설명 참조 / 자료 대기') for line in lines]
source=target.read_text(encoding='utf-8-sig')
start='<!-- E8300-QCC-CURRENT:BEGIN -->'; end='<!-- E8300-QCC-CURRENT:END -->'
section='\n'.join(lines)
if start in source:
    source=source[:source.index(start)]+section+source[source.index(end)+len(end):].lstrip('\n')
else:
    pos=source.index('## 미비 사항 표시 규칙')
    source=source[:pos]+section+'\n'+source[pos:]
target.write_text(source,encoding='utf-8')
print(json.dumps({'files':len(records),'locations':sum(len(r['entries']) for r in records),'registry':str(target)},ensure_ascii=False))
