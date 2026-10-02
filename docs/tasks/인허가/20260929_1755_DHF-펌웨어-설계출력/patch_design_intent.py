"""Revise design instructions without flattening pending Word revisions."""
from copy import deepcopy
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path
import json
import re
import zipfile
from lxml import etree as E

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/design-readability/design-intent-and-placeholders'
LIVE = TASK.parents[2] / '참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
BASE = '1a5de1d93b4e8ff42db3d8960e89fa8dccd0fc9349ce3fb25dc293ab0ec027a9'
N = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main',
     'a': 'http://schemas.openxmlformats.org/drawingml/2006/main',
     'wp': 'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing',
     'r': 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
def q(s): return '{' + N['w'] + '}' + s
def txt(p): return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=N))
def packed(path):
    with zipfile.ZipFile(path) as z: return {n: z.read(n) for n in z.namelist()}

# Values identify the complete active paragraph, not arbitrary substrings in code.
CHANGES = {
 'PCM/오디오/HEAR 정지와 기상 대기 상태':
 'PCM/오디오/HEAR 정지와 절전 해제 입력 대기 상태',
 'PCM 및 출력단, I2S, DMIC, IOC, FIFO, HEAR의 동작을 정지한다. 인터페이스 DIO와 대기용 인터럽트를 전환한 뒤 저전력 요청을 해제하고 기상을 기다린다.':
 'PCM 및 출력단, I2S, DMIC, IOC, FIFO, HEAR의 동작을 정지한다. 인터페이스 DIO와 대기용 인터럽트를 전환한 뒤 저전력 요청을 해제하고 절전 해제 인터럽트를 기다린다.',
 '대역 대표값에는 현재 구현의 4비트 축소 보상을 적용한다. 주파수별 음향 응답이나 마이크 감도 보정은 이 정수 축소 연산과 별개다.':
 '대역 대표값은 4비트 우측 이동하여 매핑 입력의 크기 범위에 맞춘다. 주파수별 음향 응답이나 마이크 감도 보정은 이 정수 축소 연산과 구분한다.',
 'A/B는 Q16.8, x의 p승은 Q8.16으로 계산한다. xmin/xmax는 현재 맵의 입력 범위를 사용하며 구형 문서의 고정 숫자로 대체하지 않는다. 오디오 볼륨을 반영하는 매핑 계수 경로에는 별도의 볼륨 표가 사용되므로 AGC의 A 계수를 대신 적용하지 않는다.':
 'A/B는 Q16.8, x의 p승은 Q8.16으로 계산한다. xmin/xmax에는 선택한 맵의 입력 범위를 적용한다. 오디오 볼륨을 반영하는 매핑 계수는 별도의 볼륨 표로 계산하고, AGC의 이득 계수와 구분하여 관리한다.',
 '흐름 참고 : 일반 스트리밍의 I2S 게인은 1이다. 유효한 마이크 설정을 전제로 하며 미정의 마이크 수의 동작을 확장하지 않는다.':
 '흐름 참고 : 일반 스트리밍의 I2S 게인은 1로 설정한다. 마이크 수는 1개 또는 2개로 설정하며, 2개일 때는 전면 방향에 맞는 지연 합산 경로를 선택한다.',
 '흐름 참고 : 실제 운용 모드는 1/2이다. 알림 덮어쓰기는 일반 매핑 결과를 공유한 뒤 수행한다.':
 '흐름 참고 : 매핑 모드 1은 T 이하를 묵음으로 처리하고, 모드 2는 T를 출력 하한으로 사용한다. 일반 매핑 결과를 공유한 뒤 알림 채널의 크기를 적용한다.',
 '흐름 참고 : 현 설정 N=16의 흐름이다. 이 단계는 시작점 이동이나 전극 번호 변환을 하지 않는다.':
 '흐름 참고 : 선택 수 N은 16으로 정한다. 이 단계에서는 선택한 대역의 기본 교차 순서만 만들고, 시작점 조정과 전극 번호 변환은 패킷 구성 단계에서 수행한다.',
 '흐름 참고 : 패킷 생성 시 이전 마지막 대역에서 더 먼 끝을 선택한다. 마지막 요소가 더 멀면 그 요소 하나만 맨 앞으로 옮긴다. 현재 NofM은 자극당 3프레임, 1ms당 8개를 출력한다.':
 '흐름 참고 : 패킷 생성 시 이전 마지막 대역에서 더 먼 끝을 선택한다. 마지막 요소가 더 멀면 그 요소 하나만 맨 앞으로 옮긴다. NofM은 자극당 3프레임, 1ms당 8개로 나누어 출력하도록 설계한다.',
 'QCC 제어 핀을 GPIO 출력으로 설정한다. 초기에는 내부기 미연결, 종료 요청 상태로 설정한다. 소스에 남은 NRF 이름은 기존 함수 이름이며 실제 연계 대상은 QCC다.':
 'QCC 제어 핀을 GPIO 출력으로 설정한다. 초기에는 내부기 미연결, 종료 요청 상태로 설정한다. 내부기 연결 여부와 QCC 동작 요청은 각각의 제어 핀으로 전달한다.',
 '수신 잔량과 충전/뚜껑 상태를 갱신하고 조회 인터페이스로 제공한다. 현 동작 경로의 잔량은 QCC 수신값을 사용하며 구형 CFX LSAD 값을 잔량 입력으로 간주하지 않는다.':
 'QCC에서 수신한 배터리 잔량, 충전 상태와 크래들 뚜껑 상태를 각각 갱신하고 조회 인터페이스로 제공한다. 배터리 정보의 수신과 시스템 제어에서의 사용을 분리한다.',
 '두 코어와 주변장치의 종료 순서를 맞추면서 기상 입력은 유지한다.':
 '두 코어와 주변장치의 종료 순서를 맞추고 절전을 해제할 터치 입력을 계속 확인할 수 있게 한다.',
 '정상 장치 사용을 정리하고 CFX의 저전력 완료를 기다린다. 클럭과 I2C 속도를 대기 상태에 맞추며 터치 기상 감시를 수행한다. 기상 조건에서는 재기동 경로로 전환한다.':
 '일반 동작 중 사용하던 주변장치를 정리하고 CFX의 저전력 전환 완료를 기다린다. 클럭과 I2C 속도를 대기 상태에 맞춘 뒤 절전 해제용 터치 입력을 확인한다. 터치에 의한 절전 해제 조건을 만족하면 시스템을 다시 시작한다.',
 'CFX 정지 확인 전 클럭을 먼저 낮추지 않고 기상 감시 경로가 유지되어야 한다. 기상 조건은 재기동으로 연결되어야 한다.':
 'CFX 정지를 확인한 뒤 클럭을 낮추고, 대기 중에도 절전 해제용 터치 입력을 읽을 수 있어야 한다. 절전 해제 조건을 만족하면 재시작 경로로 연결한다.',
 '흐름 참고 : 그림은 한 번의 이벤트 처리를 나타낸다. 정지 검출 전까지 완료 상태를 발표하지 않는다.':
 '흐름 참고 : 각 인터럽트에서는 해당 시점의 버스 이벤트만 처리한다. STOP 조건을 확인한 뒤 전송 완료 상태를 호출자에게 전달한다.',
 '표 145. 현재 내부기 설정의 DAC 환산':
 '표 145. 계산 예에 적용하는 내부기 DAC 파라미터',
 'isd_ver1_1_2 설정에서 자극 디지털값은 (전류-오프셋 전류)에 선택 범위 환산 계수를 곱하고 15비트 우측 이동하여 구한다. 오프셋 레지스터도 선택한 오프셋 계수로 계산한다. 펄스폭 25us, 32채널 모두 T=100/C=300uA이면 A/A 범위, 오프셋 전류 100uA, 오프셋 레지스터 44, T=0/C=127이 된다. 범위 선택과 전하량 조건의 분기는 각각 확인한다.':
 '내부기의 DAC 파라미터가 표 145와 같을 때, 자극 디지털값은 (전류-오프셋 전류)에 선택 범위의 환산 계수를 곱하고 15비트 우측 이동하여 구한다. 오프셋 레지스터도 선택한 오프셋 계수로 계산한다. 이 파라미터에서 펄스폭 25us, 32채널 모두 T=100/C=300uA를 입력하면 A/A 범위, 오프셋 전류 100uA, 오프셋 레지스터 44, T=0/C=127이 된다. 연결할 내부기의 DAC 상한과 환산 계수는 파라미터로 제공하고, 범위 선택과 전하량 조건을 각각 판단하도록 한다.',
 '입출력 예 : isd_ver1_1_2, 펄스폭 25us, 32채널 모두 T=100uA/C=300uA이면 Stimulation_DAC_A/Offset_DAC_A를 선택한다. 오프셋 전류=100uA, 오프셋 레지스터=44, T 디지털값=0/C 디지털값=127이고 반환값은 true이다. 범위 선택에 실패하면 false를 반환한다.':
 '입출력 예 : 내부기 파라미터가 표 145와 같고 펄스폭 25us, 32채널 모두 T=100uA/C=300uA이면 자극 DAC A/오프셋 DAC A를 선택한다. 오프셋 전류=100uA, 오프셋 레지스터=44, T 디지털값=0/C 디지털값=127이고 반환값은 true이다. 범위 선택에 실패하면 false를 반환한다.',
 '흐름 참고 : 현재 빌드의 전류 범위 선택 흐름이다. 선택 순서는 작은 자극 DAC 범위와 offset 조합부터이며 정수 나눗셈 오차를 포함한다.':
 '흐름 참고 : 필요한 전류 범위를 만족하는 조합을 작은 자극 DAC 범위와 오프셋 범위부터 찾는다. 정수 환산 시 버려지는 소수 부분을 고려하여 디지털값을 계산한다.',
 '흐름 참고 : 유효한 맵이 입력 전제다. 대역 수 부족 로그를 자동 안전 정지로 표현하지 않는다. 완료 통보는 호출자가 처리한다.':
 '흐름 참고 : NofM에 사용할 맵은 대역 수가 16 이상이어야 한다. 이 유닛은 계산 결과와 성공 여부를 반환하고, 호출자는 결과를 확인한 뒤 계산 완료 상태를 CFX에 알린다.',
 '흐름 참고 : 현재 활성화된 내부기 전원 응답 방식이다. 송신 전원은 정의된 상하한 내에서 1단계 조정한다. 각 단계는 반복 호출로 진행하며 오류 응답 후에도 FIFO 정리와 플래그 해제를 수행한다.':
 '흐름 참고 : 내부기의 전원 상태 응답에 따라 송신 전원을 정의된 상하한 안에서 한 단계씩 조정한다. 각 단계는 반복 호출로 진행하며, 오류 응답을 처리한 뒤에도 FIFO와 완료 플래그를 정리한다.',
 '내부기별 정보와 사용자 설정을 파일 시스템에 저장/읽기한다. 기본 자료를 기록하는 경로와 변경 자료를 저장하는 경로를 구분한다. 구형 CFX의 EEPROM 직접 접근 역할을 CM3 파일 처리로 이관한다.':
 '내부기별 정보와 사용자 설정의 파일 저장/읽기는 CM3에서 담당한다. 기본 자료를 기록하는 경로와 변경 자료를 저장하는 경로를 구분하고, CFX에서 사용할 자료는 공유 메모리로 전달한다.',
 '권한이 확인된 대상의 조회 및 설정 명령을 처리한다.':
 '리모콘 연결 허용 상태에 따라 조회와 설정 명령을 처리하고, 명령별 응답을 준비한다.',
 '리모콘 명령, 인증 비교 결과, 현재 내부기/설정':
 '리모콘 명령, 연결 허용 상태, 선택한 내부기/설정',
 '수신 패킷에서 명령을 추출하고 필요한 인증 비교 결과를 확인한다. 해당 기능을 실행하여 응답을 구성한 뒤 처리한 명령을 정리한다.':
 '수신 패킷에서 명령을 추출한다. 연결 확인 명령은 리모콘 연결 허용 상태를 설정하고, 일반 명령은 그 상태를 확인한 뒤 실행한다. 명령별 응답을 구성한 뒤 처리한 명령을 정리한다.',
 '명령별 인증 조건을 만족하는 경로에서만 해당 처리가 실행되고 완료 후 명령 상태가 정리되어야 한다.':
 '일반 명령은 연결 허용 상태에서만 실행하고, 연결 확인 명령은 별도로 처리한다. 처리 완료 후 명령 상태를 정리해야 한다.',
 '흐름 참고 : 현재 E8300의 패스키 바이트 비교는 비활성이다. 연결 확인 명령의 허가 설정을 인증 검증 성공으로 해석하지 않는다.':
 '흐름 참고 : 연결 확인 명령의 응답과 일반 명령의 허용 상태를 구분하여 관리한다. 이 허용 상태는 명령 실행을 제어하는 상태값이며 암호학적 인증 결과와는 구분한다.',
 '갱신 데이터 패킷을 파일 기록 처리에 전달한다. 기동 정보 조회 및 선택 명령을 구분하여 상태 파일을 읽거나 갱신한다. OTA 저장 경로의 슬롯 범위와 부트로더 전체 슬롯 범위를 같다고 가정하지 않는다.':
 '갱신 데이터 패킷을 파일 기록 처리에 전달한다. 기동 정보 조회와 선택 명령은 구분하여 상태 파일을 읽거나 갱신한다. OTA 명령에서 선택할 수 있는 슬롯은 1~2로 제한하고, 부트로더의 슬롯 정보 범위 1~4와 구분한다.',
 '흐름 참고 : 여기서는 OTA 파일 데이터 경로를 표시한다. 기동 슬롯 명령은 같은 유닛의 별도 인터페이스이며 QCC 내부 갱신 절차를 뜻하지 않는다.':
 '흐름 참고 : OTA 파일 데이터의 수신 순서와 파일 쓰기 결과를 확인한다. 기동 슬롯 명령은 별도 인터페이스로 처리하며, QCC 자체 펌웨어의 갱신 절차와 구분한다.',
 '흐름 참고 : 활성 기동 LED는 CM3 이관 전에 표시 완료를 기다려 정리한다. CRC는 완결성 검사이며 인증을 대신하지 않는다.':
 '흐름 참고 : 기동 LED를 표시한 경우에는 표시 완료를 확인하고 정리한 뒤 CM3 응용 프로그램으로 실행 제어를 넘긴다. CRC는 데이터 손상 여부를 검사하는 값으로 사용하고 인증과 구분한다.',
 '흐름 참고 : 현재 메모리에 읽은 목록의 파일 처리 흐름이다. 키 값이나 제조 정보를 순서도에 기재하지 않는다.':
 '흐름 참고 : 매니페스트는 적재할 파일 이름을 담은 목록이다. 목록에서 파일을 하나씩 선택하여 적재하고, 해당 파일의 처리를 마친 뒤 다음 파일로 이동한다.',
 '동일할 때 통과, 다를 때 오류를 반환해야 한다. 데이터 길이와 정렬 조건은 이 함수의 입력 전제로 구분한다.':
 '계산한 CRC와 기준 CRC가 같으면 성공, 다르면 오류를 반환한다. 호출자는 검사할 데이터의 바이트 길이와 메모리 정렬을 CRC 인터페이스의 조건에 맞춰 전달한다.',
 '암호화 구획, 복호화 문맥/키 선택 정보':
 '암호화된 이미지 구획, 복호화 처리 상태와 키 선택 정보',
 '이미지의 키 선택 정보로 복호화 문맥을 초기화하고 암호화 구획을 복호화한다. 실제 제품 키를 문서/로그에 기록하지 않는다. 복호화와 CRC 검사는 서로 다른 단계로 처리한다.':
 '이미지의 키 선택 정보로 복호화 처리 상태를 초기화하고 암호화된 구획을 복호화한다. 제품 키는 지정된 키 관리 경로에서 사용하며 문서나 로그에 노출하지 않는다. 복호화와 CRC 검사는 서로 다른 단계로 처리한다.',
 'CM3가 수신한 이미지와 기동 상태를 읽어 다음 실행 위치를 정한다. 갱신 전송, 파일 완결성 검사, 기동 위치 선택을 구분하며 이미지 CRC가 인증 기능을 대신하지 않는다.':
 'CM3가 수신한 이미지와 기동 상태를 읽어 다음 실행 위치를 정한다. 갱신 데이터 전송, 이미지의 데이터 손상 여부 검사, 기동 위치 선택을 각각의 단계로 나누며, CRC 검사와 인증을 구분한다.',
 '상태 파일을 확인하고 구조체 크기만큼 읽는다. 파일이 없거나 읽기가 불완전하면 기본 상태를 초기화하여 기록한다. 현재 읽기 유효 판정은 읽기 결과/크기 기준이며 구조체의 CRC 필드가 있다는 이유만으로 CRC 검사를 수행한다고 간주하지 않는다.':
 '상태 파일을 확인하고 구조체 크기만큼 읽는다. 파일이 없거나 읽기에 실패하거나 읽은 크기가 부족하면 기본 상태를 초기화하여 기록한다. 상태 파일의 읽기 결과와 바이트 수로 정상 읽기 여부를 판단하며, 이미지의 CRC 검사와는 별도로 처리한다.',
 '흐름 참고 : 읽기 성공은 구조체 크기로 판정한다. CRC 필드가 있다는 이유로 상태 파일의 CRC 검증을 수행한다고 해석하지 않는다.':
 '흐름 참고 : 상태 파일 읽기는 반환 상태와 읽은 바이트 수가 구조체 크기와 같은지로 판단한다. 실패하면 기본 상태를 기록한 뒤 다시 사용할 수 있도록 한다.',
 '흐름 참고 : 응용 코드가 기록한 대체 기동 성공 상태를 다음 기동에서 확인한다. 같은 호출 안에서 성공을 임의 확정하지 않는다.':
 '흐름 참고 : 대체 슬롯의 실행 성공 상태는 응용 프로그램이 기록하도록 한다. 부트로더는 다음 기동에서 이 상태를 읽어 대체 슬롯 유지 또는 기본 슬롯 복귀를 결정한다.',
}

