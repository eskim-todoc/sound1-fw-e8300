# BOOT-009 적재 대상 파일 이름 결정

시험 ID: `SW-FW-UnitTest-BOOT-ID-9`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1391~1408행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1391 -->
선택한 코어에 맞는 이미지를 찾는다.<br>
<!-- /source-cell:E1391 -->


## 시험 방법

<!-- source-cell:F1391 -->
T<br>
<!-- /source-cell:F1391 -->


## 상세 설계 유닛

<!-- source-cell:G1391 -->
SU-ID-BOOT-009<br>
(적재 대상 파일 이름 결정)<br>
<!-- /source-cell:G1391 -->


## 시험 준비와 입력

<!-- source-cell:H1391 -->
- boot\_app\_load="test", boot\_app\_ext="app"로 구성하고 빈 이름/빈 확장자/null 인자 조건을 독립 준비한다.<br>
<!-- /source-cell:H1391 -->


## 가정 및 제한 조건

<!-- source-cell:I1391 -->
시험 입력 또는 가상 장치 응답은 유닛 입력 경계에 설정한다. 실행 결과를 미리 예상값으로 덮어쓰지 않는다. 실제 전원이나 RF 연결 확인과 소프트웨어 상태 확인을 구분한다. 운영 이미지 대신 격리된 시험 저장소를 사용한다. 초기 RTT 미사용 시 결과를 보존한 뒤 읽는다.<br>
<!-- /source-cell:I1391 -->


## 시험 절차

<!-- source-cell:J1391 -->
- 준비한 조건을 각각 독립 초기화하여 해당 유닛을 실행한다. 입력/출력 배열, 계산값과 횟수를 처리 완료 후 RTT로 기록하여 아래 예상 결과와 비교한다.<br>
수집: 준비된 BOOT 그룹(ut boot 1) 실행 후 UT-BOOT-009의 조건별 배열/상태와 판정 줄을 찾는다. <br>
<!-- /source-cell:J1391 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1391 -->
판정 범위<br>
- 정상 결과는"test.app", 반환 길이는 8 이다. 빈 이름/빈 확장자/null 인자는-1 이다. 이 함수는 코어 번호를 판정하지 않고 전달한 파일 이름/확장자를 조합한다.<br>
<!-- /source-cell:K1391 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1391 -->
검증 결과<br>
boot\_app\_load="test", boot\_app\_ext="app" / 파일 이름=test.app; 반환 길이=8문자(NUL 제외); name\[8\]=NUL<br>
boot\_app\_load가 빈 문자열 / 반환값=-1<br>
boot\_app\_ext가 빈 문자열 / 반환값=-1<br>
<!-- /source-cell:L1391 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1392 -->
조건 \[1\]<br>
입력/조건 \[1\]: boot\_app\_load="test", boot\_app\_ext="app"<br>
예상 결과: 파일 이름=test.app; 반환 길이=8문자(NUL 제외); name\[8\]=NUL<br>
<!-- /source-cell:K1392 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1392 -->
검증 \[1\]<br>
boot\_app\_load="test", boot\_app\_ext="app" / 파일 이름=test.app; 반환 길이=8문자(NUL 제외); name\[8\]=NUL<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1392 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1393 -->
조건 \[2\]<br>
입력/조건 \[2\]: boot\_app\_load가 빈 문자열<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1393 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1393 -->
검증 \[2\]<br>
boot\_app\_load가 빈 문자열 / 반환값=-1<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1393 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1394 -->
조건 \[3\]<br>
입력/조건 \[3\]: boot\_app\_ext가 빈 문자열<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1394 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1394 -->
검증 \[3\]<br>
boot\_app\_ext가 빈 문자열 / 반환값=-1<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1394 -->


### 비교 5

#### 입력 조건과 통과 기준

<!-- source-cell:K1395 -->
조건 \[4\]<br>
입력/조건 \[4\]: 정보 포인터=NULL 또는 출력 포인터=NULL<br>
예상 결과: 반환값=-1<br>
<!-- /source-cell:K1395 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1395 -->
검증 \[4\]<br>
정보 포인터=NULL 또는 출력 포인터=NULL / 반환값=-1<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1395 -->


### 비교 6

#### 입력 조건과 통과 기준

