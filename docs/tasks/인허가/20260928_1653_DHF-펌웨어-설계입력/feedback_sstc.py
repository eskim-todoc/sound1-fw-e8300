from pathlib import Path
import json,re,shutil,hashlib
import openpyxl
T=Path(__file__).parent;Q=T/'feedback-sstc-20260929';Q.mkdir(exist_ok=True)
O=T.parents[2]/'참고/인허가/사운드1/DHF/02_설계 입력/SW/펌웨어'
jobs=json.loads((T/'workbook-plan.json').read_text(encoding='utf8'));job=jobs[1]
target=O/job['name'];shutil.copy2(target,Q/'before.xlsx')
srsfile=O/jobs[0]['name'];srs=openpyxl.load_workbook(srsfile,data_only=True).worksheets[1]
s=openpyxl.load_workbook(target,data_only=True).worksheets[1]
old=openpyxl.load_workbook(job['source'],data_only=True).worksheets[1]
def readref(r):
 for m in srs.merged_cells:
  if m.min_row<=r<=m.max_row and m.min_col<=8<=m.max_col:return srs.cell(m.min_row,m.min_col).value
 return srs.cell(r,8).value
requirements={int(re.search(r'(\d+)$',str(srs.cell(r,7).value))[1]):{'text':srs.cell(r,6).value,'ref':readref(r),'row':r} for r in range(6,srs.max_row+1) if str(srs.cell(r,7).value).startswith('SW-FW-RID-')}
assert set(requirements)==set(range(1,82))
original={n:[old.cell(n+24,c).value for c in range(2,11)] for n in range(1,69)}
updates={
1:{'G':'32채널 맵을 적용한다.','I':'각 채널의 자극률을 측정하고 32채널의 합산 자극률을 확인한다.','J':'전채널 합산 자극률은 20 kHz 이상이어야 한다. 상위 SYS-RID-FW-2의 목표 24 kHz에 대한 허용오차는 기존 기준의 ±5%로 확인한다.'},
2:{'G':'24채널 맵을 적용한다.','I':'24채널 맵에서 각 채널의 최대 자극률을 측정한다.','J':'채널별 최대 자극률은 1000 Hz 이상이어야 한다.'},
6:{'H':'상위 매트릭스 합격 기준의 하한 12 us와 요구사항의 13 us 표기가 다르다. 본 시험은 SRS의 13~255 us를 기준으로 한다.','I':'펄스폭을 13, 14, 255 us로 설정하여 1 us 단계 설정과 출력 펄스폭을 확인한다.','J':'13~255 us 범위에서 1 us 단위로 설정할 수 있어야 하며, 측정 펄스폭은 각 설정값의 ±5% 이내이어야 한다.'},
11:{'G':'CIS 맵을 준비한다. NofM 옵션을 적용하는 경우 NofM 맵과 합성음을 추가로 준비한다.','I':'CIS 맵의 채널별 자극 순서를 확인한다. NofM 옵션 적용 시 선택 채널 수보다 많은 대역의 합성음을 입력하고 각 전극 출력을 확인한다.','J':'필수 CIS는 맵의 자극 순서와 출력 순서가 일치해야 한다. 옵션 NofM 적용 시 설정된 N개 채널이 선택되고 미선택 채널은 무자극이며, 맵의 자극 순서를 따라야 한다.'},
15:{'C':'오디오 소스 입력','G':'2개 이상의 오디오 소스와 E8300/QCC의 입력 확인 수단을 준비한다. 향후 I2S 입력도 같은 항목으로 확인한다.','I':'각 오디오 소스를 순서대로 선택하여 입력한다. E8300과 QCC에서 각자 처리하는 입력 경로의 데이터 수신을 확인하고 최종 자극 출력을 확인한다.','J':'2개 이상의 오디오 소스를 입력 받을 수 있어야 한다. E8300/QCC 각각의 해당 입력 경로와 최종 자극 출력이 정상이어야 한다.'},
16:{'C':'8kHz 오디오 입력 대역','H':'SRS의 8kHz 표현에 대해 정확히 8kHz 순음 입력과 8kHz 대역 지원 중 판정 범위를 확정해야 한다. 대역 내 순음 결과만으로 정확히 8kHz 입력의 통과를 판정하지 않는다.'},
42:{'C':'터치 센서 I2C 통신'},
45:{'I':'블루투스를 사용하여 매핑 프로그램과 데이터 통신을 수행하고, E8300으로 전달된 제어 명령과 응답을 확인한다.','J':'매핑 프로그램과 블루투스를 사용한 데이터 통신이 가능하고, 요청한 제어와 응답이 일치해야 한다.'},
46:{'I':'등록된 내부기와 등록되지 않은 내부기를 각각 준비한다. 외부기를 각각의 내부기에 결합하여 근접 무선 통신 연결 여부를 확인한다.','J':'등록된 내부기와는 연결되어야 하며, 등록되지 않은 내부기와는 연결되지 않아야 한다.'},
53:{'C':'소프트웨어 유지보수','D':'Inspection','I':'고객불만처리 절차 또는 A/S 처리 방침을 확인한다.','J':'소프트웨어 업데이트가 필요한 경우 기존 제품을 회수하고 업데이트된 제품을 생산하여 교체한다는 내용이 있어야 한다.'},
81:{'C':'인가된 인원의 펌웨어 생성 및 배포','D':'Inspection','I':'품질관리시스템의 펌웨어 생성/배포 절차, 인가된 담당자 및 배포 이력을 확인한다.','J':'품질관리시스템 절차에 따라 인가된 인원이 펌웨어를 생성하여 배포해야 한다.'}}
body=[];current={}
for r in range(25,105):
 v=[s.cell(r,c).value for c in range(2,11)];n=int(re.search(r'(\d+)$',v[3])[1]);current[n]=r
 for col,value in updates.get(n,{}).items():v[ord(col)-66]=value
 v[4]=requirements[n]['ref']
 prefix=re.match(r'^\[[^\]]+\]',requirements[n]['text'])
 if prefix:v[1]=prefix[0]+' '+re.sub(r'^\[[^\]]+\]\s*','',v[1])
 body.append({'row':25+len(body),'sourceRow':r,'id':n,'values':v})
 if n==53:
  nv=['SW-FW-SYS-Test-ID-81',updates[81]['C'],updates[81]['D'],'SW-FW-RID-81',requirements[81]['ref'],None,None,updates[81]['I'],updates[81]['J']]
  body.append({'row':25+len(body),'sourceRow':r,'id':81,'values':nv})
assert [b['id'] for b in body]==list(requirements)
entries=[]
for b in body:
 before=original.get(b['id']);after=b['values']
 if before is None:entries.append({'mark':'[+]','bodyrow':b['row'],'values':after})
 elif before==after:entries.append({'mark':'','bodyrow':b['row'],'values':after})
 else:entries.extend([{'mark':'[-]','bodyrow':b['row'],'values':before},{'mark':'[+]','bodyrow':b['row'],'values':after}])
plan={'target':str(target),'hash':hashlib.sha256(target.read_bytes()).hexdigest(),'srs':str(srsfile),'srsHash':hashlib.sha256(srsfile.read_bytes()).hexdigest(),'sheet':s.title,'diffname':s.title+'_변경 비교','body':body,'entries':entries,'insertRow':current[53]+1,'requirements':requirements}
(Q/'plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf8')
print('81 requirements mapped; inserting test81 at',plan['insertRow'])
