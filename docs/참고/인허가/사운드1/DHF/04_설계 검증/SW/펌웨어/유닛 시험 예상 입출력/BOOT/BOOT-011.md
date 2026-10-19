# BOOT-011 이미지 구획 처리

시험 ID: `SW-FW-UnitTest-BOOT-ID-11`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1452~1496행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1452 -->
프로그램, 데이터 및 기동 자료를 각 대상 메모리에 배치한다.<br>
<!-- /source-cell:E1452 -->


## 시험 방법

<!-- source-cell:F1452 -->
A<br>
<!-- /source-cell:F1452 -->


## 상세 설계 유닛

<!-- source-cell:G1452 -->
SU-ID-BOOT-011<br>
(이미지 구획 처리)<br>
<!-- /source-cell:G1452 -->


## 시험 준비와 입력

<!-- source-cell:H1452 -->
- 지원 구획별 주소/길이/데이터에 구분 값을 넣고 미지원 구획도 준비한다.<br>
<!-- /source-cell:H1452 -->


## 가정 및 제한 조건

<!-- source-cell:I1452 -->
코드 분석으로 판정한다. 실제 핀 파형, 전원, 처리 시간 및 전극 전류의 실측과 구분한다.<br>
<!-- /source-cell:I1452 -->


## 시험 절차

<!-- source-cell:J1452 -->
- 준비한 각 입력과 초기 상태에 대해 조건 분기, 호출 순서, 인자, 반환값 및 상태 변경을 코드에서 확인한다. 아래 예상 결과와 대조하고 검토한 조건 및 근거를 기록한다.<br>
수집: 준비된 BOOT 그룹(ut boot 1) 실행 후 UT-BOOT-011의 조건별 배열/상태와 판정 줄을 찾는다. <br>
<!-- /source-cell:J1452 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1452 -->
판정 범위<br>
- 지원 구획은 대응 코어/메모리로 전달하고 주소/길이가 입력과 같다. 오류 경로를 정상 적재로 처리하지 않는다.<br>
<!-- /source-cell:K1452 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1452 -->
검증 결과<br>
정상 이미지의 CM3 stack=536903664(0x20007FF0), entry=74565(0x12345), 반환값=0이었다. 잘린 이미지와 헤더 읽기 실패는 반환값=-1이었다. 각 구획의 대상 코어/주소/길이가 입력과 일치했고 오류 발생 후 정상 적재 완료로 처리되지 않았다.<br>
<!-- /source-cell:L1452 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1453 -->
조건 \[1\]<br>
입력/조건 \[1\]: type=3, offset=0, size=4워드, 입력=\[0x11,0x22,0x33,0x44\]<br>
예상 결과: 반환값=0; 대상 주소=0x20220000; 길이=16바이트; 자료 순서=0x11,0x22,0x33,0x44<br>
<!-- /source-cell:K1453 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1453 -->
검증 \[1\]<br>
입력: type=3, offset=0, size=4워드, 입력=\[0x11,0x22,0x33,0x44\]<br>
출력: 반환값=0; 대상 주소=0x20220000; 길이=16바이트; 자료 순서=0x11,0x22,0x33,0x44<br>
<!-- /source-cell:L1453 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1454 -->
조건 \[2\]<br>
입력/조건 \[2\]: type=1/2/4/5 지원 대상, size=4워드, 지정 offset<br>
예상 결과: 대상 주소=0x20800000/0x20C50000/0x00010000/0x20008000; 길이=각 16바이트<br>
<!-- /source-cell:K1454 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1454 -->
검증 \[2\]<br>
입력: type=1/2/4/5 지원 대상, size=4워드, 지정 offset<br>
출력: 대상 주소=0x20800000/0x20C50000/0x00010000/0x20008000; 길이=각 16바이트<br>
<!-- /source-cell:L1454 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1455 -->
조건 \[3\]<br>
입력/조건 \[3\]: CM3 기동 구획의 자료=\[0x20007FF0,0x00012345\]<br>
예상 결과: 반환값=0; stack\_pointer=0x20007FF0; init\_pointer=0x00012345<br>
<!-- /source-cell:K1455 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1455 -->
검증 \[3\]<br>
입력: CM3 기동 구획의 자료=\[0x20007FF0,0x00012345\]<br>
출력: 반환값=0; stack\_pointer=0x20007FF0; init\_pointer=0x00012345<br>
<!-- /source-cell:L1455 -->


