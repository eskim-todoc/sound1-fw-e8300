# BOOT-019 부트 상태 파일 읽기와 초기화

시험 ID: `SW-FW-UnitTest-BOOT-ID-19`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1649~1685행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1649 -->
전원 재인가 뒤에도 선택한 이미지와 갱신 진행 정보를 유지한다.<br>
<!-- /source-cell:E1649 -->


## 시험 방법

<!-- source-cell:F1649 -->
D<br>
<!-- /source-cell:F1649 -->


## 상세 설계 유닛

<!-- source-cell:G1649 -->
SU-ID-BOOT-019<br>
(부트 상태 파일 읽기와 초기화)<br>
<!-- /source-cell:G1649 -->


## 시험 준비와 입력

<!-- source-cell:H1649 -->
- 시험 상태 파일 없음/짧은읽기/정상읽기를 준비한다. 정상은 state=BOOT, boot\_slot\_num=1 로 고정한다.<br>
<!-- /source-cell:H1649 -->


## 가정 및 제한 조건

<!-- source-cell:I1649 -->
시험 입력 또는 가상 장치 응답은 유닛 입력 경계에 설정한다. 실행 결과를 미리 예상값으로 덮어쓰지 않는다. 실제 전원이나 RF 연결 확인과 소프트웨어 상태 확인을 구분한다. 운영 이미지 대신 격리된 시험 저장소를 사용한다. 초기 RTT 미사용 시 결과를 보존한 뒤 읽는다.<br>
<!-- /source-cell:I1649 -->


## 시험 절차

<!-- source-cell:J1649 -->
- 시험 상태 파일을 읽은 뒤 반환 결과와 선택 슬롯을 보존하여 RTT로 확인한다.<br>
수집: 준비된 BOOT 그룹(ut boot 1) 실행 후 UT-BOOT-019의 조건별 배열/상태와 판정 줄을 찾는다. <br>
<!-- /source-cell:J1649 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1649 -->
판정 범위<br>
- 없음/짧은읽기는 기본 초기화/기록 경로다. 정상은 저장 상태를 사용하여 슬롯 1 을 선택한다. CRC필드 존재만으로 파일CRC 검사를 수행했다고 판정하지 않는다.<br>
<!-- /source-cell:K1649 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1649 -->
검증 결과<br>
상태 파일 없음 또는 길이=0/1/3/63바이트 / 선택 슬롯=255; 저장 state=BOOT(1); 저장 boot\_slot\_num=255; 초기화 기록=1회<br>
정상 64바이트 상태 파일, state=BOOT, boot\_slot\_num=1 / 선택 슬롯=1; 저장 state=BOOT(1); 저장 boot\_slot\_num=1; 추가 쓰기=0회<br>
<!-- /source-cell:L1649 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1650 -->
조건 \[1\]<br>
입력/조건 \[1\]: 상태 파일 없음 또는 길이=0/1/3/63바이트<br>
예상 결과: 선택 슬롯=255; 저장 state=BOOT(1); 저장 boot\_slot\_num=255; 초기화 기록=1회<br>
<!-- /source-cell:K1650 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1650 -->
검증 \[1\]<br>
상태 파일 없음 또는 길이=0/1/3/63바이트 / 선택 슬롯=255; 저장 state=BOOT(1); 저장 boot\_slot\_num=255; 초기화 기록=1회<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1650 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1651 -->
조건 \[2\]<br>
입력/조건 \[2\]: 정상 64바이트 상태 파일, state=BOOT, boot\_slot\_num=1<br>
예상 결과: 선택 슬롯=1; 저장 state=BOOT(1); 저장 boot\_slot\_num=1; 추가 쓰기=0회<br>
<!-- /source-cell:K1651 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1651 -->
검증 \[2\]<br>
정상 64바이트 상태 파일, state=BOOT, boot\_slot\_num=1 / 선택 슬롯=1; 저장 state=BOOT(1); 저장 boot\_slot\_num=1; 추가 쓰기=0회<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1651 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1652 -->
조건 \[3\]<br>
입력/조건 \[3\]: 정상 상태의 crc32=0xDEADBEEF<br>
예상 결과: 저장 crc32=0xDEADBEEF; 파일 CRC 검사=수행 없음<br>
<!-- /source-cell:K1652 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1652 -->
검증 \[3\]<br>
정상 상태의 crc32=0xDEADBEEF / 저장 crc32=0xDEADBEEF; 파일 CRC 검사=수행 없음<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1652 -->


