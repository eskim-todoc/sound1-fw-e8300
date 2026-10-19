# BOOT-020 기동 슬롯 선택과 대체 기동

시험 ID: `SW-FW-UnitTest-BOOT-ID-20`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1686~1726행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1686 -->
새 이미지 시도 결과에 따라 사용할 기동 위치를 결정한다.<br>
<!-- /source-cell:E1686 -->


## 시험 방법

<!-- source-cell:F1686 -->
T<br>
<!-- /source-cell:F1686 -->


## 상세 설계 유닛

<!-- source-cell:G1686 -->
SU-ID-BOOT-020<br>
(기동 슬롯 선택과 대체 기동)<br>
<!-- /source-cell:G1686 -->


## 시험 준비와 입력

<!-- source-cell:H1686 -->
- 기본 기동 슬롯은 1, 대체 슬롯은 2, 기동 시도 상한은 3으로 설정한다. 정상 기동(BOOT), 대체 기동/시도 중(ALT\_BOOT/BOOT\_TRY), 시도 완료(TRY\_DONE) 상태를 각각 준비한다.<br>
- 시도 중 상태의 호출 전 횟수 2와 3, 시도 완료의 성공/실패 및 지원하지 않는 하위 상태를 독립적으로 적용한다.<br>
<!-- /source-cell:H1686 -->


## 가정 및 제한 조건

<!-- source-cell:I1686 -->
시험 입력 또는 가상 장치 응답은 유닛 입력 경계에 설정한다. 실행 결과를 미리 예상값으로 덮어쓰지 않는다. 실제 전원이나 RF 연결 확인과 소프트웨어 상태 확인을 구분한다. 운영 이미지 대신 격리된 시험 저장소를 사용한다. 초기 RTT 미사용 시 결과를 보존한 뒤 읽는다.<br>
<!-- /source-cell:I1686 -->


## 시험 절차

<!-- source-cell:J1686 -->
- 준비한 조건을 각각 독립 초기화하여 해당 유닛을 실행한다. 입력/출력 배열, 계산값과 횟수를 처리 완료 후 RTT로 기록하여 아래 예상 결과와 비교한다.<br>
수집: 준비된 BOOT 그룹(ut boot 1) 실행 후 UT-BOOT-020의 조건별 배열/상태와 판정 줄을 찾는다. <br>
<!-- /source-cell:J1686 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1686 -->
판정 범위<br>
- 정상 기동은 슬롯 1이다. 호출 전 시도 횟수가 2이면 새 횟수 3, 슬롯 2다. 횟수가 3이면 새 횟수 4, 슬롯 1로 돌아가며 BOOT/TRY\_DONE/FAIL 상태를 기록한다.<br>
- 시도 완료 성공은 슬롯 2, 실패 또는 미지원 상태는 슬롯 1과 UNKNOWN 상태다. 슬롯 번호의 유효 범위 확인은 해당 분기를 별도로 분석한다.<br>
<!-- /source-cell:K1686 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1686 -->
검증 결과<br>
기본 슬롯=1, 대체 슬롯=2, 정상 상태=BOOT / 선택 슬롯=1<br>
ALT\_BOOT/BOOT\_TRY, 상한=3, 호출 전 시도 횟수=2 / 선택 슬롯=2; 저장 시도 횟수=3; 저장 상태=ALT\_BOOT/BOOT\_TRY<br>
ALT\_BOOT/BOOT\_TRY, 상한=3, 호출 전 시도 횟수=3 / 선택 슬롯=1; 저장 시도 횟수=4; 저장 상태=BOOT/TRY\_DONE/FAIL<br>
<!-- /source-cell:L1686 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1687 -->
조건 \[1\]<br>
입력/조건 \[1\]: 기본 슬롯=1, 대체 슬롯=2, 정상 상태=BOOT<br>
예상 결과: 선택 슬롯=1<br>
<!-- /source-cell:K1687 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1687 -->
검증 \[1\]<br>
기본 슬롯=1, 대체 슬롯=2, 정상 상태=BOOT / 선택 슬롯=1<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1687 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1688 -->
조건 \[2\]<br>
입력/조건 \[2\]: ALT\_BOOT/BOOT\_TRY, 상한=3, 호출 전 시도 횟수=2<br>
예상 결과: 선택 슬롯=2; 저장 시도 횟수=3; 저장 상태=ALT\_BOOT/BOOT\_TRY<br>
<!-- /source-cell:K1688 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1688 -->
검증 \[2\]<br>
ALT\_BOOT/BOOT\_TRY, 상한=3, 호출 전 시도 횟수=2 / 선택 슬롯=2; 저장 시도 횟수=3; 저장 상태=ALT\_BOOT/BOOT\_TRY<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1688 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1689 -->
조건 \[3\]<br>
입력/조건 \[3\]: ALT\_BOOT/BOOT\_TRY, 상한=3, 호출 전 시도 횟수=3<br>
예상 결과: 선택 슬롯=1; 저장 시도 횟수=4; 저장 상태=BOOT/TRY\_DONE/FAIL<br>
<!-- /source-cell:K1689 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1689 -->
검증 \[3\]<br>
ALT\_BOOT/BOOT\_TRY, 상한=3, 호출 전 시도 횟수=3 / 선택 슬롯=1; 저장 시도 횟수=4; 저장 상태=BOOT/TRY\_DONE/FAIL<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1689 -->


