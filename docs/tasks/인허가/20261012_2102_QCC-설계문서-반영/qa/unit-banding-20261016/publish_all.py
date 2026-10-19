from pathlib import Path
import sys,json,re,shutil
from urllib.parse import quote
H=Path(__file__).resolve().parent;QA=H.parent;PROJECT=H.parents[5]
sys.path.insert(0,str(QA/'unit-rebuild-20261016'))
from rebuild import Book,digest
G=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF')
SPECS={
 'unit':('unit-banding-20261016',G/'04_설계 검증/SW/외부기/[E8300-QCC] SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서_재작성.xlsx'),
 'criteria':('system-criteria-banding-20261016',G/'03_설계 출력/SW/외부기/[E8300-QCC] [완료 - QCC 입력 대기] SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서.xlsx'),
 'report':('system-report-banding-20261016',G/'04_설계 검증/SW/외부기/[E8300-QCC] [완료 - QCC 입력 대기] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx')}
PREFIXES=['[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]']
def save(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')
def publish(key):
 folder,target=SPECS[key];d=QA/folder
 assert (d/'native-final.json').exists(),'native visual QA missing'
 nf=json.loads((d/'native-final.json').read_text(encoding='utf-8-sig'))
 assert nf.get('status') in ['PASS','통과'],'native QA not passed'
 assert digest(target)==digest(d/'before.xlsx'),'Current target changed; rebase rather than overwrite'
 shutil.copyfile(d/'candidate.xlsx',target)
 assert digest(target)==digest(d/'candidate.xlsx')
 b=Book(target)
 notices={p:{n:[a for a in b.cells[n] if p in b.value(n,a)] for n in b.sheets if '비교' not in n} for p in PREFIXES}
 notices={p:{n:v for n,v in groups.items() if v} for p,groups in notices.items()}
 save(d/'saved-notices.json',notices)
 save(d/'publication.json',{'target':str(target),'status':'PASS','beforeSHA':digest(d/'before.xlsx'),'afterSHA':digest(target),'backup':str(d/'before.xlsx')})
 print(json.dumps({'published':key,'sha':digest(target)},ensure_ascii=False))
def locations(groups):
 out=[]
 for n,addrs in groups.items():
  cols={}
  for a in addrs:
   m=re.fullmatch(r'([A-Z]+)(\d+)',a);cols.setdefault(m[1],[]).append(int(m[2]))
  refs=[]
  for col,rs in sorted(cols.items()):
   rs=sorted(set(rs));lo=hi=rs[0]
   for v in rs[1:]+[None]:
    if v==hi+1:hi=v;continue
    refs.append(f'{col}{lo}'+(f':{col}{hi}' if hi!=lo else ''))
    if v is not None:lo=hi=v
  out.append('`'+n+'!'+', '.join(refs)+'`')
 return '; '.join(out) if out else '현재 본문에서 해당 접두어 미검출'
def record():
 pubs={key:json.loads((QA/folder/'publication.json').read_text(encoding='utf-8')) for key,(folder,t) in SPECS.items()}
 for key,(folder,target) in SPECS.items():assert digest(target)==pubs[key]['afterSHA']
 notices={key:json.loads((QA/folder/'saved-notices.json').read_text(encoding='utf-8')) for key,(folder,t) in SPECS.items()}
 registry=PROJECT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
 text=registry.read_text(encoding='utf-8');lines=text.splitlines()
 types={'Q':PREFIXES[0],'QE':PREFIXES[1],'E':PREFIXES[2]}
 for i,line in enumerate(lines):
  m=re.match(r'^\| QCC-NEW-(SSTC|SSTR)-(Q|QE|E) /',line)
  if m:
   code,typ=m.groups();key='criteria' if code=='SSTC' else 'report';p=types[typ];url=quote(str(SPECS[key][1]).replace('\\','/'),safe='/:')
   lines[i]=f'| QCC-NEW-{code}-{typ} / [{code}]({url}) | `{p}` | {locations(notices[key][p])} | 사용자 지정 현재 파일명/본문 위치 확인. 배경색만 수정, 핑크 TEST ID 및 기존 내용/판정/미비 표시 보존 / 2026-10-16 |'
  if line.startswith('| QCC-SUVR-REBUILD-0'):
   num=int(line.split('|')[1].strip()[-1]);p=PREFIXES[num-1]
   lines[i]=f'| QCC-SUVR-REBUILD-0{num} | `{p}` | {locations(notices["unit"][p])} | 최신 사용자 편집본의 현재 좌표로 갱신. 130시험 연속 교차색 적용, 내용/판정/접두어 보존 / 2026-10-16 |'
  if line.startswith('| SSTR-PHOTO-0'):
   num=int(line.split('|')[1].strip()[-1]);p={1:PREFIXES[1],2:PREFIXES[0],3:PREFIXES[2]}[num]
   lines[i]=f'| SSTR-PHOTO-0{num} | `{p}` | {locations(notices["report"][p])} | 현재 완료/QCC 입력 대기 파일의 본문 위치. 핑크 ID·사진·기존 판정 및 미비 표시 보존 / 2026-10-16 |'
  if line.startswith('[[E8300-QCC] SSTR-FW-TD24A'):
   target=str(SPECS['report'][1]).replace('\\','/');lines[i]=f'[시스템 시험 보고서 현재본](<{target}>)'
  if '새 사본의 QCC 시험은' in line:lines[i]=line.replace('D1727:D1764','D1726:D1763')
 text='\n'.join(lines)+'\n'
 note='2026-10-16 현재 파일명과 검색 위치를 다시 확인했다. 유닛 보고서는 첫 시험부터 130개를 연속 흰색/연회색으로, 시스템 기준서·보고서는 각 83개를 실제 표시 순서대로 교차색 적용했다. 시스템 문서의 핑크 확인 완료 TEST ID 70개씩은 사용자 지시로 유지했다. 이번 작업은 채우기만 바꾸었고 사용자 편집본의 내용·사진·병합·판정은 보존했다. [저장 근거](../../../tasks/인허가/20261012_2102_QCC-설계문서-반영/qa/unit-banding-20261016/publication.json).\n\n'
 text=text.replace('<!-- SSTR-PHOTO-20261016:BEGIN -->',note+'<!-- SSTR-PHOTO-20261016:BEGIN -->',1)
 registry.write_text(text,encoding='utf-8')
 task=H.parent.parent/'작업.md'
 with task.open('a',encoding='utf-8') as f:
  f.write('\n## 2026-10-16 유닛 보고서와 시스템 기준서·보고서 교차 배경색\n\n')
  f.write('사용자 요청으로 유닛 재작성 보고서와 `[완료 - QCC 입력 대기]` 시스템 기준서/보고서의 시험별 배경색을 정리했다. 첫 시험은 흰색 `#FFFFFF`, 다음은 연회색 `#F5F5F5`로 실제 표시 순서대로 반복하며 코어/ID 번호 재시작과 무관하게 연속 적용했다. 한 시험의 모든 행은 같은 교차색이며, 사용자 추가 답변에 따라 시스템 문서 각각70개의 기존 핑크 TEST ID 셀만 예외로 원형 보존했다.\n\n')
  for key,(folder,target) in SPECS.items():f.write(f'- {target.name}: [채우기/내용 보존 검사](qa/{folder}/validation.json), [독립 Excel 표시 검사](qa/{folder}/native-final.json), [저장 및 복구본](qa/{folder}/publication.json). SHA256 `{pubs[key]["afterSHA"]}`.\n')
  f.write('\nSV7·22·24·25·27·28은 이번 채우기/보존 범위 통과: 내용·수식·글꼴·정렬·테두리·병합·행높이·열폭·인쇄설정과 사진/다른 시트 불변, 본문/비교 시트 분리 유지, 실제 Excel 읽기 전용 표시 확인 후 저장본 해시 일치. SV23은 사용자 변경된 현재 파일명과 QCC 위치를 실제 저장본 검색으로 갱신했다. SV1~6·8~21·26·29의 내용·추적·시험 실행·Word 수정은 이번 범위에 해당없음; 앞서 확인한 전압/조건 전사 차이를 이번 배경색 작업에서 수정하지 않았다. 제품 시험/승인 상태를 새로 변경하지 않았다.\n')
 print(json.dumps({'records':'updated','outputs':3},ensure_ascii=False))
if __name__=='__main__':record() if sys.argv[1]=='record' else publish(sys.argv[1])
