import json,zipfile
from pathlib import Path
from preserve_package import values
HERE=Path(__file__).resolve().parent
p=json.loads((HERE/'payload.json').read_text(encoding='utf-8'));books={b['key']:b for b in p['books']}
with zipfile.ZipFile(books['SUVC']['target'])as z:criterion=values(z,'xl/worksheets/sheet2.xml')
with zipfile.ZipFile(books['SUVR']['target'])as z:report=values(z,'xl/worksheets/sheet2.xml')
scope={
 'E1727':'검증 범위: RID46의 QCC 연결표시 입력 지원, RID67/68의 기동/종료 상태 입력 지원. 등록 내부기 신원/RF 연결과 기동/종료 자극 억제의 완주 검증은 E8300 SYS46/67/68 경로에서 별도 확인한다. 기존 E8300 시험 결과 범위를 유지한다.',
 'E1729':'검증 범위: RID23/28/34의 배터리 상태·알림 조건에 필요한 QCC 전원/잔량 입력 지원. 실제 자극 출력과 LED/자극 알림의 완주 검증은 E8300의 해당 시스템 시험23/28/34 경로에서 별도 확인한다. QCC 상태 확인을 알림/자극 통과로 확대하지 않는다.',
 'E1736':'검증 범위: RID46의 QCC 연결표시/저장 상태 지원. 이 유닛의 상태 조회는 등록된 내부기 신원 및 근접 RF 연결을 검증하지 않는다. 해당 요구의 완주 검증은 E8300 SYS46 경로에서 별도 확인한다.',
 'E1738':'검증 범위: RID46의 연결표시를 받아 QCC Bluetooth 활성/장치명을 구성하는 연계 지원. 등록 내부기 신원/RF 연결은 E8300 SYS46, 페어링/보호 데이터의 완주 검증은 SYS48/49의 별도 경로에서 확인한다.',
 'E1740':'검증 범위: RID46의 연결표시와 RID67/68의 부착/탈착·기동/종료 상태 연계 지원 및 QCC Bluetooth 동작. 등록 내부기 신원/RF 검증과 자극 출력 억제의 완주 검증은 E8300 SYS46/67/68 경로에서 별도 확인한다.'}
cells={}
for c,extra in scope.items():
 assert criterion[c]==report[c]
 cells[c]=criterion[c].strip()+'\n'+extra
 cells[c]='\n'+cells[c]+'\n'
(HERE/'scope-patch.json').write_text(json.dumps({'cells':cells,'break_before_rows':[g['rows'][0]for g in books['SUVC']['groups']]},ensure_ascii=False,indent=2),encoding='utf-8')