<!-- source-cell:K1396 -->
조건 \[5\]<br>
입력/조건 \[5\]: 유효한 이름/확장자를 전달한 조합 처리<br>
예상 결과: 처리 대상=전달된 이름과 확장자; 코어 번호 판정=없음<br>
<!-- /source-cell:K1396 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1396 -->
검증 \[5\]<br>
유효한 이름/확장자를 전달한 조합 처리 / 처리 대상=전달된 이름과 확장자; 코어 번호 판정=없음<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1396 -->


### 비교 7

#### 입력 조건과 통과 기준

<!-- source-cell:K1397 -->
검증 \[1\]의 입력/원소별 기준 (C1): 이름과 확장자 결합<br>
초기화/입력/동작: 기동정보 전체를 0으로 하고 boot\_app\_load\[0..4\]="test\\0", 나머지\[5..9\]=0, boot\_app\_ext\[0..3\]="app\\0"로 설정한다. name\[0..19\]=0에서 get\_app\_load\_ext를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 길이 (문자, NUL 제외)<br>
사전 예상값: 8<br>
payload 인덱스: 1<br>
의미/단위: name\[0\] ASCII<br>
사전 예상값: 116<br>
payload 인덱스: 2<br>
의미/단위: name\[1\] ASCII<br>
사전 예상값: 101<br>
<!-- /source-cell:K1397 -->


<!-- source-cell:K1398 -->
payload 인덱스: 3<br>
의미/단위: name\[2\] ASCII<br>
사전 예상값: 115<br>
payload 인덱스: 4<br>
의미/단위: name\[3\] ASCII<br>
사전 예상값: 116<br>
payload 인덱스: 5<br>
의미/단위: name\[4\] ASCII<br>
사전 예상값: 46<br>
payload 인덱스: 6<br>
의미/단위: name\[5\] ASCII<br>
사전 예상값: 97<br>
<!-- /source-cell:K1398 -->


<!-- source-cell:K1399 -->
payload 인덱스: 7<br>
의미/단위: name\[6\] ASCII<br>
사전 예상값: 112<br>
payload 인덱스: 8<br>
의미/단위: name\[7\] ASCII<br>
사전 예상값: 112<br>
문자열은 test.app, name\[8\]=NUL이다. 자동 payload는 앞 8문자와 길이를 비교한다.<br>
<!-- /source-cell:K1399 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1397 -->
검증 \[1\]: 이름과 확장자 결합<br>
출력\[0\] 길이 (문자, NUL 제외) = 8<br>
출력\[1\] name\[0\] ASCII = 116<br>
출력\[2\] name\[1\] ASCII = 101<br>
출력\[3\] name\[2\] ASCII = 115<br>
출력\[4\] name\[3\] ASCII = 116<br>
출력\[5\] name\[4\] ASCII = 46<br>
출력\[6\] name\[5\] ASCII = 97<br>
출력\[7\] name\[6\] ASCII = 112<br>
출력\[8\] name\[7\] ASCII = 112<br>
<!-- /source-cell:L1397 -->


### 비교 8

#### 입력 조건과 통과 기준

<!-- source-cell:K1400 -->
검증 \[2\]의 입력/원소별 기준 (C2): 빈 이름<br>
초기화/입력/동작: C1 뒤 boot\_app\_load\[0\]=0으로 한다. get\_app\_load\_ext를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1400 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1400 -->
검증 \[2\]: 빈 이름<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1400 -->


### 비교 9

#### 입력 조건과 통과 기준

<!-- source-cell:K1401 -->
검증 \[3\]의 입력/원소별 기준 (C3): 빈 확장자<br>
초기화/입력/동작: 이름을 test로 복구하고 boot\_app\_ext\[0\]=0으로 한다. get\_app\_load\_ext를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1401 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1401 -->
검증 \[3\]: 빈 확장자<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1401 -->


### 비교 10

#### 입력 조건과 통과 기준

<!-- source-cell:K1402 -->
검증 \[4\]의 입력/원소별 기준 (C4): NULL 정보<br>
초기화/입력/동작: 정보 포인터=NULL, name은 유효 버퍼로 전달한다. get\_app\_load\_ext를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1402 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1402 -->
검증 \[4\]: NULL 정보<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1402 -->


