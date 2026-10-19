# BOOT-014 이미지 AES 복호화

시험 ID: `SW-FW-UnitTest-BOOT-ID-14`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1577~1589행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1577 -->
저장된 암호화 실행 이미지를 적재 가능한 데이터로 변환한다.<br>
<!-- /source-cell:E1577 -->


## 시험 방법

<!-- source-cell:F1577 -->
T<br>
<!-- /source-cell:F1577 -->


## 상세 설계 유닛

<!-- source-cell:G1577 -->
SU-ID-BOOT-014<br>
(이미지 AES 복호화)<br>
<!-- /source-cell:G1577 -->


## 시험 준비와 입력

<!-- source-cell:H1577 -->
- 키 uint32\_t\[4\]=\{0x2b7e1516,0x28aed2a6,0xabf71588,0x09cf4f3c\}로 문맥을 초기화한다. data\[4\]=\{0x3ad77bb4,0x0d7a3660,0xa89ecaf3,0x2466ef97\}, len=4(워드 수)를 복호화한다. 별도 사본에 len=3 을 적용한다.<br>
<!-- /source-cell:H1577 -->


## 가정 및 제한 조건

<!-- source-cell:I1577 -->
시험 입력 또는 가상 장치 응답은 유닛 입력 경계에 설정한다. 실행 결과를 미리 예상값으로 덮어쓰지 않는다. 실제 전원이나 RF 연결 확인과 소프트웨어 상태 확인을 구분한다. 운영 이미지 대신 격리된 시험 저장소를 사용한다. 초기 RTT 미사용 시 결과를 보존한 뒤 읽는다.<br>
<!-- /source-cell:I1577 -->


## 시험 절차

<!-- source-cell:J1577 -->
- 준비한 조건을 각각 독립 초기화하여 해당 유닛을 실행한다. 입력/출력 배열, 계산값과 횟수를 처리 완료 후 RTT로 기록하여 아래 예상 결과와 비교한다.<br>
수집: 준비된 BOOT 그룹(ut boot 1) 실행 후 UT-BOOT-014의 조건별 배열/상태와 판정 줄을 찾는다. <br>
<!-- /source-cell:J1577 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1577 -->
판정 범위<br>
- len=4 는 반환 0, 출력 워드 \{0x6bc1bee2,0x2e409f96,0xe93d7e11,0x7393172a\}다(NIST SP 800-38A F.1 공개 벡터). len=3 은 -1 을 반환하고 입력은 바뀌지 않아야 한다. ECB에는 IV를 사용하지 않는다.<br>
<!-- /source-cell:K1577 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1577 -->
검증 결과<br>
공개 시험 키 초기화, len=4워드, 암호문=\[3AD77BB4,0D7A3660,A89ECAF3,2466EF97\] / 초기화 반환값=0; 복호화 반환값=0; 출력 워드=\[0x6BC1BEE2,0x2E409F96,0xE93D7E11,0x7393172A\]<br>
원래 암호문 별도 사본, len=3워드 / 복호화 반환값=-1; 출력 워드=\[0x3AD77BB4,0x0D7A3660,0xA89ECAF3,0x2466EF97\]<br>
<!-- /source-cell:L1577 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1578 -->
조건 \[1\]<br>
입력/조건 \[1\]: 공개 시험 키 초기화, len=4워드, 암호문=\[3AD77BB4,0D7A3660,A89ECAF3,2466EF97\]<br>
예상 결과: 초기화 반환값=0; 복호화 반환값=0; 출력 워드=\[0x6BC1BEE2,0x2E409F96,0xE93D7E11,0x7393172A\]<br>
<!-- /source-cell:K1578 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1578 -->
검증 \[1\]<br>
공개 시험 키 초기화, len=4워드, 암호문=\[3AD77BB4,0D7A3660,A89ECAF3,2466EF97\] / 초기화 반환값=0; 복호화 반환값=0; 출력 워드=\[0x6BC1BEE2,0x2E409F96,0xE93D7E11,0x7393172A\]<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1578 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1579 -->
조건 \[2\]<br>
입력/조건 \[2\]: 원래 암호문 별도 사본, len=3워드<br>
예상 결과: 복호화 반환값=-1; 출력 워드=\[0x3AD77BB4,0x0D7A3660,0xA89ECAF3,0x2466EF97\]<br>
<!-- /source-cell:K1579 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1579 -->
검증 \[2\]<br>
원래 암호문 별도 사본, len=3워드 / 복호화 반환값=-1; 출력 워드=\[0x3AD77BB4,0x0D7A3660,0xA89ECAF3,0x2466EF97\]<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1579 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1580 -->
조건 \[3\]<br>
입력/조건 \[3\]: AES-128 ECB 한 블록 복호화<br>
예상 결과: 길이 단위=uint32 워드; IV=사용 없음<br>
<!-- /source-cell:K1580 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1580 -->
검증 \[3\]<br>
AES-128 ECB 한 블록 복호화 / 길이 단위=uint32 워드; IV=사용 없음<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1580 -->


