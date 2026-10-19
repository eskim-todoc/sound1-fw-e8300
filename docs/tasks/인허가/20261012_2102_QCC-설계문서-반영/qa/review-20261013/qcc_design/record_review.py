import json
from pathlib import Path
from editor import ROOT

CHECKS=[
(1,'통과','활성82개 요구1..81+83; 송신을RID69로 통합, RID82현재참조0. DMIC2 양쪽 입력과 두 오디오 방향을 본문/그림에 작성.'),
(2,'통과','requirements-contract.json 최종 기본/상세와 오디오69/문서83 설계 연결을 대조. SRS 자체 작성은 요구사항 담당.'),
(3,'실패','4모듈/14유닛/실제시험ID 연결 확인. 저장 시험2/5/9의 보조 요구사항 연결 추가 필요; cross-loop 후 재검사.'),
(4,'통과','E/Q/current9개 실제ZIP 본문·표·그림 확인; 스마트케이스/CC 잔존식별은 local근거에만 보존.'),
(5,'통과','시스템 구조와 현 E8300 SPI/I2S 계약 및 사용자 확정 DMIC2 이중 연결 대조; QCC 실제 핀/코덱/내부코드 미확정 유지.'),
(6,'실패','시험 저장본→설계 역방향 대조에서 지원 요구 누락 발견. 본문 조건 동일; 표적 추적 보완 후 동일ID/조건 재검사 예정.'),
(7,'통과','SAD E기능 항목 틀/본문 수준에 맞춰4모듈/14항목, SDDD 동종2열표grid1304/7512 유지. Q전용 소속유닛 틀 제거.'),
(8,'통과','14유닛 목적 본문과 입력/출력/처리 표, 구현 조건/예외/키 수명주기 설명. 시험문서 없이 기능 이해 가능. 미확정 계약 분리.'),
(9,'통과','SIP 시험방법은 증거별 I/A/D/T 정의 유지. 새 SYS82 D 송신과 정량 I2S 측정을 구분; 해당 Excel방법열은 시험 담당.'),
(10,'통과','기준/실제 증거와 PC/모의/실장치 범위 분리. 코드분석을 물리QCC/I2S 실측으로 기재하지 않음.'),
(11,'통과','SIP 사전 구성 표와 실제 연결/확인 통합단계 구분. 제한된 핀/설정은 구체 미비로 보존.'),
(12,'해당없음','소유3DOCX에는 기준/보고서 공통셀 없음. 6Excel 공통열/병합 검사는 시험 담당 증거를 연결.'),
(13,'통과','SIP 각 단계에 실제 시험ID와 기능/비교대상 기재. 수치·배열 결과는 기준/보고서 같은 조건에서 대조하도록 명시.'),
(14,'해당없음','새 실제 RTT 결과를 작성하지 않음. 기존 E8300 결과 범위 보존, 패턴전달을 CRA 성공으로 확대하지 않음.'),
(15,'해당없음','시험 조건/검증 번호 행 변경 없음. 기준/보고서 번호 대응은 시험 담당 소유.'),
(16,'통과','새 QCC 결과 미실행/제품판정대기. 설계 작성/검토를 실제 관찰/시험 통과로 기재하지 않음.'),
(17,'통과','사용자 시험자 확인 기존범위 보존. Codex 새 보드실행/새원시로그 수집 주장 없음.'),
(18,'통과','수신 SYS69와 송신 SYS82를 별도 실행 조건/판정으로 유지. QCC 실제자료/미측정 계약 완료로 확대하지 않음.'),
(19,'해당없음','과거 사용자 지정 시험자 확인 결과의 중간값/절삭 작성 작업 아님; 새 측정값 생성 없음.'),
(20,'해당없음','아키텍처 코드-native 도식만 작성, 예상 계측기/파형사진이나 측정값 생성 없음. 삭제 지시 사진 요청 재추가 없음.'),
(21,'해당없음','일반 시간시험 입력/결과 날짜 변경 작업 없음. 기존 서명/승인날짜 보존.'),
(22,'통과','SAD23/SDDD21/SIP4 미비 안내 지정색/기본10+2pt/밑줄/비굵게. 알려진DMIC2연결을 미확인으로 확대하지 않음; 접두어 유지.'),
(23,'미실행','중앙현재위치 등록은 root 소유. 최종필드/렌더 후 실제저장 검색 위치와 미비 목록 전달 예정.'),
(24,'해당없음','Excel 셀/행높이/병합/인쇄 서식 쓰기 소유권 없음. 관련 저장본 의미 대조만 수행.'),
(25,'해당없음','Word actual track작성. Excel [-]/[+] 표시/배경 복구는 시험/요구 담당 소유.'),
(26,'통과','newauthor김은수, priorhistory보존. 신규삽입중첩0, 새트랙 거부 C14N currentbaselineexact. 원media/관계 보존, drawing tracked교체.'),
(27,'통과','동일Gstage [E8300-QCC]3사본에만 작성. E/Q6원본 해시 불변, 작성자전용/승인란/서명 보존.'),
(28,'미실행','native Open3후보 통과. 최종root 필드/목차 및 native전체페이지 검증 대기. 제품시험/정식승인 별도.'),
(29,'미실행','README/actualDOTX 읽고 saved59TDC정의/hash 대조. 새동종표 폭/스타일/반복머리글/정상빈문단 static통과. 실제폰트/기준선 및 전체렌더 final대기.')]
def main():
    assert [n for n,_,_ in CHECKS]==list(range(1,30))
    r={'scope':'Owned SAD/SDDD/SIP content and package; independent requirements/test cross-check. No other owner edits.','checks':[{'number':n,'status':s,'inspection_and_reason':v,'product_state':'시험/제품판정대기'if n in [5,8,16,18,22,28]else'해당없음'}for n,s,v in CHECKS],
       'renderer_attempt':{'tool':'documents/render_docx.py','candidate':'SAD','result':'FileNotFoundError: LibreOffice soffice.exe was not found on PATH','fallback':'root serialized native Word PDF and bundled pypdfium2 PNG','native_final_status':'미실행'},
       'format_source':'E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx','evidence':['input-audit.json','format-baseline.json','candidate-qa.json','authored.json','SAD-authored.json','SDDD-authored.json','SIP-authored.json'],
       'status':'실패','remaining':['지원 요구 연결 누락 표적 보완/재검사','root 최종필드/native전체페이지 검사','root 현재위치 등록']}
    (ROOT/'checks-29.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
    print('29 item checks recorded; content cross-loop and native final checks pending.')
if __name__=='__main__':main()
