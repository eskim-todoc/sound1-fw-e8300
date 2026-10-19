# BOOT-013 이미지 CRC 검사

시험 ID: `SW-FW-UnitTest-BOOT-ID-13`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1534~1576행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1534 -->
이미지 저장/전달 중 데이터 손상을 검사한다.<br>
<!-- /source-cell:E1534 -->


## 시험 방법

<!-- source-cell:F1534 -->
A<br>
<!-- /source-cell:F1534 -->


## 상세 설계 유닛

<!-- source-cell:G1534 -->
SU-ID-BOOT-013<br>
(이미지 CRC 검사)<br>
<!-- /source-cell:G1534 -->


## 시험 준비와 입력

<!-- source-cell:H1534 -->
- 헤더를 포함한 첫 CRC 계산값을 0(잔여값 정상)과 1(잔여값 오류)로 나누어 코드의 분기를 검토한다. 복호화 후 CRC도 0/1, 복호화된 헤더의 암호화 표시 비트도 0(표시 없음)/1(표시 있음)로 각각 검토한다.<br>
- 헤더 포인터 없음, AES 초기화/복호화 오류 및 데이터 길이 0/1/3/4/5 조건을 추가 검토한다.<br>
<!-- /source-cell:H1534 -->


## 가정 및 제한 조건

<!-- source-cell:I1534 -->
코드 분석으로 판정한다. 실제 핀 파형, 전원, 처리 시간 및 전극 전류의 실측과 구분한다.<br>
<!-- /source-cell:I1534 -->


## 시험 절차

<!-- source-cell:J1534 -->
- 준비한 각 입력과 초기 상태에 대해 조건 분기, 호출 순서, 인자, 반환값 및 상태 변경을 코드에서 확인한다. 아래 예상 결과와 대조하고 검토한 조건 및 근거를 기록한다.<br>
수집: 준비된 BOOT 그룹(ut boot 1) 실행 후 UT-BOOT-013의 조건별 배열/상태와 판정 줄을 찾는다. <br>
<!-- /source-cell:J1534 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1534 -->
판정 범위<br>
- 첫 CRC가 0이면 성공(반환 0)이다. 첫 CRC 오류 후 복호화한 CRC가 0이고 암호화 표시 비트가 1이면 성공이다. 복호화 후 CRC 오류 또는 암호화 표시가 없으면 -1이다.<br>
- 헤더 포인터가 없으면 -1, AES 오류는 해당 오류값을 반환한다. CRC는 4바이트 워드와 남은 바이트를 처리하며 헤더 포함 잔여값 0으로 판정한다.<br>
<!-- /source-cell:K1534 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1534 -->
검증 결과<br>
헤더를 포함한 첫 CRC 잔여값=0 / 반환값=0; 판정 근거=헤더 포함 잔여값 0<br>
첫 CRC 잔여값=1, 복호화 후 잔여값=0, 암호화 표시 비트=1 / 반환값=0<br>
첫 CRC 잔여값=1, 복호화 후 잔여값=1 또는 암호화 표시 비트=0 / 반환값=-1<br>
<!-- /source-cell:L1534 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1535 -->
조건 \[1\]<br>
입력/조건 \[1\]: 헤더를 포함한 첫 CRC 잔여값=0<br>
예상 결과: 반환값=0; 판정 근거=헤더 포함 잔여값 0<br>
<!-- /source-cell:K1535 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1535 -->
검증 \[1\]<br>
헤더를 포함한 첫 CRC 잔여값=0 / 반환값=0; 판정 근거=헤더 포함 잔여값 0<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1535 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1536 -->
조건 \[2\]<br>
입력/조건 \[2\]: 첫 CRC 잔여값=1, 복호화 후 잔여값=0, 암호화 표시 비트=1<br>
예상 결과: 반환값=0<br>
<!-- /source-cell:K1536 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1536 -->
검증 \[2\]<br>
첫 CRC 잔여값=1, 복호화 후 잔여값=0, 암호화 표시 비트=1 / 반환값=0<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1536 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1537 -->
조건 \[3\]<br>
입력/조건 \[3\]: 첫 CRC 잔여값=1, 복호화 후 잔여값=1 또는 암호화 표시 비트=0<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1537 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1537 -->
검증 \[3\]<br>
첫 CRC 잔여값=1, 복호화 후 잔여값=1 또는 암호화 표시 비트=0 / 반환값=-1<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1537 -->


