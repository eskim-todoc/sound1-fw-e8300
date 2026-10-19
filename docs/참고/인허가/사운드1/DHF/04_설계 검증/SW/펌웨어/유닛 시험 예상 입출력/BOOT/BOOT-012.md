# BOOT-012 매니페스트 처리

시험 ID: `SW-FW-UnitTest-BOOT-ID-12`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1497~1533행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1497 -->
여러 실행 이미지를 정해진 목록에 따라 함께 적재한다.<br>
<!-- /source-cell:E1497 -->


## 시험 방법

<!-- source-cell:F1497 -->
D<br>
<!-- /source-cell:F1497 -->


## 상세 설계 유닛

<!-- source-cell:G1497 -->
SU-ID-BOOT-012<br>
(매니페스트 처리)<br>
<!-- /source-cell:G1497 -->


## 시험 준비와 입력

<!-- source-cell:H1497 -->
- 정상 목록에 시험파일A/B 두 항목을 넣는다. 별도로 목록 읽기 실패/파일 열기 실패를 준비한다.<br>
<!-- /source-cell:H1497 -->


## 가정 및 제한 조건

<!-- source-cell:I1497 -->
시험 입력 또는 가상 장치 응답은 유닛 입력 경계에 설정한다. 실행 결과를 미리 예상값으로 덮어쓰지 않는다. 실제 전원이나 RF 연결 확인과 소프트웨어 상태 확인을 구분한다. 운영 이미지 대신 격리된 시험 저장소를 사용한다. 초기 RTT 미사용 시 결과를 보존한 뒤 읽는다.<br>
<!-- /source-cell:I1497 -->


## 시험 절차

<!-- source-cell:J1497 -->
- 준비한 조건을 각각 독립 초기화하여 해당 유닛을 실행한다. 상태 변화, 반환 결과와 처리 순서를 처리 완료 후 RTT로 기록하여 아래 예상 결과와 비교한다.<br>
수집: 준비된 BOOT 그룹(ut boot 1) 실행 후 UT-BOOT-012의 조건별 배열/상태와 판정 줄을 찾는다. <br>
<!-- /source-cell:J1497 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1497 -->
판정 범위<br>
- A/B 각 항목을 목록 순서대로 한 번씩 적재한다. 실패는 오류로 반환하며 정상 완료와 혼동하지 않는다.<br>
<!-- /source-cell:K1497 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1497 -->
검증 결과<br>
목록에서 A.FEZ와 B.FEZ를 각각 1회 분리했다. 각 이름 길이=5, 끝 문자=0이었다. A.FEZ의 첫 문자=65, B.FEZ의 첫 문자=66이었다. 파일 읽기 오류는 반환값=-1로 전달됐다.<br>
<!-- /source-cell:L1497 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1498 -->
조건 \[1\]<br>
입력/조건 \[1\]: 목록=A.FEZ\\nB.FEZ\\n, 길이=12바이트, CRC=0x00FA<br>
예상 결과: 적재 순서=A.FEZ → B.FEZ; 적재 횟수=각 1회<br>
<!-- /source-cell:K1498 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1498 -->
검증 \[1\]<br>
입력: 목록=A.FEZ\\nB.FEZ\\n, 길이=12바이트, CRC=0x00FA<br>
출력: 적재 순서=A.FEZ → B.FEZ; 적재 횟수=각 1회<br>
<!-- /source-cell:L1498 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1499 -->
조건 \[2\]<br>
입력/조건 \[2\]: 목록 첫 읽기 오류<br>
예상 결과: 반환값=-1; 결과 구분=오류 반환<br>
<!-- /source-cell:K1499 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1499 -->
검증 \[2\]<br>
입력: 목록 첫 읽기 오류<br>
출력: 반환값=-1; 결과 구분=오류 반환<br>
<!-- /source-cell:L1499 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1500 -->
조건 \[3\]<br>
입력/조건 \[3\]: 목록의 대상 파일 열기 실패<br>
예상 결과: 반환값=-1; 결과 구분=오류 반환<br>
<!-- /source-cell:K1500 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1500 -->
검증 \[3\]<br>
입력: 목록의 대상 파일 열기 실패<br>
출력: 반환값=-1; 결과 구분=오류 반환<br>
<!-- /source-cell:L1500 -->