### 비교 5

#### 입력 조건과 통과 기준

<!-- source-cell:K1690 -->
조건 \[4\]<br>
입력/조건 \[4\]: ALT\_BOOT/TRY\_DONE, 결과=성공<br>
예상 결과: 선택 슬롯=2; 저장 하위 상태=TRY\_DONE<br>
<!-- /source-cell:K1690 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1690 -->
검증 \[4\]<br>
ALT\_BOOT/TRY\_DONE, 결과=성공 / 선택 슬롯=2; 저장 하위 상태=TRY\_DONE<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1690 -->


### 비교 6

#### 입력 조건과 통과 기준

<!-- source-cell:K1691 -->
조건 \[5\]<br>
입력/조건 \[5\]: ALT\_BOOT/TRY\_DONE, 결과=실패<br>
예상 결과: 선택 슬롯=1; 저장 하위 상태=UNKNOWN<br>
<!-- /source-cell:K1691 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1691 -->
검증 \[5\]<br>
ALT\_BOOT/TRY\_DONE, 결과=실패 / 선택 슬롯=1; 저장 하위 상태=UNKNOWN<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1691 -->


### 비교 7

#### 입력 조건과 통과 기준

<!-- source-cell:K1692 -->
조건 \[6\]<br>
입력/조건 \[6\]: ALT\_BOOT, 하위 상태=99(미지원)<br>
예상 결과: 선택 슬롯=1; 저장 하위 상태=UNKNOWN<br>
<!-- /source-cell:K1692 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1692 -->
검증 \[6\]<br>
ALT\_BOOT, 하위 상태=99(미지원) / 선택 슬롯=1; 저장 하위 상태=UNKNOWN<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1692 -->


### 비교 8

#### 입력 조건과 통과 기준

<!-- source-cell:K1693 -->
조건 \[7\]<br>
입력/조건 \[7\]: 전체 state=99(미지원)<br>
예상 결과: 선택 슬롯=1; 저장 state=99; 저장 하위 상태=UNKNOWN<br>
<!-- /source-cell:K1693 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1693 -->
검증 \[7\]<br>
전체 state=99(미지원) / 선택 슬롯=1; 저장 state=99; 저장 하위 상태=UNKNOWN<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1693 -->


### 비교 9

#### 입력 조건과 통과 기준

<!-- source-cell:K1694 -->
검증 \[1\]의 입력/원소별 기준 (C1): 기본 슬롯<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=1, sub\_state=1, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. 새 값으로 파일을 덮은 뒤 handler 실행과 파일 다시 읽기를 수행한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1694 -->


<!-- source-cell:K1695 -->
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 1<br>
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
<!-- /source-cell:K1695 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1694 -->
검증 \[1\]: 기본 슬롯<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 1<br>
출력\[3\] 저장 state = 1<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
<!-- /source-cell:L1694 -->


### 비교 10

#### 입력 조건과 통과 기준

<!-- source-cell:K1696 -->
검증 \[2\]의 입력/원소별 기준 (C2): 대체 세 번째 시도<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=2, alt\_boot\_try\_count=2, alt\_boot\_result=3로 저장한다. 새 값으로 파일을 덮은 뒤 handler 실행과 파일 다시 읽기를 수행한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 2<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 3<br>
<!-- /source-cell:K1696 -->


