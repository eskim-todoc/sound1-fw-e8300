from pathlib import Path
from lxml import etree as E
import json,re,zipfile,hashlib
import openpyxl
from integrate import sheetmap,vals,texts,N,sha,NS
HERE=Path(__file__).resolve().parent;TASK=HERE.parents[2]
P=json.loads((HERE/'author-payload.json').read_text(encoding='utf-8'))
A=json.loads((HERE/'audit-current.json').read_text(encoding='utf-8'))
F=json.loads((HERE/'followup-plan.json').read_text(encoding='utf-8'))
DATE=json.loads((HERE/'date-plan.json').read_text(encoding='utf-8')) if (HERE/'date-plan.json').exists() else {}
CFG={b['key']:b for b in json.loads((TASK/'builders/qcc_tests/payload.json').read_text(encoding='utf-8'))['books']}
def current(v):
 # Every previous paragraph has its own leading [-], so a retired comparison
 # nested inside that paragraph cannot re-enter the active value.
 if '[-] ' not in v and '[+] ' not in v:return v
 lines=[]
 for line in v.splitlines():
  if line.startswith('[-] '):continue
  if line.startswith('[+] '):line=line[4:]
  if line:lines.append(line)
 return '\n'+'\n'.join(lines)+'\n' if lines else ''
out={'document_checks':{},'pairs':[],'files':[],'native_display_status':'이전 실제 픽셀의 짧은 설명 glyph 경계 실패를 group-boundary 공통 보완본으로 수정함. 현재 G 최종 native 재출력/픽셀 검사 미실행. 이전 실패 증거 native-boundary-round1-review.json 유지.','previous_native_display_review':{'evidence':'native-boundary-round1-review.json','status':'실패','correction':'group-boundary-published.json','current_display':'미실행'},'product_execution':'신규 조건 미실행/판정 대기. 기존 시험자 확인 기록만 기존 범위에서 보존.'}
allcurrent={};loaded={};rich={};notices=[]
for k,b in P['books'].items():
 path=Path(b['path']);w=openpyxl.load_workbook(path,data_only=False,rich_text=True);loaded[k]=w
 with zipfile.ZipFile(path) as z:
  assert z.testzip() is None;data={i.filename:z.read(i.filename) for i in z.infolist()}
  for name in data:
   if name.endswith('.xml'):E.fromstring(data[name])
  mapping=sheetmap(data);style=E.fromstring(data['xl/styles.xml']);fonts=style.find('x:fonts',N);xfs=style.find('x:cellXfs',N)
  allcurrent[k]={};rich[k]={}
  for sn,member in mapping.items():
   display=vals(data,member);allcurrent[k][sn]={a:current(v) for a,v in display.items()};rich[k][sn]={}
   root=E.fromstring(data[member])
   for c in root.findall('.//x:sheetData/x:row/x:c',N):
    isn=c.find('x:is',N)
    if isn is None:continue
    assert all(texts(run) for run in isn.findall('x:r',N)),(k,sn,c.get('r'),'empty rich run would be rejected by native Excel')
    if isn.findall('x:r',N):rich[k][sn][c.get('r')]=E.tostring(isn).decode()
    base=float(fonts[int(xfs[int(c.get('s','0'))].get('fontId','0'))].find('x:sz',N).get('val','11'))
    prior=False
    for run in isn.findall('x:r',N):
     t=run.findtext('x:t',namespaces=N) or '';rp=run.find('x:rPr',N)
     if t=='[-] ':prior=True
     elif t=='[+] ':prior=False
     if t.startswith(('[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]')) and not prior:
      assert rp is not None
      size=float(rp.find('x:sz',N).get('val'));color=rp.find('x:color',N).get('rgb');expected='FF7030A0' if t.startswith('[QCC/E8300') else 'FF0070C0' if t.startswith('[E8300') else 'FFC00000'
      assert size==base+2 and color==expected and rp.find('x:u',N) is not None,(k,sn,c.get('r'),base,size,color)
      notices.append({'key':k,'sheet':sn,'cell':c.get('r'),'prefix':t.split(']')[0]+']','base':base,'size':size,'color':color,'underline':True})
     if t.endswith('\n'):prior=False
   if sn in b['comparison']:
    for addr,comp in b['comparison'][sn].items():
     assert display[addr]==comp['display'],(k,sn,addr,'display')
     assert [x for x in current(display[addr]).splitlines() if x]==[x for x in comp['current'].splitlines() if x],(k,sn,addr,'current')
  with zipfile.ZipFile(CFG[k]['original']) as oldzip:
   features=[n for n in oldzip.namelist() if n.startswith('xl/media/') or n.startswith('xl/drawings/')]
   assert all(data[n]==oldzip.read(n) for n in features)
 out['files'].append({'key':k,'path':str(path),'sha256':sha(path),'all_rows_reopened':{s.title:s.max_row for s in w},'changed_cells':{sn:list(cells) for sn,cells in b['comparison'].items()},'native_features_preserved':len(features),'height_max':max((d.height or 15) for s in w for d in s.row_dimensions.values())})
 assert out['files'][-1]['height_max']<=409.5

