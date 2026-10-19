# BOOT-003 정상 기동과 서비스 모드 선택

시험 ID: `SW-FW-UnitTest-BOOT-ID-3`  
원본: [유닛 검증 보고서 1차본](../../SUVR-FW-TD24A%20%EC%86%8C%ED%94%84%ED%8A%B8%EC%9B%A8%EC%96%B4%20%EC%9C%A0%EB%8B%9B%20%EA%B2%80%EC%A6%9D%20%EB%B3%B4%EA%B3%A0%EC%84%9C_1%EC%B0%A8%EB%B3%B8.xlsx), `보고서_유닛 시험` 1305~1307행.  
아래 예상 결과는 보고서에 기록된 결과를 그대로 옮긴 비교값이다.

## 유닛 설명

<!-- source-cell:E1305 -->
점검용 서비스 실행과 일반 응용 실행을 분리한다.<br>
<!-- /source-cell:E1305 -->


## 시험 방법

<!-- source-cell:F1305 -->
A<br>
<!-- /source-cell:F1305 -->


## 상세 설계 유닛

<!-- source-cell:G1305 -->
SU-ID-BOOT-003<br>
(정상 기동과 서비스 모드 선택)<br>
<!-- /source-cell:G1305 -->


## 시험 준비와 입력

<!-- source-cell:H1305 -->
- 서비스 진입 조건false/true를 각각 고정하여 기동 분기를 분석한다.<br>
<!-- /source-cell:H1305 -->


## 가정 및 제한 조건

<!-- source-cell:I1305 -->
코드 분석으로 판정한다. 실제 핀 파형, 전원, 처리 시간 및 전극 전류의 실측과 구분한다.<br>
<!-- /source-cell:I1305 -->


## 시험 절차

<!-- source-cell:J1305 -->
- 준비한 각 입력과 초기 상태에 대해 조건 분기, 호출 순서, 인자, 반환값 및 상태 변경을 코드에서 확인한다. 아래 예상 결과와 대조하고 검토한 조건 및 근거를 기록한다.<br>
<!-- /source-cell:J1305 -->


## 입력 조건별 기준과 예상 결과

### 비교 1

#### 입력 조건과 통과 기준

<!-- source-cell:K1305 -->
판정 범위<br>
- false는 정상 부트, true는 service\_main만 선택한다. 서비스 경로를 정상 자극 루프로 연결하지 않는다.<br>
<!-- /source-cell:K1305 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1305 -->
검증 결과<br>
서비스 진입 조건=false, 첫 CAL 읽기=1(비활성) / 선택 경로=공유 int\[0\]에 0 기록 후 bootloader\_main; service\_main 호출=없음<br>
서비스 진입 조건=true, CAL 읽기=0 → 0(활성 유지) / 선택 경로=service\_main; bootloader\_main 호출=없음; 정상 자극 루프 연결=없음<br>
<!-- /source-cell:L1305 -->


### 비교 2

#### 입력 조건과 통과 기준

<!-- source-cell:K1306 -->
조건 \[1\]<br>
입력/조건 \[1\]: 서비스 진입 조건=false, 첫 CAL 읽기=1(비활성)<br>
예상 결과: 선택 경로=공유 int\[0\]에 0 기록 후 bootloader\_main; service\_main 호출=없음<br>
<!-- /source-cell:K1306 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1306 -->
검증 \[1\]<br>
서비스 진입 조건=false, 첫 CAL 읽기=1(비활성) / 선택 경로=공유 int\[0\]에 0 기록 후 bootloader\_main; service\_main 호출=없음<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1306 -->


### 비교 3

#### 입력 조건과 통과 기준

<!-- source-cell:K1307 -->
조건 \[2\]<br>
입력/조건 \[2\]: 서비스 진입 조건=true, CAL 읽기=0 → 0(활성 유지)<br>
예상 결과: 선택 경로=service\_main; bootloader\_main 호출=없음; 정상 자극 루프 연결=없음<br>
<!-- /source-cell:K1307 -->


#### 예상 결과 (보고서 기록값)

<!-- source-cell:L1307 -->
검증 \[2\]<br>
서비스 진입 조건=true, CAL 읽기=0 → 0(활성 유지) / 선택 경로=service\_main; bootloader\_main 호출=없음; 정상 자극 루프 연결=없음<br>
판정: 출력/동작이 기준과 일치하여 통과.<br>
<!-- /source-cell:L1307 -->


## 보고서 판정

<!-- source-cell:M1305 -->
통과<br>
<!-- /source-cell:M1305 -->