### 비교 5

#### 입력 조건과 통과 기준

<!-- source-cell:K1538 -->
조건 \[4\]<br>
입력/조건 \[4\]: 헤더 포인터=NULL<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1538 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1538 -->
검증 \[4\]<br>
헤더 포인터=NULL / 반환값=-1<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1538 -->


### 비교 6

#### 입력 조건과 통과 기준

<!-- source-cell:K1539 -->
조건 \[5\]<br>
입력/조건 \[5\]: AES 문맥 초기화 또는 복호화에서 오류 반환<br>
예상 결과: 반환값=해당 AES 함수의 오류값 그대로<br>
<!-- /source-cell:K1539 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1539 -->
검증 \[5\]<br>
AES 문맥 초기화 또는 복호화에서 오류 반환 / 반환값=해당 AES 함수의 오류값 그대로<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1539 -->


### 비교 7

#### 입력 조건과 통과 기준

<!-- source-cell:K1540 -->
조건 \[6\]<br>
입력/조건 \[6\]: 입력 워드=\[0x04030201,0x00000005\], 길이=0/1/3/4/5바이트<br>
예상 결과: CRC=0xFFFF/0xF1D1/0xADAD/0x89C3/0x9304; 처리 길이=0/1/3/4/5바이트<br>
<!-- /source-cell:K1540 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1540 -->
검증 \[6\]<br>
입력 워드=\[0x04030201,0x00000005\], 길이=0/1/3/4/5바이트 / CRC=0xFFFF/0xF1D1/0xADAD/0x89C3/0x9304; 처리 길이=0/1/3/4/5바이트<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1540 -->


### 비교 8

#### 입력 조건과 통과 기준

<!-- source-cell:K1541 -->
조건 \[7\]<br>
입력/조건 \[7\]: CRC 처리 길이=4/5바이트<br>
예상 결과: 처리 단위=4바이트 워드 1개/4바이트 워드 1개와 나머지 1바이트<br>
<!-- /source-cell:K1541 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1541 -->
검증 \[7\]<br>
CRC 처리 길이=4/5바이트 / 처리 단위=4바이트 워드 1개/4바이트 워드 1개와 나머지 1바이트<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1541 -->


### 비교 9

#### 입력 조건과 통과 기준

<!-- source-cell:K1542 -->
검증 \[1\]의 입력/원소별 기준 (C1): 평문 헤더 CRC 정상<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: offset=0, size=4, type=3, flags=0, aes\_pad\_count=0, reserved=0 헤더에 독립 CRC를 붙인다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1542 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1542 -->
검증 \[1\]: 평문 헤더 CRC 정상<br>
출력\[0\] 함수 반환값 (정수) = 0<br>
<!-- /source-cell:L1542 -->


### 비교 10

#### 입력 조건과 통과 기준

<!-- source-cell:K1543 -->
검증 \[2\]의 입력/원소별 기준 (C2): NULL 헤더<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 헤더 포인터=NULL을 전달한다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1543 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1543 -->
검증 \[2\]: NULL 헤더<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1543 -->


### 비교 11

#### 입력 조건과 통과 기준

<!-- source-cell:K1544 -->
검증 \[3\]의 입력/원소별 기준 (C3): 암호화 헤더 CRC 정상<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: flags=0x80으로 새 CRC를 붙인 16바이트 헤더 전체를 공개 키로 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1544 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1544 -->
검증 \[3\]: 암호화 헤더 CRC 정상<br>
출력\[0\] 함수 반환값 (정수) = 0<br>
<!-- /source-cell:L1544 -->


### 비교 12

#### 입력 조건과 통과 기준

<!-- source-cell:K1545 -->
검증 \[4\]의 입력/원소별 기준 (C4): 복호화 후 암호화 표시 없음<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 직전 호출이 복호화한 헤더에 flags=0을 설정하고 CRC를 재계산한 뒤 다시 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1545 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1545 -->
검증 \[4\]: 복호화 후 암호화 표시 없음<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1545 -->


### 비교 13

#### 입력 조건과 통과 기준

<!-- source-cell:K1546 -->
검증 \[5\]의 입력/원소별 기준 (C5): 복호화 후 CRC 손상<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: flags=0x80으로 CRC를 새로 만든 뒤 offset XOR 1로 손상시키고 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1546 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1546 -->
검증 \[5\]: 복호화 후 CRC 손상<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1546 -->