### 비교 5

#### 입력 조건과 통과 기준

<!-- source-cell:K1456 -->
조건 \[4\]<br>
입력/조건 \[4\]: CFX 다음 CM3의 두 기동 구획<br>
예상 결과: 기동 자료 반환 플래그=3(CFX와 CM3); CFX stack\_pointer=0; CFX init\_pointer=0x23456; CM3 stack\_pointer=0x20007FF0; CM3 init\_pointer=0x00012345<br>
<!-- /source-cell:K1456 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1456 -->
검증 \[4\]<br>
입력: CFX 다음 CM3의 두 기동 구획<br>
출력: 기동 자료 반환 플래그=3(CFX와 CM3); CFX stack\_pointer=0; CFX init\_pointer=0x23456; CM3 stack\_pointer=0x20007FF0; CM3 init\_pointer=0x00012345<br>
<!-- /source-cell:L1456 -->


### 비교 6

#### 입력 조건과 통과 기준

<!-- source-cell:K1457 -->
조건 \[5\]<br>
입력/조건 \[5\]: 미지원 type=0xFFFF/PRAM 범위 밖/CRC 손상/AES 길이 3워드/입력 길이 부족<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1457 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1457 -->
검증 \[5\]<br>
입력: 미지원 type=0xFFFF/PRAM 범위 밖/CRC 손상/AES 길이 3워드/입력 길이 부족<br>
출력: 반환값=-1<br>
<!-- /source-cell:L1457 -->


### 비교 7

#### 입력 조건과 통과 기준

<!-- source-cell:K1458 -->
조건 \[6\]<br>
입력/조건 \[6\]: C6/C108: 구획 1개/2개 선언 후 빈 입력<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1458 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1458 -->
검증 \[6\]<br>
입력: C6/C108: 구획 1개/2개 선언 후 빈 입력<br>
출력: 반환값=-1<br>
<!-- /source-cell:L1458 -->


### 비교 8

#### 입력 조건과 통과 기준

<!-- source-cell:K1459 -->
조건 \[7\]<br>
입력/조건 \[7\]: C109: 첫 구획 헤더 읽기 오류<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1459 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1459 -->
검증 \[7\]<br>
입력: C109: 첫 구획 헤더 읽기 오류<br>
출력: 반환값=-1<br>
<!-- /source-cell:L1459 -->


### 비교 9

#### 입력 조건과 통과 기준

<!-- source-cell:K1460 -->
조건 \[8\]<br>
입력/조건 \[8\]: C110: CFX 구획 처리 뒤 세 번째 읽기 오류<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1460 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1460 -->
검증 \[8\]<br>
입력: C110: CFX 구획 처리 뒤 세 번째 읽기 오류<br>
출력: 반환값=-1<br>
<!-- /source-cell:L1460 -->


### 비교 10

#### 입력 조건과 통과 기준

<!-- source-cell:K1461 -->
검증 \[1\]의 입력/원소별 기준 (C1): 정상 CFX P 구획<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: data word\[0..3\]=\{0x11,0x22,0x33,0x44\}, type=3, size=4, offset=0, flags=0, 정상 data CRC. 파일을 처음부터 열어 bootloader\_transfer\_section\_data를 호출하고 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1461 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1461 -->
검증 \[1\]: 정상 CFX P 구획<br>
출력\[0\] 함수 반환값 (정수) = 0<br>
<!-- /source-cell:L1461 -->


### 비교 11

#### 입력 조건과 통과 기준

<!-- source-cell:K1462 -->
검증 \[2\]의 입력/원소별 기준 (C2): 미지원 구획 종류<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 같은 파일, type=0xFFFF, data\_crc=0. 파일을 처음부터 열어 bootloader\_transfer\_section\_data를 호출하고 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1462 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1462 -->
검증 \[2\]: 미지원 구획 종류<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1462 -->


### 비교 12

#### 입력 조건과 통과 기준

<!-- source-cell:K1463 -->
검증 \[3\]의 입력/원소별 기준 (C3): PRAM 끝 다음 주소<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: type=3, offset=(0x2023FFFF-0x20220000+1)/4=32768 word, size=4, data\_crc=0. 파일을 처음부터 열어 bootloader\_transfer\_section\_data를 호출하고 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1463 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1463 -->
검증 \[3\]: PRAM 끝 다음 주소<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1463 -->