### 비교 11

#### 입력 조건과 통과 기준

<!-- source-cell:K1403 -->
검증 \[5\]의 입력/원소별 기준 (C5): NULL 결과<br>
초기화/입력/동작: 정보는 C3 상태의 유효 포인터이고 출력 포인터=NULL이다. get\_app\_load\_ext를 호출한다.<br>
payload 인덱스: 0<br>
의미/단위: 함수 반환값 (정수)<br>
사전 예상값: -1<br>
<!-- /source-cell:K1403 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1403 -->
검증 \[5\]: NULL 결과<br>
출력\[0\] 함수 반환값 (정수) = -1<br>
<!-- /source-cell:L1403 -->


### 비교 12

#### 입력 조건과 통과 기준

<!-- source-cell:K1404 -->
검증 \[1\]의 RTT 출력 (C1)<br>
입력/조건: 기동정보 전체를 0으로 하고 boot\_app\_load\[0..4\]="test\\0", 나머지\[5..9\]=0, boot\_app\_ext\[0..3\]="app\\0"로 설정한다. name\[0..19\]=0에서 get\_app\_load\_ext를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-009] C1 A[0:8]=8,116,101,115,116,46,97,112,112
[UT-BOOT-009] C1 PASS N=9 F=0
```
<!-- /source-cell:K1404 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1404 -->

```text
[UT-BOOT-009] C1 A[0:8]=8,116,101,115,116,46,97,112,112
[UT-BOOT-009] C1 PASS N=9 F=0
```
<!-- /source-cell:L1404 -->


### 비교 13

#### 입력 조건과 통과 기준

<!-- source-cell:K1405 -->
검증 \[2\]의 RTT 출력 (C2)<br>
입력/조건: C1 뒤 boot\_app\_load\[0\]=0으로 한다. get\_app\_load\_ext를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-009] C2 A[0:0]=-1
[UT-BOOT-009] C2 PASS N=1 F=0
```
<!-- /source-cell:K1405 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1405 -->

```text
[UT-BOOT-009] C2 A[0:0]=-1
[UT-BOOT-009] C2 PASS N=1 F=0
```
<!-- /source-cell:L1405 -->


### 비교 14

#### 입력 조건과 통과 기준

<!-- source-cell:K1406 -->
검증 \[3\]의 RTT 출력 (C3)<br>
입력/조건: 이름을 test로 복구하고 boot\_app\_ext\[0\]=0으로 한다. get\_app\_load\_ext를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-009] C3 A[0:0]=-1
[UT-BOOT-009] C3 PASS N=1 F=0
```
<!-- /source-cell:K1406 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1406 -->

```text
[UT-BOOT-009] C3 A[0:0]=-1
[UT-BOOT-009] C3 PASS N=1 F=0
```
<!-- /source-cell:L1406 -->


### 비교 15

#### 입력 조건과 통과 기준

<!-- source-cell:K1407 -->
검증 \[4\]의 RTT 출력 (C4)<br>
입력/조건: 정보 포인터=NULL, name은 유효 버퍼로 전달한다. get\_app\_load\_ext를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-009] C4 A[0:0]=-1
[UT-BOOT-009] C4 PASS N=1 F=0
```
<!-- /source-cell:K1407 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1407 -->

```text
[UT-BOOT-009] C4 A[0:0]=-1
[UT-BOOT-009] C4 PASS N=1 F=0
```
<!-- /source-cell:L1407 -->


### 비교 16

#### 입력 조건과 통과 기준

<!-- source-cell:K1408 -->
검증 \[5\]의 RTT 출력 (C5)<br>
입력/조건: 정보는 C3 상태의 유효 포인터이고 출력 포인터=NULL이다. get\_app\_load\_ext를 호출한다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-009] C5 A[0:0]=-1
[UT-BOOT-009] C5 PASS N=1 F=0
```
<!-- /source-cell:K1408 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1408 -->

```text
[UT-BOOT-009] C5 A[0:0]=-1
[UT-BOOT-009] C5 PASS N=1 F=0
```
<!-- /source-cell:L1408 -->


## 보고서 판정

<!-- source-cell:M1391 -->
통과<br>
<!-- /source-cell:M1391 -->