### 비교 14

#### 입력 조건과 통과 기준

<!-- source-cell:K1547 -->
검증 \[6\]의 입력/원소별 기준 (C6): CRC 바이트 길이 0<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=0 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 65535 (0x0000FFFF)<br>
<!-- /source-cell:K1547 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1547 -->
검증 \[6\]: CRC 바이트 길이 0<br>
출력\[0\] CCITT CRC (16비트 정수) = 65535 (0x0000FFFF)<br>
<!-- /source-cell:L1547 -->


### 비교 15

#### 입력 조건과 통과 기준

<!-- source-cell:K1548 -->
검증 \[7\]의 입력/원소별 기준 (C7): CRC 바이트 길이 1<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=1 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 61905 (0x0000F1D1)<br>
<!-- /source-cell:K1548 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1548 -->
검증 \[7\]: CRC 바이트 길이 1<br>
출력\[0\] CCITT CRC (16비트 정수) = 61905 (0x0000F1D1)<br>
<!-- /source-cell:L1548 -->


### 비교 16

#### 입력 조건과 통과 기준

<!-- source-cell:K1549 -->
검증 \[8\]의 입력/원소별 기준 (C8): CRC 바이트 길이 3<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=3 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 44461 (0x0000ADAD)<br>
<!-- /source-cell:K1549 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1549 -->
검증 \[8\]: CRC 바이트 길이 3<br>
출력\[0\] CCITT CRC (16비트 정수) = 44461 (0x0000ADAD)<br>
<!-- /source-cell:L1549 -->


### 비교 17

#### 입력 조건과 통과 기준

<!-- source-cell:K1550 -->
검증 \[9\]의 입력/원소별 기준 (C9): CRC 바이트 길이 4<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=4 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 35267 (0x000089C3)<br>
<!-- /source-cell:K1550 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1550 -->
검증 \[9\]: CRC 바이트 길이 4<br>
출력\[0\] CCITT CRC (16비트 정수) = 35267 (0x000089C3)<br>
<!-- /source-cell:L1550 -->


### 비교 18

#### 입력 조건과 통과 기준

<!-- source-cell:K1551 -->
검증 \[10\]의 입력/원소별 기준 (C10): CRC 바이트 길이 5<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=5 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 37636 (0x00009304)<br>
<!-- /source-cell:K1551 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1551 -->
검증 \[10\]: CRC 바이트 길이 5<br>
출력\[0\] CCITT CRC (16비트 정수) = 37636 (0x00009304)<br>
<!-- /source-cell:L1551 -->


### 비교 19

#### 입력 조건과 통과 기준

<!-- source-cell:K1552 -->
검증 \[11\]의 입력/원소별 기준 (C11): CRC 경계에 전달한 0바이트<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C6 직후 Sys\_CRC\_Add에 전달된 바이트 누계를 확인한다.<br>
payload 인덱스: 0<br>
의미/단위: CRC 하드웨어 모형 입력 길이 (byte)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1552 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1552 -->
검증 \[11\]: CRC 경계에 전달한 0바이트<br>
출력\[0\] CRC 하드웨어 모형 입력 길이 (byte) = 0<br>
<!-- /source-cell:L1552 -->


### 비교 20

#### 입력 조건과 통과 기준

<!-- source-cell:K1553 -->
검증 \[12\]의 입력/원소별 기준 (C12): CRC 경계에 전달한 1바이트<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C7 직후 Sys\_CRC\_Add에 전달된 바이트 누계를 확인한다.<br>
payload 인덱스: 0<br>
의미/단위: CRC 하드웨어 모형 입력 길이 (byte)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1553 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1553 -->
검증 \[12\]: CRC 경계에 전달한 1바이트<br>
출력\[0\] CRC 하드웨어 모형 입력 길이 (byte) = 1<br>
<!-- /source-cell:L1553 -->


### 비교 21

#### 입력 조건과 통과 기준

