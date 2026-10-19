# BOOT-005 응용 코어 기동

시험 ID: `SW-FW-UnitTest-BOOT-ID-5`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1319~1322행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1319 -->
적재된 프로그램의 시작 주소로 제어를 넘긴다.<br>
<!-- /source-cell:E1319 -->


## 시험 방법

<!-- source-cell:F1319 -->
A<br>
<!-- /source-cell:F1319 -->


## 상세 설계 유닛

<!-- source-cell:G1319 -->
SU-ID-BOOT-005<br>
(응용 코어 기동)<br>
<!-- /source-cell:G1319 -->


## 시험 준비와 입력

<!-- source-cell:H1319 -->
- CM3/CFX 기동 함수의 시작 주소 설정과 실행 전 조건을 확인한다.<br>
<!-- /source-cell:H1319 -->


## 가정 및 제한 조건

<!-- source-cell:I1319 -->
코드 분석으로 판정한다. 실제 핀 파형, 전원, 처리 시간 및 전극 전류의 실측과 구분한다.<br>
<!-- /source-cell:I1319 -->


## 시험 절차

<!-- source-cell:J1319 -->
- 준비한 각 입력과 초기 상태에 대해 조건 분기, 호출 순서, 인자, 반환값 및 상태 변경을 코드에서 확인한다. 아래 예상 결과와 대조하고 검토한 조건 및 근거를 기록한다.<br>
<!-- /source-cell:J1319 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1319 -->
판정 범위<br>
- 각 코어는 해당 이미지 시작 정보로 실행되어야 하고 적재 절차보다 먼저 기동하지 않아야 한다.<br>
<!-- /source-cell:K1319 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1319 -->
검증 결과<br>
이미지 적재 완료, CM3 기동 플래그=0xA5 / 기동 순서=SP에 이미지 stack\_pointer 설정 → BX로 이미지 init\_pointer 분기; 적재 전 기동=없음<br>
CFX mailbox 요청=PCREQ, 시작 PC=0x8000/0x18000 / 반환값=0; 처리 순서=메모리 접근 허용 → 시작 주소/PCKEY 게시 → CFX\_CMD\_0 요청<br>
<!-- /source-cell:L1319 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1320 -->
조건 \[1\]<br>
입력/조건 \[1\]: 이미지 적재 완료, CM3 기동 플래그=0xA5<br>
예상 결과: 기동 순서=SP에 이미지 stack\_pointer 설정 → BX로 이미지 init\_pointer 분기; 적재 전 기동=없음<br>
<!-- /source-cell:K1320 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1320 -->
검증 \[1\]<br>
이미지 적재 완료, CM3 기동 플래그=0xA5 / 기동 순서=SP에 이미지 stack\_pointer 설정 → BX로 이미지 init\_pointer 분기; 적재 전 기동=없음<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1320 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1321 -->
조건 \[2\]<br>
입력/조건 \[2\]: CFX mailbox 요청=PCREQ, 시작 PC=0x8000/0x18000<br>
예상 결과: 반환값=0; 처리 순서=메모리 접근 허용 → 시작 주소/PCKEY 게시 → CFX\_CMD\_0 요청<br>
<!-- /source-cell:K1321 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1321 -->
검증 \[2\]<br>
CFX mailbox 요청=PCREQ, 시작 PC=0x8000/0x18000 / 반환값=0; 처리 순서=메모리 접근 허용 → 시작 주소/PCKEY 게시 → CFX\_CMD\_0 요청<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1321 -->


### 비교 4

#### 입력 조건과 통과 기준

<!-- source-cell:K1322 -->
조건 \[3\]<br>
입력/조건 \[3\]: CFX 시작 PC=0x7FFF/0x18001 또는 PCREQ 불일치<br>
예상 결과: 반환값=-1; CFX 기동 요청=없음<br>
<!-- /source-cell:K1322 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1322 -->
검증 \[3\]<br>
CFX 시작 PC=0x7FFF/0x18001 또는 PCREQ 불일치 / 반환값=-1; CFX 기동 요청=없음<br>
<!-- /source-cell:L1322 -->


## 보고서 판정

<!-- source-cell:M1319 -->
통과<br>
<!-- /source-cell:M1319 -->
