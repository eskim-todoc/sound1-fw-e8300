---
name: G2 fs 게이트 노트
purpose: G2(Gen1_5/FS -> fs/) 파일·심볼 매핑, 저장 포맷 판단(중대), 유닛맵, 검증 기록
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g2, fs, filesystem, record-format]
---

# G2 — fs (파일시스템 계층)

**TL;DR**: **⑤ 계획의 전제가 실측으로 부정확함이 드러나 범위를 축소한다.** 계획은 "`ci_stim_mute` = magic+version+size+crc 패턴 → `tdc_fs_record` 표준으로 승격"이라 했으나, 실제 `ci_stim_mute` 는 **magic 센티널 2개뿐**(version·size·crc 없음)이고, `ci_filesystem` 에는 **이미 CRC+AES128 레코드 계층이 존재**한다. 신규 표준 도입은 ① 세 번째 패턴 추가 ② 저장 포맷 변경 = 기기 내 기존 데이터 비호환 → **엄격 보존(요구사항_8) 위반**. 따라서 **G2 는 이동·rename 만 수행**하고 레코드 표준화는 별도 작업으로 분리 제안한다.

## 1. 실측 — FS 저장 패턴 3종 (계획 §5 정정)

| 패턴 | 구현 | 소비자 | 무결성 |
|---|---|---|---|
| **A. CRC+AES128** | `ci_filesystem_{read,write}_with_crc_and_aes128()` | `ci_map.c` (ISD info · user setting · map stamp · map data) — **제품 핵심 데이터** | CRC-CCITT + AES128 암호화 |
| **B. magic 센티널** | `CI_STIM_MUTE_T` = `file_ident_begin`(0x12345678) + payload 2 + `file_ident_end`(0x87654321), 구조체 직접 `f_read`/`f_write` | `ci_stim_mute.c` | **CRC 없음** |
| **C. raw 구조체** | `f_open`/`f_read`/`f_write` 직접, `CI_EVENT_LOG_T` 통짜 | `ci_event_log.c` | 없음 |

> [!IMPORTANT]
> **계획 §5 의 "`ci_stim_mute` 패턴(magic+version+size+crc)" 서술은 오류다.** 그 스펙은 선례로 인용한 `touch/20260701_touch-setting-file-persist` **설계 문서의 제안**이었고, `ci_stim_mute` 는 그중 magic 센티널만 구현한 상태다. ③ 분석이 헤더 실물을 확인하지 않고 선례 문서를 근거로 삼은 것이 원인.

## 2. 범위 결정

### 이번 게이트 (수행)

- `Gen1_5/FS/` → `fs/` 이동, `ci_*` → `tdc_fs_*` rename
- `.cproject` include 경로 갱신
- **저장 포맷·파일명 문자열·구조체 레이아웃 전부 불변** (기기 내 기존 데이터 호환 유지)

### 분리 (별도 작업 제안)

**레코드 표준화(`tdc_fs_record`)** — 포맷 변경이 수반되므로 독립 작업으로:
- 마이그레이션 설계(구 포맷 읽기 호환 또는 버전 필드 도입) 필요
- 패턴 A(CRC+AES128, 제품 핵심)를 표준으로 승격할지, B/C 를 A 로 이관할지 결정 필요
- 실기 데이터 손실 위험 검증 필요

### 이월 판단 (조사 완료, 조치는 후속)

| 항목 | 판단 |
|---|---|
| `99_eeprom_address.h` (G0 이월) | 포함처 3곳(`fn_from_cfx_eeprom_read.h` · `ci_filesystem.h` · `systemControl/cfx_cm3_shared_Memory_Addr.h`) — EEPROM **주소 맵 상수**. `fs` 와 `cfx_link`(G8) 양쪽이 소비하므로 **G8 에서 `cfx_link` 와 함께 처분**(단독 이동 시 G8 재이동 발생) |
| `ci_boot` 계보 (계획 위임) | bootloader 사본과 **내용 상이**(독립 진화 확인). 계보 단절 우려 없음 → **G4 이후 `boot/` 도메인에서 rename**. FS 소비자이나 `f_open` 직접 사용이라 fs 이동 대상 아님 |
| `ci_fft.c` (622줄) | **FS 폴더에 오배치** — FFT pass-bin 데이터의 파일 I/O 를 겸함. 신호처리 성격이나 파일 접근이 본체라 이번엔 `fs/` 로 함께 이동, 최종 소속은 G7(stim) 에서 재판단 |