### 비교 5

#### 입력 조건과 통과 기준

<!-- source-cell:K1653 -->
검증 \[1\]의 입력/원소별 기준 (C1): PC 상태 파일 없음<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 새 임시 루트에서 /BOOT\_STATUS.TXT가 없도록 시작하고 snd\_boot\_handle\_file 호출 뒤 저장 파일을 다시 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 255<br>
payload 인덱스: 1<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 2<br>
의미/단위: 저장 boot\_slot\_num<br>
사전 예상값: 255<br>
<!-- /source-cell:K1653 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1653 -->
검증 \[1\]: 상태 파일 없음<br>
출력\[0\] 선택 슬롯 = 255<br>
출력\[1\] 저장 state = 1<br>
출력\[2\] 저장 boot\_slot\_num = 255<br>
<!-- /source-cell:L1653 -->


### 비교 6

#### 입력 조건과 통과 기준

<!-- source-cell:K1654 -->
검증 \[2\]의 입력/원소별 기준 (C2): PC 3바이트 불완전 상태<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C1의 상태 구조체 첫 3바이트만 파일에 저장하고 처리 후 다시 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: boot\_slot\_num<br>
사전 예상값: 255<br>
<!-- /source-cell:K1654 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1654 -->
검증 \[2\]: 3바이트 불완전 상태<br>
출력\[0\] boot\_slot\_num = 255<br>
<!-- /source-cell:L1654 -->


### 비교 7

#### 입력 조건과 통과 기준

<!-- source-cell:K1655 -->
검증 \[3\]의 입력/원소별 기준 (C3): PC 64바이트 정상 읽기<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C2 기본 상태의 state=1, boot\_slot\_num=1, crc32=0xDEADBEEF로 바꾸고 64바이트를 저장한 뒤 처리한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
<!-- /source-cell:K1655 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1655 -->
검증 \[3\]: 64바이트 정상 읽기<br>
출력\[0\] 선택 슬롯 = 1<br>
<!-- /source-cell:L1655 -->


### 비교 8

#### 입력 조건과 통과 기준

<!-- source-cell:K1656 -->
검증 \[4\]의 입력/원소별 기준 (C4): PC CRC 필드 보존 관찰<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C3 처리 후 파일을 다시 읽고 crc32를 확인한다.<br>
payload 인덱스: 0<br>
의미/단위: crc32 (uint32 bit pattern)<br>
사전 예상값: -559038737 (0xDEADBEEF)<br>
비교 통과는 필드 보존의 특성화다. CRC 무결성 검사가 통과했다는 뜻이 아니다.<br>
<!-- /source-cell:K1656 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1656 -->
검증 \[4\]: CRC 필드 보존 관찰<br>
출력\[0\] crc32 (uint32 bit pattern) = -559038737 (0xDEADBEEF)<br>
<!-- /source-cell:L1656 -->


### 비교 9

#### 입력 조건과 통과 기준

<!-- source-cell:K1657 -->
검증 \[101\]의 입력/원소별 기준 (C101): RAM 파일 없음<br>
초기화/입력/동작: 매 건 RAM 파일 상태를 새로 0 초기화한 뒤 exists=0, size=0, bytes\[0..63\]=0으로 설정하고 snd\_boot\_handle\_file을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 255<br>
payload 인덱스: 1<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 2<br>
의미/단위: 저장 boot\_slot\_num<br>
사전 예상값: 255<br>
<!-- /source-cell:K1657 -->


<!-- source-cell:K1658 -->
payload 인덱스: 3<br>
의미/단위: RAM write 횟수 (회)<br>
사전 예상값: 1<br>
payload 인덱스: 4<br>
의미/단위: 논리 파일 크기 (byte)<br>
사전 예상값: 4096 (0x00001000)<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1658 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1657 -->
검증 \[101\]: RAM 파일 없음<br>
출력\[0\] 선택 슬롯 = 255<br>
출력\[1\] 저장 state = 1<br>
출력\[2\] 저장 boot\_slot\_num = 255<br>
출력\[3\] RAM write 횟수 (회) = 1<br>
출력\[4\] 논리 파일 크기 (byte) = 4096 (0x00001000)<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1657 -->


### 비교 10