### 비교 5

#### 입력 조건과 통과 기준

<!-- source-cell:K1501 -->
조건 \[4\]<br>
입력/조건 \[4\]: 목록 CRC 오류/중첩 매니페스트/슬롯 디렉터리 오류<br>
예상 결과: 반환값=-1; app handle=닫힘<br>
<!-- /source-cell:K1501 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1501 -->
검증 \[4\]<br>
입력: 목록 CRC 오류/중첩 매니페스트/슬롯 디렉터리 오류<br>
출력: 반환값=-1; app handle=닫힘<br>
<!-- /source-cell:L1501 -->


### 비교 6

#### 입력 조건과 통과 기준

<!-- source-cell:K1502 -->
조건 \[5\]<br>
입력/조건 \[5\]: C106: A/B 처리 후 닫힌 매니페스트를 다시 읽어 오류 발생<br>
예상 결과: 오류 처리=오류 반환; 정상 완료 처리=없음<br>
<!-- /source-cell:K1502 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1502 -->
검증 \[5\]<br>
입력: C106: A/B 처리 후 닫힌 매니페스트를 다시 읽어 오류 발생<br>
출력: 오류 처리=오류 반환; 정상 완료 처리=없음<br>
<!-- /source-cell:L1502 -->


### 비교 7

#### 입력 조건과 통과 기준

<!-- source-cell:K1503 -->
검증 \[1\]의 입력/원소별 기준 (C1): 두 파일 목록<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: A.FEZ와 B.FEZ는 각각 정상 무섹션 헤더 8바이트, 목록은 A.FEZ\\nB.FEZ\\n의 12바이트에서 bootloader\_process\_manifest를 호출한다. 오류 후 남은 열린 목록은 호출자가 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1503 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1503 -->
검증 \[1\]: 두 파일 목록<br>
출력\[0\] 함수 반환값 (정수) = 0<br>
<!-- /source-cell:L1503 -->


### 비교 8

#### 입력 조건과 통과 기준

<!-- source-cell:K1504 -->
검증 \[2\]의 입력/원소별 기준 (C2): PC 대상 파일 순서<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C1 직후 연 파일 횟수와 순서를 확인한다.<br>
payload 인덱스: 0<br>
의미/단위: 연 app 파일 수 (개)<br>
사전 예상값: 2<br>
payload 인덱스: 1<br>
의미/단위: 첫 파일 ID (1=A.FEZ)<br>
사전 예상값: 1<br>
payload 인덱스: 2<br>
의미/단위: 둘째 파일 ID (2=B.FEZ)<br>
사전 예상값: 2<br>
<!-- /source-cell:K1504 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1504 -->
검증 \[2\]: 대상 파일 순서<br>
출력\[0\] 연 app 파일 수 (개) = 2<br>
출력\[1\] 첫 파일 ID (1=A.FEZ) = 1<br>
출력\[2\] 둘째 파일 ID (2=B.FEZ) = 2<br>
<!-- /source-cell:L1504 -->


### 비교 9

#### 입력 조건과 통과 기준

<!-- source-cell:K1505 -->
검증 \[3\]의 입력/원소별 기준 (C3): 목록 첫 읽기 오류<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 정상 목록을 다시 열고 s\_tdc\_read\_error=1로 모든 파일 읽기에 FR\_DISK\_ERR(1) 주입에서 bootloader\_process\_manifest를 호출한다. 오류 후 남은 열린 목록은 호출자가 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1505 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1505 -->
검증 \[3\]: 목록 첫 읽기 오류<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1505 -->


### 비교 10

#### 입력 조건과 통과 기준

<!-- source-cell:K1506 -->
검증 \[4\]의 입력/원소별 기준 (C4): 목록의 대상 파일 없음<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 목록을 missing.FEZ\\n의 12바이트로 바꾸고 CRC 재계산에서 bootloader\_process\_manifest를 호출한다. 오류 후 남은 열린 목록은 호출자가 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1506 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1506 -->
검증 \[4\]: 목록의 대상 파일 없음<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1506 -->


### 비교 11

#### 입력 조건과 통과 기준