## 3. 파일 매핑

| 현행 | 신규 |
|---|---|
| `Gen1_5/FS/ci_filesystem.c/.h` | `fs/tdc_fs.c/.h` |
| `Gen1_5/FS/ci_map.c/.h` | `fs/tdc_fs_map.c/.h` |
| `Gen1_5/FS/ci_stim_mute.c/.h` | `fs/tdc_fs_stim_mute.c/.h` |
| `Gen1_5/FS/ci_event_log.c/.h` | `fs/tdc_fs_event_log.c/.h` |
| `Gen1_5/FS/ci_fft.c/.h` | `fs/tdc_fs_fft.c/.h` (소속 재판단 → G7) |

## 4. 유닛/모듈 맵

| 모듈 | 유닛 | 검증 라벨 |
|---|---|---|
| fs | `tdc_fs`(마운트·read/write·CRC/AES 레코드) · `tdc_fs_map`(맵 데이터 CRUD) · `tdc_fs_stim_mute` · `tdc_fs_event_log` · `tdc_fs_fft` | **[로직설명]** — 이번 게이트는 rename/이동뿐, 로직·포맷 무변경 |

의존: `tdc_fs_map` · `stim_mute` · `event_log` · `fft` → **`tdc_fs` 통과 전제** (마운트·저수준 I/O 선행). `tdc_fs` → G1 util(`tdc_crc`) · lib(AES) 통과 전제.

## 5. 검증

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존 0) | ✅ 11패턴(`ci_filesystem`·`ci_map_`·`ci_stim_mute`·`ci_event_log`·`ci_fft`·`snd_fatfs`·`CI_*_` 4종·`g_ci_filesystem`) 전부 0. `@file` 주석 헤더 4건도 신규 파일명으로 정정 |
| 검증_공통_2 (공유 ABI 무변경) | ✅ `cfx_cm3_sharedMemory.h` diff = include 파일명뿐 |
| 검증_공통_3 (균형·훅) | ✅ fs 10파일 브레이스·전처리기 균형 OK |
| 검증_공통_4 (사장 실증) | ✅ 공개 함수 68개 전수 → **2건 제거**(`tdc_fs_fatfs_remount_twice` · `tdc_fs_remount`). §6 스크립트 결함 참조 |
| **검증_G2_특화 (저장 포맷 불변)** | ✅ `"STIM_MUTE.TXT"` · `"EVENTLOG.TXT"` · magic `0x12345678`/`0x87654321` · 레코드 구조체 필드 전부 무변경 (심볼명만 `CI_`→`TDC_FS_`) |
| 검증_공통_5 (빌드·실기) | ✅ PASS — 은수님 확인 (2026-07-21): "빌드 후 동작은 한다" → **G2 폐쇄**. 단 **정밀 동작 검증은 은수님이 별도 시간에 수행 예정**(맵 데이터 read/write·게인 저장/복원). 미검출 회귀 발견 시 소급 대응 |

## 6. 실증 스크립트 결함 (기록)

1차 사장 검출 스크립트가 `return tdc_fs_read_with_crc_and_aes128(` 같은 **호출문을 정의로 오분류**해 실사용 함수 4개를 "참조 0"으로 보고했다(`[\w\s\*]*?` 가 `return ` 을 타입 나열로 매칭). 직접 grep 대조에서 발견 → 첫 토큰 키워드 검사 + 대입/괄호 선행 검사로 수정 후 재실증. **G1 의 static 테이블 참조 미검사(도구결함_2)와 같은 계열** — 자동 검출 결과는 반드시 직접 grep 으로 교차 확인한다.

## 7. 2차 파생 (상한 규칙 — 제거 안 함)

`tdc_fs_remount` 제거로 **`tdc_fs_mount` 이 호출 0** 이 됐다(유일 호출자였음). 1차 파생 상한(요구사항-검증 결함_2)에 따라 이번엔 기록만 하고, 후속 게이트 또는 별도 정리 작업에서 판단한다.