for ck,rk in [('SUVC','SUVR'),('SITC','SITR'),('SSTC','SSTR')]:
 c=loaded[ck][CFG[ck]['sheet']];r=loaded[rk][CFG[rk]['sheet']];start=CFG[ck]['commonStart'];end=max(CFG[ck]['newRows']);last=openpyxl.utils.column_index_from_string(CFG[ck]['commonLast'])
 diffs=[];runs=[];heights=[]
 for row in range(start,end+1):
  for col in range(2,last+1):
   a=c.cell(row,col).coordinate
   if str(c[a].value or '')!=str(r[a].value or ''):diffs.append(a)
   if rich[ck][c.title].get(a)!=rich[rk][r.title].get(a):runs.append(a)
  if c.row_dimensions[row].height!=r.row_dimensions[row].height:heights.append(row)
 cm={str(m) for m in c.merged_cells.ranges if m.min_row>=start and m.max_row<=end and m.min_col>=2 and m.max_col<=last};rm={str(m) for m in r.merged_cells.ranges if m.min_row>=start and m.max_row<=end and m.min_col>=2 and m.max_col<=last}
 newheights=[row for row in heights if row in CFG[ck]['newRows']]
 assert not diffs and not runs and cm==rm and not newheights,(ck,diffs,runs,cm^rm,newheights)
 break_rows=lambda sheet:[(b.id,b.min,b.max,b.man,b.pt) for b in sheet.row_breaks.brk]
 assert break_rows(c)==break_rows(r),(ck,'manual print break mismatch')
 if ck=='SUVC':
  assert str(c['D24'].value)==str(r['D24'].value) and rich[ck][c.title]['D24']==rich[rk][r.title]['D24']
  assert '새 QCC' in allcurrent[ck][c.title]['D24'] and '미실행' in allcurrent[ck][c.title]['D24']
 idcol='D' if ck=='SUVC' else 'C' if ck=='SITC' else 'B';ids=[allcurrent[ck][c.title].get(idcol+str(row),'').strip() for row in range(start,end+1)];ids=[x for x in ids if x.startswith('SW-FW-')]
 assert len(ids)==len(set(ids))==({'SUVC':130,'SITC':24,'SSTC':83}[ck]),(ck,'IDs',len(ids))
 for row in CFG[rk]['newRows']:
  result=allcurrent[rk][r.title].get(CFG[rk]['resultCol']+str(row),'');verdict=allcurrent[rk][r.title].get(CFG[rk]['verdictCol']+str(row),'')
  assert '미실행' in result and '검증 [' in result and '판정 대기' in verdict and '통과' not in verdict,(rk,row)
 # All existing criteria and historic observed/verdict fields stay identical.
 resultcols=[CFG[rk]['resultCol'],CFG[rk]['verdictCol']]
 historical=[a for a in A['books'][rk]['all_cells'][r.title] if re.fullmatch('('+'|'.join(resultcols)+r')\d+',a) and int(re.search(r'\d+',a)[0])<min(CFG[rk]['newRows'])]
 clarified=[a for a in historical if rk=='SSTR' and ((a in F[rk]['changes']) or (a in DATE.get(rk,{}).get('changes',{}))) and a.startswith('L')]
 explained=[a for a in historical if a in DATE.get(rk,{}).get('changes',{}) and a.startswith('K')]
 assert all(str(r[a].value or '')==A['books'][rk]['all_cells'][r.title][a] for a in historical if a not in clarified+explained)
 assert all(A['books'][rk]['all_cells'][r.title][a].strip() in str(r[a].value or '') for a in explained),(rk,'historic observation text lost')
 assert all(allcurrent[rk][r.title][a].strip()=='판정 대기' for a in clarified)
 if clarified:out['historical_verdict_clarifications']=[{'cell':a,'previous':(F[rk]['changes'].get(a) or DATE[rk]['changes'][a])['before'],'current':'판정 대기','previous_preserved':'red [-] in same body cell; observed K preserved, date input/readback scope separately explained'} for a in clarified]
 out['pairs'].append({'pair':[ck,rk],'common_range':f'B{start}:{CFG[ck]["commonLast"]}{end}','common_values_rich_runs_merges_and_new_heights':'통과','manual_page_breaks_equal':True,'manual_page_breaks_after_rows':[b.id for b in c.row_breaks.brk],'additional_common_intro_cells':['D24'] if ck=='SUVC' else [],'old_height_differences':heights,'old_height_scope':'기존 보고서의 결과 표시 높이는 기준서와 다를 수 있으며 공통 내용/행/병합은 동일. 신규 조건 높이는 정확히 동일.','id_count':len(ids),'ids':ids,'new_condition_rows':CFG[ck]['newRows'],'historic_result_cells_unchanged':len(historical)-len(clarified)-len(explained),'historic_verdict_scope_clarified':len(clarified),'historic_observation_scope_explained_without_changing_values':explained})
 # Moving preparation actions did not remove or change the existing inputs/actions.
 for a,comp in P['books'][ck]['comparison'][c.title].items():
  pc='H' if ck=='SUVC' else 'J' if ck=='SITC' else 'G';jc='J' if ck=='SUVC' else 'L' if ck=='SITC' else 'I'
  if a.startswith(pc) and int(a[1:])<min(CFG[ck]['newRows']):
   j=allcurrent[ck][c.title].get(jc+a[1:],'');jlines=[x for x in j.splitlines() if x]
   previous=[x for x in comp['before'].strip().splitlines() if x]
   assert any(jlines[i:i+len(previous)]==previous for i in range(len(jlines))),(ck,a,'input/action loss')