<!-- source-cell:K1507 -->
검증 \[5\]의 입력/원소별 기준 (C5): 목록 CRC 불일치<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C4 목록과 파일을 유지하고 CRC 목록\[0\] XOR 1에서 bootloader\_process\_manifest를 호출한다. 오류 후 남은 열린 목록은 호출자가 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1507 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1507 -->
검증 \[5\]: 목록 CRC 불일치<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1507 -->


### 비교 12

#### 입력 조건과 통과 기준

<!-- source-cell:K1508 -->
검증 \[6\]의 입력/원소별 기준 (C6): NULL 목록 헤더<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: file\_desc=NULL, app\_header=NULL에서 bootloader\_process\_manifest를 호출한다. 오류 후 남은 열린 목록은 호출자가 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1508 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1508 -->
검증 \[6\]: NULL 목록 헤더<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1508 -->


### 비교 13

#### 입력 조건과 통과 기준

<!-- source-cell:K1509 -->
검증 \[101\]의 입력/원소별 기준 (C101): A.FEZ 항목 분리<br>
초기화/입력/동작: 목록 byte\[0\]를 시작으로 get\_manifest\_files(src,name,16)을 호출한다. name은 내부에서 16바이트 모두 0으로 초기화된다.<br>
payload 인덱스: 0<br>
의미/단위: 복사한 길이 (문자)<br>
사전 예상값: 5<br>
payload 인덱스: 1<br>
의미/단위: name\[0\] ASCII<br>
사전 예상값: 65<br>
<!-- /source-cell:K1509 -->


<!-- source-cell:K1510 -->
payload 인덱스: 2<br>
의미/단위: name\[5\] NUL<br>
사전 예상값: 0<br>
name\[0..5\]="A.FEZ\\0", name\[6..15\]=0. payload는 길이/첫 글자/NUL만 비교한다.<br>
<!-- /source-cell:K1510 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1509 -->
검증 \[101\]: A.FEZ 항목 분리<br>
출력\[0\] 복사한 길이 (문자) = 5<br>
출력\[1\] name\[0\] ASCII = 65<br>
출력\[2\] name\[5\] NUL = 0<br>
<!-- /source-cell:L1509 -->


### 비교 14

#### 입력 조건과 통과 기준

<!-- source-cell:K1511 -->
검증 \[102\]의 입력/원소별 기준 (C102): B.FEZ 항목 분리<br>
초기화/입력/동작: 목록 byte\[6\]를 시작으로 get\_manifest\_files(src,name,16)을 호출한다. name은 내부에서 16바이트 모두 0으로 초기화된다.<br>
payload 인덱스: 0<br>
의미/단위: 복사한 길이 (문자)<br>
사전 예상값: 5<br>
payload 인덱스: 1<br>
의미/단위: name\[0\] ASCII<br>
사전 예상값: 66<br>
<!-- /source-cell:K1511 -->


<!-- source-cell:K1512 -->
payload 인덱스: 2<br>
의미/단위: name\[5\] NUL<br>
사전 예상값: 0<br>
name\[0..5\]="B.FEZ\\0", name\[6..15\]=0. payload는 길이/첫 글자/NUL만 비교한다.<br>
<!-- /source-cell:K1512 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1511 -->
검증 \[102\]: B.FEZ 항목 분리<br>
출력\[0\] 복사한 길이 (문자) = 5<br>
출력\[1\] name\[0\] ASCII = 66<br>
출력\[2\] name\[5\] NUL = 0<br>
<!-- /source-cell:L1511 -->


### 비교 15

#### 입력 조건과 통과 기준

<!-- source-cell:K1513 -->
검증 \[103\]의 입력/원소별 기준 (C103): NULL 목록<br>
초기화/입력/동작: src=NULL, name은 유효, len=16으로 get\_manifest\_files를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1513 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1513 -->
검증 \[103\]: NULL 목록<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1513 -->


### 비교 16

#### 입력 조건과 통과 기준

<!-- source-cell:K1514 -->
검증 \[104\]의 입력/원소별 기준 (C104): NULL 이름 출력<br>
초기화/입력/동작: src는 유효 목록, dst=NULL, len=16으로 get\_manifest\_files를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1514 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1514 -->
검증 \[104\]: NULL 이름 출력<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1514 -->


