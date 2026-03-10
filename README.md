# sound1-fw-e8300

Sound1 디바이스 — E8300 메인 프로세서 펌웨어

## 개요

| 항목 | 내용 |
|------|------|
| **칩셋** | E8300 |
| **역할** | 메인 프로세서 |
| **주요 기능** | 자극 제어, 오디오 처리, DSP|

## 관련 Repository

| Repo | 설명 |
|------|------|
| [sound1-fw-qcc5181](https://github.com/todoc-dev/sound1-fw-qcc5181) | QCC5181 BT SoC 펌웨어 |
| [sound1-fw-common](https://github.com/todoc-dev/sound1-fw-common) | 칩셋 간 공유 코드 |
| [sound1-ios](https://github.com/todoc-dev/sound1-ios) | Sound1 iOS 앱 |
| [sound1-android](https://github.com/todoc-dev/sound1-android) | Sound1 Android 앱 |

## Safety Critical 코드

⚠️ 자극(stimulation) 및 안전(safety) 관련 코드는 환자 안전에 직접 영향을 미칩니다.

해당 코드 변경 시 반드시:
1. Risk Analysis 문서 검토
2. PR에 Safety Impact Assessment 작성
3. ~~리뷰어 2명 이상 승인 필요~~

## Branch 전략

```
main     ← 출시/규제 제출용 (직접 push 금지)
develop  ← 통합 개발
feature/* / bugfix/* / hotfix/* ← 작업 브랜치
```

## 빌드

```bash
# 빌드 환경 및 방법 추후 기술
```