### 비교 5

#### 입력 조건과 통과 기준

<!-- source-cell:K1581 -->
검증 \[1\]의 입력/원소별 기준 (C1): 공개 AES 128 ECB 한 블록<br>
초기화/입력/동작: 문맥을 공개 key\[0..3\]으로 초기화하고 ciphertext\[0..3\]에 bootloader\_decrypt\_data(ctx,data,4)를 호출한다. 4는 바이트가 아닌 uint32 워드 수다.<br>
payload 인덱스: 0<br>
의미/단위: 문맥 초기화 반환값<br>
사전 예상값: 0<br>
payload 인덱스: 1<br>
의미/단위: 복호화 반환값<br>
사전 예상값: 0<br>
payload 인덱스: 2<br>
의미/단위: data\[0\] (uint32 bit pattern)<br>
사전 예상값: 1807859426 (0x6BC1BEE2)<br>
<!-- /source-cell:K1581 -->


<!-- source-cell:K1582 -->
payload 인덱스: 3<br>
의미/단위: data\[1\] (uint32 bit pattern)<br>
사전 예상값: 775987094 (0x2E409F96)<br>
payload 인덱스: 4<br>
의미/단위: data\[2\] (uint32 bit pattern)<br>
사전 예상값: -381845999 (0xE93D7E11)<br>
payload 인덱스: 5<br>
의미/단위: data\[3\] (uint32 bit pattern)<br>
사전 예상값: 1939019562 (0x7393172A)<br>
<!-- /source-cell:K1582 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1581 -->
검증 \[1\]: 공개 AES 128 ECB 한 블록<br>
출력\[0\] 문맥 초기화 반환값 = 0<br>
출력\[1\] 복호화 반환값 = 0<br>
출력\[2\] data\[0\] (uint32 bit pattern) = 1807859426 (0x6BC1BEE2)<br>
출력\[3\] data\[1\] (uint32 bit pattern) = 775987094 (0x2E409F96)<br>
출력\[4\] data\[2\] (uint32 bit pattern) = -381845999 (0xE93D7E11)<br>
출력\[5\] data\[3\] (uint32 bit pattern) = 1939019562 (0x7393172A)<br>
<!-- /source-cell:L1581 -->


### 비교 6

#### 입력 조건과 통과 기준

<!-- source-cell:K1583 -->
검증 \[2\]의 입력/원소별 기준 (C2): 3워드 거부와 원본 보존<br>
초기화/입력/동작: C1 이전에 만들어 둔 원래 암호문 4워드 사본에 len=3을 적용한다. C1의 복호화된 데이터를 재사용하지 않는다.<br>
payload 인덱스: 0<br>
의미/단위: 복호화 반환값<br>
사전 예상값: -1<br>
payload 인덱스: 1<br>
의미/단위: 암호문 16바이트 보존 (0=다름,1=같음)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1583 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1583 -->
검증 \[2\]: 3워드 거부와 원본 보존<br>
출력\[0\] 복호화 반환값 = -1<br>
출력\[1\] 암호문 16바이트 보존 (0=다름,1=같음) = 1<br>
<!-- /source-cell:L1583 -->