def main():
    QA.mkdir(parents=True, exist_ok=True)
    raw = LIVE.read_bytes()
    assert sha256(raw).hexdigest() == BASE, 'Live author edits changed; reread.'
    before = QA / 'before.docx'
    if before.exists(): assert before.read_bytes() == raw
    else: before.write_bytes(raw)
    parts = packed(LIVE); root = E.fromstring(parts['word/document.xml'])
    body = root.find(q('body'))
    stamp = datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    seq = max(int(v) for v in root.xpath('//@w:id', namespaces=N) if v.isdigit()) + 1
    def rev(tag):
        nonlocal seq
        e = E.Element(q(tag), {q('id'): str(seq), q('author'): '김은수', q('date'): stamp})
        seq += 1; return e
    changes = []
    def replace(p, value, red=False):
        old = txt(p)
        ts = p.xpath('.//w:t[not(ancestor::w:del)]', namespaces=N)
        assert ts and all(t.xpath('ancestor::w:ins', namespaces=N) for t in ts), old
        # Pending inserted design text belongs to the existing redline; never nest it.
        if p.xpath('.//w:instrText', namespaces=N):
            # Keep the live SEQ number field; edit only the caption suffix.
            prefix = re.match(r'^표 \d+\.', old).group(0)
            assert value.startswith(prefix)
            last = p.xpath('.//w:fldChar[@w:fldCharType="end"]', namespaces=N)[-1]
            suffix = last.getparent().xpath('following-sibling::w:r/w:t', namespaces=N)
            assert suffix
            suffix[0].text = value[len(prefix)-1:]
            for t in suffix[1:]: t.text = ''
        else:
            ts[0].text = value
            for t in ts[1:]: t.text = ''
        if red:
            for t in ts:
                run = t.getparent(); rp = run.find(q('rPr'))
                if rp is None: rp = E.Element(q('rPr')); run.insert(0, rp)
                color = rp.find(q('color'))
                if color is None: color = E.SubElement(rp, q('color'))
                color.set(q('val'), 'C00000')
        changes.append({'before': old, 'after': value, 'red': red})
    allps = body.xpath('.//w:p[not(ancestor::w:del)]', namespaces=N)
    for old,new in CHANGES.items():
        matches = [p for p in allps if txt(p) == old]
        assert len(matches) == 1, (old, len(matches))
        assert '·' not in new
        replace(matches[0], new)

    # Missing internal design is not a claim about the confirmed E8300 interface.
    old = 'QCC 내부 모듈과 유닛은 QCC 펌웨어 자료를 확보한 후 이 절에 추가한다. E8300 연계 유닛과 구분되는 QCC 식별자를 부여하고 각 유닛의 목적, 입출력, 처리와 인터페이스를 정의한다.'
    replace(next(p for p in allps if txt(p)==old), '[자료 추가 위치] 여기에 QCC 내부 모듈과 유닛의 상세 설계를 입력하세요. QCC 자료 수령 후 전용 식별자, 유닛의 목적, 입력/출력/처리, 인터페이스와 필요한 데이터를 작성한다. E8300 연계 유닛은 그대로 유지한다.', True)
    def add_after(anchor,value,level):
        hits=[p for p in body.findall(q('p')) if txt(p)==anchor]
        assert len(hits)==1,(anchor,len(hits))
        p=E.Element(q('p'));pp=E.SubElement(p,q('pPr'))
        E.SubElement(pp,q('pStyle'),{q('val'):f'TDC{level}0'})
        E.SubElement(pp,q('rPr')).append(rev('ins'))
        ins=rev('ins');p.append(ins);run=E.SubElement(ins,q('r'))
        rp=E.SubElement(run,q('rPr'));E.SubElement(rp,q('color'),{q('val'):'C00000'})
        E.SubElement(run,q('t')).text=value
        hits[0].addnext(p);changes.append({'after_anchor':anchor,'after':value,'red':True})
    add_after('E8300 연계', '[자료 추가 위치] 여기에 QCC 측 연계 조건을 대조한 내용을 입력하세요. SPI 명령/응답, I2S 오디오와 QCC_CTRL/ISD_CHECK의 송수신 조건을 QCC 자료와 확인하고 아래 설계에 필요한 차이만 반영한다.', 2)
    add_after('<그림 5. 블루투스 연계>', '[자료 추가 위치] 여기에 QCC 내부 모듈과 E8300 연결 경로를 보완한 그림을 넣으세요. 자료 수령 후 그림의 QCC 내부 영역을 채우고, 하드웨어 구조와 다른 모듈의 연계 그림에도 영향이 있는지 확인한다.', 2)
    add_after('소프트웨어 상세 설계 검증', '[자료 추가 위치] 여기에 QCC 내부 상세 설계의 검토 내용을 입력하세요. 내부 자료를 추가한 뒤 모듈/유닛의 역할, 입출력 조건과 E8300 연계가 서로 일치하는지 검토한다.', 1)

    def deleted(p): return bool(p.xpath('./w:pPr/w:rPr/w:del',namespaces=N))
    def blank(p):
        return p.tag==q('p') and not txt(p) and not p.xpath('.//w:drawing|.//w:br|.//w:fldChar|.//w:delText',namespaces=N)
    gaps=[]
    for p in list(body):
        if p.tag!=q('p') or not txt(p).startswith('SU-ID-'):continue
        previous=p.getprevious();blanks=[]
        while previous is not None:
            if previous.tag==q('p') and deleted(previous):previous=previous.getprevious();continue
            if blank(previous):blanks.append(previous);previous=previous.getprevious();continue
            break
        ps=previous.find('w:pPr/w:pStyle',N) if previous is not None else None
        if ps is not None and ps.get(q('val')) in ('TDC1','TDC2','TDC3','TDC4','TDC5','TDC6'):
            for b in blanks:
                assert b.xpath('./w:pPr/w:rPr/w:ins',namespaces=N), 'Only remove our inserted separator.'
                body.remove(b)
            if blanks:gaps.append({'unit':txt(p),'reason':'consecutive headings','removed':len(blanks)})

    # The diagram uses the same forward design terms as its adjacent explanation.
    import draw_unit_flows as draw
    from unit_flowcharts import FLOWS
    edited_images=[]
    relroot=E.fromstring(parts['word/_rels/document.xml.rels'])
    rels={e.get('Id'):e.get('Target') for e in relroot}
    for key in ('CM3-029','BOOT-010'):
        entry=draw.render(key,FLOWS[key])
        cap=next(p for p in body.findall(q('p')) if re.match(r'그림\s+\d+\.\s+SU-ID-'+key+r'\b',txt(p)))
        dp=cap.getprevious()
        rid=dp.xpath('.//a:blip/@r:embed',namespaces=N)[0]
        target=rels[rid];part='word/'+target
        assert part.startswith('word/media/')
        parts[part]=Path(entry['path']).read_bytes()
        width=4320000; height=round(width*entry['height']/entry['width'])
        for size in dp.xpath('.//wp:extent|.//a:xfrm/a:ext',namespaces=N):
            size.set('cx',str(width));size.set('cy',str(height))
        edited_images.append({'unit':key,'part':part,'source':entry['path']})

    parts['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
    with zipfile.ZipFile(QA/'candidate.docx','w',zipfile.ZIP_DEFLATED) as z:
        for n,v in parts.items():z.writestr(n,v)
    assert LIVE.read_bytes()==raw
    report={'baseline':BASE,'changes':changes,'gap_changes':gaps,'images':edited_images,
            'pending_tracking_preserved':True,'candidate_sha256':sha256((QA/'candidate.docx').read_bytes()).hexdigest()}
    (QA/'patch.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'wording_edits':len(CHANGES),'red_markers':4,'heading_gaps_removed':len(gaps),'images':len(edited_images)},ensure_ascii=True))

if __name__=='__main__':main()