### 비교 13

#### 입력 조건과 통과 기준

<!-- source-cell:K1464 -->
검증 \[4\]의 입력/원소별 기준 (C4): AES 3워드 길이 거부<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: offset=0, type=3, flags=0x80(암호화), size=3 word, data\_crc=0. 파일을 처음부터 열어 bootloader\_transfer\_section\_data를 호출하고 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1464 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1464 -->
검증 \[4\]: AES 3워드 길이 거부<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1464 -->


### 비교 14

#### 입력 조건과 통과 기준

<!-- source-cell:K1465 -->
검증 \[5\]의 입력/원소별 기준 (C5): 파일보다 큰 5워드 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: flags=0, type=3, size=5 word이나 파일은 16바이트, data\_crc=0. 파일을 처음부터 열어 bootloader\_transfer\_section\_data를 호출하고 닫는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1465 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1465 -->
검증 \[5\]: 파일보다 큰 5워드 요청<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1465 -->


### 비교 15

#### 입력 조건과 통과 기준

<!-- source-cell:K1466 -->
검증 \[6\]의 입력/원소별 기준 (C6): 선언한 한 구획 누락<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 0으로 초기화한 app\_header에 crc\_table\_size=1을 설정한다. 빈 파일과 0으로 초기화한 CFX/CM3 출력으로 load\_sections를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1466 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1466 -->
검증 \[6\]: 선언한 한 구획 누락<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1466 -->


### 비교 16

#### 입력 조건과 통과 기준

<!-- source-cell:K1467 -->
검증 \[101\]의 입력/원소별 기준 (C101): CM3 기동 자료 추출<br>
초기화/입력/동작: type=9, size=2 word, flags=0의 헤더와 평문 word\[0..1\]=\{0x20007FF0,0x00012345\}, 정상 data CRC를 주고 bootloader\_load\_boot\_section을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: 0<br>
payload 인덱스: 1<br>
의미/단위: CM3 stack\_pointer (byte 주소)<br>
사전 예상값: 536903664 (0x20007FF0)<br>
payload 인덱스: 2<br>
의미/단위: CM3 init\_pointer (byte 주소)<br>
사전 예상값: 74565 (0x00012345)<br>
<!-- /source-cell:K1467 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1467 -->
검증 \[101\]: CM3 기동 자료 추출<br>
출력\[0\] 반환값 = 0<br>
출력\[1\] CM3 stack\_pointer (byte 주소) = 536903664 (0x20007FF0)<br>
출력\[2\] CM3 init\_pointer (byte 주소) = 74565 (0x00012345)<br>
<!-- /source-cell:L1467 -->


### 비교 17

#### 입력 조건과 통과 기준

<!-- source-cell:K1468 -->
검증 \[102\]의 입력/원소별 기준 (C102): 기동 자료 3바이트<br>
초기화/입력/동작: C101의 파일 위치=0, 길이=3으로 재호출. 이전 출력의 구체적 값은 이 payload에서 비교하지 않는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1468 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1468 -->
검증 \[102\]: 기동 자료 3바이트<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1468 -->


### 비교 18

#### 입력 조건과 통과 기준

<!-- source-cell:K1469 -->
검증 \[103\]의 입력/원소별 기준 (C103): 기동 자료 CRC 오류<br>
초기화/입력/동작: 위치=0, 길이=8, data\_crc XOR 1로 재호출. 이전 출력의 구체적 값은 이 payload에서 비교하지 않는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1469 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1469 -->
검증 \[103\]: 기동 자료 CRC 오류<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1469 -->


### 비교 19

#### 입력 조건과 통과 기준

<!-- source-cell:K1470 -->
검증 \[104\]의 입력/원소별 기준 (C104): NULL 기동 헤더<br>
초기화/입력/동작: section\_header=NULL로 호출. 이전 출력의 구체적 값은 이 payload에서 비교하지 않는다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1470 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1470 -->
검증 \[104\]: NULL 기동 헤더<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1470 -->


### 비교 20

#### 입력 조건과 통과 기준

