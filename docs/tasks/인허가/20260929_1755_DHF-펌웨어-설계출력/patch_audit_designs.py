"""Extend three existing units without changing IDs, images or document styles."""
from copy import deepcopy
from datetime import datetime, timezone
import json
import zipfile
from lxml import etree as E
from inspect_audit_units import QA, FILES
from inspect_sad_unit_trace import N, q, text
from patch_sad_audit_trace import parts, current_rows, run

PURPOSES = {
 1:'정상 반복 처리가 시작되기 전에 통신과 주기 이벤트의 실행 환경을 준비하고, 연결 이력을 남길 감사기록 저장 영역을 초기화한다.',
 5:'CM3의 여러 제어 기능을 같은 반복 흐름에서 순서대로 진행하고, 개발/점검용 조회 요청이 들어오면 저장된 감사기록을 읽어 출력한다.',
 46:'리모콘 연결 허용 상태에 따라 조회와 설정 명령을 처리하고, 연결 확인 시 받은 시간과 주소를 감사기록에 남긴다.'}
EDITS = {
 1:{
 '입력':'기동 상태, 보드 주변장치 설정, 기존 감사기록 파일',
 '출력':'I2C/SPI/DMA/인터럽트 초기 상태, 감사기록 파일의 식별자/기록 수/다음 기록 위치',
 '처리':'초기화 진입점에서 인터럽트와 DMA 상태를 정리하고 I2C와 SPI를 설정한다. 주변장치 설정 완료 후 필요한 인터럽트와 전송 경로를 허용한다.\n감사기록을 쓰기 전에 저장 영역을 연다. 파일 크기, 식별자, 기록 수와 다음 기록 위치가 유효하면 기존 내용을 보존한다. 파일이 없거나 정보가 유효하지 않으면 저장 영역을 비우고 식별자와 초기 상태를 기록한다. 초기화 후 파일의 변경 내용을 저장하고 닫는다.',
 '관련 함수':'Initialize, init_I2c, init_cm3_SPI, ci_SPI_enable_DMA / ci_timer_init, ci_event_log_init',
 '동작 조건':'주변장치 사용보다 초기화가 먼저 이루어져야 한다. 감사기록 영역은 4,096바이트이며, 앞의 16바이트에 식별자 12바이트와 다음 기록 위치/기록 수 각 2바이트를 둔다. 이어서 16바이트 기록을 최대 255개 저장한다. 초기 기록 수와 다음 위치는 0이다. 유효한 다음 위치는 0~254, 기록 수는 0~255이다.\n파일 열기, 필요한 초기화 기록, 변경 내용 저장 또는 닫기에 실패하면 실패(-1)를 반환하고, 정상 완료 시 0을 반환하도록 한다.'},
 5:{
 '입력':'주기 이벤트, 터치/통신/내부기 상태, 개발/점검용 감사기록 조회 명령',
 '출력':'각 기능 처리와 정상/절전 전환, 저장된 감사기록의 시간/주소/이벤트 출력',
 '처리':'입력 상태를 수집하고 시스템 제어, 맵 갱신, 내부기 실행, 통신 처리를 진행한다. 종료 조건에서는 반복 실행을 해제하고 절전 경로로 전환한다.\n개발/점검 명령을 사용하는 구성에서는 정상 반복 중 조회 요청을 확인한다. --dump_log를 받으면 감사기록 파일을 읽기 전용으로 열고 파일 크기, 식별자, 기록 수와 다음 위치를 검사한다. 유효한 기록의 시간, 블루투스 주소와 이벤트를 저장 배열의 순서대로 RTT에 출력한다. 조회는 저장 내용을 변경하지 않는다.',
 '관련 함수':'func_normal, disable_iteration / tdc_ui_command_poll, handle_dump_log, ci_event_log_read',
 '동작 조건':'입력 갱신 후 해당 제어 처리가 실행되고 종료 후에는 정상 반복이 계속되지 않아야 한다. 감사기록 조회 함수는 파일 열기/읽기 또는 기록 정보 검사 실패 시 -1, 정상 조회 시 0을 반환한다. 조회 완료 안내와 실제 기록의 정상 출력 여부를 구분한다.\n기록에는 연/월/일/시/분/초, 주소 6바이트와 이벤트 4바이트를 표시한다. 기록 수가 0이면 식별자/기록 수/다음 위치만 출력한다. 순환 저장된 배열은 시간순으로 다시 정렬하지 않는다.'},
 46:{
 '입력':'리모콘 명령, 연결 허용 상태, 선택한 내부기/설정, 연결 확인 명령에 포함된 기준 시간과 블루투스 주소',
 '출력':'명령 실행 결과와 응답, 연결 이벤트와 해당 시간/주소가 저장된 감사기록',
 '처리':'수신 패킷에서 명령을 추출한다. 연결 확인 명령은 리모콘 연결 허용 상태를 설정하고, 일반 명령은 그 상태를 확인한 뒤 실행한다. 명령별 응답을 구성한 뒤 처리한 명령을 정리한다.\n연결 확인 시 데이터의 연/월/일/시/분/초로 기준 시간을 갱신하고, 주소 6바이트를 설정한 다음 연결 이벤트(0x40)를 기록한다. 기록 시에는 기준 시간에 경과 시간을 반영하여 시간 6바이트, 주소 6바이트, 이벤트 4바이트를 한 묶음으로 저장한다. 기록 수는 255에서 더 늘리지 않고, 다음 기록 위치는 254 다음에 0으로 돌아가 가장 오래된 자리를 교체한다.',
 '관련 함수':'fetch_remoteControlPacket, remoteControl, getRemoteCommand, clearRemoteColtrolCommand / is_isd_passKeyMatch, set_isd_passKeyMatchResult / ci_timer_update_reference_time, ci_timer_get_reference_time_after_self_update, ci_event_log_update_bt_addr, ci_event_log_write',
 '동작 조건':'일반 명령은 연결 허용 상태에서만 실행하고, 연결 확인 명령은 별도로 처리한다. 처리 완료 후 명령 상태를 정리한다.\n시간은 연결 확인 데이터의 4~9번 바이트, 주소는 10~15번 바이트에서 받는다(0부터 센 위치). 연도는 2000년을 기준으로 한 값이다. 예를 들어 시간 {26,10,2,13,14,15}, 주소 {12,34,56,78,9A,BC}(16진수), 이벤트 0x40이면 2026-10-02 13:14:15의 연결 기록을 저장한다. 경과 1,000 ms마다 시간을 1초 증가시키고 일/월/연도 경계를 반영한다.\n파일 열기/읽기/쓰기와 기록 정보 검사에 실패하면 저장을 완료한 것으로 처리하지 않고 오류를 전달한다. 파일 닫기까지 정상 완료한 경우 0, 실패한 경우 -1을 반환하도록 한다.'}}