<!-- source-cell:K1554 -->
검증 \[13\]의 입력/원소별 기준 (C13): CRC 경계에 전달한 3바이트<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C8 직후 Sys\_CRC\_Add에 전달된 바이트 누계를 확인한다.<br>
payload 인덱스: 0<br>
의미/단위: CRC 하드웨어 모형 입력 길이 (byte)<br>
사전 예상값: 3<br>
<!-- /source-cell:K1554 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1554 -->
검증 \[13\]: CRC 경계에 전달한 3바이트<br>
출력\[0\] CRC 하드웨어 모형 입력 길이 (byte) = 3<br>
<!-- /source-cell:L1554 -->


### 비교 22

#### 입력 조건과 통과 기준

<!-- source-cell:K1555 -->
검증 \[14\]의 입력/원소별 기준 (C14): CRC 경계에 전달한 4바이트<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C9 직후 Sys\_CRC\_Add에 전달된 바이트 누계를 확인한다.<br>
payload 인덱스: 0<br>
의미/단위: CRC 하드웨어 모형 입력 길이 (byte)<br>
사전 예상값: 4<br>
<!-- /source-cell:K1555 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1555 -->
검증 \[14\]: CRC 경계에 전달한 4바이트<br>
출력\[0\] CRC 하드웨어 모형 입력 길이 (byte) = 4<br>
<!-- /source-cell:L1555 -->


### 비교 23

#### 입력 조건과 통과 기준

<!-- source-cell:K1556 -->
검증 \[15\]의 입력/원소별 기준 (C15): CRC 경계에 전달한 5바이트<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: C10 직후 Sys\_CRC\_Add에 전달된 바이트 누계를 확인한다.<br>
payload 인덱스: 0<br>
의미/단위: CRC 하드웨어 모형 입력 길이 (byte)<br>
사전 예상값: 5<br>
<!-- /source-cell:K1556 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1556 -->
검증 \[15\]: CRC 경계에 전달한 5바이트<br>
출력\[0\] CRC 하드웨어 모형 입력 길이 (byte) = 5<br>
<!-- /source-cell:L1556 -->


### 비교 24

#### 입력 조건과 통과 기준

<!-- source-cell:K1557 -->
검증 \[101\]의 입력/원소별 기준 (C101): 평문 헤더 CRC 정상<br>
초기화/입력/동작: offset=0, size=4, type=3, flags=0, aes\_pad\_count=0, reserved=0 헤더에 독립 CRC를 붙인다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1557 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1557 -->
검증 \[101\]: 평문 헤더 CRC 정상<br>
출력\[0\] 함수 반환값 (정수) = 0<br>
<!-- /source-cell:L1557 -->


### 비교 25

#### 입력 조건과 통과 기준

<!-- source-cell:K1558 -->
검증 \[102\]의 입력/원소별 기준 (C102): NULL 헤더<br>
초기화/입력/동작: 헤더 포인터=NULL을 전달한다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1558 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1558 -->
검증 \[102\]: NULL 헤더<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1558 -->


### 비교 26

#### 입력 조건과 통과 기준

<!-- source-cell:K1559 -->
검증 \[103\]의 입력/원소별 기준 (C103): 암호화 헤더 CRC 정상<br>
초기화/입력/동작: flags=0x80으로 새 CRC를 붙인 16바이트 헤더 전체를 공개 키로 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1559 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1559 -->
검증 \[103\]: 암호화 헤더 CRC 정상<br>
출력\[0\] 함수 반환값 (정수) = 0<br>
<!-- /source-cell:L1559 -->


### 비교 27

#### 입력 조건과 통과 기준

<!-- source-cell:K1560 -->
검증 \[104\]의 입력/원소별 기준 (C104): 복호화 후 암호화 표시 없음<br>
초기화/입력/동작: 직전 호출이 복호화한 헤더에 flags=0을 설정하고 CRC를 재계산한 뒤 다시 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1560 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1560 -->
검증 \[104\]: 복호화 후 암호화 표시 없음<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1560 -->


### 비교 28

#### 입력 조건과 통과 기준

<!-- source-cell:K1561 -->
검증 \[105\]의 입력/원소별 기준 (C105): 복호화 후 CRC 손상<br>
초기화/입력/동작: flags=0x80으로 CRC를 새로 만든 뒤 offset XOR 1로 손상시키고 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1561 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1561 -->
검증 \[105\]: 복호화 후 CRC 손상<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1561 -->


### 비교 29

#### 입력 조건과 통과 기준