<!-- source-cell:K1471 -->
검증 \[105\]의 입력/원소별 기준 (C105): CFX 다음 CM3의 두 구획<br>
초기화/입력/동작: 48바이트의 두 기동 구획을 제공한다. CRC 목록 2개, CFX/CM3 출력은 0으로 초기화한 후 load\_sections를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환 기동자료 플래그 (bit0=CFX, bit1=CM3)<br>
사전 예상값: 3<br>
payload 인덱스: 1<br>
의미/단위: CFX stack\_pointer<br>
사전 예상값: 0<br>
payload 인덱스: 2<br>
의미/단위: CFX init\_pointer (CFX 주소)<br>
사전 예상값: 144470 (0x00023456)<br>
<!-- /source-cell:K1471 -->


<!-- source-cell:K1472 -->
payload 인덱스: 3<br>
의미/단위: CM3 stack\_pointer (byte 주소)<br>
사전 예상값: 536903664 (0x20007FF0)<br>
payload 인덱스: 4<br>
의미/단위: CM3 init\_pointer (byte 주소)<br>
사전 예상값: 74565 (0x00012345)<br>
3은 두 기동 자료를 파싱했다는 뜻이다. 실제 이미지 전송 또는 코어 실행 완료 플래그가 아니다.<br>
<!-- /source-cell:K1472 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1471 -->
검증 \[105\]: CFX 다음 CM3의 두 구획<br>
출력\[0\] 반환 기동자료 플래그 (bit0=CFX, bit1=CM3) = 3<br>
출력\[1\] CFX stack\_pointer = 0<br>
출력\[2\] CFX init\_pointer (CFX 주소) = 144470 (0x00023456)<br>
출력\[3\] CM3 stack\_pointer (byte 주소) = 536903664 (0x20007FF0)<br>
출력\[4\] CM3 init\_pointer (byte 주소) = 74565 (0x00012345)<br>
<!-- /source-cell:L1471 -->


### 비교 21

#### 입력 조건과 통과 기준

<!-- source-cell:K1473 -->
검증 \[106\]의 입력/원소별 기준 (C106): 첫 구획 헤더 CRC 손상<br>
초기화/입력/동작: 첫 헤더 word\[0\] XOR 1, 파일 위치=0, 위치=0에서 load\_sections를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1473 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1473 -->
검증 \[106\]: 첫 구획 헤더 CRC 손상<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1473 -->


### 비교 22

#### 입력 조건과 통과 기준

<!-- source-cell:K1474 -->
검증 \[107\]의 입력/원소별 기준 (C107): 첫 구획 데이터 부족<br>
초기화/입력/동작: 첫 헤더를 복구하고 입력 길이를 16+3=19바이트로 제한, 위치=0에서 load\_sections를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1474 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1474 -->
검증 \[107\]: 첫 구획 데이터 부족<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1474 -->


### 비교 23

#### 입력 조건과 통과 기준

<!-- source-cell:K1475 -->
검증 \[108\]의 입력/원소별 기준 (C108): 선언한 두 구획 누락<br>
초기화/입력/동작: CRC 목록 개수=2를 유지하고 입력 길이=0, 위치=0에서 load\_sections를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1475 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1475 -->
검증 \[108\]: 선언한 두 구획 누락<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1475 -->


### 비교 24

#### 입력 조건과 통과 기준

<!-- source-cell:K1476 -->
검증 \[109\]의 입력/원소별 기준 (C109): 첫 헤더 읽기 실패<br>
초기화/입력/동작: 정상 48바이트 입력으로 복구, 읽기 수=0, fail\_on\_read=1, 위치=0에서 load\_sections를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1476 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1476 -->
검증 \[109\]: 첫 헤더 읽기 실패<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1476 -->


### 비교 25

#### 입력 조건과 통과 기준

<!-- source-cell:K1477 -->
검증 \[110\]의 입력/원소별 기준 (C110): CFX 뒤 CM3 헤더 읽기 실패<br>
초기화/입력/동작: 정상 48바이트 입력, 읽기 수=0, fail\_on\_read=3, 위치=0에서 load\_sections를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1477 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1477 -->
검증 \[110\]: CFX 뒤 CM3 헤더 읽기 실패<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1477 -->


### 비교 26

#### 입력 조건과 통과 기준