NOTES={
 1:'[E8300 보완] 변경 내용 저장(f_sync) 실패 시 초기화 함수가 실패를 반환하는지 확인해야 한다. 현재 보충 시험에서는 실패 응답을 넣어도 0을 반환한다.',
 5:'[E8300 확인] RTT 조회의 인가자 접근 제한과 출하 시 디버그 사용 조건을 확정해야 한다. 외부 전송을 제공하는 경우 전송 보호 조건도 정한다. 로컬 RTT 조회는 QCC 무선 암호화와 다른 경로이다.',
 46:'[E8300 보완] 파일 닫기(f_close) 실패 시 기록 함수가 실패를 반환하는지 확인해야 한다. 현재 보충 시험에서는 실패 응답을 넣어도 0을 반환한다. 시간 입력의 유효 범위와 출처 확인, 저장 실패의 상위 명령 전달 조건도 확인한다.'}

def edit_p(p, value, mode=None):
    """Edit the author's pending insertion, preserving earlier deletion history."""
    insertions=p.findall('w:ins',N)
    assert len(insertions)==1 and not p.findall('w:r',N), text(p)
    ins=insertions[0]
    assert ins.get(q('author'))=='김은수'
    for c in list(ins): ins.remove(c)
    ins.append(run(value,mode))