<!-- source-cell:K1697 -->
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 2<br>
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
<!-- /source-cell:K1697 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1696 -->
검증 \[2\]: 대체 세 번째 시도<br>
출력\[0\] 선택 슬롯 = 2<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 3<br>
출력\[2\] 저장 sub\_state = 2<br>
출력\[3\] 저장 state = 2<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
<!-- /source-cell:L1696 -->


### 비교 11

#### 입력 조건과 통과 기준

<!-- source-cell:K1698 -->
검증 \[3\]의 입력/원소별 기준 (C3): 대체 시도 상한 초과<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=2, alt\_boot\_try\_count=3, alt\_boot\_result=3로 저장한다. 새 값으로 파일을 덮은 뒤 handler 실행과 파일 다시 읽기를 수행한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 4<br>
<!-- /source-cell:K1698 -->


<!-- source-cell:K1699 -->
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 3<br>
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
<!-- /source-cell:K1699 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1698 -->
검증 \[3\]: 대체 시도 상한 초과<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 4<br>
출력\[2\] 저장 sub\_state = 3<br>
출력\[3\] 저장 state = 1<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
<!-- /source-cell:L1698 -->


### 비교 12

#### 입력 조건과 통과 기준

<!-- source-cell:K1700 -->
검증 \[4\]의 입력/원소별 기준 (C4): 대체 성공 유지<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=3, alt\_boot\_try\_count=0, alt\_boot\_result=2로 저장한다. 새 값으로 파일을 덮은 뒤 handler 실행과 파일 다시 읽기를 수행한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 2<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1700 -->


<!-- source-cell:K1701 -->
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 3<br>
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 2<br>
<!-- /source-cell:K1701 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1700 -->
검증 \[4\]: 대체 성공 유지<br>
출력\[0\] 선택 슬롯 = 2<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 3<br>
출력\[3\] 저장 state = 2<br>
출력\[4\] 저장 alt\_boot\_result = 2<br>
<!-- /source-cell:L1700 -->


### 비교 13

#### 입력 조건과 통과 기준

<!-- source-cell:K1702 -->
검증 \[5\]의 입력/원소별 기준 (C5): 대체 실패 후 복귀<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=3, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. 새 값으로 파일을 덮은 뒤 handler 실행과 파일 다시 읽기를 수행한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1702 -->


<!-- source-cell:K1703 -->
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 4<br>
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
<!-- /source-cell:K1703 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1702 -->
검증 \[5\]: 대체 실패 후 복귀<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 4<br>
출력\[3\] 저장 state = 2<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
<!-- /source-cell:L1702 -->


### 비교 14

#### 입력 조건과 통과 기준

<!-- source-cell:K1704 -->
검증 \[6\]의 입력/원소별 기준 (C6): 미지원 하위 상태<br>
시험 입력 (모의 I/O/임시 파일)<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=99, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. 새 값으로 파일을 덮은 뒤 handler 실행과 파일 다시 읽기를 수행한다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
<!-- /source-cell:K1704 -->


<!-- source-cell:K1705 -->
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 4<br>
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
<!-- /source-cell:K1705 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1704 -->
검증 \[6\]: 미지원 하위 상태<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 4<br>
출력\[3\] 저장 state = 2<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
<!-- /source-cell:L1704 -->


### 비교 15

#### 입력 조건과 통과 기준

<!-- source-cell:K1706 -->
검증 \[101\]의 입력/원소별 기준 (C101): 기본 슬롯<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=1, sub\_state=1, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 1<br>
<!-- /source-cell:K1706 -->


<!-- source-cell:K1707 -->
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1707 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1706 -->
검증 \[101\]: 기본 슬롯<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 1<br>
출력\[3\] 저장 state = 1<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1706 -->


### 비교 16

#### 입력 조건과 통과 기준

<!-- source-cell:K1708 -->
검증 \[102\]의 입력/원소별 기준 (C102): 대체 세 번째 시도<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=2, alt\_boot\_try\_count=2, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 2<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 3<br>
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 2<br>
<!-- /source-cell:K1708 -->


<!-- source-cell:K1709 -->
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1709 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1708 -->
검증 \[102\]: 대체 세 번째 시도<br>
출력\[0\] 선택 슬롯 = 2<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 3<br>
출력\[2\] 저장 sub\_state = 2<br>
출력\[3\] 저장 state = 2<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1708 -->