<!-- source-cell:K1478 -->
검증 \[111\]의 입력/원소별 기준 (C111): NULL 기동 출력은 읽지 않음<br>
초기화/입력/동작: 위치/읽기 수=0, fail\_on\_read=0에서 load\_boot\_section의 boot\_data=NULL로 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: RAM 읽기 횟수 (회)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1478 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1478 -->
검증 \[111\]: NULL 기동 출력은 읽지 않음<br>
출력\[0\] 반환값 = -1<br>
출력\[1\] RAM 읽기 횟수 (회) = 0<br>
<!-- /source-cell:L1478 -->


### 비교 27

#### 입력 조건과 통과 기준

<!-- source-cell:K1479 -->
검증 \[112\]의 입력/원소별 기준 (C112): 암호화 기동 자료<br>
초기화/입력/동작: 평문 word\[0..3\]=\{0x20007FF0,0x00012345,0x11223344,0x55667788\}를 공개 키로 암호화한 16바이트를 제공한다. type=9, size=2, flags=0x80, 평문 전체 16바이트의 data CRC로 load\_boot\_section을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: 0<br>
payload 인덱스: 1<br>
의미/단위: CM3 stack\_pointer (byte 주소)<br>
<!-- /source-cell:K1479 -->


<!-- source-cell:K1480 -->
사전 예상값: 536903664 (0x20007FF0)<br>
payload 인덱스: 2<br>
의미/단위: CM3 init\_pointer (byte 주소)<br>
사전 예상값: 74565 (0x00012345)<br>
<!-- /source-cell:K1480 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1479 -->
검증 \[112\]: 암호화 기동 자료<br>
출력\[0\] 반환값 = 0<br>
출력\[1\] CM3 stack\_pointer (byte 주소) = 536903664 (0x20007FF0)<br>
출력\[2\] CM3 init\_pointer (byte 주소) = 74565 (0x00012345)<br>
<!-- /source-cell:L1479 -->


### 비교 28

#### 입력 조건과 통과 기준

<!-- source-cell:K1481 -->
검증 \[113\]의 입력/원소별 기준 (C113): AES 15바이트 짧은 입력<br>
초기화/입력/동작: 출력 stack\_pointer=0x00654321, init\_pointer=0x00123456으로 놓고 위치=0에서 16바이트 암호문의 첫 15바이트만 제공하여 load\_boot\_section을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: 출력 8바이트 보존 (0=다름, 1=같음)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1481 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1481 -->
검증 \[113\]: AES 15바이트 짧은 입력<br>
출력\[0\] 반환값 = -1<br>
출력\[1\] 출력 8바이트 보존 (0=다름, 1=같음) = 1<br>
<!-- /source-cell:L1481 -->


### 비교 29

#### 입력 조건과 통과 기준

<!-- source-cell:K1482 -->
검증 \[114\]의 입력/원소별 기준 (C114): AES 데이터 CRC 오류<br>
초기화/입력/동작: 출력 stack\_pointer=0x00654321, init\_pointer=0x00123456으로 놓고 위치=0에서 암호문 16바이트를 제공하되 data\_crc XOR 1하여 load\_boot\_section을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: 출력 8바이트 보존 (0=다름, 1=같음)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1482 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1482 -->
검증 \[114\]: AES 데이터 CRC 오류<br>
출력\[0\] 반환값 = -1<br>
출력\[1\] 출력 8바이트 보존 (0=다름, 1=같음) = 1<br>
<!-- /source-cell:L1482 -->


### 비교 30

#### 입력 조건과 통과 기준

<!-- source-cell:K1483 -->
검증 \[101\]의 RTT 출력 (C101)<br>
입력/조건: type=9, size=2 word, flags=0의 헤더와 평문 word\[0..1\]=\{0x20007FF0,0x00012345\}, 정상 data CRC를 주고 bootloader\_load\_boot\_section을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C101 A[0:2]=0,536903664,74565
[UT-BOOT-011] C101 PASS N=3 F=0
```
<!-- /source-cell:K1483 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1483 -->

```text
[UT-BOOT-011] C101 A[0:2]=0,536903664,74565
[UT-BOOT-011] C101 PASS N=3 F=0
```
<!-- /source-cell:L1483 -->


### 비교 31

#### 입력 조건과 통과 기준

<!-- source-cell:K1484 -->
검증 \[102\]의 RTT 출력 (C102)<br>
입력/조건: C101의 파일 위치=0, 길이=3으로 재호출. 이전 출력의 구체적 값은 이 payload에서 비교하지 않는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C102 A[0:0]=-1
[UT-BOOT-011] C102 PASS N=1 F=0
```
<!-- /source-cell:K1484 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1484 -->