### 비교 17

#### 입력 조건과 통과 기준

<!-- source-cell:K1515 -->
검증 \[105\]의 입력/원소별 기준 (C105): 0바이트 출력 용량<br>
초기화/입력/동작: src/dst는 유효, len=0으로 get\_manifest\_files를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1515 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1515 -->
검증 \[105\]: 0바이트 출력 용량<br>
출력\[0\] 함수 반환값 (정수) = 0<br>
<!-- /source-cell:L1515 -->


### 비교 18

#### 입력 조건과 통과 기준

<!-- source-cell:K1516 -->
검증 \[106\]의 입력/원소별 기준 (C106): RAM 목록을 한 번씩 순서대로 처리<br>
초기화/입력/동작: RAM 목록 A.FEZ\\nB.FEZ\\n, 정상 CRC, ident=0x657A616D, crc\_table\_size=1을 준비한다. open/read/close/chdir를 RAM 콜백으로 연결하고 process\_manifest를 호출한다. 두 대상은 무섹션 app이다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: 0<br>
payload 인덱스: 1<br>
의미/단위: 대상 open 성공 수<br>
사전 예상값: 2<br>
payload 인덱스: 2<br>
의미/단위: order\[0\] (1=A)<br>
사전 예상값: 1<br>
payload 인덱스: 3<br>
<!-- /source-cell:K1516 -->


<!-- source-cell:K1517 -->
의미/단위: order\[1\] (2=B)<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 두 handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
payload 인덱스: 5<br>
의미/단위: 닫힌 목록 재읽기 횟수 (회)<br>
사전 예상값: 1<br>
마지막 값 1은 현재 구현이 닫은 목록을 한 번 다시 읽는 특성이다. 다중 블록 처리나 파일 수명 적합성의 PASS 근거가 아니다.<br>
<!-- /source-cell:K1517 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1516 -->
검증 \[106\]: RAM 목록을 한 번씩 순서대로 처리<br>
출력\[0\] 반환값 = 0<br>
출력\[1\] 대상 open 성공 수 = 2<br>
출력\[2\] order\[0\] (1=A) = 1<br>
출력\[3\] order\[1\] (2=B) = 2<br>
출력\[4\] 두 handle 닫힘 (0=열림,1=닫힘) = 1<br>
출력\[5\] 닫힌 목록 재읽기 횟수 (회) = 1<br>
<!-- /source-cell:L1516 -->


### 비교 19

#### 입력 조건과 통과 기준

<!-- source-cell:K1518 -->
검증 \[107\]의 입력/원소별 기준 (C107): RAM 목록 첫 읽기 실패<br>
초기화/입력/동작: C106과 같은 새 RAM 상태로 시작하되 read\_error=1, FR\_DISK\_ERR(1) 반환을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: app handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1518 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1518 -->
검증 \[107\]: RAM 목록 첫 읽기 실패<br>
출력\[0\] 반환값 = -1<br>
출력\[1\] app handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1518 -->


### 비교 20

#### 입력 조건과 통과 기준

<!-- source-cell:K1519 -->
검증 \[108\]의 입력/원소별 기준 (C108): RAM B 파일 없음<br>
초기화/입력/동작: C106과 같은 새 RAM 상태로 시작하되 missing\_b=1, B.FEZ open에서 FR\_NO\_FILE(4)을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: app handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1519 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1519 -->
검증 \[108\]: RAM B 파일 없음<br>
출력\[0\] 반환값 = -1<br>
출력\[1\] app handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1519 -->


### 비교 21

#### 입력 조건과 통과 기준

<!-- source-cell:K1520 -->
검증 \[109\]의 입력/원소별 기준 (C109): RAM 목록 CRC 오류<br>
초기화/입력/동작: C106과 같은 새 RAM 상태로 시작하되 정상 목록 CRC 항목 XOR 1을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: app handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1520 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1520 -->
검증 \[109\]: RAM 목록 CRC 오류<br>
출력\[0\] 반환값 = -1<br>
출력\[1\] app handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1520 -->


### 비교 22

#### 입력 조건과 통과 기준