# Read and hash every protected source listed by the parent; never change them.
baseline=json.loads((TASK/'qa/review-20261013/review-baseline.json').read_text(encoding='utf-8'))
protected=[x for x in baseline if x['role']=='protected']
if not protected:protected=[x for x in baseline if 'protected' in x['role'].lower() or 'source' in x['role'].lower()]
out['protected']=[{'path':x['path'],'unchanged':sha(x['path']).lower()==x['sha256'].lower()} for x in protected]
assert all(x['unchanged'] for x in out['protected'])
out['active_notices']=notices
(HERE/'current-values.json').write_text(json.dumps(allcurrent,ensure_ascii=False,indent=2),encoding='utf-8')
out['current_extraction']='통과: 줄별 [-] 과거 문단 제거, [+] 현재 문단 및 전후 불변 문단/전체 RTT/전바이트 보존. author-payload의 현재값과 전 셀 독립 대조.'
numbers=json.loads((HERE/'condition-numbers.json').read_text(encoding='utf-8'))
assert len(numbers)==237 and all(not x['missing_result_numbers'] and not x['extra_result_numbers'] for x in numbers)
out['condition_number_correspondence']={'groups':237,'missing_or_extra':0,'evidence':'condition-numbers.json','scope':'same group Korean condition/verification numbers plus preserved RTT Cn or standalone C82 range/C99 no-output cases; component reference C108 etc are not test conditions'}
rtt=json.loads((HERE/'rtt-correspondence.json').read_text(encoding='utf-8'))
assert len(rtt)==72
diff=[x for x in rtt if x['missing_expected_lines'] or x['extra_observed_lines']]
assert len(diff)==1 and diff[0]['id']=='SW-FW-UnitTest-HEAR-ID-1' and not diff[0]['missing_expected_lines'] and diff[0]['extra_observed_lines']==['[UT-HEAR-001] C99 로그 출력 없음.']
out['full_rtt_correspondence']={'groups':72,'literal_whole_line_equal_groups':71,'allowed_no_output_observation':diff,'evidence':'rtt-correspondence.json','scope':'Existing preserved RTT observations only. QCC absent RTT is pending and not synthesized.'}
checks={
1:('통과','요구82를69/FW15에 통합하고 실제 통화 송신 시험82는 별도로 유지. Q 원시 요구ID를 새 제품 요구ID로 사용하지 않음.'),
2:('통과','SRS/매트릭스 편집은 requirements 소유. 최종 requirements-contract의69/FW15,83/ACDOC17/18/20 및47/52/71/72/76 조건과 시험을 대조.'),
3:('통과','130유닛/24통합/83시스템 ID 및 기존 E8300 식별자를 보존. Q14유닛→15시험 실제 목적·입력·기준을 design과 두 차례 교차 대조.'),
4:('통과','원본 E/Q 모든 시험 행과 Q Word 본문 표를 직접 읽음. 미갱신 CC 명칭은 QCC 실제 기능에 대응하되 동일 기능의 E8300 ID 유지.'),
5:('통과','실제 구조 진입점/근거로 DMIC2→E8300/QCC 및 QCC→E8300 I2S 별개 경로 확인. 사용자 확정 하드웨어 연결을SYS82에 반영.'),
6:('통과','요구69 통합·생산 Standalone/TWS·AUTH와CRA 경계·키 역할·지원 추적을 requirements/design과 반복 교차 검토. 신규 관찰 생성 없음.'),
7:('미실행','E8300 공통 필드/설명 수준과 본문은 저장본에서 확인. 이전 짧은 문단 glyph 경계 실패를 group-boundary 공통 저장본으로 보완했고 현재 native 최종 픽셀 재검사 필요.'),
8:('해당없음','상세설계 작성은 design 소유. 입력/기준/유닛 목적을 전달하여 구현 조건 검토에 사용.'),
9:('통과','I/A/D/T 정의 적용: 31유닛·19시스템 및SYS74 방법 보완. SYS82 기능 데모D, SYS83 설명서검사I 유지.'),
10:('통과','RTT·바이트·수치·시간 원소 비교는T 또는A/T, 정성 기능은D, 문서/필드 검사는I. 도구명 또는1000Hz 입력만으로T 판정하지 않음.'),
11:('통과','기존 준비조건의 수행 동작을 입력/한계/예상값 손실 없이 절차로 이동. 이전 동작 문자열이 현재 절차에 전부 남음을 독립 검사.'),
12:('통과','B30:K1764, B25:M144, B34:J285 공통 값/rich run/행/병합 및 신규 높이 exact 대조.'),
13:('통과','전체 기존 관찰 값 보존, 새38/14/3조건의 입력/구체 예상/미실행을 같은 행 인접 열에 배치. 5/2 기준과5/29 실제 입력의 날짜 대응·readback 미확인 범위 명시.'),
14:('통과','변경 본문 current 독립 대조로 전체RTT/전바이트 불변. 기존RTT72그룹 중71전체행 동일, HEAR1의C99무출력 관찰은 기준 무출력 조건에 대응. Q 실제RTT/벡터 부재는 대기; 코드상 예상/시험자 확인/실측 구분.'),
15:('통과','전체237그룹의 조건/검증 번호 및 원래RTT Cn 식별자 대응 누락/잉여0. C82범위/무출력C99는 해당 관찰 기준으로 구별하며 새 결과를 만들지 않음. 같은행/공통구조 유지.'),
16:('통과','신규 결과 미실행/판정대기. 결과 칸에 같은 입력의 확인 범위·기존 관찰·부재 근거, 현재 판정 칸은 판정만 기재. 과거 삭제 추적은 별도 색으로 유지.'),
17:('통과','사용자가 확인한 기존 시험자 관찰을 해당 입력·조건에서 보존. 원시 실측 수집을 새 승인 조건으로 만들거나 Codex 실행으로 표현하지 않음.'),
18:('통과','AUTH 패턴→CRA,수신→통화송신,상태입력→RF신원/자극출력,표시→독립저장비교 통과 확대 금지. 기존 결과15현재 판정은 부재 근거에 맞게 대기로 제한.'),
19:('통과','역사적으로 지정된 시험자 수치 보존. 새 오차·반복수·측정값 생성 없음.'),
20:('통과','모든 기존 media/drawing 부품 byte 동일. 예상 도식과 실제사진을 새로 생성하지 않음. 기존 사진 필요·자료대기 유지.'),
21:('통과','유닛 CM3 K/L1212 및SYS74 J/K218 날짜 매개변수 대응 명시. 기준5/2·결과5/29와 월말/윤년 등 별도 경계 입력을 구분하여 경계값 일괄치환 없음.'),
22:('통과','활성 미비 run의 담당 접두어/색/base+2pt/밑줄을 전부 검사. 수신/반영은 접두어 자동 제거가 아니며 실제 남은 벡터·코드·결과를 표시.'),
23:('미실행','중앙 위치목록은 root 소유. 저장본 current-values/native-plan/해시로 동시 갱신에 필요한 위치 전달; root 최종 대조 대기.'),
24:('미실행','행높이409.5pt·신규 양문서 동일 높이·병합·rich 서식 확인. CFX31 C1 전체 RTT 숨김은 이전 실제 화면에서 보완 확인. 이전 짧은 문단 glyph 경계 실패는 group-boundary 저장본의 실제 native 최종 픽셀 재검사 필요.'),
25:('통과','본문 실질 변경 문단에 빨간[-]·초록[+] 사용. 불변RTT/바이트를 한 번 유지하고 현재 추출을 변경계획과 대조. 의미없는 전체 노란색 강제 없음.'),
26:('해당없음','Word 본파일 변경추적은 이6XLSX 담당 범위 밖.'),
27:('통과','G E8300-QCC 사본6개만 수정. 보호 원본23개 해시 불변. 작성자전용·원본 과거 서명/승인 이력 불변.'),
28:('미실행','전행 재개봉/ZIP/XML/공통 구조/원본·관찰 불변 검사 완료. 이전 실제 G 전체 native 출력/경계 픽셀 검사 실패 증거를 보존하고 group-boundary 현재 저장본 최종 출력 재검사 필요.'),
29:('해당없음','Word 공통 형식은 이6XLSX 담당 범위 밖; E8300 Excel 본문 형식은7/24에서 확인.')}
out['document_checks']={str(n):{'status':s,'scope':scope} for n,(s,scope) in checks.items()}
out['final_native_display_pass']=False
display_proof=HERE/'final-native-display-qa.json'
if display_proof.exists():
 proof=json.loads(display_proof.read_text(encoding='utf-8'))
 current_hashes={x['key']:x['sha256'].lower() for x in out['files']}
 if proof.get('final_native_display_pass') is True and proof.get('source_hashes')==current_hashes:
  out['final_native_display_pass']=True
  out['native_display_status']=proof['scope']
  out['final_native_display_evidence']=str(display_proof)
  out['previous_native_display_review']['current_display']='통과: 최신 저장본 실제 native 재검사'
  for n in ['7','24','28']:
   out['document_checks'][n]={'status':'통과','scope':proof['scope']+' / native 픽셀 증거 final-native-display-qa.json. 실제 신규 제품시험 결과 생성 없음.'}
out['status']='통과: 저장 본문/공통값/원본/결과 범위. 이전 표시 실패 증거 보존, 현재 보완본 native 최종 픽셀 재검사 미실행.'
if out['final_native_display_pass']:out['status']='통과: 저장 본문/공통값/원본/결과 범위 및 현재 실제 Excel native 표시 재검사. 이전 실패/보완과 정밀·개요 검사 범위를 별도 증거로 유지.'
(HERE/'qa.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
for w in loaded.values():w.close()
print(json.dumps({'status':out['status'],'pairs':[{'pair':x['pair'],'range':x['common_range'],'IDs':x['id_count'],'historical_cells':x['historic_result_cells_unchanged']} for x in out['pairs']],'protected_count':len(protected),'notice_runs':len(notices)},ensure_ascii=True))