```text
[UT-BOOT-011] C102 A[0:0]=-1
[UT-BOOT-011] C102 PASS N=1 F=0
```
<!-- /source-cell:L1484 -->


### 비교 32

#### 입력 조건과 통과 기준

<!-- source-cell:K1485 -->
검증 \[103\]의 RTT 출력 (C103)<br>
입력/조건: 위치=0, 길이=8, data\_crc XOR 1로 재호출. 이전 출력의 구체적 값은 이 payload에서 비교하지 않는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C103 A[0:0]=-1
[UT-BOOT-011] C103 PASS N=1 F=0
```
<!-- /source-cell:K1485 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1485 -->

```text
[UT-BOOT-011] C103 A[0:0]=-1
[UT-BOOT-011] C103 PASS N=1 F=0
```
<!-- /source-cell:L1485 -->


### 비교 33

#### 입력 조건과 통과 기준

<!-- source-cell:K1486 -->
검증 \[104\]의 RTT 출력 (C104)<br>
입력/조건: section\_header=NULL로 호출. 이전 출력의 구체적 값은 이 payload에서 비교하지 않는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C104 A[0:0]=-1
[UT-BOOT-011] C104 PASS N=1 F=0
```
<!-- /source-cell:K1486 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1486 -->

```text
[UT-BOOT-011] C104 A[0:0]=-1
[UT-BOOT-011] C104 PASS N=1 F=0
```
<!-- /source-cell:L1486 -->


### 비교 34

#### 입력 조건과 통과 기준

<!-- source-cell:K1487 -->
검증 \[105\]의 RTT 출력 (C105)<br>
입력/조건: 48바이트의 두 기동 구획을 제공한다. CRC 목록 2개, CFX/CM3 출력은 0으로 초기화한 후 load\_sections를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C105 A[0:4]=3,0,144470,536903664,74565
[UT-BOOT-011] C105 PASS N=5 F=0
```
<!-- /source-cell:K1487 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1487 -->

```text
[UT-BOOT-011] C105 A[0:4]=3,0,144470,536903664,74565
[UT-BOOT-011] C105 PASS N=5 F=0
```
<!-- /source-cell:L1487 -->


### 비교 35

#### 입력 조건과 통과 기준

<!-- source-cell:K1488 -->
검증 \[106\]의 RTT 출력 (C106)<br>
입력/조건: 첫 헤더 word\[0\] XOR 1, 파일 위치=0, 위치=0에서 load\_sections를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C106 A[0:0]=-1
[UT-BOOT-011] C106 PASS N=1 F=0
```
<!-- /source-cell:K1488 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1488 -->

```text
[UT-BOOT-011] C106 A[0:0]=-1
[UT-BOOT-011] C106 PASS N=1 F=0
```
<!-- /source-cell:L1488 -->


### 비교 36

#### 입력 조건과 통과 기준

<!-- source-cell:K1489 -->
검증 \[107\]의 RTT 출력 (C107)<br>
입력/조건: 첫 헤더를 복구하고 입력 길이를 16+3=19바이트로 제한, 위치=0에서 load\_sections를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C107 A[0:0]=-1
[UT-BOOT-011] C107 PASS N=1 F=0
```
<!-- /source-cell:K1489 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1489 -->

```text
[UT-BOOT-011] C107 A[0:0]=-1
[UT-BOOT-011] C107 PASS N=1 F=0
```
<!-- /source-cell:L1489 -->


### 비교 37

#### 입력 조건과 통과 기준

<!-- source-cell:K1490 -->
검증 \[108\]의 RTT 출력 (C108)<br>
입력/조건: CRC 목록 개수=2를 유지하고 입력 길이=0, 위치=0에서 load\_sections를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C108 A[0:0]=-1
[UT-BOOT-011] C108 PASS N=1 F=0
```
<!-- /source-cell:K1490 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1490 -->

```text
[UT-BOOT-011] C108 A[0:0]=-1
[UT-BOOT-011] C108 PASS N=1 F=0
```
<!-- /source-cell:L1490 -->