### 비교 17

#### 입력 조건과 통과 기준

<!-- source-cell:K1710 -->
검증 \[103\]의 입력/원소별 기준 (C103): 대체 시도 상한 초과<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=2, alt\_boot\_try\_count=3, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 4<br>
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 3<br>
<!-- /source-cell:K1710 -->


<!-- source-cell:K1711 -->
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 1<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1711 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1710 -->
검증 \[103\]: 대체 시도 상한 초과<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 4<br>
출력\[2\] 저장 sub\_state = 3<br>
출력\[3\] 저장 state = 1<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1710 -->


### 비교 18

#### 입력 조건과 통과 기준

<!-- source-cell:K1712 -->
검증 \[104\]의 입력/원소별 기준 (C104): 대체 성공 유지<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=3, alt\_boot\_try\_count=0, alt\_boot\_result=2로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 2<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 3<br>
<!-- /source-cell:K1712 -->


<!-- source-cell:K1713 -->
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 2<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1713 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1712 -->
검증 \[104\]: 대체 성공 유지<br>
출력\[0\] 선택 슬롯 = 2<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 3<br>
출력\[3\] 저장 state = 2<br>
출력\[4\] 저장 alt\_boot\_result = 2<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1712 -->


### 비교 19

#### 입력 조건과 통과 기준

<!-- source-cell:K1714 -->
검증 \[105\]의 입력/원소별 기준 (C105): 대체 실패 후 복귀<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=3, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 4<br>
<!-- /source-cell:K1714 -->


<!-- source-cell:K1715 -->
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1715 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1714 -->
검증 \[105\]: 대체 실패 후 복귀<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 4<br>
출력\[3\] 저장 state = 2<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1714 -->


### 비교 20

#### 입력 조건과 통과 기준

<!-- source-cell:K1716 -->
검증 \[106\]의 입력/원소별 기준 (C106): 미지원 하위 상태<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=99, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 4<br>
<!-- /source-cell:K1716 -->


<!-- source-cell:K1717 -->
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 2<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1717 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1716 -->
검증 \[106\]: 미지원 하위 상태<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 4<br>
출력\[3\] 저장 state = 2<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1716 -->


### 비교 21

#### 입력 조건과 통과 기준

<!-- source-cell:K1718 -->
검증 \[107\]의 입력/원소별 기준 (C107): 미지원 전체 상태<br>
초기화/입력/동작: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=99, sub\_state=1, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
payload 인덱스: 0<br>
의미/단위: 선택 슬롯<br>
사전 예상값: 1<br>
payload 인덱스: 1<br>
의미/단위: 저장 alt\_boot\_try\_count (회)<br>
사전 예상값: 0<br>
payload 인덱스: 2<br>
의미/단위: 저장 sub\_state<br>
사전 예상값: 4<br>
<!-- /source-cell:K1718 -->


<!-- source-cell:K1719 -->
payload 인덱스: 3<br>
의미/단위: 저장 state<br>
사전 예상값: 99<br>
payload 인덱스: 4<br>
의미/단위: 저장 alt\_boot\_result<br>
사전 예상값: 3<br>
payload 인덱스: 5<br>
의미/단위: handle 닫힘 (0=열림,1=닫힘)<br>
사전 예상값: 1<br>
<!-- /source-cell:K1719 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1718 -->
검증 \[107\]: 미지원 전체 상태<br>
출력\[0\] 선택 슬롯 = 1<br>
출력\[1\] 저장 alt\_boot\_try\_count (회) = 0<br>
출력\[2\] 저장 sub\_state = 4<br>
출력\[3\] 저장 state = 99<br>
출력\[4\] 저장 alt\_boot\_result = 3<br>
출력\[5\] handle 닫힘 (0=열림,1=닫힘) = 1<br>
<!-- /source-cell:L1718 -->


### 비교 22

#### 입력 조건과 통과 기준