#### 입력 조건과 통과 기준

<!-- source-cell:K1659 -->
검증 \[102\]의 입력/원소별 기준 (C102): RAM 3바이트 읽기<br>
초기화/입력/동작: 매 건 RAM 파일 상태를 새로 0 초기화한 뒤 exists=1, size=3, bytes\[0..63\]=0으로 설정하고 snd\_boot\_handle\_file을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 255<br>
payload 인덱스: 1<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 2<br>
의미/단위: 저장 boot\_slot\_num<br>
사전 예상값: 255<br>
<!-- /source-cell:K1659 -->


<!-- source-cell:K1660 -->
payload 인덱스: 3<br>
의미/단위: RAM write 횟수 (회)<br>
사전 예상값: 1<br>
payload 인덱스: 4<br>
의미/단위: 논리 파일 크기 (byte)<br>
사전 예상값: 4096 (0x00001000)<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1660 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1659 -->
검증 \[102\]: RAM 3바이트 읽기<br>
출력\[0\] 선택 슬롯 = 255<br>
출력\[1\] 저장 state = 1<br>
출력\[2\] 저장 boot\_slot\_num = 255<br>
출력\[3\] RAM write 횟수 (회) = 1<br>
출력\[4\] 논리 파일 크기 (byte) = 4096 (0x00001000)<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1659 -->


### 비교 11

#### 입력 조건과 통과 기준

<!-- source-cell:K1661 -->
검증 \[103\]의 입력/원소별 기준 (C103): RAM 읽기 오류<br>
초기화/입력/동작: 매 건 RAM 파일 상태를 새로 0 초기화한 뒤 exists=1, size=64, state=1/boot\_slot\_num=1/crc32=0xDEADBEEF 이외 0인 구조체, read\_fault\_once=1으로 설정하고 snd\_boot\_handle\_file을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 255<br>
payload 인덱스: 1<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 2<br>
의미/단위: 저장 boot\_slot\_num<br>
사전 예상값: 255<br>
<!-- /source-cell:K1661 -->


<!-- source-cell:K1662 -->
payload 인덱스: 3<br>
의미/단위: RAM write 횟수 (회)<br>
사전 예상값: 1<br>
payload 인덱스: 4<br>
의미/단위: 논리 파일 크기 (byte)<br>
사전 예상값: 4096 (0x00001000)<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1662 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1661 -->
검증 \[103\]: RAM 읽기 오류<br>
출력\[0\] 선택 슬롯 = 255<br>
출력\[1\] 저장 state = 1<br>
출력\[2\] 저장 boot\_slot\_num = 255<br>
출력\[3\] RAM write 횟수 (회) = 1<br>
출력\[4\] 논리 파일 크기 (byte) = 4096 (0x00001000)<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1661 -->


### 비교 12

#### 입력 조건과 통과 기준

<!-- source-cell:K1663 -->
검증 \[104\]의 입력/원소별 기준 (C104): RAM 유효 상태 유지<br>
초기화/입력/동작: 매 건 RAM 파일 상태를 새로 0 초기화한 뒤 C103과 같은 64바이트 입력, read\_fault\_once=0으로 설정하고 snd\_boot\_handle\_file을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 2<br>
의미/단위: 저장 boot\_slot\_num<br>
사전 예상값: 1<br>
<!-- /source-cell:K1663 -->


<!-- source-cell:K1664 -->
payload 인덱스: 3<br>
의미/단위: RAM write 횟수 (회)<br>
사전 예상값: 0<br>
payload 인덱스: 4<br>
의미/단위: 논리 파일 크기 (byte)<br>
사전 예상값: 64<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1664 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1663 -->
검증 \[104\]: RAM 유효 상태 유지<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 state = 1<br>
출력\[2\] 저장 boot\_slot\_num = 1<br>
출력\[3\] RAM write 횟수 (회) = 0<br>
출력\[4\] 논리 파일 크기 (byte) = 64<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1663 -->


### 비교 13

#### 입력 조건과 통과 기준

<!-- source-cell:K1665 -->
검증 \[105\]의 입력/원소별 기준 (C105): RAM CRC 필드 특성<br>
초기화/입력/동작: C104 직후 result.crc32를 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: crc32 (uint32 bit pattern)<br>
사전 예상값: -559038737 (0xDEADBEEF)<br>
CRC 계산이나 유효 판정이 없다. 저장 필드 보존만 확인한다.<br>
<!-- /source-cell:K1665 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1665 -->
검증 \[105\]: RAM CRC 필드 특성<br>
출력\[0\] crc32 (uint32 bit pattern) = -559038737 (0xDEADBEEF)<br>
<!-- /source-cell:L1665 -->