### 비교 38

#### 입력 조건과 통과 기준

<!-- source-cell:K1491 -->
검증 \[109\]의 RTT 출력 (C109)<br>
입력/조건: 정상 48바이트 입력으로 복구, 읽기 수=0, fail\_on\_read=1, 위치=0에서 load\_sections를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C109 A[0:0]=-1
[UT-BOOT-011] C109 PASS N=1 F=0
```
<!-- /source-cell:K1491 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1491 -->

```text
[UT-BOOT-011] C109 A[0:0]=-1
[UT-BOOT-011] C109 PASS N=1 F=0
```
<!-- /source-cell:L1491 -->


### 비교 39

#### 입력 조건과 통과 기준

<!-- source-cell:K1492 -->
검증 \[110\]의 RTT 출력 (C110)<br>
입력/조건: 정상 48바이트 입력, 읽기 수=0, fail\_on\_read=3, 위치=0에서 load\_sections를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C110 A[0:0]=-1
[UT-BOOT-011] C110 PASS N=1 F=0
```
<!-- /source-cell:K1492 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1492 -->

```text
[UT-BOOT-011] C110 A[0:0]=-1
[UT-BOOT-011] C110 PASS N=1 F=0
```
<!-- /source-cell:L1492 -->


### 비교 40

#### 입력 조건과 통과 기준

<!-- source-cell:K1493 -->
검증 \[111\]의 RTT 출력 (C111)<br>
입력/조건: 위치/읽기 수=0, fail\_on\_read=0에서 load\_boot\_section의 boot\_data=NULL로 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C111 A[0:1]=-1,0
[UT-BOOT-011] C111 PASS N=2 F=0
```
<!-- /source-cell:K1493 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1493 -->

```text
[UT-BOOT-011] C111 A[0:1]=-1,0
[UT-BOOT-011] C111 PASS N=2 F=0
```
<!-- /source-cell:L1493 -->


### 비교 41

#### 입력 조건과 통과 기준

<!-- source-cell:K1494 -->
검증 \[112\]의 RTT 출력 (C112)<br>
입력/조건: 평문 word\[0..3\]=\{0x20007FF0,0x00012345,0x11223344,0x55667788\}를 공개 키로 암호화한 16바이트를 제공한다. type=9, size=2, flags=0x80, 평문 전체 16바이트의 data CRC로 load\_boot\_section을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C112 A[0:2]=0,536903664,74565
[UT-BOOT-011] C112 PASS N=3 F=0
```
<!-- /source-cell:K1494 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1494 -->

```text
[UT-BOOT-011] C112 A[0:2]=0,536903664,74565
[UT-BOOT-011] C112 PASS N=3 F=0
```
<!-- /source-cell:L1494 -->


### 비교 42

#### 입력 조건과 통과 기준

<!-- source-cell:K1495 -->
검증 \[113\]의 RTT 출력 (C113)<br>
입력/조건: 출력 stack\_pointer=0x00654321, init\_pointer=0x00123456으로 놓고 위치=0에서 16바이트 암호문의 첫 15바이트만 제공하여 load\_boot\_section을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C113 A[0:1]=-1,1
[UT-BOOT-011] C113 PASS N=2 F=0
```
<!-- /source-cell:K1495 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1495 -->

```text
[UT-BOOT-011] C113 A[0:1]=-1,1
[UT-BOOT-011] C113 PASS N=2 F=0
```
<!-- /source-cell:L1495 -->


### 비교 43

#### 입력 조건과 통과 기준

<!-- source-cell:K1496 -->
검증 \[114\]의 RTT 출력 (C114)<br>
입력/조건: 출력 stack\_pointer=0x00654321, init\_pointer=0x00123456으로 놓고 위치=0에서 암호문 16바이트를 제공하되 data\_crc XOR 1하여 load\_boot\_section을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-011] C114 A[0:1]=-1,1
[UT-BOOT-011] C114 PASS N=2 F=0
```
<!-- /source-cell:K1496 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1496 -->

```text
[UT-BOOT-011] C114 A[0:1]=-1,1
[UT-BOOT-011] C114 PASS N=2 F=0
```
<!-- /source-cell:L1496 -->


## 보고서 판정

<!-- source-cell:M1452 -->
통과<br>
<!-- /source-cell:M1452 -->