### 비교 7

#### 입력 조건과 통과 기준

<!-- source-cell:K1584 -->
검증 \[3\]의 입력/원소별 기준 (C3): NULL AES 문맥<br>
초기화/입력/동작: bootloader\_initialize\_aes\_ctx(NULL,key)을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1584 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1584 -->
검증 \[3\]: NULL AES 문맥<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1584 -->


### 비교 8

#### 입력 조건과 통과 기준

<!-- source-cell:K1585 -->
검증 \[4\]의 입력/원소별 기준 (C4): NULL AES 키<br>
초기화/입력/동작: bootloader\_initialize\_aes\_ctx(&amp;ctx,NULL)을 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1585 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1585 -->
검증 \[4\]: NULL AES 키<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1585 -->


### 비교 9

#### 입력 조건과 통과 기준

<!-- source-cell:K1586 -->
검증 \[1\]의 RTT 출력 (C1)<br>
입력/조건: 문맥을 공개 key\[0..3\]으로 초기화하고 ciphertext\[0..3\]에 bootloader\_decrypt\_data(ctx,data,4)를 호출한다. 4는 바이트가 아닌 uint32 워드 수다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-014] C1 A[0:3]=0,0,1807859426,775987094
[UT-BOOT-014] C1 A[4:5]=-381845999,1939019562
[UT-BOOT-014] C1 PASS N=6 F=0
```
<!-- /source-cell:K1586 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1586 -->

```text
[UT-BOOT-014] C1 A[0:3]=0,0,1807859426,775987094
[UT-BOOT-014] C1 A[4:5]=-381845999,1939019562
[UT-BOOT-014] C1 PASS N=6 F=0
```
<!-- /source-cell:L1586 -->


### 비교 10

#### 입력 조건과 통과 기준

<!-- source-cell:K1587 -->
검증 \[2\]의 RTT 출력 (C2)<br>
입력/조건: C1 이전에 만들어 둔 원래 암호문 4워드 사본에 len=3을 적용한다. C1의 복호화된 데이터를 재사용하지 않는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-014] C2 A[0:1]=-1,1
[UT-BOOT-014] C2 PASS N=2 F=0
```
<!-- /source-cell:K1587 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1587 -->

```text
[UT-BOOT-014] C2 A[0:1]=-1,1
[UT-BOOT-014] C2 PASS N=2 F=0
```
<!-- /source-cell:L1587 -->


### 비교 11

#### 입력 조건과 통과 기준

<!-- source-cell:K1588 -->
검증 \[3\]의 RTT 출력 (C3)<br>
입력/조건: bootloader\_initialize\_aes\_ctx(NULL,key)을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-014] C3 A[0:0]=-1
[UT-BOOT-014] C3 PASS N=1 F=0
```
<!-- /source-cell:K1588 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1588 -->

```text
[UT-BOOT-014] C3 A[0:0]=-1
[UT-BOOT-014] C3 PASS N=1 F=0
```
<!-- /source-cell:L1588 -->


### 비교 12

#### 입력 조건과 통과 기준

<!-- source-cell:K1589 -->
검증 \[4\]의 RTT 출력 (C4)<br>
입력/조건: bootloader\_initialize\_aes\_ctx(&amp;ctx,NULL)을 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-014] C4 A[0:0]=-1
[UT-BOOT-014] C4 PASS N=1 F=0
```
<!-- /source-cell:K1589 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1589 -->

```text
[UT-BOOT-014] C4 A[0:0]=-1
[UT-BOOT-014] C4 PASS N=1 F=0
```
<!-- /source-cell:L1589 -->


## 보고서 판정

<!-- source-cell:M1577 -->
통과<br>
<!-- /source-cell:M1577 -->