def main():
    changes=[]
    for kind in ('sad','sddd'):
        package=parts(QA/(kind+'-before.docx'))
        root=E.fromstring(package['word/document.xml'])
        if kind=='sad':
            rows=current_rows(root)
            for num in (50,74):
                cell=rows[f'SW-FW-RID-{num}'].findall('w:tc',N)[2]
                p=cell.findall('w:p',N)[-1]
                assert '[E8300 보완]' in text(p)
                old=text(p)
                # This paragraph was a pending insertion: omit it without losing original deletions.
                cell.remove(p)
                changes.append({'file':kind,'id':num,'before':old,'after':'기존 유닛 연결 유지, 상세 설계 보완 안내 제거'})
        else:
            body=root.find(q('body'))
            positions={1:(880,881,883,886,887),5:(937,938,940,943,944),46:(2281,2282,2284,2287,2288)}
            maxid=max(int(v) for v in root.xpath('//@w:id',namespaces=N) if v.isdigit())
            note_positions=[]
            for unit,(heading,purpose,design,conditions,note) in positions.items():
                assert text(body[heading]).startswith(f'SU-ID-CM3-{unit:03d},')
                edit_p(body[purpose],PURPOSES[unit])
                for idx in (design,conditions):
                    for row in body[idx].findall('w:tr',N):
                        cells=row.findall('w:tc',N)
                        key=text(cells[0])
                        if key in EDITS[unit]:
                            paragraphs=cells[1].findall('w:p',N)
                            assert len(paragraphs)==1
                            edit_p(paragraphs[0],EDITS[unit][key])
                # Keep the normal blank paragraph after the table. Insert the note
                # after it only after all original body indexes have been used.
                anchor=body[note]
                assert not text(anchor)
                p=E.Element(q('p'))
                pr=E.SubElement(p,q('pPr'))
                E.SubElement(pr,q('pStyle'),{q('val'):'TDC43'})
                E.SubElement(pr,q('spacing'),{q('after'):'160'})
                rp=E.SubElement(pr,q('rPr'))
                maxid+=1
                E.SubElement(rp,q('ins'),{q('id'):str(maxid),q('author'):'김은수',q('date'):'2026-10-12T01:00:00Z'})
                maxid+=1
                ins=E.SubElement(p,q('ins'),{q('id'):str(maxid),q('author'):'김은수',q('date'):'2026-10-12T01:00:00Z'})
                ins.append(run(NOTES[unit],'e8300'))
                note_positions.append((anchor,p))
                changes.append({'file':kind,'unit':f'CM3-{unit:03d}','purpose':PURPOSES[unit],'fields':EDITS[unit],'note':NOTES[unit]})
            for anchor,p in note_positions:
                anchor.addnext(p)
            for p in root.findall('.//w:p',N):
                old=text(p)
                if '[자료 추가 위치]' in old and 'QCC' in old:
                    new=old.replace('[자료 추가 위치]','[QCC 자료 입력]')
                    edit_p(p,new,'qcc')
                    changes.append({'file':kind,'before':old,'after':new,'format':'red 12pt bold underline'})
        package['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
        with zipfile.ZipFile(QA/(kind+'.docx'),'w',zipfile.ZIP_DEFLATED) as z:
            for name,data in package.items():z.writestr(name,data)
    (QA/'design-changes.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'documents':2,'units':3,'new_unit_ids':0,'QCC_sddd_markers':4},ensure_ascii=False))

if __name__=='__main__': main()