<!-- source-cell:K1720 -->
검증 \[101\]의 RTT 출력 (C101)<br>
입력/조건: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=1, sub\_state=1, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-020] C101 A[0:5]=1,0,1,1,3,1
[UT-BOOT-020] C101 PASS N=6 F=0
```
<!-- /source-cell:K1720 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1720 -->

```text
[UT-BOOT-020] C101 A[0:5]=1,0,1,1,3,1
[UT-BOOT-020] C101 PASS N=6 F=0
```
<!-- /source-cell:L1720 -->


### 비교 23

#### 입력 조건과 통과 기준

<!-- source-cell:K1721 -->
검증 \[102\]의 RTT 출력 (C102)<br>
입력/조건: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=2, alt\_boot\_try\_count=2, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-020] C102 A[0:5]=2,3,2,2,3,1
[UT-BOOT-020] C102 PASS N=6 F=0
```
<!-- /source-cell:K1721 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1721 -->

```text
[UT-BOOT-020] C102 A[0:5]=2,3,2,2,3,1
[UT-BOOT-020] C102 PASS N=6 F=0
```
<!-- /source-cell:L1721 -->


### 비교 24

#### 입력 조건과 통과 기준

<!-- source-cell:K1722 -->
검증 \[103\]의 RTT 출력 (C103)<br>
입력/조건: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=2, alt\_boot\_try\_count=3, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-020] C103 A[0:5]=1,4,3,1,3,1
[UT-BOOT-020] C103 PASS N=6 F=0
```
<!-- /source-cell:K1722 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1722 -->

```text
[UT-BOOT-020] C103 A[0:5]=1,4,3,1,3,1
[UT-BOOT-020] C103 PASS N=6 F=0
```
<!-- /source-cell:L1722 -->


### 비교 25

#### 입력 조건과 통과 기준

<!-- source-cell:K1723 -->
검증 \[104\]의 RTT 출력 (C104)<br>
입력/조건: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=3, alt\_boot\_try\_count=0, alt\_boot\_result=2로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-020] C104 A[0:5]=2,0,3,2,2,1
[UT-BOOT-020] C104 PASS N=6 F=0
```
<!-- /source-cell:K1723 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1723 -->

```text
[UT-BOOT-020] C104 A[0:5]=2,0,3,2,2,1
[UT-BOOT-020] C104 PASS N=6 F=0
```
<!-- /source-cell:L1723 -->


### 비교 26

#### 입력 조건과 통과 기준

<!-- source-cell:K1724 -->
검증 \[105\]의 RTT 출력 (C105)<br>
입력/조건: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=3, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-020] C105 A[0:5]=1,0,4,2,3,1
[UT-BOOT-020] C105 PASS N=6 F=0
```
<!-- /source-cell:K1724 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1724 -->

```text
[UT-BOOT-020] C105 A[0:5]=1,0,4,2,3,1
[UT-BOOT-020] C105 PASS N=6 F=0
```
<!-- /source-cell:L1724 -->


### 비교 27

#### 입력 조건과 통과 기준

<!-- source-cell:K1725 -->
검증 \[106\]의 RTT 출력 (C106)<br>
입력/조건: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=2, sub\_state=99, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-020] C106 A[0:5]=1,0,4,2,3,1
[UT-BOOT-020] C106 PASS N=6 F=0
```
<!-- /source-cell:K1725 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1725 -->

```text
[UT-BOOT-020] C106 A[0:5]=1,0,4,2,3,1
[UT-BOOT-020] C106 PASS N=6 F=0
```
<!-- /source-cell:L1725 -->


### 비교 28

#### 입력 조건과 통과 기준

<!-- source-cell:K1726 -->
검증 \[107\]의 RTT 출력 (C107)<br>
입력/조건: 각 건 상태 64바이트를 0으로 초기화하고 boot\_slot\_num=1, alt\_boot\_slot\_num=2, state=99, sub\_state=1, alt\_boot\_try\_count=0, alt\_boot\_result=3로 저장한다. RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.<br>
예상 RTT 출력 결과:<br>

```text
[UT-BOOT-020] C107 A[0:5]=1,0,4,99,3,1
[UT-BOOT-020] C107 PASS N=6 F=0
```
<!-- /source-cell:K1726 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1726 -->

```text
[UT-BOOT-020] C107 A[0:5]=1,0,4,99,3,1
[UT-BOOT-020] C107 PASS N=6 F=0
```
<!-- /source-cell:L1726 -->


## 보고서 판정

<!-- source-cell:M1686 -->
통과<br>
<!-- /source-cell:M1686 -->