<!-- source-cell:K1562 -->
검증 \[106\]의 입력/원소별 기준 (C106): CRC 바이트 길이 0<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=0 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 65535 (0x0000FFFF)<br>
<!-- /source-cell:K1562 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1562 -->
검증 \[106\]: CRC 바이트 길이 0<br>
출력\[0\] CCITT CRC (16비트 정수) = 65535 (0x0000FFFF)<br>
<!-- /source-cell:L1562 -->


### 비교 30

#### 입력 조건과 통과 기준

<!-- source-cell:K1563 -->
검증 \[107\]의 입력/원소별 기준 (C107): CRC 바이트 길이 1<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=1 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 61905 (0x0000F1D1)<br>
<!-- /source-cell:K1563 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1563 -->
검증 \[107\]: CRC 바이트 길이 1<br>
출력\[0\] CCITT CRC (16비트 정수) = 61905 (0x0000F1D1)<br>
<!-- /source-cell:L1563 -->


### 비교 31

#### 입력 조건과 통과 기준

<!-- source-cell:K1564 -->
검증 \[108\]의 입력/원소별 기준 (C108): CRC 바이트 길이 3<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=3 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 44461 (0x0000ADAD)<br>
<!-- /source-cell:K1564 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1564 -->
검증 \[108\]: CRC 바이트 길이 3<br>
출력\[0\] CCITT CRC (16비트 정수) = 44461 (0x0000ADAD)<br>
<!-- /source-cell:L1564 -->


### 비교 32

#### 입력 조건과 통과 기준

<!-- source-cell:K1565 -->
검증 \[109\]의 입력/원소별 기준 (C109): CRC 바이트 길이 4<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=4 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 35267 (0x000089C3)<br>
<!-- /source-cell:K1565 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1565 -->
검증 \[109\]: CRC 바이트 길이 4<br>
출력\[0\] CCITT CRC (16비트 정수) = 35267 (0x000089C3)<br>
<!-- /source-cell:L1565 -->


### 비교 33

#### 입력 조건과 통과 기준

<!-- source-cell:K1566 -->
검증 \[110\]의 입력/원소별 기준 (C110): CRC 바이트 길이 5<br>
초기화/입력/동작: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=5 byte로 bootloader\_CRC\_calc를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: CCITT CRC (16비트 정수)<br>
사전 예상값: 37636 (0x00009304)<br>
<!-- /source-cell:K1566 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1566 -->
검증 \[110\]: CRC 바이트 길이 5<br>
출력\[0\] CCITT CRC (16비트 정수) = 37636 (0x00009304)<br>
<!-- /source-cell:L1566 -->


### 비교 34

#### 입력 조건과 통과 기준

<!-- source-cell:K1567 -->
검증 \[101\]의 RTT 출력 (C101)<br>
입력/조건: offset=0, size=4, type=3, flags=0, aes\_pad\_count=0, reserved=0 헤더에 독립 CRC를 붙인다. bootloader\_check\_header\_crc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C101 A[0:0]=0
[UT-BOOT-013] C101 PASS N=1 F=0
```
<!-- /source-cell:K1567 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1567 -->

```text
[UT-BOOT-013] C101 A[0:0]=0
[UT-BOOT-013] C101 PASS N=1 F=0
```
<!-- /source-cell:L1567 -->


### 비교 35

#### 입력 조건과 통과 기준

<!-- source-cell:K1568 -->
검증 \[102\]의 RTT 출력 (C102)<br>
입력/조건: 헤더 포인터=NULL을 전달한다. bootloader\_check\_header\_crc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C102 A[0:0]=-1
[UT-BOOT-013] C102 PASS N=1 F=0
```
<!-- /source-cell:K1568 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1568 -->

```text
[UT-BOOT-013] C102 A[0:0]=-1
[UT-BOOT-013] C102 PASS N=1 F=0
```
<!-- /source-cell:L1568 -->


### 비교 36

#### 입력 조건과 통과 기준

<!-- source-cell:K1569 -->
검증 \[103\]의 RTT 출력 (C103)<br>
입력/조건: flags=0x80으로 새 CRC를 붙인 16바이트 헤더 전체를 공개 키로 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C103 A[0:0]=0
[UT-BOOT-013] C103 PASS N=1 F=0
```
<!-- /source-cell:K1569 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1569 -->

```text
[UT-BOOT-013] C103 A[0:0]=0
[UT-BOOT-013] C103 PASS N=1 F=0
```
<!-- /source-cell:L1569 -->


### 비교 37

#### 입력 조건과 통과 기준

<!-- source-cell:K1570 -->
검증 \[104\]의 RTT 출력 (C104)<br>
입력/조건: 직전 호출이 복호화한 헤더에 flags=0을 설정하고 CRC를 재계산한 뒤 다시 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C104 A[0:0]=-1
[UT-BOOT-013] C104 PASS N=1 F=0
```
<!-- /source-cell:K1570 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1570 -->

