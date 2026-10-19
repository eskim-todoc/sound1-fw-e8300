import json,zipfile
from pathlib import Path
from preserve_package import values
HERE=Path(__file__).resolve().parent;P=json.loads((HERE/'payload.json').read_text(encoding='utf-8'));books={b['key']:b for b in P['books']}
def pad(s):return '\n'+s.strip()+'\n'
def get(key):
 with zipfile.ZipFile(books[key]['target'])as z:return values(z,'xl/worksheets/sheet2.xml')
u=get('SUVC');it=get('SITC')
unit={}
unit['K1735']=pad(u['K1735'].strip().replace('QCC 송신20byte(hex)=','논리 메시지 버퍼20byte(hex)=').replace('송신20byte(hex)=','논리 메시지 버퍼20byte(hex)=')+'\n전송 경계: type+payload는최대20byte이며 위 hex는논리20byte버퍼(미사용00포함)이다. 실제SPI물리프레임은20byte+별도length1byte=21byte로구분한다.\n[QCC/E8300 자료 입력] 실제QCC 송신의length필드 값/위치 및논리메시지→SPI21byte변환 자료. E8300 응답index20=2와구분한다.')
unit['J1735']=pad(u['J1735'].strip().replace('QCC 송신20바이트,','QCC 논리메시지(type+payload 최대20byte)와실제SPI21byte(별도length1byte 포함),'))
integration={}
for addr in ['M131','M132']:
 integration[addr]=pad(it[addr].strip().replace('송신20byte(hex)=','논리 메시지 버퍼20byte(hex)=')+'\n전송 경계: type+payload최대20byte; 위20byte버퍼는미사용00채움까지표시한다. 물리SPI프레임은20byte+별도length1byte=21byte이다.\n[QCC/E8300 자료 입력] 실제QCC 송신length필드 값/위치·SPI21byte변환 자료. E8300 ACK의index20=2와구분한다.')
integration['E134']=pad(it['E134'].strip()+'\n검증 범위: RID46/67/68의 QCC PIO/연결표시·광고 및기동/종료상태 연계 지원. 등록된 내부기RF신원 확인과 실제자극출력 억제의 완주 판정은 E8300 SYS46/67/68의 해당경로에서별도확인한다. 기존E8300 결과의범위를유지한다.')
out={'unit':unit,'integration':integration,'integration_break_before_rows':[g['rows'][0]for g in books['SITC']['groups']],'unit_heights':{'1735':409.5},'integration_heights':{'131':350,'132':350}}
(HERE/'spi-patch.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
