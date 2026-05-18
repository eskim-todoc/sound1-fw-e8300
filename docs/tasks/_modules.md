---
name: Archive 모듈 인덱스
purpose: Sound1 archive 모듈별 최종 업데이트 인덱스 (Wiki sync 진입판단용)
type: tasks/메타
applies_to: [Sound1]
tags: [meta, archive, index, wiki-sync]
---

# Archive 모듈 인덱스 — Sound1

**TL;DR**: Sound1의 `_archive/` 하위 모듈별 최종 업데이트 날짜 인덱스. Wiki 등 외부 소비자는 본 파일만 읽어 마지막 sync 시점 이후 변경된 모듈만 식별 가능. 갱신은 Claude의 archive 처리 흐름에서 자동 수행 ([`작업 진행 규칙.md §6.1`](../../../../../docs/지침/일반/작업%20진행%20규칙.md)).

## 운영 메모

- 정렬: **최종 업데이트 내림차순** → 동률 시 **작업 수 내림차순** → 그래도 동률이면 모듈명 사전순
- 갱신 트리거: 작업 archive 처리 시 — [`작업 진행 규칙.md §6.1`](../../../../../docs/지침/일반/작업%20진행%20규칙.md) 5번 단계가 본 파일 자동 갱신
- 추적 범위: archive **신규/갱신**. 기존 archive 파일 *수정*은 추적 대상 아님

## 모듈 인덱스

| 모듈 | 최종 업데이트 | 최근 작업 | 작업 수 |
|---|---|---|---|
| signalProcessing | 2026-05-14 | [nofm-tuning](signalProcessing/nofm-tuning/) | 3 |
| ble | 2026-05-14 | [qcc-shutdown-delay](ble/qcc-shutdown-delay/) | 1 |
| sync | 2026-05-14 | [sullivan1.5-rel2](sync/sullivan1.5-rel2/) | 1 |
| ui | 2026-05-14 | [program-change-command](ui/program-change-command/) | 1 |
| LED | 2026-05-08 | [bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) | 5 |
| meta | 2026-05-08 | [claude-md-slim](meta/claude-md-slim/) | 5 |
| touch | 2026-05-08 | [board-variant-iqs323-ati](touch/board-variant-iqs323-ati/) | 3 |
| bootloader | 2026-05-08 | [service-rtt-debug-console](bootloader/service-rtt-debug-console/) | 1 |
| power | 2026-05-07 | [low-power-mode](power/low-power-mode/) | 1 |

## 작업 상세 (Task α 이후 신규)

### ble

| 작업 | 완료일 | 폴더 |
|---|---|---|
| qcc-shutdown-delay | — | [ble/qcc-shutdown-delay](ble/qcc-shutdown-delay/) |

### bootloader

| 작업 | 완료일 | 폴더 |
|---|---|---|
| service-rtt-debug-console | 2026-05-07 | [bootloader/service-rtt-debug-console](bootloader/service-rtt-debug-console/) |

### LED

| 작업 | 완료일 | 폴더 |
|---|---|---|
| bootloader-power-on-indicator | 2026-04-27 | [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) |
| non-blocking-fade-off | 2026-04-27 | [LED/non-blocking-fade-off](LED/non-blocking-fade-off/) |
| operation-scheme | — | [LED/operation-scheme](LED/operation-scheme/) |
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
| low-power-mode | — | [power/low-power-mode](power/low-power-mode/) |

### signalProcessing

| 작업 | 완료일 | 폴더 |
|---|---|---|
| cfx-cm3-analysis | 2026-05-14 | [signalProcessing/cfx-cm3-analysis](signalProcessing/cfx-cm3-analysis/) |
| nofm-merge | — | [signalProcessing/nofm-merge](signalProcessing/nofm-merge/) |
| nofm-tuning | 2026-05-14 | [signalProcessing/nofm-tuning](signalProcessing/nofm-tuning/) |

### sync

| 작업 | 완료일 | 폴더 |
|---|---|---|
| sullivan1.5-rel2 | — | [sync/sullivan1.5-rel2](sync/sullivan1.5-rel2/) |

### touch

| 작업 | 완료일 | 폴더 |
|---|---|---|
| board-variant-iqs323-ati | 2026-05-08 | [touch/board-variant-iqs323-ati](touch/board-variant-iqs323-ati/) |
| init-split | — | [touch/init-split](touch/init-split/) |
| layer-separation | — | [touch/layer-separation](touch/layer-separation/) |

### ui

| 작업 | 완료일 | 폴더 |
|---|---|---|
| program-change-command | — | [ui/program-change-command](ui/program-change-command/) |