<!-- source-cell:K1521 -->
검증 \[110\]의 입력/원소별 기준 (C110): 중첩 매니페스트 거부<br>
초기화/입력/동작: C106과 같은 새 RAM 상태로 시작하되 nested=1, 열린 대상 헤더 ident=0x657A616D이고 CRC 정상을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: app handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1521 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1521 -->
검증 \[110\]: 중첩 매니페스트 거부<br>
출력\[0\] 반환값 = -1<br>
출력\[1\] app handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1521 -->


### 비교 23

#### 입력 조건과 통과 기준

<!-- source-cell:K1522 -->
검증 \[111\]의 입력/원소별 기준 (C111): RAM 슬롯 경로 실패<br>
초기화/입력/동작: C106과 같은 새 RAM 상태로 시작하되 chdir\_error=1로 FR\_NO\_PATH(5) 반환을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: app handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1522 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1522 -->
검증 \[111\]: RAM 슬롯 경로 실패<br>
출력\[0\] 반환값 = -1<br>
출력\[1\] app handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1522 -->


### 비교 24

#### 입력 조건과 통과 기준

<!-- source-cell:K1523 -->
검증 \[101\]의 RTT 출력 (C101)<br>
입력/조건: 목록 byte\[0\]를 시작으로 get\_manifest\_files(src,name,16)을 호출한다. name은 내부에서 16바이트 모두 0으로 초기화된다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C101 A[0:2]=5,65,0
[UT-BOOT-012] C101 PASS N=3 F=0
```
<!-- /source-cell:K1523 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1523 -->

```text
[UT-BOOT-012] C101 A[0:2]=5,65,0
[UT-BOOT-012] C101 PASS N=3 F=0
```
<!-- /source-cell:L1523 -->


### 비교 25

#### 입력 조건과 통과 기준

<!-- source-cell:K1524 -->
검증 \[102\]의 RTT 출력 (C102)<br>
입력/조건: 목록 byte\[6\]를 시작으로 get\_manifest\_files(src,name,16)을 호출한다. name은 내부에서 16바이트 모두 0으로 초기화된다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C102 A[0:2]=5,66,0
[UT-BOOT-012] C102 PASS N=3 F=0
```
<!-- /source-cell:K1524 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1524 -->

```text
[UT-BOOT-012] C102 A[0:2]=5,66,0
[UT-BOOT-012] C102 PASS N=3 F=0
```
<!-- /source-cell:L1524 -->


### 비교 26

#### 입력 조건과 통과 기준

<!-- source-cell:K1525 -->
검증 \[103\]의 RTT 출력 (C103)<br>
입력/조건: src=NULL, name은 유효, len=16으로 get\_manifest\_files를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C103 A[0:0]=-1
[UT-BOOT-012] C103 PASS N=1 F=0
```
<!-- /source-cell:K1525 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1525 -->

```text
[UT-BOOT-012] C103 A[0:0]=-1
[UT-BOOT-012] C103 PASS N=1 F=0
```
<!-- /source-cell:L1525 -->


### 비교 27

#### 입력 조건과 통과 기준

<!-- source-cell:K1526 -->
검증 \[104\]의 RTT 출력 (C104)<br>
입력/조건: src는 유효 목록, dst=NULL, len=16으로 get\_manifest\_files를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C104 A[0:0]=-1
[UT-BOOT-012] C104 PASS N=1 F=0
```
<!-- /source-cell:K1526 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1526 -->

```text
[UT-BOOT-012] C104 A[0:0]=-1
[UT-BOOT-012] C104 PASS N=1 F=0
```
<!-- /source-cell:L1526 -->


### 비교 28

#### 입력 조건과 통과 기준

