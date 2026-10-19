# BOOT-002 기동 지연 대기

시험 ID: `SW-FW-UnitTest-BOOT-ID-2`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1288~1304행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1288 -->
초기 하드웨어 제어 사이에 필요한 대기 시간을 확보한다.<br>
<!-- /source-cell:E1288 -->


## 시험 방법

<!-- source-cell:F1288 -->
A<br>
<!-- /source-cell:F1288 -->


## 상세 설계 유닛

<!-- source-cell:G1288 -->
SU-ID-BOOT-002<br>
(기동 지연 대기)<br>
<!-- /source-cell:G1288 -->


## 시험 준비와 입력

<!-- source-cell:H1288 -->
- 요청0/1000us의 시간 단위/클럭/지연 계산/종료를 분석한다. 선택 실측은 이미 준비/승인된 시험 마커와 핀 역할을 확인한 경우만 한다. Calibration(DIO22)은 DMIC\_CLK1/서비스 진입과 공유되므로 전용 자동 마커로 가정하거나 원래 기능을 변경하지 않는다.<br>
<!-- /source-cell:H1288 -->


## 가정 및 제한 조건

<!-- source-cell:I1288 -->
코드 분석으로 판정한다. 실제 핀 파형, 전원, 처리 시간 및 전극 전류의 실측과 구분한다.<br>
<!-- /source-cell:I1288 -->


## 시험 절차

<!-- source-cell:J1288 -->
- Sys\_Delay 계산 cycle과 호출 경로를 코드 근거로 기록해 K와 비교한다. 선택 실측은 확인된 마커 전후 에지의 시간차/클럭/토글 오버헤드/측정점을 스코프로 별도 기록한다. 마커가 준비되지 않았으면 선택 실측 미실시로 남기며 cycle 값을 실제 us/ms 측정값으로 기록하지 않는다.<br>
<!-- /source-cell:J1288 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1288 -->
판정 범위<br>
- 시간 단위가us이고 요청 시간/클럭 조건이 지연 계산에 반영된다. 핀 실측에는 토글 오버헤드를 별도 기록한다. 단순 루프 분석을 정확한 실측 시간으로 쓰지 않는다.<br>
<!-- /source-cell:K1288 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1288 -->
검증 결과<br>
SystemCoreClock=30.72 MHz, 지연 요청=0/1000 us / 요청 단위=us; Sys\_Delay 인자=0/30000 cycle; 지연 처리=해당 인자 전달 뒤 호출 경로 종료<br>
SystemCoreClock=30.72 MHz, 지연 요청=1/999 us / 요청 단위=us; Sys\_Delay 인자=30/29970 cycle<br>
SystemCoreClock=30.72 MHz, 지연 요청=0/1/999/1000 ms / 요청 단위=ms; Sys\_Delay 인자=0/30720/30689280/30720000 cycle<br>
<!-- /source-cell:L1288 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1289 -->
조건 \[1\]<br>
입력/조건 \[1\]: SystemCoreClock=30.72 MHz, 지연 요청=0/1000 us<br>
예상 결과: 요청 단위=us; Sys\_Delay 인자=0/30000 cycle; 지연 처리=해당 인자 전달 뒤 호출 경로 종료<br>
<!-- /source-cell:K1289 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1289 -->
검증 \[1\]<br>
SystemCoreClock=30.72 MHz, 지연 요청=0/1000 us / 요청 단위=us; Sys\_Delay 인자=0/30000 cycle; 지연 처리=해당 인자 전달 뒤 호출 경로 종료<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1289 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1290 -->
조건 \[2\]<br>
입력/조건 \[2\]: SystemCoreClock=30.72 MHz, 지연 요청=1/999 us<br>
예상 결과: 요청 단위=us; Sys\_Delay 인자=30/29970 cycle<br>
<!-- /source-cell:K1290 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1290 -->
검증 \[2\]<br>
SystemCoreClock=30.72 MHz, 지연 요청=1/999 us / 요청 단위=us; Sys\_Delay 인자=30/29970 cycle<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1290 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1291 -->
조건 \[3\]<br>
입력/조건 \[3\]: SystemCoreClock=30.72 MHz, 지연 요청=0/1/999/1000 ms<br>
예상 결과: 요청 단위=ms; Sys\_Delay 인자=0/30720/30689280/30720000 cycle<br>
<!-- /source-cell:K1291 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1291 -->
검증 \[3\]<br>
SystemCoreClock=30.72 MHz, 지연 요청=0/1/999/1000 ms / 요청 단위=ms; Sys\_Delay 인자=0/30720/30689280/30720000 cycle<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1291 -->


### 비교 5

#### 입력 조건과 통과 기준

<!-- source-cell:K1292 -->
조건 \[4\]<br>
입력/조건 \[4\]: SystemCoreClock=32 MHz, 지연 요청=1000 us/1 ms<br>
예상 결과: 요청 단위=us/ms; Sys\_Delay 인자=32000/32000 cycle<br>
<!-- /source-cell:K1292 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1292 -->
검증 \[4\]<br>
SystemCoreClock=32 MHz, 지연 요청=1000 us/1 ms / 요청 단위=us/ms; Sys\_Delay 인자=32000/32000 cycle<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1292 -->


### 비교 6

#### 입력 조건과 통과 기준

<!-- source-cell:K1293 -->
검증 \[1\]의 입력/원소별 기준 (C1): 0 us 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: SystemCoreClock=30,720,000 Hz로 설정하고 tdc\_delay\_us(0)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1293 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1293 -->
검증 \[1\]: 0 us 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 0<br>
<!-- /source-cell:L1293 -->


### 비교 7