```text
[UT-BOOT-013] C104 A[0:0]=-1
[UT-BOOT-013] C104 PASS N=1 F=0
```
<!-- /source-cell:L1570 -->


### 비교 38

#### 입력 조건과 통과 기준

<!-- source-cell:K1571 -->
검증 \[105\]의 RTT 출력 (C105)<br>
입력/조건: flags=0x80으로 CRC를 새로 만든 뒤 offset XOR 1로 손상시키고 암호화한다. bootloader\_check\_header\_crc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C105 A[0:0]=-1
[UT-BOOT-013] C105 PASS N=1 F=0
```
<!-- /source-cell:K1571 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1571 -->

```text
[UT-BOOT-013] C105 A[0:0]=-1
[UT-BOOT-013] C105 PASS N=1 F=0
```
<!-- /source-cell:L1571 -->


### 비교 39

#### 입력 조건과 통과 기준

<!-- source-cell:K1572 -->
검증 \[106\]의 RTT 출력 (C106)<br>
입력/조건: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=0 byte로 bootloader\_CRC\_calc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C106 A[0:0]=65535
[UT-BOOT-013] C106 PASS N=1 F=0
```
<!-- /source-cell:K1572 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1572 -->

```text
[UT-BOOT-013] C106 A[0:0]=65535
[UT-BOOT-013] C106 PASS N=1 F=0
```
<!-- /source-cell:L1572 -->


### 비교 40

#### 입력 조건과 통과 기준

<!-- source-cell:K1573 -->
검증 \[107\]의 RTT 출력 (C107)<br>
입력/조건: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=1 byte로 bootloader\_CRC\_calc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C107 A[0:0]=61905
[UT-BOOT-013] C107 PASS N=1 F=0
```
<!-- /source-cell:K1573 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1573 -->

```text
[UT-BOOT-013] C107 A[0:0]=61905
[UT-BOOT-013] C107 PASS N=1 F=0
```
<!-- /source-cell:L1573 -->


### 비교 41

#### 입력 조건과 통과 기준

<!-- source-cell:K1574 -->
검증 \[108\]의 RTT 출력 (C108)<br>
입력/조건: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=3 byte로 bootloader\_CRC\_calc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C108 A[0:0]=44461
[UT-BOOT-013] C108 PASS N=1 F=0
```
<!-- /source-cell:K1574 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1574 -->

```text
[UT-BOOT-013] C108 A[0:0]=44461
[UT-BOOT-013] C108 PASS N=1 F=0
```
<!-- /source-cell:L1574 -->


### 비교 42

#### 입력 조건과 통과 기준

<!-- source-cell:K1575 -->
검증 \[109\]의 RTT 출력 (C109)<br>
입력/조건: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=4 byte로 bootloader\_CRC\_calc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C109 A[0:0]=35267
[UT-BOOT-013] C109 PASS N=1 F=0
```
<!-- /source-cell:K1575 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1575 -->

```text
[UT-BOOT-013] C109 A[0:0]=35267
[UT-BOOT-013] C109 PASS N=1 F=0
```
<!-- /source-cell:L1575 -->


### 비교 43

#### 입력 조건과 통과 기준

<!-- source-cell:K1576 -->
검증 \[110\]의 RTT 출력 (C110)<br>
입력/조건: uint32\_t words\[0..1\]=\{0x04030201,0x00000005\}를 입력하고 길이=5 byte로 bootloader\_CRC\_calc를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-013] C110 A[0:0]=37636
[UT-BOOT-013] C110 PASS N=1 F=0
```
<!-- /source-cell:K1576 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1576 -->

```text
[UT-BOOT-013] C110 A[0:0]=37636
[UT-BOOT-013] C110 PASS N=1 F=0
```
<!-- /source-cell:L1576 -->


## 보고서 판정

<!-- source-cell:M1534 -->
통과<br>
<!-- /source-cell:M1534 -->