<!-- source-cell:K1527 -->
검증 \[105\]의 RTT 출력 (C105)<br>
입력/조건: src/dst는 유효, len=0으로 get\_manifest\_files를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C105 A[0:0]=0
[UT-BOOT-012] C105 PASS N=1 F=0
```
<!-- /source-cell:K1527 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1527 -->

```text
[UT-BOOT-012] C105 A[0:0]=0
[UT-BOOT-012] C105 PASS N=1 F=0
```
<!-- /source-cell:L1527 -->


### 비교 29

#### 입력 조건과 통과 기준

<!-- source-cell:K1528 -->
검증 \[106\]의 RTT 출력 (C106)<br>
입력/조건: RAM 목록 A.FEZ\\nB.FEZ\\n, 정상 CRC, ident=0x657A616D, crc\_table\_size=1을 준비한다. open/read/close/chdir를 RAM 콜백으로 연결하고 process\_manifest를 호출한다. 두 대상은 무섹션 app이다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C106 A[0:5]=0,2,1,2,1,1
[UT-BOOT-012] C106 PASS N=6 F=0
```
<!-- /source-cell:K1528 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1528 -->

```text
[UT-BOOT-012] C106 A[0:5]=0,2,1,2,1,1
[UT-BOOT-012] C106 PASS N=6 F=0
```
<!-- /source-cell:L1528 -->


### 비교 30

#### 입력 조건과 통과 기준

<!-- source-cell:K1529 -->
검증 \[107\]의 RTT 출력 (C107)<br>
입력/조건: C106과 같은 새 RAM 상태로 시작하되 read\_error=1, FR\_DISK\_ERR(1) 반환을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C107 A[0:1]=-1,1
[UT-BOOT-012] C107 PASS N=2 F=0
```
<!-- /source-cell:K1529 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1529 -->

```text
[UT-BOOT-012] C107 A[0:1]=-1,1
[UT-BOOT-012] C107 PASS N=2 F=0
```
<!-- /source-cell:L1529 -->


### 비교 31

#### 입력 조건과 통과 기준

<!-- source-cell:K1530 -->
검증 \[108\]의 RTT 출력 (C108)<br>
입력/조건: C106과 같은 새 RAM 상태로 시작하되 missing\_b=1, B.FEZ open에서 FR\_NO\_FILE(4)을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C108 A[0:1]=-1,1
[UT-BOOT-012] C108 PASS N=2 F=0
```
<!-- /source-cell:K1530 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1530 -->

```text
[UT-BOOT-012] C108 A[0:1]=-1,1
[UT-BOOT-012] C108 PASS N=2 F=0
```
<!-- /source-cell:L1530 -->


### 비교 32

#### 입력 조건과 통과 기준

<!-- source-cell:K1531 -->
검증 \[109\]의 RTT 출력 (C109)<br>
입력/조건: C106과 같은 새 RAM 상태로 시작하되 정상 목록 CRC 항목 XOR 1을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C109 A[0:1]=-1,1
[UT-BOOT-012] C109 PASS N=2 F=0
```
<!-- /source-cell:K1531 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1531 -->

```text
[UT-BOOT-012] C109 A[0:1]=-1,1
[UT-BOOT-012] C109 PASS N=2 F=0
```
<!-- /source-cell:L1531 -->


### 비교 33

#### 입력 조건과 통과 기준

<!-- source-cell:K1532 -->
검증 \[110\]의 RTT 출력 (C110)<br>
입력/조건: C106과 같은 새 RAM 상태로 시작하되 nested=1, 열린 대상 헤더 ident=0x657A616D이고 CRC 정상을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C110 A[0:1]=-1,1
[UT-BOOT-012] C110 PASS N=2 F=0
```
<!-- /source-cell:K1532 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1532 -->

```text
[UT-BOOT-012] C110 A[0:1]=-1,1
[UT-BOOT-012] C110 PASS N=2 F=0
```
<!-- /source-cell:L1532 -->


### 비교 34

#### 입력 조건과 통과 기준

<!-- source-cell:K1533 -->
검증 \[111\]의 RTT 출력 (C111)<br>
입력/조건: C106과 같은 새 RAM 상태로 시작하되 chdir\_error=1로 FR\_NO\_PATH(5) 반환을 주입한다. process\_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-012] C111 A[0:1]=-1,1
[UT-BOOT-012] C111 PASS N=2 F=0
```
<!-- /source-cell:K1533 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1533 -->

```text
[UT-BOOT-012] C111 A[0:1]=-1,1
[UT-BOOT-012] C111 PASS N=2 F=0
```
<!-- /source-cell:L1533 -->


## 보고서 판정

<!-- source-cell:M1497 -->
통과<br>
<!-- /source-cell:M1497 -->
