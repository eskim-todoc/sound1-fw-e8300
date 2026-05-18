---
name: 작업 모듈 인덱스
purpose: Sound1 tasks 모듈별 최종 업데이트 인덱스 (Wiki sync 진입판단용)
type: tasks/메타
applies_to: [Sound1]
tags: [meta, tasks, index, wiki-sync]
---

# 작업 모듈 인덱스 — Sound1

**TL;DR**: Sound1의 `docs/tasks/` 하위 모듈별 최종 업데이트 날짜 인덱스. Task α (2026-05-15)부터 archive 폐지 — 작업 폴더는 완료 후에도 `tasks/<모듈>/<작업>/` 그대로. Wiki 등 외부 소비자는 본 파일만 읽어 마지막 sync 시점 이후 변경된 모듈 식별 가능. 갱신은 Claude의 작업 종료 흐름에서 자동 수행 ([`작업 진행 규칙.md §6.1`](../../../../docs/지침/일반/작업%20진행%20규칙.md)).

## 운영 메모

- 정렬: **최종 업데이트 내림차순** → 동률 시 **작업 수 내림차순** → 그래도 동률이면 모듈명 사전순
- 갱신 트리거: 작업 종료 처리 시 — [`작업 진행 규칙.md §6.1`](../../../../docs/지침/일반/작업%20진행%20규칙.md) 3번 단계가 본 파일 자동 갱신
- 추적 범위: 작업 **신규/완료**. 기존 작업 파일 *수정*은 추적 대상 아님

## 모듈 인덱스

| 모듈 | 최종 업데이트 | 최근 작업 | 작업 수 |
|---|---|---|---|
| signalProcessing | 2026-05-14 | [nofm-tuning](signalProcessing/nofm-tuning/) | 3 |
| ble | 2026-05-14 | [qcc-shutdown-delay](ble/qcc-shutdown-delay/) | 1 |
| sync | 2026-05-14 | [sullivan1.5-rel2](sync/sullivan1.5-rel2/) | 1 |
| ui | 2026-05-14 | [program-change-command](ui/program-change-command/) | 1 |
| LED | 2026-05-08 | [ota-dfu-fw-variant](LED/ota-dfu-fw-variant/) | 5 |
| meta | 2026-05-08 | [claude-md-slim](meta/claude-md-slim/) | 5 |
| touch | 2026-05-08 | [board-variant-iqs323-ati](touch/board-variant-iqs323-ati/) | 3 |
| bootloader | 2026-05-08 | [service-rtt-debug-console](bootloader/service-rtt-debug-console/) | 1 |
| power | 2026-05-07 | [low-power-mode](power/low-power-mode/) | 1 |

## 작업 상세

### ble

| 작업 | 완료일 | 폴더 |
|---|---|---|
| qcc-shutdown-delay | 2026-05-14 | [ble/qcc-shutdown-delay](ble/qcc-shutdown-delay/) |

### bootloader

| 작업 | 완료일 | 폴더 |
|---|---|---|
| service-rtt-debug-console | 2026-05-07 | [bootloader/service-rtt-debug-console](bootloader/service-rtt-debug-console/) |

### LED

| 작업 | 완료일 | 폴더 |
|---|---|---|
| bootloader-power-on-indicator | 2026-04-27 | [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) |
| non-blocking-fade-off | 2026-04-27 | [LED/non-blocking-fade-off](LED/non-blocking-fade-off/) |
| operation-scheme | 2026-04-15 | [LED/operation-scheme](LED/operation-scheme/) |
| ota-dfu-fw-variant | 2026-05-08 | [LED/ota-dfu-fw-variant](LED/ota-dfu-fw-variant/) |
| power-on-early-lighting | 2026-04-23 | [LED/power-on-early-lighting](LED/power-on-early-lighting/) |

### meta

| 작업 | 완료일 | 폴더 |
|---|---|---|
| claude-md-slim | 2026-05-08 | [meta/claude-md-slim](meta/claude-md-slim/) |
| docs-restructure | 2026-04-24 | [meta/docs-restructure](meta/docs-restructure/) |
| folder-readmes | 2026-05-07 | [meta/folder-readmes](meta/folder-readmes/) |
| frontmatter-retrofit | 2026-05-07 | [meta/frontmatter-retrofit](meta/frontmatter-retrofit/) |
| internal-links-fix | 2026-05-07 | [meta/internal-links-fix](meta/internal-links-fix/) |

### power

| 작업 | 완료일 | 폴더 |
|---|---|---|
| low-power-mode | 2026-05-07 | [power/low-power-mode](power/low-power-mode/) |

### signalProcessing

| 작업 | 완료일 | 폴더 |
|---|---|---|
| cfx-cm3-analysis | 2026-05-14 | [signalProcessing/cfx-cm3-analysis](signalProcessing/cfx-cm3-analysis/) |
| nofm-merge | 2026-05-14 | [signalProcessing/nofm-merge](signalProcessing/nofm-merge/) |
| nofm-tuning | 2026-05-14 | [signalProcessing/nofm-tuning](signalProcessing/nofm-tuning/) |

### sync

| 작업 | 완료일 | 폴더 |
|---|---|---|
| sullivan1.5-rel2 | 2026-05-14 | [sync/sullivan1.5-rel2](sync/sullivan1.5-rel2/) |

### touch

| 작업 | 완료일 | 폴더 |
|---|---|---|
| board-variant-iqs323-ati | 2026-05-08 | [touch/board-variant-iqs323-ati](touch/board-variant-iqs323-ati/) |
| init-split | 2026-05-07 | [touch/init-split](touch/init-split/) |
| layer-separation | 2026-05-07 | [touch/layer-separation](touch/layer-separation/) |

### ui

| 작업 | 완료일 | 폴더 |
|---|---|---|
| program-change-command | 2026-05-14 | [ui/program-change-command](ui/program-change-command/) |