#### 입력 조건과 통과 기준

<!-- source-cell:K1294 -->
검증 \[2\]의 입력/원소별 기준 (C2): 1000 us 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: SystemCoreClock=30,720,000 Hz로 설정하고 tdc\_delay\_us(1000)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 30000 (0x00007530)<br>
<!-- /source-cell:K1294 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1294 -->
검증 \[2\]: 1000 us 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 30000 (0x00007530)<br>
<!-- /source-cell:L1294 -->


### 비교 8

#### 입력 조건과 통과 기준

<!-- source-cell:K1295 -->
검증 \[3\]의 입력/원소별 기준 (C3): 0 us 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: SystemCoreClock=30,720,000 Hz로 설정하고 tdc\_delay\_us(0)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1295 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1295 -->
검증 \[3\]: 0 us 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 0<br>
<!-- /source-cell:L1295 -->


### 비교 9

#### 입력 조건과 통과 기준

<!-- source-cell:K1296 -->
검증 \[4\]의 입력/원소별 기준 (C4): 1 us 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: SystemCoreClock=30,720,000 Hz로 설정하고 tdc\_delay\_us(1)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 30<br>
<!-- /source-cell:K1296 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1296 -->
검증 \[4\]: 1 us 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 30<br>
<!-- /source-cell:L1296 -->


### 비교 10

#### 입력 조건과 통과 기준

<!-- source-cell:K1297 -->
검증 \[5\]의 입력/원소별 기준 (C5): 999 us 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: SystemCoreClock=30,720,000 Hz로 설정하고 tdc\_delay\_us(999)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 29970 (0x00007512)<br>
<!-- /source-cell:K1297 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1297 -->
검증 \[5\]: 999 us 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 29970 (0x00007512)<br>
<!-- /source-cell:L1297 -->


### 비교 11

#### 입력 조건과 통과 기준

<!-- source-cell:K1298 -->
검증 \[6\]의 입력/원소별 기준 (C6): 1000 us 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: SystemCoreClock=30,720,000 Hz로 설정하고 tdc\_delay\_us(1000)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 30000 (0x00007530)<br>
<!-- /source-cell:K1298 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1298 -->
검증 \[6\]: 1000 us 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 30000 (0x00007530)<br>
<!-- /source-cell:L1298 -->


### 비교 12

#### 입력 조건과 통과 기준

<!-- source-cell:K1299 -->
검증 \[7\]의 입력/원소별 기준 (C7): 0 ms 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 30,720,000 Hz에서 tdc\_delay\_ms(0)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1299 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1299 -->
검증 \[7\]: 0 ms 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 0<br>
<!-- /source-cell:L1299 -->


### 비교 13

#### 입력 조건과 통과 기준

<!-- source-cell:K1300 -->
검증 \[8\]의 입력/원소별 기준 (C8): 1 ms 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 30,720,000 Hz에서 tdc\_delay\_ms(1)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 30720 (0x00007800)<br>
<!-- /source-cell:K1300 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1300 -->
검증 \[8\]: 1 ms 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 30720 (0x00007800)<br>
<!-- /source-cell:L1300 -->


### 비교 14

#### 입력 조건과 통과 기준

<!-- source-cell:K1301 -->
검증 \[9\]의 입력/원소별 기준 (C9): 999 ms 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 30,720,000 Hz에서 tdc\_delay\_ms(999)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 30689280 (0x01D44800)<br>
<!-- /source-cell:K1301 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1301 -->
검증 \[9\]: 999 ms 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 30689280 (0x01D44800)<br>
<!-- /source-cell:L1301 -->


### 비교 15

#### 입력 조건과 통과 기준

<!-- source-cell:K1302 -->
검증 \[10\]의 입력/원소별 기준 (C10): 1000 ms 요청<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 30,720,000 Hz에서 tdc\_delay\_ms(1000)를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 30720000 (0x01D4C000)<br>
<!-- /source-cell:K1302 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1302 -->
검증 \[10\]: 1000 ms 요청<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 30720000 (0x01D4C000)<br>
<!-- /source-cell:L1302 -->


### 비교 16

#### 입력 조건과 통과 기준

<!-- source-cell:K1303 -->
검증 \[11\]의 입력/원소별 기준 (C11): 32 MHz에서 1000 us<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 기존 클럭을 보관하고 SystemCoreClock=32,000,000 Hz로 바꿔 tdc\_delay\_us(1000)를 호출한 뒤 클럭을 복원한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 32000 (0x00007D00)<br>
<!-- /source-cell:K1303 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1303 -->
검증 \[11\]: 32 MHz에서 1000 us<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 32000 (0x00007D00)<br>
<!-- /source-cell:L1303 -->


### 비교 17

#### 입력 조건과 통과 기준

<!-- source-cell:K1304 -->
검증 \[12\]의 입력/원소별 기준 (C12): 32 MHz에서 1 ms<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 기존 클럭을 보관하고 SystemCoreClock=32,000,000 Hz로 바꿔 tdc\_delay\_ms(1)를 호출한 뒤 클럭을 복원한다.<br>
payload 인덱스: 0<br>
의미/단위: Sys\_Delay에 전달한 cycle 수 (cycle)<br>
사전 예상값: 32000 (0x00007D00)<br>
<!-- /source-cell:K1304 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1304 -->
검증 \[12\]: 32 MHz에서 1 ms<br>
출력\[0\] Sys\_Delay에 전달한 cycle 수 (cycle) = 32000 (0x00007D00)<br>
<!-- /source-cell:L1304 -->


## 보고서 판정

<!-- source-cell:M1288 -->
통과<br>
<!-- /source-cell:M1288 -->
