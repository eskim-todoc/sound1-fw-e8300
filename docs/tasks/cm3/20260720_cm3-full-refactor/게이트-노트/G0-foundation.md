---
name: G0 foundation 게이트 노트
purpose: G0(foundation) 범위·조치·검증 기록 - 보류 동결·루트 헤더 처분·오타 정정·ABI 주석·규약 문서
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g0, foundation]
---

# G0 — foundation

**TL;DR**: 코드 로직 무변경 게이트. 보류 동결 목록(70종) 산출, `99_errorCode.h` 삭제(포함 0 실증), 오타 파일명 3건 정정(+include 15파일), 공유 ABI 경고 주석, `tdc_convention.md` 규약 문서, 루트 지침 `tdc_hal_` 행 추가(rules repo). **유닛/모듈 맵 해당 없음**(로직 변경 없는 문서·정리 게이트 — [로직설명] 라벨로 갈음).

## 조치 내역

| # | 조치 | 근거 |
|---|---|---|
| 1 | 보류 동결 목록 산출 — `processorDirective.h` 39 + `definitionsForAlgorithm.h` 31 = **70종** | [`08_보류-동결-목록.md`](../분석-데이터/08_보류-동결-목록.md). `definitionsForAlgorithm.h` 는 루트 잔여 `signalProcessing/` 에서 발견 |
| 2 | `99_errorCode.h` 삭제 | 포함 0 (유일 참조 = `error.h:12` 주석). 내용 2건(NoError=0 · clock_init_Error=1)은 `error.h` 주석으로 이전 |
| 3 | 오타 파일명 정정: `dirver_PCM.h`→`driver_PCM.h` · `dirver_i2c_for_ISD.c/.h`→`driver_*` | git mv + include 15파일 갱신, 잔존 `dirver` 0 |
| 4 | 공유 ABI 경고 주석 | `cfx_cm3_sharedMemory.h` 상단 — 단독 rename/제거 금지 + 근거 문서 링크 |
| 5 | `src/2__cm3/tdc_convention.md` 신설 | 규약 단일 참조점 (네이밍·약어표·불가침·FSM·파일처리·인코딩) |
| 6 | 루트 지침 `tdc_hal_<periph>_` 행 추가 | rules repo `858e6fb` (승인_3 에 병합 포함 승인됨) |

## 이월 항목

| 항목 | 이월처 | 사유 |
|---|---|---|
| `99_eeprom_address.h` 처분 (포함 3) | G2 (fs) | EEPROM 주소 맵 = fs 도메인 자산 |
| `processorDirective.h` → `tdc_config.h` rename | 후속 게이트 | 포함 22 전역 파급 — 동결 70종 확정 후 전용 심볼만 |
| 루트 잔여 디렉토리 이동 (`systemControl/`·`isdExecution/`·`signalProcessing/`) | 소속 게이트 (G4/G7/G8) | 게이트 자기완결성 (계획 §3 원칙) |
| 골격 폴더 생성 | 각 게이트 이동 시 | git 은 빈 폴더 미추적 |

## 검증

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존) | ✅ `dirver` 잔존 0 · `99_errorCode` 참조 = 이력 주석뿐 |
| 검증_공통_2 (공유 ABI 무변경) | ✅ `cfx_cm3_sharedMemory.h` diff = include 오타 정정 + 주석 블록뿐, 구조체 무변경 |
| 검증_공통_3 (균형·훅) | ✅ 커밋 성립 (pre-commit 인코딩 훅 통과) |
| 검증_공통_4 (사장 실증) | ✅ `99_errorCode.h` 포함 0 grep 실증 |
| 검증_공통_5 (빌드·실기) | ✅ PASS — 은수님 확인 (2026-07-20): "빌드 잘 되고 동작도 문제 없어". fake-sleep 실기 게이트도 함께 통과 → **G0 폐쇄** |
| [로직설명] | 본 게이트는 실행 코드 로직 무변경 (파일명·include·주석·문서·삭제뿐) |