### 비교 14

#### 입력 조건과 통과 기준

<!-- source-cell:K1666 -->
검증 \[106\]의 입력/원소별 기준 (C106): RAM 파일 크기 0바이트와 반복<br>
초기화/입력/동작: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=0로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 최종 선택 슬롯<br>
사전 예상값: 255<br>
payload 인덱스: 1<br>
의미/단위: 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1666 -->


<!-- source-cell:K1667 -->
payload 인덱스: 2<br>
의미/단위: 둘째 호출 추가 write 없음 (0=추가,1=없음)<br>
사전 예상값: 1<br>
payload 인덱스: 3<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
길이가 64 미만이면 문서의 기본 상태 64바이트, 64 이상이면 입력 상태 64바이트를 유지해야 한다. 4096은 논리 길이이며 RAM 자료 배열은 64바이트다.<br>
<!-- /source-cell:K1667 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1666 -->
검증 \[106\]: RAM 파일 크기 0바이트와 반복<br>
출력\[0\] 최종 선택 슬롯 = 255<br>
출력\[1\] 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음) = 1<br>
출력\[2\] 둘째 호출 추가 write 없음 (0=추가,1=없음) = 1<br>
출력\[3\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1666 -->


### 비교 15

#### 입력 조건과 통과 기준

<!-- source-cell:K1668 -->
검증 \[107\]의 입력/원소별 기준 (C107): RAM 파일 크기 1바이트와 반복<br>
초기화/입력/동작: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=1로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 최종 선택 슬롯<br>
사전 예상값: 255<br>
payload 인덱스: 1<br>
의미/단위: 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1668 -->


<!-- source-cell:K1669 -->
payload 인덱스: 2<br>
의미/단위: 둘째 호출 추가 write 없음 (0=추가,1=없음)<br>
사전 예상값: 1<br>
payload 인덱스: 3<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
길이가 64 미만이면 문서의 기본 상태 64바이트, 64 이상이면 입력 상태 64바이트를 유지해야 한다. 4096은 논리 길이이며 RAM 자료 배열은 64바이트다.<br>
<!-- /source-cell:K1669 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1668 -->
검증 \[107\]: RAM 파일 크기 1바이트와 반복<br>
출력\[0\] 최종 선택 슬롯 = 255<br>
출력\[1\] 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음) = 1<br>
출력\[2\] 둘째 호출 추가 write 없음 (0=추가,1=없음) = 1<br>
출력\[3\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1668 -->


### 비교 16

#### 입력 조건과 통과 기준

<!-- source-cell:K1670 -->
검증 \[108\]의 입력/원소별 기준 (C108): RAM 파일 크기 63바이트와 반복<br>
초기화/입력/동작: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=63로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 최종 선택 슬롯<br>
사전 예상값: 255<br>
payload 인덱스: 1<br>
의미/단위: 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1670 -->


<!-- source-cell:K1671 -->
payload 인덱스: 2<br>
의미/단위: 둘째 호출 추가 write 없음 (0=추가,1=없음)<br>
사전 예상값: 1<br>
payload 인덱스: 3<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
길이가 64 미만이면 문서의 기본 상태 64바이트, 64 이상이면 입력 상태 64바이트를 유지해야 한다. 4096은 논리 길이이며 RAM 자료 배열은 64바이트다.<br>
<!-- /source-cell:K1671 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1670 -->
검증 \[108\]: RAM 파일 크기 63바이트와 반복<br>
출력\[0\] 최종 선택 슬롯 = 255<br>
출력\[1\] 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음) = 1<br>
출력\[2\] 둘째 호출 추가 write 없음 (0=추가,1=없음) = 1<br>
출력\[3\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1670 -->


### 비교 17

#### 입력 조건과 통과 기준

<!-- source-cell:K1672 -->
검증 \[109\]의 입력/원소별 기준 (C109): RAM 파일 크기 64바이트와 반복<br>
초기화/입력/동작: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=64로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 최종 선택 슬롯<br>
사전 예상값: 4<br>
payload 인덱스: 1<br>
의미/단위: 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1672 -->


<!-- source-cell:K1673 -->
payload 인덱스: 2<br>
의미/단위: 둘째 호출 추가 write 없음 (0=추가,1=없음)<br>
사전 예상값: 1<br>
payload 인덱스: 3<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
길이가 64 미만이면 문서의 기본 상태 64바이트, 64 이상이면 입력 상태 64바이트를 유지해야 한다. 4096은 논리 길이이며 RAM 자료 배열은 64바이트다.<br>
<!-- /source-cell:K1673 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1672 -->
검증 \[109\]: RAM 파일 크기 64바이트와 반복<br>
출력\[0\] 최종 선택 슬롯 = 4<br>
출력\[1\] 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음) = 1<br>
출력\[2\] 둘째 호출 추가 write 없음 (0=추가,1=없음) = 1<br>
출력\[3\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1672 -->


### 비교 18

#### 입력 조건과 통과 기준

<!-- source-cell:K1674 -->
검증 \[110\]의 입력/원소별 기준 (C110): RAM 파일 크기 4096바이트와 반복<br>
초기화/입력/동작: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=4096로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 최종 선택 슬롯<br>
사전 예상값: 4<br>
payload 인덱스: 1<br>
의미/단위: 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1674 -->


<!-- source-cell:K1675 -->
payload 인덱스: 2<br>
의미/단위: 둘째 호출 추가 write 없음 (0=추가,1=없음)<br>
사전 예상값: 1<br>
payload 인덱스: 3<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
길이가 64 미만이면 문서의 기본 상태 64바이트, 64 이상이면 입력 상태 64바이트를 유지해야 한다. 4096은 논리 길이이며 RAM 자료 배열은 64바이트다.<br>
<!-- /source-cell:K1675 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1674 -->
검증 \[110\]: RAM 파일 크기 4096바이트와 반복<br>
출력\[0\] 최종 선택 슬롯 = 4<br>
출력\[1\] 첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음) = 1<br>
출력\[2\] 둘째 호출 추가 write 없음 (0=추가,1=없음) = 1<br>
출력\[3\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1674 -->


### 비교 19

#### 입력 조건과 통과 기준

<!-- source-cell:K1676 -->
검증 \[101\]의 RTT 출력 (C101)<br>
입력/조건: 매 건 RAM 파일 상태를 새로 0 초기화한 뒤 exists=0, size=0, bytes\[0..63\]=0으로 설정하고 snd\_boot\_handle\_file을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C101 A[0:5]=255,1,255,1,4096,1
[UT-BOOT-019] C101 PASS N=6 F=0
```
<!-- /source-cell:K1676 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1676 -->

```text
[UT-BOOT-019] C101 A[0:5]=255,1,255,1,4096,1
[UT-BOOT-019] C101 PASS N=6 F=0
```
<!-- /source-cell:L1676 -->


### 비교 20

#### 입력 조건과 통과 기준

<!-- source-cell:K1677 -->
검증 \[102\]의 RTT 출력 (C102)<br>
입력/조건: 매 건 RAM 파일 상태를 새로 0 초기화한 뒤 exists=1, size=3, bytes\[0..63\]=0으로 설정하고 snd\_boot\_handle\_file을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C102 A[0:5]=255,1,255,1,4096,1
[UT-BOOT-019] C102 PASS N=6 F=0
```
<!-- /source-cell:K1677 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1677 -->

```text
[UT-BOOT-019] C102 A[0:5]=255,1,255,1,4096,1
[UT-BOOT-019] C102 PASS N=6 F=0
```
<!-- /source-cell:L1677 -->


### 비교 21

#### 입력 조건과 통과 기준

<!-- source-cell:K1678 -->
검증 \[103\]의 RTT 출력 (C103)<br>
입력/조건: 매 건 RAM 파일 상태를 새로 0 초기화한 뒤 exists=1, size=64, state=1/boot\_slot\_num=1/crc32=0xDEADBEEF 이외 0인 구조체, read\_fault\_once=1으로 설정하고 snd\_boot\_handle\_file을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C103 A[0:5]=255,1,255,1,4096,1
[UT-BOOT-019] C103 PASS N=6 F=0
```
<!-- /source-cell:K1678 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1678 -->

```text
[UT-BOOT-019] C103 A[0:5]=255,1,255,1,4096,1
[UT-BOOT-019] C103 PASS N=6 F=0
```
<!-- /source-cell:L1678 -->


### 비교 22

#### 입력 조건과 통과 기준

<!-- source-cell:K1679 -->
검증 \[104\]의 RTT 출력 (C104)<br>
입력/조건: 매 건 RAM 파일 상태를 새로 0 초기화한 뒤 C103과 같은 64바이트 입력, read\_fault\_once=0으로 설정하고 snd\_boot\_handle\_file을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C104 A[0:5]=1,1,1,0,64,1
[UT-BOOT-019] C104 PASS N=6 F=0
```
<!-- /source-cell:K1679 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1679 -->

```text
[UT-BOOT-019] C104 A[0:5]=1,1,1,0,64,1
[UT-BOOT-019] C104 PASS N=6 F=0
```
<!-- /source-cell:L1679 -->


### 비교 23

#### 입력 조건과 통과 기준

<!-- source-cell:K1680 -->
검증 \[105\]의 RTT 출력 (C105)<br>
입력/조건: C104 직후 result.crc32를 읽는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C105 A[0:0]=-559038737
[UT-BOOT-019] C105 PASS N=1 F=0
```
<!-- /source-cell:K1680 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1680 -->

```text
[UT-BOOT-019] C105 A[0:0]=-559038737
[UT-BOOT-019] C105 PASS N=1 F=0
```
<!-- /source-cell:L1680 -->


### 비교 24

#### 입력 조건과 통과 기준

<!-- source-cell:K1681 -->
검증 \[106\]의 RTT 출력 (C106)<br>
입력/조건: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=0로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C106 A[0:3]=255,1,1,1
[UT-BOOT-019] C106 PASS N=4 F=0
```
<!-- /source-cell:K1681 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1681 -->

```text
[UT-BOOT-019] C106 A[0:3]=255,1,1,1
[UT-BOOT-019] C106 PASS N=4 F=0
```
<!-- /source-cell:L1681 -->


### 비교 25

#### 입력 조건과 통과 기준

<!-- source-cell:K1682 -->
검증 \[107\]의 RTT 출력 (C107)<br>
입력/조건: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=1로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C107 A[0:3]=255,1,1,1
[UT-BOOT-019] C107 PASS N=4 F=0
```
<!-- /source-cell:K1682 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1682 -->

```text
[UT-BOOT-019] C107 A[0:3]=255,1,1,1
[UT-BOOT-019] C107 PASS N=4 F=0
```
<!-- /source-cell:L1682 -->


### 비교 26

#### 입력 조건과 통과 기준

<!-- source-cell:K1683 -->
검증 \[108\]의 RTT 출력 (C108)<br>
입력/조건: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=63로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C108 A[0:3]=255,1,1,1
[UT-BOOT-019] C108 PASS N=4 F=0
```
<!-- /source-cell:K1683 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1683 -->

```text
[UT-BOOT-019] C108 A[0:3]=255,1,1,1
[UT-BOOT-019] C108 PASS N=4 F=0
```
<!-- /source-cell:L1683 -->


### 비교 27

#### 입력 조건과 통과 기준

<!-- source-cell:K1684 -->
검증 \[109\]의 RTT 출력 (C109)<br>
입력/조건: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=64로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C109 A[0:3]=4,1,1,1
[UT-BOOT-019] C109 PASS N=4 F=0
```
<!-- /source-cell:K1684 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1684 -->

```text
[UT-BOOT-019] C109 A[0:3]=4,1,1,1
[UT-BOOT-019] C109 PASS N=4 F=0
```
<!-- /source-cell:L1684 -->


### 비교 28

#### 입력 조건과 통과 기준

<!-- source-cell:K1685 -->
검증 \[110\]의 RTT 출력 (C110)<br>
입력/조건: RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot\_slot\_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size=4096로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-019] C110 A[0:3]=4,1,1,1
[UT-BOOT-019] C110 PASS N=4 F=0
```
<!-- /source-cell:K1685 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1685 -->

```text
[UT-BOOT-019] C110 A[0:3]=4,1,1,1
[UT-BOOT-019] C110 PASS N=4 F=0
```
<!-- /source-cell:L1685 -->


## 보고서 판정

<!-- source-cell:M1649 -->
통과<br>
<!-- /source-cell:M1649 -->
